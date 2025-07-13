#ifndef SQLITE_H
#define SQLITE_H
#define SQLITE_HAS_CODEC
#include <sqlcipher/sqlite3.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "cruxpass.h"
#include "enc.h"
/* #include "sqlcipher.h" */
#include "tui.h"

#define UPDATE_DESCRIPTION 0x01
#define UPDATE_SECRET 0x02
#define UPDATE_USERNAME 0x04

#define P_DB 0x01
#define S_DB 0x02

/**
 * @brief Initializes a SQLite database connection.
 *
 * This function initializes a connection to a SQLite database with the given
 * database name and returns a pointer to the SQLite database object.
 */
int init_sqlite(void);
void cleanup_paths(void);
sqlite3 *open_db_wrap(int8_t db_name_flag, int8_t flags);

hash_t *fetch_hash(void);
int update_hash(hash_t *hash_obj);
int insert_hash(sqlite3 *db, hash_t *hash_obj);

int load_records(sqlite3 *db, record_array_t *records);
int lookup_record(char *db_name, char *searchstr);
int delete_record(sqlite3 *db, int password_id);
int insert_record(sqlite3 *db, secret_t *password_obj);
int update_record(char *db_name, secret_t *secret_rec, int id, uint8_t flags);

#endif  // !SQLITE_H
