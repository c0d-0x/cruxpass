CC = gcc
CFLAGS = -Wall -Wextra -Wformat-security -g

# Libraries and paths
LIBS = -lsodium -lncursesw -lm  -lsqlcipher -ldl -lpthread 
INCLUDES = -Iinclude
STATIC_LIBS = ./lib/*.a

SRC = $(wildcard src/*.c) 
OBJ = $(SRC:.c=.o)

BIN = ./bin/cruxpass

PREFIX ?= /usr/local
DESTDIR = $(HOME)/.local/share/cruxpass

all: $(BIN)
	@rm -f src/*.o 
	@ echo 'Build complete...'

$(BIN): $(OBJ)
	@$(CC) $(CFLAGS) $(OBJ) -o $@ $(LIBS) $(STATIC_LIBS)

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

