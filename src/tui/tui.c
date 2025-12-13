#include "termbox2.h"
#define TB_IMPL
#include "tui.h"

#include <locale.h>
#include <stdint.h>
#include <stdlib.h>

#include "database.h"

int total_pages;
int current_page;
int records_per_page = 30;

bool init_tui(void) {
    if (tb_init() != TB_OK) {
        fprintf(stderr, "Error: Failed to initialize TUS\n");
        return false;
    }

    tb_set_input_mode(TB_INPUT_ESC);
    setlocale(LC_ALL, "");
    return true;
}

void cleanup_tui(void) {
    tb_clear();
    tb_shutdown();
}

static bool notify_deleted(int64_t id) {
    if (id == DELETED) {
        display_notifctn("Note: Record Already Deleted");
        return true;
    }
    return false;
}

int main_tui(sqlite3 *db) {
    struct tb_event ev = {0};
    queue_t search_queue = {0};
    char *search_pattern = NULL;
    int64_t current_position = 0;
    record_array_t records = {0, 0, NULL};

    if (!load_records(db, &records)) {
        fprintf(stderr, "Error: Failed to load data from database\n");
        return 0;
    }

    if (records.size == 0) {
        fprintf(stderr, "Warning: No records found\n");
        return 0;
    }

    init_tui();
    int term_width = tb_width();
    int term_height = tb_height();

    int start_x = (term_width - TABLE_WIDTH) / 2;
    if (start_x < 0) start_x = 0;
    int start_y = 1;
    int table_h = term_height - 4;

    draw_table_border(start_x, start_y, table_h);

    while (1) {
        records_per_page = term_height - 8;
        if (records_per_page < 1) records_per_page = 1;

        current_page = current_position / records_per_page;

        draw_table(&records, &search_queue, search_pattern, .start_x = start_x, .height = table_h,
                   .cursor = current_position);

        if (tb_poll_event(&ev) != TB_OK) continue;

        if (ev.type == TB_EVENT_KEY) {
            if (ev.key == TB_KEY_ESC || ev.ch == 'q' || ev.ch == 'Q') {
                break;
            } else if (ev.ch == 'k' || ev.key == TB_KEY_ARROW_UP) {
                if (current_position > 0) current_position--;
            } else if (ev.ch == 'j' || ev.key == TB_KEY_ARROW_DOWN) {
                if (current_position < records.size - 1) current_position++;
            } else if (ev.ch == 'h' || ev.key == TB_KEY_ARROW_LEFT) {
                current_position = (current_page - 1) * records_per_page;
                if (current_position < 0) current_position = 0;
            } else if (ev.ch == 'l' || ev.key == TB_KEY_ARROW_RIGHT) {
                current_position = (current_page + 1) * records_per_page;
                if (current_position >= records.size) current_position = records.size - 1;
            } else if (ev.ch == 'g' || ev.key == TB_KEY_HOME) {
                current_position = 0;
            } else if (ev.ch == 'G' || ev.key == TB_KEY_END) {
                current_position = records.size - 1;
            } else if (ev.ch == '/') {
                if (search_pattern != NULL) {
                    free(search_pattern);
                    search_pattern = NULL;
                }

                free_queue(&search_queue);
                search_pattern = get_search_parttern();
                draw_table_border(start_x, start_y, table_h);
                continue;
            } else if (ev.ch == 'n') {
                if (!queue_is_empty(&search_queue)) {
                    int64_t index = dequeue(&search_queue);

                    if (index == QUEUE_ERR) {
                        display_notifctn("Error: Dequeue Failed");
                        continue;
                    }

                    current_position = index;
                    continue;
                } else {
                    display_notifctn("Note: Record Not Found");
                }
            } else if (ev.ch == '?') {
                display_help();
                draw_table_border(start_x, start_y, table_h);
                continue;

            } else if (ev.ch == 'd') {
                if (notify_deleted(records.data[current_position].id)) continue;
                if (!delete_record(db, records.data[current_position].id)) {
                    display_notifctn("Error: Deletion Failed");
                    continue;
                }

                display_notifctn("Note: Record Deleted");
                records.data[current_position].id = DELETED;

            } else if (ev.ch == 'u') {
                if (notify_deleted(records.data[current_position].id)) continue;
                if (!do_updates(db, &records, current_position)) {
                    draw_table_border(start_x, start_y, table_h);
                    continue;
                }

                display_notifctn("Note: Record Updated");
                draw_table_border(start_x, start_y, table_h);
            } else if (ev.key == TB_KEY_ENTER) {
                if (notify_deleted(records.data[current_position].id)) continue;
                display_secret(db, records.data[current_position].id);
                draw_table_border(start_x, start_y, table_h);
            } else if (ev.ch == 'L') {
                if (notify_deleted(records.data[current_position].id)) continue;
                display_desc(records.data[current_position].description);
                draw_table_border(start_x, start_y, table_h);
                continue;
            }
        } else if (ev.type == TB_EVENT_RESIZE) {
            term_width = ev.w;
            term_height = ev.h;

            records_per_page = term_height - 6;
            if (records_per_page < 1) records_per_page = 1;

            start_x = (term_width - TABLE_WIDTH) / 2;
            start_y = 1;
            table_h = term_height - 4;
            tb_clear();
            draw_table_border(start_x, start_y, table_h);
        }
    }

    cleanup_tui();
    free_records(&records);
    free_queue(&search_queue);
    if (search_pattern != NULL) free(search_pattern);
    return 1;
}
