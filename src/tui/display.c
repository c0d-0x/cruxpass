#include "tui.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "database.h"

void display_notifctn(char *message) {
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
    tb_peek_event(&ev, 2000);
    raise(SIGWINCH);
}

void display_secret(sqlite3 *db, uint16_t id) {
    const unsigned char *secret = fetch_secret(db, id);
    if (secret == NULL) {
        display_notifctn("Error: failed to fetch secret");
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
    tb_print(start_x + 2, start_y, COLOR_HEADER, TB_DEFAULT, " Secret ");
    tb_print(start_x + 2, start_y + 1, TB_DEFAULT | TB_BOLD, TB_DEFAULT, (char *) secret);
    tb_present();

    struct tb_event ev;
    while (1) {
        if (tb_poll_event(&ev) != TB_OK
            || (ev.type == TB_EVENT_KEY && (ev.key == TB_KEY_ENTER || ev.ch == 'q' || ev.ch == 'Q')))
            break;
    }

    free((void *) secret);
}

void display_help(void) {
    int help_w = 50;
    int help_h = 15;

    int term_w = tb_width();
    int term_h = tb_height();

    if (term_w < 60 + 4 || term_h < 18 + 2) {
        display_notifctn("Warning: Term width too small");
        return;
    }

    int start_x = (term_w - (help_w)) / 2;
    int start_y = (term_h - help_h) / 2;

    tb_clear();
    draw_border(start_x, start_y, help_w + 4, help_h + 2, COLOR_PAGINATION, TB_DEFAULT);
    tb_print(start_x + 2, start_y, COLOR_HEADER, TB_DEFAULT, " Help ");

    int line = start_y + 2;
    tb_print(start_x + 2, line++, TB_DEFAULT | TB_BOLD, TB_DEFAULT, "Actions:");
    tb_print(start_x + 2, line++, TB_DEFAULT, TB_DEFAULT, " Enter - View secret      u - Update record");
    tb_print(start_x + 2, line++, TB_DEFAULT, TB_DEFAULT, " d - Delete record        ");
    tb_print(start_x + 2, line++, TB_DEFAULT, TB_DEFAULT, " / - Search               n - Next search result");
    tb_print(start_x + 2, line++, TB_DEFAULT, TB_DEFAULT, " ? - Show this help       q/Q - Quit");

    line++;
    tb_print(start_x + 2, line++, TB_DEFAULT | TB_BOLD, TB_DEFAULT, "Navigation:");
    tb_print(start_x + 2, line++, TB_DEFAULT, TB_DEFAULT, " j/k - Down/Up            h/l - Page left/right");
    tb_print(start_x + 2, line++, TB_DEFAULT, TB_DEFAULT, " g/G - First/Last");

    line++;
    tb_print(start_x + 2, line++, TB_DEFAULT, TB_DEFAULT, "Press any key to close...");

    tb_present();
    struct tb_event ev;
    tb_poll_event(&ev);
}
