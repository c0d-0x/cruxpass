#ifndef ENC_H
#define ENC_H
#define SQLITE_HAS_CODEC
#include <sodium.h>
#include <sodium/core.h>
#include <sodium/crypto_pwhash.h>
#include <sodium/utils.h>
#include <sqlcipher/sqlite3.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define GEN_KEY 0x01
#define GEN_HASH 0x02
#define KEY_LEN crypto_box_SEEDBYTES
#define PASS_HASH_LEN crypto_pwhash_STRBYTES
#define SALT_HASH_LEN crypto_pwhash_SALTBYTES
#define BUFFMAX SECRET_MAX_LEN + USERNAME_MAX_LEN + DESC_MAX_LEN

typedef struct {
  char hash[PASS_HASH_LEN + 1];
  unsigned char salt[SALT_HASH_LEN];
} hash_t;

hash_t *authenticate(char *master_passdm);
int generate_key_or_hash(u_char *key, char *hash, const char *const passwd_str, u_char *salt, uint8_t flag);
sqlite3 *decrypt(sqlite3 *db, unsigned char *key);
unsigned char *decryption_helper(sqlite3 *db);

#endif  // !ENC_H
