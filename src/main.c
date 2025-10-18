#include "main.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "args.h"
#include "cruxpass.h"
#include "database.h"
#include "enc.h"
#include "tui.h"

sqlite3 *db;
unsigned char *key;

extern char *cruxpass_db_path;
extern char *auth_db_path;
char *description
    = "A lightweight, command-line password/secrets manager designed to securely store and\nretrieve encrypted "
      "credentials. It uses an SQLCipher database to manage entries, with\nauthentication separated from password "
      "storage. Access is controlled via a master password.\n";
char *footer = "It emphasizes simplicity, security, and efficiency for developers and power users.";

void cleanup_main(void);
void sig_handler(int sig);

int main(int argc, char **argv) {
    args args_obj = {0};
    bool *help = option_flag(&args_obj, 'h', "help", "Show this help");
    bool *save = option_flag(&args_obj, 's', "save", "Save a given record");
    bool *list = option_flag(&args_obj, 'l', "list", "List all records");
    bool *version = option_flag(&args_obj, 'v', "version", "Cruxpass version");
    bool *new_password = option_flag(&args_obj, 'n', "new-password", "Creates a new master password for cruxpass");
    long *record_id = option_long(&args_obj, 'd', "delete", "Deletes a record by id", true, -1);
    long *gen_secret_len
        = option_long(&args_obj, 'g', "generate-rand", "Generates a random secret of a given length", true, 0);
    bool *unambiguous_password
        = option_flag(&args_obj, 'x', "exclude-ambiguous",
                      "Exclude ambiguous characters when generating a random secret (combine with -g)");
    const char **export_file
        = option_str(&args_obj, 'e', "export", "Export all saved records to a csv format", true, NULL);
    const char **cruxpass_run_dir = option_str(
        &args_obj, 'r', "run-directory", "Specify the directory path where the database will be stored.", true, NULL);
    const char **import_file = option_str(&args_obj, 'i', "import", "Import records from a csv file", true, NULL);

    char **pos_args = NULL;
    int pos_args_len = parse_args(&args_obj, argc, argv, &pos_args);
    bool check_gen_flags = *unambiguous_password && !*gen_secret_len;

    if (*help || pos_args_len != 0 || argc == 1 || check_gen_flags) {
        fprintf(stdout, "usage: %s [options]\n", argv[0]);
        fprintf(stdout, "\n");
        fprintf(stdout, "%s\n", description);
        print_options(&args_obj, stdout);
        fprintf(stdout, "\n%s\n", footer);
        free_args(&args_obj);
        return EXIT_SUCCESS;
    }

    if (*version) {
        fprintf(stdout, "cruxpass version: %s\n", VERSION);
        fprintf(stdout, "Authur: %s\n", AUTHUR);
        free_args(&args_obj);
        return EXIT_SUCCESS;
    }

    if (*gen_secret_len != 0) {
        secret_bank_options_t bank_options = {0};
        bank_options.uppercase = true;
        bank_options.lowercase = true;
        bank_options.numbers = true;
        bank_options.symbols = true;
        bank_options.exclude_ambiguous = (unambiguous_password != 0) ? true : false;
        char *secret = NULL;
        if ((secret = random_secret(*gen_secret_len, &bank_options)) == NULL) {
            free_args(&args_obj);
            return EXIT_FAILURE;
        }

        fprintf(stdout, "secret: %s\n", secret);
        free(secret);
        free_args(&args_obj);
        return EXIT_SUCCESS;
    }

    if (*cruxpass_run_dir != NULL) {
        if (strlen(*cruxpass_run_dir) >= MAX_PATH_LEN - 16) {
            fprintf(stderr, "Error: Path to run-directory too long.\n");
            free_args(&args_obj);
            return EXIT_FAILURE;
        }
    }

    struct sigaction sigact;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_handler = sig_handler;
    sigact.sa_flags = SA_RESTART;

    if (sigaction(SIGTERM, &sigact, NULL) != 0 || sigaction(SIGINT, &sigact, NULL) != 0) {
        fprintf(stderr, "Fail to make reception for signals");
        free_args(&args_obj);
        return EXIT_FAILURE;
    }

    if ((db = initcrux((char *) *cruxpass_run_dir)) == NULL) {
        free_args(&args_obj);
        return EXIT_FAILURE;
    }

    if ((key = decryption_helper(db)) == NULL) {
        cleanup_main();
        free_args(&args_obj);
        return EXIT_FAILURE;
    }

    if (*new_password) {
        if (!create_new_master_secret(db)) {
            fprintf(stderr, "Error: Failed to creat a new master password\n");
            cleanup_main();
            free_args(&args_obj);
            return EXIT_FAILURE;
        }

        fprintf(stderr, "Note: Master password changed successfully.\n");
    }

    if (*save) {
        init_tui();
        tb_clear();
        secret_t record = {0};
        if (get_input("> username: ", record.username, USERNAME_MAX_LEN, 2, 0) == NULL
            || get_input("> secret: ", record.secret, SECRET_MAX_LEN, 3, 0) == NULL
            || get_input("> description: ", record.description, DESC_MAX_LEN, 4, 0) == NULL) {
            cleanup_tui();
            cleanup_main();
            free_args(&args_obj);

            fprintf(stderr, "Error: Failed to retrieve record\n");
            return EXIT_FAILURE;
        }

        cleanup_tui();
        if (!insert_record(db, &record)) {
            cleanup_main();

            free_args(&args_obj);
            return EXIT_FAILURE;
        }

        fprintf(stderr, "Note: Password saved successfully.\n");
    }

    if (*import_file != NULL) {
        if (strlen(*import_file) > FILE_PATH_LEN) {
            fprintf(stderr, "Warning: Import file name too long [MAX: %d character long]\n", FILE_PATH_LEN);
            cleanup_main();
            free_args(&args_obj);
            return EXIT_FAILURE;
        }

        if (!import_secrets(db, (char *) *import_file)) {
            fprintf(stderr, "Error: Failed to import secrets from: %s", *import_file);
            cleanup_main();
            free_args(&args_obj);
            return EXIT_FAILURE;
        }

        fprintf(stderr, "Info: secrets imported successfully from: %s\n", *import_file);
    }

    if (*export_file != NULL) {
        if (strlen(*export_file) > FILE_PATH_LEN) {
            fprintf(stderr, "Warning: Export file name too long: MAX_LEN =>15\n");
            cleanup_main();
            free_args(&args_obj);
            return EXIT_FAILURE;
        }

        if (!export_secrets(db, *export_file)) {
            fprintf(stdout, "Error: Failed to export secrets to: %s\n", *export_file);
            cleanup_main();
            free_args(&args_obj);
            return EXIT_FAILURE;
        };

        fprintf(stderr, "Info: secrets exported successfully to: %s\n", *export_file);
    }

    if (*record_id != -1) {
        if (!delete_record(db, *record_id)) {
            cleanup_main();
            free_args(&args_obj);
            return EXIT_FAILURE;
        }
    }

    if (*list) {
        main_tui(db);
    }

    free_args(&args_obj);
    cleanup_main();
    return EXIT_SUCCESS;
}

void sig_handler(int sig) {
    cleanup_main();
    cleanup_tui();
    fprintf(stdout, "Note: Signal %d receive\n", sig);
    exit(EXIT_FAILURE);
}

void cleanup_main(void) {
    sqlite3_close(db);
    cleanup_stmts();
    if (cruxpass_db_path != NULL) free(cruxpass_db_path);
    if (auth_db_path != NULL) free(auth_db_path);

    if (key != NULL) {
        sodium_memzero(key, KEY_LEN);
        sodium_free(key);
    }
}
