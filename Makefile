CC             := gcc

CFLAGS         := -Wall -Wextra -Wformat-security -Wformat-overflow=2 -O3

STRIP_BIN			 := -s

INCLUDE        := -Iinclude

LDLIBS         := -lsodium -lm -lsqlcipher -ldl -lpthread

SRC            := $(wildcard src/*.c)
TUI_SRC        := $(wildcard src/tui/*.c)
ALL_SRC        := $(SRC) $(TUI_SRC)

OBJ            := $(ALL_SRC:src/%.c=build/%.o)

BIN            := bin/cruxpass
BIN_NAME	   := cruxpass

PREFIX         := /usr/
OLD_PREFIX_BIN := /usr/local/bin/cruxpass

BASH_COMPLETION_PATH := /usr/share/bash-completion/completions/$(BIN_NAME)
ZSH_COMPLETION_PATH  := /usr/share/zsh/site-functions/_$(BIN_NAME)
FISH_COMPLETION_PATH := /usr/share/fish/vendor_completions.d/$(BIN_NAME).fish


all: $(BIN)
	@echo 'Build complete (dev).'

$(BIN): $(OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(STRIP_BIN) $(OBJ) -o $@ $(LDLIBS)

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(INCLUDE) $(CFLAGS) -c $< -o $@

install: clean
	$(MAKE)  $(INCLUDE) $(BIN)
	-$(BIN) completion bash > $(BASH_COMPLETION_PATH)
	-$(BIN) completion zsh > $(ZSH_COMPLETION_PATH)
	-$(BIN) completion fish > $(FISH_COMPLETION_PATH) 
	@install -d $(PREFIX)/bin
	@install -m 0755 $(BIN) $(PREFIX)/bin

# migrating from /usr/local/bin/ to /usr/bin/
# Because bin used to be installed: /usr/local/bin/ 
	@echo "Checking for $(OLD_PREFIX_BIN)"
	@if [ -f "$(OLD_PREFIX_BIN)" ]; then \
		echo "Removing system-local copy: $(OLD_PREFIX_BIN)"; \
		 rm -f "$(OLD_PREFIX_BIN)"; \
	fi
	@echo 'Installation complete.'

.PHONY: all clean install uninstall

clean:
	@rm -rf build $(BIN)
	@echo "Clean up complete..."

uninstall:
	rm -f $(PREFIX)/bin/cruxpass $(BASH_COMPLETION_PATH) $(ZSH_COMPLETION_PATH) $(FISH_COMPLETION_PATH)

