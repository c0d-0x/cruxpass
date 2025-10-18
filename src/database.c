#include "database.h"

#include <sodium/core.h>
#include <sodium/utils.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cruxpass.h"
#include "enc.h"
#include "tui.h"

char *cruxpass_db_path;
char *auth_db_path;
char *sql_str[] = {"INSERT INTO secrets (username, secret,description )  VALUES (?, ?, ?);",
                   "DELETE FROM secrets WHERE secret_id = ?;", "SELECT secret FROM secrets WHERE secret_id = ?;"};

sqlite3_stmt *sql_stmts[NUM_STMTS] = {NULL};

bool prepare_stmt(sqlite3 *db) {
    for (int i = 0; i < NUM_STMTS; i++) {
        if (sqlite3_prepare_v2(db, sql_str[i], -1, &sql_stmts[i], NULL) != SQLITE_OK) {
            fprintf(stderr, "Error: failed to prepare statement: %s\n", sqlite3_errmsg(db));
            sqlite3_close(db);
            return false;
        }
    }

    return true;
}

void cleanup_stmts(void) {
    for (int i = 0; i < NUM_STMTS; i++) {
        sqlite3_finalize(sql_stmts[i]);
    }
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

sqlite3 *open_db(char *db_name, int flags) {
    sqlite3 *db = NULL;
    int rc = sqlite3_open_v2(db_name, &db, flags, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Error: failed to open cruxpass_d: %s\n", sqlite3_errmsg(db));
        return NULL;
    }

    return db;
}

static int create_databases(sqlite3 *db, sqlite3 *hashes_db) {
    char *sql_err_msg = NULL;
    char *sql_fmt_str = NULL;
    unsigned char *key = NULL;
    hash_t hash_rec = {0};

    if (!init_tui()) {
        return C_ERR;
    }

    tb_clear();
    tb_print(0, 2, TB_DEFAULT, TB_DEFAULT, "Create a new master password for cruxpass.");
    tb_present();
    char *master_psswd = get_input("> Enter password: ", NULL, MASTER_MAX_LEN, 3, 0);
    cleanup_tui();

    if (master_psswd == NULL || strlen(master_psswd) < SECRET_MIN_LEN) {
        fprintf(stderr, "Error: password invalid\n");
        return C_ERR;
    }

    if ((key = (unsigned char *) sodium_malloc(KEY_LEN)) == NULL) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        free(master_psswd);
        return C_ERR;
    }

    randombytes_buf(hash_rec.salt, crypto_pwhash_SALTBYTES);
    if (!generate_key_or_hash(key, hash_rec.hash, master_psswd, hash_rec.salt, GEN_HASH | GEN_KEY)) {
        sodium_free(key);
        free(master_psswd);
        return C_ERR;
    }

    free(master_psswd);
    if ((db = decrypt(db, key)) == NULL) {
        sodium_free(key);
        return C_ERR;
    }

    sodium_free(key);
    sql_fmt_str
        = "CREATE TABLE IF NOT EXISTS hashes ( hash_id INTEGER PRIMARY "
          "KEY, hash TEXT NOT NULL, salt TEXT NOT NULL );";
    if (!sql_exec_n_err(hashes_db, sql_fmt_str, sql_err_msg, NULL, NULL)) {
        return C_ERR;
    }

    if (!insert_hash(hashes_db, &hash_rec)) {
        return C_ERR;
    }

    sql_fmt_str
        = "CREATE TABLE IF NOT EXISTS secrets ( secret_id INTEGER "
          "PRIMARY KEY, username TEXT NOT NULL, "
          "secret TEXT NOT NULL, description TEXT NOT NULL, date_added "
          "TEXT DEFAULT CURRENT_DATE);";
    if (!sql_exec_n_err(db, sql_fmt_str, sql_err_msg, NULL, NULL)) {
        return C_ERR;
    }

    sqlite3_close(db);
    sqlite3_close(hashes_db);

    return C_RET_OK;
}

int init_sqlite(void) {
    sqlite3 *db = NULL;
    sqlite3 *hashes_db = NULL;

    hash_t *stored_hash_rec = NULL;

    if ((db = open_db(cruxpass_db_path, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX)) == NULL) {
        return C_ERR;
    }

    if ((hashes_db = open_db(auth_db_path, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE)) == NULL) {
        sqlite3_close(db);
        return C_ERR;
    }

    if ((stored_hash_rec = fetch_hash()) != NULL) {
        free(stored_hash_rec);
        sqlite3_close(db);
        sqlite3_close(hashes_db);
        return C_RET_OK;
    }

    if (!create_databases(db, hashes_db)) {
        sqlite3_close(db);
        sqlite3_close(hashes_db);
        return C_RET_OK;
    }

    return C_RET_OKK;
}

int insert_record(sqlite3 *db, secret_t *record) {
    if (record == NULL) {
        fprintf(stderr, "Error: empty record\n");
        return 0;
    }

    if (sqlite3_bind_text(sql_stmts[INSERT_REC_STMT], 1, record->username, -1, SQLITE_STATIC) != SQLITE_OK
        || sqlite3_bind_text(sql_stmts[INSERT_REC_STMT], 2, record->secret, -1, SQLITE_STATIC) != SQLITE_OK
        || sqlite3_bind_text(sql_stmts[INSERT_REC_STMT], 3, record->description, -1, SQLITE_STATIC) != SQLITE_OK) {
        fprintf(stderr, "Error: failed to bind sql statement: %s\n", sqlite3_errmsg(db));
        sqlite3_reset(sql_stmts[INSERT_REC_STMT]);
        sqlite3_clear_bindings(sql_stmts[INSERT_REC_STMT]);
        return 0;
    }

    if (sqlite3_step(sql_stmts[INSERT_REC_STMT]) != SQLITE_DONE) {
        fprintf(stderr, "Error: failed to execute statement: %s\n", sqlite3_errmsg(db));
        sqlite3_reset(sql_stmts[INSERT_REC_STMT]);
        sqlite3_clear_bindings(sql_stmts[INSERT_REC_STMT]);
        return 0;
    }

    sqlite3_reset(sql_stmts[INSERT_REC_STMT]);
    sqlite3_clear_bindings(sql_stmts[INSERT_REC_STMT]);
    return 1;
}

int delete_record(sqlite3 *db, int record_id) {
    if (sqlite3_bind_int(sql_stmts[DELETE_REC_STMT], 1, record_id) != SQLITE_OK) {
        fprintf(stderr, "Error: failed to bind sql statement: %s", sqlite3_errmsg(db));
        sqlite3_reset(sql_stmts[DELETE_REC_STMT]);
        sqlite3_clear_bindings(sql_stmts[DELETE_REC_STMT]);
        return 0;
    }

    if (sqlite3_step(sql_stmts[DELETE_REC_STMT]) != SQLITE_DONE) {
        fprintf(stderr, "Error: failed to execute statement\n");
        sqlite3_reset(sql_stmts[DELETE_REC_STMT]);
        sqlite3_clear_bindings(sql_stmts[DELETE_REC_STMT]);
        return 0;
    }

    sqlite3_reset(sql_stmts[DELETE_REC_STMT]);
    sqlite3_clear_bindings(sql_stmts[DELETE_REC_STMT]);
    return 1;
}

static int sql_prep_n_exec(sqlite3 *db, char *sql_fmt_str, sqlite3_stmt *sql_stmt, const char *field, int secret_id) {
    if (sqlite3_prepare_v2(db, sql_fmt_str, -1, &sql_stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Error: failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return 0;
    }

    if (sqlite3_bind_text(sql_stmt, 1, field, -1, SQLITE_STATIC) != SQLITE_OK
        || sqlite3_bind_int(sql_stmt, 2, secret_id) != SQLITE_OK) {
        fprintf(stderr, "Error: failed to bind sql statement: %s", sqlite3_errmsg(db));
        sqlite3_finalize(sql_stmt);
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

int update_record(sqlite3 *db, secret_t *secret_record, int record_id, uint8_t flags) {
    char *sql_fmt_str = NULL;
    sqlite3_stmt *sql_stmt = NULL;

    if (flags & UPDATE_DESCRIPTION) {
        if (secret_record->description[0] == '\0') {
            fprintf(stderr, "Error: empty description\n");
            sqlite3_close(db);
            return 0;
        }

        sql_fmt_str = "UPDATE secrets SET description = ? WHERE secret_id = ?;";
        if (!sql_prep_n_exec(db, sql_fmt_str, sql_stmt, secret_record->description, record_id)) {
            sqlite3_close(db);
            return 0;
        }

        sqlite3_reset(sql_stmt);
        sqlite3_finalize(sql_stmt);
    }

    if (flags & UPDATE_SECRET) {
        if (secret_record->secret[0] == '\0') {
            fprintf(stderr, "Error: empty secret\n");
            sqlite3_close(db);
            return 0;
        }
        sql_fmt_str = "UPDATE secrets SET secret = ? WHERE secret_id = ?;";
        if (!sql_prep_n_exec(db, sql_fmt_str, sql_stmt, secret_record->secret, record_id)) {
            sqlite3_close(db);
            return 0;
        }

        sqlite3_reset(sql_stmt);
        sqlite3_finalize(sql_stmt);
    }

    if (flags & UPDATE_USERNAME) {
        if (secret_record->username[0] == '\0') {
            fprintf(stderr, "Error: empty username\n");
            sqlite3_close(db);
            return 0;
        }
        sql_fmt_str = "UPDATE secrets SET username = ? WHERE secret_id = ?;";
        if (!sql_prep_n_exec(db, sql_fmt_str, sql_stmt, secret_record->username, record_id)) {
            sqlite3_close(db);
            return 0;
        }

        sqlite3_reset(sql_stmt);
        sqlite3_finalize(sql_stmt);
    }

    sqlite3_close(db);
    return 1;
}

int load_records(sqlite3 *db, record_array_t *records) {
    char *err_msg = 0;

    const char *sql
        = "SELECT secret_id, username, description FROM secrets "
          "ORDER BY secret_id;";
    if (sqlite3_exec(db, sql, pipeline, records, &err_msg) != SQLITE_OK) {
        fprintf(stderr, "Error: SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return 0;
    }

    return 1;
}

const unsigned char *fetch_secret(sqlite3 *db, const int64_t id) {
    if (db == NULL) return NULL;

    const unsigned char *secret = NULL;
    if (sqlite3_bind_int64(sql_stmts[FETCH_SEC_STMT], 1, id) != SQLITE_OK) {
        fprintf(stderr, "Error: failed to bind sql statement: %s", sqlite3_errmsg(db));
        sqlite3_reset(sql_stmts[FETCH_SEC_STMT]);
        sqlite3_clear_bindings(sql_stmts[FETCH_SEC_STMT]);
        return NULL;
    }

    if (sqlite3_step(sql_stmts[FETCH_SEC_STMT]) != SQLITE_ROW) {
        fprintf(stderr, "Error: failed to execute statement: %s\n", sqlite3_errmsg(db));
        sqlite3_reset(sql_stmts[FETCH_SEC_STMT]);
        sqlite3_clear_bindings(sql_stmts[FETCH_SEC_STMT]);
        return NULL;
    }

    char *tmp = (char *) sqlite3_column_text(sql_stmts[FETCH_SEC_STMT], 0);
    if (tmp != NULL) secret = (unsigned char *) strdup(tmp);
    sqlite3_reset(sql_stmts[FETCH_SEC_STMT]);
    sqlite3_clear_bindings(sql_stmts[FETCH_SEC_STMT]);
    return secret;
}

hash_t *fetch_hash(void) {
    sqlite3 *auth_db = NULL;
    sqlite3_stmt *sql_stmt = NULL;
    hash_t *hash_rec = NULL;
    const unsigned char *hash = NULL;
    const unsigned char *salt = NULL;
    char *sql_str = "SELECT hash, salt FROM hashes;";

    if ((auth_db = open_db(auth_db_path, SQLITE_OPEN_READWRITE)) == NULL) {
        return NULL;
    }

    int sql_ret = sqlite3_prepare_v2(auth_db, sql_str, -1, &sql_stmt, NULL);
    if (sql_ret != SQLITE_OK) {
        fprintf(stderr, "Warning/Error: failed to prepare statement: %s\n", sqlite3_errmsg(auth_db));
        sqlite3_close(auth_db);
        return NULL;
    }

    bool done = false;
    while (sqlite3_step(sql_stmt) == SQLITE_ROW && !done) {
        hash = sqlite3_column_text(sql_stmt, 0);
        salt = sqlite3_column_text(sql_stmt, 1);
        if (hash != NULL || salt != NULL) {
            if ((hash_rec = calloc(1, sizeof(hash_t))) == NULL) {
                fprintf(stderr, "Error: Memory allocation failed: calloc\n");
                sqlite3_close(auth_db);
                return NULL;
            }

            memcpy(hash_rec->hash, hash, PASS_HASH_LEN);
            memcpy(hash_rec->salt, salt, SALT_HASH_LEN);
            done = true;
        }
    }

    if (hash_rec == NULL) {
        fprintf(stderr, "Warning: empty secrets db\n");
        sqlite3_close(auth_db);
        return NULL;
    }

    sqlite3_finalize(sql_stmt);
    sqlite3_close(auth_db);
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

    if ((db = open_db(auth_db_path, SQLITE_OPEN_READWRITE)) == NULL) {
        return 0;
    }

    sql_fmt_str = "UPDATE hashes SET hash = ? WHERE hash_id = ?;";
    if (!sql_prep_n_exec(db, sql_fmt_str, sql_stmt, hash_rec->hash, 1)) {
        sqlite3_close(db);
        return 0;
    }

    sqlite3_reset(sql_stmt);
    sqlite3_finalize(sql_stmt);

    sql_fmt_str = "UPDATE hashes SET salt= ? WHERE hash_id = ?;";
    if (!sql_prep_n_exec(db, sql_fmt_str, sql_stmt, (char *) hash_rec->salt, 1)) {
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

    sql_fmt_str = "INSERT INTO hashes (hash, salt) VALUES (?, ?);";
    if (sqlite3_prepare_v2(db, sql_fmt_str, -1, &sql_stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Error: failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 0;
    }

    if (sqlite3_bind_text(sql_stmt, 1, hash_rec->hash, -1, SQLITE_STATIC) != SQLITE_OK
        || sqlite3_bind_text(sql_stmt, 2, (char *) hash_rec->salt, -1, SQLITE_STATIC) != SQLITE_OK) {
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
