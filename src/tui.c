#define NCURSES_WIDECHAR 1
#include "../include/tui.h"

#include <locale.h>
#include <ncurses.h>
#include <sodium/utils.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../include/cruxpass.h"
#include "../include/database.h"
#include "../include/sqlite3.h"

#define ESC_KEY 27
#define LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

static record_array_t records = {0, 0, NULL};
static volatile int64_t current_position = 1;
static volatile int current_page = 0;
static int records_per_page = 10;
static int term_height, term_width;
static char search_pattern[MAX_FIELD_LEN] = {0};
static int search_active = 0;
static char yank_buffer[MAX_FIELD_LEN * 3] = {0};

void init_ncurses(void) {
  initscr();
  cbreak();
  keypad(stdscr, TRUE);
  noecho();
  curs_set(0);
  getmaxyx(stdscr, term_height, term_width);
}

static void reset_term(void) {
  echo();
  curs_set(0);
  resetterm();
  endwin();
}

static void set_colors(void) {
  start_color();
  init_pair(1, COLOR_WHITE, COLOR_BLUE);    // Header
  init_pair(2, COLOR_BLACK, COLOR_WHITE);   // Selected row
  init_pair(3, COLOR_BLACK, COLOR_GREEN);   // Pagination info
  init_pair(4, COLOR_WHITE, COLOR_RED);     // Status message and DELETED
  init_pair(5, COLOR_BLACK, COLOR_YELLOW);  // Search highlight
}

/**
 * Displays centered ASCII art (4 bytes wide char) for "cruxpass"
 */
static void display_ascii_art(void) {
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

  init_ncurses();
  setlocale(LC_ALL, "");

  int art_lines = LEN(ascii_art);
  int art_width = wcslen(ascii_art[0]);

  int start_y = (term_height / 2) - (art_lines / 2) - 3;
  int start_x = (term_width - art_width) / 2;

  if (start_y < 0) start_y = 0;
  if (start_x < 0) start_x = 0;

  clear();
  for (int i = 0; i < art_lines; i++) {
    if (start_y + i < term_height - 1) {
      mvaddwstr(start_y + i, start_x, ascii_art[i]);
    }
  }
  refresh();
}

char *get_input(const char *prompt, char *input, const int input_len, int cod_y, int cod_x) {
  if (prompt == NULL) {
    fprintf(stderr, "Error: No prompt provided\n");
    return NULL;
  }

  init_ncurses();
  curs_set(1);
  echo();

  mvprintw(cod_y, cod_x, prompt, NULL);
  getnstr(input, input_len);
  reset_term();
  return input;
}

char *get_password(const char *prompt) {
  if (prompt == NULL) {
    fprintf(stderr, "Error: No prompt provided\n");
    return NULL;
  }

  char *password = NULL;
  int prompt_len = strlen(prompt);
  ssize_t ch, i = 0;
  ssize_t center_y;
  ssize_t center_x;

  init_ncurses();
  display_ascii_art();
  curs_set(1);

  if (sodium_init() == -1) {
    free(password);
    fprintf(stderr, "Error: Failed to initialize libsodium\n");
    reset_term();
    return NULL;
  }

  if ((password = (char *)sodium_malloc(MASTER_LENGTH + 1)) == NULL) {
    fprintf(stderr, "Error: Failed to allocate memory\n");
    reset_term();
    return NULL;
  }

  sodium_memzero((void *const)password, MASTER_LENGTH + 1);
  center_y = (term_height / 2) + 4;
  center_x = (term_width - prompt_len - MASTER_LENGTH) / 2;

  if (center_x < 0) center_x = 0;
  if (center_y >= term_height - 1) center_y = term_height - 2;

  move(center_y, center_x);
  attron(COLOR_PAIR(4) | A_BOLD);
  printw("%s", prompt);
  attroff(COLOR_PAIR(4) | A_BOLD);
  refresh();

  while (1) {
    ch = getch();

    if (ch == ESC_KEY) {
      display_status_message("Password entry canceled\n");
      sodium_memzero((void *const)password, MASTER_LENGTH);
      sodium_free(password);
      reset_term();
      return NULL;
    } else if (ch == '\n' || ch == KEY_ENTER) {
      break;

    } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
      if (i > 0) {
        password[--i] = '\0';

        move(center_y, center_x + prompt_len + i);
        addch(' ');
        move(center_y, center_x + prompt_len + i);
        refresh();
      }
    } else if (i < MASTER_LENGTH && ch >= 32 && ch <= 126) {
      password[i] = ch;
      move(center_y, center_x + prompt_len + i);
      addch('*');
      refresh();
      i++;
    }
  }

  password[i] = '\0';
  reset_term();
  return password;
}

int main_tui(sqlite3 *db) {
  records.data = NULL;
  records.size = 0;
  records.capacity = 0;
  char status_msg[100] = {0};
  int ch = 0;

  init_ncurses();
  set_colors();
  if (!load_data_from_db(db, &records)) {
    cleanup();
    fprintf(stderr, "Error: Failed to load data from database\n");
    return 0;
  }

  records_per_page = term_height - 6;
  if (records_per_page < 1) records_per_page = 1;

  do {
    clear();
    getmaxyx(stdscr, term_height, term_width);
    records_per_page = term_height - 6;
    if (records_per_page < 1) records_per_page = 1;

    current_page = current_position / records_per_page;

    /* Handles input (limited to vim keys j, k, l, h, y, /, page navigation: Ctrl+b, Ctrl+f and refresh TUI Ctrl +r) */
    switch (ch) {
      case 'k':
        if (current_position > 0) current_position--;
        break;

      case 'j':
        if (current_position < records.size - 1) current_position++;
        break;

      case 'h':
        current_position = (current_page - 1) * records_per_page;
        if (current_position < 0) current_position = 0;
        break;

      case 'l':
        current_position = (current_page + 1) * records_per_page;
        if (current_position >= records.size) current_position = records.size - 1;
        break;

      case 'g':
        current_position = 0;
        break;
      case 'G':
        current_position = records.size - 1;
        break;

      case CTRL('b'):
        current_position = current_page * records_per_page - records_per_page;
        if (current_position < 0) current_position = 0;
        break;

      case CTRL('f'):
        current_position = (current_page + 1) * records_per_page;
        if (current_position >= records.size) current_position = records.size - 1;
        break;

      case 'y':
        yank_current_record();
        snprintf(status_msg, sizeof(status_msg), "Info: Record ID <%ld> yanked to buffer",
                 records.data[current_position].id);
        break;

      case '/':
        handle_search();
        break;
      case 'n':
        if (search_active && search_pattern[0] != '\0') search_next();
        break;
      case 'd':
        /* TODO: deletes a password and reload the TUI. */
        if (!delete_password_v2(db, records.data[current_position].id)) {
          display_status_message("Error: Failed to delete password");
          getch();
          cleanup();
          return 0;
        }

        /* TODO: make display_status_message variadic to show pswd id */
        display_status_message("Note: password deleted");
        records.data[current_position].id = DELETED;
        break;
      /* Refresh screen */
      case CTRL('r'):
        clear();
        break;
      default:
        refresh();
    }

    /* Display data */
    display_table();
    display_pagination_info();
    refresh();

    if (status_msg[0] != '\0') {
      display_status_message(status_msg);
      memset(status_msg, 0, 100);
    }

    refresh();
  } while ((ch = getch()) != 'q' && ch != 'Q');

  cleanup();
  return 1;
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

void free_records(record_array_t *arr) {
  if (arr->data != NULL) {
    free(arr->data);
    arr->data = NULL;
  }
  arr->size = 0;
  arr->capacity = 0;
}

int callback_feed_tui(void *data, int argc, char **argv, char **azColName) {
  record_array_t *arr = (record_array_t *)data;
  record_t rec;

  rec.id = argv[0] ? atoi(argv[0]) : 0;

  if (argv[1] != NULL)
    strncpy(rec.username, argv[1], USERNAME_LENGTH - 1);
  else
    strcpy(rec.username, "");
  rec.username[USERNAME_LENGTH - 1] = '\0';

  if (argv[2] != NULL)
    strncpy(rec.description, argv[2], DESC_LENGTH - 1);
  else
    strcpy(rec.description, "");
  rec.description[DESC_LENGTH - 1] = '\0';

  add_record(arr, rec);
  return 0;
}

void display_table(void) {
  int64_t start_idx = current_page * records_per_page;
  int64_t end_idx = start_idx + records_per_page;
  if (end_idx > records.size) end_idx = records.size;

  int table_width = ID_WIDTH + USERNAME_WIDTH + DESC_WIDTH + 2;
  int start_x = (term_width - table_width) / 2;
  if (start_x < 0) start_x = 0;

  clear();
  attron(COLOR_PAIR(1) | A_BOLD);
  mvprintw(1, start_x, "%-*s %-*s %-*s", ID_WIDTH, "ID", USERNAME_WIDTH, "USERNAME", DESC_WIDTH, "DESCRIPTION");
  attroff(COLOR_PAIR(1) | A_BOLD);

  mvprintw(2, start_x, "%.*s", table_width,
           "-------------------------------------------------------------------------------------------------------");
  record_t *rec = NULL;
  int64_t row;
  for (int64_t i = start_idx; i < end_idx; i++) {
    rec = &records.data[i];
    row = 3 + (i - start_idx);

    if (rec->id == DELETED) {
      attron(COLOR_PAIR(4) | A_DIM);
      mvprintw(row, start_x, "%-*s %-*s %-*.*s", ID_WIDTH, "DELETED", USERNAME_WIDTH, rec->username, DESC_WIDTH,
               DESC_WIDTH, rec->description);
      attroff(COLOR_PAIR(4) | A_DIM);
      continue;
    }

    if (i == current_position) attron(COLOR_PAIR(2) | A_BOLD);

    /* TODO: Refactor search highlight */
    else if (search_active && search_pattern[0] != '\0' &&
             (strstr(rec->username, search_pattern) || strstr(rec->description, search_pattern))) {
      attron(COLOR_PAIR(5));
    }

    mvprintw(row, start_x, "%-*ld %-*s %-*.*s", ID_WIDTH, rec->id, USERNAME_WIDTH, rec->username, DESC_WIDTH,
             DESC_WIDTH, rec->description);

    if (i == current_position)
      attroff(COLOR_PAIR(2) | A_BOLD);
    else if (search_active && search_pattern[0] != '\0' &&
             (strstr(rec->username, search_pattern) || strstr(rec->description, search_pattern))) {
      attroff(COLOR_PAIR(5));
    }
  }

  /* TODO: printed only with '?' in tui mode. centered and in a new window (by line)  */
  mvprintw(term_height - 1, 2,
           "j/k: Up/Down | h/l: Page | g/G: Top/Bottom | Ctrl+f/b: Page | y: Yank | /: Search | n: Next | q: Quit");
  refresh();
}

void display_pagination_info(void) {
  int total_pages = (records.size + records_per_page - 1) / records_per_page;
  char pagination_info[100];
  snprintf(pagination_info, sizeof(pagination_info), "Status: Page %d of %d | Record %ld of %d", current_page + 1,
           total_pages, current_position + 1, records.size);

  int info_len = strnlen(pagination_info, 100);
  int start_x = (term_width - info_len) / 2;
  if (start_x < 0) start_x = 0;

  attron(COLOR_PAIR(3) | A_BOLD);
  mvprintw(term_height - 2, start_x, "%s", pagination_info);
  attroff(COLOR_PAIR(3) | A_BOLD);
  refresh();
}

void cleanup(void) {
  free_records(&records);
  reset_term();
}

void handle_search(void) {
  echo();
  curs_set(1);
  memset(search_pattern, 0, sizeof(search_pattern));

  mvprintw(term_height - 3, 2, "Search: ");
  clrtoeol();
  refresh();

  getnstr(search_pattern, MAX_FIELD_LEN - 1);
  noecho();
  curs_set(0);

  if (search_pattern[0] != '\0') {
    search_active = 1;
    search_next();
  } else {
    search_active = 0;
  }
}

void search_next(void) {
  int start_pos = current_position + 1;
  if (start_pos >= records.size) start_pos = 0;

  int i;
  for (i = 0; i < records.size; i++) {
    int pos = (start_pos + i) % records.size;
    record_t *rec = &records.data[pos];

    if (strstr(rec->username, search_pattern) || strstr(rec->description, search_pattern)) {
      current_position = pos;
      break;
    }
  }

  if (i == records.size) {
    display_status_message("INFO: Pattern not found\n");
  }
}

void yank_current_record(void) {
  if (current_position < 0 || current_position >= records.size) return;

  record_t *rec = &records.data[current_position];

  // TODO: fetch password and copy to clipboard
  snprintf(yank_buffer, sizeof(yank_buffer), "ID: %ld, Username: %s, Description: %s", rec->id, rec->username,
           rec->description);
}

void display_status_message(const char *message) {
  int msg_len = strlen(message);
  int start_x = (term_width - msg_len) / 2;
  if (start_x < 0) start_x = 0;

  attron(COLOR_PAIR(4) | A_BOLD);
  mvprintw(term_height - 3, start_x, "%s", message);
  attroff(COLOR_PAIR(4) | A_BOLD);
  refresh();
}
