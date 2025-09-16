#include "../../include/tui.h"

#include <locale.h>
#include <stdint.h>

#include "../../include/database.h"

static record_array_t records = {0, 0, NULL};
static char search_pattern[MAX_FIELD_LEN] = {0};
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

  tb_set_input_mode(TB_INPUT_ESC);
  setlocale(LC_ALL, "");
  return true;
}

void cleanup_tui(void) { tb_shutdown(); }

static bool notify_deleted(int64_t id) {
  if (id == DELETED) {
    display_notifctn("Note: this secret has been deleted");
    return true;
  }
  return false;
}

int main_tui(sqlite3 *db) {
  records.data = NULL;
  records.size = 0;
  records.capacity = 0;
  current_page = 0;
  records_per_page = 30;
  struct tb_event ev = {0};

  int term_width;
  int term_height;
  init_tui();
  if (!load_records(db, &records)) {
    cleanup_tui();
    fprintf(stderr, "Error: Failed to load data from database\n");
    return 0;
  }

  while (1) {
    term_width = tb_width();
    term_height = tb_height();
    records_per_page = term_height - 6;
    if (records_per_page < 1) records_per_page = 1;

    current_page = current_position / records_per_page;

    draw_table(current_page, records_per_page, &records, .start_x = (term_width - TABLE_WIDTH) / 2,
               .height = term_height - 4, .cursor = current_position);

    int ret = tb_poll_event(&ev);
    if (ret != TB_OK) continue;

    if (ev.type == TB_EVENT_KEY) {
      if (ev.key == TB_KEY_ESC || ev.ch == 'q' || ev.ch == 'Q') {
        break;
      } else if (ev.ch == 'k' || ev.key == TB_KEY_ARROW_UP) {
        if (current_position > 0) current_position--;
      } else if (ev.ch == 'j' || ev.key == TB_KEY_ARROW_DOWN) {
        if (current_position < records.size - 1) current_position++;
      } else if (ev.ch == 'h' || ev.key == TB_KEY_ARROW_LEFT) {
        current_position = (current_page - 1) * records_per_page;
        if (current_position < 0) current_position = 0;
      } else if (ev.ch == 'l' || ev.key == TB_KEY_ARROW_RIGHT) {
        current_position = (current_page + 1) * records_per_page;
        if (current_position >= records.size) current_position = records.size - 1;
      } else if (ev.ch == 'g' || ev.key == TB_KEY_HOME) {
        current_position = 0;
      } else if (ev.ch == 'G' || ev.key == TB_KEY_END) {
        current_position = records.size - 1;
      } else if (ev.ch == '/') {
        /* TODO: handle search*/
        continue;
      } else if (ev.ch == 'n') {
        continue;
      } else if (ev.ch == '?') {
        /* TODO: show help  */
        display_help();
        continue;

      } else if (ev.ch == 'd') {
        if (notify_deleted(records.data[current_position].id)) continue;
        if (!delete_record(db, records.data[current_position].id)) {
          display_notifctn("Error: Failed to delete secret");
          continue;
        }

        display_notifctn("Note: secret deleted");
        records.data[current_position].id = DELETED;

      } else if (ev.ch == 'u') {
        if (notify_deleted(records.data[current_position].id)) continue;
        if (!do_updates(db, &records, current_position)) continue;
        display_notifctn("Note: record updated");

      } else if (ev.key == TB_KEY_ENTER) {
        if (notify_deleted(records.data[current_position].id)) continue;
        display_secret(db, records.data[current_position].id);
      }
    } else if (ev.type == TB_EVENT_RESIZE) {
      term_width = ev.w;
      term_height = ev.h;
      records_per_page = term_height - 6;
      if (records_per_page < 1) records_per_page = 1;
    }
  }

  cleanup_tui();
  free_records(&records);
  return 1;
}
