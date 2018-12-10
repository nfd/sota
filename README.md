##State of the Art

This is a reimplementation, in C, of the classic Amiga demo State Of The Art, by Spaceballs: http://www.pouet.net/prod.php?which=99

**Work in progress:** It is a complete reimplementation, but the timing is off in parts.

The code is new, but it uses the demo's original graphics, animation, and sounds.

To build it, you'll need SDL2 (and dev packages), mikmod, and Python 3.

	cd native
	make fromscratch
	make
	./sota

If you're rebuilding later, you can just use 'make'.

