#include "tui.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

bool add_record(record_array_t *arr, record_t rec) {
    if (arr->size >= arr->capacity) {
        int new_capacity = arr->capacity == 0 ? 8 : arr->capacity * 2;
        record_t *new_data = realloc(arr->data, new_capacity * sizeof(record_t));
        if (new_data == NULL) {
            fprintf(stderr, "Error: Failed to allocate Memory\n");
            return false;
        }
        arr->data = new_data;
        arr->capacity = new_capacity;
    }

    arr->data[arr->size++] = rec;
    return true;
}

/*NOTE: This is used by the load_records */
int pipeline(void *data, int argc, char **argv, char **azColName) {
    record_array_t *arr = (record_array_t *) data;
    record_t rec;

    rec.id = argv[0] ? atoi(argv[0]) : 0;

    if (argv[1] != NULL) strncpy(rec.username, argv[1], USERNAME_MAX_LEN);
    else strcpy(rec.username, "");
    rec.username[USERNAME_MAX_LEN] = '\0';

    if (argv[2] != NULL) strncpy(rec.description, argv[2], DESC_MAX_LEN);
    else strcpy(rec.description, "");
    rec.description[DESC_MAX_LEN] = '\0';

    if (!add_record(arr, rec)) return 1;
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
