#include "../../include/tui.h"

#include "../../include/database.h"
#include "../../include/termbox.h"

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

#define LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

bool init_tui(void) {
  if (tb_init() != TB_OK) {
    fprintf(stderr, "Error: Failed to initialize TUS\n");
    return false;
  }

  term_height = tb_height();
  term_width = tb_width();
  tb_set_input_mode(TB_INPUT_ESC | TB_INPUT_MOUSE);
  return true;
}

void cleanup_tui(void) {
  tb_shutdown();
  free_records(&records);
}

static void draw_box(int coord_x, int coord_y, int width, int height, uintattr_t fg, uintattr_t bg) {
  /* top and bottom borders */
  for (int i = 1; i < width - 1; i++) {
    tb_set_cell(coord_x + i, coord_y, 0x2500, fg, bg);               // ─
    tb_set_cell(coord_x + i, coord_y + height - 1, 0x2500, fg, bg);  // ─
  }

  /* left and right borders */
  for (int i = 1; i < height - 1; i++) {
    tb_set_cell(coord_x, coord_y + i, 0x2502, fg, bg);              // │
    tb_set_cell(coord_x + width - 1, coord_y + i, 0x2502, fg, bg);  // │
  }

  /* Rounded corners */
  tb_set_cell(coord_x, coord_y, 0x256D, fg, bg);                           // ╭
  tb_set_cell(coord_x + width - 1, coord_y, 0x256E, fg, bg);               // ╮
  tb_set_cell(coord_x, coord_y + height - 1, 0x2570, fg, bg);              // ╰
  tb_set_cell(coord_x + width - 1, coord_y + height - 1, 0x256F, fg, bg);  // ╯
}

void show_notifctn(const char *fmt, ...) {}

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

  cleanup_tui();
  return 1;
}
