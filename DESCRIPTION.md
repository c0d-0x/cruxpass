# cruxpass internals

## Authentication

There is no stored password hash. Decryption success **is** the authentication.

1. `authenticate()` reads the salt from `meta.db` and prompts for the login password via the TUI.
2. A 256-bit key is derived using `key_gen()`, which runs Argon2id via `crypto_pwhash()`
   with `OPSLIMIT_INTERACTIVE` / `MEMLIMIT_SENSITIVE`.
3. The key is passed to SQLCipher via `sqlite3_key()` to decrypt `cruxpass.db` in memory.
4. There's no literal comparison. A wrong password derives the wrong key, and `decrypt()`
   confirms failure by running `SELECT count(*) FROM sqlite_master` on the database.

Passwords and key buffers are allocated with `sodium_malloc()` and wiped with
`sodium_memzero()` on every code path.

## SQLCipher PRAGMA Pinning

`pin_db()` explicitly sets every cipher-relevant PRAGMA on each open rather than relying
on library defaults. This matters because SQLCipher's defaults have changed across versions.

| PRAGMA                   | Value              | Why                                                                                 |
| ------------------------ | ------------------ | ----------------------------------------------------------------------------------- |
| `cipher_use_hmac`        | ON                 | Per-page HMAC catches tampered ciphertext instead of silently decrypting to garbage |
| `cipher_hmac_algorithm`  | HMAC_SHA512        | Pins the HMAC hash so it can't change silently across upgrades                      |
| `cipher_kdf_algorithm`   | PBKDF2_HMAC_SHA512 | Pins the KDF for internal page-key derivation                                       |
| `kdf_iter`               | 256000             | Pins iteration count to the stronger baseline                                       |
| `cipher_page_size`       | 4096               | Prevents a library rebuild from reinterpreting existing DBs                         |
| `cipher_memory_security` | ON                 | Zeros internal buffers holding key data and decrypted pages                         |
| `cipher_log_level`       | NONE               | Suppresses debug output that could leak cipher-state to stderr                      |

## Import Implementation

Import does not use a CSV library. It reads the file line by line with `fgets()` and
splits each line into three `str_view_t` fields (pointer + length) by scanning for
commas in place. No per-field heap allocation.

- Malformed or oversized rows are skipped with a line-numbered error.
- Valid rows go through a single prepared, bound `INSERT` statement
  (`insert_view_record`), which is reused across all rows rather than re-preparing SQL per line.

## Random Secret Generation

`init_secret_bank()` builds a character pool from up to four optional classes
(uppercase, lowercase, digits, symbols), each with a normal and an "unambiguous"
variant. The unambiguous variant drops:

| Class   | Omitted            | Reason                             |
| ------- | ------------------ | ---------------------------------- |
| Upper   | `I`, `O`           | Visually confusable with `1`, `0`  |
| Lower   | `l`                | Visually confusable with `1`       |
| Digits  | `0`                | Visually confusable with `O`       |
| Symbols | `<`, `>`, `?`, `!` | Shell interpretation / readability |

`random_secret()` draws characters one at a time using libsodium's
`randombytes_uniform()`, a CSPRNG with unbiased modulo reduction.

The same function backs both the CLI flags (`-g` with `-a/-A/-p/-s/-x`) and the
TUI keybinds (`ra`, `rA`, `rp`, `rr`, `rx`), called through `get_random_secret()`.

## TUI Architecture

Built on **termbox2** (vendored as `lib/termbox2.h`).

- **Event-driven** `tui_main()` and every input prompt block on `tb_poll_event()`.
  Redraws only happen in response to a keypress or terminal resize.

- **Hand-rolled line editing** `get_input()` tracks its own cursor and buffer, handling
  backspace, Enter, Esc/Ctrl-C, and a printable-character filter (`IS_VALID`) against
  raw `tb_event` key codes. `get_secret()` reuses this with echo suppressed.
  Tradeoff: no history, no tab completion.

- **Search:** Keystrokes feed a pattern matched against loaded records into a small queue
  structure, which `n` walks. No regex engine.

- **Layout:** Rendering, pagination, and the detail viewer are recomputed from
  `tb_width()`/`tb_height()` on every redraw, so resize events are just another event.

## Source Layout

```
├── include
│   ├── cruxpass.h          # Types, constants
│   ├── crypt.h             # Key derivation, authentication, decryption
│   ├── database.h          # Database operations
│   └── tui.h               # TUI interface
├── lib
│   ├── args.h              # CLI argument parsing
│   └── termbox2.h          # Vendored terminal library
└── src
    ├── cruxpass.c           # Core logic (import/export, secret generation)
    ├── crypt.c              # Cryptography concerning logic
    ├── database.c           # SQLite/SQLCipher operations
    ├── main.c               # Entry point
    └── tui
        ├── chalk.c          # drawig, color, and styling
        ├── display.c        # Screen rendering
        ├── helpers.c        # TUI utilities and wrappers
        ├── input.c          # Input handling
        ├── pipeline.c       # Feeds the TUI with records
        ├── queue.c          # Search result queue
        └── tui.c            # TUI main loop
```
