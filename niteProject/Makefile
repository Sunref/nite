# Makefile for nite
# Targets:
#   make          -> build the project (default)
#   make build    -> build the project
#   make install  -> install the `nite` binary and grammars
#   make uninstall-> remove installed `nite` binary and grammars
#   make clean    -> remove build artifacts
#   make distclean-> remove builds/debug directories as well

NAME := nite
CC := cc
# Caminho absoluto para help.txt no build (para !help funcionar de qualquer cwd)
HELPFILE := $(shell realpath help.txt 2>/dev/null || true)
CFLAGS := -std=gnu11 -Wall -Wextra -Iinclude -g -MMD -MP $(if $(HELPFILE),-DHELP_FILE=\"$(HELPFILE)\")
DEPS   := $(OBJS:.o=.d)
LDLIBS := -lncurses -lpanel -ltree-sitter -ldl

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
LIBDIR ?= $(PREFIX)/lib
GRAMMARDIR ?= $(LIBDIR)/nite/grammars
DESTDIR ?=

SRC := $(wildcard src/*.c)
OBJDIR := build/obj
OBJS := $(patsubst src/%.c,$(OBJDIR)/%.o,$(SRC))
BUILD_DIR := build
BUILD_BIN := $(BUILD_DIR)/$(NAME)

GRAMMARS := $(wildcard grammars/*.so)

.PHONY: all build install uninstall clean distclean

all: build

build: $(BUILD_BIN)

# Ensure object directory exists
$(OBJDIR):
	mkdir -p $(OBJDIR)

# Compile each source file to object in $(OBJDIR)
$(OBJDIR)/%.o: src/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Ensure build directory exists
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Link final binary
$(BUILD_BIN): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDLIBS)
	@printf "Built %s\n" $@

# Install the built binary and grammars
install: build
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(BUILD_BIN) $(DESTDIR)$(BINDIR)/$(NAME)
	@printf "Installed %s to %s\n" $(NAME) $(DESTDIR)$(BINDIR)
	@if [ -n "$(GRAMMARS)" ]; then \
		install -d $(DESTDIR)$(GRAMMARDIR); \
		for grammar in $(GRAMMARS); do \
			install -m 644 $$grammar $(DESTDIR)$(GRAMMARDIR)/; \
		done; \
		printf "Installed grammars to %s\n" $(DESTDIR)$(GRAMMARDIR); \
	else \
		printf "Warning: No grammars found in grammars/\n"; \
	fi

# Uninstall removes the installed binary and grammars
uninstall:
	@if [ -f "$(DESTDIR)$(BINDIR)/$(NAME)" ]; then \
		rm -f "$(DESTDIR)$(BINDIR)/$(NAME)"; \
		printf "Removed %s from %s\n" $(NAME) $(DESTDIR)$(BINDIR); \
	else \
		printf "No installed binary found at %s\n" "$(DESTDIR)$(BINDIR)/$(NAME)"; \
	fi
	@if [ -d "$(DESTDIR)$(GRAMMARDIR)" ]; then \
		rm -rf "$(DESTDIR)$(GRAMMARDIR)"; \
		printf "Removed grammars from %s\n" $(DESTDIR)$(GRAMMARDIR); \
	fi

# Remove build artifacts produced by this Makefile
clean:
	rm -rf $(OBJDIR)
	rm -f $(BUILD_BIN)
	@printf "Cleaned build artifacts\n"

# Remove build artifacts and any generated build directories
distclean: clean
	rm -rf $(BUILD_DIR) debug
	@printf "Removed build and debug directories\n"

# Convenience PHONY targets for clarity
rebuild: distclean all
	@printf "Rebuilt project\n"

-include $(DEPS)