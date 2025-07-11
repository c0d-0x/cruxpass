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
#define UPDATE_PASSWORD 0x02
#define UPDATE_USERNAME 0x04

#define P_DB 0x01
#define S_DB 0x02

/**
 * @brief Initializes a SQLite database connection.
 *
 * This function initializes a connection to a SQLite database with the given
 * database name and returns a pointer to the SQLite database object.
 *
 * @param db_name The name of the database to connect to.
 * @param DB A pointer to a sqlite3 object where the database connection will be
 * stored.
 * @return A pointer to the initialized sqlite3 database object.
 */
int init_sqlite(void);
void cleanup_paths(void);
/* sqlite3 *open_db(char *db_name, int flags); */
sqlite3 *open_db_wrap(int8_t db_name_flag, int flags);
hashed_pass_t *fetch_hash(void);
int list_all_passwords_v2(sqlite3 *db);
int load_data_from_db(sqlite3 *db, record_array_t *records);
int update_secret(hashed_pass_t *hash_obj);
int lookup_password(char *db_name, char *searchstr);
int delete_password_v2(sqlite3 *db, int password_id);
int insert_secret(sqlite3 *db, hashed_pass_t *hash_obj);
int insert_password(sqlite3 *db, password_t *password_obj);
int update_password(char *db_name, char *description, char *passwd, char *username, int id, uint8_t flags);

#endif  // !SQLITE_H
