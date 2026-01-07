#include "cruxpass.h"
#include "database.h"
#include "tui.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

void display_notifctn(char *message) {
    int term_w = tb_width();
    int message_len = strlen(message) + 4;
    if (term_w <= message_len) {
        tb_clear();
        tb_print(2, 2 / 2, COLOR_STATUS, TB_DEFAULT, "Error: Terminal width too small");
        tb_present();

        struct tb_event ev = {0};
        tb_poll_event(&ev);
        return;
    }

    int start_x = term_w - (message_len + 3);
    int start_y = 1;

    tb_print(start_x + 2, start_y + 2, COLOR_STATUS, TB_DEFAULT, message);
    draw_border(start_x, start_y, message_len + 2, 5, COLOR_STATUS, TB_DEFAULT);
    struct tb_event ev;
    tb_peek_event(&ev, 2000);
    raise(SIGWINCH);
}

void display_secret(sqlite3 *db, uint16_t id) {
    const unsigned char *secret = fetch_secret(db, id);
    if (secret == NULL) {
        display_notifctn("Error: Failed to fetch secret");
        return;
    }

    int secret_len = strlen((char *) secret);

    int term_w = tb_width();
    int term_h = tb_height();

    int start_x = (term_w - (secret_len + 2)) / 2;
    int start_y = (term_h - 4) / 2;
    if (start_x <= 0 || start_y <= 0) {
        display_notifctn("Warning: Term width or height too small");
        return;
    }

    tb_clear();
    draw_border(start_x, start_y, secret_len + 4, 3, COLOR_PAGINATION, TB_DEFAULT);
    tb_print(start_x + 2, start_y, COLOR_HEADER, TB_DEFAULT, "| Secret |");
    tb_print(start_x + 2, start_y + 1, TB_DEFAULT | TB_BOLD, TB_DEFAULT, (char *) secret);
    tb_present();

    struct tb_event ev;
    while (tb_poll_event(&ev) == TB_OK) {
        if (ev.type == TB_EVENT_KEY && (ev.key == TB_KEY_ENTER || ev.ch == 'q' || ev.ch == 'Q')) break;
    }

    free((void *) secret);
}

void display_help(void) {
    int help_win_w = 50;
    int help_win_h = 15;

    int term_w = tb_width();
    int term_h = tb_height();

    if (term_w < 60 + 4 || term_h < 18 + 2) {
        display_notifctn("Warning: Term width or height too small");
        return;
    }

    int start_x = (term_w - (help_win_w)) / 2;
    int start_y = (term_h - help_win_h) / 2;

    tb_clear();
    draw_border(start_x, start_y, help_win_w + 4, help_win_h + 2, COLOR_PAGINATION, TB_DEFAULT);
    tb_print(start_x + 2, start_y, COLOR_HEADER, TB_DEFAULT, "| Help |");

    int line = start_y + 2;
    tb_print(start_x + 2, line++, TB_DEFAULT | TB_BOLD, TB_DEFAULT, "Actions:");
    tb_print(start_x + 2, line++, TB_DEFAULT, TB_DEFAULT, " Enter - View secret      u - Update record");
    tb_print(start_x + 2, line++, TB_DEFAULT, TB_DEFAULT, " d - Delete record        n - Next search result");
    tb_print(start_x + 2, line++, TB_DEFAULT, TB_DEFAULT, " / - Search               L - Show description");
    tb_print(start_x + 2, line++, TB_DEFAULT, TB_DEFAULT, " ? - Show this help       q/Q - Quit");
    tb_print(start_x + 2, line++, TB_DEFAULT, TB_DEFAULT, " Ctrl+r - Reload tui");
    tb_print(start_x + 2, line++, TB_DEFAULT, TB_DEFAULT, " r - a/A/p/r/x Regenerate secret");

    line++;
    tb_print(start_x + 2, line++, TB_DEFAULT | TB_BOLD, TB_DEFAULT, "Navigation:");
    tb_print(start_x + 2, line++, TB_DEFAULT, TB_DEFAULT, " j/k - Down/Up");
    tb_print(start_x + 2, line++, TB_DEFAULT, TB_DEFAULT, " g/G - First/Last");
    tb_print(start_x + 2, line++, TB_DEFAULT, TB_DEFAULT, " h/l - Page left/right");

    line++;
    tb_print(start_x + 2, line, TB_DEFAULT, TB_DEFAULT, "Press any key to close...");

    tb_present();
    struct tb_event ev;
    tb_poll_event(&ev);
}

void display_desc(char *description) {
    if (description == NULL) {
        display_notifctn("Warning: Record has no desc");
        return;
    }

    int desc_win_w = 64;
    int desc_win_h = 8;
    bool wrapped = false;

    int term_w = tb_width();
    int term_h = tb_height();
    int desc_len = strlen(description);

    if (desc_len <= DESC_WIDTH) {
        display_notifctn("Info: Record fits on screen");
        return;
    }

    if (desc_len > desc_win_w) wrapped = true;
    if (term_w < 60 + 4 || term_h < 18 + 2) {
        display_notifctn("Warning: Term width or height too small");
        return;
    }

    int start_x = (term_w - (desc_win_w)) / 2;
    int start_y = (term_h - desc_win_h) / 2;

    tb_clear();
    draw_border(start_x, start_y, desc_win_w + 4, desc_win_h + 2, COLOR_PAGINATION, TB_DEFAULT);
    tb_print(start_x + 2, start_y, COLOR_HEADER, TB_DEFAULT, "| Description |");

    int line = start_y + 2;
    char *tmp = description;

    do {
        tb_printf(start_x + 2, line++, TB_DEFAULT | TB_BOLD, TB_DEFAULT, "%-*.*s", desc_win_w, desc_win_w, tmp);
        tmp += desc_win_w;
        if (tmp - description >= desc_len) break;
    } while (wrapped);

    line++;
    tb_print(start_x + 2, line, TB_DEFAULT, TB_DEFAULT, "Press any key to close...");

    tb_present();
    struct tb_event ev;
    tb_poll_event(&ev);
}

// TODO: save and generate new secrets from TUI
void display_ran_secret(sqlite3 *db, const char *secret_str) {
    int sec_len = strlen((char *) secret_str);
    int sec_win_w = (sec_len + 2 < (MIN_WIN_WIDTH + 2)) ? MIN_WIN_WIDTH : sec_len + 2;
    int sec_win_h = 4;

    int term_w = tb_width();
    int term_h = tb_height();

    if (term_w < sec_win_w) {
        display_notifctn("Warning: Term width too small");
        return;
    }

    int start_x = (term_w - (sec_win_w)) / 2;
    int start_y = (term_h - sec_win_h) / 2;

    tb_clear();
    draw_border(start_x, start_y, sec_win_w + 2, sec_win_h + 2, COLOR_PAGINATION, TB_DEFAULT);
    tb_print(start_x + 2, start_y, COLOR_HEADER, TB_DEFAULT, "| Random Secret |");

    int line = start_y + 2;
    tb_printf(start_x + 2, line++, TB_DEFAULT | TB_BOLD, TB_DEFAULT, "%-*.*s", sec_len, sec_len, secret_str);
    line++;
    tb_print(start_x + 2, line, TB_DEFAULT, TB_DEFAULT, "Press s to save or q to close");

    tb_present();
    struct tb_event ev;
    while (tb_poll_event(&ev) == TB_OK) {
        if (ev.type == TB_EVENT_KEY && (ev.key == TB_KEY_ESC || ev.ch == 'q' || ev.ch == 'Q')) break;
        if (ev.ch == 's' || ev.ch == 'S') {
            start_y = 1;
            start_x = 2;
            secret_t rec = {0};

            tb_clear();
            get_input("> username: ", rec.username, USERNAME_MAX_LEN, start_x + 4, start_y++);
            get_input("> description: ", rec.description, DESC_MAX_LEN, start_x + 4, start_y++);

            if (strlen(rec.username) == 0 || strlen(rec.description) == 0) return;
            memcpy(rec.secret, secret_str, SECRET_MAX_LEN);
            if (!insert_record(db, &rec)) {
                display_notifctn("Error: Failed to saved secret");
                return;
            };

            display_notifctn("Info: secret saved");
            break;
        }
    }
}
