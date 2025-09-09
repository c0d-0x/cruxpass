CC = gcc

CFLAGS   ?= -Wall -Wextra -Wformat-security -g
CPPFLAGS ?= -Iinclude

LDFLAGS  ?=
LDLIBS    = -lsodium -lncursesw -lpanel -lm -lsqlcipher -ldl -lpthread
STATIC_LIBS = ./lib/*.a

SRC      := $(wildcard src/*.c)
TUI_SRC  := $(wildcard src/tui/*.c)
ALL_SRC  := $(SRC) $(TUI_SRC)

OBJ      := $(ALL_SRC:src/%.c=build/%.o)

BIN := bin/cruxpass

PREFIX ?= /usr/local
RUNDIR ?= $(HOME)/.local/share/cruxpass
INSTALL_CRUXPASS_DB := $(RUNDIR)/cruxpass.db
INSTALL_AUTH_DB    := $(RUNDIR)/auth.db

all: $(BIN)
	@echo 'Build complete (dev).'

$(BIN): $(OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LDFLAGS) $(STATIC_LIBS) $(LDLIBS)

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

install: clean
	# cleanup and rebuild with production DB paths
	$(MAKE) CPPFLAGS='$(CPPFLAGS) -DCRUXPASS_DB=\"$(INSTALL_CRUXPASS_DB)\" -DAUTH_DB=\"$(INSTALL_AUTH_DB)\"' $(BIN)
	install -d $(RUNDIR)
	sudo install -d $(PREFIX)/bin
	sudo install -m 0755 $(BIN) $(PREFIX)/bin
	@echo "Installed with:"
	@echo "  CRUXPASS_DB=$(INSTALL_CRUXPASS_DB)"
	@echo "  AUTH_DB=$(INSTALL_AUTH_DB)"

.PHONY: all clean install uninstall

clean:
	@rm -rf build $(BIN)
	@echo "Clean up complete..."

uninstall:
	rm -rf $(RUNDIR)
	sudo rm -f $(PREFIX)/bin/cruxpass

