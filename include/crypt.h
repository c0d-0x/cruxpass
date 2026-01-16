#ifndef CRTYPT_H
#define CRTYPT_H

#ifndef SQLITE_HAS_CODEC
#define SQLITE_HAS_CODEC
#endif

#include <sodium.h>
#include <sodium/core.h>
#include <sodium/crypto_pwhash.h>
#include <sodium/utils.h>
#include <sqlcipher/sqlite3.h>
#include <stdbool.h>
#include <stdint.h>
#include "cruxpass.h"

#define GEN_KEY 0x01
#define KEY_LEN 32
#define SALT_LEN 16
#define BUFFMAX SECRET_MAX_LEN + USERNAME_MAX_LEN + DESC_MAX_LEN + 1

bool create_new_master_secret(sqlite3 *db);
unsigned char *authenticate(vault_ctx_t *ctx);
bool decrypt(sqlite3 *db, unsigned char *key);
bool key_gen(unsigned char *key, const char *const passd_str, unsigned char *salt);

#endif  // !CRTYPT_H
