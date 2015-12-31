import json
import sys
import os
import struct

OUTPUT_DIR='shapes'

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

		# Read the script. Note that indices may repeat, hence the dict.
		for index in indices:
			pos = startpos + index
			if index not in script:
				print(hex(index))
				self.handle.seek(pos)
				#
				# have to pass startpos down because tweens
				# require it
				script[index] = self.read_one(startpos)
				if script[index] is None:
					print("Early exit")
					break

		return {'indices': indices, 'frames': script}

	def read_one(self, startpos):
		cmd_major = struct.unpack('B', self.handle.read(1))[0]
		script = [cmd_major]

		if cmd_major not in (1, 2, 3, 4, 5, 6):
			print("Unknown major cmd %x" % (cmd_major))
			return None
	

		cmd_start = self.handle.tell()
		for i in range(cmd_major):

			cmd_minor = struct.unpack('B', self.handle.read(1))[0]
			element = None

			# TODO the 0 story is FUBAR
			if cmd_minor == 0xff:
				# 0 = unknown. ff = likely end of frames
				print('End of frames %x' % (cmd_minor))
				element = (cmd_minor, [])
			elif cmd_minor in (0, 1, 2, 3, 4, 5, 6):
				element = (cmd_minor, [])
			elif cmd_minor == 7:
				element = (cmd_minor, list(self.handle.read(2))) # unknown
			elif cmd_minor == 8:
				element = (cmd_minor, list(self.handle.read(45))) # unknown
			elif cmd_minor == 0xc:
				element = (cmd_minor, list(self.handle.read(10))) # unknown
			elif cmd_minor == 0xe:
				element = (cmd_minor, list(self.handle.read(9))) # unknown
			elif cmd_minor == 0xf:
				element = (cmd_minor, list(self.handle.read(1))) # unknown
			elif cmd_minor in (0x27, 0x29):
				element = (cmd_minor, list(self.handle.read(11))) # unknown
			elif cmd_minor == 0x3f and self.handle.tell() == 0x01231a:
				element = (cmd_minor, list(self.handle.read(49))) # don't know what this is yet, collecting evidence.
			elif cmd_minor & 0xd0 == 0xd0:
				element = self.get_vector(cmd_minor)
			elif cmd_minor in (0x76, 0x78):
				element = (cmd_minor, list(self.handle.read(4))) # unknown
			elif cmd_minor == 0x92:
				element = (cmd_minor, list(self.handle.read(7))) # unknown
			elif cmd_minor == 0x96:
				element = (cmd_minor, list(self.handle.read(14))) # unknown
			elif cmd_minor == 0xe6:
				# Tweening -- probably also applies to e7
				relative_to = cmd_start - startpos 
				idx_from, idx_to, current_step, total_steps = struct.unpack('>HHBB', self.handle.read(6))
				# Turns out that these shapes can refer to INSIDE PORTIONS of draw-multiple commands! Aargh...
				# have to change file format.
				element = (cmd_minor, (str(relative_to - idx_from), str(relative_to + idx_to), current_step, total_steps))
			elif cmd_minor in (0xe7, 0xf2):
				element = (cmd_minor, list(self.handle.read(6))) # unknown
			elif cmd_minor == 0xe8:
				element = (cmd_minor, list(self.handle.read(7))) # unknown
			else:
				print("Unknown cmd %02x @%06x" % (cmd_minor, self.handle.tell() - 1))

			if element is not None:
				script.append(element)
			else:
				return None

		return script

	def get_vector(self, cmd_minor):
		position = self.handle.tell()

		points = None

		output_dict = {"position": '0x%x' % (position), "shape": []}

		num_points = struct.unpack('B', self.handle.read(1))[0]
		points = self.handle.read(num_points * 2)

		# Load the shape
		shape = []
		for i in range(num_points):
			idx = i * 2
			# data in (y, x) format
			shape.append((points[idx + 1], points[idx]) )

		# Write JSON data
		output_dict['shape'] = shape
		print("Found vector at position %x" % (position))

		return (cmd_minor, output_dict)

path = 'Spaceballs-StateOfTheArt.adf'

def write_animation(image):
	filename = 'script%06x.json' % (image)
	pathname = os.path.join(OUTPUT_DIR, filename)

	with open(path, 'rb') as h:
		h.seek(image)
		shapemaker = ShapeMaker(h)

		script = shapemaker.getscript()
		with open(pathname, 'w') as h:
			json.dump(script, h, sort_keys=True, indent=4)
		
		print("Wrote", pathname)
	
	return filename

def write_all_animations():
	filenames = []
	for image in animations:
		filenames.append(write_animation(image))
		# break # just the first for now

	with open(os.path.join(OUTPUT_DIR, 'scripts.json'), 'w') as h:
		json.dump(filenames, h)

write_all_animations()

