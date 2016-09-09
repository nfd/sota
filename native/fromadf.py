import json
import sys
import os
import struct
import urllib.request
import gzip

import libsotadisk

OUTPUT_DIR_JSON='../shapes'
OUTPUT_DIR_BINARY='data'

"""
Data format. View it as a finite-state machine, I guess.
"""

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
	0x9d9c4,

	0x9e1b8,

	0x9f31a,

	0xa0552,

	0xa12f2,

	# "angley" distorted dancer (but not distorted?) (TODO where is this?)

	# Opening iris to square to female dancer, profile, to five-pointed shape, to same dancer in profile, turning towards
	# camera, to circle, to front-facing dancer, to collection of triangles at bottom of screen, to front-facing dancer,
	# to square filling the screen
	0xaface,

	# dancer, unknown (suspiciously similar to the first one)
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

path = '../Spaceballs-StateOfTheArt.adf'

def write_binary(script, path_prefix):
	with open(path_prefix + '_anim.bin', 'wb') as h:
		h.write(struct.pack('>H', len(script['indices']))) # number of indices following

		for index in script['indices']:
			h.write(struct.pack('>H', index))

		h.write(bytes(script['data']))

def write_animation(image):
	filename_json = 'script%06x.json' % (image)
	pathname_json = os.path.join(OUTPUT_DIR_JSON, filename_json)
	pathname_bin  = os.path.join(OUTPUT_DIR_BINARY, '%06x' % (image))

	with open(path, 'rb') as h:
		h.seek(image)
		script = libsotadisk.getscript(h)

		with open(pathname_json, 'w') as h:
			json.dump(script, h, sort_keys=True)

		write_binary(script, pathname_bin)
		
		print("Wrote", pathname_json)
	
	return filename_json

def write_all_animations():
	filenames = []
	for image in animations:
		filenames.append(write_animation(image))
		# break # just the first for now

	with open(os.path.join(OUTPUT_DIR_JSON, 'scripts.json'), 'w') as h:
		json.dump(filenames, h)

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

