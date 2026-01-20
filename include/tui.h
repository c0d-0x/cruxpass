#ifndef TUI_H
#define TUI_H

#include <sodium/utils.h>
#include <stdbool.h>
#include <stdint.h>

#include "cruxpass.h"
#include "termbox2.h"

#define ID_WIDTH 8
#define USERNAME_WIDTH USERNAME_MAX_LEN
#define DESC_WIDTH 48
#define TABLE_WIDTH (ID_WIDTH + USERNAME_WIDTH + DESC_WIDTH + 3)
#define QUEUE_MAX 10
#define DELETED (-1)

#define COLOR_HEADER (TB_BLUE | TB_BOLD)
#define COLOR_SELECTED (TB_REVERSE)
#define COLOR_PAGINATION (TB_GREEN | TB_BOLD)
#define COLOR_STATUS (TB_RED | TB_BOLD)
#define COLOR_SEARCH (TB_YELLOW | TB_BOLD)

#define SEARCH_TXT_MAX 32
#define MIN_WIN_WIDTH 32
#define QUEUE_ERR (-2)
#define DIGIT_COUNT_MAX 8
#define HELP_WIN_WIDTH (TABLE_WIDTH / 2)

#define BORDER_H 0x2500             // ─
#define BORDER_V 0x2502             // │
#define BORDER_TOP_LEFT 0x256D      // ╭
#define BORDER_TOP_RIGHT 0x256E     // ╮
#define BORDER_BOTTOM_LEFT 0x2570   // ╰
#define BORDER_BOTTOM_RIGHT 0x256F  // ╯

#define LEN(arr) (sizeof(arr) / sizeof((arr)[0]))
#define draw_table(records, search_queue, search_parttern, ...) \
    _draw_table((records), (search_queue), (search_parttern),   \
                (table_t) {.width = TABLE_WIDTH, .start_y = 1, __VA_ARGS__})

typedef struct {
    int64_t id;
    char username[USERNAME_MAX_LEN + 1];
    char description[DESC_MAX_LEN + 1];
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
    int size;
    int head;
    int tail;
    int capacity;
    int64_t *data;
} queue_t;

bool tui_init(void);
void tui_cleanup(void);
int tui_main(sqlite3 *db);
int tui_pipeline(void *data, int argc, char **argv, char **column_name);

int get_long(char *prompt);
char *get_search_parttern(void);
char *get_secret(const char *prompt);
void get_random_secret(sqlite3 *db, bank_options_t opt);
char *get_input(const char *prompt, char *input, const int text_len, int cod_y, int cod_x);

void draw_art(void);
void draw_border(int start_x, int start_y, int width, int height, uintattr_t fg, uintattr_t bg);
void _draw_table(record_array_t *records, queue_t *search_queue, char *search_parttern, table_t table);
void draw_update_menu(int option, int start_x, int start_y);
void draw_table_border(int start_x, int start_y, int table_h);

bool do_updates(sqlite3 *db, record_array_t *records, int64_t current_position);

void display_help(void);
void display_desc(char *description);
void display_notifctn(char *message);
void display_ran_secret(sqlite3 *db, const char *secret);
void display_secret(sqlite3 *db, uint16_t id);

bool select_next(queue_t *queue);

int64_t dequeue(queue_t *queue);
bool enqueue(queue_t *queue, int64_t id);
bool queue_is_full(queue_t *queue);
bool queue_is_empty(queue_t *queue);
void free_queue(queue_t *queue);

bool add_record(record_array_t *arr, record_t rec);
void free_records(record_array_t *arr);

#endif  // !TUI_H
