CC = gcc

CFLAGS   ?= -Wall -Wextra -Wformat-security 
CPPFLAGS ?= -Iinclude

LDFLAGS  ?=
LDLIBS    = -lsodium -lm -lsqlcipher -ldl -lpthread

SRC      := $(wildcard src/*.c)
TUI_SRC  := $(wildcard src/tui/*.c)
ALL_SRC  := $(SRC) $(TUI_SRC)

OBJ      := $(ALL_SRC:src/%.c=build/%.o)

BIN := bin/cruxpass

PREFIX ?= /usr/local

all: $(BIN)
	@echo 'Build complete (dev).'

$(BIN): $(OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LDFLAGS) $(LDLIBS)

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

install: clean
	$(MAKE) $(CPPFLAGS) $(BIN)
	sudo install -d $(PREFIX)/bin
	sudo install -m 0755 $(BIN) $(PREFIX)/bin
	@echo 'Installation complete.'

.PHONY: all clean install uninstall

clean:
	@rm -rf build $(BIN)
	@echo "Clean up complete..."

uninstall:
	sudo rm -f $(PREFIX)/bin/cruxpass

