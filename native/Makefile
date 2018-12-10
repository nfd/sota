# Use Mikmod? Mikmod supports MODs so can play the original SOTA music, but many mobile browsers don't support the APIs
# it needs when running under Emscripten.
#
# If you're making an executable for a desktop, use Emscripten.
# If you're making a JavaScript / wasm bundle for mobile, use sdl-mixer instead, and use MP3s rather than the original
# MODs.

OBJS=main.o graphics.o anim.o scene.o wad.o choreography.o iff.o iff-font.o posix_sdl2_backend.o heap.o mbit.o pcgrandom.o tinf/src/adler32.o tinf/src/crc32.o tinf/src/tinflate.o tinf/src/tinfzlib.o 

# Set your local Mikmod path here if you have one.  I use a local mikmod due
# to a bug the official release has with playing samples on OS X (and also
# because it's nice not to chase a moving target, though mikmod isn't really
# moving much nowadays)
MIKMOD_CFLAGS=-Ilibmikmod-3.3.11.1/include
BUILD_DEMO_ARGS=

ifdef EMSCRIPTEN
	# use sdl-mixer audio rather than emscripten.
	OBJS:=$(OBJS) sound_sdlmixer.o

	# -g4 -O0 for debugging, -g -O2 otherwise
	CFLAGS=-g -O2 -std=c99 -Wall -Werror -Wno-unused -DHEAP_SIZE_KB=1024 -s USE_SDL=2

	# EXTRA_EXPORTED_RUNTIME_METHODS is a bug workaround: https://github.com/kripken/emscripten/issues/5940
	CFLAGS:=$(CFLAGS) -s 'EXTRA_EXPORTED_RUNTIME_METHODS=["Pointer_stringify"]'

	# Debugging
	#CFLAGS:=$(CFLAGS) -s 'SAFE_HEAP=1' -s 'ASSERTIONS=1'

	# Emscripten doesn't seem to do realtime clocks well so fall back to gettimeofday
	#CFLAGS+=-ISDL_mixer/installed/include -DNO_POSIX_REALTIME_CLOCKS
	CFLAGS+=-DNO_POSIX_REALTIME_CLOCKS
	#LIBS:=SDL_mixer/installed/lib/libSDL2_mixer.a libmodplug-0.8.9.0/installed/lib/libmodplug.a -lm
	LIBS:=-lm
	LDFLAGS=--embed-file sota.wad
	UNAME:=emscripten
	MAIN_TARGET:=sota.js

	# Use MP3s (sadly) rather than mods to workaround poorly supported web audio APIs
	BUILD_DEMO_ARGS:=--mp3
else
	OBJS:=$(OBJS) sound_mikmod.o
	MIKMOD_LIBS = -L. -lmikmod
	CFLAGS=-g -O0 -std=c99 -Wall -Werror -fsanitize=address -Wno-unused-function -DHEAP_SIZE_KB=512 $(MIKMOD_CFLAGS)
	# Lame OS detection
	UNAME:=$(shell uname)
	LIBS=-lSDL2 -lSDL2_mixer -lm $(MIKMOD_LIBS)
	MAIN_TARGET:=sota
endif

# -fsanitize=address maps 16GB, which confuses the hell out of Valgrind. So if you're Valgrinding, don't use fsanitize=address.
#CFLAGS=-g -O0 -std=c99 -Wall -Werror -Wno-unused-function -DHEAP_SIZE_KB=512 $(MIKMOD_CFLAGS)
#CFLAGS=-g -O0 -std=c99 -Wall -Werror -Wno-unused-function -DHEAP_SIZE_KB=512

# POSIX backend
CFLAGS += -DBACKEND_SUPPORTS_SOUND=1 -DBACKEND_SUPPORTS_ILBM=1

ifeq ($(UNAME), Darwin)
	# Macs are magical, but the magic doesn't include realtime clocks or fmemopen().
	CFLAGS+=-DEMULATE_FMEMOPEN -DNO_POSIX_REALTIME_CLOCKS -DMAC_ENDIAN_H
	OBJS+=fmemopen.o
endif

$(MAIN_TARGET): $(OBJS) sota.wad
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

sota.wad: build_demo.py
	python3 build_demo.py $(BUILD_DEMO_ARGS)

.PHONY: clean
clean:
	rm -f $(OBJS) sota sota.js sota.js.map sota.js.mem sota.html.map sota.html.mem sota.wad

.PHONY: publish
publish: sota.js
	scp sota.js sota.js.mem index.html wzdd@lardcave.net:/home/wzdd/www/lardcave/www/sota/

.PHONY: fromscratch
fromscratch:
	mkdir data
	python3 fromadf.py
	python3 split_and_compress.py
	python3 build_demo.py

