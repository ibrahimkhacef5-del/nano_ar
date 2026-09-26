
CC      = clang
CFLAGS  = -Wall -Wextra -O2 -std=c11
LDFLAGS = -lncurses
TARGET  = nano-ar
SRC     = nano_ar.c
PREFIX  ?= $(PREFIX)
BINDIR  ?= $(PREFIX)/bin

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

install: $(TARGET)
	@mkdir -p $(BINDIR)
	cp $(TARGET) $(BINDIR)/
	@echo "✓ تم تثبيت $(TARGET) في $(BINDIR)"

uninstall:
	rm -f $(BINDIR)/$(TARGET)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all install uninstall clean run
