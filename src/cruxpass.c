#include "../include/cruxpass.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "../include/database.h"
#include "../include/enc.h"
#include "../include/sqlcipher.h"

char *random_password(int password_len) {
  if (password_len < PASS_MIN || password_len > PASSWORD_LENGTH) {
    printf("Warning: Password must be at least 8 & %d characters long\n", PASSWORD_LENGTH);
    return NULL;
  }

  char *password = NULL;
  char pass_bank[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ123456789#%&()_+={}[-]:<@>?";
  const int bank_len = sizeof(pass_bank) / sizeof(char);

  if ((password = calloc(sizeof(char) * password_len, sizeof(char))) == NULL) {
    perror("Error: Fail to creat password");
    return NULL;
  }

  if (sodium_init() == -1) {
    free(password);
    fprintf(stderr, "Error: Failed to initialize libsodium");
    return NULL;
  }

  for (int i = 0; i < password_len && i < bank_len; i++) {
    password[i] = pass_bank[(int)randombytes_uniform(bank_len - 1)];
  }

  return password;
}

/*TODO: use defined paths instead of changing dir*/
char *setpath(char *relative_path) {
  char *abs_path = NULL;
  char *home = NULL;

  if ((abs_path = calloc(PATH_MAX + 1, sizeof(char))) == NULL) {
    fprintf(stderr, "Error: Memory Allocation Failed: calloc\n");
    return NULL;
  }

  if ((home = getenv("HOME")) == NULL) {
    perror("Error: Failed to get home path");
    free(abs_path);
    return NULL;
  }

  snprintf(abs_path, PATH_MAX, "%s/", home);
  strncat(abs_path, relative_path, (PATH_MAX - strlen(home)));
  return abs_path;
}

int export_pass(sqlite3 *db, const char *export_file) {
  FILE *fp;
  const unsigned char *username = NULL;
  const unsigned char *description = NULL;
  const unsigned char *password = NULL;
  sqlite3_stmt *sql_stmt = NULL;

  if ((fp = fopen(export_file, "wb")) == NULL) {
    fprintf(stderr, "Error: Failed to open %s: %s", export_file, strerror(errno));
    return EXIT_FAILURE;
  }

  char *sql_str = "SELECT username, password_hash, description FROM passwords;";
  if (sqlite3_prepare_v2(db, sql_str, -1, &sql_stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Error: Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    fclose(fp);
    return 0;
  }

  fputs("Username,Password,Description\n", fp);
  while (sqlite3_step(sql_stmt) == SQLITE_ROW) {
    username = sqlite3_column_text(sql_stmt, 0);
    password = sqlite3_column_text(sql_stmt, 1);
    description = sqlite3_column_text(sql_stmt, 2);

    fprintf(fp, "%s,%s,%s\n", username, password, description);
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

int import_pass(sqlite3 *db, char *import_file) {
  FILE *fp;
  size_t line_number = 1;
  char *saveptr;
  char buffer[BUFFMAX + 1];
  password_t *password_obj = NULL;

  if ((fp = fopen(import_file, "r")) == NULL) {
    fprintf(stderr, "Error: Failed to open %s FILE: %s", import_file, strerror(errno));
    return 0;
  }

  if ((password_obj = malloc(sizeof(password_t))) == NULL) {
    fprintf(stderr, "Error: Memory Allocation Failed");
    fclose(fp);
    return 0;
  }

  while (fgets(buffer, BUFFMAX, fp) != NULL) {
    buffer[strcspn(buffer, "\n")] = '\0';

    if (!process_field(password_obj->username, USERNAME_LENGTH, strtok_r(buffer, ",", &saveptr), "Username",
                       line_number)) {
      line_number++;
      continue;
    }

    if (!process_field(password_obj->passd, PASSWORD_LENGTH, strtok_r(NULL, ",", &saveptr), "Password", line_number)) {
      line_number++;
      continue;
    }

    if (!process_field(password_obj->description, DESC_LENGTH, strtok_r(NULL, ",", &saveptr), "Description",
                       line_number)) {
      line_number++;
      continue;
    }

    /* TODO: proper error handling */
    insert_password(db, password_obj);
    line_number++;
    memset(password_obj, '\0', sizeof(password_t));
  }

  fclose(fp);
  free(password_obj);
  return 1;
}

sqlite3 *initcrux() {
  sqlite3 *db = NULL;
  int inited = init_sqlite();
  if (inited == C_RET_OKK)
    printf("Warning: Default password is: 'cruxpassisgr8!'. \nWarning: Change it with<< cruxpass -n >>\n");
  if (inited != C_ERR) db = open_db_wrap(P_DB, SQLITE_OPEN_READWRITE);

  return db;
}
