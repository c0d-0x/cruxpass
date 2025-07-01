CC = gcc
CFLAGS = -Wall -Wextra -Wformat-security -Wstrict-overflow=4 -g  -fsanitize=signed-integer-overflow #-O2 

# Libraries and paths
LIBS = -lsodium -lncurses -lm -Llib -lsqlcipher -largparse -ldl -lpthread -lcrypto
INCLUDES = -Iinclude

# Source and object files
SRC = $(wildcard src/*.c) 
OBJ = $(SRC:.c=.o)

# Target binary
BIN = ./bin/cruxpass

# Installation prefix (default /usr/local)
PREFIX ?= /usr/local
DESTDIR = $(HOME)/.local/share/cruxpass

# Main target
all: $(BIN)
	@rm -f src/*.o 

# Build executable
$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LIBS)

# Compile source files into object files
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

install: $(BIN)
	sudo install -d $(PREFIX)/bin 
	install -d $(DESTDIR)
	sudo install -m 0755 $(BIN) $(PREFIX)/bin

.PHONY: all test clean install uninstall

clean:
	@rm -f src/*.o $(BIN)
	@echo "Clean up complete..."

uninstall:
	rm -rf $(DESTDIR)
	sudo rm $(PREFIX)/bin/cruxpass

