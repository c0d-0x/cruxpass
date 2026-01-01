#ifndef ENC_H
#define ENC_H

#include <stdbool.h>
#include "database.h"
#ifndef SQLITE_HAS_CODEC
#define SQLITE_HAS_CODEC
#endif

#include <sodium.h>
#include <sodium/core.h>
#include <sodium/crypto_pwhash.h>
#include <sodium/utils.h>
#include <sqlcipher/sqlite3.h>
#include <stdint.h>

#define GEN_KEY 0x01
#define GEN_HASH 0x02
#define KEY_LEN 32
#define HASH_LEN crypto_pwhash_STRBYTES
#define SALT_LEN crypto_pwhash_SALTBYTES
#define BUFFMAX SECRET_MAX_LEN + USERNAME_MAX_LEN + DESC_MAX_LEN

unsigned char *authenticate(vault_ctx_t *ctx);
bool create_new_master_secret(sqlite3 *db);
bool decrypt(sqlite3 *db, unsigned char *key);
bool key_gen(unsigned char *key, const char *const passd_str, unsigned char *salt);

#endif  // !ENC_H
