#include "tui.h"

#include <stdint.h>
#include <wchar.h>

extern int current_page;
extern int records_per_page;
extern int total_pages;

/**
 * Displays centered ASCII art (4 bytes wide char)
 */
void draw_art(void) {
    // Font Type: Bloody
    const wchar_t *ascii_art[] = {L" ▄████▄   ██▀███   █    ██ ▒██   ██▒ ██▓███   ▄▄▄        ██████   ██████ ",
                                  L"▒██▀ ▀█  ▓██ ▒ ██▒ ██  ▓██▒▒▒ █ █ ▒░▓██░  ██▒▒████▄    ▒██    ▒ ▒██    ▒ ",
                                  L"▒▓█    ▄ ▓██ ░▄█ ▒▓██  ▒██░░░  █   ░▓██░ ██▓▒▒██  ▀█▄  ░ ▓██▄   ░ ▓██▄   ",
                                  L"▒▓▓▄ ▄██▒▒██▀▀█▄  ▓▓█  ░██░ ░ █ █ ▒ ▒██▄█▓▒ ▒░██▄▄▄▄██   ▒   ██▒  ▒   ██▒",
                                  L"▒ ▓███▀ ░░██▓ ▒██▒▒▒█████▓ ▒██▒ ▒██▒▒██▒ ░  ░ ▓█   ▓██▒▒██████▒▒▒██████▒▒",
                                  L"░ ░▒ ▒  ░░ ▒▓ ░▒▓░░▒▓▒ ▒ ▒ ▒▒ ░ ░▓ ░▒▓▒░ ░  ░ ▒▒   ▓▒█░▒ ▒▓▒ ▒ ░▒ ▒▓▒ ▒ ░",
                                  L"  ░  ▒     ░▒ ░ ▒░░░▒░ ░ ░ ░░   ░▒ ░░▒ ░       ▒   ▒▒ ░░ ░▒  ░ ░░ ░▒  ░ ░",
                                  L"░          ░░   ░  ░░░ ░ ░  ░    ░  ░░         ░   ▒   ░  ░  ░  ░  ░  ░  ",
                                  L"░ ░         ░        ░      ░    ░                 ░  ░      ░        ░  ",
                                  L"░                                                                         "};
    int term_w = tb_width();
    int term_h = tb_height();

    int art_lines = LEN(ascii_art);
    int art_width = wcslen(ascii_art[0]);

    int coord_y = (term_h / 2) - (art_lines / 2) - 3;
    int coord_x = (term_w - art_width) / 2;

    if (coord_y < 0) coord_y = 0;
    if (coord_x < 0) coord_x = 0;

    tb_clear();
    for (int i = 0; i < art_lines; i++) {
        art_width = wcslen(ascii_art[i]);
        for (int j = 0; j < art_width; j++) {
            tb_set_cell(coord_x++, coord_y, ascii_art[i][j], TB_DEFAULT, TB_DEFAULT);
        }

        coord_x -= art_width;
        coord_y++;
    }

    tb_present();
}

void draw_update_menu(int option, int start_x, int start_y) {
    int option_w = 22;
    uintattr_t fg = (option == 0) ? COLOR_PAGINATION : TB_DEFAULT;
    draw_border(start_x, start_y, option_w, 3, fg, TB_DEFAULT);
    tb_print(start_x + 3, start_y + 1, fg, TB_DEFAULT, "Update Username");
    start_y += 4;

    fg = (option == 1) ? COLOR_PAGINATION : TB_DEFAULT;
    draw_border(start_x, start_y, option_w, 3, fg, TB_DEFAULT);
    tb_print(start_x + 2, start_y + 1, fg, TB_DEFAULT, "Update Description");
    start_y += 4;

    fg = (option == 2) ? COLOR_PAGINATION : TB_DEFAULT;
    draw_border(start_x, start_y, option_w, 3, fg, TB_DEFAULT);
    tb_print(start_x + 4, start_y + 1, fg, TB_DEFAULT, "Update Secrets");
    start_y += 4;

    fg = (option == 3) ? COLOR_PAGINATION : TB_DEFAULT;
    draw_border(start_x, start_y, option_w, 3, fg, TB_DEFAULT);
    tb_print(start_x + 2, start_y + 1, fg, TB_DEFAULT, "Update All Fields");
    start_y += 4;
    tb_present();
}

void draw_border(int start_x, int start_y, int width, int height, uintattr_t fg, uintattr_t bg) {
    /* top and bottom borders */
    for (int i = 1; i < width - 1; i++) {
        tb_set_cell(start_x + i, start_y, BORDER_H, fg, bg);
        tb_set_cell(start_x + i, start_y + height - 1, BORDER_H, fg, bg);
    }

    /* left and right borders */
    for (int i = 1; i < height - 1; i++) {
        tb_set_cell(start_x, start_y + i, BORDER_V, fg, bg);
        tb_set_cell(start_x + width - 1, start_y + i, BORDER_V, fg, bg);
    }

    /* Rounded corners */
    tb_set_cell(start_x, start_y, BORDER_TOP_LEFT, fg, bg);
    tb_set_cell(start_x + width - 1, start_y, BORDER_TOP_RIGHT, fg, bg);
    tb_set_cell(start_x, start_y + height - 1, BORDER_BOTTOM_LEFT, fg, bg);
    tb_set_cell(start_x + width - 1, start_y + height - 1, BORDER_BOTTOM_RIGHT, fg, bg);
    tb_present();
}

static void draw_status(int start_y, int cursor, int64_t total_records) {
    int width = 40;
    int start_x = (tb_width() - width) / 2;
    int rec_number = (total_records == 0) ? 0 : cursor + 1;
    for (int i = 1; i < width - 1; i++) {
        tb_set_cell(start_x + i, start_y, BORDER_H, COLOR_PAGINATION, TB_DEFAULT);
    }

    tb_set_cell(start_x, start_y + 1, BORDER_V, COLOR_PAGINATION, TB_DEFAULT);
    tb_set_cell(start_x + width - 1, start_y + 1, BORDER_V, COLOR_PAGINATION, TB_DEFAULT);

    tb_set_cell(start_x, start_y, BORDER_TOP_LEFT, COLOR_PAGINATION, TB_DEFAULT);
    tb_set_cell(start_x + width - 1, start_y, BORDER_TOP_RIGHT, COLOR_PAGINATION, TB_DEFAULT);
    tb_printf(start_x + 4, start_y + 1, COLOR_HEADER, TB_DEFAULT, "Page %d of %02d │ Record %d of %d", current_page + 1,
              total_pages + 1, rec_number, total_records);
}

void draw_table_border(int start_x, int start_y, int table_h) {
    tb_clear();
    draw_border(start_x, start_y, TABLE_WIDTH + 2, table_h + 2, COLOR_PAGINATION, TB_DEFAULT);
    tb_printf(start_x + 1, start_y + 1, COLOR_HEADER, TB_DEFAULT, " %-*s  %-*s %-*s", ID_WIDTH - 1, "ID",
              USERNAME_WIDTH, "USERNAME/E-MAIL", DESC_WIDTH, "DESCRIPTION");
    for (int i = 0; i < TABLE_WIDTH; i++) {
        tb_set_cell(start_x + i + 1, start_y + 2, BORDER_H, COLOR_PAGINATION, TB_DEFAULT);
    }
}

void _draw_table(record_array_t *records, queue_t *search_queue, char *search_parttern, table_t table) {
    total_pages = records->size / records_per_page;

    int64_t start_index = current_page * records_per_page;
    int64_t end_index = start_index + records_per_page;
    if (end_index > records->size) end_index = records->size;

    record_t *rec = NULL;
    int64_t row = table.start_y + 4;
    int start_x = table.start_x + 1;

    uintattr_t fg = TB_DEFAULT;
    uintattr_t bg = TB_DEFAULT;

    /* cleanup cells before redrawing rows */
    for (int i = 1; i < table.height - 1; i++) {
        for (int j = 0; j < TABLE_WIDTH; j++) {
            tb_set_cell(table.start_x + j + 1, table.start_y + i + 2, ' ', COLOR_PAGINATION, TB_DEFAULT);
        }
    }

    if (search_parttern != NULL) {
        for (int64_t i = 0; i < records->size; i++) {
            if (strstr(records->data[i].username, search_parttern) != NULL
                || strstr(records->data[i].description, search_parttern) != NULL) {
                if (!enqueue(search_queue, i)) display_notifctn("Error: Failed to enqueue record index");
            }
        }
    }

    for (int64_t i = start_index; i < end_index; i++) {
        rec = &records->data[i];
        row = 4 + (i - start_index);
        fg = TB_DEFAULT;
        bg = TB_DEFAULT;

        if (rec->id == DELETED) {
            tb_printf(start_x, row, COLOR_STATUS, TB_WHITE, " %-*s %-*s %-*.*s", ID_WIDTH, "DELETED", USERNAME_WIDTH,
                      rec->username, DESC_WIDTH, DESC_WIDTH + 1, rec->description);
            continue;
        }

        if (search_parttern != NULL) {
            if (strstr(rec->username, search_parttern) != NULL || strstr(rec->description, search_parttern) != NULL) {
                fg = TB_DEFAULT;
                bg = COLOR_SEARCH;
            }
        }

        if (i == table.cursor) {
            fg = TB_BLACK;
            bg = TB_WHITE;
        }

        tb_printf(start_x, row, fg, bg, " %-*ld %-*s %-*.*s", ID_WIDTH, rec->id, USERNAME_WIDTH, rec->username,
                  DESC_WIDTH, DESC_WIDTH + 1, rec->description);
    }

    draw_status(table.height, table.cursor, records->size);
    tb_present();
}
