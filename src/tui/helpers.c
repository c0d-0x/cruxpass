#include "cruxpass.h"
#include "database.h"
#include "tui.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

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
        if (tb_poll_event(&ev) != TB_OK || ev.type != TB_EVENT_KEY || ev.ch == 'q' || ev.ch == 'Q'
            || ev.key == TB_KEY_ESC) {
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

    int8_t flag = 0;
    secret_t rec = {0};

    tb_clear();
    switch (option) {
        case 0:
            get_input("> username: ", rec.username, USERNAME_MAX_LEN, start_x + 4, start_y);
            if (strlen(rec.username) == 0) return false;
            flag = UPDATE_USERNAME;
            break;
        case 1:
            get_input("> description: ", rec.description, DESC_MAX_LEN, start_x + 4, start_y);
            if (strlen(rec.description) == 0) return false;
            flag = UPDATE_DESCRIPTION;
            break;
        case 2:
            get_input("> secret: ", rec.secret, SECRET_MAX_LEN, start_x + 4, start_y);
            if (strlen(rec.secret) < 8) return false;
            flag = UPDATE_SECRET;
            break;
        case 3:
            get_input("> username: ", rec.username, USERNAME_MAX_LEN, start_x + 4, start_y++);
            get_input("> secret: ", rec.secret, SECRET_MAX_LEN, start_x + 4, start_y++);
            get_input("> description: ", rec.description, DESC_MAX_LEN, start_x + 4, start_y++);
            if (strlen(rec.username) == 0 || strlen(rec.description) == 0 || strlen(rec.secret) < 8) return false;
            flag = UPDATE_USERNAME | UPDATE_DESCRIPTION | UPDATE_SECRET;
            break;
        default: return false;
    }

    if (flag & UPDATE_DESCRIPTION) memcpy(records->data[current_position].description, rec.description, DESC_MAX_LEN);
    if (flag & UPDATE_USERNAME) memcpy(records->data[current_position].username, rec.username, USERNAME_MAX_LEN);

    if (!update_record(db, &rec, id, flag)) {
        display_notifctn("Error: Failed to update record");
        return false;
    }

    return true;
}
