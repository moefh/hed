
CC = gcc
CFLAGS = -Wall -Wextra
LDFLAGS =

TARGETS = debug release

TEST_FILE = src/editor.o

.PHONY: $(TARGETS) build clean check

all: debug

debug:
	$(MAKE) build TARGET_CFLAGS="-O1 -g -Wstrict-prototypes -Werror" TARGET_LDFLAGS=""

release:
	$(MAKE) build TARGET_CFLAGS="-O2" TARGET_LDFLAGS="-s"

clean:
	rm -f *~ core
	$(MAKE) -C src clean

build:
	$(MAKE) -C src CFLAGS="$(CFLAGS) $(TARGET_CFLAGS)" CC="$(CC)" LDFLAGS="$(LDFLAGS) $(TARGET_LDFLAGS)" LIBS="$(LIBS)"

check: debug
	cat $(TEST_FILE) | valgrind --track-origins=yes --leak-check=full --show-leak-kinds=all src/hed - 2>x.hex
