"""
Convert an RGBA png to a palettised one in a pretty mean way.
"""
import sys
import argparse
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

def force_to_palette(filename, donor_palette_filename):
	donor_img = Image.open(donor_palette_filename)
	palette = pilutils.decode_lousy_pil_palette_data(donor_img.palette.getdata())

	img = Image.open(filename)

	dst = Image.new("P", img.size)

	print(donor_img.palette.getdata())
	dst.putpalette(donor_img.palette.getdata()[1])

	width, height = img.size

	for idx, pixel in enumerate(img.getdata()):

		colour = closest_match(pixel, palette)
		dst.putpixel((idx % width, idx // width), colour)
	
	return dst

def main():
	parser = argparse.ArgumentParser()
	parser.add_argument('input', help='input filename')
	parser.add_argument('donor', help='palette donor filename')
	args = parser.parse_args()

	output_img = force_to_palette(args.input, args.donor)

	output_img.save(args.input)


if __name__ == '__main__':
	main()

