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

/* #include "sqlcipher.h" */

#define PASS_MIN 8
#define PATH_LEN 256
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

sqlite3 *initcrux(void);

/**
 * Generates a random password of the specified length.
 *
 * @param password_len the length of the password to generate
 * @return a pointer to the generated password, or NULL on failure
 */
char *random_secret(int secret_len);
char *setpath(char *);

/**
 * Exports a password from the database to a file.
 * @param database_ptr the file pointer to the database
 * @param export_file the file path to export the password to
 * @return 1 on success, 0 on failure
 */
int export_secrets(sqlite3 *db, const char *export_file);

/**
 * Exports a password from the database to a file.
 *
 * @param database_ptr the file pointer to the database
 * @param export_file the file path to export the password to
 * @return 1 on success, 0 on failure
 */
int import_secrets(sqlite3 *db, char *import_file);

/**
 * Creates a new master password for the database.
 *
 * @param master_passd the master password to hash
 * @return 0 on success, 1 on failure
 */
int create_new_master_secret(sqlite3 *db, unsigned char *key);

#endif  // !CRUXPASS_H
