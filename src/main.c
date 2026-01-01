#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#ifndef VERSION
#define VERSION "v2.0.0"
#endif

#define ARGS_HIDE_DEFAULTS
#define ARGS_LINE_LENGTH 120
#define ARGS_MIN_DESC_LENGTH 80

#include "args.h"
#include "cruxpass.h"
#include "crypt.h"
#include "database.h"
#include "tui.h"

unsigned char *key;
vault_ctx_t *ctx;

extern char *meta_db_path;
extern char *cruxpass_db_path;

void cleanup_main(void);
void sig_handler(int sig);
void print_help(Args *cmd_args, const char *program);

int main(int argc, char **argv) {
    Args cmd_args = {0};
    const bool *help = option_flag(&cmd_args, "help", "Show this help", .short_name = 'h', .early_exit = true);
    option_version(&cmd_args, "cruxpass-" VERSION);
    const bool *save = option_flag(&cmd_args, "save", "Save a given record", .short_name = 's');
    const bool *list = option_flag(&cmd_args, "list", "List all records", .short_name = 'l');
    const bool *new_password = option_flag(&cmd_args, "new-password", "Change your master password", .short_name = 'n');
    const long *record_id = option_long(&cmd_args, "delete", "Deletes a record by id", .short_name = 'd');
    const long *gen_secret_len
        = option_long(&cmd_args, "generate-rand", "Generates a random secret of a given length", .short_name = 'g');
    const bool *unambiguous = option_flag(
        &cmd_args, "exclude-ambiguous",
        "Exclude ambiguous characters when generating a random secret (combine with -g)", .short_name = 'x');
    const char **export_file
        = option_path(&cmd_args, "export", "Export all records to a csv format", .short_name = 'e');
    const char **cruxpass_run_dir = option_path(
        &cmd_args, "run-directory", "Specify the directory path where the database will be stored.", .short_name = 'r');
    const char **import_file = option_path(&cmd_args, "import", "Import records from a csv file", .short_name = 'i');

    char **pos_args = NULL;
    int pos_args_len = parse_args(&cmd_args, argc, argv, &pos_args);
    bool check_gen_flags = *unambiguous && !*gen_secret_len;

    if (*help || pos_args_len != 0 || argc == 1 || check_gen_flags) {
        print_help(&cmd_args, argv[0]);
        free_args(&cmd_args);
        return EXIT_SUCCESS;
    }

    if (*gen_secret_len != 0) {
        // clang-format off
        bank_options_t bank_options = {
            .upper= true, 
            .lower= true, 
            .digit = true,
            .symbols = true, 
            .ex_ambiguous = (unambiguous != NULL)? *unambiguous: false};
        // clang-format on
        char *secret = NULL;
        if ((secret = random_secret(*gen_secret_len, &bank_options)) == NULL) {
            free_args(&cmd_args);
            return EXIT_FAILURE;
        }

        fprintf(stdout, "secret: %s\n", secret);
        free(secret);
        free_args(&cmd_args);
        return EXIT_SUCCESS;
    }

    if (*cruxpass_run_dir != NULL) {
        if (strlen(*cruxpass_run_dir) >= MAX_PATH_LEN - 16) {
            fprintf(stderr, "Error: Path to run-directory too long.\n");
            free_args(&cmd_args);
            return EXIT_FAILURE;
        }
    }

    struct sigaction sigact;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_handler = sig_handler;
    sigact.sa_flags = SA_RESTART;

    if (sigaction(SIGTERM, &sigact, NULL) != 0 || sigaction(SIGINT, &sigact, NULL) != 0) {
        fprintf(stderr, "Fail to make reception for signals");
        free_args(&cmd_args);
        return EXIT_FAILURE;
    }

    if ((ctx = initcrux((char *) *cruxpass_run_dir)) == NULL) {
        free_args(&cmd_args);
        return EXIT_FAILURE;
    }

    if ((key = authenticate(ctx)) == NULL) {
        cleanup_main();
        free_args(&cmd_args);
        return EXIT_FAILURE;
    }

    if (*new_password) {
        if (!create_new_master_secret(ctx->secret_db)) {
            fprintf(stderr, "Error: Failed to creat a new master password\n");
            cleanup_main();
            free_args(&cmd_args);
            return EXIT_FAILURE;
        }

        fprintf(stderr, "Note: Master password changed successfully.\n");
    }

    if (*save) {
        init_tui();
        tb_clear();
        secret_t record = {0};
        if (get_input("> username: ", record.username, USERNAME_MAX_LEN, 0, 2) == NULL
            || get_input("> secret: ", record.secret, SECRET_MAX_LEN, 0, 3) == NULL
            || get_input("> description: ", record.description, DESC_MAX_LEN, 0, 4) == NULL) {
            cleanup_tui();
            cleanup_main();
            free_args(&cmd_args);

            fprintf(stderr, "Error: Failed to retrieve record\n");
            return EXIT_FAILURE;
        }

        cleanup_tui();
        if (!insert_record(ctx->secret_db, &record)) {
            cleanup_main();

            free_args(&cmd_args);
            return EXIT_FAILURE;
        }

        fprintf(stderr, "Note: Password saved successfully.\n");
    }

    if (*import_file != NULL) {
        if (strlen(*import_file) > FILE_PATH_LEN) {
            fprintf(stderr, "Warning: Import file name too long [MAX: %d character long]\n", FILE_PATH_LEN);
            cleanup_main();
            free_args(&cmd_args);
            return EXIT_FAILURE;
        }

        if (!import_secrets(ctx->secret_db, (char *) *import_file)) {
            fprintf(stderr, "Error: Failed to import secrets from: %s", *import_file);
            cleanup_main();
            free_args(&cmd_args);
            return EXIT_FAILURE;
        }

        fprintf(stderr, "Info: secrets imported successfully from: %s\n", *import_file);
    }

    if (*export_file != NULL) {
        if (strlen(*export_file) > FILE_PATH_LEN) {
            fprintf(stderr, "Warning: Export file path must not be more than: %d\n", FILE_PATH_LEN);
            cleanup_main();
            free_args(&cmd_args);
            return EXIT_FAILURE;
        }

        if (!export_secrets(ctx->secret_db, *export_file)) {
            fprintf(stderr, "Error: Failed to export secrets to: %s\n", *export_file);
            cleanup_main();
            free_args(&cmd_args);
            return EXIT_FAILURE;
        };

        fprintf(stderr, "Info: secrets exported successfully to: %s\n", *export_file);
    }

    if (*record_id != -1) {
        if (!delete_record(ctx->secret_db, *record_id)) {
            cleanup_main();
            free_args(&cmd_args);
            return EXIT_FAILURE;
        }
    }

    if (*list) {
        tui_main(ctx->secret_db);
    }

    free_args(&cmd_args);
    cleanup_main();
    return EXIT_SUCCESS;
}

void sig_handler(int sig) {
    cleanup_main();
    cleanup_tui();
    fprintf(stdout, "Warning: Signal %d Received\n", sig);
    fprintf(stdout, "Warning: Terminating cruxpass\n");
    exit(EXIT_FAILURE);
}

void cleanup_main(void) {
    sqlite3_close(ctx->secret_db);
    cleanup_stmts();
    if (cruxpass_db_path != NULL) free(cruxpass_db_path);
    if (meta_db_path != NULL) free(meta_db_path);

    if (key != NULL) {
        sodium_memzero(key, KEY_LEN);
        sodium_free(key);
    }
}

void print_help(Args *cmd_args, const char *program) {
    char *description
        = "A lightweight, command-line password/secrets manager designed to securely store and\nretrieve encrypted "
          "credentials. It uses an SQLCipher database to manage entries, with\nauthentication separated from password "
          "storage. Access is controlled via a master password.\n";
    char *footer = "It emphasizes simplicity, security, and efficiency for developers and power users.";

    fprintf(stdout, "usage: %s [options]\n\n", program);
    fprintf(stdout, "%s\n", description);
    print_options(cmd_args, stdout);
    fprintf(stdout, "\n%s\n", footer);
}
