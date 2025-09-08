#include "../include/tui.h"

#include <locale.h>
#include <panel.h>
#include <sodium/utils.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../include/cruxpass.h"
#include "../include/database.h"

#define TB_IMPL
#define __USE_XOPEN
#include <wchar.h>

#include "../include/termbox.h"

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

#include <locale.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

bool init_tui(void) {
  if (tb_init() != TB_OK) {
    fprintf(stderr, "Error: Failed to initialize TUS\n");
    return false;
  }
  return true;
}

void cleanup_tui(void) { tb_shutdown(); }

void draw_art(void) {
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
  tb_clear();
  int term_w = tb_width();
  int term_h = tb_height();
  setlocale(LC_ALL, "");

  int art_lines = LEN(ascii_art);
  int art_width = wcslen(ascii_art[0]);

  int coord_y = (term_h / 2) - (art_lines / 2) - 3;
  int coord_x = (term_w - art_width) / 2;
  int fix_x = coord_x;

  if (coord_y < 0) coord_y = 0;
  if (coord_x < 0) coord_x = 0;

  for (int i = 0; i < art_lines; i++) {
    const wchar_t *w_str = ascii_art[i];
    art_width = wcslen(ascii_art[i]);
    for (int j = 0; j < art_width; j++) {
      tb_set_cell(coord_x, coord_y, ascii_art[i][j], TB_WHITE, TB_DEFAULT);
      coord_x++;
    }
    coord_x = fix_x;
    coord_y++;
  }

  tb_present();
}

char *get_input(const char *prompt, char *input, const int input_len, int coord_y, int coord_x) {
  bool is_input_dynamic = false;
  if (input == NULL) {
    if ((input = calloc(1, input_len)) == NULL) {
      fprintf(stderr, "Error: Failed to allocate memory");
      return NULL;
    }
    is_input_dynamic = true;
  }

  tb_clear();
  tb_print(coord_x, coord_y, TB_WHITE | TB_BOLD, TB_DEFAULT, prompt);
  tb_present();

  int position = 0;
  int prompt_len = strlen(prompt);

  while (position < input_len) {
    struct tb_event ev;
    if (tb_poll_event(&ev) != TB_OK) continue;

    if (ev.type == TB_EVENT_KEY) {
      if (ev.key == TB_KEY_ESC) {
        if (is_input_dynamic) free(input);
        return NULL;
      } else if (ev.key == TB_KEY_ENTER) {
        break;
      } else if (ev.key == TB_KEY_BACKSPACE || ev.key == TB_KEY_BACKSPACE2) {
        if (position > 0) {
          position--;
          input[position] = '\0';
          tb_set_cell(coord_x + prompt_len + position, coord_y, ' ', TB_DEFAULT, TB_DEFAULT);
          tb_present();
        }
      } else if (isalnum(ev.ch) != 0) {
        input[position] = (char)ev.ch;
        tb_set_cell(coord_x + prompt_len + position, coord_y, ev.ch, TB_DEFAULT, TB_DEFAULT);
        position++;
        tb_present();
      }
    }
  }
  return input;
}

char *get_secret(const char *prompt) {
  char *secret = NULL;
  int prompt_len = strlen(prompt);
  ssize_t ch;
  ssize_t center_y;
  ssize_t center_x;

  draw_art();

  int term_w = tb_width();
  int term_h = tb_height();
  if ((secret = (char *)sodium_malloc(MASTER_MAX_LEN + 1)) == NULL) {
    fprintf(stderr, "Error: Failed to allocate memory\n");
    return NULL;
  }

  sodium_memzero((void *const)secret, MASTER_MAX_LEN + 1);
  center_y = (term_h / 2) + 4;
  center_x = (term_w - prompt_len - MASTER_MAX_LEN) / 2;

  if (center_x < 0) center_x = 0;
  if (center_y >= term_h - 1) center_y = term_h - 2;

  tb_print(center_x, center_y, TB_BOLD | TB_WHITE, TB_DEFAULT, prompt);
  tb_present();

  int i = 0;
  struct tb_event ev;
  while (1) {
    tb_poll_event(&ev);
    if (ev.type == TB_EVENT_KEY) {
      if (ev.key == TB_KEY_ENTER || ev.key == TB_KEY_ESC) {
        break;
      }

      if (ev.key == TB_KEY_BACKSPACE2 || ev.key == TB_KEY_BACKSPACE) {
        if (i > 0) {
          secret[--i] = '\0';
          tb_set_cell(center_x + prompt_len + i, center_y, ' ', TB_WHITE, TB_DEFAULT);
          tb_present();
          continue;
        }
      }

      if (isalnum(ev.ch) == 0) continue;

      secret[i] = (char)ev.ch;
      tb_set_cell(center_x + prompt_len + i, center_y, '*', TB_WHITE, TB_DEFAULT);
      tb_present();
      i++;
    }
  }

  return secret;
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
    cleanup_tui();
    fprintf(stderr, "Error: Failed to load data from database\n");
    return 0;
  }

  return 1;
}
