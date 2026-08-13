#include "cruxpass.h"

#include <errno.h>
#include <sodium/utils.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "crypt.h"
#include "database.h"

char *cruxpass_db_path;
char *meta_db_path;

char *random_secret(int secret_len, bank_options_t *opt) {
    if (secret_len < GEN_SECRET_MIN_LEN || secret_len > RAND_SECRET_MAX_LEN) {
        printf("Warning: Secret must be at least %d or %d characters long\n", GEN_SECRET_MIN_LEN, RAND_SECRET_MAX_LEN);
        return NULL;
    }

    char *secret = NULL;
    char *bank = NULL;
    if ((bank = init_secret_bank(opt)) == NULL) {
        fprintf(stderr, "Error: Failed to init secret bank\n");
        return NULL;
    }

    const int bank_len = strlen(bank);
    if ((secret = malloc(sizeof(char) * secret_len + 1)) == NULL) CRXP__OUT_OF_MEMORY();
    if (sodium_init() == -1) {
        free(bank);
        free(secret);
        fprintf(stderr, "Error: Failed to initialize libsodium\n");
        return NULL;
    }

    for (int i = 0; i < secret_len; i++) {
        secret[i] = bank[(int) randombytes_uniform(bank_len)];
    }

    free(bank);
    secret[secret_len] = '\0';
    return secret;
}

int export_secrets(sqlite3 *db, const char *export_file) {
    FILE *fp = NULL;
    const unsigned char *username = NULL;
    const unsigned char *description = NULL;
    const unsigned char *secret = NULL;
    sqlite3_stmt *sql_stmt = NULL;

    if ((fp = fopen(export_file, "wb")) == NULL) {
        fprintf(stderr, "Error: Failed to open %s: %s", export_file, strerror(errno));
        return CRXP_ERR;
    }

    char *sql_str = "SELECT username, secret, description FROM secrets;";
    if (sqlite3_prepare_v2(db, sql_str, -1, &sql_stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Error: Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        fclose(fp);
        return CRXP_ERR;
    }

    fputs("Username,Secret,Description\n", fp);
    while (sqlite3_step(sql_stmt) == SQLITE_ROW) {
        username = sqlite3_column_text(sql_stmt, 0);
        secret = sqlite3_column_text(sql_stmt, 1);
        description = sqlite3_column_text(sql_stmt, 2);

        fprintf(fp, "%s,%s,%s\n", username, secret, description);
    }

    fclose(fp);
    sqlite3_finalize(sql_stmt);
    return CRXP_OK;
}

/**
 * @max_length: field MAX, a const
 * @field_name: for error handling
 * @line_num for error handling
 * Verifies: length requirements and each character of a given field
 */
static int verify_field(const int max_length, str_view_t view, const char *field_name, size_t line_num) {
    if (view.str == NULL || view.len == 0) {
        fprintf(stderr, "Error: Missing %s at line %zu\n", field_name, line_num);
        return CRXP_ERR;
    }

    if (view.len > max_length) {
        fprintf(stderr, "Error: %s at line %zu is more than %d characters\n", field_name, line_num, max_length);
        return CRXP_ERR;
    }

    if (view.len < FIELD_MIN && (max_length != SECRET_MAX_LEN)) {
        fprintf(stderr, "Error: %s at line %zu is less than %d characters\n", field_name, line_num, FIELD_MIN);
        return CRXP_ERR;
    } else if (view.len < SECRET_MIN_LEN && (max_length == SECRET_MAX_LEN)) {
        fprintf(stderr, "Error: %s at line %zu is less than %d characters\n", field_name, line_num, SECRET_MIN_LEN);
        return CRXP_ERR;
    }

    for (int i = 0; i < view.len; i++) {
        if (!IS_VALID(view.str[i])) {
            fprintf(stderr, "Error: %s at line %zu has an invalid character '%c'\n", field_name, line_num, view.str[i]);
            return CRXP_ERR;
        }
    }

    return CRXP_OK;
}

static char *find_char(char *str, char cc) {
    if (str == NULL) return NULL;
    while (*str != '\0' && cc != *str && *str != '\n') str++;
    if ((*str) == '\0' || *str == '\n') return NULL;

    return str;
}

static int count_char(char *str, char cc) {
    int i = 0;
    char *tmp = str;
    while ((tmp = find_char(str, cc)) != NULL && ++i) str += (size_t) (tmp - str) + 1;

    return i;
}

static int chup_bychar(char *buff, str_view_t *views, char cc) {
    if (views == NULL || buff == NULL) return 0;

    int i = 0;
    char *tmp = buff;
    while (i < CSV_COLUMN_MAX && tmp != NULL) {
        tmp = find_char(buff, cc);

        views[i].str = buff;
        views[i].len = (tmp != NULL) ? (int) (tmp - buff) : (int) (strlen(buff));
        buff += views[i++].len + 1;
    };

    return i;
}

int import_secrets(sqlite3 *db, const char *import_file) {
    FILE *fp = NULL;
    char *buf = NULL;
    size_t line_num = 1;
    str_view_t view_tab[CSV_COLUMN_MAX] = {0};
    bool saved = false;

    if ((fp = fopen(import_file, "r")) == NULL) {
        fprintf(stderr, "Error: Failed to open %s: %s", import_file, strerror(errno));
        return CRXP_ERR;
    }

    if ((buf = sodium_malloc(sizeof(char) * BUFFMAX + 1)) == NULL) CRXP__OUT_OF_MEMORY();
    while (fgets(buf, BUFFMAX, fp) != NULL) {
        buf[strcspn(buf, "\n")] = '\0';
        if (count_char(buf, ',') != 2) {
            fprintf(stderr, "Error: Invalid row in %s line: %zu\n", import_file, line_num++);
            line_num++;
            continue;
        }

        chup_bychar(buf, view_tab, ',');
        if (!verify_field(USERNAME_MAX_LEN, view_tab[VIEW_UNAME], CSV_HEADER_UNAME, line_num)) {
            line_num++;
            continue;
        }

        if (!verify_field(SECRET_MAX_LEN, view_tab[VIEW_SECRET], CSV_HEADER_SECRET, line_num)) {
            line_num++;
            continue;
        }

        if (!verify_field(DESC_MAX_LEN, view_tab[VIEW_DESC], CSV_HEADER_DESC, line_num)) {
            line_num++;
            continue;
        }

        if (!insert_view_record(db, view_tab)) fprintf(stderr, "Error: Failed to insert record at line: %zu", line_num);
        sodium_memzero((void *) buf, BUFFMAX);
        if (!saved) saved = true;
        line_num++;
    }

    fclose(fp);
    sodium_memzero((void *) buf, BUFFMAX);
    sodium_free(buf);
    return (saved) ? CRXP_OK : CRXP_ERR;
}

static bool create_run_dir(const char *path) {
    if (mkdir(path, 0776) == 0) fprintf(stderr, "Info: Run directory created\n");
    else if (errno != EEXIST) {
        fprintf(stderr, "Error: Failed to create run directory: %s\n", strerror(errno));
        fprintf(stderr, "Run Directory: %s\n", path);
        return false;
    }

    return true;
}

static char *set_path(char *path, char *file_name) {
    char *full_path = NULL;
    if (path == NULL || file_name == NULL) return NULL;
    if ((full_path = calloc(MAX_PATH_LEN, sizeof(char))) == NULL) CRXP__OUT_OF_MEMORY();
    if (snprintf(full_path, MAX_PATH_LEN, "%s/%s", path, file_name) <= 0) return NULL;

    return full_path;
}

static bool validate_run_dir(char *path) {
    bool allocated = false;
    if (path == NULL) {
        char *home = NULL;
        allocated = true;
        if ((home = getenv("HOME")) == NULL) return false;
        if (strlen(home) > HOME_PATH_MAX_LEN) {
            fprintf(stderr, "Error: [ %s ] home path too long, max %u", home, HOME_PATH_MAX_LEN);
            return false;
        }

        if ((path = calloc(MAX_PATH_LEN + 1, sizeof(char))) == NULL) CRXP__OUT_OF_MEMORY();
        if (snprintf(path, MAX_PATH_LEN, "%s/%s", home, CRUXPASS_RUNDIR) <= 0) {
            free(path);
            return false;
        }

        if (!create_run_dir(path)) {
            free(path);
            return false;
        }

    } else if (strlen(path) > MAX_PATH_LEN - HOME_PATH_MAX_LEN) {
        fprintf(stderr, "Error: Run directory path too long (max: %d characters)\n", MAX_PATH_LEN);
        return false;
    }

    struct stat file_stat = {0};
    if (stat(path, &file_stat) != 0) {
        fprintf(stderr, "Error: [ %s ] is either missing or not a valid directory\n", path);
        if (allocated) free(path);
        return false;
    }

    if (!S_ISDIR(file_stat.st_mode)) {
        fprintf(stderr, "Error: [ %s ] is not a valid directory\n", path);
        if (allocated) free(path);
        return false;
    }

    cruxpass_db_path = set_path(path, CRUXPASS_DB);
    meta_db_path = set_path(path, META_DB);

    if (allocated) free(path);
    return true;
}

vault_ctx_t *initcrux(char *run_dir) {
    vault_ctx_t *ctx = NULL;
    if (!validate_run_dir(run_dir)) return NULL;
    if (sodium_init() == -1) {
        fprintf(stderr, "Error: Failed to initialize libsodium\n");
        return NULL;
    }

    int inited = init_sqlite();
    switch (inited) {
        case CRXP_OKK: fprintf(stderr, "Info: New password created\nWarning: Retry your operation\n"); return NULL;
        case CRXP_OK:
            if ((ctx = malloc(sizeof(vault_ctx_t))) == NULL) CRXP__OUT_OF_MEMORY();
            if ((ctx->secret_db = open_db(cruxpass_db_path, SQLITE_OPEN_READWRITE)) == NULL) {
                free(ctx);
                return NULL;
            }
            return ctx;
        default: return NULL;
    }
}

char *init_secret_bank(const bank_options_t *opt) {
    if (opt == NULL) return NULL;
    char *all_upper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char *all_lower = "abcdefghijklmnopqrstuvwxyz";
    char *all_numbers = "0123456789";
    char *all_symbols = "#%&()_+={}[-]:<@>?!";

    char *unambiguous_upper = "ABCDEFGHJKLMNPQRSTUVWXYZ";
    char *unambiguous_lower = "abcdefghijkmnopqrstuvwxyz";
    char *unambiguous_numbers = "123456789";
    char *unambiguous_symbols = "#%&()_+={}[-]:@";

    char *bank = NULL;
    if ((bank = malloc(sizeof(char) * BANK_SIZE)) == NULL) CRXP__OUT_OF_MEMORY();
    sodium_memzero(bank, sizeof(char) * BANK_SIZE);

    if (opt->ex_ambiguous) {
        if (opt->upper) strcat(bank, unambiguous_upper);
        if (opt->lower) strcat(bank, unambiguous_lower);
        if (opt->digit) strcat(bank, unambiguous_numbers);
        if (opt->symbols) strcat(bank, unambiguous_symbols);
    } else {
        if (opt->upper) strcat(bank, all_upper);
        if (opt->lower) strcat(bank, all_lower);
        if (opt->digit) strcat(bank, all_numbers);
        if (opt->symbols) strcat(bank, all_symbols);
    }

    return bank;
}
