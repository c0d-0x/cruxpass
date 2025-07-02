#define SQLITE_HAS_CODEC
#include "../include/main.h"

#include <sodium/utils.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/argparse.h"
#include "../include/cruxpass.h"
#include "../include/database.h"
#include "../include/enc.h"
#include "../include/sqlcipher.h"
#include "../include/tui.h"

static const char *const usages[] = {
    "cruxpass --options [FILE]",
    NULL,
};

static int save = 0;
static int list = 0;
static int version = 0;
static int new_password = 0;
static int password_id = 0;
static int password_len = 0;
static char *export_file = NULL;
static char *import_file = NULL;
static char *master_passd = NULL;

int parse_options(int argc, const char **argv);

int main(int argc, const char **argv) {
  sqlite3 *db = NULL;
  unsigned char *key = NULL;

  if (!parse_options(argc, argv)) return EXIT_FAILURE;
  if (new_password != 0) {
    // WARNING: db object never used but necessary to initcrux()
    if ((db = initcrux()) == NULL) return EXIT_SUCCESS;
    sqlite3_close(db);

    if ((master_passd = get_password("Master Password: ")) == NULL) return EXIT_FAILURE;
    if (!create_new_master_passd(master_passd)) {
      fprintf(stderr, "Error: Failed to Creat a New Password\n");
      sodium_memzero(master_passd, PASSLENGTH);
      sodium_free(master_passd);
      return EXIT_FAILURE;
    }
  }

  if (save != 0) {
    password_t password_obj = {0};
    get_input("> username: ", password_obj.username, USERNAMELENGTH, 2, 0);
    get_input("> password: ", password_obj.passd, PASSLENGTH, 3, 0);
    get_input("> description: ", password_obj.description, DESCLENGTH, 4, 0);

    if ((db = initcrux()) == NULL) return EXIT_SUCCESS;
    if ((key = decryption_helper(db)) == NULL) return EXIT_FAILURE;
    if (!insert_password(db, &password_obj)) return EXIT_FAILURE;
  }

  if (password_len != 0) {
    char *secret = NULL;
    if ((secret = random_password(password_len)) == NULL) {
      return EXIT_FAILURE;
    }

    fprintf(stdout, "%s\n", secret);
    free(secret);
  }

  if (import_file != NULL) {
    if (strlen(import_file) > 20) {
      fprintf(stderr, "Warning: Import file name too long\n");
      return EXIT_FAILURE;
    }

    char *real_path;
    if ((real_path = realpath(import_file, NULL)) == NULL) {
      fprintf(stderr, "Error: Failed to get realpath for: %s", import_file);
      return EXIT_FAILURE;
    }

    if ((db = initcrux()) == NULL) return EXIT_SUCCESS;
    if ((key = decryption_helper(db)) == NULL) {
      return EXIT_FAILURE;
    }

    import_pass(db, real_path);
    sqlite3_close(db);
  }

  if (export_file != NULL) {
    if (strlen(export_file) > 15) {
      fprintf(stderr, "Warning: Export file name too long: MAX_LEN =>15\n");
      return EXIT_FAILURE;
    }

    char export_file_path[256];
    if (getcwd(export_file_path, sizeof(export_file_path)) == NULL) {
      perror("Error: Failed to get current path");
      return EXIT_FAILURE;
    }

    export_file_path[strlen(export_file_path)] = '/';
    strncat(export_file_path, export_file, sizeof(char) * 15);
    // TODO: fix set path to set paths to CRUXPASS_DB instead.

    if ((db = initcrux()) == NULL) return EXIT_SUCCESS;
    if ((key = decryption_helper(db)) == NULL) return EXIT_FAILURE;
    if (!export_pass(db, export_file_path)) {
      sqlite3_close(db);
      fprintf(stdout, "Info: Passwords exported to: %s\n", export_file);
      return EXIT_FAILURE;
    };

    sqlite3_close(db);
  }

  if (password_id != 0) {
    /* WARNING: db object never used but necessary to initcrux() */
    if ((db = initcrux()) == NULL) return EXIT_SUCCESS;
    /* sqlite3_close(db); */
    if ((key = decryption_helper(db)) != NULL) {
      if (!delete_password_v2(db, password_id)) {
        sqlite3_close(db);
        return EXIT_FAILURE;
      }
    }

    sqlite3_close(db);
  }

  if (list != 0) {
    if ((db = initcrux()) == NULL) return EXIT_FAILURE;
    if ((key = decryption_helper(db)) == NULL) {
      sqlite3_close(db);
      return EXIT_FAILURE;
    }

    main_tui(db);
    sqlite3_close(db);
  }

  if (version != 0) {
    fprintf(stdout, "cruxpass version: %s\n", VERSION);
    fprintf(stdout, "Authur: %s\n", AUTHUR);
  }

  if (key != NULL) {
    sodium_memzero(key, KEY_LEN);
    sodium_free(key);
  }

  return EXIT_SUCCESS;
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
                    "cruxpass is a lightweight, command-line password manager designed to secure,\n"
                    "store and retrieve encrypted credentials. It uses an SQLite database to manage\n"
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
