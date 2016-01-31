# Generate the `sotasequence.c` file used by the choreographer.
import struct
import urllib.request
import urllib.parse
import os
import gzip

from wad import Wad

ENDIAN = '<'

FONTMAP = [
0,   208, 43,  255, # a
48,  208, 48,  255, # b
96,  208, 133, 255, # c
144, 208, 180, 255, # d
192, 208, 220, 255, # e
240, 208, 262, 255, # f
0,   160, 37,  207, # g
48,  160, 84,  207, # h
96,  160, 107, 207, # i
144, 160, 144, 207, # j
192, 160, 227, 207, # k
240, 160, 268, 207, # l
0,   112, 37,  159, # m
48,  112, 83,  159, # n
96,  112, 137, 159, # o
144, 112, 177, 159, # p
192, 112, 192, 159, # q
240, 112, 276, 159, # r
0,   64,  34,  111, # s
48,  64,  81,  111, # t
96,  64,  128, 111, # u
144, 64,  187, 111, # v
192, 64,  192, 111, # w
240, 64,  278, 111, # x
0,   16,  38,  63, # y 
48,  16,  77,  63, # z 
96,  16,  110, 63, # !, mapped as [
]

DEMO = [
		# Intro: hand moving out
		('after', 'clear', {'plane': 'all'}),
		('after', 'palette', {'values': (0xff110022, 0xffffffff, 0xff110022, 0xffffffff)}),
		('after', 'fadeto', {'ms': 2000, 'values': (0xff110022, 0xff221144, 0xff110022, 0xff110033)}),
		('simultaneously', 'anim', {'name': 0x10800, 'from': 0, 'to': 50}),
		#('after', 'pause', {'ms': 500}),
		('after', 'mod', {'type': 'start', 'name': 'data/stateldr.mod'}),
		# Girl with gun displayed inside hand
		('after', 'anim', {'name': 0x2c00, 'from': 0, 'to': 141, 'plane': 1}),
		('after', 'clear', {'plane': 1}),
		# Hand zooms out; dancing Bond girls. Frame 51 is skipped because it's weird (draws two hands almost over each other)
		# xor is manually disabled -- this is probably also encoded in draw cmds but whatever.
		('after', 'anim', {'name': 0x10800, 'from': 52, 'to': 145, 'xor': 0}),
		('after', 'anim', {'name': 0x10800, 'from': 146, 'to': 334, 'xor': 1}),
		('after', 'mod', {'type': 'stop'}),

		# Title: STATE OF THE ART, credits, dragon pic
		('after', 'ilbm', {'name': 'data/state.iff', 'display': 'fullscreen'}),
		('after', 'sound', {'name': 'data/heartbeat.wav', 'ms': 1000}),
		('after', 'fadeto', {'ms': 250, 'values': ()}),

		('after', 'ilbm', {'name': 'data/of.iff', 'display': 'fullscreen'}),
		('after', 'sound', {'name': 'data/heartbeat.wav', 'ms': 1000}),
		('after', 'fadeto', {'ms': 250, 'values': ()}),

		('after', 'ilbm', {'name': 'data/the.iff', 'display': 'fullscreen'}),
		('after', 'sound', {'name': 'data/heartbeat.wav', 'ms': 1000}),
		('after', 'fadeto', {'ms': 250, 'values': ()}),

		('after', 'ilbm', {'name': 'data/art.iff', 'display': 'fullscreen'}),
		('after', 'sound', {'name': 'data/heartbeat.wav', 'ms': 1000}),
		('after', 'fadeto', {'ms': 250, 'values': ()}),

		('after', 'clear', {'plane': 'all'}),
		('after', 'ilbm', {'name': 'data/authors.iff', 'display': 'fullscreen', 'fadein_ms': 2000}),
		('after', 'pause', {'ms': 3000}),

		('after', 'fadeto', {'ms': 250, 'values': (0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff)}),
		('after', 'ilbm', {'name': 'data/dragon.iff', 'display': 'fullscreen', 'fadein_ms': 500}),
		('after', 'pause', {'ms': 4000}),

		# End of intro finally! fade 'er out.
		('after', 'fadeto', {'ms': 1000, 'values': ()}),

		# Morph to first dancer
		('after', 'clear', {'plane': 'all'}),
		('simultaneously', 'starteffect', {'name': 'spotlights'}),
		('after', 'mod', {'type': 'start', 'name': 'data/condom corruption.mod'}),
		('after', 'palette', {'values': (0xff000000, 0xff000000, 0xff117700, 0xff000000, 0xff770077, 0xff000000, 0xff771111)}),
		('after', 'fadeto', {'ms': 1000, 'values': (0xff000000, 0xff000000, 0xff117700, 0xffdd0000, 0xff770077, 0xffdd7700, 0xff771111,
			0xffbbbb00)}),
		('simultaneously', 'anim', {'name': 0x67080, 'from': 0, 'to': 25}),
		('after', 'anim', {'name': 0x67230, 'from': 0, 'to': 195}),
		('after', 'fadeto', {'ms': 250, 'values': ()}),
		('simultaneously', 'anim', {'name': 0x67230, 'from': 196, 'to': 209}),
		('after', 'starteffect', {'name': 'nothing'}),

		# VOTE! VOTE! VOTE!
		('after', 'clear', {'plane': 'all'}),
		# black-on-white version
		('after', 'alternate_palette', {'idx': 0, 'values': (0xffeeffff, 0xff000000, 0xff779999, 0xffddeeee, 0xff114444, 0xff447777, 0xff003333, 0xffaaaadd)}),
		# white-on-black version
		('after', 'alternate_palette', {'idx': 1, 'values': (0xff000000, 0xffeeffff, 0xff336666, 0xff113333, 0xffaabbbb, 0xff778888, 0xffccdddd, 0xff224444)}),
		('after', 'loadfont', {'name': 'data/fontmap.iff', 'startchar': 'A', 'map' : FONTMAP}),
		('after', 'starteffect', {'name': 'votevotevote', 
			'text': ('VOTE[', 'VERY', 'LOTSA', 'NO', 'REAL', 'COOL', 'DULL', 'HIGH', 'FUNNY',
				'VOTE[', 'FUNK', 'KRAZY', 'NICE', 'RAPID', 'RAP', 'RIPPED', 'UGLY', 'CLASS',
				'VOTE[', 'CODE', 'MUSIC', 'GFX',)}),

		('after', 'pause', {'ms': 3000}),
		('after', 'starteffect', {'name': 'nothing'}),
		('after', 'clear', {'plane': 'all'}),

		# Second set of dancers. Fast cuts (1 second per shot) of two separate dancers, 
		# one with orange-brown trails and one with white-blue trails.
		# Palette 0: Orange-brown dancer on white
		('after', 'alternate_palette', {'idx': 0, 'values': (
			0xffffffff, 0xffcc7733, 0xffdd7733, 0xffaa3311, 0xffdd8844, 0xffaa3311, 0xffaa4422, 0xff661100,
			0xffee9944, 0xffaa3311, 0xffaa3311, 0xffaa3311, 0xffbb4422, 0xffaa3311, 0xff771100, 0xff440000,
			0xffffaa55, 0xffcc6633, 0xffaa3311, 0xff992211, 0xffaa3311, 0xffaa3311, 0xffaa3311, 0xff661100,
			0xffbb6622, 0xff881100, 0xffaa3311, 0xff550000, 0xff771100, 0xff550000, 0xff440000, 0xff330000)}),
		# Palette 1: Blue-white dancer on black.
		('after', 'alternate_palette', {'idx': 1, 'values': (
			0xff000000, 0xff3388cc, 0xff2288cc, 0xff55ccee, 0xff2277bb, 0xff55ccee, 0xff55bbdd, 0xff99eeff,
			0xff1166bb, 0xff55ccee, 0xff55ccee, 0xff55ccee, 0xff44bbdd, 0xff55ccee, 0xff88eeff, 0xffbbffff,
			0xff0055aa, 0xff3399cc, 0xff55ccee, 0xff66ddee, 0xff55ccee, 0xff55ccee, 0xff55ccee, 0xff99eeff,
			0xff44aadd, 0xff77eeff, 0xff55ccee, 0xffaaffff, 0xff88eeff, 0xffaaffff, 0xffbbffff, 0xffccffff)}),
		# Effect for this scene is a trail of onionskinned previous frames
		('after', 'starteffect', {'name': 'delayedblit'}),
		# 1
		('after', 'use_alternate_palette', {'idx': 0}),
		('after', 'anim', {'name': 0x75c66, 'from': 0, 'to': 21}),
		('after', 'clear', {'plane': 'all'}),
		#2
		('after', 'use_alternate_palette', {'idx': 1}),
		('after', 'anim', {'name': 0x77016, 'from': 0, 'to': 21}),
		('after', 'clear', {'plane': 'all'}),
		#3
		('after', 'use_alternate_palette', {'idx': 0}),
		('after', 'anim', {'name': 0x75c66, 'from': 21, 'to': 42}),
		('after', 'clear', {'plane': 'all'}),
		#4: TODO CROP
		('after', 'use_alternate_palette', {'idx': 1}),
		('after', 'anim', {'name': 0x77016, 'from': 21, 'to': 42}),
		('after', 'clear', {'plane': 'all'}),
		#5
		('after', 'use_alternate_palette', {'idx': 0}),
		('after', 'anim', {'name': 0x78104, 'from': 0, 'to': 21}),
		('after', 'clear', {'plane': 'all'}),
		#6
		('after', 'use_alternate_palette', {'idx': 1}),
		('after', 'anim', {'name': 0x77016, 'from': 0, 'to': 21}),
		('after', 'clear', {'plane': 'all'}),
		#7
		('after', 'use_alternate_palette', {'idx': 0}),
		('after', 'anim', {'name': 0x75c66, 'from': 0, 'to': 21}),
		('after', 'clear', {'plane': 'all'}),
		#8
		('after', 'use_alternate_palette', {'idx': 1}),
		('after', 'anim', {'name': 0x7789e, 'from': 0, 'to': 21}),
		('after', 'clear', {'plane': 'all'}),
		#9
		('after', 'use_alternate_palette', {'idx': 0}),
		('after', 'anim', {'name': 0x77016, 'from': 16, 'to': 36}),
		('after', 'clear', {'plane': 'all'}),
		#10
		('after', 'use_alternate_palette', {'idx': 1}),
		('after', 'anim', {'name': 0x78104, 'from': 0, 'to': 21}),
		('after', 'clear', {'plane': 'all'}),
		#11
		('after', 'use_alternate_palette', {'idx': 0}),
		('after', 'anim', {'name': 0x77016, 'from': 0, 'to': 21}),
		('after', 'clear', {'plane': 'all'}),
		#12
		('after', 'use_alternate_palette', {'idx': 1}),
		('after', 'anim', {'name': 0x7789e, 'from': 10, 'to': 30}),
		('after', 'clear', {'plane': 'all'}),
		#13
		('after', 'use_alternate_palette', {'idx': 0}),
		('after', 'anim', {'name': 0x77016, 'from': 20, 'to': 30}),
		('after', 'clear', {'plane': 'all'}),

		('after', 'end', {}),
]

CMD_END = 0
CMD_CLEAR = 1
CMD_PALETTE = 2
CMD_FADETO = 3
CMD_ANIM = 4
CMD_PAUSE = 5
CMD_MOD = 6
CMD_ILBM = 7
CMD_SOUND = 8
CMD_STARTEFFECT = 9
CMD_LOADFONT = 10
CMD_ALTERNATE_PALETTE = 11
CMD_USE_ALTERNATE_PALETTE = 12

NET_USE_OK = False
MAX_FILE_LENGTH = 1024 * 1024

URL_BASE = "http://lardcave.net/sota/"

def get_file(local_filename):
	global NET_USE_OK

	if os.path.exists(local_filename):
		return

	url = URL_BASE + urllib.parse.quote(local_filename + ".gz")

	if NET_USE_OK is False:
		print("%s not present. Press enter to download it (and any other required files) from %s" % (local_filename, url))
		input()

	NET_USE_OK = True

	print("Downloading %s" % (url))
	with urllib.request.urlopen(url) as h:
		result = h.read(MAX_FILE_LENGTH)

	if not os.path.exists('data'):
		os.mkdir('data')

	with open(local_filename, 'wb') as h:
		h.write(gzip.decompress(result))

def encode_clear(wad, args):
	plane = args.get('plane', 'all')
	if plane == 'all':
		plane = 0xff
	else:
		plane = int(plane)

	return 0, struct.pack(ENDIAN + 'II', CMD_CLEAR, plane)

def _get_32_palette_entries(values):
	if len(values) < 32:
		values += [0xff000000] * (32 - len(values))

	assert len(values) == 32
	return struct.pack(ENDIAN + (32 * 'I'), *values)

def encode_palette(wad, args):
	# No-value palettes result in all-zero entries.
	values = list(args.get('values', ()))

	return 0, struct.pack(ENDIAN + 'II', CMD_PALETTE, 32) + _get_32_palette_entries(values)

def encode_alternate_palette(wad, args):
	values = list(args.get('values', ()))

	return 0, struct.pack(ENDIAN + 'II', CMD_ALTERNATE_PALETTE, args['idx']) + _get_32_palette_entries(values)

def encode_use_alternate_palette(wad, args):
	return 0, struct.pack(ENDIAN + 'II', CMD_USE_ALTERNATE_PALETTE, args['idx'])

def encode_fadeto(wad, args):
	# This is implemented as a "fade to" command followed by the destination palette.
	ms = args['ms']
	return ms, struct.pack(ENDIAN + 'II', CMD_FADETO, ms) + encode_palette(wad, args)[1]

def encode_anim(wad, args):
	idx_fn = 'data/%06x_index.bin' % (args['name'])
	get_file(idx_fn)
	idx_idx = wad.add(idx_fn)

	data_fn = 'data/%06x_anim.bin' % (args['name'])
	get_file(data_fn)
	data_idx = wad.add(data_fn)

	frame_from = args['from']
	frame_to   = args['to']
	ms_per_frame = args.get('msperframe', 40)
	bitplane = args.get('plane', 0)
	xor = args.get('xor', 1)

	ms = ms_per_frame * (1 + frame_to - frame_from)
	encoded = struct.pack(ENDIAN + 'IIIIIIII', CMD_ANIM, ms_per_frame, frame_from, frame_to, idx_idx, data_idx, bitplane, xor)

	return ms, encoded

def encode_end(wad, args):
	return 0, struct.pack(ENDIAN + 'I', CMD_END)

def encode_pause(wad, args):
	return args['ms'], struct.pack(ENDIAN + 'I', CMD_PAUSE)

def encode_mod(wad, args):
	if args['type'] == 'start':
		get_file(args['name'])
		packme = (CMD_MOD, 1, wad.add(args['name']))
	elif args['type'] == 'stop':
		packme = (CMD_MOD, 2, 0)

	return 0, struct.pack(ENDIAN + 'III', *packme)

def encode_ilbm(wad, args):
	get_file(args['name'])
	file_idx = wad.add(args['name'])

	# display type: fullscreen = 0, centre = 1
	display_type = {'fullscreen': 0, 'centre': 1}[args.get('display', 'fullscreen')]

	# fade-in milliseconds 
	# if set, fade-ins happen from the currently-set palette, i.e. the ILBM is displayed without
	# changing the palette and a palette lerp is initiated to the ILBM colours. 
	fade_in_ms = args.get('fadein_ms', 0)

	return fade_in_ms, struct.pack(ENDIAN + 'IIII', CMD_ILBM, file_idx, display_type, fade_in_ms)

def encode_sound(wad, args):
	get_file(args['name'])
	file_idx = wad.add(args['name'])

	return args['ms'], struct.pack(ENDIAN + 'II', CMD_SOUND, file_idx)

def pad_to_i32(block):
	if len(block) % 4 != 0:
		block += b'\00' * (4 - (len(block) % 4))
	
	return block

def pack_text_block(text_block):
	# Format:
	# I32: length of entire sub-block (including this length)
	# I32: number of entries
	# S  : position S (for relative positioning below)
	# I16: position of entry 1 (relative to position S)
	# I16: position of entry 2 (...)
	# ...: position of entry N (...)
	# ...: position of entry N+1 if it exists (since positions are used in text length calculations)
	# I8*: text of entry 1
	# ...
	# Result is padded to I32.

	text_bytes = []
	text_indices = []
	next_index = 2 * (len(text_block) + 1) # add a dummy index to help us figure out length of final character.
	for t in text_block:
		text_indices.append(struct.pack(ENDIAN + 'H', next_index))
		encoded = t.encode('utf-8')
		text_bytes.append(encoded)
		next_index += len(encoded)

	# dummy final index
	text_indices.append(struct.pack(ENDIAN + 'H', next_index))
	
	indices_and_bytes = pad_to_i32(b''.join(text_indices) + b''.join(text_bytes))

	return struct.pack(ENDIAN + 'II', len(indices_and_bytes) + 8, len(text_block)) + indices_and_bytes

def encode_starteffect(wad, args):
	effect_num = {'nothing': 0, 'spotlights': 1, 'votevotevote': 2, 'delayedblit': 3}[args['name']]
	#files_idx = [wad.add(filename) for filename in args.get('files', ())]
	#files_packed = struct.pack(ENDIAN + ('I' * len(files_idx)), *files_idx)

	# text block for VOTE! VOTE! VOTE! effect
	packed_text = pack_text_block(args.get('text', ()))

	return 0, struct.pack(ENDIAN + 'II', CMD_STARTEFFECT, effect_num) + packed_text

def encode_loadfont(wad, args):
	get_file(args['name'])
	file_idx = wad.add(args['name'])
	startchar = ord(args['startchar'])
	numchars = len(args['map']) // 4
	fontmap = struct.pack(ENDIAN + ('H' * len(args['map'])), *args['map'])
	packed = struct.pack(ENDIAN + 'IIII', CMD_LOADFONT, file_idx, startchar, numchars) + fontmap
	return 0, packed

def encode_demo_entry(wad, entry):
	# return (tick count, encoded entry)
	command = entry[1]
	args = entry[2]

	encoder = globals()['encode_%s' % (command)]
	return encoder(wad, args)

def get_demo_sequence(wad):
	# The time unit is milliseconds. Most animations run at 25fps or 40ms/frame.
	# This can be changed if necessary using the 'msperframe' key of an 'anim' entry.

	# Returns an encoded demo sequence.

	previous_start = 0 # ms
	previous_end = 0 # ms

	for entry in DEMO:

		this_entry_ms, encoded_entry = encode_demo_entry(wad, entry)
		
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

wad = Wad(ENDIAN)
encoded = b''.join(encoded for encoded in get_demo_sequence(wad))
wad.add_bin(encoded, is_choreography=True)
wad.write('sota.wad')
print("wrote sota.wad")

