#ifndef TUI_H
#define TUI_H
#define NCURSES_WIDECHAR 1
#include <sodium/utils.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <wchar.h>

#include "cruxpass.h"

#define MAX_FIELD_LEN 100
#define ID_WIDTH 7
#define USERNAME_WIDTH 30
#define DESC_WIDTH 51
#define TABLE_WIDTH (ID_WIDTH + USERNAME_WIDTH + DESC_WIDTH + 2)
#define DELETED (-1)

#define COLOR_HEADER (TB_BLUE | TB_BOLD)
#define COLOR_SELECTED (TB_REVERSE)
#define COLOR_PAGINATION (TB_GREEN | TB_BOLD)
#define COLOR_STATUS (TB_RED | TB_BOLD)
#define COLOR_SEARCH (TB_YELLOW | TB_BOLD)

#define HELP_WIN_WIDTH (TABLE_WIDTH / 2)
#define LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

typedef struct {
  ssize_t id;
  char username[USERNAME_MAX_LEN];
  char description[MAX_FIELD_LEN];
} record_t;

typedef struct {
  int size;
  int capacity;
  record_t *data;
} record_array_t;

char *get_input(const char *prompt, char *input, const int text_len, int cod_y, int cod_x);
char *get_secret(const char *prompt);

int callback_feed_tui(void *data, int argc, char **argv, char **column_name);
int main_tui(sqlite3 *db);

void add_record(record_array_t *arr, record_t rec);
void cleanup(void);
void display_pagination_info(void);
void display_status_message(const char *message);
void display_table(void);
/* void draw_border(WINDOW *window); */
void free_records(record_array_t *arr);
void handle_search(void);
bool init_tui(void);
void init_ncurses(void);
void search_next(void);

/* WINDOW *create_main_window(int win_hight, int win_width); */
#endif  // !TUI_H
