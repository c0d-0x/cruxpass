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
#define CTRL(x) ((x) & 0x1f)
#define DELETED -1

#define HEADER 1
#define SELECTED 2
#define PAGINATION 3
#define STATUS 4
#define SEARCH 5

typedef struct {
  ssize_t id;
  char username[USERNAME_LENGTH];
  char description[MAX_FIELD_LEN];
} record_t;

typedef struct {
  int size;
  int capacity;
  record_t *data;
} record_array_t;

/**
 * Gets a password from the user, displaying '*' for each character
 * Left-aligned prompt on a vertically centered line
 *
 * @param prompt The prompt message to display
 * @param password Buffer to store the password (must be at least MAX_PASSWORD_LEN+1 bytes)
 * @param max_len Maximum length of password
 * @return The length of the password entered (0 if canceled with ESC)
 */
char *get_input(const char *prompt, char *input, const int text_len, int cod_y, int cod_x);
char *get_password(const char *prompt);
void init_ncurses(void);
int main_tui(sqlite3 *db);
int callback_feed_tui(void *data, int argc, char **argv, char **column_name);
void display_table(void);
void display_pagination_info(void);
void cleanup(void);
void add_record(record_array_t *arr, record_t rec);
void free_records(record_array_t *arr);
void handle_search(void);
void search_next(void);
void yank_current_record(void);
void display_status_message(const char *message);
void draw_border(WINDOW *window);
WINDOW *create_main_window(int win_hight, int win_width);

#endif  // !TUI_H
