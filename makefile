CC = gcc

# Warnings/opts
CFLAGS   ?= -Wall -Wextra -Wformat-security -g
# Preprocessor flags (includes + optional -D)
CPPFLAGS ?= -Iinclude

# Linker
LDFLAGS  ?=
LDLIBS    = -lsodium -lncursesw -lpanel -lm -lsqlcipher -ldl -lpthread
STATIC_LIBS = ./lib/*.a

SRC := $(wildcard src/*.c)
OBJ := $(SRC:.c=.o)
BIN := ./bin/cruxpass

PREFIX ?= /usr/local
# App data dir (runtime). (Note: not the packaging DESTDIR meaning.)
RUNDIR ?= $(HOME)/.local/share/cruxpass

# Production paths (expanded by make at build time)
INSTALL_CRUXPASS_DB := /.local/share/cruxpass/cruxpass.db
INSTALL_AUTH_DB    := /.local/share/cruxpass/auth.db

all: $(BIN)
	@rm -f src/*.o
	@echo 'Build complete (dev).'

$(BIN): $(OBJ)
	@$(CC) $(CFLAGS) $(OBJ) -o $@ $(LDFLAGS) $(STATIC_LIBS) $(LDLIBS)

src/%.o: src/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

install: clean
	# Rebuild with production DB paths
	$(MAKE) CPPFLAGS='$(CPPFLAGS) -DCRUXPASS_DB=\"$(INSTALL_CRUXPASS_DB)\" -DAUTH_DB=\"$(INSTALL_AUTH_DB)\"' $(BIN)
	install -d $(RUNDIR)
	sudo install -d $(PREFIX)/bin
	sudo install -m 0755 $(BIN) $(PREFIX)/bin
	@echo "Installed with:"
	@echo "  CRUXPASS_DB=$(INSTALL_CRUXPASS_DB)"
	@echo "  AUTH_DB=$(INSTALL_AUTH_DB)"

.PHONY: all clean install uninstall

clean:
	@rm -f $(OBJ) $(BIN)
	@echo "Clean up complete..."

uninstall:
	rm -rf $(RUNDIR)
	sudo rm -f $(PREFIX)/bin/cruxpass

