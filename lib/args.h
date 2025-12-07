/*
MIT License

Copyright (c) 2025 Serhii Pievniev

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

// clang-format off
/*
=======================================================================================================================
                                                    DOCUMENTATION
=======================================================================================================================

args v2.0.0 - Command-line argument parser for C/C++
Github: https://github.com/spevnev/args

1. Introduction
===============

args is a simple single-header library for parsing command-line arguments in C/C++.
To use the library, simply include "args.h" in the file with the main function.
Note that the header must be included only once.

1.1 Requirements
----------------

- C99 or C++11
- Tested on Linux, macOS, and Windows

1.2 C++ usage
-------------

The documentation is written for C, and the C++ API is almost identical except:
- `Args` become `ArgsCpp`, functions become methods
- Freeing is no longer needed
- Options return a reference to an object containing the value
- Named arguments become builder-style methods

1.3 Quick start
---------------

This is a basic example of library usage. See the repository for more examples.

    ```
    #include "args.h"

    int main(int argc, char **argv) {
        // Initialize library.
        Args a = {0};

        // Define options.
        const long *num = option_long(&a, "long", "A long option", .default_value = 5);
        const char **str = option_string(&a, "string", "A string option", .required = true);

        // Parse arguments.
        char **positional_args;
        int positional_args_length = parse_args(&a, argc, argv, &positional_args);

        // Handle the positional arguments.
        printf("Positional arguments:");
        for (int i = 0; i < positional_args_length; i++) printf(" %s", positional_args[i]);
        printf("\n");

        // Use option values.
        printf("num=%ld str=%s\n", *num, *str);

        // Free library.
        free_args(&a);
        return EXIT_SUCCESS;
    }
    ```


2. Error handling
=================

The library handles errors by printing to stderr and exiting immediately.
This prioritizes API simplicity over error recovery by guaranteeing that all
library functions return valid results or do not return.

As such, argument parsing should happen at the beginning of the program, and
everything requiring proper error recovery should be placed after.


3. Library state
================

To initialize the library, create a zero-initialized variable of type `Args`.
It stores the entire library state and must be passed by pointer as the first
argument to every library function.
At the end, all allocated memory must be freed using `free_args`.

The library can be configured through `#define`s before the include as follows:

    ```
    #define ARGS_DISABLE_COMPLETION
    #define ARGS_PADDING 4
    #include "args.h"
    ```

Table of options:

| Option                      | Description                                                  | Default     |
|-----------------------------|--------------------------------------------------------------|-------------|
| ARGS_DISABLE_COMPLETION     | Disable shell completion                                     | not defined |
| ARGS_ALLOW_OVERWRITING      | Allow overwriting option instead of exiting with an error    | not defined |
| ARGS_PADDING                | Number of spaces between the option name and its description | 2           |
| ARGS_LINE_LENGTH            | Line length                                                  | 80          |
| ARGS_MIN_DESCRIPTION_LENGTH | Minimum description length regardless of `ARGS_LINE_LENGTH`  | 30          |
| ARGS_HIDE_DEFAULTS          | Hide default values in `print_options`                       | not defined |


4. Options
==========

Options are defined using functions of the form `option_TYPE`, which return a
non-NULL pointer to the value that will be set after parsing the arguments.

Option types: long, float, string, path, flag, enum, enum_string.

These functions accept `long_name`, `description` and optional named arguments.
`long_name` is a non-NULL string consisting of alphanumerics and '-', '+', '_'.
`description` is a string of printable ASCII characters, or NULL.

Named arguments use designated initializers (i.e. `.name = value`) to provide a
convenient way of specifying multiple optional arguments:

    ```
    option_long(&a, "long1", "description");
    option_long(&a, "long2", "description", .required = true);
    option_long(&a, "long3", "description", .short_name = 'l', .default_value = 123);
    ```

Enum options also require `values`, a NULL-terminated array of strings:

    ```
    option_enum(&a, "enum", "description", ((const char *[]) {"some", "enum", "values", NULL}));
    option_enum_string(&a, "enum-str", "description", ((const char *[]) {"other", "enum", "values", NULL}));
    ```

Table of named arguments:

| Argument      | Type        | Description                                                        | Default   | Option types    |
|---------------|-------------|--------------------------------------------------------------------|-----------|-----------------|
| short_name    | char        | Short name (e.g. `-s`). An alphanumeric char, or '\0' for none.    | '\0'      | All             |
| hidden        | bool        | Hide in `print_options`, don't provide shell completions.          | false     | All             |
|               |             | Intended for deprecated or experimental options.                   |           |                 |
| required      | bool        | Require option value to be set. When a required option is missing  | false     | All except flag |
|               |             | `parse_args` will print an error message to stderr and exit.       |           |                 |
| default_value | option type | Default option value. No effect on required options.               | 0 or NULL | All except flag |
| early_exit    | bool        | If set, `parse_args` will exit as soon as the option is found.     | false     | flag            |
|               |             | If found, the other options are not validated or set. It must be   |           |                 |
|               |             | handled before accessing other options, intended for command-like  |           |                 |
|               |             | flags that ignore required/invalid options, e.g. help and version. |           |                 |


5. Parsing arguments
====================

After options are defined, the arguments can be parsed using `parse_args`.
It takes `argc`, `argv`, and NULL-able pointer to `char** positional_args`.
Returns the number of positional arguments and sets `positional_args` to the
array of strings from `argv`, if it isn't NULL.

The function parses the arguments, validates the options and sets their values.
In case of an error, such as an unknown option or invalid value type, it prints
an error message to stderr and exits.

The following option syntaxes are supported:
--long value
--long=value
-s value
-svalue
Additionally, short options can be stacked when all preceding ones are flags:
-abcvalue <=> -a -b -c value

5.1 Shell completion
--------------------

The library provides shell completion for bash, zsh, and fish.

Suggestions are generated dynamically, which has two implications:
1. The completion scripts don't need to be updated.
2. The executable must be in the user's PATH.

Completion is implemented by handling predefined commands in `parse_args`:
`completion <bash|zsh|fish>` prints a completion script and exits.
`__complete`, used by the completion script, prints suggestions and exits.

Since scripts and suggestions are printed to stdout, there must be no other
output prior to calling `parse_args`.

The scripts should be installed via a build system or package manager.
Additionally, the `completion` command can be documented in the help message
to allow manual installation.

Commands for system-wide installation (requires root):

    ```
    PROGRAM_NAME completion bash > /usr/share/bash-completion/completions/PROGRAM_NAME
    PROGRAM_NAME completion zsh > /usr/share/zsh/site-functions/_PROGRAM_NAME
    PROGRAM_NAME completion fish > /usr/share/fish/vendor_completions.d/PROGRAM_NAME.fish
    ```


6. Printing help
================

The library provides `print_options`, which prints the following information:
- short name, if defined
- long name
- description, word-wrapped and split on `\n`
- default value, if not required

For example, here's `print_options` output for the following options:

    ```
    option_long(&a, "long", "A long option", .default_value = 5, .short_name = 'l');
    option_float(&a, "float", "A float option", .required = true);
    option_string(&a, "str", "A string option", .short_name = 's', .default_value = "string default");
    option_path(&a, "path", "A path option");
    ```
    ```
    Options:
      -l, --long      A long option (default: 5)
          --float     A float option
      -s, --str       A string option (default: "string default")
          --path      A path option (default: none)
    ```

The rest of the help message (e.g. title, program description, usage) must be
printed by the user.

Help and version flags can be either handled manually using `option_flag` or
through the convenience wrappers:
`option_help` calls the provided callback when `-h` or `--help` is set.
`option_version` prints the provided string when `-v` or `--version` is set.

    ```
    static void print_help(Args *a, const char *program_name) {
        printf("%s - Example of using 'args' library\n", program_name);
        printf("\n");
        printf("Usage:\n");
        printf("  %s [options]\n", program_name);
        printf("  %s some_command <arg1|arg2>\n", program_name);
        printf("\n");
        print_options(a, stdout);
    }

    int main(int argc, char **argv) {
        Args a = {0};
        option_help(&a, print_help);
        ...
    }
    ```
*/
// clang-format on

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

#ifndef ARGS_MIN_DESCRIPTION_LENGTH
#define ARGS_MIN_DESCRIPTION_LENGTH 30
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
    char *long_name;
    char *description;
    char short_name;
    bool is_required;
    bool is_set;
    bool is_matching;
    bool is_hidden;
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
        struct {
            bool value;
            bool early_exit;
            bool is_help;
            bool is_version;
        } bool_;
        struct {
            char **values;
            unsigned int length;
            union {
                size_t index;
                const char *value;
            } as;
        } enum_;
    } as;
} Args__Option;

typedef struct Args Args;

struct Args {
    Args__Option *head;
    Args__Option *tail;
    char **positional_args;
    bool are_parsed;
    bool options_have_short_name;
    size_t options_max_name_length;
    void (*help_callback)(Args *, const char *);
    char *version_string;
};

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

// clang-format off
#ifdef __cplusplus
#define ARGS__ZERO_INIT {}
#else
#define ARGS__ZERO_INIT {0}
#endif
// clang-format on

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

static void args__set_enum_values(Args__Option *option, const char **values) {
    ARGS__ASSERT(option != NULL && values != NULL);

    size_t length = 0;
    while (values[length] != NULL) length++;
    option->as.enum_.length = length;

    char **copy = (char **) malloc(sizeof(*copy) * length);
    if (copy == NULL) ARGS__OUT_OF_MEMORY();
    option->as.enum_.values = copy;

    for (size_t i = 0; i < length; i++) {
        for (const char *c = values[i]; *c != '\0'; c++) {
            if (!isprint(*c)) {
                ARGS__FATAL(
                    "Enum value of \"%s\" contains an invalid character 0x%x. "
                    "Maybe enum array is missing a NULL-terminator",
                    option->long_name,
                    *c
                );
            }
        }
        copy[i] = args__strdup(values[i]);
    }
}

static Args__Option *args__new_option(
    Args *a,
    const char *long_name,
    const char *description,
    char short_name,
    bool is_required,
    bool is_hidden,
    Args__Type type
) {
    ARGS__ASSERT(a != NULL);

    if (a->are_parsed) ARGS__FATAL("New options cannot be added after parsing the arguments");

    if (short_name != '\0' && !isalnum(short_name)) {
        ARGS__FATAL("Invalid short name '%c'. It must be alphanumeric", short_name);
    }

    if (long_name == NULL) ARGS__FATAL("Option must have a long name");
    for (const char *c = long_name; *c != '\0'; c++) {
        if (isalnum(*c) || *c == '_' || *c == '+') continue;
        if (*c == '-' && c != long_name) continue;
        ARGS__FATAL(
            "Invalid long name \"%s\". It must consist of alphanumerics and \"-_+\", and cannot start with '-'",
            long_name
        );
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
    memset(option, 0, sizeof(*option));

    option->next = NULL;
    option->short_name = short_name;
    option->long_name = args__strdup(long_name);
    option->description = description == NULL ? NULL : args__strdup(description);
    option->is_required = is_required;
    option->is_hidden = is_hidden;
    option->is_set = false;
    option->is_matching = false;
    option->type = type;

    if (!option->is_hidden) {
        size_t name_length = strlen(option->long_name);
        if (name_length > a->options_max_name_length) a->options_max_name_length = name_length;
        if (short_name != '\0') a->options_have_short_name = true;
    }

    if (a->head == NULL) {
        a->head = option;
    } else {
        a->tail->next = option;
    }
    a->tail = option;
    return option;
}

#define ARGS__GET_FIRST_(N, ...) N
#define ARGS__GET_FIRST(...) ARGS__GET_FIRST_(__VA_ARGS__, 0)
#define ARGS__GET_SECOND(N, M, ...) M
#define ARGS__DROP_TWO(N, M, ...) __VA_ARGS__

// Takes struct type and variadic arguments consisting of a sentinel, followed by zero or more designations.
// Returns a compound literal of the provided type followed by a trailing comma.
// If there are designations, uses designated initializer, otherwise struct is zero-initialized.
// clang-format off
#define ARGS__ZERO_OR_DESIGNATED(struct_type_t, ...) \
        (struct_type_t) {                            \
            ARGS__GET_SECOND(__VA_ARGS__, 0 }, ),          \
            ARGS__DROP_TWO(__VA_ARGS__, }, )
// clang-format on

#define ARGS__OPTION_COMMON_FIELDS \
    char short_name;               \
    bool hidden;

#define ARGS__OPTION_VALUE_STRUCT(default_value_t) \
    struct {                                       \
        ARGS__OPTION_COMMON_FIELDS                 \
        bool required;                             \
        default_value_t default_value;             \
    }

typedef ARGS__OPTION_VALUE_STRUCT(long) Args__OptionLongArgs;

ARGS__MAYBE_UNUSED ARGS__WARN_UNUSED_RESULT static const long *args__option_long(
    Args__OptionLongArgs args,
    Args *a,
    const char *long_name,
    const char *description
) {
    ARGS__ASSERT(a != NULL);
    Args__Option *option = args__new_option(
        a,
        long_name,
        description,
        args.short_name,
        args.required,
        args.hidden,
        ARGS__TYPE_LONG
    );
    option->default_value.long_ = option->as.long_ = args.default_value;
    return &option->as.long_;
}

typedef ARGS__OPTION_VALUE_STRUCT(float) Args__OptionFloatArgs;

ARGS__MAYBE_UNUSED ARGS__WARN_UNUSED_RESULT static const float *args__option_float(
    Args__OptionFloatArgs args,
    Args *a,
    const char *long_name,
    const char *description
) {
    ARGS__ASSERT(a != NULL);
    Args__Option *option = args__new_option(
        a,
        long_name,
        description,
        args.short_name,
        args.required,
        args.hidden,
        ARGS__TYPE_FLOAT
    );
    option->default_value.float_ = option->as.float_ = args.default_value;
    return &option->as.float_;
}

typedef ARGS__OPTION_VALUE_STRUCT(const char *) Args__OptionStringArgs;

ARGS__MAYBE_UNUSED ARGS__WARN_UNUSED_RESULT static const char **args__option_string(
    Args__OptionStringArgs args,
    Args__Type type,
    Args *a,
    const char *long_name,
    const char *description
) {
    ARGS__ASSERT((type == ARGS__TYPE_STRING || type == ARGS__TYPE_PATH) && a != NULL);
    Args__Option *option = args__new_option(
        a,
        long_name,
        description,
        args.short_name,
        args.required,
        args.hidden,
        type
    );
    if (args.default_value != NULL) {
        option->default_value.string = args__strdup(args.default_value);
        option->as.string = args__strdup(args.default_value);
    }
    return (const char **) &option->as.string;
}

typedef struct {
    ARGS__OPTION_COMMON_FIELDS
    // Checked before all other options. When present, `parse_args` exits early
    // without validating other options or setting their values.
    // As such, it must be handled before accessing other option values.
    bool early_exit;
} Args__OptionFlagArgs;

ARGS__MAYBE_UNUSED ARGS__WARN_UNUSED_RESULT static const bool *args__option_flag(
    Args__OptionFlagArgs args,
    Args *a,
    const char *long_name,
    const char *description
) {
    ARGS__ASSERT(a != NULL);
    Args__Option *option = args__new_option(
        a,
        long_name,
        description,
        args.short_name,
        false,
        args.hidden,
        ARGS__TYPE_BOOL
    );
    option->as.bool_.early_exit = args.early_exit;
    return &option->as.bool_.value;
}

typedef ARGS__OPTION_VALUE_STRUCT(size_t) Args__OptionEnumArgs;

ARGS__MAYBE_UNUSED ARGS__WARN_UNUSED_RESULT static const size_t *args__option_enum(
    Args__OptionEnumArgs args,
    Args *a,
    const char *long_name,
    const char *description,
    const char **values
) {
    ARGS__ASSERT(a != NULL && values != NULL);
    Args__Option *option = args__new_option(
        a,
        long_name,
        description,
        args.short_name,
        args.required,
        args.hidden,
        ARGS__TYPE_ENUM_INDEX
    );
    args__set_enum_values(option, values);
    option->as.enum_.as.index = args.default_value;
    if (args.default_value < option->as.enum_.length) {
        option->default_value.enum_ = args__strdup(values[args.default_value]);
    }
    return &option->as.enum_.as.index;
}

typedef ARGS__OPTION_VALUE_STRUCT(const char *) Args__OptionEnumStringArgs;

ARGS__MAYBE_UNUSED ARGS__WARN_UNUSED_RESULT static const char **args__option_enum_string(
    Args__OptionEnumStringArgs args,
    Args *a,
    const char *long_name,
    const char *description,
    const char **values
) {
    ARGS__ASSERT(a != NULL && values != NULL);
    Args__Option *option = args__new_option(
        a,
        long_name,
        description,
        args.short_name,
        args.required,
        args.hidden,
        ARGS__TYPE_ENUM_STRING
    );
    if (args.default_value != NULL) option->default_value.enum_ = args__strdup(args.default_value);
    args__set_enum_values(option, values);
    option->as.enum_.as.value = option->default_value.enum_;
    return &option->as.enum_.as.value;
}

typedef struct {
    bool is_short;
    union {
        char short_name;
        struct {
            const char *start;
            size_t length;
        } long_;
    } as;
    const char *arg;
    const char *value;
} Args__ParsedOption;

typedef struct {
    Args__ParsedOption *data;
    size_t length;
    size_t capacity;
} Args__ParsedOptionVec;

static void args__parsed_vec_push(Args__ParsedOptionVec *vec, Args__ParsedOption element) {
    ARGS__ASSERT(vec != NULL);

    const size_t INITIAL_VECTOR_CAPACITY = 8;
    if (vec->length >= vec->capacity) {
        vec->capacity = vec->capacity == 0 ? INITIAL_VECTOR_CAPACITY : vec->capacity * 2;
        vec->data = (Args__ParsedOption *) realloc(vec->data, vec->capacity * sizeof(*vec->data));
        if (vec->data == NULL) ARGS__OUT_OF_MEMORY();
    }
    vec->data[vec->length++] = element;
}

static Args__Option *args__find_option(Args *a, Args__ParsedOption option) {
    ARGS__ASSERT(a != NULL);
    for (Args__Option *i = a->head; i != NULL; i = i->next) {
        if (option.is_short) {
            if (i->short_name == option.as.short_name) return i;
        } else {
            if (strncmp(i->long_name, option.as.long_.start, option.as.long_.length) == 0) return i;
        }
    }
    return NULL;
}

static void args__parse_value(Args__Option *option, const char *value) {
    ARGS__ASSERT(option != NULL && value != NULL);
    switch (option->type) {
        case ARGS__TYPE_LONG: {
            char *end = NULL;
            option->as.long_ = strtol(value, &end, 0);
            if (end == NULL || *end != '\0') ARGS__FATAL("Invalid integer \"%s\"", value);
        } break;
        case ARGS__TYPE_FLOAT: {
            char *end = NULL;
            option->as.float_ = strtof(value, &end);
            if (end == NULL || *end != '\0') ARGS__FATAL("Invalid float \"%s\"", value);
        } break;
        case ARGS__TYPE_STRING:
        case ARGS__TYPE_PATH:
            free(option->as.string);
            option->as.string = args__strdup(value);
            break;
        case ARGS__TYPE_BOOL: ARGS__UNREACHABLE(); break;
        case ARGS__TYPE_ENUM_INDEX:
        case ARGS__TYPE_ENUM_STRING:
            for (size_t i = 0; i < option->as.enum_.length; i++) {
                if (strcasecmp(option->as.enum_.values[i], value) != 0) continue;

                if (option->type == ARGS__TYPE_ENUM_INDEX) {
                    option->as.enum_.as.index = i;
                } else {
                    option->as.enum_.as.value = option->as.enum_.values[i];
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
        program_name,
        program_name,
        program_name,
        program_name
    );
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
        program_name,
        program_name,
        program_name,
        program_name
    );
}

static void args__fish_completion_script(const char *program_name) {
    ARGS__ASSERT(program_name != NULL);
    printf(
        "complete -c %s -e\n"
        "\n"
        "for args in (%s __complete fish 2>/dev/null)\n"
        "    eval \"complete -c %s $args\"\n"
        "end\n",
        program_name,
        program_name,
        program_name
    );
}

static void args__handle_completion(int argc, char **argv, const char *program_name) {
    ARGS__ASSERT(argc >= 1 && argv != NULL && program_name != NULL);

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
            if (i->is_hidden) i->is_matching = false;
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
                if (print_long) length += 2;
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
                if (i->description != NULL) {
                    if (padding > 0) {
                        printf("%*c", padding, ' ');
                        length += padding;
                    }

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
            case ARGS__TYPE_STRING:
            case ARGS__TYPE_BOOL:   printf("-\n"); break;
            case ARGS__TYPE_PATH:   break;
            case ARGS__TYPE_ENUM_INDEX:
            case ARGS__TYPE_ENUM_STRING:
                for (size_t i = 0; i < option->as.enum_.length; i++) {
                    const char *value = option->as.enum_.values[i];
                    if (strlen(value) >= cur_length && strncasecmp(value, cur, cur_length) == 0) {
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
            for (size_t i = 0; i < option->as.enum_.length; i++) {
                if (i > 0) printf(" ");
                args__completion_print_escaped(option->as.enum_.values[i], " :()");
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
        if (i->is_hidden) continue;
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

static void args__completion_print_escaped_fish(const char *string) {
    ARGS__ASSERT(string != NULL);
    for (const char *c = string; *c != '\0'; c++) {
        if (*c == '\n') {
            printf(" ");
        } else {
            if (*c == '\'' || *c == '\\') printf("\\\\\\");
            if (strchr(" $\"", *c) != NULL) printf("\\");
            printf("%c", *c);
        }
    }
}

static void args__fish_complete(Args *a) {
    ARGS__ASSERT(a != NULL);
    for (Args__Option *i = a->head; i != NULL; i = i->next) {
        if (i->is_hidden) continue;
        printf("-l %s -%c", i->long_name, i->type == ARGS__TYPE_PATH ? 'F' : 'f');
        if (i->type == ARGS__TYPE_ENUM_INDEX || i->type == ARGS__TYPE_ENUM_STRING) {
            printf(" -a '");
            for (size_t j = 0; j < i->as.enum_.length; j++) {
                if (j > 0) printf(" ");
                args__completion_print_escaped_fish(i->as.enum_.values[j]);
            }
            printf("'");
        }
        if (i->short_name != '\0') {
            // Use '-r' to avoid stacking short options.
            printf(" -s %c -r", i->short_name);
        }
        if (i->description != NULL) {
            printf(" -d '");
            args__completion_print_escaped(i->description, "'");
            printf("'");
        }
        printf("\n");
    }
}

static void args__handle_complete(Args *a, int argc, char **argv) {
    ARGS__ASSERT(a != NULL && argc >= 1 && argv != NULL);

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
                free(cur->as.string);
                break;
            case ARGS__TYPE_ENUM_STRING:
            case ARGS__TYPE_ENUM_INDEX:
                free(cur->default_value.enum_);
                for (size_t i = 0; i < cur->as.enum_.length; i++) free(cur->as.enum_.values[i]);
                free(cur->as.enum_.values);
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

    free(a->version_string);
}

// Parses the arguments and sets option values.
// Handles shell completions, must be called before printing to stdout.
// Returns positional arguments via `positional_args`, and their count as return value.
// Elements are from `argv`. The array memory is managed by the library.
static int parse_args(Args *a, int argc, char **argv, char ***positional_args) {
    ARGS__ASSERT(a != NULL && argv != NULL);
    a->are_parsed = true;

    ARGS__ASSERT(argc >= 0);
    if (argc == 0) ARGS__FATAL("Expected the first argument to be a program name");
    ARGS__MAYBE_UNUSED const char *program_name = args__basename(argv[0]);
    argc--;
    argv++;

    // Check duplicate options.
    for (Args__Option *i = a->head; i != NULL; i = i->next) {
        for (Args__Option *j = i->next; j != NULL; j = j->next) {
            if (i->short_name == j->short_name && i->short_name != '\0') {
                ARGS__FATAL(
                    "Duplicate short name '%c' in options \"%s\" and \"%s\"",
                    i->short_name,
                    i->long_name,
                    j->long_name
                );
            }

            if (strcmp(i->long_name, j->long_name) == 0) ARGS__FATAL("Duplicate option \"%s\"", i->long_name);
        }
    }

#ifndef ARGS_DISABLE_COMPLETION
    // Handle shell completion.
    if (argc >= 1) {
        if (strcmp(argv[0], "completion") == 0) {
            args__handle_completion(argc, argv, program_name);
            free_args(a);
            exit(EXIT_SUCCESS);
        } else if (strcmp(argv[0], "__complete") == 0) {
            args__handle_complete(a, argc, argv);
            free_args(a);
            exit(EXIT_SUCCESS);
        }
    }
#endif

    int positional_args_index = 0;
    if (positional_args != NULL) {
        a->positional_args = (char **) malloc(sizeof(*argv) * argc);
        if (a->positional_args == NULL) ARGS__OUT_OF_MEMORY();
        *positional_args = a->positional_args;
    }

    // Parse arguments without validation and errors.
    Args__ParsedOptionVec parsed = ARGS__ZERO_INIT;
    while (argc > 0) {
        char *arg = *argv++;
        argc--;

        size_t arg_length = strlen(arg);
        if (arg_length <= 1 || arg[0] != '-') {
            if (positional_args != NULL) (*positional_args)[positional_args_index] = arg;
            positional_args_index++;
            continue;
        }

        const char *full_arg = arg;
        if (arg[1] == '-') {
            arg += 2;
            arg_length -= 2;

            const char *equals = strchr(arg, '=');
            if (equals != NULL) arg_length = equals - arg;

            Args__ParsedOption cur = ARGS__ZERO_INIT;
            cur.as.long_.start = arg;
            cur.as.long_.length = arg_length;
            cur.arg = full_arg;

            if (equals != NULL) {
                cur.value = equals + 1;
            } else if (argc > 0) {
                Args__Option *option = args__find_option(a, cur);
                if (option != NULL && option->type != ARGS__TYPE_BOOL) {
                    cur.value = *argv++;
                    argc--;
                }
            }

            args__parsed_vec_push(&parsed, cur);
        } else {
            arg++;
            arg_length--;
            while (arg_length > 0) {
                char ch = *arg++;
                arg_length--;

                Args__ParsedOption cur = ARGS__ZERO_INIT;
                cur.is_short = true;
                cur.as.short_name = ch;
                cur.arg = full_arg;

                Args__Option *option = args__find_option(a, cur);
                if (option == NULL || option->type != ARGS__TYPE_BOOL) {
                    if (arg_length > 0) {
                        cur.value = arg;
                        arg_length = 0;
                    } else if (argc > 0) {
                        cur.value = *argv++;
                        argc--;
                    }
                }

                args__parsed_vec_push(&parsed, cur);
            }
        }
    }

    // Check help, version, and early_exit.
    for (size_t i = 0; i < parsed.length; i++) {
        Args__Option *option = args__find_option(a, parsed.data[i]);
        if (option == NULL) continue;
        if (option->type != ARGS__TYPE_BOOL) continue;

        if (option->as.bool_.is_help) {
            ARGS__ASSERT(a->help_callback != NULL);
            a->help_callback(a, program_name);
            free_args(a);
            exit(EXIT_SUCCESS);
        }

        if (option->as.bool_.is_version) {
            ARGS__ASSERT(a->version_string != NULL);
            printf("%s\n", a->version_string);
            free_args(a);
            exit(EXIT_SUCCESS);
        }

        if (option->as.bool_.early_exit) {
            option->as.bool_.value = true;
            free(parsed.data);
            return positional_args_index;
        }
    }

    // Validate parsed and set their values.
    for (size_t i = 0; i < parsed.length; i++) {
        Args__ParsedOption cur = parsed.data[i];
        Args__Option *option = args__find_option(a, cur);

        if (option == NULL) {
            if (cur.is_short && strlen(cur.arg) > 2) {
                ARGS__FATAL("Unknown or invalid option '%c' in \"%s\"", cur.as.short_name, cur.arg);
            } else {
                ARGS__FATAL("Unknown or invalid option \"%s\"", cur.arg);
            }
        }

#ifndef ARGS_ALLOW_OVERWRITING
        if (option->is_set) ARGS__FATAL("Option \"%s\" is already set: \"%s\"", option->long_name, cur.arg);
#endif
        option->is_set = true;

        if (option->type == ARGS__TYPE_BOOL) {
            if (cur.value != NULL) ARGS__FATAL("Flags cannot have a value: \"%s\"", cur.arg);
            option->as.bool_.value = true;
        } else {
            if (cur.value == NULL) ARGS__FATAL("Option \"%s\" is missing a value", cur.arg);
            args__parse_value(option, cur.value);
        }
    }

    // Check required.
    for (Args__Option *option = a->head; option != NULL; option = option->next) {
        if (option->is_required && !option->is_set) {
            ARGS__FATAL("Missing a required option \"%s\"", option->long_name);
        }
    }

    free(parsed.data);
    return positional_args_index;
}

ARGS__MAYBE_UNUSED static void print_options(Args *a, FILE *fp) {
    ARGS__ASSERT(a != NULL && fp != NULL);

    int offset = 4 + a->options_max_name_length + ARGS_PADDING;
    if (a->options_have_short_name) offset += 4;

    fprintf(fp, "Options:\n");
    for (Args__Option *option = a->head; option != NULL; option = option->next) {
        if (option->is_hidden) continue;

#ifdef ARGS_HIDE_DEFAULTS
        bool print_defaults = false;
#else
        bool print_defaults = !option->is_required;
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
            if (line_length < ARGS_MIN_DESCRIPTION_LENGTH) line_length = ARGS_MIN_DESCRIPTION_LENGTH;

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

// On "-h" or "--help", calls `help_callback` and exits.
// Help flag is checked before validating other options.
ARGS__MAYBE_UNUSED static void option_help(Args *a, void (*help_callback)(Args *a, const char *program_name)) {
    ARGS__ASSERT(a != NULL && help_callback != NULL);
    if (a->are_parsed) ARGS__FATAL("New options cannot be added after parsing the arguments");
    a->help_callback = help_callback;

    Args__Option *option = args__new_option(a, "help", "Show help", 'h', false, false, ARGS__TYPE_BOOL);
    option->as.bool_.is_help = true;
}

// On "-v" or "--version", prints `version_string` to stdout and exits.
// Version flag is checked before validating other options.
ARGS__MAYBE_UNUSED static void option_version(Args *a, const char *version_string) {
    ARGS__ASSERT(a != NULL && version_string != NULL);
    if (a->are_parsed) ARGS__FATAL("New options cannot be added after parsing the arguments");
    a->version_string = args__strdup(version_string);

    Args__Option *option = args__new_option(a, "version", "Print version", 'v', false, false, ARGS__TYPE_BOOL);
    option->as.bool_.is_version = true;
}

#ifndef __cplusplus

// Defines a long option, returns a pointer set by `parse_args`.
#define option_long(a, long_name, ...)                                 \
    args__option_long(                                                 \
        ARGS__ZERO_OR_DESIGNATED(Args__OptionLongArgs, __VA_ARGS__) a, \
        long_name,                                                     \
        ARGS__GET_FIRST(__VA_ARGS__)                                   \
    )

// Defines a float option, returns a pointer set by `parse_args`.
#define option_float(a, long_name, ...)                                 \
    args__option_float(                                                 \
        ARGS__ZERO_OR_DESIGNATED(Args__OptionFloatArgs, __VA_ARGS__) a, \
        long_name,                                                      \
        ARGS__GET_FIRST(__VA_ARGS__)                                    \
    )

// Defines a string option, returns a pointer set by `parse_args`.
// The value is NULL only if the default is NULL and the option is not set.
// String memory is managed by the library.
#define option_string(a, long_name, ...)                                                 \
    args__option_string(                                                                 \
        ARGS__ZERO_OR_DESIGNATED(Args__OptionStringArgs, __VA_ARGS__) ARGS__TYPE_STRING, \
        a,                                                                               \
        long_name,                                                                       \
        ARGS__GET_FIRST(__VA_ARGS__)                                                     \
    )

// Same as `option_string` except that shell completion suggests paths.
// Does NOT check that the value is a valid path.
#define option_path(a, long_name, ...)                                                 \
    args__option_string(                                                               \
        ARGS__ZERO_OR_DESIGNATED(Args__OptionStringArgs, __VA_ARGS__) ARGS__TYPE_PATH, \
        a,                                                                             \
        long_name,                                                                     \
        ARGS__GET_FIRST(__VA_ARGS__)                                                   \
    )

// Defines a flag, returns a pointer set by `parse_args`.
#define option_flag(a, long_name, ...)                                 \
    args__option_flag(                                                 \
        ARGS__ZERO_OR_DESIGNATED(Args__OptionFlagArgs, __VA_ARGS__) a, \
        long_name,                                                     \
        ARGS__GET_FIRST(__VA_ARGS__)                                   \
    )

// Defines an enum option, returns a pointer set by `parse_args`.
// The value is either a valid index or the default value.
// `values` must be a NULL-terminated array.
// `values` are matched case-insensitively.
// If `values` are defined in the arguments, they must be wrapped in parenthesis.
#define option_enum(a, long_name, description, ...)                    \
    args__option_enum(                                                 \
        ARGS__ZERO_OR_DESIGNATED(Args__OptionEnumArgs, __VA_ARGS__) a, \
        long_name,                                                     \
        description,                                                   \
        ARGS__GET_FIRST(__VA_ARGS__)                                   \
    )

// Same as `option_enum` except that the value is either a string from `values`
// or the default value.
// String memory is managed by the library.
#define option_enum_string(a, long_name, description, ...)                   \
    args__option_enum_string(                                                \
        ARGS__ZERO_OR_DESIGNATED(Args__OptionEnumStringArgs, __VA_ARGS__) a, \
        long_name,                                                           \
        description,                                                         \
        ARGS__GET_FIRST(__VA_ARGS__)                                         \
    )

#else  // __cplusplus

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class ArgsCpp {
private:
    class Buildable {
    public:
        virtual ~Buildable() = default;

    protected:
        bool is_built{false};

    private:
        friend ArgsCpp;

        virtual void build() = 0;
    };

    template <typename T, typename R>
    class BaseOption : public Buildable {
    public:
        // Delete constructors to force user to keep a reference.
        BaseOption(const BaseOption &) = delete;
        BaseOption &operator=(const BaseOption &) = delete;
        BaseOption(BaseOption &&) = delete;
        BaseOption &operator=(BaseOption &&) = delete;

        R &short_name(char short_name) {
            m_short_name = short_name;
            return static_cast<R &>(*this);
        }

        R &hidden() {
            m_hidden = true;
            return static_cast<R &>(*this);
        }

        const T &value() const {
            if (!is_built) ARGS__FATAL("Option values cannot be accessed before `parse_args()`");
            return *m_value;
        }

        operator const T &() const { return value(); }

    protected:
        std::shared_ptr<Args> args;
        const char *long_name;
        const char *description;
        char m_short_name{'\0'};
        bool m_hidden{false};
        const T *m_value{nullptr};

    private:
        friend ArgsCpp;
        friend R;

        BaseOption(const std::shared_ptr<Args> &args, const char *long_name, const char *description)
            : args(args), long_name(long_name), description(description) {}
    };

    template <typename T, typename R>
    class ValueOption : public BaseOption<T, R> {
    public:
        R &required() {
            m_required = true;
            return static_cast<R &>(*this);
        }

        R &default_value(T default_value) {
            m_default_value = default_value;
            return static_cast<R &>(*this);
        }

    protected:
        bool m_required{false};
        T m_default_value{};

    private:
        friend R;

        ValueOption() = default;
        using BaseOption<T, R>::BaseOption;
    };

    template <typename T, typename R>
    class EnumOption : public ValueOption<T, R> {
    protected:
        const char **values{nullptr};

    private:
        friend R;
        friend ArgsCpp;

        EnumOption(
            const std::shared_ptr<Args> &args,
            const char *long_name,
            const char *description,
            const char **values
        )
            : ValueOption<T, R>(args, long_name, description), values(values) {}
    };

    std::shared_ptr<Args> args;
    std::vector<std::unique_ptr<Buildable> > options{};
    std::function<void(ArgsCpp &, const char *)> help_callback{nullptr};

    static ArgsCpp *instance;

    static std::shared_ptr<Args> init_args() {
        return std::shared_ptr<Args>(new Args{}, [](Args *a) {
            free_args(a);
            delete a;
        });
    }

    static void callback_bridge(Args *c_args, const char *program_name) {
        ARGS__ASSERT(instance != nullptr && instance->args.get() == c_args);
        instance->help_callback(*instance, program_name);
    }

    void build_options() {
        for (const auto &option : options) {
            option->build();
            option->is_built = true;
        }
    }

public:
    class OptionLong : public ValueOption<long, OptionLong> {
    private:
        using ValueOption::ValueOption;

        void build() override {
            Args__OptionLongArgs option_args;
            option_args.short_name = m_short_name;
            option_args.required = m_required;
            option_args.hidden = m_hidden;
            option_args.default_value = m_default_value;
            m_value = args__option_long(option_args, args.get(), long_name, description);
        }
    };

    class OptionFloat : public ValueOption<float, OptionFloat> {
    private:
        using ValueOption::ValueOption;

        void build() override {
            Args__OptionFloatArgs option_args;
            option_args.short_name = m_short_name;
            option_args.required = m_required;
            option_args.hidden = m_hidden;
            option_args.default_value = m_default_value;
            m_value = args__option_float(option_args, args.get(), long_name, description);
        }
    };

    class OptionString : public ValueOption<const char *, OptionString> {
    public:
        std::string string() const { return std::string{value()}; }

        operator std::string() const { return string(); }

    private:
        friend ArgsCpp;

        Args__Type type;

        OptionString(const std::shared_ptr<Args> &args, const char *long_name, const char *description, Args__Type type)
            : ValueOption(args, long_name, description), type(type) {}

        void build() override {
            Args__OptionStringArgs option_args;
            option_args.short_name = m_short_name;
            option_args.required = m_required;
            option_args.hidden = m_hidden;
            option_args.default_value = m_default_value;
            m_value = args__option_string(option_args, type, args.get(), long_name, description);
        }
    };

    class OptionFlag : public BaseOption<bool, OptionFlag> {
    public:
        // Checked before all other options. When present, `parse_args` exits
        // early without validating other options or setting their values.
        // As such, it must be handled before accessing other option values.
        OptionFlag &early_exit() {
            m_early_exit = true;
            return *this;
        }

    private:
        bool m_early_exit{false};

        using BaseOption::BaseOption;

        void build() override {
            Args__OptionFlagArgs option_args;
            option_args.short_name = m_short_name;
            option_args.early_exit = m_early_exit;
            option_args.hidden = m_hidden;
            m_value = args__option_flag(option_args, args.get(), long_name, description);
        }
    };

    class OptionEnum : public EnumOption<size_t, OptionEnum> {
    private:
        using EnumOption::EnumOption;

        void build() override {
            Args__OptionEnumArgs option_args;
            option_args.short_name = m_short_name;
            option_args.required = m_required;
            option_args.hidden = m_hidden;
            option_args.default_value = m_default_value;
            m_value = args__option_enum(option_args, args.get(), long_name, description, values);
        }
    };

    class OptionEnumString : public EnumOption<const char *, OptionEnumString> {
    public:
        std::string string() const { return std::string{value()}; }

        operator std::string() const { return string(); }

    private:
        using EnumOption::EnumOption;

        void build() override {
            Args__OptionEnumStringArgs option_args;
            option_args.short_name = m_short_name;
            option_args.required = m_required;
            option_args.hidden = m_hidden;
            option_args.default_value = m_default_value;
            m_value = args__option_enum_string(option_args, args.get(), long_name, description, values);
        }
    };

    ArgsCpp() : args(init_args()) { instance = this; }

    // Explicitly delete copy constructor to improve error in case when ArgsCpp
    // is passed by value instead of by reference (e.g. help callback).
    ArgsCpp(const ArgsCpp &) = delete;
    ArgsCpp &operator=(const ArgsCpp &) = delete;
    ArgsCpp(ArgsCpp &&) = default;
    ArgsCpp &operator=(ArgsCpp &&) = default;

    // On "-h" or "--help", calls the callback (can be lambda) and exits.
    // Help flag is checked before validating other options.
    void option_help(std::function<void(ArgsCpp &args, const char *program_name)> help_callback) {
        this->help_callback = help_callback;
        ::option_help(args.get(), callback_bridge);
    }

    // On "-v" or "--version", prints `version_string` to stdout and exits.
    // Version flag is checked before validating other options.
    void option_version(const char *version_string) { ::option_version(args.get(), version_string); }

    // Returns a reference to a long option, the value is set by `parse_args`.
    ARGS__WARN_UNUSED_RESULT OptionLong &option_long(const char *long_name, const char *description) {
        auto *option = new OptionLong(args, long_name, description);
        options.emplace_back(option);
        return *option;
    }

    // Returns a reference to a float option, the value is set by `parse_args`.
    ARGS__WARN_UNUSED_RESULT OptionFloat &option_float(const char *long_name, const char *description) {
        auto *option = new OptionFloat(args, long_name, description);
        options.emplace_back(option);
        return *option;
    }

    // Returns a reference to a string option, the value is set by `parse_args`.
    // The value is nullptr only if the default is nullptr and the option is not set.
    ARGS__WARN_UNUSED_RESULT OptionString &option_string(const char *long_name, const char *description) {
        auto *option = new OptionString(args, long_name, description, ARGS__TYPE_STRING);
        options.emplace_back(option);
        return *option;
    }

    // Same as `option_string` except that shell completion suggests paths.
    // Does NOT check that the value is a valid path.
    ARGS__WARN_UNUSED_RESULT OptionString &option_path(const char *long_name, const char *description) {
        auto *option = new OptionString(args, long_name, description, ARGS__TYPE_PATH);
        options.emplace_back(option);
        return *option;
    }

    // Returns a reference to a flag, the value is set by `parse_args`.
    ARGS__WARN_UNUSED_RESULT OptionFlag &option_flag(const char *long_name, const char *description) {
        auto *option = new OptionFlag(args, long_name, description);
        options.emplace_back(option);
        return *option;
    }

    // Returns a reference to an enum option, the value is set by `parse_args`.
    // The value is either a valid index or the default value.
    // `values` must be a nullptr-terminated array.
    // `values` are matched case-insensitively.
    ARGS__WARN_UNUSED_RESULT OptionEnum &option_enum(
        const char *long_name,
        const char *description,
        const char **values
    ) {
        auto *option = new OptionEnum(args, long_name, description, values);
        options.emplace_back(option);
        return *option;
    }

    // Same as `option_enum` except that the value is either a string from
    // `values` or the default value.
    ARGS__WARN_UNUSED_RESULT OptionEnumString &option_enum_string(
        const char *long_name,
        const char *description,
        const char **values
    ) {
        auto *option = new OptionEnumString(args, long_name, description, values);
        options.emplace_back(option);
        return *option;
    }

    // Parses the arguments and sets option values.
    // Handles shell completions, must be called before printing to stdout.
    int parse_args(int argc, char **argv) {
        build_options();
        return ::parse_args(args.get(), argc, argv, nullptr);
    }

    // Parses the arguments and sets option values.
    // Handles shell completions, must be called before printing to stdout.
    // Returns positional arguments via `positional_args`, and their count as return value.
    int parse_args(int argc, char **argv, char **&positional_args) {
        build_options();
        return ::parse_args(args.get(), argc, argv, &positional_args);
    }

    void print_options(FILE *fp = stdout) { ::print_options(args.get(), fp); }
};

ArgsCpp *ArgsCpp::instance = nullptr;

#endif  // __cplusplus

#undef ARGS__HAS_ATTRIBUTE
#undef ARGS__MAYBE_UNUSED
#undef ARGS__WARN_UNUSED_RESULT
#undef ARGS__ZERO_INIT
#undef ARGS__FATAL
#undef ARGS__OUT_OF_MEMORY
#undef ARGS__UNREACHABLE
#undef ARGS__ASSERT
#undef ARGS__OPTION_COMMON_FIELDS
#undef ARGS__OPTION_VALUE_STRUCT
