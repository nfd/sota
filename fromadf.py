import json
import sys
import os
import struct

OUTPUT_DIR_JSON='shapes'
OUTPUT_DIR_BINARY='native/data'

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

	# Crazy-hips dancer, morphs to geometric shapes and other dancers, ending face spinning away and loading screen (1:02-1:10)
	0x8da00,

	# Sequence after loading screen w/ crouching ending at 1:36
	# TODO
	#0x983b5,

	# Dancer with hat, cut in with previous.
	0x99546,

	0x9ae1a,

	# jumping 
	0x9d9c4,

	0x9e1b8,

	0x9f31a,

	0xa0552,

	0xa12f2,

	# "angley" distorted dancer (but not distorted?)
	# TODO -- the index for this is FUBAR
	#0xb05fb,

	# dancer, unknown (suspiciously similar to the first one)
	0xb3274,

	# TODO: lots of image-looking stuff here, but doesn't obviously fit a pattern. Maybe it's that grid thing?

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

class ShapeMaker:
	def __init__(self, handle):
		self.handle = handle

	def getscript(self):
		script = {}

		# Read the index first.
		startpos = self.handle.tell()

		num_indices = struct.unpack('>H', self.handle.read(2))[0]
		indices = []

		for i in range(num_indices + 1):
			index = struct.unpack('>I', self.handle.read(4))[0]
			indices.append(index)

		# find the smallest index and subtract to get 0-based indices.
		script_offset = min(indices)
		for i in range(len(indices)):
			indices[i] -= script_offset
			assert indices[i] >= 0

		# Read the script. Note that indices may repeat, hence the dict.
		for index in indices:
			pos = startpos + index + script_offset
			if index not in script:
				self.handle.seek(pos)
				script[index] = self.read_one()
				if script[index] is None:
					print("Early exit")
					break

		# Combine all the script portions into one byte array.
		max_length = max(script.keys())
		max_length += len(script[max_length])

		all_scripts = [0] * max_length

		for index, thescript in script.items():
			all_scripts[index:index+len(thescript)] = thescript

		return {'indices': indices, 'data': all_scripts}

	def next_byte(self):
		return struct.unpack('B', self.handle.read(1))[0]

	def read_one(self):
		# Read one combined drawing command.
		data = []

		cmd_major = self.next_byte()
		data.append(cmd_major)

		for i in range(cmd_major):
			cmd_minor = self.next_byte()
			data.append(cmd_minor)

			if cmd_minor & 0xf0 == 0xd0:
				# Drawing command
				num_points = self.next_byte()
				data.append(num_points)
				data.extend(list(self.handle.read(num_points * 2)))
			elif cmd_minor in (0xe6, 0xe7, 0xe8):
				# Tweening
				data.extend(list(self.handle.read(6)))
			elif cmd_minor == 0xf2:
				unknown = list(self.handle.read(6))
				print(unknown)
				data.extend(unknown)
			else:
				print("Unknown cmd %02x @%06x" % (cmd_minor, self.handle.tell() - 1))
				raise NotImplementedError()

		return data

path = 'Spaceballs-StateOfTheArt.adf'

def write_binary(script, path_prefix):
	with open(path_prefix + '_index.bin', 'wb') as h:
		for index in script['indices']:
			h.write(struct.pack('>H', index))

	with open(path_prefix + '_anim.bin', 'wb') as h:
		h.write(bytes(script['data']))

def write_animation(image):
	filename_json = 'script%06x.json' % (image)
	pathname_json = os.path.join(OUTPUT_DIR_JSON, filename_json)
	pathname_bin  = os.path.join(OUTPUT_DIR_BINARY, '%06x' % (image))

	with open(path, 'rb') as h:
		h.seek(image)
		shapemaker = ShapeMaker(h)

		script = shapemaker.getscript()
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

write_all_animations()

