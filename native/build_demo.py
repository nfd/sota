# Generate the `sotasequence.c` file used by the choreographer.
import struct

ENDIAN = '<'

MOD_INTRO = 1
MOD_MAIN = 2

DEMO = [
		# Intro: hand moving out
		('after', 'clear', {'plane': 'all'}),
		('after', 'palette', {'values': (0xff110022, 0xffffffff, 0xff110022, 0xffffffff)}),
		('after', 'fadeto', {'ms': 2000, 'values': (0xff110022, 0xff221144, 0xff110022, 0xff110033)}),
		('simultaneously', 'anim', {'name': 0x10800, 'from': 0, 'to': 50}),
		#('after', 'pause', {'ms': 500}),
		('after', 'mod', {'type': 'start', 'name': MOD_INTRO}),
		# Girl with gun displayed inside hand
		('after', 'anim', {'name': 0x2c00, 'from': 0, 'to': 141, 'bitplane': 1}),
		('after', 'clear', {'plane': 1}),
		# Hand zooms out; dancing Bond girls. Frame 51 is skipped because it's weird (draws two hands almost over each other)
		# xor is manually disabled -- this is probably also encoded in draw cmds but whatever.
		('after', 'anim', {'name': 0x10800, 'from': 52, 'to': 145, 'xor': 0}),
		('after', 'anim', {'name': 0x10800, 'from': 146, 'to': 334, 'xor': 1}),
		('after', 'mod', {'type': 'stop', 'name': MOD_INTRO}),


		# Title: STATE OF THE ART, credits, dragon pic
		# TODO: find the heartbeat sound

		#('after', 'mod', {'type': 'start', 'name': MOD_MAIN}),
		# First dancer
		('after', 'end', {}),
]

CMD_END = 0
CMD_CLEAR = 1
CMD_PALETTE = 2
CMD_FADETO = 3
CMD_ANIM = 4
CMD_PAUSE = 5
CMD_MOD = 6

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
	ms = args['ms']
	return ms, struct.pack(ENDIAN + 'II', CMD_FADETO, ms) + encode_palette(args)[1]

def encode_anim(args):
	name = args['name']
	frame_from = args['from']
	frame_to   = args['to']
	ms_per_frame = args.get('msperframe', 40)
	bitplane = args.get('bitplane', 0)
	xor = args.get('xor', 1)

	ms = ms_per_frame * (frame_to - frame_from)
	encoded = struct.pack(ENDIAN + 'IIIIIII', CMD_ANIM, ms_per_frame, frame_from, frame_to, name, bitplane, xor)

	return ms, encoded

def encode_end(args):
	return 0, struct.pack(ENDIAN + 'I', CMD_END)

def encode_pause(args):
	return args['ms'], struct.pack(ENDIAN + 'I', CMD_PAUSE)

def encode_mod(args):
	if args['type'] == 'start':
		packme = (CMD_MOD, 1, args['name'])
	elif args['type'] == 'stop':
		packme = (CMD_MOD, 2, 0)

	return 0, struct.pack(ENDIAN + 'III', *packme)

def encode_demo_entry(entry):
	# return (tick count, encoded entry)
	command = entry[1]
	args = entry[2]

	encoder = globals()['encode_%s' % (command)]
	return encoder(args)

def get_demo_sequence():
	# The time unit is milliseconds. Most animations run at 25fps or 40ms/frame.
	# This can be changed if necessary using the 'msperframe' key of an 'anim' entry.

	# Returns an encoded demo sequence.

	previous_start = 0 # ms
	previous_end = 0 # ms

	for entry in DEMO:

		this_entry_ms, encoded_entry = encode_demo_entry(entry)
		
		if entry[0] == 'after':
			this_start = previous_end
			this_end = this_start + this_entry_ms
		elif entry[0] == 'simultaneously':
			this_start = previous_start
			this_end = max(previous_end, this_start + this_entry_ms)
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

