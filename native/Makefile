OBJS=main.o graphics.o anim.o scene.o wad.o choreography.o sound.o iff.o iff-font.o posix_sdl2_backend.o heap.o mbit.o pcgrandom.o tinf/src/adler32.o tinf/src/crc32.o tinf/src/tinflate.o tinf/src/tinfzlib.o

LIBS=-lSDL2 -lm -lmikmod -lpthread
CFLAGS=-g -O2 -std=c99 -Wall -Werror -fsanitize=address -Wno-unused-function -DHEAP_SIZE_KB=512
#CFLAGS=-g -O0 -std=c99 -Wall -Werror -Wno-unused-function -DHEAP_SIZE_KB=512

# POSIX backend
CFLAGS += -DBACKEND_SUPPORTS_SOUND=1 -DBACKEND_SUPPORTS_ILBM=1

# Lame OS detection
UNAME:=$(shell uname)

ifeq ($(UNAME), Darwin)
	CFLAGS+=-DEMULATE_FMEMOPEN -DNO_POSIX_REALTIME_CLOCKS -DMAC_ENDIAN_H
	OBJS+=fmemopen.o
endif

sota: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $+ $(LIBS)

.PHONY: data
data:
	python3 build_demo.py

.PHONY: clean
clean:
	rm -f $(OBJS) sota

