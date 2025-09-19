#include <sodium/utils.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../include/tui.h"

/* special characters */
#define is_special(ch) ((ch >= 0x21 && ch <= 0x2F) || (ch == 0x40 || (ch >= 0x7B && ch <= 0x7D)))

char *get_input(const char *prompt, char *input, const int input_len, int start_y, int start_x) {
  bool input_is_dynamic = false;
  if (input == NULL) {
    if ((input = calloc(1, input_len)) == NULL) {
      fprintf(stderr, "Error: Failed to allocate memory");
      return NULL;
    }
    input_is_dynamic = true;
  }

  int prompt_len = 0;
  if (prompt != NULL) {
    prompt_len = strlen(prompt);
    tb_print(start_x, start_y, TB_WHITE | TB_BOLD, TB_DEFAULT, prompt);
    tb_present();
  }

  int position = 0;

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
          tb_set_cell(start_x + prompt_len + position, start_y, ' ', TB_DEFAULT, TB_DEFAULT);
          tb_present();
        }
      } else if (isalnum(ev.ch) != 0 || is_special(ev.ch)) {
        input[position] = (char)ev.ch;
        tb_set_cell(start_x + prompt_len + position, start_y, ev.ch, TB_DEFAULT, TB_DEFAULT);
        position++;
        tb_present();
      }
    } else if (ev.type == TB_EVENT_RESIZE) {
      continue;
    }
  }
  return input;
}

char *get_secret(const char *prompt) {
  char *secret = NULL;
  int prompt_len = strlen(prompt);

  draw_art();
  int term_w = tb_width();
  int term_h = tb_height();
  if ((secret = (char *)sodium_malloc(MASTER_MAX_LEN + 1)) == NULL) {
    fprintf(stderr, "Error: Failed to allocate memory\n");
    return NULL;
  }

  sodium_memzero((void *const)secret, MASTER_MAX_LEN + 1);
  int start_y = (term_h / 2) + 4;
  int start_x = (term_w - prompt_len - MASTER_MAX_LEN) / 2;

  if (start_x < 0) start_x = 0;
  if (start_y >= term_h - 1) start_y = term_h - 2;

  tb_print(start_x, start_y, TB_BOLD | TB_WHITE, TB_DEFAULT, prompt);
  tb_present();

  int i = 0;
  struct tb_event ev;
  while (1) {
    tb_poll_event(&ev);
    if (ev.type == TB_EVENT_KEY) {
      if (ev.key == TB_KEY_ENTER) {
        break;
      }

      if (ev.key == TB_KEY_ESC) {
        sodium_memzero((void *const)secret, MASTER_MAX_LEN);
        sodium_free(secret);
        return NULL;
      }

      if (ev.key == TB_KEY_BACKSPACE2 || ev.key == TB_KEY_BACKSPACE) {
        if (i > 0) {
          secret[--i] = '\0';
          tb_set_cell(start_x + prompt_len + i, start_y, ' ', TB_WHITE, TB_DEFAULT);
          tb_present();
          continue;
        }
      }

      if (isalnum(ev.ch) == 0 && !is_special(ev.ch)) continue;

      secret[i] = (char)ev.ch;
      tb_set_cell(start_x + prompt_len + i, start_y, '*', TB_WHITE, TB_DEFAULT);
      tb_present();
      i++;
    } else if (ev.type == TB_EVENT_RESIZE) {
      tb_clear();
      draw_art();
      term_w = ev.w;
      term_h = ev.h;
      start_y = (term_h / 2) + 4;
      start_x = (term_w - prompt_len - MASTER_MAX_LEN) / 2;

      if (start_x < 0) start_x = 0;
      if (start_y >= term_h - 1) start_y = term_h - 2;

      tb_print(start_x, start_y, TB_BOLD | TB_WHITE, TB_DEFAULT, prompt);
      tb_present();

      continue;
    }
  }

  return secret;
}

char *get_search_parttern(void) {
  int term_w = tb_width();
  int term_h = tb_height();
  char *search_parttern = NULL;

  int start_x = (term_w - (SEARCH_TXT_MAX + 2)) / 2;
  int start_y = (term_h - 4) / 2;
  if (start_x <= 0 || start_y <= 0) {
    display_notifctn("Warning: Term width or height too small");
    return NULL;
  }

  tb_clear();

  draw_border(start_x, start_y, SEARCH_TXT_MAX + 4, 3, TB_DEFAULT, TB_DEFAULT);
  tb_print(start_x + 2, start_y, TB_DEFAULT | TB_BOLD, TB_DEFAULT, " Search ");
  tb_present();
  search_parttern = get_input(NULL, NULL, SEARCH_TXT_MAX, start_y + 1, start_x + 2);
  tb_clear();
  return search_parttern;
}
