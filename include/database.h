#ifndef SQLITE_H
#define SQLITE_H
#ifndef SQLITE_HAS_CODEC
#define SQLITE_HAS_CODEC
#endif

#include <sqlcipher/sqlite3.h>
#include <stdint.h>

#include "cruxpass.h"
#include "enc.h"
#include "tui.h"

#define UPDATE_DESCRIPTION 0x01
#define UPDATE_SECRET 0x02
#define UPDATE_USERNAME 0x04

#define NUM_STMTS 3

typedef enum {
    INSERT_REC_STMT,
    DELETE_REC_STMT,
    FETCH_SEC_STMT,
} SQL_STMT;

bool prepare_stmt(sqlite3 *db);
void cleanup_stmts(void);

int init_sqlite(void);
sqlite3 *open_db(char *db_name, int flags);

hash_t *fetch_hash(void);
int update_hash(hash_t *hash_obj);
int insert_hash(sqlite3 *db, hash_t *hash_obj);

int delete_record(sqlite3 *db, int password_id);
int insert_record(sqlite3 *db, secret_t *password_obj);
int load_records(sqlite3 *db, record_array_t *records);
int lookup_record(char *db_name, char *searchstr);
int update_record(sqlite3 *db, secret_t *secret_rec, int id, uint8_t flags);

const unsigned char *fetch_secret(sqlite3 *db, const int64_t id);
#endif  // !SQLITE_H
