OBJS=main.o graphics.o anim.o scene.o wad.o choreography.o iff.o iff-font.o posix_sdl2_backend.o heap.o mbit.o pcgrandom.o tinf/src/adler32.o tinf/src/crc32.o tinf/src/tinflate.o tinf/src/tinfzlib.o sound.o

ifdef EMSCRIPTEN
	# -g4 -O0 for debugging.
	CFLAGS=-g -O2 -std=c99 -Wall -Werror -Wno-unused -DHEAP_SIZE_KB=1024 $(MIKMOD_CFLAGS) -s USE_SDL=2

	# EXTRA_EXPORTED_RUNTIME_METHODS is a bug workaround: https://github.com/kripken/emscripten/issues/5940
	CFLAGS:=$(CFLAGS) -s 'EXTRA_EXPORTED_RUNTIME_METHODS=["Pointer_stringify"]'

	# Debugging
	#CFLAGS:=$(CFLAGS) -s 'SAFE_HEAP=1' -s 'ASSERTIONS=1'

	# Emscripten doesn't seem to do realtime clocks well so fall back to gettimeofday
	CFLAGS+=-ISDL_mixer/installed/include -DNO_POSIX_REALTIME_CLOCKS
	LIBS:=SDL_mixer/installed/lib/libSDL2_mixer.a libmodplug-0.8.9.0/installed/lib/libmodplug.a -lm
	LDFLAGS=--embed-file sota.wad
	UNAME:=emscripten
	MAIN_TARGET:=sota.html
else
	CFLAGS=-g -O0 -std=c99 -Wall -Werror -fsanitize=address -Wno-unused-function -DHEAP_SIZE_KB=512 $(MIKMOD_CFLAGS)
	# Lame OS detection
	UNAME:=$(shell uname)
	LIBS=-lSDL2 -lSDL2_mixer -lm
	MAIN_TARGET:=sota
endif

# -fsanitize=address maps 16GB, which confuses the hell out of Valgrind. So if you're Valgrinding, don't use fsanitize=address.
#CFLAGS=-g -O0 -std=c99 -Wall -Werror -Wno-unused-function -DHEAP_SIZE_KB=512 $(MIKMOD_CFLAGS)
#CFLAGS=-g -O0 -std=c99 -Wall -Werror -Wno-unused-function -DHEAP_SIZE_KB=512

# POSIX backend
CFLAGS += -DBACKEND_SUPPORTS_SOUND=1 -DBACKEND_SUPPORTS_ILBM=1

ifeq ($(UNAME), Darwin)
	CFLAGS+=-DEMULATE_FMEMOPEN -DNO_POSIX_REALTIME_CLOCKS -DMAC_ENDIAN_H
	OBJS+=fmemopen.o
endif

$(MAIN_TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $+ $(LIBS)

.PHONY: data
data:
	python3 build_demo.py

.PHONY: clean
clean:
	rm -f $(OBJS) sota

