#include "../include/tui.h"

#include <locale.h>
#include <panel.h>
#include <sodium/utils.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>

#include "../include/cruxpass.h"
#include "../include/database.h"

#define ESC_KEY 27
#define LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

static record_array_t records = {0, 0, NULL};
static volatile int64_t current_position = 1;
static volatile int current_page = 0;
static int records_per_page = 10;
static int term_height, term_width;
static char search_pattern[MAX_FIELD_LEN] = {0};
static int search_active = 0;

/**
 * Displays centered ASCII art (4 bytes wide char)
 */

bool init_tui(void) { return true; }
static void draw_art(void) {
  const wchar_t *ascii_art[] = {L" ▄████▄   █    ██ ▒██   ██▒ ██▓███   ▄▄▄        ██████   ██████    ",
                                L"▒██▀ ▀█   ██  ▓██▒▒▒ █ █ ▒░▓██░  ██▒▒████▄    ▒██    ▒ ▒██    ▒    ",
                                L"▒▓█    ▄ ▓██  ▒██░░░  █   ░▓██░ ██▓▒▒██  ▀█▄  ░ ▓██▄   ░ ▓██▄      ",
                                L"▒▓▓▄ ▄██▒▓▓█  ░██░ ░ █ █ ▒ ▒██▄█▓▒ ▒░██▄▄▄▄██   ▒   ██▒  ▒   ██▒   ",
                                L"▒ ▓███▀ ░▒▒█████▓ ▒██▒ ▒██▒▒██▒ ░  ░ ▓█   ▓██▒▒██████▒▒▒██████▒▒   ",
                                L"░ ░▒ ▒  ░░▒▓▒ ▒ ▒ ▒▒ ░ ░▓ ░▒▓▒░ ░  ░ ▒▒   ▓▒█░▒ ▒▓▒ ▒ ░▒ ▒▓▒ ▒ ░   ",
                                L"  ░  ▒   ░░▒░ ░ ░ ░░   ░▒ ░░▒ ░       ▒   ▒▒ ░░ ░▒  ░ ░░ ░▒  ░ ░   ",
                                L"░         ░░░ ░ ░  ░    ░  ░░         ░   ▒   ░  ░  ░  ░  ░  ░     ",
                                L"░ ░         ░      ░    ░                 ░  ░      ░        ░     ",
                                L"░                                                                  "};
  init_tui();
}

void add_record(record_array_t *arr, record_t rec) {
  if (arr->size >= arr->capacity) {
    int new_capacity = arr->capacity == 0 ? 8 : arr->capacity * 2;
    record_t *new_data = realloc(arr->data, new_capacity * sizeof(record_t));
    if (new_data == NULL) {
      fprintf(stderr, "Error: Memory allocation failed\n");
      return;
    }
    arr->data = new_data;
    arr->capacity = new_capacity;
  }

  arr->data[arr->size++] = rec;
}

int callback_feed_tui(void *data, int argc, char **argv, char **azColName) {
  record_array_t *arr = (record_array_t *)data;
  record_t rec;

  rec.id = argv[0] ? atoi(argv[0]) : 0;

  if (argv[1] != NULL)
    strncpy(rec.username, argv[1], USERNAME_MAX_LEN - 1);
  else
    strcpy(rec.username, "");
  rec.username[USERNAME_MAX_LEN - 1] = '\0';

  if (argv[2] != NULL)
    strncpy(rec.description, argv[2], DESC_MAX_LEN - 1);
  else
    strcpy(rec.description, "");
  rec.description[DESC_MAX_LEN - 1] = '\0';

  add_record(arr, rec);
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

int main_tui(sqlite3 *db) {
  records.data = NULL;
  records.size = 0;
  records.capacity = 0;
  int ch = 0;

  if (!load_records(db, &records)) {
    cleanup();
    fprintf(stderr, "Error: Failed to load data from database\n");
    return 0;
  }

  return 1;
}
