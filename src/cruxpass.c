#include "../include/cruxpass.h"

#include <errno.h>
#include <sqlcipher/sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "../include/database.h"
#include "../include/enc.h"

char *random_secret(int secret_len) {
  if (secret_len < PASS_MIN || secret_len > SECRET_MAX_LEN) {
    printf("Warning: Password must be at least 8 & %d characters long\n", SECRET_MAX_LEN);
    return NULL;
  }

  char *secret = NULL;
  char secret_bank[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ123456789#%&()_+={}[-]:<@>?";
  const int bank_len = sizeof(secret_bank) / sizeof(char);

  if ((secret = calloc(1, secret_len)) == NULL) {
    perror("Error: Fail to creat password");
    return NULL;
  }

  if (sodium_init() == -1) {
    free(secret);
    fprintf(stderr, "Error: Failed to initialize libsodium");
    return NULL;
  }

  for (int i = 0; i < secret_len; i++) {
    secret[i] = secret_bank[(int)randombytes_uniform(bank_len - 1)];
  }

  return secret;
}

int export_secrets(sqlite3 *db, const char *export_file) {
  FILE *fp;
  const unsigned char *username = NULL;
  const unsigned char *description = NULL;
  const unsigned char *secret = NULL;
  sqlite3_stmt *sql_stmt = NULL;

  if ((fp = fopen(export_file, "wb")) == NULL) {
    fprintf(stderr, "Error: Failed to open %s: %s", export_file, strerror(errno));
    return EXIT_FAILURE;
  }

  char *sql_str = "SELECT username, secret, description FROM passwords;";
  if (sqlite3_prepare_v2(db, sql_str, -1, &sql_stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Error: Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    fclose(fp);
    return 0;
  }

  fputs("Username,Secret,Description\n", fp);
  while (sqlite3_step(sql_stmt) == SQLITE_ROW) {
    username = sqlite3_column_text(sql_stmt, 0);
    secret = sqlite3_column_text(sql_stmt, 1);
    description = sqlite3_column_text(sql_stmt, 2);

    fprintf(fp, "%s,%s,%s\n", username, secret, description);
  }

  fclose(fp);
  sqlite3_finalize(sql_stmt);
  return EXIT_SUCCESS;
}

/**
 * @field: password_t field
 * @max_length: field MAX, a const
 * @field_name: for error handling
 * @line_number also for error handling
 */
static int process_field(char *field, const int max_length, char *token, const char *field_name, size_t line_number) {
  if (token == NULL) {
    fprintf(stderr, "Error: Missing %s at line %ld\n", field_name, line_number);
    return 0;
  }
  if ((const int)strlen(token) > max_length) {
    fprintf(stderr, "Error: %s at line %ld is more than %d characters\n", field_name, line_number, max_length);
    return 0;
  }
  strncpy(field, token, max_length);
  return 1;
}

int import_secrets(sqlite3 *db, char *import_file) {
  FILE *fp;
  size_t line_number = 1;
  char *saveptr;
  char buffer[BUFFMAX + 1];
  secret_t *secret_record = NULL;

  if ((fp = fopen(import_file, "r")) == NULL) {
    fprintf(stderr, "Error: Failed to open %s FILE: %s", import_file, strerror(errno));
    return 0;
  }

  if ((secret_record = malloc(sizeof(secret_t))) == NULL) {
    fprintf(stderr, "Error: Memory Allocation Failed");
    fclose(fp);
    return 0;
  }

  while (fgets(buffer, BUFFMAX, fp) != NULL) {
    buffer[strcspn(buffer, "\n")] = '\0';

    if (!process_field(secret_record->username, USERNAME_MAX_LEN, strtok_r(buffer, ",", &saveptr), "Username",
                       line_number)) {
      line_number++;
      continue;
    }

    if (!process_field(secret_record->secret, SECRET_MAX_LEN, strtok_r(NULL, ",", &saveptr), "Password", line_number)) {
      line_number++;
      continue;
    }

    if (!process_field(secret_record->description, DESC_MAX_LEN, strtok_r(NULL, ",", &saveptr), "Description",
                       line_number)) {
      line_number++;
      continue;
    }

    /* TODO: proper error handling */
    insert_record(db, secret_record);
    line_number++;
    memset(secret_record, '\0', sizeof(secret_t));
  }

  fclose(fp);
  free(secret_record);
  return 1;
}

sqlite3 *initcrux() {
  sqlite3 *db = NULL;
  int inited = init_sqlite();
  if (inited == C_RET_OKK)
    printf("Warning: Default password is: 'cruxpassisgr8!'. \nWarning: Change it with<< cruxpass -n >>\n");
  if (inited != C_ERR) db = open_db(CRUXPASS_DB, SQLITE_OPEN_READWRITE);

  return db;
}
