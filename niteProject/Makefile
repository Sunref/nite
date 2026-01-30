# Makefile for nite (nano-inspired text editor)
# Targets:
#   make          -> build the project (default)
#   make build    -> build the project
#   make install  -> install the `nite` binary (PREFIX/bin or DESTDIR+PREFIX/bin)
#   make uninstall-> remove installed `nite` binary
#   make clean    -> remove build artifacts
#   make distclean-> remove builds/debug directories as well
#
# Usage examples:
#   make
#   make PREFIX=/usr/local install
#   make DESTDIR=/tmp/package install

NAME := nite

CC := cc
CFLAGS := -std=c11 -Wall -Wextra -Iinclude -g
LDLIBS := -lncurses -lpanel

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DESTDIR ?= 

SRC := $(wildcard src/*.c)
OBJDIR := build/obj
OBJS := $(patsubst src/%.c,$(OBJDIR)/%.o,$(SRC))
BUILD_DIR := builds
BUILD_BIN := $(BUILD_DIR)/$(NAME)

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
$(BUILD_BIN): $(OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDLIBS)
	@printf "Built %s\n" $@

# Install the built binary to $(DESTDIR)$(BINDIR)
install: build
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(BUILD_BIN) $(DESTDIR)$(BINDIR)/$(NAME)
	@printf "Installed %s to %s\n" $(NAME) $(DESTDIR)$(BINDIR)

# Uninstall removes the installed binary (uses DESTDIR/PREFIX same as install)
uninstall:
	@if [ -f "$(DESTDIR)$(BINDIR)/$(NAME)" ]; then \
		rm -f "$(DESTDIR)$(BINDIR)/$(NAME)"; \
		printf "Removed %s from %s\n" $(NAME) $(DESTDIR)$(BINDIR); \
	else \
		printf "No installed binary found at %s\n" "$(DESTDIR)$(BINDIR)/$(NAME)"; \
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