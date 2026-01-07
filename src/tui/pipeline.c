#include "cruxpass.h"
#include "tui.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

bool add_record(record_array_t *vec, record_t rec) {
    if (vec->size >= vec->capacity) {
        int new_capacity = vec->capacity == 0 ? 8 : vec->capacity * 2;
        record_t *new_data = realloc(vec->data, new_capacity * sizeof(record_t));
        if (new_data == NULL) {
            fprintf(stderr, "Error: Failed to allocate Memory\n");
            return false;
        }
        vec->data = new_data;
        vec->capacity = new_capacity;
    }

    vec->data[vec->size++] = rec;
    return true;
}

/*NOTE: This is used by the load_records to feed the tui */
int pipeline(void *data, MAYBE_UNUSED int argc, char **argv, MAYBE_UNUSED char **azColName) {
    record_array_t *vec = (record_array_t *) data;
    record_t rec = {0};

    rec.id = argv[0] ? atoi(argv[0]) : 0;

    if (argv[1] != NULL) strncpy(rec.username, argv[1], USERNAME_MAX_LEN);
    else strcpy(rec.username, "...");

    if (argv[2] != NULL) strncpy(rec.description, argv[2], DESC_MAX_LEN);
    else strcpy(rec.description, "...");

    if (!add_record(vec, rec)) return 1;
    return 0;
}

void free_records(record_array_t *arr) {
    if (arr->data != NULL) {
        free(arr->data);
        arr->data = NULL;
    }

    arr->size = 0;
    arr->capacity = 0;
}
