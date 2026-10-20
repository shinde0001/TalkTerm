# SimpleTerminal - Makefile
#
# make          build
# make run      build + run
# make clean    cleanup
# make install  install to /usr/local/bin

CC       = gcc
CFLAGS   = -Wall -Wextra -Wpedantic -std=c17 -Isrc/
LDFLAGS  = -lreadline

SRC_DIR  = src
OBJ_DIR  = build

SRC      = $(SRC_DIR)/main.c \
           $(SRC_DIR)/parser.c \
           $(SRC_DIR)/history.c \
           $(SRC_DIR)/executor.c

OBJ      = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRC))
TARGET   = talkterm
PREFIX   = /usr/local

.PHONY: all clean run install uninstall

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)
	@echo "  ✓ Build successful: ./$(TARGET)"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)
	@echo "  ✓ Cleaned."

run: $(TARGET)
	./$(TARGET)

install: $(TARGET)
	install -m 755 $(TARGET) $(PREFIX)/bin/$(TARGET)
	@echo "  ✓ Installed to $(PREFIX)/bin/$(TARGET)"

uninstall:
	rm -f $(PREFIX)/bin/$(TARGET)
	@echo "  ✓ Uninstalled."
