#include "../include/database.h"

#include <sodium/core.h>
#include <sodium/utils.h>
#include <sqlcipher/sqlite3.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "../include/cruxpass.h"
#include "../include/enc.h"
#include "../include/tui.h"

static char *auth_db_abs_path = NULL;
static char *cruxpass_db_abs_path = NULL;

void cleanup_paths(void) {
  free(auth_db_abs_path);
  free(cruxpass_db_abs_path);
  auth_db_abs_path = NULL;
  cruxpass_db_abs_path = NULL;
}

static int sql_exec_n_err(sqlite3 *db, char *sql_fmt_str, char *sql_err_msg,
                          int (*callback)(void *, int, char **, char **), void *callback_arg) {
  int rc = sqlite3_exec(db, sql_fmt_str, callback, callback_arg, &sql_err_msg);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Error: failed execute sql statement: %s\n", sql_err_msg);
    sqlite3_close(db);
    sqlite3_free(sql_err_msg);
    return 0;
  }
  return 1;
}

static sqlite3 *_open_db(char *db_name, int flags) {
  sqlite3 *db = NULL;
  int rc = sqlite3_open_v2(db_name, &db, flags, NULL);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Error: failed to open cruxpass_db.\n");
    return NULL;
  }

  return db;
}

sqlite3 *open_db_wrap(int8_t db_name_flag, int8_t flags) {
  if (db_name_flag == P_DB)
    return _open_db(cruxpass_db_abs_path, flags);
  else if (db_name_flag == S_DB)
    return _open_db(auth_db_abs_path, flags);

  return NULL;
}

int init_sqlite(void) {
  char *sql_err_msg = NULL;
  char *sql_fmt_str = NULL;
  sqlite3 *db = NULL;
  sqlite3 *secrets_db = NULL;
  unsigned char *key = NULL;

  hash_t hash_rec;
  hash_t *tmp_hash_rec = NULL;

  cruxpass_db_abs_path = setpath(CRUXPASS_DB);
  auth_db_abs_path = setpath(AUTH_DB);

  if (cruxpass_db_abs_path == NULL || auth_db_abs_path == NULL) {
    fprintf(stderr, "Error: Failed to setup absolute paths\n");
    return C_ERR;
  }

  char *psswd_str = "cruxpassisgr8!";
  /*NOTE:  a temporary key, hash, and salt are generated for the creation of the new database*/
  if ((db = _open_db(cruxpass_db_abs_path, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE)) == NULL) {
    return C_ERR;
  }

  if ((secrets_db = _open_db(auth_db_abs_path, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE)) == NULL) {
    sqlite3_close(db);
    return C_ERR;
  }

  if ((tmp_hash_rec = fetch_hash()) != NULL) {
    free(tmp_hash_rec);
    sqlite3_close(db);
    sqlite3_close(secrets_db);
    return C_RET_OK;
  }

  if (sodium_init() == -1) return C_ERR;
  if ((key = (unsigned char *)sodium_malloc(KEY_LEN)) == NULL) {
    fprintf(stderr, "Error: Memory allocation failed.\n");
    sqlite3_close(db);
    sqlite3_close(secrets_db);
    return C_ERR;
  }

  randombytes_buf(hash_rec.salt, crypto_pwhash_SALTBYTES);
  if (!generate_key_or_hash(key, hash_rec.hash, psswd_str, hash_rec.salt, GEN_HASH | GEN_KEY)) {
    sodium_free(key);
    sqlite3_close(db);
    sqlite3_close(secrets_db);
    return C_ERR;
  }

  if ((db = decrypt(db, key)) == NULL) {
    sodium_free(key);
    sqlite3_close(secrets_db);
    return C_ERR;
  }

  sodium_free(key);
  sql_fmt_str =
      "CREATE TABLE IF NOT EXISTS secrets ( secret_id INTEGER "
      "PRIMARY KEY, hash TEXT NOT NULL, salt TEXT NOT NULL );";
  if (!sql_exec_n_err(secrets_db, sql_fmt_str, sql_err_msg, NULL, NULL)) {
    sqlite3_close(db);
    sqlite3_close(secrets_db);
    return C_ERR;
  }

  if (!insert_hash(secrets_db, &hash_rec)) {
    sqlite3_close(db);
    sqlite3_close(secrets_db);
    return C_ERR;
  }

  sqlite3_close(secrets_db);
  sql_fmt_str =
      "CREATE TABLE IF NOT EXISTS passwords ( secret_id INTEGER PRIMARY KEY, username TEXT NOT NULL, "
      "secret TEXT NOT NULL, description TEXT NOT NULL, date_added TEXT DEFAULT CURRENT_DATE);";
  if (!sql_exec_n_err(db, sql_fmt_str, sql_err_msg, NULL, NULL)) {
    sqlite3_close(db);
    return C_ERR;
  }

  sqlite3_close(db);
  return C_RET_OKK;
}

int insert_record(sqlite3 *db, secret_t *record) {
  sqlite3_stmt *sql_stmt = NULL;
  char *sql_fmt_str = NULL;
  if (record == NULL) {
    fprintf(stderr, "Error: empty record\n");
    return 0;
  }

  sql_fmt_str =
      "INSERT INTO passwords (username, secret,description ) "
      "VALUES (?, ?, ?);";
  if (sqlite3_prepare_v2(db, sql_fmt_str, -1, &sql_stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Error: failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  if (sqlite3_bind_text(sql_stmt, 1, record->username, -1, SQLITE_STATIC) != SQLITE_OK ||
      sqlite3_bind_text(sql_stmt, 2, record->secret, -1, SQLITE_STATIC) != SQLITE_OK ||
      sqlite3_bind_text(sql_stmt, 3, record->description, -1, SQLITE_STATIC) != SQLITE_OK) {
    fprintf(stderr, "Error: failed to bind sql statement: %s", sqlite3_errmsg(db));
    return 0;
  }

  if (sqlite3_step(sql_stmt) != SQLITE_DONE) {
    fprintf(stderr, "Error: failed to execute statement: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(sql_stmt);
    return 0;
  }

  sqlite3_finalize(sql_stmt);
  return 1;
}

int delete_record(sqlite3 *db, int record_id) {
  sqlite3_stmt *sql_stmt = NULL;
  char *sql_fmt_str = NULL;

  sql_fmt_str = "DELETE FROM passwords WHERE secret_id = ?;";
  if (sqlite3_prepare_v2(db, sql_fmt_str, -1, &sql_stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Error: failed to prepare statement: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return 0;
  }

  if (sqlite3_bind_int(sql_stmt, 1, record_id) != SQLITE_OK) {
    fprintf(stderr, "Error: failed to bind sql statement: %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    return 0;
  }

  if (sqlite3_step(sql_stmt) != SQLITE_DONE) {
    fprintf(stderr, "Error: failed to execute statement\n");
    sqlite3_close(db);
    sqlite3_finalize(sql_stmt);
    return 0;
  }

  fprintf(stderr, "Note: password< %02d > is deleted", record_id);
  sqlite3_finalize(sql_stmt);
  return 1;
}

static int sql_prep_n_exec(sqlite3 *db, char *sql_fmt_str, sqlite3_stmt *sql_stmt, const char *field, int secret_id) {
  if (sqlite3_prepare_v2(db, sql_fmt_str, -1, &sql_stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Error: failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  if (sqlite3_bind_text(sql_stmt, 1, field, -1, SQLITE_STATIC) != SQLITE_OK ||
      sqlite3_bind_int(sql_stmt, 2, secret_id) != SQLITE_OK) {
    fprintf(stderr, "Error: failed to bind sql statement: %s", sqlite3_errmsg(db));
    sqlite3_reset(sql_stmt);
    sqlite3_finalize(sql_stmt);
    return 0;
  }

  if (sqlite3_step(sql_stmt) != SQLITE_DONE) {
    fprintf(stderr, "Error: failed to execute statement: %s\n", sqlite3_errmsg(db));
    sqlite3_reset(sql_stmt);
    sqlite3_finalize(sql_stmt);
    return 0;
  }

  sqlite3_reset(sql_stmt);
  sqlite3_finalize(sql_stmt);
  return 1;
}

int update_record(sqlite3 *db, secret_t *secret_record, int record_id, uint8_t flags) {
  bool updated = false;
  char *sql_fmt_str = NULL;
  sqlite3_stmt *sql_stmt = NULL;

  if (flags & UPDATE_DESCRIPTION) {
    if (secret_record->description[0] == '\0') {
      fprintf(stderr, "Error: empty description\n");
      sqlite3_close(db);
      return 0;
    }

    sql_fmt_str = "UPDATE passwords SET description = ? WHERE secret_id = ?;";
    if (!sql_prep_n_exec(db, sql_fmt_str, sql_stmt, secret_record->description, record_id)) {
      sqlite3_close(db);
      return 0;
    }

    sqlite3_reset(sql_stmt);
    sqlite3_finalize(sql_stmt);
    updated = true;
  }

  if (flags & UPDATE_SECRET) {
    if (secret_record->secret[0] == '\0') {
      fprintf(stderr, "Error: empty secret\n");
      sqlite3_close(db);
      return 0;
    }
    sql_fmt_str = "UPDATE passwords SET secret = ? WHERE secret_id = ?;";
    if (!sql_prep_n_exec(db, sql_fmt_str, sql_stmt, secret_record->secret, record_id)) {
      sqlite3_close(db);
      return 0;
    }

    sqlite3_reset(sql_stmt);
    sqlite3_finalize(sql_stmt);
    updated = true;
  }

  if (flags & UPDATE_USERNAME) {
    if (secret_record->username[0] == '\0') {
      fprintf(stderr, "Error: empty username\n");
      sqlite3_close(db);
      return 0;
    }
    sql_fmt_str = "UPDATE passwords SET username = ? WHERE secret_id = ?;";
    if (!sql_prep_n_exec(db, sql_fmt_str, sql_stmt, secret_record->username, record_id)) {
      sqlite3_close(db);
      return 0;
    }

    sqlite3_reset(sql_stmt);
    sqlite3_finalize(sql_stmt);
    updated = true;
  }

  if (updated) printf("Info: password ID: %d updated", record_id);
  sqlite3_close(db);
  return 1;
}

int load_records(sqlite3 *db, record_array_t *records) {
  char *err_msg = 0;

  const char *sql = "SELECT secret_id, username, description FROM passwords ORDER BY secret_id;";
  if (sqlite3_exec(db, sql, callback_feed_tui, records, &err_msg) != SQLITE_OK) {
    fprintf(stderr, "Error: SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    sqlite3_close(db);
    return 0;
  }

  return 1;
}

// TODO: use callbacks to feed records dynamic array instead.

hash_t *fetch_hash(void) {
  sqlite3 *db = NULL;
  sqlite3_stmt *sql_stmt = NULL;
  hash_t *hash_rec = NULL;
  const unsigned char *hash = NULL;
  const unsigned char *salt = NULL;
  char *sql_str = "SELECT hash, salt FROM secrets;";

  if ((db = _open_db(auth_db_abs_path, SQLITE_OPEN_READWRITE)) == NULL) {
    return NULL;
  }

  if (sqlite3_prepare_v2(db, sql_str, -1, &sql_stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Error: failed to prepare statement: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return NULL;
  }

  bool done = false;
  while (sqlite3_step(sql_stmt) == SQLITE_ROW && !done) {
    hash = sqlite3_column_text(sql_stmt, 0);
    salt = sqlite3_column_text(sql_stmt, 1);
    if (hash != NULL || salt != NULL) {
      if ((hash_rec = calloc(1, sizeof(hash_t))) == NULL) {
        fprintf(stderr, "Error: Memory allocation failed: calloc\n");
        sqlite3_close(db);
        return NULL;
      }

      memcpy(hash_rec->hash, hash, PASS_HASH_LEN);
      memcpy(hash_rec->salt, salt, SALT_HASH_LEN);
      done = true;
    }
  }

  if (hash_rec == NULL) {
    fprintf(stderr, "Warning: empty secrets db\n");
    sqlite3_close(db);
    return NULL;
  }

  sqlite3_finalize(sql_stmt);
  sqlite3_close(db);
  return hash_rec;
}

int update_hash(hash_t *hash_rec) {
  sqlite3 *db = NULL;
  char *sql_fmt_str = NULL;
  sqlite3_stmt *sql_stmt = NULL;
  if (hash_rec == NULL) {
    fprintf(stderr, "Error: empty hash\n");
    return 0;
  }

  if ((db = _open_db(auth_db_abs_path, SQLITE_OPEN_READWRITE)) == NULL) {
    return 0;
  }

  sql_fmt_str = "UPDATE secrets SET hash = ? WHERE secret_id = ?;";
  if (!sql_prep_n_exec(db, sql_fmt_str, sql_stmt, hash_rec->hash, 1)) {
    sqlite3_close(db);
    return 0;
  }

  sqlite3_reset(sql_stmt);
  sqlite3_finalize(sql_stmt);

  sql_fmt_str = "UPDATE secrets SET salt= ? WHERE secret_id = ?;";
  if (!sql_prep_n_exec(db, sql_fmt_str, sql_stmt, (char *)hash_rec->salt, 1)) {
    sqlite3_close(db);
    return 0;
  }

  sqlite3_close(db);
  sqlite3_reset(sql_stmt);
  sqlite3_finalize(sql_stmt);
  return 1;
}

int insert_hash(sqlite3 *db, hash_t *hash_rec) {
  sqlite3_stmt *sql_stmt = NULL;
  char *sql_fmt_str = NULL;
  if (hash_rec == NULL) {
    fprintf(stderr, "Error: empty record\n");
    return 0;
  }

  sql_fmt_str = "INSERT INTO secrets (hash, salt) VALUES (?, ?);";
  if (sqlite3_prepare_v2(db, sql_fmt_str, -1, &sql_stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Error: failed to prepare statement: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return 0;
  }

  if (sqlite3_bind_text(sql_stmt, 1, hash_rec->hash, -1, SQLITE_STATIC) != SQLITE_OK ||
      sqlite3_bind_text(sql_stmt, 2, (char *)hash_rec->salt, -1, SQLITE_STATIC) != SQLITE_OK) {
    fprintf(stderr, "Error: failed to bind sql statement: %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    return 0;
  }

  if (sqlite3_step(sql_stmt) != SQLITE_DONE) {
    fprintf(stderr, "Error: failed to execute statement: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(sql_stmt);
    sqlite3_close(db);
    return 0;
  }

  sqlite3_finalize(sql_stmt);
  return 1;
}
