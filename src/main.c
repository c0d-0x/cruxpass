#include "../include/main.h"

#include <sodium/utils.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/argparse.h"
#include "../include/cruxpass.h"
#include "../include/database.h"
#include "../include/enc.h"
#include "../include/tui.h"

#define FILE_PATH_LEN 32

static const char *const usages[] = {
    "cruxpass --options [FILE]",
    NULL,
};

static int save;
static int list;
static int version;
static int new_password;
static int password_id;
static int gen_secret_len;
static int unambiguous_password;
static char *export_file;
static char *cruxpass_run_dir;
static char *import_file;

sqlite3 *db;
unsigned char *key;

extern char *cruxpass_db_path;
extern char *auth_db_path;

struct argparse argparse;
void cleanup_main(void);
int parse_options(int argc, const char **argv);

int main(int argc, const char **argv) {
  if (!parse_options(argc, argv)) return EXIT_FAILURE;

  if (version != 0) {
    fprintf(stdout, "cruxpass version: %s\n", VERSION);
    fprintf(stdout, "Authur: %s\n", AUTHUR);
    return EXIT_SUCCESS;
  }

  if (gen_secret_len != 0) {
    secret_bank_options_t bank_options = {0};
    bank_options.uppercase = true;
    bank_options.lowercase = true;
    bank_options.numbers = true;
    bank_options.symbols = true;
    bank_options.exclude_ambiguous = (unambiguous_password != 0) ? true : false;
    char *secret = NULL;
    if ((secret = random_secret(gen_secret_len, &bank_options)) == NULL) {
      return EXIT_FAILURE;
    }

    fprintf(stdout, "secret: %s\n", secret);
    free(secret);
    return EXIT_SUCCESS;
  }

  if (cruxpass_run_dir != NULL) {
    if (strlen(cruxpass_run_dir) >= MAX_PATH_LEN - 16) {
      fprintf(stderr, "Error: Path to run-directory too long.\n");
      return EXIT_FAILURE;
    }
  }

  if ((db = initcrux(cruxpass_run_dir)) == NULL) {
    return EXIT_FAILURE;
  }

  if ((key = decryption_helper(db)) == NULL) {
    return EXIT_FAILURE;
  }

  cleanup_tui();
  if (!prepare_stmt(db)) {
    return EXIT_FAILURE;
  }

  if (new_password != 0) {
    if (!create_new_master_secret(db, key)) {
      fprintf(stderr, "Error: Failed to Creat a New Password\n");
      cleanup_main();
      return EXIT_FAILURE;
    }
  }

  if (save != 0) {
    init_tui();
    tb_clear();
    secret_t record = {0};
    if (get_input("> username: ", record.username, USERNAME_MAX_LEN, 2, 0) == NULL ||
        get_input("> secret: ", record.secret, SECRET_MAX_LEN, 3, 0) == NULL ||
        get_input("> description: ", record.description, DESC_MAX_LEN, 4, 0) == NULL) {
      cleanup_tui();
      cleanup_main();

      fprintf(stderr, "Error: Failed to retrieve record\n");
      return EXIT_FAILURE;
    }

    cleanup_tui();
    if (!insert_record(db, &record)) {
      cleanup_main();

      return EXIT_FAILURE;
    }
  }

  if (import_file != NULL) {
    if (strlen(import_file) > FILE_PATH_LEN) {
      fprintf(stderr, "Warning: Import file name too long [MAX: %d character long]\n", FILE_PATH_LEN);
      cleanup_main();
      return EXIT_FAILURE;
    }

    if (!import_secrets(db, import_file)) {
      fprintf(stderr, "Error: Failed to import secrets from: %s", import_file);
      cleanup_main();
      return EXIT_FAILURE;
    }

    fprintf(stderr, "Info: secrets imported successfully from: %s\n", import_file);
  }

  if (export_file != NULL) {
    if (strlen(export_file) > FILE_PATH_LEN) {
      fprintf(stderr, "Warning: Export file name too long: MAX_LEN =>15\n");
      cleanup_main();
      return EXIT_FAILURE;
    }

    if (!export_secrets(db, export_file)) {
      fprintf(stdout, "Error: Failed to export secrets to: %s\n", export_file);
      cleanup_main();
      return EXIT_FAILURE;
    };

    fprintf(stderr, "Info: secrets exported successfully to: %s\n", export_file);
  }

  if (password_id != 0) {
    /* WARNING: db object never used but necessary to initcrux() */
    if (!delete_record(db, password_id)) {
      cleanup_main();
      return EXIT_FAILURE;
    }
  }

  if (list != 0) {
    main_tui(db);
  }

  cleanup_main();
  return EXIT_SUCCESS;
}

void cleanup_main(void) {
  sqlite3_close(db);
  cleanup_stmts();
  if (cruxpass_db_path != NULL) free(cruxpass_run_dir);
  if (auth_db_path != NULL) free(auth_db_path);

  if (key != NULL) {
    sodium_memzero(key, KEY_LEN);
    sodium_free(key);
  }
}

int parse_options(int argc, const char **argv) {
  struct argparse_option options[] = {
      OPT_HELP(),
      OPT_GROUP("cruxpass options"),
      OPT_STRING('r', "run-directory", &cruxpass_run_dir,
                 "Specify the directory path where the database will be stored.\n", NULL, 0, 0),
      OPT_INTEGER('d', "delete", &password_id, "Deletes a password by id\n", NULL, 0, 0),
      OPT_STRING('e', "export", &export_file, "Export all saved passwords to a csv format\n", NULL, 0, 0),
      OPT_STRING('i', "import", &import_file, "Import passwords from a csv file\n", NULL, 0, 0),
      OPT_INTEGER('g', "generate-rand", &gen_secret_len, "Generates a random password of a given length\n", NULL, 0, 0),
      OPT_BOOLEAN('x', "exclude-ambiguous", &unambiguous_password,
                  "Exclude ambiguous characters when generating a random password (combine with -g)\n", NULL, 0, 0),
      OPT_BOOLEAN('l', "list", &list, "List all passwords\n", NULL, 0, 0),
      OPT_BOOLEAN('n', "new-password", &new_password, "Creates a new master password for cruxpass\n", NULL, 0, 0),
      OPT_BOOLEAN('s', "save", &save, "Save a given password\n", NULL, 0, 0),
      OPT_BOOLEAN('v', "version", &version, "Cruxpass version\n", NULL, 0, 0),
      OPT_END(),
  };

  argparse_init(&argparse, options, usages, ARGPARSE_STOP_AT_NON_OPTION);
  argparse_describe(&argparse,
                    "cruxpass is a lightweight, command-line password manager designed to securely\n"
                    "store and retrieve encrypted credentials. It uses an SQLCipher database to manage\n"
                    "entries, with authentication separated from password storage. Access is controlled\n"
                    "via a master password.\n",
                    "\ncruxpass emphasizes simplicity, security, and efficiency for developers and power users.\n");
  argparse_parse(&argparse, argc, argv);

  /* prevents exclude-ambiguous from being an independent option  */
  if (argc <= 1 || (!gen_secret_len && unambiguous_password)) {
    argparse_usage(&argparse);
    return 0;
  }
  return 1;
}
