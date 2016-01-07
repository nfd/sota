# Generate the `sotasequence.c` file used by the choreographer.
import struct

ENDIAN = '<'

DEMO = [
		# Intro: hand moving out
		('after', 'clear', {'plane': 'all'}),
		('after', 'palette', {'values': (0xffffffff,)}),
		('after', 'fadeto', {'ticks': 20, 'values': (0xff110022, 0xff221144, 0xff110022, 0xff110033)}),
		('simultaneously', 'anim', {'name': 0x10800, 'from': 0, 'to': 52}),
		# Girl with gun displayed inside hand
		('after', 'anim', {'name': 0x2c00, 'from': 0, 'to': 141}),
		# Hand zooms out; dancing Bond girls
		('after', 'anim', {'name': 0x10800, 'from': 53, 'to': 334}),

		# Title: STATE OF THE ART, credits, dragon pic

		# First dancer
		('after', 'end', {}),
]

CMD_END = 0
CMD_CLEAR = 1
CMD_PALETTE = 2
CMD_FADETO = 3
CMD_ANIM = 4

def encode_clear(args):
	plane = args.get('plane', 'all')
	if plane == 'all':
		plane = 0xff
	else:
		plane = int(plane)

	return 0, struct.pack(ENDIAN + 'II', CMD_CLEAR, plane)

def encode_palette(args):
	# No-value palettes result in all-zero entries.
	values = list(args.get('values', ()))
	if len(values) < 32:
		values += [0xff000000] * (32 - len(values))

	assert len(values) == 32
	return 0, struct.pack(ENDIAN + 'II' + (32 * 'I'), *([CMD_PALETTE, 32] + values ))

def encode_fadeto(args):
	# This is implemented as a "fade to" command followed by the destination palette.
	ticks = args['ticks']
	return ticks, struct.pack(ENDIAN + 'I', CMD_FADETO) + encode_palette(args)[1]

def encode_anim(args):
	name = args['name']
	frame_from = args['from']
	frame_to   = args['to']
	ticks_per_frame = args.get('ticksperframe', 2)

	ticks = ticks_per_frame * (frame_to - frame_from)
	encoded = struct.pack(ENDIAN + 'IIIII', CMD_ANIM, ticks_per_frame, frame_from, frame_to, name)

	return ticks, encoded

def encode_end(args):
	return 0, struct.pack(ENDIAN + 'I', CMD_END)

def encode_demo_entry(entry):
	# return (tick count, encoded entry)
	tick_count = 0
	command = entry[1]
	args = entry[2]

	if command == 'clear':
		return encode_clear(args)
	elif command == 'palette':
		return encode_palette(args)
	elif command == 'fadeto':
		return encode_fadeto(args)
	elif command == 'anim':
		return encode_anim(args)
	elif command == 'end':
		return encode_end(args)
	else:
		raise NotImplementedError(command)

def get_demo_sequence():
	# The base time unit ('tick') is 20 milliseconds, which is one refresh
	# period (1/50th of a second) on a PAL Amiga. However, many (all?) the
	# animations are done at half speed, i.e. 25 frames per second. The
	# ticksperframe key can be used in 'anim' sequences to set the frame rate.
	# It defaults to 2.

	# Returns an encoded demo sequence.

	previous_start = 0
	previous_end = 0

	for entry in DEMO:

		this_entry_tick_count, encoded_entry = encode_demo_entry(entry)
		
		if entry[0] == 'after':
			this_start = previous_end
			this_end = this_start + this_entry_tick_count
		elif entry[0] == 'simultaneously':
			this_start = previous_start
			this_end = max(previous_end, this_start + this_entry_tick_count)
		else:
			raise NotImplementedError()
	
		# Two-byte frame number, two-byte length (including frame # and length), then encoded data.
		print(this_start, entry)
		yield struct.pack(ENDIAN + 'II', this_start, len(encoded_entry) + 8) + encoded_entry

		previous_start = this_start
		previous_end = this_end

filename = 'data/choreography.bin'
with open(filename, 'wb') as h:
	for encoded in get_demo_sequence():
		h.write(encoded)

print("wrote", filename)

