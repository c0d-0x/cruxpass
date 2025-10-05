#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "../../include/database.h"
#include "../../include/tui.h"

static int updates_menu(void) {
  int option = 0;
  tb_clear();
  int term_h = tb_height();
  int term_w = tb_width();

  int start_x = (term_w / 2) - 8;
  if (start_x < 0) start_x = 0;
  int start_y = (term_h / 2) - 5;
  if (start_y < 0) start_y = 0;

  struct tb_event ev;

  draw_update_menu(option, start_x, start_y);
  while (true) {
    if (tb_poll_event(&ev) != TB_OK || ev.type != TB_EVENT_KEY || ev.ch == 'q' || ev.ch == 'Q') {
      return (-1);
    }

    start_x = (term_w / 2) - 8;
    if (start_x < 0) start_x = 0;
    start_y = (term_h / 2) - 5;
    if (start_y < 0) start_y = 0;

    if (ev.type == TB_EVENT_KEY) {
      if (ev.key == TB_KEY_ENTER) {
        break;
      } else if (ev.ch == 'k' || ev.key == TB_KEY_ARROW_UP) {
        if (option > 0) option--;
      } else if (ev.ch == 'j' || ev.key == TB_KEY_ARROW_DOWN) {
        if (option < 3) option++;
      }
      draw_update_menu(option, start_x, start_y);
    }
  }

  return option;
}

bool do_updates(sqlite3 *db, record_array_t *records, int64_t current_position) {
  int64_t id = records->data[current_position].id;

  int start_x = 0;
  int start_y = 1;

  int option = updates_menu();
  if (option < 0) return false;

  int8_t flags = 0;
  secret_t rec = {0};

  tb_clear();
  if (option == 0) {
    get_input("> username: ", rec.username, USERNAME_MAX_LEN, start_y, start_x + 4);
    flags = UPDATE_USERNAME;
    if (strlen(rec.username) == 0) return false;
    memcpy(records->data[current_position].username, rec.username, USERNAME_MAX_LEN);
  } else if (option == 1) {
    get_input("> description: ", rec.description, DESC_MAX_LEN, start_y, start_x + 4);
    flags = UPDATE_DESCRIPTION;
    if (strlen(rec.description) == 0) return false;
    memcpy(records->data[current_position].description, rec.description, DESC_MAX_LEN);
  } else if (option == 2) {
    get_input("> secret: ", rec.secret, SECRET_MAX_LEN, start_y, start_x + 4);
    if (strlen(rec.secret) < 8) return false;
    flags = UPDATE_SECRET;
  } else if (option == 3) {
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
