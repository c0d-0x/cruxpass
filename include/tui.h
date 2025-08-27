#ifndef TUI_H
#define TUI_H
#define NCURSES_WIDECHAR 1
#include <ncurses.h>
#include <sys/types.h>

#include "cruxpass.h"

#define MAX_FIELD_LEN 100
#define ID_WIDTH 7
#define USERNAME_WIDTH 30
#define DESC_WIDTH 51
#define TABLE_WIDTH (ID_WIDTH + USERNAME_WIDTH + DESC_WIDTH + 2)
#define CTRL(x) ((x) & 0x1f)
#define DELETED (-1)

#define HEADER 1
#define SELECTED 2
#define PAGINATION 3
#define STATUS 4
#define SEARCH 5

#define NUM_PANELS 2
#define SECRETE_WIN 0
#define HELP_WIN 1

#define HELP_WIN_WIDTH (TABLE_WIDTH / 2)

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
void draw_border(WINDOW *window);
void free_records(record_array_t *arr);
void handle_search(void);
void init_ncurses(void);
void search_next(void);

WINDOW *create_main_window(int win_hight, int win_width);
void yank_current_record(void);
#endif  // !TUI_H
