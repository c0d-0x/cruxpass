#include "../include/cruxpass.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <wchar.h>

#include "../include/database.h"
#include "../include/enc.h"

extern char *cruxpass_db_path;
extern char *auth_db_path;

char *random_secret(int secret_len, secret_bank_options_t *bank_options) {
  if (secret_len < SECRET_MIN_LEN || secret_len > SECRET_MAX_LEN) {
    printf("Warning: Password must be at least 8 or %d characters long\n", SECRET_MAX_LEN);
    return NULL;
  }

  char *secret = NULL;
  char *secret_bank = NULL;
  if ((secret_bank = init_secret_bank(bank_options)) == NULL) {
    fprintf(stderr, "Error: Fail to init password");
    return NULL;
  }
  const int bank_len = strlen(secret_bank);

  if ((secret = calloc(1, secret_len)) == NULL) {
    fprintf(stderr, "Error: Fail to creat password");
    free(secret_bank);
    return NULL;
  }

  if (sodium_init() == -1) {
    free(secret_bank);
    free(secret);
    fprintf(stderr, "Error: Failed to initialize libsodium\n");
    return NULL;
  }

  for (int i = 0; i < secret_len; i++) {
    secret[i] = secret_bank[(int)randombytes_uniform(bank_len)];
  }

  free(secret_bank);
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
    return 0;
  }

  char *sql_str = "SELECT username, secret, description FROM secrets;";
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
  return 1;
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

    if (!insert_record(db, secret_record)) fprintf(stderr, "Error: Failed to insert record at line: %ld", line_number);
    line_number++;
    memset(secret_record, 0, sizeof(secret_t));
  }

  fclose(fp);
  free(secret_record);
  return 1;
}

static bool create_run_dir(const char *path) {
  int ret = mkdir(path, 0776);
  if (ret == 0)
    fprintf(stderr, "Note: Run directory created\n");
  else if (errno != EEXIST) {
    fprintf(stderr, "Error: Failed to create run directory: %s\n", strerror(errno));
    return false;
  }

  return true;
}

static char *set_path(char *path, char *file_name) {
  char *full_path = NULL;

  if (path == NULL || file_name == NULL) return NULL;
  if ((full_path = calloc(MAX_PATH_LEN + 1, sizeof(char))) == NULL) {
    fprintf(stderr, "Error: Failed to allocate Memory\n");
    return NULL;
  }

  if (snprintf(full_path, MAX_PATH_LEN, "%s/%s", path, file_name) <= 0) return NULL;

  return full_path;
}

static bool validate_run_dir(char *path) {
  bool path_is_dynamic = false;
  if (path == NULL) {
    char *home = NULL;
    if ((home = getenv("HOME")) == NULL) return false;

    if ((path = calloc(MAX_PATH_LEN + 1, sizeof(char))) == NULL) {
      fprintf(stderr, "Error: Failed to allocate Memory\n");
      return false;
    }

    if (snprintf(path, MAX_PATH_LEN, "%s/%s", home, CRUXPASS_RUNDIR) <= 0) {
      free(path);
      return false;
    }

    if (!create_run_dir(path)) {
      free(path);
      return false;
    }

    path_is_dynamic = true;
  } else if (strlen(path) > MAX_PATH_LEN - 16) {
    fprintf(stderr, "Error: Run directory path too long\n");
    return false;
  }

  struct stat file_stat;

  if (stat(path, &file_stat) != 0) {
    if (path_is_dynamic) free(path);
    fprintf(stderr, "Error: Failed to validate path\n");
    fprintf(stderr, "Warning: Default run directory is ~/.local/share/cruxpass/ (create it if missing).\n");
    return false;
  }

  if (!S_ISDIR(file_stat.st_mode)) {
    if (path_is_dynamic) free(path);
    fprintf(stderr, "Error: [ %s ] is not a valid directory\n", path);
    return false;
  }

  cruxpass_db_path = set_path(path, CRUXPASS_DB);
  auth_db_path = set_path(path, AUTH_DB);

  if (path_is_dynamic) free(path);
  if (cruxpass_db_path == NULL || auth_db_path == NULL) return false;

  return true;
}

sqlite3 *initcrux(char *run_dir) {
  if (!validate_run_dir(run_dir)) return NULL;
  int inited = init_sqlite();
  if (inited == C_RET_OKK) {
    fprintf(stderr, "Note: New password created\nWarning: Retry your opetation\n");
    return NULL;
  }

  sqlite3 *db = NULL;
  if (inited != C_ERR) db = open_db(cruxpass_db_path, SQLITE_OPEN_READWRITE);

  return db;
}

char *init_secret_bank(const secret_bank_options_t *options) {
  char all_upper[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  char all_lower[] = "abcdefghijklmnopqrstuvwxyz";
  char all_numbers[] = "0123456789";
  char all_symbols[] = "#%&()_+={}[-]:<@>?";

  char unambiguous_upper[] = "ABCDEFGHJKLMNPQRSTUVWXYZ";
  char unambiguous_lower[] = "abcdefghijkmnopqrstuvwxyz";
  char unambiguous_numbers[] = "123456789";
  char unambiguous_symbols[] = "#%&()_+={}[-]:<@>?";

  if (options == NULL) return NULL;
  size_t bank_len = 0;

  if (options->exclude_ambiguous) {
    if (options->uppercase) bank_len += strlen(unambiguous_upper);
    if (options->lowercase) bank_len += strlen(unambiguous_lower);
    if (options->numbers) bank_len += strlen(unambiguous_numbers);
    if (options->symbols) bank_len += strlen(unambiguous_symbols);
  } else {
    if (options->uppercase) bank_len += strlen(all_upper);
    if (options->lowercase) bank_len += strlen(all_lower);
    if (options->numbers) bank_len += strlen(all_numbers);
    if (options->symbols) bank_len += strlen(all_symbols);
  }

  if (bank_len == 0) return NULL;  // User didn't use any of the available banks.
  bank_len += 1;                   // Add 1 for NULL termination character.
  char *bank = NULL;
  if ((bank = calloc(bank_len, sizeof(char))) == NULL) return NULL;

  if (options->exclude_ambiguous) {
    if (options->uppercase) strncat(bank, unambiguous_upper, strlen(unambiguous_upper));
    if (options->lowercase) strncat(bank, unambiguous_lower, strlen(unambiguous_lower));
    if (options->numbers) strncat(bank, unambiguous_numbers, strlen(unambiguous_numbers));
    if (options->symbols) strncat(bank, unambiguous_symbols, strlen(unambiguous_symbols));
  } else {
    if (options->uppercase) strncat(bank, all_upper, strlen(all_upper));
    if (options->lowercase) strncat(bank, all_lower, strlen(all_lower));
    if (options->numbers) strncat(bank, all_numbers, strlen(all_numbers));
    if (options->symbols) strncat(bank, all_symbols, strlen(all_symbols));
  }

  return bank;
}
