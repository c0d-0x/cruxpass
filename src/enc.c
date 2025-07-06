#include "../include/enc.h"

#include <ncurses.h>
#include <sodium/utils.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "../include/cruxpass.h"
#include "../include/database.h"
#include "../include/tui.h"

int generate_key_pass_hash(unsigned char *key, char *hash, const char *const passd_str, unsigned char *salt,
                           uint8_t flag) {
  if (sodium_init() == -1) {
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

hashed_pass_t *authenticate(char *master_passd) {
  if (sodium_init() == -1) {
    return NULL;
  }

  hashed_pass_t *hash_obj = NULL;
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

int create_new_master_passd(sqlite3 *db, unsigned char *key) {
  int ret = 0;
  char *new_passd = NULL;
  char *temp_passd = NULL;
  unsigned char *new_key = NULL;
  hashed_pass_t *new_hashed_password = NULL;

  // TODO: Refactor _get_new_password
  if ((new_passd = get_password("New Password: ")) == NULL ||
      (temp_passd = get_password("Confirm New Password: ")) == NULL) {
    return ret;
  }

  if (strncmp(new_passd, temp_passd, MASTER_LENGTH) != 0) {
    fprintf(stderr, "Error: Passwords do not match\n");
    sodium_free(new_passd);
    sodium_free(temp_passd);
    return ret;
  }

  if (sodium_init() == -1) {
    fprintf(stderr, "Error: Failed to initialize libsodium");
    sodium_free(new_passd);
    sodium_free(temp_passd);
    return ret;
  }

  new_hashed_password = calloc(1, sizeof(hashed_pass_t));
  new_key = (unsigned char *)sodium_malloc(sizeof(unsigned char) * KEY_LEN);

  if (new_hashed_password == NULL || key == NULL || new_key == NULL) {
    fprintf(stderr, "Error: Memory Allocation Fail\n");
    sodium_free(new_passd);
    sodium_free(temp_passd);
    return ret;
  }

  randombytes_buf(new_hashed_password->salt, sizeof(unsigned char) * SALT_HASH_LEN);

  if (!generate_key_pass_hash(new_key, new_hashed_password->hash, (const char *const)new_passd,
                              new_hashed_password->salt, GEN_KEY | GEN_HASH)) {
    fprintf(stderr, "Error: Failed to Create New Password\n");
    goto free_all;
  }

  if (sqlite3_rekey(db, new_key, KEY_LEN) != SQLITE_OK) {
    fprintf(stderr, "Error: Failed to change password: %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    goto free_all;
  }

  if (!update_secret(new_hashed_password)) {
    sqlite3_close(db);
    goto free_all;
  }

  ret = 1;
free_all:
  free(new_hashed_password);
  sodium_free(new_passd);
  sodium_free(temp_passd);
  sodium_free(new_key);
  return ret;
}

/**
 * decrypts the db and returns a key for encryption.
 */

unsigned char *decryption_helper(sqlite3 *db) {
  char *master_passd = NULL;
  unsigned char *key;
  hashed_pass_t *hashed_password = NULL;

  if (sodium_init() == -1) {
    fprintf(stderr, "Error: Failed to initialize libsodium");
    return NULL;
  }

  if ((key = (unsigned char *)sodium_malloc(sizeof(unsigned char) * KEY_LEN)) == NULL) {
    fprintf(stderr, "Error: Failed to Allocate Memory: sodium_malloc\n");
    return NULL;
  }

  if ((master_passd = get_password("Master Password: ")) == NULL) {
    sodium_free(key);
    return NULL;
  }

  if ((hashed_password = authenticate(master_passd)) == NULL) {
    sodium_free(key);
    sodium_free(master_passd);
    free(hashed_password);
    return NULL;
  }

  if (!generate_key_pass_hash(key, NULL, master_passd, hashed_password->salt, GEN_KEY)) {
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
