# A CLI Password Management

![cruxpass Screenshot](https://raw.githubusercontent.com/c0d-0x/cruxpass/dev/resources/cruxpass.png)

[![Version](https://img.shields.io/badge/version-v1.0.0-blue.svg)](https://github.com/c0d-0x/cruxpass/releases)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Language](https://img.shields.io/badge/language-C-blue.svg)](https://github.com/c0d-0x/cruxpass)

A secure, fast, and lightweight command-line password manager built in C. It enables secure storage and retrieval of credentials using encrypted SQLite database storage (via [SQLCipher](https://www.zetetic.net/sqlcipher/)).

---

## Features

- Generate strong, random passwords
- Store passwords securely
- Retrieve passwords
- List all saved credentials in a TUI
- Export and import passwords via CSV
- Delete specific password entries

---

## Available Options

| Short | Long                       | Description                                                                      |
| ----- | -------------------------- | -------------------------------------------------------------------------------- |
| `-h`  | `--help`                   | Show help message                                                                |
| `-r`  | `--run-directory`          | Specify the directory path where the database will be stored.                    |
| `-d`  | `--delete <id>`            | Delete a password by its ID                                                      |
| `-e`  | `--export <file>`          | Export all saved passwords to a CSV file                                         |
| `-i`  | `--import <file>`          | Import passwords from a CSV file                                                 |
| `-g`  | `--generate-rand <length>` | Generate a random password of given length                                       |
| `-x`  | `--exclude-ambiguous`      | Exclude ambiguous characters when generating a random password (combine with -g) |
| `-l`  | `--list`                   | List all stored passwords                                                        |
| `-n`  | `--new-password`           | Change your master password                                                      |
| `-s`  | `--save`                   | Save a password with its metadata (prompts interactively)                        |
| `-v`  | `--version`                | Show cruxPass version                                                            |

### CSV file format for imports

| username      | secret     | Description      |
| ------------- | ---------- | ---------------- |
| test@test.com | rdj(:p6Y{p | This is a secret |

### Options in the TUI mode (--list)

#### Actions

| Key(s) | Description                                          |
| ------ | ---------------------------------------------------- |
| Enter  | View secret                                          |
| u      | Update record                                        |
| d      | Delete record                                        |
| /      | Search                                               |
| n      | Next search result                                   |
| ?      | Show this help                                       |
| L      | Open full description viewer for the selected record |
| q / Q  | Quit                                                 |

#### Navigation

| Key(s)         | Description       |
| -------------- | ----------------- |
| j / k or ↓ / ↑ | Down / Up         |
| h / l or ← / → | Page left / right |
| g / G          | First / Last      |

## Installation

> [!IMPORTANT]
>
> ### This project requires:
>
> - libsqlchipher
> - libsodium
>
> This Project is built in a linux envronment and haven't been tested on Windows or Mac.

```bash
# Clone the repository
git clone https://github.com/c0d-0x/cruxpass

cd cruxpass
## build and test out cruxpass
mkdir bin # cruxpass binary is built here
mkdir .cruxpass # only if you're testing: you can import moc.csv for testing
make
./bin/cruxpass -i moc.csv -r ./cruxpass

# Install the cruxpass
make install

# Run the program
cruxpass <option> <argument>

# To uninstall
make uninstall
```

## Defaults

Encrypted Storage: All secrets are securely stored using an encrypted SQLite database (SQLCipher) at ~/.local/share/cruxpass/.

- secret database: `~/.local/share/cruxpass/cruxpass.db`
- Authentication hash db: `~/.local/share/cruxpass/auth.db`
- The `-r` option lets you specify a directory where cruxpass will store its databases. Make sure the directory already exists before using it..

Authentication: A master password is required to access or modify any stored data.

> [!NOTE]
> cruxpass enforces internal limits on the size of various fields to ensure performance and security. Here are the default constraints:
>
> - Minimum Password Length: Passwords must be at least 8 characters long to ensure basic security and maximum of 45 characters.
> - Stored Passwords/secrets Length: Each saved password can be up to 128 characters long. This accommodates strong, randomly generated credentials.
> - Username Length: Usernames are limited to 20 characters. This is sufficient for most standard account identifiers.
> - Description Length: Each password entry can include a label or description up to 100 characters, helping users recognize the purpose of stored credentials.

> These limits are designed to balance usability with memory safety and can be adjusted in the source code if needed

## Contributing

Contributions are welcome! You can open an issues or submit pull requests to help improve cruxpass.
