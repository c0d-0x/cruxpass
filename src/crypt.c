#include "crypt.h"

#include <locale.h>
#include <sodium/crypto_pwhash.h>
#include <sodium/utils.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cruxpass.h"
#include "database.h"
#include "tui.h"

bool key_gen(unsigned char *key, const char *const passd_str, unsigned char *salt) {
    if (key == NULL) return 0;
    sodium_memzero(key, sizeof(unsigned char) * KEY_LEN);
    if (crypto_pwhash(key, sizeof(unsigned char) * KEY_LEN, passd_str, strlen(passd_str), salt,
                      crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE, crypto_pwhash_ALG_DEFAULT)
        != 0) {
        /* out of memory */
        fprintf(stderr, "Error: Failed not Generate key\n");
        return false;
    }

    return true;
}

bool decrypt(sqlite3 *db, unsigned char *key) {
    if (sqlite3_key(db, key, KEY_LEN) != SQLITE_OK) {
        fprintf(stderr, "Error: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return false;
    }

    if (sqlite3_exec(db, "PRAGMA cipher_log = NONE;", NULL, NULL, NULL) != SQLITE_OK) {
        fprintf(stderr, "Error: Failed to disable logging\n");
    }

    if (sqlite3_exec(db, "SELECT count(*) FROM sqlite_master;", NULL, NULL, NULL) != SQLITE_OK) {
        fprintf(stderr, "Warning: Wrong password\n");
        return false;
    }

    return true;
}

bool create_new_master_secret(sqlite3 *db) {
    bool ok = true;
    char *new_secret = NULL;
    char *temp_secret = NULL;
    unsigned char *new_key = NULL;
    meta_t *meta = NULL;

    init_tui();
    if ((new_secret = get_secret("New Password: ")) == NULL
        || (temp_secret = get_secret("Confirm New Password: ")) == NULL) {
        cleanup_tui();
        return !ok;
    }

    cleanup_tui();
    if (strncmp(new_secret, temp_secret, MASTER_MAX_LEN) != 0) {
        fprintf(stderr, "Error: Passwords do not match\n");
        sodium_free(new_secret);
        sodium_free(temp_secret);
        return !ok;
    }

    if ((meta = calloc(1, sizeof(meta_t) + SALT_LEN)) == NULL) CRXP__OUT_OF_MEMORY();
    if ((new_key = (unsigned char *) sodium_malloc(sizeof(unsigned char) * KEY_LEN)) == NULL) CRXP__OUT_OF_MEMORY();

    randombytes_buf(meta->salt, sizeof(unsigned char) * SALT_LEN);
    if (!key_gen(new_key, (const char *const) new_secret, meta->salt)) {
        fprintf(stderr, "Error: Failed to Create New Password\n");
        goto defer;
    }

    if (sqlite3_rekey(db, new_key, KEY_LEN) != SQLITE_OK) {
        fprintf(stderr, "Error: Failed to change password: %s", sqlite3_errmsg(db));
        goto defer;
    }

    if (!update_meta(NULL, meta)) {
        goto defer;
    }

    ok = false;
defer:
    free(meta);
    sodium_free(new_secret);
    sodium_free(temp_secret);
    sodium_memzero(new_key, KEY_LEN);
    sodium_free(new_key);
    return !ok;
}

/**
 * Decrypts the db and returns an encryption key.
 */

unsigned char *authenticate(vault_ctx_t *ctx) {
    meta_t *meta = NULL;
    char *master_passd = NULL;
    unsigned char *key = NULL;

    if ((meta = fetch_meta()) == NULL) {
        return NULL;
    }

    init_tui();
    if ((master_passd = get_secret("Master Password: ")) == NULL) {
        cleanup_tui();
        free(meta);
        return NULL;
    }
    cleanup_tui();

    if ((key = (unsigned char *) sodium_malloc(sizeof(unsigned char) * KEY_LEN)) == NULL) CRXP__OUT_OF_MEMORY();
    if (!key_gen(key, master_passd, meta->salt)) {
        fprintf(stderr, "Error: Failed to generate description key\n");
        sodium_free(master_passd);
        sodium_free(key);
        free(meta);
        return NULL;
    }

    free(meta);
    // NOTE: db handler is mutated after a successful decryption
    if (!decrypt(ctx->secret_db, key)) {
        sodium_free(master_passd);
        sodium_free(key);
        return NULL;
    }

    if (!prepare_stmt(ctx)) {
        sodium_free(master_passd);
        sodium_free(key);
        return NULL;
    }

    return key;
}
