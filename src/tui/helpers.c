#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "../../include/database.h"
#include "../../include/tui.h"

bool do_updates(sqlite3 *db, record_array_t *records, int64_t current_position) {
  int64_t id = records->data[current_position].id;

  tb_clear();
  int term_w = tb_width();
  int term_h = tb_height();

  int start_x = (term_w - TABLE_WIDTH) / 2 + 18;
  int start_y = (term_h / 2 - 5);
  if (start_x < 0) start_x = 0;
  if (start_y < 0) start_y = 0;

  /* TODO: A proper menu here */
  tb_print(start_x, start_y++, TB_WHITE | TB_BOLD, TB_DEFAULT, "1: update username");
  tb_print(start_x, start_y++, TB_WHITE | TB_BOLD, TB_DEFAULT, "2: update description");
  tb_print(start_x, start_y++, TB_WHITE | TB_BOLD, TB_DEFAULT, "3: update secrets");
  tb_print(start_x, start_y++, TB_WHITE | TB_BOLD, TB_DEFAULT, "4: update all three");
  tb_print(start_x + 4, start_y++, TB_WHITE | TB_BOLD, TB_DEFAULT, "> option: ");
  tb_present();

  struct tb_event ev;
  while (1) {
    int ret = tb_poll_event(&ev);
    if (ret != TB_OK || ev.type != TB_EVENT_KEY || ev.ch == 'q' || ev.ch == 'Q') {
      return false;
    }

    if (ev.ch >= 0x31 && ev.ch <= 0x34) break;
  }

  int8_t flags = 0;
  secret_t rec = {0};
  tb_print(start_x, ++start_y, TB_WHITE | TB_BOLD, TB_DEFAULT, "Enter fields: ");
  tb_present();

  start_y++;
  if (ev.ch == '1') {
    get_input("> username: ", rec.username, USERNAME_MAX_LEN, start_y, start_x + 4);
    flags = UPDATE_USERNAME;
    if (strlen(rec.username) == 0) return false;
    memcpy(records->data[current_position].username, rec.username, USERNAME_MAX_LEN);
  } else if (ev.ch == '2') {
    get_input("> description: ", rec.description, DESC_MAX_LEN, start_y, start_x + 4);
    flags = UPDATE_DESCRIPTION;
    if (strlen(rec.description) == 0) return false;
    memcpy(records->data[current_position].description, rec.description, DESC_MAX_LEN);
  } else if (ev.ch == '3') {
    get_input("> secret: ", rec.secret, SECRET_MAX_LEN, start_y, start_x + 4);
    if (strlen(rec.secret) < 8) return false;
    flags = UPDATE_SECRET;
  } else if (ev.ch == '4') {
    get_input("> username: ", rec.username, USERNAME_MAX_LEN, start_y++, start_x + 4);
    get_input("> secret: ", rec.secret, SECRET_MAX_LEN, start_y++, start_x + 4);
    get_input("> description: ", rec.description, DESC_MAX_LEN, start_y, start_x + 4);

    flags = UPDATE_USERNAME | UPDATE_DESCRIPTION | UPDATE_SECRET;

    if (strlen(rec.username) == 0 || strlen(rec.description) == 0 || strlen(rec.secret) < 8) return false;
    memcpy(records->data[current_position].username, rec.username, USERNAME_MAX_LEN);
    memcpy(records->data[current_position].description, rec.description, DESC_MAX_LEN);
  } else {
    return false;
  }

  if (!update_record(db, &rec, id, flags)) {
    display_notifctn("Error: Failed to update record");
    return false;
  }

  return true;
}
