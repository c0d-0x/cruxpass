#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    option_version(&cmd_args, "cruxpass-" VERSION);
    const bool *help = option_flag(&cmd_args, "help", "Show this help", .short_name = 'h', .early_exit = true);
    const bool *list = option_flag(&cmd_args, "list", "List all records", .short_name = 'l');
    const bool *save = option_flag(&cmd_args, "save", "Save a given record", .short_name = 'S');
    const char **import_file = option_path(&cmd_args, "import", "Import records from a csv file", .short_name = 'i');
    const char **export_file
        = option_path(&cmd_args, "export", "Export all records to a csv format", .short_name = 'e');
    const bool *new_password = option_flag(&cmd_args, "new-password", "Change your login password", .short_name = 'n');
    const long *record_id
        = option_long(&cmd_args, "delete", "Deletes a record by id", .short_name = 'd', .default_value = -1);
    const long *gen_secret_len
        = option_long(&cmd_args, "generate-rand", "Generates a random secret of a given length", .short_name = 'g');
    const bool *pin
        = option_flag(&cmd_args, "pin", "Generates a random pin of a given length (combined -g)", .short_name = 'p');

    const bool *unambiguous
        = option_flag(&cmd_args, "exclude-ambiguous",
                      "Generates a random secret excluding ambiguous characters (combined -g)", .short_name = 'x');
    const bool *symbols = option_flag(
        &cmd_args, "symbols", "Generates a symbols' random secret of a given length (combined -g)", .short_name = 's');
    const bool *lower_case
        = option_flag(&cmd_args, "lower", "Generates an all lower case random secret of a given length (combined -g)",
                      .short_name = 'a');
    const bool *upper_case
        = option_flag(&cmd_args, "upper", "Generates an all upper case random pin of a given length (combined -g)",
                      .short_name = 'A');

    const char **cruxpass_run_dir = option_path(
        &cmd_args, "run-directory", "Specify the directory path where the database will be stored.", .short_name = 'r');

    char **pos_args = NULL;
    int pos_args_len = parse_args(&cmd_args, argc, argv, &pos_args);

    if ((*gen_secret_len == 0) && (*upper_case || *lower_case || *pin || *unambiguous || *symbols)) {
        fprintf(stderr, "Warning: -[ a, A, p, x, s ] must be combined with -g\n");
        free_args(&cmd_args);
        return EXIT_FAILURE;
    }

    if (*help || pos_args_len != 0 || argc == 1) {
        print_help(&cmd_args, argv[0]);
        free_args(&cmd_args);
        return EXIT_SUCCESS;
    }

    if (*gen_secret_len != 0) {
        bank_options_t opt = {0};
        if (!(*pin) && !(*upper_case) && !(*lower_case) && !(*symbols)) {
            opt.upper = true;
            opt.lower = true;
            opt.digit = true;
            opt.symbols = true;
        }

        if (*unambiguous) {
            opt.ex_ambiguous = true;
        }

        if (*symbols) {
            opt.symbols = true;
        }

        if (*pin) {
            opt.digit = true;
        }

        if (*upper_case) {
            opt.upper = true;
        }

        if (*lower_case) {
            opt.lower = true;
        }

        char *secret = NULL;
        if ((secret = random_secret(*gen_secret_len, &opt)) == NULL) {
            free_args(&cmd_args);
            return EXIT_FAILURE;
        }

        fprintf(stdout, "secret: %s\n", secret);
        sodium_memzero(secret, sizeof(secret));

        free(secret);
        free_args(&cmd_args);
        return EXIT_SUCCESS;
    }

    if (*cruxpass_run_dir != NULL) {
        if (strlen(*cruxpass_run_dir) >= MAX_PATH_LEN - HOME_PATH_MAX_LEN) {
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
        fprintf(stderr, "Error: Fail to make reception for signals");
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
        if (!rotate_login_secret(ctx->secret_db)) {
            fprintf(stderr, "Error: Failed to create a new login password\n");
            cleanup_main();
            free_args(&cmd_args);
            return EXIT_FAILURE;
        }

        fprintf(stderr, "Note: Login password changed successfully.\n");
    }

    if (*save) {
        tui_init();
        tb_clear();
        secret_t rec = {0};
        if (get_input("> username: ", rec.username, USERNAME_MAX_LEN, 0, 2) == NULL
            || get_input("> secret: ", rec.secret, SECRET_MAX_LEN, 0, 3) == NULL
            || get_input("> description: ", rec.description, DESC_MAX_LEN, 0, 4) == NULL) {
            tui_cleanup();
            cleanup_main();
            free_args(&cmd_args);

            fprintf(stderr, "Error: Failed to retrieve record\n");
            return EXIT_FAILURE;
        }

        tui_cleanup();
        if (strlen(rec.description) < FIELD_MIN || strlen(rec.secret) < FIELD_MIN || strlen(rec.username) < FIELD_MIN) {
            cleanup_main();
            free_args(&cmd_args);
            fprintf(stderr, "Error: Failed to retrieve record\n");
            return EXIT_FAILURE;
        }

        if (!insert_record(ctx->secret_db, &rec)) {
            cleanup_main();
            free_args(&cmd_args);
            return EXIT_FAILURE;
        }

        fprintf(stderr, "Note: Password saved successfully\n");
    }

    if (*import_file != NULL) {
        if (strlen(*import_file) > FILE_PATH_LEN) {
            cleanup_main();
            free_args(&cmd_args);
            fprintf(stderr, "Warning: Import file name too long [MAX: %d character long]\n", FILE_PATH_LEN);
            return EXIT_FAILURE;
        }

        if (!import_secrets(ctx->secret_db, (char *) *import_file)) {
            cleanup_main();
            free_args(&cmd_args);
            fprintf(stderr, "Error: Failed to import secrets from: %s", *import_file);
            return EXIT_FAILURE;
        }

        fprintf(stderr, "Info: secrets imported successfully from: %s\n", *import_file);
    }

    if (*export_file != NULL) {
        if (strlen(*export_file) > FILE_PATH_LEN) {
            cleanup_main();
            free_args(&cmd_args);
            fprintf(stderr, "Warning: Export file path must not be more than: %d\n", FILE_PATH_LEN);
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
        if (tui_main(ctx->secret_db) == CRXP_ERR) {
            cleanup_main();
            free_args(&cmd_args);
            return EXIT_FAILURE;
        };
    }

    cleanup_main();
    free_args(&cmd_args);
    return EXIT_SUCCESS;
}

void sig_handler(int sig) {
    cleanup_main();
    tui_cleanup();
    fprintf(stdout, "Warning: Signal \"%s\" Received\n", strsignal(sig));
    exit(EXIT_FAILURE);
}

void cleanup_main(void) {
    tui_cleanup();
    cleanup_stmts();
    sqlite3_close(ctx->secret_db);
    if (cruxpass_db_path != NULL) free(cruxpass_db_path);
    if (meta_db_path != NULL) free(meta_db_path);

    if (key != NULL) {
        sodium_memzero(key, KEY_LEN);
        sodium_free(key);
    }
}

void print_help(Args *cmd_args, const char *program) {
    char *description
        = "A lightweight, command-line password/secrets manager designed to be simple, dependency-light,\nand "
          "transparent. It uses an SQLCipher database to manage entries, and storage accessed via a login password.\n";
    char *footer = "Its philosophy is simplicity, security, and efficiency for devs and terminal wizards.";

    fprintf(stdout, "usage: %s [options]\n\n", program);
    fprintf(stdout, "%s\n", description);
    print_options(cmd_args, stdout);
    fprintf(stdout, "\n%s\n", footer);
}
