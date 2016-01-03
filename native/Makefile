OBJS=main.o graphics.o anim.o palettes.o

LIBS=-lSDL2 -lm
CFLAGS=-g

sota: $(OBJS)
	$(CC) -o $@ $+ $(LIBS)

.PHONY: clean
clean:
	rm -f *.o sota

