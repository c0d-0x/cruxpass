/**
 * PROJECT: cruxpass - A simple password manager
 * AUTHOR: c0d_0x @ 2025
 * MIT LICENSE
 */

#ifndef CRUXPASS_H
#define CRUXPASS_H
#ifndef SQLITE_HAS_CODEC
#define SQLITE_HAS_CODEC
#endif
#include <ctype.h>
#include <sodium.h>
#include <sodium/core.h>
#include <sodium/crypto_pwhash.h>
#include <sodium/utils.h>
#include <sqlcipher/sqlite3.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define DESC_MAX_LEN 100
#define FILE_PATH_LEN 32
#define MASTER_MAX_LEN 45
#define MAX_PATH_LEN 255
#define SECRET_MAX_LEN 128
#define SECRET_MIN_LEN 8
#define USERNAME_MAX_LEN 30

#ifndef CRUXPASS_DB
#define CRUXPASS_DB "cruxpass.db"
#endif

#ifndef AUTH_DB
#define AUTH_DB "auth.db"
#endif

#ifndef CRUXPASS_RUNDIR
#define CRUXPASS_RUNDIR ".local/share/cruxpass"  // default ~/.local/share/cruxpass/
#endif

typedef struct {
  ssize_t id;
  char username[USERNAME_MAX_LEN + 1];
  char secret[SECRET_MAX_LEN + 1];
  char description[DESC_MAX_LEN + 1];
} secret_t;

typedef enum {
  C_ERR,     // C_ERR:     Error
  C_RET_OK,  // C_RET_0K:  Custom OK 1
  C_RET_OKK  // C_RET_OKK: Custom OK 2
} ERROR_T;

typedef struct {
  bool uppercase;
  bool lowercase;
  bool numbers;
  bool symbols;
  bool exclude_ambiguous;
} secret_bank_options_t;

char *random_secret(int secret_len, secret_bank_options_t *bank_options);
char *setpath(char *);
int create_new_master_secret(sqlite3 *db, unsigned char *key);
int export_secrets(sqlite3 *db, const char *export_file);
int import_secrets(sqlite3 *db, char *import_file);
sqlite3 *initcrux(char *run_dir);
char *init_secret_bank(const secret_bank_options_t *options);

#endif  // !CRUXPASS_H
