
#include <stdint.h>
#include <wchar.h>

#include "../../include/tui.h"

extern int current_page;
extern int records_per_page;
extern int total_pages;

/**
 * Displays centered ASCII art (4 bytes wide char)
 */
void draw_art(void) {
  const wchar_t *ascii_art[] = {L" ▄████▄   █    ██ ▒██   ██▒ ██▓███   ▄▄▄        ██████   ██████    ",
                                L"▒██▀ ▀█   ██  ▓██▒▒▒ █ █ ▒░▓██░  ██▒▒████▄    ▒██    ▒ ▒██    ▒    ",
                                L"▒▓█    ▄ ▓██  ▒██░░░  █   ░▓██░ ██▓▒▒██  ▀█▄  ░ ▓██▄   ░ ▓██▄      ",
                                L"▒▓▓▄ ▄██▒▓▓█  ░██░ ░ █ █ ▒ ▒██▄█▓▒ ▒░██▄▄▄▄██   ▒   ██▒  ▒   ██▒   ",
                                L"▒ ▓███▀ ░▒▒█████▓ ▒██▒ ▒██▒▒██▒ ░  ░ ▓█   ▓██▒▒██████▒▒▒██████▒▒   ",
                                L"░ ░▒ ▒  ░░▒▓▒ ▒ ▒ ▒▒ ░ ░▓ ░▒▓▒░ ░  ░ ▒▒   ▓▒█░▒ ▒▓▒ ▒ ░▒ ▒▓▒ ▒ ░   ",
                                L"  ░  ▒   ░░▒░ ░ ░ ░░   ░▒ ░░▒ ░       ▒   ▒▒ ░░ ░▒  ░ ░░ ░▒  ░ ░   ",
                                L"░         ░░░ ░ ░  ░    ░  ░░         ░   ▒   ░  ░  ░  ░  ░  ░     ",
                                L"░ ░         ░      ░    ░                 ░  ░      ░        ░     ",
                                L"░                                                                  "};
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
      tb_set_cell(coord_x++, coord_y, ascii_art[i][j], TB_WHITE, TB_DEFAULT);
    }
    coord_x -= art_width;
    coord_y++;
  }

  tb_present();
}

void draw_border(int start_x, int start_y, int width, int height, uintattr_t fg, uintattr_t bg) {
  /* top and bottom borders */
  for (int i = 1; i < width - 1; i++) {
    tb_set_cell(start_x + i, start_y, 0x2500, fg, bg);               // ─
    tb_set_cell(start_x + i, start_y + height - 1, 0x2500, fg, bg);  // ─
  }

  /* left and right borders */
  for (int i = 1; i < height - 1; i++) {
    tb_set_cell(start_x, start_y + i, 0x2502, fg, bg);              // │
    tb_set_cell(start_x + width - 1, start_y + i, 0x2502, fg, bg);  // │
  }

  /* Rounded corners */
  tb_set_cell(start_x, start_y, 0x256D, fg, bg);                           // ╭
  tb_set_cell(start_x + width - 1, start_y, 0x256E, fg, bg);               // ╮
  tb_set_cell(start_x, start_y + height - 1, 0x2570, fg, bg);              // ╰
  tb_set_cell(start_x + width - 1, start_y + height - 1, 0x256F, fg, bg);  // ╯
  tb_present();
}

static void draw_status(int start_y, int cursor) {
  int width = 40;
  int start_x = (tb_width() - width) / 2;

  for (int i = 1; i < width - 1; i++) {
    tb_set_cell(start_x + i, start_y, 0x2500, COLOR_PAGINATION, TB_DEFAULT);
  }

  tb_set_cell(start_x, start_y + 1, 0x2502, COLOR_PAGINATION, TB_DEFAULT);
  tb_set_cell(start_x + width - 1, start_y + 1, 0x2502, COLOR_PAGINATION, TB_DEFAULT);

  tb_set_cell(start_x, start_y, 0x256D, COLOR_PAGINATION, TB_DEFAULT);
  tb_set_cell(start_x + width - 1, start_y, 0x256E, COLOR_PAGINATION, TB_DEFAULT);
  tb_printf(start_x + 3, start_y + 1, COLOR_HEADER, TB_DEFAULT, "Page %d of %02d │ Record %d of %d", current_page + 1,
            total_pages + 1, cursor + 1, (records_per_page * total_pages + 1) + 1);
}

void _draw_table(record_array_t *records, queue_t *search_queue, char *search_parttern, table_t table) {
  /* TODO: redraw the table border and headings once term term resizes */

  /* tb_clear(); */
  /* draw_border(table.start_x, table.start_y, table.width + 2, table.height + 2, COLOR_PAGINATION, TB_DEFAULT); */

  /* tb_printf(table.start_x + 1, table.start_y + 1, COLOR_HEADER, TB_DEFAULT, "%-*s %-*s %-*s", ID_WIDTH, "ID", */
  /* USERNAME_WIDTH, "USERNAME", DESC_WIDTH, "DESCRIPTION"); */

  /* for (int i = 0; i < TABLE_WIDTH; i++) { */
  /*   tb_set_cell(table.start_x + i + 1, table.start_y + 2, 0x2500, COLOR_PAGINATION, TB_DEFAULT);  // ─ */
  /* } */

  total_pages = records->size / records_per_page;

  int64_t start_index = current_page * records_per_page;
  int64_t end_index = start_index + records_per_page;
  if (end_index > records->size) end_index = records->size;

  record_t *rec = NULL;
  int64_t row = table.start_y + 4;
  int start_x = table.start_x + 1;
  uintattr_t fg;
  uintattr_t bg;

  /* cleanup cells before redrawing rows */
  for (int i = 1; i < table.height - 1; i++) {
    for (int j = 0; j < TABLE_WIDTH; j++) {
      tb_set_cell(table.start_x + j + 1, table.start_y + i + 2, ' ', COLOR_PAGINATION, TB_DEFAULT);  // ─
    }
  }

  if (search_parttern != NULL) {
    /*NOTE: perform a search and free the search_parttern */
    for (int64_t i = 0; i < records->size; i++) {
      if (strstr(records->data[i].username, search_parttern) != NULL ||
          strstr(records->data[i].description, search_parttern) != NULL) {
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
      tb_printf(start_x, row, COLOR_STATUS, TB_WHITE, "%-*s %-*s %-*.*s", ID_WIDTH, "DELETED", USERNAME_WIDTH,
                rec->username, DESC_WIDTH, DESC_WIDTH, rec->description);
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

    tb_printf(start_x, row, fg, bg, "%-*ld %-*s %-*.*s", ID_WIDTH, rec->id, USERNAME_WIDTH, rec->username, DESC_WIDTH,
              DESC_WIDTH, rec->description);
  }

  draw_status(table.height, table.cursor);
  tb_present();
}
