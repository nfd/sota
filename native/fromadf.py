import json
import sys
import os
import struct
import urllib.request
import gzip

import libsotadisk
import iffilbm

OUTPUT_DIR_JSON='../shapes'
OUTPUT_DIR_INFO='../shapes'
OUTPUT_DIR_BINARY='data'
OUTPUT_DIR_ILBM='data'

# Magic image-finding regex: 0. +d. +.. +cc

animations = (
	# Initial hands move-in
	0x12bc,

	# Intro picture-in-picture dancing girl with gun
	0x2c00,

	# Main intro (Bond girls)
	0x10800,

	# Morph into first dancer
	0x67080,

	# First dancer (38s - 47s)
	0x67230,

	# 4: Second dancer (without cuts) (50s-53s)
	0x75c66,

	# Third dancer (cut in with second) (51s-54s?)
	0x77016,

	0x7789e,

	0x78104,

	# ?? broken, hat
	#0x8d840,

	# Crazy-hips dancer, morphs to geometric shapes and other dancers, ending face spinning away (1:02-1:10)
	0x8da00,

	# Loading screen, 23 frames.
	0x9541c,

	# Sequence after loading screen 
	#0x981ec,

	# Spin hands around over head, place on thighs, small crouch
	0x98a00,

	# Dancer, wearing hat, arms extended, moving hands up and down in sinuous motion
	0x99546,

	# facing right side, shimmy in from right, shimmy out
	0x9ae1a,

	# jumping 
	0x9d9c4,  # 34 (facing right, ends horizontal middle)

	0x9e1b8,  # 53 (somersault middle bottom to top to bottom)

	0x9f31a,  # 52 (kind of an unside down flip from bottom to top)

	0xa0552,  # 33 (backflip, ends up almost in crawling position)

	0xa12f2,  # splay out, 'fly'

	# "angley" distorted dancer (but not distorted?) (TODO where is this?)

	# Opening iris to square to female dancer, profile, to five-pointed shape, to same dancer in profile, turning towards
	# camera, to circle, to front-facing dancer, to collection of triangles at bottom of screen, to front-facing dancer,
	# to square filling the screen
	0xaface, # 232

	# dancer, unknown (suspiciously similar to the first one) -- probably distorted-zoomed dancer?
	0xb3274,

	# The grid thing.
	0xb989c,

	# Dancer, various shape morphs (which do not morph here)
	0xc0374,

	# dancer, various shape morphs (which do not morph here)
	0xc472a,

	# final two dancers, shown in close-up, drawn as outline in the demo, part 1 (male)
	0xcbd82,

	# final two dancers, part 2 (male)
	0xcd850,

	# Dancer, front view 
	0xcf46e,

	# Final two dancers, part 3 (female)
	0xd0b52,

	# Final two dancers, part 4 (female)
	0xd1872,

	# Arm outstretched, panning towards fingers and offscreen
	0xd2700,

	# Dancer (flickering portion?)
	0xd3c00,

	# Dancer (flickering portion?) #2
	0xd4488,

	# Dancer (flickering portion?) #3 (and end of ADF!)
	0xd82fa,
)

images = (
	('static1', 0xa2400, 352, 283, 1), # that's right, 352 by *283*.
	('static2', 0xa55d8, 352, 283, 1),
	('static3', 0xa87b0, 352, 283, 1),
	('static4', 0xab988, 352, 283, 1),
)

path = '../Spaceballs-StateOfTheArt.adf'

def write_binary(script, path_prefix):
	with open(path_prefix + '_anim.bin', 'wb') as h:
		h.write(struct.pack('>H', len(script['indices']))) # number of indices following

		for index in script['indices']:
			h.write(struct.pack('>H', index))

		h.write(bytes(script['data']))

def write_info(script, pathname):
	with open(pathname, 'w') as h:
		h.write('length: %d' % (len(script['indices'])))

def write_animation(image):
	filename_json = 'script%06x.json' % (image)
	pathname_json = os.path.join(OUTPUT_DIR_JSON, filename_json)
	pathname_bin  = os.path.join(OUTPUT_DIR_BINARY, '%06x' % (image))
	pathname_info = os.path.join(OUTPUT_DIR_INFO, 'script%06x.txt' % (image))

	with open(path, 'rb') as h:
		h.seek(image)
		script = libsotadisk.getscript(h)

		with open(pathname_json, 'w') as h:
			json.dump(script, h, sort_keys=True)

		write_binary(script, pathname_bin)
		write_info(script, pathname_info)
		
		print("Wrote", pathname_json)
	
	return filename_json

def write_all_animations():
	filenames = []
	for image in animations:
		filenames.append(write_animation(image))
		# break # just the first for now

	with open(os.path.join(OUTPUT_DIR_JSON, 'scripts.json'), 'w') as h:
		json.dump(filenames, h)

def write_all_images():
	with open(path, 'rb') as h:
		for filename, offset, width, height, num_planes in images:
			bytes_per_plane = width * height // 8

			h.seek(offset)
			planes = [h.read(bytes_per_plane) for plane_num in range(num_planes)]

			ilbm = iffilbm.to_iff(planes, width, height)

			pathname = os.path.join(OUTPUT_DIR_ILBM, filename) + '.iff'
			with open(pathname, 'wb') as out:
				out.write(ilbm)

			print('wrote %s' % (pathname,))
				

URL = "http://lardcave.net/sota/Spaceballs-StateOfTheArt.adf.gz"
MAX_LENGTH = 880 * 1024

def retrieve_disk_image():
	if os.path.exists(path):
		return
	
	print("%s not present. Press enter to download it from %s" % (path, URL))
	input()

	print("Downloading...")
	with urllib.request.urlopen(URL) as h:
		result = h.read(MAX_LENGTH)

	print("Decrunching...")
	with open(path, 'wb') as h:
		h.write(gzip.decompress(result))

retrieve_disk_image()
write_all_animations()
write_all_images()

