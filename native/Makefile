OBJS=main.o graphics.o anim.o background.o files.o choreography.o sound.o iff.o font.o

LIBS=-lSDL2 -lm -lmikmod -lpthread
CFLAGS=-g -O2 -std=c99 -Wall -Werror -fsanitize=address -Wno-unused-function

# Lame OS detection
UNAME:=$(shell uname)

ifeq ($(UNAME), Darwin)
	CFLAGS+=-DEMULATE_FMEMOPEN
	OBJS+=fmemopen.o
endif

sota: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $+ $(LIBS)

.PHONY: data
data:
	python3 build_demo.py

.PHONY: clean
clean:
	rm -f *.o sota

