/**
 * PROJECT: cruxpass - A simple password manager
 * AUTHOR: c0d_0x @ 2025
 * MIT LICENSE
 */

#ifndef CRUXPASS_H
#define CRUXPASS_H

#ifndef SQLITE_HAS_CODEC
#define SQLITE_HAS_CODEC
#endif

#include <sodium.h>
#include <sodium/core.h>
#include <sodium/crypto_pwhash.h>
#include <sodium/utils.h>
#include <sqlcipher/sqlite3.h>
#include <stdbool.h>

#define DESC_MAX_LEN 256
#define FILE_PATH_LEN 32
#define MASTER_MAX_LEN 48
#define MAX_PATH_LEN 256
#define SECRET_MAX_LEN 128
#define RAND_SECRET_MAX_LEN 256
#define SECRET_MIN_LEN 8
#define USERNAME_MAX_LEN 32

#ifndef CRUXPASS_DB
#define CRUXPASS_DB "cruxpass.db"
#endif

#ifndef META_DB
#define META_DB "meta.db"
#endif

#ifndef CRUXPASS_RUNDIR
#define CRUXPASS_RUNDIR ".local/share/cruxpass"  // default ~/.local/share/cruxpass/
#endif

#define CRXP__FATAL(...)              \
    do {                              \
        fprintf(stderr, "Error: ");   \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n");        \
        exit(EXIT_FAILURE);           \
    } while (0)

#define CRXP__OUT_OF_MEMORY() CRXP__FATAL("Out of memory")

// clang-format off
#if defined(__has_attribute)
    #if __has_attribute(unused)
        #define MAYBE_UNUSED __attribute__((unused))
    #endif
#endif
// clang-format on

#ifndef MAYBE_UNUSED
#define MAYBE_UNUSED
#endif

typedef struct {
    ssize_t id;
    char username[USERNAME_MAX_LEN + 1];
    char secret[SECRET_MAX_LEN + 1];
    char description[DESC_MAX_LEN + 1];
} secret_t;

typedef struct {
    sqlite3 *secret_db;
    sqlite3 *meta_db;
} vault_ctx_t;

typedef enum {
    C_ERR,     // C_ERR:     Error
    C_RET_OK,  // C_RET_0K:  Custom OK 1
    C_RET_OKK  // C_RET_OKK: Custom OK 2
} ERROR_T;

typedef struct {
    bool upper;
    bool lower;
    bool digit;
    bool symbols;
    bool ex_ambiguous;
} bank_options_t;

vault_ctx_t *initcrux(char *run_dir);
char *init_secret_bank(const bank_options_t *options);

char *random_secret(int secret_len, bank_options_t *bank_options);
int export_secrets(sqlite3 *db, const char *export_file);
int import_secrets(sqlite3 *db, char *import_file);

#endif  // !CRUXPASS_H
