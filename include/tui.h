#ifndef TUI_H
#define TUI_H
#include <sodium/utils.h>
#include <stdbool.h>
#include <stdint.h>

#include "cruxpass.h"
#include "termbox2.h"

#define MAX_FIELD_LEN 100
#define ID_WIDTH 7
#define USERNAME_WIDTH 30
#define DESC_WIDTH 51
#define TABLE_WIDTH (ID_WIDTH + USERNAME_WIDTH + DESC_WIDTH + 2)
#define QUEUE_MAX 10
#define DELETED (-1)

#define COLOR_HEADER (TB_BLUE | TB_BOLD)
#define COLOR_SELECTED (TB_REVERSE)
#define COLOR_PAGINATION (TB_GREEN | TB_BOLD)
#define COLOR_STATUS (TB_RED | TB_BOLD)
#define COLOR_SEARCH (TB_YELLOW | TB_BOLD)

#define SEARCH_TXT_MAX 32
#define EMPTY_QUEUE (-2)
#define QUEUE_ERR (-2)
#define HELP_WIN_WIDTH (TABLE_WIDTH / 2)
#define LEN(arr) (sizeof(arr) / sizeof((arr)[0]))
#define draw_table(records, search_queue, search_parttern, ...) \
  _draw_table((records), (search_queue), (search_parttern), (table_t){.width = TABLE_WIDTH, .start_y = 1, __VA_ARGS__})

typedef struct {
  int64_t id;
  char username[USERNAME_MAX_LEN + 1];
  char description[MAX_FIELD_LEN + 1];
} record_t;

typedef struct {
  int size;
  int capacity;
  record_t *data;
} record_array_t;

typedef struct {
  int start_x;
  int start_y;
  int width;
  int height;
  int64_t cursor;
} table_t;

typedef struct {
  int64_t *data;
  int size;
  int head;
  int tail;
  int capacity;
} queue_t;

char *get_input(const char *prompt, char *input, const int text_len, int cod_y, int cod_x);
char *get_secret(const char *prompt);

int pipeline(void *data, int argc, char **argv, char **column_name);
int main_tui(sqlite3 *db);

void draw_art(void);
void draw_border(int start_x, int start_y, int width, int height, uintattr_t fg, uintattr_t bg);
void _draw_table(record_array_t *records, queue_t *search_queue, char *search_parttern, table_t table);
void draw_update_menu(int option, int start_x, int start_y);
void draw_table_border(int start_x, int start_y, int table_h);

bool do_updates(sqlite3 *db, record_array_t *records, int64_t current_position);

void display_help(void);
void display_notifctn(char *message);
void display_secret(sqlite3 *db, uint16_t id);

char *get_search_parttern(void);
bool select_next(queue_t *queue);

int64_t dequeue(queue_t *queue);
bool enqueue(queue_t *queue, int64_t id);
bool queue_is_full(queue_t *queue);
bool queue_is_empty(queue_t *queue);
void free_queue(queue_t *queue);

void add_record(record_array_t *arr, record_t rec);
void free_records(record_array_t *arr);

bool init_tui(void);
void cleanup_tui(void);

#endif  // !TUI_H
