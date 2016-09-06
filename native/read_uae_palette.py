# nfd-only code which reads dumped .palette.bin files from his hacked-up UAE.
import struct
import sys

BUFSIZE = 4096
OFFSET = 0x40
NUM_COLOURS = 32
UNPACK_COLOUR = '<I'

def iter_palette(data):
	for idx in range(NUM_COLOURS):
		offset = OFFSET + idx * 4

		raw = data[offset: offset + 4]
		colour = struct.unpack(UNPACK_COLOUR, raw)[0]

		# MSB of UAE colour is 00. We assume ARGB, i.e. MSB is alpha:
		colour |= 0xff000000
		yield colour

def show_palette(filename):
	with open(filename, 'rb') as h:
		data = h.read(BUFSIZE)
	
	colours = list(iter_palette(data))

	# Trim the list of trailing black colours (there are automatically added)
	length = NUM_COLOURS
	while colours[length - 1] == 0xFF000000:
		length -= 1
	colours = colours[:length]

	# Display as a Python tuple.
	print("(%s)" % (', '.join('0x%08x' % (colour) for colour in colours)))

if __name__ == '__main__':
	show_palette(sys.argv[1])

