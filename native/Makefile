OBJS=main.o graphics.o anim.o background.o files.o choreography.o sound.o iff.o

LIBS=-lSDL2 -lm -lmikmod -lpthread
CFLAGS=-g -O2 -std=c99

sota: $(OBJS)
	$(CC) -o $@ $+ $(LIBS)

.PHONY: clean
clean:
	rm -f *.o sota

