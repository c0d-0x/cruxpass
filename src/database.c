#include "database.h"

#include <locale.h>
#include <sodium/core.h>
#include <sodium/utils.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cruxpass.h"
#include "crypt.h"
#include "tui.h"

extern char *cruxpass_db_path;
extern char *meta_db_path;
sqlite3_stmt *sql_stmts[STMT_COUNT];

// clang-format off
char *sql_str[STMT_COUNT] = {
                 "INSERT INTO secrets (username, secret,description )  VALUES (?, ?, ?);",
                 "DELETE FROM secrets WHERE id = ?;", 
                 "SELECT secret FROM secrets WHERE id = ?;"
};
// clang-format on

static int sql_exec_n_err(sqlite3 *db, char *sql_fmt_str, char *sql_err_msg,
                          int (*callback)(void *, int, char **, char **), void *callback_arg) {
    if (sqlite3_exec(db, sql_fmt_str, callback, callback_arg, &sql_err_msg) != SQLITE_OK) {
        fprintf(stderr, "Error: Failed execute sql statement: %s\n", sql_err_msg);
        sqlite3_close(db);
        sqlite3_free(sql_err_msg);
        return 0;
    }

    return 1;
}

static int sql_prep_n_exec(sqlite3 *db, char *sql_fmt_str, sqlite3_stmt *sql_stmt, const char *field, int id) {
    if (sqlite3_prepare_v2(db, sql_fmt_str, -1, &sql_stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Error: Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return 0;
    }

    if (sqlite3_bind_text(sql_stmt, 1, field, -1, SQLITE_STATIC) != SQLITE_OK
        || sqlite3_bind_int(sql_stmt, 2, id) != SQLITE_OK) {
        fprintf(stderr, "Error: Failed to bind sql statement: %s", sqlite3_errmsg(db));
        sqlite3_finalize(sql_stmt);
        return 0;
    }

    if (sqlite3_step(sql_stmt) != SQLITE_DONE) {
        fprintf(stderr, "Error: Failed to execute statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(sql_stmt);
        return 0;
    }

    sqlite3_finalize(sql_stmt);
    return 1;
}

static int create_databases(vault_ctx_t *ctx) {
    char *sql_err_msg = NULL;
    char *sql_fmt_str = NULL;
    unsigned char *key = NULL;
    meta_t *meta = NULL;

    init_tui();
    tb_clear();
    tb_print(0, 2, TB_DEFAULT, TB_DEFAULT, "Create a new master password for cruxpass.");
    tb_present();
    char *master_psswd = get_input("> Enter password: ", NULL, MASTER_MAX_LEN, 0, 3);
    cleanup_tui();

    if (master_psswd == NULL || strlen(master_psswd) < SECRET_MIN_LEN) {
        fprintf(stderr, "Error: password invalid\n");
        return CRXP_ERR;
    }

    if ((meta = malloc(sizeof(meta_t))) == NULL) CRXP__OUT_OF_MEMORY();
    meta->version = 0x02;
    randombytes_buf(meta->salt, SALT_LEN);

    if ((key = (unsigned char *) sodium_malloc(KEY_LEN)) == NULL) CRXP__OUT_OF_MEMORY();
    if (!key_gen(key, master_psswd, meta->salt)) {
        sodium_memzero(key, KEY_LEN);
        sodium_free(key);
        free(master_psswd);
        free(meta);
        return CRXP_ERR;
    }

    free(master_psswd);
    if (!decrypt(ctx->secret_db, key)) {
        sodium_memzero(key, KEY_LEN);
        sodium_free(key);
        free(meta);
        return CRXP_ERR;
    }

    sodium_memzero(key, KEY_LEN);
    sodium_free(key);
    sql_fmt_str
        = "CREATE TABLE IF NOT EXISTS meta ( id INTEGER PRIMARY "
          "KEY, salt TEXT NOT NULL, version INTEGER NOT NULL);";
    if (!sql_exec_n_err(ctx->meta_db, sql_fmt_str, sql_err_msg, NULL, NULL)) {
        free(meta);
        return CRXP_ERR;
    }

    if (!insert_meta(ctx->meta_db, meta)) {
        free(meta);
        return CRXP_ERR;
    }

    free(meta);
    sql_fmt_str
        = "CREATE TABLE IF NOT EXISTS secrets ( id INTEGER PRIMARY KEY, username TEXT NOT NULL, "
          "secret TEXT NOT NULL, description TEXT NOT NULL, date_added "
          "TEXT DEFAULT CURRENT_DATE);";
    if (!sql_exec_n_err(ctx->secret_db, sql_fmt_str, sql_err_msg, NULL, NULL)) {
        return CRXP_ERR;
    }

    return CRXP_OK;
}

bool prepare_stmt(vault_ctx_t *ctx) {
    for (int i = 0; i < STMT_COUNT; i++) {
        if (sqlite3_prepare_v2(ctx->secret_db, sql_str[i], -1, &sql_stmts[i], NULL) != SQLITE_OK) {
            fprintf(stderr, "Error: failed to prepare statement: %s\n", sqlite3_errmsg(ctx->secret_db));
            return false;
        }
    }

    return true;
}

void cleanup_stmts(void) {
    for (int i = 0; i < STMT_COUNT; i++) {
        sqlite3_finalize(sql_stmts[i]);
    }
}

sqlite3 *open_db(char *db_name, int flags) {
    sqlite3 *db = NULL;
    int rc = sqlite3_open_v2(db_name, &db, flags, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Error: failed to open %s: %s\n", db_name, sqlite3_errmsg(db));
        return NULL;
    }

    return db;
}

int init_sqlite(void) {
    vault_ctx_t ctx = {0};
    meta_t *meta = NULL;

    if ((ctx.secret_db = open_db(cruxpass_db_path, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX))
        == NULL) {
        return CRXP_ERR;
    }

    if ((ctx.meta_db = open_db(meta_db_path, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX))
        == NULL) {
        return CRXP_ERR;
    }

    if (sqlite3_exec(ctx.secret_db, "PRAGMA cipher_log_level = NONE;", NULL, NULL, NULL) != SQLITE_OK) {
        fprintf(stderr, "Error: Failed to disable logging to STDERR\n");
    }

    if (sqlite3_exec(ctx.secret_db, "PRAGMA cipher_memory_security = ON ;", NULL, NULL, NULL) != SQLITE_OK) {
        fprintf(stderr, "Error: Failed to enable memory security\n");
    }

    if ((meta = fetch_meta()) != NULL) {
        free(meta);
        sqlite3_close(ctx.meta_db);
        sqlite3_close(ctx.secret_db);
        return CRXP_OK;
    }

    free(meta);
    if (!create_databases(&ctx)) {
        sqlite3_close(ctx.meta_db);
        sqlite3_close(ctx.secret_db);
        return CRXP_ERR;
    }

    sqlite3_close(ctx.meta_db);
    sqlite3_close(ctx.secret_db);
    return CRXP_OKK;
}

int insert_record(sqlite3 *db, secret_t *record) {
    if (record == NULL) {
        fprintf(stderr, "Error: Empty record\n");
        return 0;
    }

    if (sqlite3_bind_text(sql_stmts[INSERT_REC_STMT], 1, record->username, -1, SQLITE_STATIC) != SQLITE_OK
        || sqlite3_bind_text(sql_stmts[INSERT_REC_STMT], 2, record->secret, -1, SQLITE_STATIC) != SQLITE_OK
        || sqlite3_bind_text(sql_stmts[INSERT_REC_STMT], 3, record->description, -1, SQLITE_STATIC) != SQLITE_OK) {
        fprintf(stderr, "Error: Failed to bind sql statement: %s\n", sqlite3_errmsg(db));
        sqlite3_reset(sql_stmts[INSERT_REC_STMT]);
        sqlite3_clear_bindings(sql_stmts[INSERT_REC_STMT]);
        return 0;
    }

    if (sqlite3_step(sql_stmts[INSERT_REC_STMT]) != SQLITE_DONE) {
        fprintf(stderr, "Error: Failed to execute statement: %s\n", sqlite3_errmsg(db));
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
        fprintf(stderr, "Error: Failed to bind sql statement: %s", sqlite3_errmsg(db));
        sqlite3_reset(sql_stmts[DELETE_REC_STMT]);
        sqlite3_clear_bindings(sql_stmts[DELETE_REC_STMT]);
        return 0;
    }

    if (sqlite3_step(sql_stmts[DELETE_REC_STMT]) != SQLITE_DONE) {
        fprintf(stderr, "Error: Failed to execute statement\n");
        sqlite3_reset(sql_stmts[DELETE_REC_STMT]);
        sqlite3_clear_bindings(sql_stmts[DELETE_REC_STMT]);
        return 0;
    }

    sqlite3_reset(sql_stmts[DELETE_REC_STMT]);
    sqlite3_clear_bindings(sql_stmts[DELETE_REC_STMT]);
    return 1;
}

int update_record(sqlite3 *db, secret_t *secret_record, int record_id, uint8_t flags) {
    char *sql_fmt_str = NULL;
    sqlite3_stmt *sql_stmt = NULL;

    if (flags & UPDATE_DESCRIPTION) {
        if (secret_record->description[0] == '\0') {
            fprintf(stderr, "Error: Empty description\n");
            return 0;
        }

        sql_fmt_str = "UPDATE secrets SET description = ? WHERE id = ?;";
        if (!sql_prep_n_exec(db, sql_fmt_str, sql_stmt, secret_record->description, record_id)) {
            return 0;
        }

        sqlite3_reset(sql_stmt);
        sqlite3_finalize(sql_stmt);
    }

    if (flags & UPDATE_SECRET) {
        if (secret_record->secret[0] == '\0') {
            fprintf(stderr, "Error: Empty secret\n");
            return 0;
        }

        sql_fmt_str = "UPDATE secrets SET secret = ? WHERE id = ?;";
        if (!sql_prep_n_exec(db, sql_fmt_str, sql_stmt, secret_record->secret, record_id)) {
            return 0;
        }

        sqlite3_reset(sql_stmt);
        sqlite3_finalize(sql_stmt);
    }

    if (flags & UPDATE_USERNAME) {
        if (secret_record->username[0] == '\0') {
            fprintf(stderr, "Error: Empty username\n");
            return 0;
        }

        sql_fmt_str = "UPDATE secrets SET username = ? WHERE id = ?;";
        if (!sql_prep_n_exec(db, sql_fmt_str, sql_stmt, secret_record->username, record_id)) {
            return 0;
        }

        sqlite3_reset(sql_stmt);
        sqlite3_finalize(sql_stmt);
    }

    return 1;
}

int load_records(sqlite3 *db, record_array_t *records) {
    const char *sql
        = "SELECT id, username, description FROM secrets "
          "ORDER BY id;";
    if (sqlite3_exec(db, sql, pipeline, records, NULL) != SQLITE_OK) {
        fprintf(stderr, "Error: SQL error: %s\n", sqlite3_errmsg(db));
        return 0;
    }

    return 1;
}

meta_t *fetch_meta(void) {
    sqlite3 *meta_db = NULL;
    sqlite3_stmt *sql_stmt = NULL;
    meta_t *meta = NULL;
    char *sql_str = "SELECT salt, version FROM meta WHERE id = ?;";
    int id = 1;
    if ((meta_db = open_db(meta_db_path, SQLITE_OPEN_READWRITE)) == NULL) {
        return NULL;
    }

    if ((meta = malloc(sizeof(meta_t) + SALT_LEN)) == NULL) CRXP__OUT_OF_MEMORY();
    if (sqlite3_prepare_v2(meta_db, sql_str, -1, &sql_stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Warning: Failed to prepare statement: %s\n", sqlite3_errmsg(meta_db));
        sqlite3_close(meta_db);
        free(meta);
        return NULL;
    }

    if (sqlite3_bind_int64(sql_stmt, 1, id) != SQLITE_OK) {
        fprintf(stderr, "Error: Failed to bind sql statement: %s", sqlite3_errmsg(meta_db));
        sqlite3_finalize(sql_stmt);
        sqlite3_close(meta_db);
        free(meta);
        return NULL;
    }

    if (sqlite3_step(sql_stmt) != SQLITE_ROW) {
        fprintf(stderr, "Error: Failed to execute statement: %s\n", sqlite3_errmsg(meta_db));
        sqlite3_finalize(sql_stmt);
        sqlite3_close(meta_db);
        free(meta);
        return NULL;
    }

    uint8_t *salt = (uint8_t *) sqlite3_column_text(sql_stmt, 0);
    int salt_len = sqlite3_column_bytes(sql_stmt, 0);
    if (salt == NULL || salt_len != SALT_LEN) {
        fprintf(stderr, "Error: Invalid salt data\n");
        sqlite3_finalize(sql_stmt);
        sqlite3_close(meta_db);
        free(meta);
        return NULL;
    }

    memcpy(meta->salt, salt, SALT_LEN);
    meta->version = (uint8_t) sqlite3_column_int(sql_stmt, 1);

    sqlite3_finalize(sql_stmt);
    sqlite3_close(meta_db);
    return meta;
}

bool update_meta(sqlite3 *db, meta_t *meta) {
    int id = 1;
    bool db_self = false;
    char *sql_fmt_str = NULL;
    sqlite3_stmt *sql_stmt = NULL;
    if (db == NULL) {
        db_self = true;
        if ((db = open_db(meta_db_path, SQLITE_OPEN_READWRITE)) == NULL) {
            return false;
        }
    }

    sql_fmt_str = "UPDATE meta SET salt = ? WHERE id = ?;";
    if (!sql_prep_n_exec(db, sql_fmt_str, sql_stmt, (char *) meta->salt, id)) {
        sqlite3_finalize(sql_stmt);
        if (db_self) sqlite3_close(db);
        return false;
    }

    if (db_self) sqlite3_close(db);
    sqlite3_finalize(sql_stmt);
    return true;
}

bool insert_meta(sqlite3 *db, meta_t *meta) {
    bool db_self = false;
    char *sql_fmt_str = NULL;
    sqlite3_stmt *sql_stmt = NULL;
    sql_fmt_str = "INSERT INTO meta (salt, version) VALUES (?, ?);";

    if (db == NULL) {
        db_self = true;
        if ((db = open_db(meta_db_path, SQLITE_OPEN_READWRITE)) == NULL) {
            return NULL;
        }
    }

    if (sqlite3_prepare_v2(db, sql_fmt_str, -1, &sql_stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Error: Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(sql_stmt);
        if (db_self) sqlite3_close(db);
        return false;
    }

    if (sqlite3_bind_text(sql_stmt, 1, (char *) meta->salt, -1, SQLITE_STATIC) != SQLITE_OK
        || sqlite3_bind_int(sql_stmt, 2, meta->version) != SQLITE_OK) {
        fprintf(stderr, "Error: Failed to bind sql statement: %s", sqlite3_errmsg(db));
        sqlite3_finalize(sql_stmt);
        if (db_self) sqlite3_close(db);
        return false;
    }

    if (sqlite3_step(sql_stmt) != SQLITE_DONE) {
        fprintf(stderr, "Error: Failed to step through statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(sql_stmt);
        if (db_self) sqlite3_close(db);
        return false;
    }

    sqlite3_finalize(sql_stmt);
    if (db_self) sqlite3_close(db);
    return true;
}

const unsigned char *fetch_secret(sqlite3 *db, const int64_t id) {
    const unsigned char *secret = NULL;
    if (sqlite3_bind_int64(sql_stmts[FETCH_SEC_STMT], 1, id) != SQLITE_OK) {
        fprintf(stderr, "Error: Failed to bind sql statement: %s", sqlite3_errmsg(db));

        sqlite3_reset(sql_stmts[FETCH_SEC_STMT]);
        sqlite3_clear_bindings(sql_stmts[FETCH_SEC_STMT]);
        return NULL;
    }

    if (sqlite3_step(sql_stmts[FETCH_SEC_STMT]) != SQLITE_ROW) {
        fprintf(stderr, "Error: Failed to execute statement: %s\n", sqlite3_errmsg(db));

        sqlite3_reset(sql_stmts[FETCH_SEC_STMT]);
        sqlite3_clear_bindings(sql_stmts[FETCH_SEC_STMT]);
        return NULL;
    }

    char *tmp = (char *) sqlite3_column_text(sql_stmts[FETCH_SEC_STMT], 0);
    int secret_len = sqlite3_column_bytes(sql_stmts[FETCH_SEC_STMT], 0);
    if (tmp != NULL || secret_len <= SECRET_MAX_LEN) secret = (unsigned char *) strdup(tmp);

    sqlite3_reset(sql_stmts[FETCH_SEC_STMT]);
    sqlite3_clear_bindings(sql_stmts[FETCH_SEC_STMT]);
    return secret;
}
