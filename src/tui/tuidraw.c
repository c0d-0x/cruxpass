
#include "../../include/tui.h"

static volatile int64_t cursor_position = 1;
static volatile int current_page = 0;
static int records_per_page = 10;

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
  tb_clear();
  int term_w = tb_width();
  int term_h = tb_height();

  int art_lines = LEN(ascii_art);
  int art_width = wcslen(ascii_art[0]);

  int coord_y = (term_h / 2) - (art_lines / 2) - 3;
  int coord_x = (term_w - art_width) / 2;

  if (coord_y < 0) coord_y = 0;
  if (coord_x < 0) coord_x = 0;

  for (int i = 0; i < art_lines; i++) {
    const wchar_t *w_str = ascii_art[i];
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

void draw_table(table_t *table, int current_page, int rec_per_page, record_array_t *records) {
  tb_clear();
  draw_border(table->start_x, table->start_y, table->width + 2, table->height + 2, COLOR_PAGINATION, TB_DEFAULT);

  tb_printf(table->start_x + 1, table->start_y + 1, COLOR_HEADER, TB_DEFAULT, "%-*s %-*s %-*s", ID_WIDTH, "ID",
            USERNAME_WIDTH, "USERNAME", DESC_WIDTH, "DESCRIPTION");

  for (int i = 0; i < TABLE_WIDTH; i++) {
    tb_set_cell(table->start_x + i + 1, table->start_y + 2, 0x2500, COLOR_PAGINATION, TB_DEFAULT);  // ─
  }

  int64_t start_index = current_page * rec_per_page;
  int64_t end_index = start_index + rec_per_page;
  if (end_index > records->size) end_index = records->size;

  record_t *rec = NULL;
  int64_t row = table->start_y + 4;
  int start_x = table->start_x + 1;

  for (int64_t i = start_index; i < end_index; i++) {
    rec = &records->data[i];
    row = 5 + (i - start_index);

    uintattr_t fg = TB_DEFAULT;
    uintattr_t bg = TB_DEFAULT;

    if (rec->id == DELETED) {
      fg = COLOR_STATUS;
      tb_printf(start_x, row, fg, bg, "%-*s %-*s %-*.*s", ID_WIDTH, "DELETED", USERNAME_WIDTH, rec->username,
                DESC_WIDTH, DESC_WIDTH, rec->description);
      continue;
    }

    if (i == table->cursor) {
      fg = TB_BLACK;
      bg = TB_WHITE;
    }

    tb_printf(start_x, row, fg, bg, "%-*ld %-*s %-*.*s", ID_WIDTH, rec->id, USERNAME_WIDTH, rec->username, DESC_WIDTH,
              DESC_WIDTH, rec->description);
  }
}
