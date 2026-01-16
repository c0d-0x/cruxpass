# A CLI Password Management

![cruxpass Screenshot](https://raw.githubusercontent.com/c0d-0x/cruxpass/dev/resources/cruxpass.png)
A minimal command-line password manager written in C, using SQLCipher-encrypted for secure local credential storage.

---

## Features

- Generate strong random passwords
- Securely store credentials
  - AES-256 encrypted database
  - 256-bit Argon2id key, derived from the user’s password
  - Key derivation strengthened with a 128-bit salt
- Retrieve passwords
  - Database decrypted in memory
- List saved credentials in a TUI
- Import and export credentials via CSV
- Delete individual entries

---

> **Breaking Change**
>
> Before upgrading, export your credentials/records with `cruxpass --export backup.csv` and remove existing databases (`rm ~/.local/share/cruxpass/*`).
>
> Skipping these steps may cause data corruption. Reff [changelog](CHANGELOG.md).

---

## Available Options

| Short | Long                       | Description                                                                      |
| ----- | -------------------------- | -------------------------------------------------------------------------------- |
| `-h`  | `--help`                   | Show help message                                                                |
| `-r`  | `--run-directory`          | Specify the directory path where the database will be stored.                    |
| `-d`  | `--delete <id>`            | Delete a password by its ID                                                      |
| `-e`  | `--export <file>`          | Export all saved passwords to a CSV file (32 characters long)                    |
| `-i`  | `--import <file>`          | Import passwords from a CSV file (32 characters long)                            |
| `-g`  | `--generate-rand <length>` | Generate a random password of given length (256 max)                             |
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

| Key(s)   | Description                                            |
| -------- | ------------------------------------------------------ |
| Enter    | View secret                                            |
| u        | Update record                                          |
| d        | Delete record                                          |
| /        | Search                                                 |
| n        | Next search result                                     |
| ?        | Show this help                                         |
| L        | Open full description viewer for the selected record   |
| ra       | Generate Lowercase letters                             |
| rA       | Generate Uppercase letters                             |
| rp       | Generate Digits (pin)                                  |
| rr       | Generate Lowercase, uppercase, digits, and symbols     |
| rx       | Generate All characters, excluding ambiguous ones      |
| q / Q    | Quit                                                   |
| Ctrl + r | Reload TUI (useful after saving a secret from the TUI) |

> [!NOTE]
> All `r/*` actions will prompt you to enter the desired length of the secret, which can then be saved directly

#### Navigation

| Key(s)         | Description       |
| -------------- | ----------------- |
| j / k or ↓ / ↑ | Down / Up         |
| h / l or ← / → | Page left / right |
| g / G          | First / Last      |

## Installation

> [!IMPORTANT]
>
> ### Requirements
>
> - libsqlcipher
> - libsodium
>
> This project is built and tested on Linux only.  
> Windows and macOS are not currently supported.

```bash
### Debian / Ubuntu
sudo apt install libsqlcipher-dev libsodium-dev

### Arch Linux
sudo pacman -S sqlcipher libsodium

### RHEL / Fedora / Rocky / Alma
sudo dnf install sqlcipher-devel libsodium-devel

# Clone the repository
git clone https://github.com/c0d-0x/cruxpass
cd cruxpass

# Build and test cruxpass
mkdir -p bin
mkdir -p .cruxpass  # only for testing; you can import moc.csv
make

./bin/cruxpass -i moc.csv -r .cruxpass
./bin/cruxpass -l -r .cruxpass # list all records

# Install cruxpass
make install

# Run the program
cruxpass <option> <argument>

# Uninstall
make uninstall
```

## Defaults

Encrypted vault: ~/.local/share/cruxpass/

- records' database: `~/.local/share/cruxpass/cruxpass.db`
- metadata(salt is stored here): `~/.local/share/cruxpass/meta.db`
- The `-r` option lets you specify a directory where cruxpass will store its databases. Make sure the directory already exists before using it..

Authentication: A master password is required to access or modify any stored data.

> [!NOTE]
> cruxpass enforces internal limits on the size of various fields to ensure performance and security. Here are the default constraints:
>
> - Minimum Master Password Length: Passwords must be at least 8 characters long to ensure basic security and maximum of 48 characters.
> - Stored Passwords/secrets Length: Each saved password can be up to 128 characters long. This accommodates strong, randomly generated credentials.
> - Username Length: Usernames are limited to 32 characters. This is sufficient for most standard account identifiers.
> - Description Length: Each password entry can include a label or description up to 256 characters, helping users recognize the purpose of stored credentials.

> These limits are designed to balance usability with memory safety and can be adjusted in the source code if needed

## Contributing

Contributions are welcome! You can open an issues or submit pull requests to help improve cruxpass.
