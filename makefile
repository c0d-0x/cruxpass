CC             = gcc

CFLAGS         ?= -Wall -Wextra -Wformat-security -Wformat-overflow=2

INCLUDE        ?= -Iinclude

LDLIBS         = -lsodium -lm -lsqlcipher -ldl -lpthread

SRC            := $(wildcard src/*.c)
TUI_SRC        := $(wildcard src/tui/*.c)
ALL_SRC        := $(SRC) $(TUI_SRC)

OBJ            := $(ALL_SRC:src/%.c=build/%.o)

BIN            := bin/cruxpass

PREFIX         ?= /usr/
OLD_PREFIX_BIN ?= /usr/local/bin/cruxpass

all: $(BIN)
	@echo 'Build complete (dev).'

$(BIN): $(OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LDLIBS)

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(INCLUDE) $(CFLAGS) -c $< -o $@

install: clean
	$(MAKE) $(INCLUDE) $(BIN)
	@sudo install -d $(PREFIX)/bin
	@sudo install -m 0755 $(BIN) $(PREFIX)/bin

# migrating from /usr/local/bin/ to /usr/bin/
	@echo "Checking for $(OLD_PREFIX_BIN)"
	@if [ -f "$(OLD_PREFIX_BIN)" ]; then \
		echo "Removing system-local copy: $(OLD_PREFIX_BIN)"; \
		sudo rm -f "$(OLD_PREFIX_BIN)"; \
	fi
	@echo 'Installation complete.'

.PHONY: all clean install uninstall

clean:
	@rm -rf build $(BIN)
	@echo "Clean up complete..."

uninstall:
	sudo rm -f $(PREFIX)/bin/cruxpass

