"""
Convert an RGBA png to a palettised one in a pretty mean way.
"""
import sys
import argparse
import struct
#import math

from PIL import Image

import pilutils

def get_palette(donor_filename):
	img = Image.open(donor_filename)
	return pilutils.decode_lousy_pil_palette_data(img.palette.getdata())

def closest_match(pixel, palette):
	best_score = None
	best_idx = None

	for idx, entry in enumerate(palette):
		diff = abs(pixel[0] - entry[0]) + abs(pixel[1] - entry[1]) + abs(pixel[2] - entry[2])
		if best_score is None or diff < best_score:
			best_score = diff
			best_idx = idx
	
	return best_idx

def find_palette(img):
	count = {}
	for pixel in img.getdata():
		if pixel not in count:
			count[pixel] = 0
		count[pixel] += 1

	popular = sorted(count.keys(), key=lambda k: count[k], reverse=True)[:16]
	popular = [col[:3] for col in popular] # remove alpha
	return popular

def todata(palette):
	return b''.join(struct.pack('BBB', *elem) for elem in palette)

def force_to_palette(filename, donor_palette_filename):
	img = Image.open(filename)

	if donor_palette_filename:
		donor_img = Image.open(donor_palette_filename)
		palette = pilutils.decode_lousy_pil_palette_data(donor_img.palette.getdata())
		print(donor_img.palette.getdata())
	else:
		palette = find_palette(img)

	dst = Image.new("P", img.size)

	#dst.putpalette(donor_img.palette.getdata()[1])
	dst.putpalette(todata(palette))

	width, height = img.size

	for idx, pixel in enumerate(img.getdata()):

		colour = closest_match(pixel, palette)
		dst.putpixel((idx % width, idx // width), colour)
	
	return dst

def main():
	parser = argparse.ArgumentParser()
	parser.add_argument('input', help='input filename')
	parser.add_argument('donor', help='palette donor filename', nargs="?")
	args = parser.parse_args()

	output_img = force_to_palette(args.input, args.donor)

	output_img.save(args.input)


if __name__ == '__main__':
	main()

