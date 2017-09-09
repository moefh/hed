
OBJS = main.o term.o input.o screen.o file.o file_sel.o editor.o help.o

.PHONY: clean

hed: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	rm -f hed *.o *~

%.o: %.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -o $@ -c $<

.c.o:
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -o $@ -c $<
