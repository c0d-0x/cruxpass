// args v1.3.0
// Documentation, examples, and issues: https://github.com/spevnev/args

// MIT License
//
// Copyright (c) 2025 Serhii Pievniev
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// ====================================================================================================================
//                                                  IMPLEMENTATION
// ====================================================================================================================

#ifdef ARGS_H_
#error args must be included only once
#else
#define ARGS_H_
#endif

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#ifndef ARGS_PADDING
#define ARGS_PADDING 2
#endif

#ifndef ARGS_LINE_LENGTH
#define ARGS_LINE_LENGTH 80
#endif

#ifndef ARGS_MIN_DESC_LENGTH
#define ARGS_MIN_DESC_LENGTH 30
#endif

typedef enum {
    ARGS__TYPE_LONG,
    ARGS__TYPE_FLOAT,
    ARGS__TYPE_STRING,
    ARGS__TYPE_PATH,
    ARGS__TYPE_BOOL,
    ARGS__TYPE_ENUM_INDEX,
    ARGS__TYPE_ENUM_STRING,
} Args__Type;

typedef struct Args__Option {
    struct Args__Option *next;
    char short_name;
    char *long_name;
    char *description;
    bool is_optional;
    bool is_set;
    bool is_matching;
    Args__Type type;
    union {
        long long_;
        float float_;
        char *string;
        char *enum_;
    } default_value;
    union {
        long long_;
        float float_;
        char *string;
        bool bool_;
        struct {
            char **values;
            unsigned int length;
            union {
                size_t index;
                const char *value;
            } as;
        } enum_;
    } value;
} Args__Option;

typedef struct {
    Args__Option *head;
    Args__Option *tail;
    bool are_parsed;
    char **positional_args;
    bool options_have_short_name;
    size_t options_max_name_length;
} Args;

#ifdef __has_attribute
#define ARGS__HAS_ATTRIBUTE(attribute) __has_attribute(attribute)
#else
#define ARGS__HAS_ATTRIBUTE(attribute) 0
#endif

#if ARGS__HAS_ATTRIBUTE(unused)
#define ARGS__MAYBE_UNUSED __attribute__((unused))
#else
#define ARGS__MAYBE_UNUSED
#endif

#if ARGS__HAS_ATTRIBUTE(warn_unused_result)
#define ARGS__WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#define ARGS__WARN_UNUSED_RESULT
#endif

#define ARGS__FATAL(...)              \
    do {                              \
        fprintf(stderr, "ERROR: ");   \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, ".\n");       \
        exit(EXIT_FAILURE);           \
    } while (0)

#define ARGS__OUT_OF_MEMORY() ARGS__FATAL("Process ran out of memory")
#define ARGS__UNREACHABLE() ARGS__FATAL("Unreachable")

#define ARGS__ASSERT(condition)                                                         \
    do {                                                                                \
        if (!(condition)) {                                                             \
            ARGS__FATAL("Assert (%s) failed at %s:%d", #condition, __FILE__, __LINE__); \
        }                                                                               \
    } while (0)

static char *args__strdup(const char *string) {
    ARGS__ASSERT(string != NULL);
    size_t length = strlen(string);
    char *copy = (char *) malloc(length + 1);
    if (copy == NULL) ARGS__OUT_OF_MEMORY();
    memcpy(copy, string, length);
    copy[length] = '\0';
    return copy;
}

static const char *args__basename(const char *path) {
    ARGS__ASSERT(path != NULL);

    const char *filename = strrchr(path, '/');
#ifdef _WIN32
    const char *win_filename = strrchr(path, '\\');
    // Windows can use either slash, take the last one.
    if (filename == NULL || (win_filename != NULL && win_filename > filename)) filename = win_filename;
#endif
    return filename == NULL ? path : filename + 1;
}

static Args__Option *args__new_option(Args *a, char short_name, const char *long_name, const char *description,
                                      bool is_optional, Args__Type type) {
    ARGS__ASSERT(a != NULL);

    if (a->are_parsed) ARGS__FATAL("New options cannot be added after parsing the arguments");

    if (short_name != '\0' && !isalnum(short_name)) {
        ARGS__FATAL("Invalid short name '%c'. It must be alphanumeric", short_name);
    }

    if (long_name == NULL) ARGS__FATAL("Option must have a long name");
    for (const char *c = long_name; *c != '\0'; c++) {
        if (isalnum(*c) || *c == '_' || *c == '+') continue;
        if (*c == '-' && c != long_name) continue;
        ARGS__FATAL("Invalid long name \"%s\". It must consist of alphanumerics and \"-_+\", and cannot start with '-'",
                    long_name);
    }

    if (description != NULL) {
        for (const char *c = description; *c != '\0'; c++) {
            if (*c == '\n') continue;
            if (*c == '\t') ARGS__FATAL("Description must not contain tabs to maintain proper length");
            if (!isprint(*c)) ARGS__FATAL("Description of \"%s\" contains an invalid character 0x%x", long_name, *c);
        }
    }

    Args__Option *option = (Args__Option *) malloc(sizeof(*option));
    if (option == NULL) ARGS__OUT_OF_MEMORY();
    option->next = NULL;
    option->short_name = short_name;
    option->long_name = args__strdup(long_name);
    option->description = description == NULL ? NULL : args__strdup(description);
    option->is_optional = is_optional;
    option->is_set = false;
    option->is_matching = false;
    option->type = type;

    size_t name_length = strlen(option->long_name);
    if (name_length > a->options_max_name_length) a->options_max_name_length = name_length;
    if (short_name != '\0') a->options_have_short_name = true;

    if (a->head == NULL) {
        a->head = option;
    } else {
        a->tail->next = option;
    }
    a->tail = option;
    return option;
}

static void args__set_enum_values(Args__Option *option, const char **values) {
    ARGS__ASSERT(option != NULL && values != NULL);

    size_t length = 0;
    while (values[length] != NULL) length++;
    option->value.enum_.length = length;

    char **copy = (char **) malloc(sizeof(*copy) * length);
    if (copy == NULL) ARGS__OUT_OF_MEMORY();
    option->value.enum_.values = copy;

    for (size_t i = 0; i < length; i++) {
        for (const char *c = values[i]; *c != '\0'; c++) {
            if (!isprint(*c)) {
                ARGS__FATAL(
                    "Enum value of \"%s\" contains an invalid character 0x%x. "
                    "Maybe enum array is missing a NULL-terminator",
                    option->long_name, *c);
            }
        }
        copy[i] = args__strdup(values[i]);
    }
}

static void args__parse_value(Args__Option *option, const char *value) {
    ARGS__ASSERT(option != NULL && value != NULL);
    switch (option->type) {
        case ARGS__TYPE_LONG: {
            char *end = NULL;
            option->value.long_ = strtol(value, &end, 0);
            if (end == NULL || *end != '\0') ARGS__FATAL("Invalid integer \"%s\"", value);
        } break;
        case ARGS__TYPE_FLOAT: {
            char *end = NULL;
            option->value.float_ = strtof(value, &end);
            if (end == NULL || *end != '\0') ARGS__FATAL("Invalid float \"%s\"", value);
        } break;
        case ARGS__TYPE_STRING:
        case ARGS__TYPE_PATH:
            free(option->value.string);
            option->value.string = args__strdup(value);
            break;
        case ARGS__TYPE_BOOL: ARGS__UNREACHABLE(); break;
        case ARGS__TYPE_ENUM_INDEX:
        case ARGS__TYPE_ENUM_STRING:
            for (size_t i = 0; i < option->value.enum_.length; i++) {
                if (strcasecmp(option->value.enum_.values[i], value) != 0) continue;

                if (option->type == ARGS__TYPE_ENUM_INDEX) {
                    option->value.enum_.as.index = i;
                } else {
                    option->value.enum_.as.value = option->value.enum_.values[i];
                }
                return;
            }
            ARGS__FATAL("Invalid value \"%s\" for option \"%s\"", value, option->long_name);
    }
}

ARGS__MAYBE_UNUSED static void args__print_string_value(FILE *fp, const char *string) {
    ARGS__ASSERT(fp != NULL);

    if (string == NULL) {
        fprintf(fp, "none");
        return;
    }

    fprintf(fp, "\"");
    for (const char *c = string; *c != '\0'; c++) {
        switch (*c) {
            case '\n': fprintf(fp, "\\n"); break;
            case '\r': fprintf(fp, "\\r"); break;
            case '\t': fprintf(fp, "\\t"); break;
            default:
                if (isprint(*c)) {
                    fprintf(fp, "%c", *c);
                } else {
                    fprintf(fp, "\\x%02hhx", *c);
                }
                break;
        }
    }
    fprintf(fp, "\"");
}

#ifndef ARGS_DISABLE_COMPLETION
// Prints string with '\' and chars in `escaped_chars` escaped with '\'.
// Replaces '\n' with ' '.
static void args__completion_print_escaped(const char *string, const char *escaped_chars) {
    ARGS__ASSERT(string != NULL && escaped_chars != NULL);
    for (const char *c = string; *c != '\0'; c++) {
        if (*c == '\n') {
            printf(" ");
        } else {
            if (strchr(escaped_chars, *c) != NULL || *c == '\\') printf("\\");
            printf("%c", *c);
        }
    }
}

static void args__bash_completion_script(const char *program_name) {
    ARGS__ASSERT(program_name != NULL);
    printf(
        "_%s() {\n"
        "    local cur prev words cword\n"
        "    _init_completion || return\n"
        "\n"
        "    local IFS=$'\\n'\n"
        "    COMPREPLY=( $(%s __complete bash \"$prev\" \"$cur\" \"$COLUMNS\" 2>/dev/null) )\n"
        "\n"
        "    if [[ ${#COMPREPLY[@]} -eq 1 && \"${COMPREPLY[0]}\" == \"-\" ]]; then\n"
        "        COMPREPLY=()\n"
        "    elif [[ ${#COMPREPLY[@]} -eq 0 ]]; then\n"
        "        COMPREPLY=( $(compgen -o default -- \"$cur\") )\n"
        "    fi\n"
        "}\n"
        "\n"
        "complete -F _%s %s\n",
        program_name, program_name, program_name, program_name);
}

static void args__zsh_completion_script(const char *program_name) {
    ARGS__ASSERT(program_name != NULL);
    printf(
        "#compdef %s\n"
        "\n"
        "_%s() {\n"
        "    local IFS=$'\\n'\n"
        "    _arguments $(%s __complete zsh 2>/dev/null)\n"
        "}\n"
        "\n"
        "_%s\n",
        program_name, program_name, program_name, program_name);
}

static void args__fish_completion_script(const char *program_name) {
    ARGS__ASSERT(program_name != NULL);
    printf(
        "complete -c %s -e\n"
        "\n"
        "for args in (%s __complete fish 2>/dev/null)\n"
        "    eval \"complete -c %s $args\"\n"
        "end\n",
        program_name, program_name, program_name);
}

static void args__bash_complete(Args *a, const char *prev, const char *cur, const char *columns_string) {
    ARGS__ASSERT(a != NULL && prev != NULL && cur != NULL && columns_string != NULL);

    size_t cur_length = strlen(cur);
    if (cur_length >= 1 && cur[0] == '-') {
        char *end = NULL;
        int columns = strtol(columns_string, &end, 0) - 1;
        if (end == NULL || *end != '\0' || columns <= 0) ARGS__FATAL("Invalid columns \"%s\"", columns_string);

        // Chooses which options to print by setting `is_matching`.
        bool print_short = true;
        bool print_long = true;
        if (cur_length == 1) {
            for (Args__Option *i = a->head; i != NULL; i = i->next) i->is_matching = true;
        } else if (cur[1] == '-') {
            const char *name = cur + 2;
            size_t name_length = cur_length - 2;
            for (Args__Option *i = a->head; i != NULL; i = i->next) {
                if (strlen(i->long_name) >= name_length && strncmp(i->long_name, name, name_length) == 0) {
                    i->is_matching = true;
                }
            }
            print_short = false;
        } else if (cur_length == 2) {
            char name = cur[1];
            for (Args__Option *i = a->head; i != NULL; i = i->next) {
                if (i->short_name == name) {
                    i->is_matching = true;
                    break;
                }
            }
            print_long = false;
        }

        int matches = 0;
        for (Args__Option *i = a->head; i != NULL; i = i->next) {
            if (i->is_matching) matches++;
        }

        // Print matching options.
        bool is_first = true;
        for (Args__Option *i = a->head; i != NULL; i = i->next) {
            if (!i->is_matching) continue;

            int length = 0;
            if (print_short && a->options_have_short_name) {
                if (i->short_name != '\0') {
                    printf("-%c", i->short_name);
                    if (print_long) printf(", ");
                } else {
                    printf("  ");
                    if (print_long) printf("  ");
                }

                length += 2;
                if (print_short) length += 2;
            }

            int padding = 0;
            if (print_long) {
                printf("--%s", i->long_name);
                size_t name_length = strlen(i->long_name);
                padding = a->options_max_name_length - name_length;
                length += 2 + name_length;
            }

            // When there is only one completion, bash will immediately append it to
            // the current command, don't print description or it will be used too.
            if (matches > 1) {
                if (padding > 0) {
                    printf("%*c", padding, ' ');
                    length += padding;
                }

                if (i->description != NULL) {
                    printf(" -- ");
                    args__completion_print_escaped(i->description, "");
                    length += 4 + strlen(i->description);
                }

                // Force bash to display options in a column by padding it to a line width.
                if (is_first && length < columns) printf("%*c", columns - length, ' ');
                is_first = false;
            }
            printf("\n");
        }
        return;
    }

    size_t prev_length = strlen(prev);
    if (prev_length >= 2 && prev[0] == '-') {
        Args__Option *option = NULL;
        if (prev[1] == '-') {
            const char *name = prev + 2;
            size_t name_length = strlen(name);
            for (Args__Option *i = a->head; i != NULL; i = i->next) {
                if (strlen(i->long_name) == name_length && strncmp(i->long_name, name, name_length) == 0) {
                    option = i;
                    break;
                }
            }
        } else {
            // Handle stacked short options. We cannot just take the last char because it can be a value.
            // e.g. `--str string` -> `-sstring`. Last char, `g`, is value, not an option.
            size_t index = 1;
            while (true) {
                // Find current option if any.
                Args__Option *current_option = NULL;
                for (Args__Option *i = a->head; i != NULL; i = i->next) {
                    if (i->short_name == prev[index]) {
                        current_option = i;
                        break;
                    }
                }
                if (current_option == NULL) return;

                // It is the last one, complete it's value.
                if (index + 1 >= prev_length) {
                    option = current_option;
                    break;
                }

                // If it's a flag, check the next option,
                // otherwise, it's value is already provided, exit.
                if (current_option->type != ARGS__TYPE_BOOL) return;
                index++;
            }
        }
        if (option == NULL) return;

        switch (option->type) {
            case ARGS__TYPE_LONG:
            case ARGS__TYPE_FLOAT:
            case ARGS__TYPE_BOOL:
            case ARGS__TYPE_STRING: printf("-\n"); break;
            case ARGS__TYPE_PATH:   break;
            case ARGS__TYPE_ENUM_INDEX:
            case ARGS__TYPE_ENUM_STRING:
                for (size_t i = 0; i < option->value.enum_.length; i++) {
                    const char *value = option->value.enum_.values[i];
                    if (strlen(value) >= cur_length && strncmp(value, cur, cur_length) == 0) {
                        printf("%s\n", value);
                    }
                }
                break;
        }
    }
}

static void args__zsh_print_option_details(Args__Option *option) {
    ARGS__ASSERT(option != NULL);

    if (option->description != NULL) {
        printf("[");
        args__completion_print_escaped(option->description, "]");
        printf("]");
    }

    switch (option->type) {
        case ARGS__TYPE_PATH: printf(":path:_files"); break;
        case ARGS__TYPE_ENUM_INDEX:
        case ARGS__TYPE_ENUM_STRING:
            printf(":%s:(", option->long_name);
            for (size_t i = 0; i < option->value.enum_.length; i++) {
                if (i > 0) printf(" ");
                args__completion_print_escaped(option->value.enum_.values[i], " :()");
            }
            printf(")");
            break;
        default: break;  // Other types can accept anything, no suggestions.
    }
    printf("\n");
}

static void args__zsh_complete(Args *a) {
    ARGS__ASSERT(a != NULL);
    for (Args__Option *i = a->head; i != NULL; i = i->next) {
        if (i->short_name != '\0') {
            printf("(-%c --%s)-%c", i->short_name, i->long_name, i->short_name);
            args__zsh_print_option_details(i);

            printf("(-%c --%s)", i->short_name, i->long_name);
        }
        printf("--%s", i->long_name);
        if (i->type != ARGS__TYPE_BOOL) printf("=");
        args__zsh_print_option_details(i);
    }
    // Set default completion of positional arguments to path.
    printf("*:file:_files\n");
}

static void args__fish_complete(Args *a) {
    ARGS__ASSERT(a != NULL);
    for (Args__Option *i = a->head; i != NULL; i = i->next) {
        printf("-l %s -%c", i->long_name, i->type == ARGS__TYPE_PATH ? 'F' : 'f');
        if (i->type == ARGS__TYPE_ENUM_INDEX || i->type == ARGS__TYPE_ENUM_STRING) {
            printf(" -a '");
            for (size_t j = 0; j < i->value.enum_.length; j++) {
                if (j > 0) printf(" ");
                args__completion_print_escaped(i->value.enum_.values[j], " \"$()");
            }
            printf("'");
        }
        if (i->short_name != '\0') {
            // Use '-r' to avoid stacking short options.
            printf(" -s %c -r", i->short_name);
        }
        if (i->description != NULL) {
            printf(" -d \"");
            args__completion_print_escaped(i->description, "\"$");
            printf("\"");
        }
        printf("\n");
    }
}
#endif

// ====================================================================================================================
//                                                    PUBLIC API
// ====================================================================================================================

// Frees all the memory, including option values and array of positional arguments.
static void free_args(Args *a) {
    if (a == NULL) return;

    Args__Option *cur = a->head;
    while (cur != NULL) {
        switch (cur->type) {
            case ARGS__TYPE_LONG:
            case ARGS__TYPE_FLOAT:
            case ARGS__TYPE_BOOL:  break;
            case ARGS__TYPE_STRING:
            case ARGS__TYPE_PATH:
                free(cur->default_value.string);
                free(cur->value.string);
                break;
            case ARGS__TYPE_ENUM_STRING:
            case ARGS__TYPE_ENUM_INDEX:
                free(cur->default_value.enum_);
                for (size_t i = 0; i < cur->value.enum_.length; i++) free(cur->value.enum_.values[i]);
                free(cur->value.enum_.values);
                break;
        }
        free(cur->long_name);
        free(cur->description);

        Args__Option *next = cur->next;
        free(cur);
        cur = next;
    }
    a->head = NULL;
    a->tail = NULL;

    free(a->positional_args);
    a->positional_args = NULL;
}

// Defines a long option, returns a pointer set by `parse_args`.
// Use '\0' for no short name.
// Exits if `a` or `long_name` is NULL, or out of memory.
ARGS__MAYBE_UNUSED ARGS__WARN_UNUSED_RESULT static long *option_long(Args *a, char short_name, const char *long_name,
                                                                     const char *description, bool is_optional,
                                                                     long default_value) {
    ARGS__ASSERT(a != NULL);
    Args__Option *option = args__new_option(a, short_name, long_name, description, is_optional, ARGS__TYPE_LONG);
    option->default_value.long_ = option->value.long_ = default_value;
    return &option->value.long_;
}

// Defines a float option, returns a pointer set by `parse_args`.
// Use '\0' for no short name.
// Exits if `a` or `long_name` is NULL, or out of memory.
ARGS__MAYBE_UNUSED ARGS__WARN_UNUSED_RESULT static float *option_float(Args *a, char short_name, const char *long_name,
                                                                       const char *description, bool is_optional,
                                                                       float default_value) {
    ARGS__ASSERT(a != NULL);
    Args__Option *option = args__new_option(a, short_name, long_name, description, is_optional, ARGS__TYPE_FLOAT);
    option->default_value.float_ = option->value.float_ = default_value;
    return &option->value.float_;
}

// Defines a string option, returns a pointer set by `parse_args`.
// String memory is owned by library, freed by `free_args`.
// Result can be NULL only if default value is NULL.
// Use '\0' for no short name.
// Exits if `a` or `long_name` is NULL, or out of memory.
ARGS__MAYBE_UNUSED ARGS__WARN_UNUSED_RESULT static const char **option_str(Args *a, char short_name,
                                                                           const char *long_name,
                                                                           const char *description, bool is_optional,
                                                                           const char *default_value) {
    ARGS__ASSERT(a != NULL);
    Args__Option *option = args__new_option(a, short_name, long_name, description, is_optional, ARGS__TYPE_STRING);
    if (default_value == NULL) {
        option->default_value.string = NULL;
        option->value.string = NULL;
    } else {
        option->default_value.string = args__strdup(default_value);
        option->value.string = args__strdup(default_value);
    }
    return (const char **) &option->value.string;
}

// Same as `option_str` except that shell completion will suggest paths.
// Does NOT check that the value is a path.
ARGS__MAYBE_UNUSED ARGS__WARN_UNUSED_RESULT static const char **option_path(Args *a, char short_name,
                                                                            const char *long_name,
                                                                            const char *description, bool is_optional,
                                                                            const char *default_value) {
    ARGS__ASSERT(a != NULL);
    Args__Option *option = args__new_option(a, short_name, long_name, description, is_optional, ARGS__TYPE_PATH);
    if (default_value == NULL) {
        option->default_value.string = NULL;
        option->value.string = NULL;
    } else {
        option->default_value.string = args__strdup(default_value);
        option->value.string = args__strdup(default_value);
    }
    return (const char **) &option->value.string;
}

// Defines a boolean flag, returns a pointer set by `parse_args`.
// Use '\0' for no short name.
// Exits if `a` or `long_name` is NULL, or out of memory.
ARGS__MAYBE_UNUSED ARGS__WARN_UNUSED_RESULT static bool *option_flag(Args *a, char short_name, const char *long_name,
                                                                     const char *description) {
    ARGS__ASSERT(a != NULL);
    Args__Option *option = args__new_option(a, short_name, long_name, description, true, ARGS__TYPE_BOOL);
    option->value.bool_ = false;
    return &option->value.bool_;
}

// Defines an enum option, returns a pointer set by `parse_args`.
// Result is either a valid index or a default value.
// `values` must be a NULL-terminated array, matched case-insensitively.
// Use '\0' for no short name.
// Exits if `a`, `long_name`, or `values` is NULL, or out of memory.
ARGS__MAYBE_UNUSED ARGS__WARN_UNUSED_RESULT static size_t *option_enum(Args *a, char short_name, const char *long_name,
                                                                       const char *description, bool is_optional,
                                                                       size_t default_value, const char **values) {
    ARGS__ASSERT(a != NULL && values != NULL);
    Args__Option *option = args__new_option(a, short_name, long_name, description, is_optional, ARGS__TYPE_ENUM_INDEX);
    args__set_enum_values(option, values);
    option->value.enum_.as.index = default_value;
    if (default_value < option->value.enum_.length) {
        option->default_value.enum_ = args__strdup(values[default_value]);
    } else {
        option->default_value.enum_ = NULL;
    }
    return &option->value.enum_.as.index;
}

// Defines an enum option, returns a pointer set by `parse_args`.
// Result is either a string from `values` or a default value.
// String and array memory is owned by library, freed by `free_args`.
// `values` must be a NULL-terminated array, matched case-insensitively.
// Use '\0' for no short name.
// Exits if `a`, `long_name`, or `values` is NULL, or out of memory.
ARGS__MAYBE_UNUSED ARGS__WARN_UNUSED_RESULT static const char **option_enum_str(
    Args *a, char short_name, const char *long_name, const char *description, bool is_optional,
    const char *default_value, const char **values) {
    ARGS__ASSERT(a != NULL && values != NULL);
    Args__Option *option = args__new_option(a, short_name, long_name, description, is_optional, ARGS__TYPE_ENUM_STRING);
    option->default_value.enum_ = default_value == NULL ? NULL : args__strdup(default_value);
    args__set_enum_values(option, values);
    option->value.enum_.as.value = option->default_value.enum_;
    return &option->value.enum_.as.value;
}

// Parses arguments, sets option-returned values.
// Unless disabled, handles shell completion by writing to stdout and exiting.
// Must be called before side effects or stdout output.
// Returns positional arguments via `positional_args`, and their count as return value.
// Elements are from `argv`, while the array memory is managed by library.
// On error, prints to stderr and exits.
static int parse_args(Args *a, int argc, char **argv, char ***positional_args) {
    ARGS__ASSERT(a != NULL && argv != NULL);
    a->are_parsed = true;

    ARGS__ASSERT(argc >= 0);
    if (argc == 0) ARGS__FATAL("Expected the first argument to be a program name");
    ARGS__MAYBE_UNUSED const char *program_name = args__basename(argv[0]);
    argc--;
    argv++;

    for (Args__Option *i = a->head; i != NULL; i = i->next) {
        for (Args__Option *j = i->next; j != NULL; j = j->next) {
            if (i->short_name == j->short_name && i->short_name != '\0') {
                ARGS__FATAL("Duplicate short name '%c' in options \"%s\" and \"%s\"",  //
                            i->short_name, i->long_name, j->long_name);
            }

            if (strcmp(i->long_name, j->long_name) == 0) ARGS__FATAL("Duplicate option \"%s\"", i->long_name);
        }
    }

#ifndef ARGS_DISABLE_COMPLETION
    if (argc >= 1 && strcmp(argv[0], "completion") == 0) {
        if (argc != 2) ARGS__FATAL("Command 'completion' requires an argument: bash, zsh, fish");

        for (const char *c = program_name; *c != '\0'; c++) {
            if (isalnum(*c) || *c == '_' || *c == '.' || *c == '+' || *c == ':') continue;
            if (*c == '-' && c != program_name) continue;
            ARGS__FATAL("Invalid program name \"%s\"", program_name);
        }

        if (strcmp(argv[1], "bash") == 0) {
            args__bash_completion_script(program_name);
        } else if (strcmp(argv[1], "zsh") == 0) {
            args__zsh_completion_script(program_name);
        } else if (strcmp(argv[1], "fish") == 0) {
            args__fish_completion_script(program_name);
        } else {
            ARGS__FATAL("Failed to generate completion script: unknown shell \"%s\"", argv[1]);
        }
        free_args(a);
        exit(EXIT_SUCCESS);
    }

    if (argc >= 1 && strcmp(argv[0], "__complete") == 0) {
        if (argc == 1) ARGS__FATAL("Command '__complete' requires an argument: bash, zsh, fish");

        if (strcmp(argv[1], "bash") == 0) {
            if (argc != 5) ARGS__FATAL("Command '__complete bash' requires arguments '<prev> <cur> <columns>'");
            args__bash_complete(a, argv[2], argv[3], argv[4]);
        } else if (strcmp(argv[1], "zsh") == 0) {
            if (argc > 2) ARGS__FATAL("Command '__complete zsh' doesn't take arguments");
            args__zsh_complete(a);
        } else if (strcmp(argv[1], "fish") == 0) {
            if (argc > 2) ARGS__FATAL("Command '__complete fish' doesn't take arguments");
            args__fish_complete(a);
        } else {
            ARGS__FATAL("Failed to generate completions: unknown shell \"%s\"", argv[1]);
        }
        free_args(a);
        exit(EXIT_SUCCESS);
    }
#endif

    int positional_args_index = 0;
    if (positional_args != NULL) {
        a->positional_args = (char **) malloc(sizeof(*argv) * argc);
        if (a->positional_args == NULL) ARGS__OUT_OF_MEMORY();
        *positional_args = a->positional_args;
    }

    while (argc > 0) {
        char *arg = *argv;
        size_t arg_length = strlen(arg);
        ARGS__MAYBE_UNUSED const char *full_arg = arg;
        argc--;
        argv++;

        if (arg_length < 2 || arg[0] != '-') {
            if (positional_args != NULL) (*positional_args)[positional_args_index] = arg;
            positional_args_index++;
            continue;
        }

        if (arg[1] == '-') {
            arg += 2;
            arg_length -= 2;

            Args__Option *option = a->head;
            size_t option_length;
            while (option != NULL) {
                option_length = strlen(option->long_name);
                if (strncmp(arg, option->long_name, option_length) == 0
                    && (arg[option_length] == '\0' || arg[option_length] == '=')) {
                    break;
                }
                option = option->next;
            }
#ifdef ARGS_SKIP_UNKNOWN
            if (option == NULL) continue;
#else
            if (option == NULL) ARGS__FATAL("Unknown or invalid option \"%s\"", full_arg);
#endif

#ifndef ARGS_ALLOW_OVERWRITING
            if (option->is_set) ARGS__FATAL("Option \"%s\" is set more than once", option->long_name);
#endif
            option->is_set = true;

            if (option->type == ARGS__TYPE_BOOL) {
                if (arg[option_length] == '=') ARGS__FATAL("Flags cannot have a value: \"%s\"", arg);
                option->value.bool_ = true;
                continue;
            }

            const char *value;
            if (arg[option_length] == '=') {
                value = arg + option_length + 1;
            } else {
                if (argc == 0) ARGS__FATAL("Option \"%s\" is missing a value", option->long_name);
                value = *argv;
                argc--;
                argv++;
            }

            args__parse_value(option, value);
        } else {
            arg++;
            arg_length--;
            while (arg_length > 0) {
                char ch = *arg;
                arg++;
                arg_length--;

                Args__Option *option = a->head;
                while (option != NULL && option->short_name != ch) option = option->next;

#ifdef ARGS_SKIP_UNKNOWN
                if (option == NULL) continue;
#else
                if (option == NULL) ARGS__FATAL("Unknown or invalid option '%c' in \"%s\"", ch, full_arg);
#endif

#ifndef ARGS_ALLOW_OVERWRITING
                if (option->is_set) ARGS__FATAL("Option '%c' is set more than once", option->short_name);
#endif
                option->is_set = true;

                if (option->type == ARGS__TYPE_BOOL) {
                    option->value.bool_ = true;
                    continue;
                }

                const char *value;
                if (arg_length > 0) {
                    value = arg;
                    arg_length = 0;
                } else {
                    if (argc == 0) ARGS__FATAL("Option '%c' is missing a value", option->short_name);
                    value = *argv;
                    argc--;
                    argv++;
                }

                args__parse_value(option, value);
            }
        }
    }

    for (Args__Option *option = a->head; option != NULL; option = option->next) {
        if (!option->is_optional && !option->is_set) ARGS__FATAL("Missing a required option \"%s\"", option->long_name);
    }

    return positional_args_index;
}

// Prints all options to `fp`.
// Caller is responsible for printing usage, as well as `completion` command.
ARGS__MAYBE_UNUSED static void print_options(Args *a, FILE *fp) {
    ARGS__ASSERT(a != NULL && fp != NULL);

    int offset = 4 + a->options_max_name_length + ARGS_PADDING;
    if (a->options_have_short_name) offset += 4;

    fprintf(fp, "Options:\n");
    for (Args__Option *option = a->head; option != NULL; option = option->next) {
#ifdef ARGS_HIDE_DEFAULTS
        bool print_defaults = false;
#else
        bool print_defaults = option->is_optional;
        // Flag's default is always false, don't print their value.
        if (option->type == ARGS__TYPE_BOOL) print_defaults = false;
#endif

        fprintf(fp, "  ");

        if (option->short_name != '\0') {
            fprintf(fp, "-%c, ", option->short_name);
        } else if (a->options_have_short_name) {
            fprintf(fp, "    ");
        }
        fprintf(fp, "--%s", option->long_name);

        if (option->description != NULL || print_defaults) {
            int length_diff = a->options_max_name_length - strlen(option->long_name);
            fprintf(fp, "%*c", length_diff + ARGS_PADDING, ' ');
        }

        bool is_description_multiline = false;
        // Print description and break into multiple lines if needed.
        if (option->description != NULL) {
            int line_length = ARGS_LINE_LENGTH - offset;
            if (line_length < ARGS_MIN_DESC_LENGTH) line_length = ARGS_MIN_DESC_LENGTH;

            bool is_first = true;
            char *cur = option->description;
            int length = strlen(option->description);
            while (length > 0) {
                if (!is_first) {
                    is_description_multiline = true;
                    fprintf(fp, "\n%*c", offset, ' ');
                }
                is_first = false;

                // Look for an explicit line break.
                int chunk_length = 0;
                while (chunk_length < length && chunk_length < line_length && cur[chunk_length] != '\n') chunk_length++;

                // If there isn't any, look for a space to break the line.
                if (chunk_length == line_length) {
                    while (chunk_length > 0 && cur[chunk_length] != ' ') chunk_length--;

                    // If failed to find a space or it is too early, break mid-word.
                    if (chunk_length < line_length / 2) chunk_length = line_length;
                }

                fprintf(fp, "%.*s", chunk_length, cur);

                if (cur[chunk_length] == ' ' || cur[chunk_length] == '\n') {
                    cur++;
                    length--;
                }
                cur += chunk_length;
                length -= chunk_length;
            }
        }

        if (print_defaults) {
            if (is_description_multiline) {
                // Print default on the new line to avoid breaking it too.
                fprintf(fp, "\n%*c", offset, ' ');
            } else {
                fprintf(fp, " ");
            }
            fprintf(fp, "(default: ");
            switch (option->type) {
                case ARGS__TYPE_LONG:        fprintf(fp, "%ld", option->default_value.long_); break;
                case ARGS__TYPE_FLOAT:       fprintf(fp, "%.3f", option->default_value.float_); break;
                case ARGS__TYPE_BOOL:        ARGS__UNREACHABLE();
                case ARGS__TYPE_STRING:
                case ARGS__TYPE_PATH:        args__print_string_value(fp, option->default_value.string); break;
                case ARGS__TYPE_ENUM_STRING:
                case ARGS__TYPE_ENUM_INDEX:  args__print_string_value(fp, option->default_value.enum_); break;
            }
            fprintf(fp, ")");
        }
        fprintf(fp, "\n");
    }
}

#ifdef __cplusplus
class ArgsCpp {
   public:
    ArgsCpp() : args() {}
    ~ArgsCpp() { free_args(&args); }

    // See `::option_long()`
    ARGS__MAYBE_UNUSED ARGS__WARN_UNUSED_RESULT const long &option_long(char short_name, const char *long_name,
                                                                        const char *description,
                                                                        bool is_optional = true,
                                                                        long default_value = 0) {
        return *::option_long(&args, short_name, long_name, description, is_optional, default_value);
    }

    // See `::option_float()`
    ARGS__MAYBE_UNUSED ARGS__WARN_UNUSED_RESULT const float &option_float(char short_name, const char *long_name,
                                                                          const char *description,
                                                                          bool is_optional = true,
                                                                          float default_value = 0.0F) {
        return *::option_float(&args, short_name, long_name, description, is_optional, default_value);
    }

    // See `::option_str()`
    ARGS__MAYBE_UNUSED ARGS__WARN_UNUSED_RESULT const char *&option_str(char short_name, const char *long_name,
                                                                        const char *description,
                                                                        bool is_optional = true,
                                                                        const char *default_value = nullptr) {
        return *::option_str(&args, short_name, long_name, description, is_optional, default_value);
    }

    // See `::option_path()`
    ARGS__MAYBE_UNUSED ARGS__WARN_UNUSED_RESULT const char *&option_path(char short_name, const char *long_name,
                                                                         const char *description,
                                                                         bool is_optional = true,
                                                                         const char *default_value = nullptr) {
        return *::option_path(&args, short_name, long_name, description, is_optional, default_value);
    }

    // See `::option_flag()`
    ARGS__MAYBE_UNUSED ARGS__WARN_UNUSED_RESULT const bool &option_flag(char short_name, const char *long_name,
                                                                        const char *description) {
        return *::option_flag(&args, short_name, long_name, description);
    }

    // See `::option_enum()`
    ARGS__MAYBE_UNUSED ARGS__WARN_UNUSED_RESULT const size_t &option_enum(char short_name, const char *long_name,
                                                                          const char *description, bool is_optional,
                                                                          size_t default_value, const char **values) {
        return *::option_enum(&args, short_name, long_name, description, is_optional, default_value, values);
    }

    // See `::option_enum_str()`
    ARGS__MAYBE_UNUSED ARGS__WARN_UNUSED_RESULT const char *&option_enum_str(char short_name, const char *long_name,
                                                                             const char *description, bool is_optional,
                                                                             const char *default_value,
                                                                             const char **values) {
        return *::option_enum_str(&args, short_name, long_name, description, is_optional, default_value, values);
    }

    // See `::parse_args()`
    int parse_args(int argc, char **argv, char **&positional_args) {
        return ::parse_args(&args, argc, argv, &positional_args);
    }

    // See `::print_options()`
    ARGS__MAYBE_UNUSED void print_options(FILE *fp = stdout) { ::print_options(&args, fp); }

   private:
    Args args;
};
#endif  // __cplusplus

#undef ARGS__HAS_ATTRIBUTE
#undef ARGS__MAYBE_UNUSED
#undef ARGS__WARN_UNUSED_RESULT
#undef ARGS__FATAL
#undef ARGS__OUT_OF_MEMORY
#undef ARGS__UNREACHABLE
#undef ARGS__ASSERT
