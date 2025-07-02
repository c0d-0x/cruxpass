# cruxpass Password management

A simple, secure command-line password manager

## Features

Generate strong, random passwords
Store passwords securely
Retrieve passwords by username
List all saved passwords

## Usage

Bash
cruxPass <option> <argument>
Use code with caution. Learn more
Available options:

- -n: creates a new master password for cruxPass
- -d: \<password id\>: Deletes a password by id
- -e: \<file name\>: Export all saved passwords to a csv format
- -h: Display help information
- -i: \<file name\>: Import passwords from a csv file
- -l: List all saved passwords
- -r: Generate a random 35 characters long password without storing it
- -s: \<password\> \<username\> \<discretion\>: Saves your password and the specified details.

## Installation

Clone this repository:

```Bash

git clone https://github.com/c0d-0x/Cruxpass-Password-manager

cd Cruxpass-Password-manager
mkdir bin # cruxpass binary is build here
make intstall

# Run the program:
cruxpass <option> <argument>

#Uninstallation
make uninstall
```

Use this code with caution!!!

## Security Considerations

Password storage: Passwords are stored in an encrypted binary file in "~/.local/share/cruxpass/"
Authentication: cruxPass requires authentication before accessing or modifying stored passwords.
Best practices: Always use strong, unique passwords and never share them with others.

NOTE: This project is to be rewrited in Go for a better TUI. Use it at your own risks, as this project was implemented for a better understanding of file IO,
basic encryption and decryption, data serialisation and deserialization. You might need to clear your bash
history after saving a password with the -s option.

## Contributing

Contributions are welcome! Please submit pull requests or open issues for any suggestions or bug reports.

## License

cruxPass is licensed under the **MIT License**. See the LICENSE file for details.

## Contact

For any questions or feedback, please contact <c0d_0x007@proton.me>.

## Additional Notes

This project is currently under development.

> [!WARNING]
> DO NOT USE THIS BRANCH
> This branch is left here for only refrence purposes.
> Future plans include:

- Better encryption implementation
- Integration with other password management tools
- Integration with sql-light for password storage
- Copy password to clipboard on generation
