OBJS=main.o graphics.o anim.o background.o files.o choreography.o

LIBS=-lSDL2 -lm
CFLAGS=-g -O2

sota: $(OBJS)
	$(CC) -o $@ $+ $(LIBS)

.PHONY: clean
clean:
	rm -f *.o sota

