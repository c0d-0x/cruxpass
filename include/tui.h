#ifndef TUI_H
#define TUI_H
#include <ncurses.h>

#include "../include/sqlcipher.h"

#define MAX_FIELD_LEN 100
#define ID_WIDTH 6
#define USERNAME_WIDTH 20
#define DESC_WIDTH 50
#define CTRL(x) ((x) & 0x1f)

// Structure to hold our data
typedef struct {
  int id;
  char username[MAX_FIELD_LEN];
  char description[MAX_FIELD_LEN];
} Record;

// Structure for a dynamically growing array
typedef struct {
  Record *data;
  int size;
  int capacity;
} RecordArray;

/**
 * Gets a password from the user, displaying '*' for each character
 * Left-aligned prompt on a vertically centered line
 *
 * @param prompt The prompt message to display
 * @param password Buffer to store the password (must be at least MAX_PASSWORD_LEN+1 bytes)
 * @param max_len Maximum length of password
 * @return The length of the password entered (0 if canceled with ESC)
 */

char *get_password(const char *prompt);
void init_ncurses();
int main_tui(sqlite3 *db);
int callback(void *data, int argc, char **argv, char **column_name);
void display_table();
void display_pagination_info();
void cleanup();
void add_record(RecordArray *arr, Record rec);
void free_records(RecordArray *arr);
void handle_search();
void search_next();
void yank_current_record();
void display_status_message(const char *message);

void draw_border(WINDOW *window);

WINDOW *create_main_window(int win_hight, int win_width);

#endif  // !TUI_H
