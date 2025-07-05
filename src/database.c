#include "../include/database.h"

#include <ncurses.h>
#include <sodium/core.h>
#include <sodium/utils.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/cruxpass.h"
#include "../include/enc.h"
#include "../include/tui.h"

static char *secrets_db_abs_path = NULL;
static char *pass_db_abs_path = NULL;

void cleanup_paths(void) {
  free(secrets_db_abs_path);
  free(pass_db_abs_path);
  secrets_db_abs_path = NULL;
  pass_db_abs_path = NULL;
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

static sqlite3 *open_db(char *db_name, int flags) {
  sqlite3 *db = NULL;
  int rc = sqlite3_open_v2(db_name, &db, flags, NULL);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Error: failed to open cruxpass_db.\n");
    return NULL;
  }
  return db;
}

sqlite3 *open_db_wrap(int8_t db_name_flag, int flags) {
  if (db_name_flag == P_DB)
    return open_db(pass_db_abs_path, flags);
  else if (db_name_flag == S_DB)
    return open_db(secrets_db_abs_path, flags);

  return NULL;
}

int init_sqlite(void) {
  char *sql_err_msg = NULL;
  char *sql_fmt_str = NULL;
  sqlite3 *db = NULL;
  sqlite3 *secrets_db = NULL;
  char *psswd_str = NULL;
  unsigned char *key = NULL;

  hashed_pass_t hash_obj;
  hashed_pass_t *tmp_hash_obj = NULL;

  pass_db_abs_path = setpath(CRUXPASS_DB);
  secrets_db_abs_path = setpath(AUTH_DB);

  if (pass_db_abs_path == NULL || secrets_db_abs_path == NULL) {
    fprintf(stderr, "Error: Failed to setup absolute paths\n");
    return C_ERR;
  }

  psswd_str = "cruxpassisgr8!";
  /*NOTE:  a temporary key, hash, and salt are generated for the creation of the new database*/
  if ((db = open_db(pass_db_abs_path, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE)) == NULL) {
    return C_ERR;
  }

  if ((secrets_db = open_db(secrets_db_abs_path, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE)) == NULL) {
    sqlite3_close(db);
    return C_ERR;
  }

  if ((tmp_hash_obj = fetch_hash()) != NULL) {
    free(tmp_hash_obj);
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

  randombytes_buf(hash_obj.salt, crypto_pwhash_SALTBYTES);
  if (!generate_key_pass_hash(key, hash_obj.hash, psswd_str, hash_obj.salt, GEN_HASH | GEN_KEY)) {
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
      "CREATE TABLE IF NOT EXISTS secrets ( id_secret INTEGER "
      "PRIMARY KEY, hash TEXT NOT NULL, salt TEXT NOT NULL );";
  if (!sql_exec_n_err(secrets_db, sql_fmt_str, sql_err_msg, NULL, NULL)) {
    sqlite3_close(db);
    sqlite3_close(secrets_db);
    return C_ERR;
  }

  if (!insert_secret(secrets_db, &hash_obj)) {
    sqlite3_close(db);
    sqlite3_close(secrets_db);
    return C_ERR;
  }

  sqlite3_close(secrets_db);
  sql_fmt_str =
      "CREATE TABLE IF NOT EXISTS passwords ( id_password INTEGER PRIMARY KEY, username TEXT NOT NULL, "
      "password_hash TEXT NOT NULL, description TEXT NOT NULL, date_added TEXT DEFAULT CURRENT_DATE);";
  if (!sql_exec_n_err(db, sql_fmt_str, sql_err_msg, NULL, NULL)) {
    sqlite3_close(db);
    return C_ERR;
  }

  sqlite3_close(db);
  return C_RET_OKK;
}

int insert_password(sqlite3 *db, password_t *password_obj) {
  sqlite3_stmt *sql_stmt = NULL;
  char *sql_fmt_str = NULL;
  if (password_obj == NULL) {
    fprintf(stderr, "Error: empty password_obj\n");
    return 0;
  }

  sql_fmt_str =
      "INSERT INTO passwords (username, password_hash,description ) "
      "VALUES (?, ?, ?);";
  if (sqlite3_prepare_v2(db, sql_fmt_str, -1, &sql_stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Error: failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  if (sqlite3_bind_text(sql_stmt, 1, password_obj->username, -1, SQLITE_STATIC) != SQLITE_OK ||
      sqlite3_bind_text(sql_stmt, 2, password_obj->passd, -1, SQLITE_STATIC) != SQLITE_OK ||
      sqlite3_bind_text(sql_stmt, 3, password_obj->description, -1, SQLITE_STATIC) != SQLITE_OK) {
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

int delete_password_v2(sqlite3 *db, int password_id) {
  sqlite3_stmt *sql_stmt = NULL;
  char *sql_fmt_str = NULL;

  sql_fmt_str = "DELETE FROM passwords WHERE id_password = ?;";
  if (sqlite3_prepare_v2(db, sql_fmt_str, -1, &sql_stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Error: failed to prepare statement: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return 0;
  }

  if (sqlite3_bind_int(sql_stmt, 1, password_id) != SQLITE_OK) {
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

  fprintf(stderr, "Note: password< %02d > is deleted", password_id);
  sqlite3_finalize(sql_stmt);
  return 1;
}

static int sql_prep_n_exec(sqlite3 *db, char *sql_fmt_str, sqlite3_stmt *sql_stmt, const char *field, int password_id) {
  if (sqlite3_prepare_v2(db, sql_fmt_str, -1, &sql_stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Error: failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  if (sqlite3_bind_text(sql_stmt, 1, field, -1, SQLITE_STATIC) != SQLITE_OK ||
      sqlite3_bind_int(sql_stmt, 2, password_id) != SQLITE_OK) {
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

int update_password(char *db_name, char *description, char *passwd_hash, char *username, int password_id,
                    uint8_t flags) {
  sqlite3 *db;
  int updated = 0;
  char *sql_fmt_str = NULL;
  sqlite3_stmt *sql_stmt = NULL;
  if (db_name == NULL) {
    fprintf(stderr, "Error: empty DB name\n");
    return 0;
  }

  if ((db = open_db(db_name, SQLITE_OPEN_READWRITE)) == NULL) {
    return 0;
  }

  if (flags & UPDATE_DESCRIPTION) {
    if (description == NULL) {
      fprintf(stderr, "Error: empty description\n");
      sqlite3_close(db);
      return 0;
    }
    sql_fmt_str = "UPDATE passwords SET description = ? WHERE id_password = ?;";
    if (!sql_prep_n_exec(db, sql_fmt_str, sql_stmt, description, password_id)) {
      sqlite3_close(db);
      return 0;
    }

    sqlite3_reset(sql_stmt);
    sqlite3_finalize(sql_stmt);
    updated = 1;
  }

  if (flags & UPDATE_PASSWORD) {
    if (passwd_hash == NULL) {
      fprintf(stderr, "Error: empty password_hash\n");
      sqlite3_close(db);
      return 0;
    }
    sql_fmt_str = "UPDATE passwords SET password_hash = ? WHERE id_password = ?;";
    if (!sql_prep_n_exec(db, sql_fmt_str, sql_stmt, passwd_hash, password_id)) {
      sqlite3_close(db);
      return 0;
    }

    sqlite3_reset(sql_stmt);
    sqlite3_finalize(sql_stmt);
    updated = 1;
  }

  if (flags & UPDATE_USERNAME) {
    if (username == NULL) {
      fprintf(stderr, "Error: empty username\n");
      sqlite3_close(db);
      return 0;
    }
    sql_fmt_str = "UPDATE passwords SET username = ? WHERE id_password = ?;";
    if (!sql_prep_n_exec(db, sql_fmt_str, sql_stmt, username, password_id)) {
      sqlite3_close(db);
      return 0;
    }

    sqlite3_reset(sql_stmt);
    sqlite3_finalize(sql_stmt);
    updated = 1;
  }

  if (updated) printf("Info: password ID: %d updated", password_id);
  sqlite3_close(db);
  return 1;
}

int load_data_from_db(sqlite3 *db, record_array_t *records) {
  char *err_msg = 0;

  const char *sql = "SELECT id_password, username, description FROM passwords ORDER BY id_password;";
  if (sqlite3_exec(db, sql, callback_feed_tui, records, &err_msg) != SQLITE_OK) {
    fprintf(stderr, "Error: SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    sqlite3_close(db);
    return 0;
  }

  return 1;
}

// TODO: use callbacks to feed records dynamic array instead.

hashed_pass_t *fetch_hash(void) {
  sqlite3 *db = NULL;
  sqlite3_stmt *sql_stmt = NULL;
  hashed_pass_t *hash_obj = NULL;
  const unsigned char *hash = NULL;
  const unsigned char *salt = NULL;
  char *sql_str = "SELECT hash, salt FROM secrets;";

  if ((db = open_db(secrets_db_abs_path, SQLITE_OPEN_READWRITE)) == NULL) {
    return NULL;
  }

  if (sqlite3_prepare_v2(db, sql_str, -1, &sql_stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Error: failed to prepare statement: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return NULL;
  }

  uint8_t done = 0;
  while (sqlite3_step(sql_stmt) == SQLITE_ROW && !done) {
    hash = sqlite3_column_text(sql_stmt, 0);
    salt = sqlite3_column_text(sql_stmt, 1);
    if (hash != NULL || salt != NULL) {
      if ((hash_obj = calloc(1, sizeof(hashed_pass_t))) == NULL) {
        fprintf(stderr, "Error: Memory allocation failed: calloc\n");
        sqlite3_close(db);
        return NULL;
      }

      memcpy(hash_obj->hash, hash, PASS_HASH_LEN);
      memcpy(hash_obj->salt, salt, SALT_HASH_LEN);
      done = 1;
    }
  }

  if (hash_obj == NULL) {
    fprintf(stderr, "Warning: empty secrets db\n");
    sqlite3_close(db);
    return NULL;
  }

  sqlite3_finalize(sql_stmt);
  sqlite3_close(db);
  return hash_obj;
}

int update_secret(hashed_pass_t *hash_obj) {
  sqlite3 *db;
  char *sql_fmt_str = NULL;
  sqlite3_stmt *sql_stmt = NULL;
  if (hash_obj == NULL) {
    fprintf(stderr, "Error: empty hash\n");
    return 0;
  }

  if ((db = open_db(secrets_db_abs_path, SQLITE_OPEN_READWRITE)) == NULL) {
    return 0;
  }
  sql_fmt_str = "UPDATE secrets SET hash = ? WHERE id_secret = ?;";
  if (!sql_prep_n_exec(db, sql_fmt_str, sql_stmt, hash_obj->hash, 1)) {
    sqlite3_close(db);
    return 0;
  }

  sqlite3_reset(sql_stmt);
  sqlite3_finalize(sql_stmt);

  sql_fmt_str = "UPDATE secrets SET salt= ? WHERE id_secret = ?;";
  if (!sql_prep_n_exec(db, sql_fmt_str, sql_stmt, (char *)hash_obj->salt, 1)) {
    sqlite3_close(db);
    return 0;
  }

  sqlite3_close(db);
  sqlite3_reset(sql_stmt);
  sqlite3_finalize(sql_stmt);
  return 1;
}

int insert_secret(sqlite3 *db, hashed_pass_t *hash_obj) {
  sqlite3_stmt *sql_stmt = NULL;
  char *sql_fmt_str = NULL;
  if (hash_obj == NULL) {
    fprintf(stderr, "Error: empty password_obj\n");
    return 0;
  }

  sql_fmt_str = "INSERT INTO secrets (hash, salt) VALUES (?, ?);";
  if (sqlite3_prepare_v2(db, sql_fmt_str, -1, &sql_stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Error: failed to prepare statement: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return 0;
  }

  if (sqlite3_bind_text(sql_stmt, 1, hash_obj->hash, -1, SQLITE_STATIC) != SQLITE_OK ||
      sqlite3_bind_text(sql_stmt, 2, (char *)hash_obj->salt, -1, SQLITE_STATIC) != SQLITE_OK) {
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
