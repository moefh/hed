
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pedantic -Wno-unused-result
LDFLAGS =

TARGETS = debug release

TEST_FILE = test.txt

.PHONY: $(TARGETS) build clean check

all: debug

debug:
	$(MAKE) build TARGET_CFLAGS="-O1 -g" TARGET_LDFLAGS=""

release:
	$(MAKE) build TARGET_CFLAGS="-O2" TARGET_LDFLAGS="-s"

clean:
	rm -f *~ core
	$(MAKE) -C src clean

build:
	$(MAKE) -C src CFLAGS="$(CFLAGS) $(TARGET_CFLAGS)" CC="$(CC)" LDFLAGS="$(LDFLAGS) $(TARGET_LDFLAGS)" LIBS="$(LIBS)"

check: debug
	valgrind --track-origins=yes --leak-check=full --show-leak-kinds=all --log-file=x src/hed $(TEST_FILE)
