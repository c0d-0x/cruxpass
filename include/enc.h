#ifndef ENC_H
#define ENC_H
#define SQLITE_HAS_CODEC
#include <sodium.h>
#include <sodium/core.h>
#include <sodium/crypto_pwhash.h>
#include <sodium/utils.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "sqlcipher.h"

#define GEN_KEY 0x01
#define GEN_HASH 0x02
#define KEY_LEN crypto_box_SEEDBYTES
#define PASS_HASH_LEN crypto_pwhash_STRBYTES
#define SALT_HASH_LEN crypto_pwhash_SALTBYTES
#define BUFFMAX PASSLENGTH + USERNAMELENGTH + DESCLENGTH

typedef struct {
  char hash[PASS_HASH_LEN + 1];
  unsigned char salt[SALT_HASH_LEN];
} hashed_pass_t;

unsigned char *decryption_helper(sqlite3 *db);

/**
 * Takes in a master password and returns a hashed password.
 * @param master_passd the master password to hash
 * @return a hashed password
 */
hashed_pass_t *authenticate(char *master_passdm);

/**
 * Generates a key or  password hash for encryption and decryption.
 *
 * @param key a pointer to a buffer to store the encryption key
 * @param hashed_password a pointer to a buffer to store the hashed password
 * @param new_passd the plaintext password to hash
 * @param salt a pointer to the salt for key generation
 * @param tag a flag indicating whether to generate a decryption key or not
 * @return 0 on success, 1 on failure
 */
int generate_key_pass_hash(u_char *key, char *hash, const char *const passwd_str, u_char *salt, uint8_t flag);

/**
 * Decrypts a sqlite3 db using the given key.
 *
 * @param db a sqlite3 db handler, already opened
 * @param key the encryption key
 * @return 0 on success, 1 on failure
 */
sqlite3 *decrypt(sqlite3 *db, unsigned char *key);

#endif  // !ENC_H
