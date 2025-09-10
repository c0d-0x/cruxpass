#include "../../include/tui.h"

#include <locale.h>

#include "../../include/database.h"

static record_array_t records = {0, 0, NULL};
static int term_height, term_width;
static char search_pattern[MAX_FIELD_LEN] = {0};
static int search_active = 0;
static int64_t current_position = 1;
static int current_page = 0;
static int records_per_page = 10;

/**
 * Displays centered ASCII art (4 bytes wide char)
 */

#define LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

bool init_tui(void) {
  if (tb_init() != TB_OK) {
    fprintf(stderr, "Error: Failed to initialize TUS\n");
    return false;
  }

  term_height = tb_height();
  term_width = tb_width();
  tb_set_input_mode(TB_INPUT_ESC | TB_INPUT_MOUSE);
  setlocale(LC_ALL, "");
  return true;
}

void cleanup_tui(void) { tb_shutdown(); }

static void show_notifctn(char *message) {
  int term_h = tb_height();
  int term_w = tb_width();
  int message_len = strlen(message) + 4;
  if (term_w <= message_len) {
    tb_print(term_w / 2, term_h / 2, COLOR_STATUS, TB_DEFAULT, "Terminal width too small");
    tb_present();
    return;
  }

  int start_x = term_w - (message_len + 4);
  int start_y = 2;

  tb_print(start_x + 2, start_y + 2, COLOR_STATUS, TB_DEFAULT, message);
  draw_border(start_x, start_y, message_len + 2, 5, COLOR_STATUS, TB_DEFAULT);
  struct tb_event ev;
  tb_peek_event(&ev, 3000);
}

int main_tui(sqlite3 *db) {
  records.data = NULL;
  records.size = 0;
  records.capacity = 0;
  current_page = 0;
  records_per_page = 30;

  if (!load_records(db, &records)) {
    cleanup_tui();
    fprintf(stderr, "Error: Failed to load data from database\n");
    return 0;
  }

  int start_y = 2;
  int start_x = (term_width - TABLE_WIDTH) / 2;
  if (start_x < 0) start_x = 0;

  tb_clear();
  term_height = tb_height();
  term_width = tb_width();
  show_notifctn("Hello, c0d_0x");
  table_t table = {
      .start_x = start_x, .start_y = start_y, .width = TABLE_WIDTH, .height = term_height - 4, .cursor = 2};
  draw_table(&table, current_page, records_per_page, &records);
  tb_present();
  struct tb_event ev;
  tb_poll_event(&ev);
  free_records(&records);
  return 1;
}
