OBJS=main.o graphics.o anim.o background.o files.o choreography.o sound.o iff.o font.o

LIBS=-lSDL2 -lm -lmikmod -lpthread
CFLAGS=-g -O2 -std=c99 -Wall -Werror -fsanitize=address

sota: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $+ $(LIBS)

.PHONY: clean
clean:
	rm -f *.o sota

