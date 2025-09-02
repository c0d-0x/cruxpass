#include "../include/main.h"

#include <ncurses.h>
#include <sodium/utils.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
static int password_len;
static char *export_file;
static char *import_file;

sqlite3 *db;
unsigned char *key;

void cleanup_main(void);
int parse_options(int argc, const char **argv);

int main(int argc, const char **argv) {
  if (!parse_options(argc, argv)) return EXIT_FAILURE;

  if (version != 0) {
    fprintf(stdout, "cruxpass version: %s\n", VERSION);
    fprintf(stdout, "Authur: %s\n", AUTHUR);
    return EXIT_SUCCESS;
  }

  if (password_len != 0) {
    secret_bank_options_t bank_options;
    memset(&bank_options, 0, sizeof(secret_bank_options_t));
    bank_options.uppercase = true;
    bank_options.lowercase = true;
    bank_options.numbers = true;
    bank_options.symbols = true;
    bank_options.exclude_ambiguous = true;
    char *secret = NULL;
    if ((secret = random_secret(password_len, &bank_options)) == NULL) {
      return EXIT_FAILURE;
    }

    fprintf(stdout, "secret: %s\n", secret);
    free(secret);
    return EXIT_SUCCESS;
  }

  if ((db = initcrux()) == NULL) return EXIT_FAILURE;
  if ((key = decryption_helper(db)) == NULL) return EXIT_FAILURE;
  if (!prepare_stmt(db)) return EXIT_FAILURE;

  if (new_password != 0) {
    if (!create_new_master_secret(db, key)) {
      fprintf(stderr, "Error: Failed to Creat a New Password\n");
      cleanup_main();
      return EXIT_FAILURE;
    }
  }

  if (save != 0) {
    secret_t record = {0};
    clear();
    get_input("> username: ", record.username, USERNAME_MAX_LEN, 2, 0);
    get_input("> secret: ", record.secret, SECRET_MAX_LEN, 3, 0);
    get_input("> description: ", record.description, DESC_MAX_LEN, 4, 0);

    if (!insert_record(db, &record)) {
      cleanup_main();
      return EXIT_FAILURE;
    }
  }

  if (import_file != NULL) {
    if (strnlen(import_file, FILE_PATH_LEN) > FILE_PATH_LEN) {
      fprintf(stderr, "Warning: Import file name too long [MAX: %d character long]\n", FILE_PATH_LEN);
      cleanup_main();
      return EXIT_FAILURE;
    }

    if (!import_secrets(db, import_file)) {
      fprintf(stderr, "Error: Failed to import: %s", import_file);
      cleanup_main();
      return EXIT_FAILURE;
    }
    fprintf(stderr, "Info: Passwords imported successfully from: %s\n", import_file);
  }

  if (export_file != NULL) {
    if (strnlen(export_file, FILE_PATH_LEN) > FILE_PATH_LEN) {
      fprintf(stderr, "Warning: Export file name too long: MAX_LEN =>15\n");
      cleanup_main();
      return EXIT_FAILURE;
    }

    // TODO: fix set path to set paths to CRUXPASS_DB instead.
    if (!export_secrets(db, export_file)) {
      fprintf(stdout, "Info: Passwords exported to: %s\n", export_file);
      cleanup_main();
      return EXIT_FAILURE;
    };
  }

  if (password_id != 0) {
    /* WARNING: db object never used but necessary to initcrux() */
    /* sqlite3_close(db); */
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
  if (key != NULL) {
    sodium_memzero(key, KEY_LEN);
    sodium_free(key);
  }
}

int parse_options(int argc, const char **argv) {
  int argc_cp = argc;
  struct argparse_option options[] = {
      OPT_HELP(),
      OPT_GROUP("cruxpass options"),
      OPT_INTEGER('d', "delete", &password_id, "Deletes a password by id\n", NULL, 0, 0),
      OPT_STRING('e', "export", &export_file, "Export all saved passwords to a csv format\n", NULL, 0, 0),
      OPT_STRING('i', "import", &import_file, "Import passwords from a csv file\n", NULL, 0, 0),
      OPT_INTEGER('g', "generate-rand", &password_len, "Generates a random password of a given length\n", NULL, 0, 0),
      OPT_BOOLEAN('l', "list", &list, "list all passwords\n", NULL, 0, 0),
      OPT_BOOLEAN('n', "new-password", &new_password, "creates a new master password for cruxpass\n", NULL, 0, 0),
      OPT_BOOLEAN('s', "save", &save, "save a given password\n", NULL, 0, 0),
      OPT_BOOLEAN('v', "version", &version, "cruxpass version\n", NULL, 0, 0),
      OPT_END(),
  };

  struct argparse argparse;
  argparse_init(&argparse, options, usages, ARGPARSE_STOP_AT_NON_OPTION);
  argparse_describe(&argparse,
                    "cruxpass is a lightweight, command-line password manager designed to securely\n"
                    "store and retrieve encrypted credentials. It uses an SQLCipher database to manage\n"
                    "entries, with authentication separated from password storage. Access is controlled\n"
                    "via a master password.\n",
                    "\ncruxpass emphasizes simplicity, security, and efficiency for developers and power users.\n");
  argc = argparse_parse(&argparse, argc, argv);

  if (argc_cp <= 1) {
    argparse_usage(&argparse);
    return 0;
  }
  return 1;
}
