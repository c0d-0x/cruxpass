# cruxpass — Password Management CLI Tool

![cruxpass Screenshot](https://raw.githubusercontent.com/c0d-0x/cruxpass/dev/resouces/cruxpass.png)

[![Version](https://img.shields.io/badge/version-v1.2.1-blue.svg)](https://github.com/c0d-0x/cruxpass/releases)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Language](https://img.shields.io/badge/language-C-blue.svg)](https://github.com/c0d-0x/cruxpass)

**cruxpass** is a secure, fast, and lightweight command-line password manager built in C. It enables secure storage and retrieval of credentials using encrypted SQLite database storage (via [SQLCipher](https://www.zetetic.net/sqlcipher/)).

---

## 🔐 Features

- 🔑 Generate strong, random passwords
- 🛡️ Store passwords securely using encryption
- 🔍 Retrieve passwords by username
- 📋 List all saved credentials
- 📤 Export and 📥 import passwords via CSV
- 🧹 Delete specific password entries
- 🧪 Generate throwaway random passwords (not stored)

---

## 🚀 Available Options

| Short | Long                       | Description                                               |
| ----- | -------------------------- | --------------------------------------------------------- |
| `-h`  | `--help`                   | Show help message                                         |
| `-d`  | `--delete <id>`            | Delete a password by its ID                               |
| `-e`  | `--export <file>`          | Export all saved passwords to a CSV file                  |
| `-i`  | `--import <file>`          | Import passwords from a CSV file                          |
| `-g`  | `--generate-rand <length>` | Generate a random password of given length                |
| `-l`  | `--list`                   | List all stored passwords                                 |
| `-n`  | `--new-password`           | Create a new master password                              |
| `-s`  | `--save`                   | Save a password with its metadata (prompts interactively) |
| `-v`  | `--version`                | Show cruxPass version                                     |

⚙️ Installation

```bash
# Clone the repository
git clone https://github.com/c0d-0x/cruxpass

cd cruxpass
mkdir bin # cruxpass binary is built here

# Install the program
make install

# Run the program
cruxpass <option> <argument>

# To uninstall
make uninstall
```

🛡️ Security
Encrypted Storage: All data is securely stored using an encrypted SQLite database (SQLCipher) at ~/.local/share/cruxpass/.

Authentication: A master password is required to access or modify any stored data. By default, cruxpass uses: `cruxpassisgr8!` as it's master password.

CLI Best Practices: For maximum security, consider clearing your shell history after using options like -s that include plaintext passwords.

📈 Roadmap
Future enhancements may include:

🔐 Additional password management tooling and CLI utilities

📋 Clipboard integration on password generation

🧠 Advanced search and filtering within stored data

🧪 Unit testing for critical functions

🤝 Contributing
Contributions are welcome! Please open issues or submit pull requests to help improve cruxPass.

📄 License
cruxPass is licensed under the MIT License. See the LICENSE file for details.
