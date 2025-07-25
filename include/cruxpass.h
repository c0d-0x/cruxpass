/**
 * PROJECT: cruxpass - A simple password manager
 * AUTHOR: c0d_0x
 * MIT LICENSE
 */

#ifndef CRUXPASS_H
#define CRUXPASS_H
#define SQLITE_HAS_CODEC
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
#include <uchar.h>
#include <unistd.h>

#define PASS_MIN 8
#define MAX_PATH_LEN 256
#define MASTER_MAX_LEN 45
#define SECRET_MAX_LEN 128
#define USERNAME_MAX_LEN 30
#define DESC_MAX_LEN 100
#define CRUXPASS_DB "workspaces/cruxpass/.cruxpass/cruxpass.db"
#define AUTH_DB "workspaces/cruxpass/.cruxpass/auth.db"

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

char *random_secret(int secret_len);
char *setpath(char *);
int create_new_master_secret(sqlite3 *db, unsigned char *key);
int export_secrets(sqlite3 *db, const char *export_file);
int import_secrets(sqlite3 *db, char *import_file);
sqlite3 *initcrux(void);

#endif  // !CRUXPASS_H
