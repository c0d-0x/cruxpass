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

#define FIELD_MIN 3
#define BANK_SIZE 92
#define DESC_MAX_LEN 256
#define FILE_PATH_LEN 256
#define LOGIN_MAX_LEN 48
#define MAX_PATH_LEN 512
#define HOME_PATH_MAX_LEN 64
#define RAND_SECRET_MAX_LEN 256
#define SECRET_MAX_LEN 128
#define SECRET_MIN_LEN 8
#define GEN_SECRET_MIN_LEN 4
#define USERNAME_MAX_LEN 32

#define CSV_COLUMN_MAX 3
#define CSV_HEADER_UNAME "Username"
#define CSV_HEADER_SECRET "Secret"
#define CSV_HEADER_DESC "Description"

#define CRXP_KDF_ITER 256000
#define CRXP_CIPHER_PAGE_SIZE 4096
#define CRXP_KDF_ALGORITHM "PBKDF2_HMAC_SHA512"
#define CRXP_HMAC_ALGORITHM "HMAC_SHA512"

#ifndef CRUXPASS_DB
#define CRUXPASS_DB "cruxpass.db"
#endif

#ifndef META_DB
#define META_DB "meta.db"
#endif

#ifndef CRUXPASS_RUNDIR
#define CRUXPASS_RUNDIR ".local/share/cruxpass"  // default ~/.local/share/cruxpass/
#endif

#define IS_VALID(ch) (((ch >= 0x20) && (ch <= 0x7E) && (ch != 0x2C)))
#define IS_DIGIT(ch) ((ch >= 0x30) && (ch <= 0x39))

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
    char description[DESC_MAX_LEN];
} secret_t;

typedef struct {
    sqlite3 *secret_db;
    sqlite3 *meta_db;
} vault_ctx_t;

typedef enum {
    CRXP_ERR,
    CRXP_OK,
    CRXP_OKK
} ERROR_T;

typedef struct {
    int len;
    char *str;
} str_view_t;

typedef enum {
    VIEW_UNAME,
    VIEW_SECRET,
    VIEW_DESC
} index_t;

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
int import_secrets(sqlite3 *db, const char *import_file);

#endif  // !CRUXPASS_H
