#include "../include/enc.h"

#include <sodium/utils.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "../include/cruxpass.h"
#include "../include/database.h"
#include "../include/tui.h"

int generate_key_or_hash(unsigned char *key, char *hash, const char *const passd_str, unsigned char *salt,
                         uint8_t flag) {
  if (sodium_init() == -1) {
    fprintf(stderr, "Error: Failed to initialize libsodium");
    return 0;
  }

  if (flag & GEN_KEY) {
    if (key == NULL) return 0;
    sodium_memzero(key, sizeof(unsigned char) * KEY_LEN);
    if (crypto_pwhash(key, sizeof(unsigned char) * KEY_LEN, passd_str, strlen(passd_str), salt,
                      crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE,
                      crypto_pwhash_ALG_DEFAULT) != 0) {
      /* out of memory */
      fprintf(stderr, "Error: Failed not Generate key\n");
      return 0;
    }
  }

  if (flag & GEN_HASH) {
    if (hash == NULL) return 0;
    sodium_memzero(hash, sizeof(unsigned char) * PASS_HASH_LEN);
    if (crypto_pwhash_str(hash, passd_str, strlen(passd_str), crypto_pwhash_OPSLIMIT_SENSITIVE,
                          crypto_pwhash_MEMLIMIT_SENSITIVE) != 0) {
      /* out of memory */
      fprintf(stderr, "Error: Failed not Generate hash\n");
      return 0;
    }
  }

  return 1;
}

sqlite3 *decrypt(sqlite3 *db, unsigned char *key) {
  if (sqlite3_key(db, key, KEY_LEN) != SQLITE_OK) {
    sqlite3_close(db);
    return NULL;
  }
  return db;
}

hash_t *authenticate(char *master_passd) {
  if (sodium_init() == -1) {
    return NULL;
  }

  hash_t *hash_obj = NULL;
  if ((hash_obj = fetch_hash()) == NULL) {
    return NULL;
  }

  if (crypto_pwhash_str_verify(hash_obj->hash, master_passd, strlen(master_passd)) != 0) {
    /* wrong password */
    free(hash_obj);
    fprintf(stderr, "Warning: Wrong Password\n");
    return NULL;
  }

  return hash_obj;
}

int create_new_master_secret(sqlite3 *db, unsigned char *key) {
  int ret = 0;
  char *new_secret = NULL;
  char *temp_secret = NULL;
  unsigned char *new_key = NULL;
  hash_t *new_hashed_secret = NULL;

  // TODO: Refactor _get_new_password
  if ((new_secret = get_secret("New Password: ")) == NULL ||
      (temp_secret = get_secret("Confirm New Password: ")) == NULL) {
    return ret;
  }

  if (strncmp(new_secret, temp_secret, MASTER_MAX_LEN) != 0) {
    fprintf(stderr, "Error: Passwords do not match\n");
    sodium_free(new_secret);
    sodium_free(temp_secret);
    return ret;
  }

  if (sodium_init() == -1) {
    fprintf(stderr, "Error: Failed to initialize libsodium");
    sodium_free(new_secret);
    sodium_free(temp_secret);
    return ret;
  }

  new_hashed_secret = calloc(1, sizeof(hash_t));
  new_key = (unsigned char *)sodium_malloc(sizeof(unsigned char) * KEY_LEN);

  if (new_hashed_secret == NULL || key == NULL || new_key == NULL) {
    fprintf(stderr, "Error: Memory Allocation Fail\n");
    sodium_free(new_secret);
    sodium_free(temp_secret);
    return ret;
  }

  randombytes_buf(new_hashed_secret->salt, sizeof(unsigned char) * SALT_HASH_LEN);

  if (!generate_key_or_hash(new_key, new_hashed_secret->hash, (const char *const)new_secret, new_hashed_secret->salt,
                            GEN_KEY | GEN_HASH)) {
    fprintf(stderr, "Error: Failed to Create New Password\n");
    goto free_all;
  }

  if (sqlite3_rekey(db, new_key, KEY_LEN) != SQLITE_OK) {
    fprintf(stderr, "Error: Failed to change password: %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    goto free_all;
  }

  if (!update_hash(new_hashed_secret)) {
    sqlite3_close(db);
    goto free_all;
  }

  ret = 1;
free_all:
  free(new_hashed_secret);
  sodium_free(new_secret);
  sodium_free(temp_secret);
  sodium_free(new_key);
  return ret;
}

/**
 * decrypts the db and returns a key for encryption.
 */

unsigned char *decryption_helper(sqlite3 *db) {
  char *master_passd = NULL;
  unsigned char *key;
  hash_t *hashed_password = NULL;

  if (sodium_init() == -1) {
    fprintf(stderr, "Error: Failed to initialize libsodium");
    return NULL;
  }

  if ((key = (unsigned char *)sodium_malloc(sizeof(unsigned char) * KEY_LEN)) == NULL) {
    fprintf(stderr, "Error: Failed to Allocate Memory: sodium_malloc\n");
    return NULL;
  }

  if ((master_passd = get_secret("Master Password: ")) == NULL) {
    sodium_free(key);
    return NULL;
  }

  if ((hashed_password = authenticate(master_passd)) == NULL) {
    sodium_free(key);
    sodium_free(master_passd);
    free(hashed_password);
    return NULL;
  }

  if (!generate_key_or_hash(key, NULL, master_passd, hashed_password->salt, GEN_KEY)) {
    fprintf(stderr, "Error: Failed to generate description key\n");
    free(hashed_password);
    sodium_free(master_passd);
    sodium_free(key);
    return NULL;
  }

  /*WARNING: db handler is mutated after a successful decryption*/
  if ((db = decrypt(db, key)) == NULL) {
    sodium_free(master_passd);
    free(hashed_password);
    sodium_free(key);
    return NULL;
  }

  sodium_free(master_passd);
  free(hashed_password);
  return key;
}
