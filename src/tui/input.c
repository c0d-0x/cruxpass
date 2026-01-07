#include "cruxpass.h"
#include "tui.h"

#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <sodium/utils.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define IS_VALID(ch) (((ch >= 0x20) && (ch <= 0x7E) && (ch != 0x2C)))
#define IS_DIGIT(ch) ((ch >= 0x30) && (ch <= 0x39))

char *get_input(const char *prompt, char *input, const int input_len, int start_x, int start_y) {
    bool input_is_dynamic = false;
    if (input == NULL) {
        if ((input = calloc(1, input_len)) == NULL) {
            display_notifctn("Error: Memory allocate");
            return NULL;
        }
        input_is_dynamic = true;
    }

    int prompt_len = 0;
    if (prompt != NULL) {
        prompt_len = strlen(prompt);
        tb_print(start_x, start_y, TB_DEFAULT | TB_BOLD, TB_DEFAULT, prompt);
        tb_present();
    }

    int position = 0;
    struct tb_event ev;
    while (position < input_len) {
        if (tb_poll_event(&ev) != TB_OK) continue;

        if (ev.type == TB_EVENT_KEY) {
            if (ev.key == TB_KEY_ESC) {
                if (input_is_dynamic) free(input);
                tb_hide_cursor();
                return NULL;
            } else if (ev.key == TB_KEY_ENTER) {
                break;
            } else if (ev.key == TB_KEY_BACKSPACE || ev.key == TB_KEY_BACKSPACE2) {
                if (position > 0) {
                    position--;
                    input[position] = '\0';
                    tb_set_cell(start_x + prompt_len + position, start_y, ' ', TB_DEFAULT, TB_DEFAULT);
                    tb_set_cursor(start_x + prompt_len + position, start_y);
                    tb_present();
                }
            } else if (IS_VALID(ev.ch)) {
                input[position] = (char) ev.ch;
                tb_set_cell(start_x + prompt_len + position, start_y, ev.ch, TB_DEFAULT, TB_DEFAULT);
                position++;
                tb_set_cursor(start_x + prompt_len + position, start_y);
                tb_present();
            }
        } else if (ev.type == TB_EVENT_RESIZE) {
            continue;
        }
    }

    if (input != NULL && (strlen(input) == 0 && input_is_dynamic)) {
        free(input);
        input = NULL;
    }

    tb_hide_cursor();
    return input;
}

char *get_secret(const char *prompt) {
    char *secret = NULL;
    int prompt_len = strlen(prompt);

    draw_art();
    int term_w = tb_width();
    int term_h = tb_height();
    if ((secret = (char *) sodium_malloc(MASTER_MAX_LEN + 1)) == NULL) {
        display_notifctn("Error: Memory allocate");
        return NULL;
    }

    sodium_memzero((void *const) secret, MASTER_MAX_LEN + 1);
    int start_y = (term_h / 2) + 4;
    int start_x = (term_w - prompt_len - MASTER_MAX_LEN) / 2;

    if (start_x < 0) start_x = 0;
    if (start_y >= term_h - 1) start_y = term_h - 2;

    tb_print(start_x, start_y, TB_DEFAULT | TB_BOLD, TB_DEFAULT, prompt);
    tb_present();

    int position = 0;
    struct tb_event ev = {0};
    while (position < MASTER_MAX_LEN) {
        tb_poll_event(&ev);
        if (ev.type == TB_EVENT_KEY) {
            if (ev.key == TB_KEY_ENTER) {
                break;
            }

            if (ev.key == TB_KEY_ESC) {
                sodium_memzero((void *const) secret, MASTER_MAX_LEN + 1);
                sodium_free(secret);
                tb_hide_cursor();
                return NULL;
            }

            if (ev.key == TB_KEY_BACKSPACE2 || ev.key == TB_KEY_BACKSPACE) {
                if (position > 0) {
                    secret[--position] = '\0';
                    tb_set_cell(start_x + prompt_len + position, start_y, ' ', TB_DEFAULT, TB_DEFAULT);
                    tb_set_cursor(start_x + prompt_len + position, start_y);
                    tb_present();
                    continue;
                }
            }

            if (!IS_VALID(ev.ch)) continue;
            secret[position] = (char) ev.ch;
            tb_set_cell(start_x + prompt_len + position, start_y, '*', TB_DEFAULT, TB_DEFAULT);

            position++;
            tb_set_cursor(start_x + prompt_len + position, start_y);
            tb_present();
        } else if (ev.type == TB_EVENT_RESIZE) {
            tb_clear();
            draw_art();
            term_w = ev.w;
            term_h = ev.h;
            start_y = (term_h / 2) + 4;
            start_x = (term_w - prompt_len - MASTER_MAX_LEN) / 2;

            if (start_x < 0) start_x = 0;
            if (start_y >= term_h - 1) start_y = term_h - 2;

            tb_print(start_x, start_y, TB_DEFAULT | TB_BOLD, TB_DEFAULT, prompt);
            tb_present();

            continue;
        }
    }

    tb_hide_cursor();
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
    tb_print(start_x + 2, start_y, TB_DEFAULT | TB_BOLD, TB_DEFAULT, "| Search |");
    tb_present();
    search_parttern = get_input(NULL, NULL, SEARCH_TXT_MAX, start_x + 2, start_y + 1);
    tb_clear();
    return search_parttern;
}

int get_ulong(char *prompt) {
    int sec_win_h = 3;
    int sec_win_w = MIN_WIN_WIDTH;

    int term_w = tb_width();
    int term_h = tb_height();

    if (term_w < 64) {
        display_notifctn("Warning: Term width too small");
        return (-1);
    }

    int start_x = (term_w - sec_win_w) / 2;
    int start_y = (term_h - sec_win_h) / 2;

    tb_clear();
    char input[DIGIT_COUNT_MAX] = {0};
    draw_border(start_x, start_y, MIN_WIN_WIDTH + 4, 3, TB_DEFAULT, TB_DEFAULT);
    if (prompt != NULL) tb_printf(start_x + 2, start_y, TB_DEFAULT | TB_BOLD, TB_DEFAULT, "| %s |", prompt);
    tb_present();

    struct tb_event ev;
    int position = 0;
    start_x += 2;
    start_y++;

    while (position < DIGIT_COUNT_MAX) {
        if (tb_poll_event(&ev) != TB_OK) continue;

        if (ev.type == TB_EVENT_KEY) {
            if (ev.key == TB_KEY_ESC) {
                tb_hide_cursor();
                return (-1);
            } else if (ev.key == TB_KEY_ENTER) {
                break;
            } else if (ev.key == TB_KEY_BACKSPACE || ev.key == TB_KEY_BACKSPACE2) {
                if (position > 0) {
                    position--;
                    input[position] = '\0';
                    tb_set_cell(start_x + position, start_y, ' ', TB_DEFAULT, TB_DEFAULT);
                    tb_set_cursor(start_x + position, start_y);
                    tb_present();
                }
            } else if (IS_DIGIT(ev.ch)) {
                input[position] = (char) ev.ch;
                tb_set_cell(start_x + position, start_y, ev.ch, TB_DEFAULT, TB_DEFAULT);
                position++;
                tb_set_cursor(start_x + position, start_y);
                tb_present();
            }
        } else if (ev.type == TB_EVENT_RESIZE) {
            continue;
        }
    }

    long result = strtol(input, NULL, 10);
    if (errno == ERANGE) return (-1);
    tb_hide_cursor();

    return result;
}

void get_random_secret(sqlite3 *db, bank_options_t opt) {
    long ran_len = get_ulong("Secret length");
    if (ran_len > SECRET_MAX_LEN || ran_len < SECRET_MIN_LEN) {
        display_notifctn("Warning: Invalid secret len");
        return;
    }

    char *secret_str = random_secret(ran_len, &opt);
    if (secret_str == NULL) return;
    display_ran_secret(db, secret_str);
    free(secret_str);
}
