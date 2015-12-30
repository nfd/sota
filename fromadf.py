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
	0x13c6,

	# Intro picture-in-picture dancing girl with gun
	0x2e3a,

	# Main intro (Bond girls)
	0x10def,

	# First dancer (38s - 47s)
	0x68012,

	# Second dancer (without cuts) (50s-53s)
	0x75d7c,

	# Third dancer (cut in with second) (51s-54s?)
	0x770e0,

	0x77934,

	0x781ca,

	# ?? broken, hat
	0x8d84e,

	# Crazy-hips dancer, morphs to geometric shapes and other dancers, ending face spinning away and loading screen (1:02-1:10)
	0x8dd4e,

	# Sequence after loading screen w/ crouching ending at 1:36
	0x983b5,

	# Dancer with hat, cut in with previous.
	0x996dc,

	0x9af08,

	# jumping 
	0x9da4e,

	0x9e28e,

	0x9f3ec,

	0xa05d8,

	0xa133c,

	# "angley" distorted dancer (but not distorted?)
	0xb05fb,

	# dancer, unknown
	0xb35be,

	# TODO: lots of image-looking stuff here, but doesn't obviously fit a pattern. Maybe it's that grid thing?

	# Dancer, various shape morphs (which do not morph here)
	0xc0776,

	# dancer, various shape morphs (which do not morph here)
	0xc4b97,

	# final two dancers, shown in close-up, drawn as outline in the demo, part 1 (male)
	0xcbeb4,

	# final two dancers, part 2 (male)
	0xcd982,

	# Dancer, front view (TODO broken 0 portion)
	0xcf53c,

	# Final two dancers, part 3 (female)
	0xd0c08,

	# Final two dancers, part 4 (female)
	0xd1930,

	# Arm outstretched, panning towards fingers and offscreen
	0xd2796,

	# Dancer (flickering portion?)
	0xd3d51,

	# Dancer (flickering portion?) #2
	0xd4732,

	# Dancer (flickering portion?) #3 (and end of ADF!)
	0xd8504,
)

image = animations[0]


path = '/home/wzdd/Downloads/Spaceballs-StateOfTheArt (1).adf'

class ShapeMaker:
	def __init__(self, handle):
		self.handle = handle

	def getscript(self):
		script = []

		while True:
			cmd = struct.unpack('B', self.handle.read(1))[0]
			# TODO the 0 story is FUBAR
			if cmd == 0xff:
				# 0 = unknown. ff = likely end of frames
				print('End of frames %x' % (cmd))
				script.append((cmd, []))
				break
			elif cmd in (0, 1, 2, 3, 4, 5, 6):
				script.append((cmd, []))
			elif cmd == 7:
				script.append((cmd, list(self.handle.read(2)))) # unknown
			elif cmd == 8:
				script.append((cmd, list(self.handle.read(45)))) # unknown
			elif cmd == 0xc:
				script.append((cmd, list(self.handle.read(10)))) # unknown
			elif cmd == 0xe:
				script.append((cmd, list(self.handle.read(9)))) # unknown
			elif cmd == 0xf:
				script.append((cmd, list(self.handle.read(1)))) # unknown
			elif cmd in (0x27, 0x29):
				script.append((cmd, list(self.handle.read(11)))) # unknown
			elif cmd == 0x37:
				# End of frames?
				print("End of frames")
				script.append((cmd, []))
				break
			elif cmd == 0x3f and self.handle.tell() == 0x01231a:
				script.append((cmd, list(self.handle.read(49)))) # don't know what this is yet, collecting evidence.
			elif cmd & 0xd0 == 0xd0:
				self.get_vector(script, cmd)
			elif cmd in (0x76, 0x78):
				script.append((cmd, list(self.handle.read(4)))) # unknown
			elif cmd == 0x92:
				script.append((cmd, list(self.handle.read(7)))) # unknown
			elif cmd == 0x96:
				script.append((cmd, list(self.handle.read(14)))) # unknown
			elif cmd in (0xe6, 0xe7, 0xf2):
				script.append((cmd, list(self.handle.read(6)))) # unknown
			elif cmd == 0xe8:
				script.append((cmd, list(self.handle.read(7)))) # unknown
			else:
				print("Unknown cmd %02x @%06x" % (cmd, self.handle.tell() - 1))
				break

		print("Final index", hex(self.handle.tell()))
		return script

	def get_vector(self, script, cmd):
		position = self.handle.tell()

		points = None

		output_dict = {"position": '0x%x' % (position), "shape": []}

		num_points = struct.unpack('B', self.handle.read(1))[0]
		if(num_points == 0):
			# this appears to be a reset.
			script.append((cmd, output_dict))
			script.append((0, []))
			return

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

		script.append((cmd, output_dict))

with open(path, 'rb') as h:
	h.seek(image)

	shapemaker = ShapeMaker(h)

	filename = os.path.join(OUTPUT_DIR, 'script%06x.json' % (image))
	script = shapemaker.getscript()
	with open(filename, 'w') as h:
		json.dump(script, h, sort_keys=True, indent=4)
	
	print("Wrote", filename)



