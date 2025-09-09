#include <locale.h>
#include <sodium/utils.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../include/termbox.h"
#include "../../include/tui.h"

/* special characters */
#define is_special(ch) ((ch >= 0x21 && ch <= 0x2F) || (ch == 0x40 || ch >= 0x7B && ch <= 0x7D))

char *get_input(const char *prompt, char *input, const int input_len, int coord_y, int coord_x) {
  bool input_is_dynamic = false;
  if (input == NULL) {
    if ((input = calloc(1, input_len)) == NULL) {
      fprintf(stderr, "Error: Failed to allocate memory");
      return NULL;
    }
    input_is_dynamic = true;
  }

  tb_clear();
  tb_print(coord_x, coord_y, TB_WHITE | TB_BOLD, TB_DEFAULT, prompt);
  tb_present();

  int position = 0;
  int prompt_len = strlen(prompt);

  while (position < input_len) {
    struct tb_event ev;
    if (tb_poll_event(&ev) != TB_OK) continue;

    if (ev.type == TB_EVENT_KEY) {
      if (ev.key == TB_KEY_ESC) {
        if (input_is_dynamic) free(input);
        return NULL;
      } else if (ev.key == TB_KEY_ENTER) {
        break;
      } else if (ev.key == TB_KEY_BACKSPACE || ev.key == TB_KEY_BACKSPACE2) {
        if (position > 0) {
          position--;
          input[position] = '\0';
          tb_set_cell(coord_x + prompt_len + position, coord_y, ' ', TB_DEFAULT, TB_DEFAULT);
          tb_present();
        }
      } else if (isalnum(ev.ch) != 0 || is_special(ev.ch)) {
        input[position] = (char)ev.ch;
        tb_set_cell(coord_x + prompt_len + position, coord_y, ev.ch, TB_DEFAULT, TB_DEFAULT);
        position++;
        tb_present();
      }
    }
  }
  return input;
}

static void draw_art(void) {
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
  setlocale(LC_ALL, "");

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

char *get_secret(const char *prompt) {
  char *secret = NULL;
  int prompt_len = strlen(prompt);
  ssize_t center_y;
  ssize_t center_x;

  draw_art();
  int term_w = tb_width();
  int term_h = tb_height();
  if ((secret = (char *)sodium_malloc(MASTER_MAX_LEN + 1)) == NULL) {
    fprintf(stderr, "Error: Failed to allocate memory\n");
    return NULL;
  }

  sodium_memzero((void *const)secret, MASTER_MAX_LEN + 1);
  center_y = (term_h / 2) + 4;
  center_x = (term_w - prompt_len - MASTER_MAX_LEN) / 2;

  if (center_x < 0) center_x = 0;
  if (center_y >= term_h - 1) center_y = term_h - 2;

  tb_print(center_x, center_y, TB_BOLD | TB_WHITE, TB_DEFAULT, prompt);
  tb_present();

  int i = 0;
  struct tb_event ev;
  while (1) {
    tb_poll_event(&ev);
    if (ev.type == TB_EVENT_KEY) {
      if (ev.key == TB_KEY_ENTER || ev.key == TB_KEY_ESC) {
        break;
      }

      if (ev.key == TB_KEY_BACKSPACE2 || ev.key == TB_KEY_BACKSPACE) {
        if (i > 0) {
          secret[--i] = '\0';
          tb_set_cell(center_x + prompt_len + i, center_y, ' ', TB_WHITE, TB_DEFAULT);
          tb_present();
          continue;
        }
      }

      if (isalnum(ev.ch) == 0 && !is_special(ev.ch)) continue;

      secret[i] = (char)ev.ch;
      tb_set_cell(center_x + prompt_len + i, center_y, '*', TB_WHITE, TB_DEFAULT);
      tb_present();
      i++;
    }
  }

  return secret;
}
