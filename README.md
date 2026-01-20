A minimal CLI password manager written in C and designed to be simple, dependency-light, and transparent

![cruxpass Screenshot](https://raw.githubusercontent.com/c0d-0x/cruxpass/dev/resources/cruxpass.gif)

---

> [!WARNING]
> **Breaking Changes in Latest Version**
>
> Before upgrading:
>
> - Export your credentials: `cruxpass --export backup.csv`
> - Remove existing databases: `rm ~/.local/share/cruxpass/*`
>
> Skipping these steps will cause data corruption. See the [changelog](CHANGELOG.md).

---

## Features

- Generate strong random passwords
- Securely store encrypted records locally
- Retrieve secrets on demand with explicit user intent
- List and manage stored credentials through a fast, keyboard-driven TUI
- Delete or update individual entries
- Import and export credentials via CSV

## Installation

### Requirements

This project runs on **Linux only**. Windows and macOS are not currently supported.

**Dependencies:**

- `libsqlcipher` - Database encryption
- `libsodium` - Cryptographic operations

#### Install Dependencies

<details>
<summary>Debian / Ubuntu / Kali</summary>

```bash
sudo apt install libsqlcipher-dev libsodium-dev
```

</details>

<details>
<summary>Arch Linux ;)</summary>

```bash
sudo pacman -S sqlcipher libsodium
```

</details>

<details>
<summary>Fedora</summary>

```bash
sudo dnf install sqlcipher-devel libsodium-devel
```

</details>

<details>
<summary>RHEL / Rocky / Alma</summary>

```bash
sudo dnf install sqlcipher-devel libsodium-devel
```

</details>

### Build and Install

```bash
# Clone the repository
git clone https://github.com/c0d-0x/cruxpass
cd cruxpass

# Build
mkdir -p bin
make

# Optional: Test before installing
mkdir -p .cruxpass
./bin/cruxpass -i moc.csv -r .cruxpass
./bin/cruxpass -l -r .cruxpass

# Install system-wide
sudo make install

# Uninstall
sudo make uninstall
```

---

## Usage

### Command-Line Options

| Short | Long                       | Description                                  |
| ----- | -------------------------- | -------------------------------------------- |
| `-h`  | `--help`                   | Show help message                            |
| `-v`  | `--version`                | Show version                                 |
| `-s`  | `--save`                   | Save a new password (interactive)            |
| `-l`  | `--list`                   | Open TUI to browse all passwords             |
| `-d`  | `--delete <id>`            | Delete password by ID                        |
| `-g`  | `--generate-rand <length>` | Generate random password (max 256 chars)     |
| `-x`  | `--exclude-ambiguous`      | Exclude ambiguous characters (use with `-g`) |
| `-e`  | `--export <file>`          | Export all passwords to CSV                  |
| `-i`  | `--import <file>`          | Import passwords from CSV                    |
| `-n`  | `--new-password`           | Change master password                       |
| `-r`  | `--run-directory`          | Specify custom database directory            |

### Examples

```bash
# Generate a 20-character password
cruxpass -g 20

# Generate password excluding ambiguous characters (0, O, l, 1, etc.)
cruxpass -xg 20

# Save a new credential
cruxpass -s

# List all credentials in TUI
cruxpass -l

# Export to backup
cruxpass -e backup.csv

# Import from backup
cruxpass -i backup.csv

# Use custom database location
cruxpass -l -r /path/to/custom/directory
```

---

## TUI Mode

Launch with `cruxpass -l` or `cruxpass --list`.

### Navigation

| Key                    | Action             |
| ---------------------- | ------------------ |
| `j` / `k` or `↓` / `↑` | Move down/up       |
| `h` / `l` or `←` / `→` | Page left/right    |
| `g` / `G`              | Jump to first/last |

### Actions

| Key       | Action                | Key  | Generates                             |
| --------- | --------------------- | ---- | ------------------------------------- |
| `Enter`   | View secret           | `ra` | Lowercase letters only                |
| `u`       | Update record         | `rA` | Uppercase letters only                |
| `d`       | Delete record         | `rp` | Digits only (PIN)                     |
| `/`       | Search                | `rr` | Lowercase, uppercase, digits, symbols |
| `n`       | Next search result    | `rx` | All characters except ambiguous ones  |
| `L`       | View full description |      |                                       |
| `?`       | Show help             |      |                                       |
| `Ctrl+r`  | Reload TUI            |      |                                       |
| `q` / `Q` | Quit                  |      |                                       |

> [!NOTE]
> All r/\* actions prompt for length (8-128 characters) and can be saved directly.

---

## CSV Import Format

Your CSV file should have three columns:

| username         | secret      | Description      |
| ---------------- | ----------- | ---------------- |
| test@test.com    | rdj(:p6Y{p  | This is a secret |
| user@example.com | P@ssw0rd123 | Work email       |

---

## Data Storage

**Default location:** `~/.local/share/cruxpass/`

- `cruxpass.db` - Encrypted password records
- `meta.db` - Salt storage for key derivation

**Custom location:** Use `-r <directory>` to specify an alternative path (directory must exist).

**Authentication:** All operations require your master password.

---

## Security Details

- **Encryption:** AES-256 in CBC mode
- **Key derivation:** Argon2id with 256-bit output
- **Salt:** 128-bit random salt per database
- **Memory safety:** Database decrypted only in memory, never written to disk unencrypted

### Field Limits

| Field           | Minimum | Maximum   |
| --------------- | ------- | --------- |
| Master password | 8 chars | 48 chars  |
| Stored secrets  | 8 chars | 128 chars |
| Username        | 8 chars | 32 chars  |
| Description     | 8 chars | 256 chars |

These limits balance security with usability and can be modified in the source code.

---

## Contributing

Contributions are welcome! Please:

- Open an issue for bugs or feature requests
- Submit pull requests with clear descriptions
- Follow the existing code style

---
