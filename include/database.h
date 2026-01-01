#ifndef SQLITE_H
#define SQLITE_H
#ifndef SQLITE_HAS_CODEC
#define SQLITE_HAS_CODEC
#endif

#include <sqlcipher/sqlite3.h>
#include <stdint.h>

#include "cruxpass.h"
#include "tui.h"

#define UPDATE_DESCRIPTION 0x01
#define UPDATE_SECRET 0x02
#define UPDATE_USERNAME 0x04

typedef enum {
    INSERT_REC_STMT,
    DELETE_REC_STMT,
    FETCH_SEC_STMT,
    STMT_COUNT
} SQL_STMT;

typedef struct {
    uint8_t version;
    uint8_t salt[];
} meta_t;

bool prepare_stmt(vault_ctx_t *ctx);
void cleanup_stmts(void);

int init_sqlite(void);
sqlite3 *open_db(char *db_name, int flags);

meta_t *fetch_meta(void);
bool update_meta(sqlite3 *db, meta_t *meta);
bool insert_meta(sqlite3 *db, meta_t *meta);

int delete_record(sqlite3 *db, int id);
int insert_record(sqlite3 *db, secret_t *secret);
int load_records(sqlite3 *db, record_array_t *records);
int update_record(sqlite3 *db, secret_t *secret, int id, uint8_t flags);

const unsigned char *fetch_secret(sqlite3 *db, const int64_t id);
#endif  // !SQLITE_H
