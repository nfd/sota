# Generate the `sotasequence.c` file used by the choreographer.
# pylint: disable=too-many-lines, bad-whitespace, global-statement, unused-argument
import struct
import urllib.request
import urllib.parse
import os
import gzip
import json
import math

from wad import Wad

MS_PER_ANIM_FRAME = 40

ENDIAN = '<'

FONTMAP = [
	0,   208, 48,  255, # a
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

EFFECT_NUM = {
		'nothing': 0,
		'spotlights': 1,
		'votevotevote': 2,
		'delayedblit': 3,
		'copperpastels': 4,
		'static': 5,
		'static2': 6,
}

# Bitplane size styles
BITPLANE_OFF = 0
BITPLANE_1X1 = 1
BITPLANE_2X1 = 2
BITPLANE_2X2 = 3

# The animation player can onion-skin frames.
SCENE_NORMAL     = 0
SCENE_ONION_SKIN = 0b01000000

DEMO = [
		# Intro: hand moving out: frame 621
		('scene', {'name': 'intro', 'planes': (BITPLANE_1X1, BITPLANE_1X1, BITPLANE_1X1, BITPLANE_1X1, BITPLANE_1X1)}),
		('clear', {'plane': 'all'}),
		('palette', {'values': (0xff110022, 0xffffffff, 0xff110022, 0xffffffff)}),
		('fadeto', {'ms': 2000, 'values': (0xff110022, 0xff221144, 0xff110022, 0xff110033)}),
		('split_anim', {'name': '010800', 'from': 0, 'to': 50}),
		#('pause', {'ms': 500}),
		('mod', {'type': 'start', 'name': 'data/stateldr.mod'}),
		# Girl with gun displayed inside hand
		('split_anim', {'name': '002c00', 'from': 0, 'to': 141, 'plane': 1, 'xor': 1}),
		('clear', {'plane': 1}),
		# Hand zooms out; dancing Bond girls. Frame 51 is skipped because it's weird (draws two hands almost over each other)
		# xor is manually disabled -- this is probably also encoded in draw cmds but whatever.
		('split_anim', {'name': '010800', 'from': 52, 'to': 145, 'xor': 0}),
		('split_anim', {'name': '010800', 'from': 146, 'to': 334, 'xor': 1}),
		('mod', {'type': 'stop'}),

		# frame 1649
		# Title: STATE OF THE ART, credits, dragon pic
		('scene', {'name': 'title', 'planes': (BITPLANE_1X1, BITPLANE_1X1, BITPLANE_1X1, BITPLANE_1X1, BITPLANE_1X1)}),
		('clear', {'plane': 'all'}),

		('sound', {'name': 'data/heartbeat.wav'}),
		('ilbm', {'name': 'data/state.iff', 'display': 'fullscreen'}),
		('pause', {'ms': 850}),
		('fadeto', {'ms': 200, 'values': (), 'wait': True}),

		# 1719
		('sound', {'name': 'data/heartbeat.wav'}),
		('ilbm', {'name': 'data/of.iff', 'display': 'fullscreen'}),
		('pause', {'ms': 850}),
		('fadeto', {'ms': 200, 'values': (), 'wait': True}),

		# 1787
		('sound', {'name': 'data/heartbeat.wav'}),
		('ilbm', {'name': 'data/the.iff', 'display': 'fullscreen'}),
		('pause', {'ms': 850}),
		('fadeto', {'ms': 200, 'values': (), 'wait': True}),

		# 1855
		('sound', {'name': 'data/heartbeat.wav'}),
		('ilbm', {'name': 'data/art.iff', 'display': 'fullscreen'}),
		('pause', {'ms': 850}),
		('fadeto', {'ms': 200, 'values': (), 'wait': True}),

		# 1909-ish
		('clear', {'plane': 'all'}),
		('ilbm', {'name': 'data/authors.iff', 'display': 'fullscreen', 'fadein_ms': 2000}),
		('pause', {'ms': 3000}),

		# 2161
		('fadeto', {'ms': 250, 'values': (0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff), 'wait': True}),
		('ilbm', {'name': 'data/dragon.iff', 'display': 'fullscreen', 'fadein_ms': 500}),
		('pause', {'ms': 4000}),

		# End of intro finally! fade 'er out.
		('fadeto', {'ms': 1000, 'values': (), 'wait': True}),

		# Morph to first dancer
		# ADF frame 02747
		# scene length 2747->3221 ~= 238 frames = ~=5.95 seconds
		# measured length (native, 25fps) = 436->670 ~= 235. -3
		('scene', {'name': 'dance-1', 'planes': (BITPLANE_1X1, BITPLANE_2X2, BITPLANE_OFF, BITPLANE_OFF, BITPLANE_OFF)}),
		('clear', {'plane': 'all'}),
		('starteffect', {'name': 'spotlights'}),
		('mod', {'type': 'start', 'name': 'data/condom corruption.mod'}),
		('palette', {'values': (0xff000000, 0xff000000, 0xff117700, 0xff000000, 0xff770077, 0xff000000, 0xff771111)}),
		('fadeto', {'ms': 1000, 'values': (0xff000000, 0xff000000, 0xff117700, 0xffdd0000, 0xff770077, 0xffdd7700, 0xff771111,
			0xffbbbb00)}),
		('split_anim', {'name': '067080', 'from': 0, 'to': 25}),
		('split_anim', {'name': '067230', 'from': 0, 'to': 195}),
		('fadeto', {'ms': 250, 'values': ()}),
		('split_anim', {'name': '067230', 'from': 196, 'to': 209}),
		('starteffect', {'name': 'nothing'}),

		# VOTE! VOTE! VOTE! - 3240 ms
		# ADF frame 03223, ends 3385 = 82 frames
		# measured length (native-timing) = 671->755 ~= 85. +0 (bang on!)
		('scene', {'name': 'votevotevote1', 'planes': (BITPLANE_1X1, BITPLANE_1X1, BITPLANE_1X1, BITPLANE_OFF, BITPLANE_OFF)}),
		('clear', {'plane': 'all'}),
		# black-on-white version
		('alternate_palette', {'idx': 0, 'values': (0xffeeffff, 0xff000000, 0xff779999, 0xffddeeee, 0xff114444, 0xff447777, 0xff003333, 0xffaaaadd)}),
		# white-on-black version
		('alternate_palette', {'idx': 1, 'values': (0xff000000, 0xffeeffff, 0xff336666, 0xff113333, 0xffaabbbb, 0xff778888, 0xffccdddd, 0xff224444)}),
		('loadfont', {'name': 'data/fontmap.iff', 'startchar': 'A', 'map' : FONTMAP}),
		('starteffect', {'name': 'votevotevote', 
			'text': ('VOTE[', 'VERY', 'LOTSA', 'NO', 'REAL', 'COOL', 'DULL', 'HIGH', 'FUNNY',
				'VOTE[', 'FUNK', 'KRAZY', 'NICE', 'RAPID', 'RAP', 'RIPPED', 'UGLY', 'CLASS',
				'VOTE[', 'CODE', 'MUSIC', 'GFX',)
				}),

		('pause', {'ms': 3240 + (MS_PER_ANIM_FRAME * 4), 'watchface-hint': 'keep'}),
		#('pause', {'ms': 324000, 'watchface-hint': 'keep'}),
		('starteffect', {'name': 'nothing'}),
		('clear', {'plane': 'all'}),

		# ADF frame 3387, ending 3943 ~= 279 frames (7 seconds)

		# Second set of dancers. Fast cuts (1 second per shot) of two separate dancers, 
		# one with orange-brown trails and one with white-blue trails.
		# Palette 0: Orange-brown dancer on white
		# measured time: 756->1035 ~= 279. +0
		('scene', {'name': 'dance-2', 'planes': (BITPLANE_1X1, BITPLANE_1X1, BITPLANE_1X1, BITPLANE_1X1, BITPLANE_1X1)}),
		('alternate_palette', {'idx': 0, 'values': (
			0xffffffff, 0xffcc7733, 0xffdd7733, 0xffaa3311, 0xffdd8844, 0xffaa3311, 0xffaa4422, 0xff661100,
			0xffee9944, 0xffaa3311, 0xffaa3311, 0xffaa3311, 0xffbb4422, 0xffaa3311, 0xff771100, 0xff440000,
			0xffffaa55, 0xffcc6633, 0xffaa3311, 0xff992211, 0xffaa3311, 0xffaa3311, 0xffaa3311, 0xff661100,
			0xffbb6622, 0xff881100, 0xffaa3311, 0xff550000, 0xff771100, 0xff550000, 0xff440000, 0xff330000)}),
		# Palette 1: Blue-white dancer on black.
		('alternate_palette', {'idx': 1, 'values': (
			0xff000000, 0xff3388cc, 0xff2288cc, 0xff55ccee, 0xff2277bb, 0xff55ccee, 0xff55bbdd, 0xff99eeff,
			0xff1166bb, 0xff55ccee, 0xff55ccee, 0xff55ccee, 0xff44bbdd, 0xff55ccee, 0xff88eeff, 0xffbbffff,
			0xff0055aa, 0xff3399cc, 0xff55ccee, 0xff66ddee, 0xff55ccee, 0xff55ccee, 0xff55ccee, 0xff99eeff,
			0xff44aadd, 0xff77eeff, 0xff55ccee, 0xffaaffff, 0xff88eeff, 0xffaaffff, 0xffbbffff, 0xffccffff)}),
		# Effect for this scene is a trail of onionskinned previous frames
		('starteffect', {'name': 'delayedblit'}),
		# 1 (3387)
		('use_alternate_palette', {'idx': 0}),
		('split_anim', {'name': '075c66', 'from': 0, 'to': 24}),
		('clear', {'plane': 'all'}),
		#2
		('use_alternate_palette', {'idx': 1}),
		('split_anim', {'name': '077016', 'from': 0, 'to': 21}),
		('clear', {'plane': 'all'}),
		#3
		('use_alternate_palette', {'idx': 0}),
		('split_anim', {'name': '075c66', 'from': 21, 'to': 42}),
		('clear', {'plane': 'all'}),
		#4 (3525)
		('scene_options', {'zoom': 2}),
		('use_alternate_palette', {'idx': 1}),
		('split_anim', {'name': '077016', 'from': 21, 'to': 42}),
		('clear', {'plane': 'all'}),
		#5 (up to frame 91 at this point)
		('scene_options', {'zoom': 1}),
		('use_alternate_palette', {'idx': 0}),
		('split_anim', {'name': '078104', 'from': 0, 'to': 21}),
		('clear', {'plane': 'all'}),
		#6
		('use_alternate_palette', {'idx': 1}),
		('split_anim', {'name': '077016', 'from': 0, 'to': 21}),
		('clear', {'plane': 'all'}),
		#7
		('use_alternate_palette', {'idx': 0}),
		('split_anim', {'name': '075c66', 'from': 0, 'to': 21}),
		('clear', {'plane': 'all'}),
		#8 (3701)
		('use_alternate_palette', {'idx': 1}),
		('split_anim', {'name': '07789e', 'from': 0, 'to': 21}),
		('clear', {'plane': 'all'}),
		#9 (at frame 179 at this point)
		('use_alternate_palette', {'idx': 0}),
		('split_anim', {'name': '077016', 'from': 16, 'to': 36}),
		('clear', {'plane': 'all'}),
		#10
		('use_alternate_palette', {'idx': 1}),
		('split_anim', {'name': '078104', 'from': 0, 'to': 21}),
		('clear', {'plane': 'all'}),
		#11
		('use_alternate_palette', {'idx': 0}),
		('split_anim', {'name': '077016', 'from': 0, 'to': 21}),
		('clear', {'plane': 'all'}),
		#12
		('use_alternate_palette', {'idx': 1}),
		('split_anim', {'name': '07789e', 'from': 10, 'to': 31}),
		('clear', {'plane': 'all'}),
		#13 (3921)
		('use_alternate_palette', {'idx': 0}),
		('split_anim', {'name': '077016', 'from': 18, 'to': 31}),
		('clear', {'plane': 'all'}),
		# total: 280 frames

		# Up to this point, we are bang on.

		# The 'crazy hips' dancer
		# frame 3945 to 4377 ~= 217 frames
		# measured 958->1169 ~= 212
		# -5

		('scene', {'name': 'dance-3', 'planes': (BITPLANE_1X1, BITPLANE_1X1, BITPLANE_1X1, BITPLANE_1X1, BITPLANE_1X1), }),
		('starteffect', {'name': 'nothing'}),
		('palette', {'values': (0,)}),
		('scene_options', {'anim_3_behind_plane_1': 1}),
		('clear', {'plane': 'all'}),
		('pause', {'ms': 5 * MS_PER_ANIM_FRAME}),
		('fadeto', {'ms': MS_PER_ANIM_FRAME * 20, 'values': (0xffffffff, 0xff000000, 0xff000000, 0xff000000, 0xffdd8844, 0xffaa3311, 0xffaa4422, 0xff661100, 0xffee9944, 0xffaa3311, 0xffaa3311, 0xffaa3311, 0xffbb4422, 0xffaa3311, 0xff771100, 0xff440000, 0xffffaa55, 0xffcc6633, 0xffaa3311, 0xff992211, 0xffaa3311, 0xffaa3311, 0xffaa3311, 0xff661100, 0xffbb5522, 0xff881100, 0xffaa3311, 0xff550000, 0xff771100, 0xff550000, 0xff440000, 0xff330000)}),
		# Initial fade-up doesn't include the copper effect.
		('split_anim', {'name': '08da00', 'from': 0, 'to': 19, 'plane': 0}),
		#('pause', {'ms': 30000}),
		('starteffect', {'name': 'copperpastels', 'palette_fade_ref': 16, 'values':
			["WWWW", "WWWY", "WWBW", "GWWW", "WYWW", "WWWB", "RYBW", "WWYW", "BWWW", ]}), 
		('split_anim', {'name': '08da00', 'from': 20, 'to': 190, 'plane': 0}),
		('fadeto', {'ms': MS_PER_ANIM_FRAME * 20, 'values': (0,)}),
		('split_anim', {'name': '08da00', 'from': 191, 'to': 210, 'plane': 0}),

		# Loading -- 4379 to 4729, 175 frames, 7 seconds
		# measured 1273->1447 ~= 174
		('scene', {'name': 'loading', 'planes':  (BITPLANE_1X1, BITPLANE_1X1, BITPLANE_1X1, BITPLANE_1X1, BITPLANE_1X1)}),
		('clear', {'plane': 'all'}),
		('palette', {'values': (0xffaaddbb, 0xff669977, 0xff77aa88, 0xff225544, 0xff77aa99, 0xff336655, 0xff336655, 0xff332222, 0xff88bb99, 0xff336655, 0xff336655, 0xff336655, 0xff447755, 0xff336655, 0xff333322, 0xff440000, 0xff99ccaa, 0xff558877, 0xff336655, 0xff225544, 0xff336655, 0xff336655, 0xff336655, 0xff442211, 0xff447766, 0xff334433, 0xff336655, 0xff441111, 0xff333322, 0xff441111, 0xff440000, 0xff440000)}),
		('starteffect', {'name': 'static'}),

		('split_anim', {'name': '09541c', 'from': 3, 'to': 22, 'plane': 1, }), # 20
		('split_anim', {'name': '09541c', 'from': 21, 'to': 4, 'plane': 1, }), # 18
		('split_anim', {'name': '09541c', 'from': 3, 'to': 22, 'plane': 1, }), # 20
		('split_anim', {'name': '09541c', 'from': 21, 'to': 4, 'plane': 1, }), # 18
		('split_anim', {'name': '09541c', 'from': 3, 'to': 22, 'plane': 1, }), # 20
		('split_anim', {'name': '09541c', 'from': 21, 'to': 4, 'plane': 1, }), # 18
		('split_anim', {'name': '09541c', 'from': 3, 'to': 22, 'plane': 1, }), # 20
		('split_anim', {'name': '09541c', 'from': 21, 'to': 4, 'plane': 1, }), # 18
		('split_anim', {'name': '09541c', 'from': 3, 'to': 22, 'plane': 1, }), # 20
		('split_anim', {'name': '09541c', 'from': 21, 'to': 19, 'plane': 1, }), # 3

		# 4731-4879 = 3 seconds of this (75 frames)
		# measured 1448->1525 ~= 77
		('scene', {'name': 'votevotevote2', 'planes': (BITPLANE_1X1, BITPLANE_1X1, BITPLANE_1X1, BITPLANE_OFF, BITPLANE_OFF)}),
		('clear', {'plane': 'all'}),
		# black-on-white version
		('alternate_palette', {'idx': 0, 'values': (0xffeeffff, 0xff000000, 0xff779999, 0xffddeeee, 0xff114444, 0xff447777, 0xff003333, 0xffaaaadd)}),
		# white-on-black version
		('alternate_palette', {'idx': 1, 'values': (0xff000000, 0xffeeffff, 0xff336666, 0xff113333, 0xffaabbbb, 0xff778888, 0xffccdddd, 0xff224444)}),
		('loadfont', {'name': 'data/fontmap.iff', 'startchar': 'A', 'map' : FONTMAP}),
		('starteffect', {'name': 'votevotevote', 
			'text': ('VOTE[', 'VERY', 'LOTSA', 'NO', 'REAL', 'COOL', 'DULL', 'HIGH', 'FUNNY',
				'VOTE[', 'FUNK', 'KRAZY', 'NICE', 'RAPID', 'RAP', 'RIPPED', 'UGLY', 'CLASS',
				'VOTE[', 'CODE', 'MUSIC', 'GFX',)}),

		('pause', {'ms': 3000, 'watchface-hint': 'keep'}),

		# Single-colour spots, alternating between blue dancer on yellow spots, and red dancer on blue spots.
		# Three separate animations are played: swing arms over head to crouch; wavey arms; shuffle on and off
		# 4881 - 5623 ~= 371 frames
		# measured at: 1526->1896 ~= 370
		('scene', {'name': 'dance-4', 'planes': (BITPLANE_1X1, BITPLANE_2X2, BITPLANE_OFF, BITPLANE_OFF, BITPLANE_OFF)}),
		('clear', {'plane': 'all'}),
		# the blue-on-yellow palette
		('alternate_palette', {'idx': 0, 'values':
			(0xff000000, 0xff0066aa, 0xffffff55, 0xff0077aa, 0xffffff55, 0xff0088aa, 0xffffff55, 0xff660000, 0xff88bb99, 0xff336655, 0xff336655, 0xff336655, 0xff447755, 0xff336655, 0xff333322, 0xff440000, 0xff99ccaa, 0xff558877, 0xff336655, 0xff225544, 0xff336655, 0xff336655, 0xff336655, 0xff442211, 0xff447766, 0xff334433, 0xff336655, 0xff441111, 0xff333322, 0xff441111, 0xff440000, 0xff440000)}),
		# the red-on-blue palette
		('alternate_palette', {'idx': 1, 'values':
			(0xff000000, 0xffff9966, 0xff0000bb, 0xffff8866, 0xff0000bb, 0xffff7766, 0xff0000bb, 0xffaa0000, 0xff88bb99, 0xff336655, 0xff336655, 0xff336655, 0xff447755, 0xff336655, 0xff333322, 0xff440000, 0xff99ccaa, 0xff558877, 0xff336655, 0xff225544, 0xff336655, 0xff336655, 0xff336655, 0xff442211, 0xff447766, 0xff334433, 0xff336655, 0xff441111, 0xff333322, 0xff441111, 0xff440000, 0xff440000)}),
		('use_alternate_palette', {'idx': 0}),
		('starteffect', {'name': 'spotlights'}),

		('split_anim', {'name': '098a00', 'from': 1, 'to': 9}),    # 9
		('use_alternate_palette', {'idx': 1}),
		('split_anim', {'name': '098a00', 'from': 11, 'to': 21}),  # 11
		('use_alternate_palette', {'idx': 0}),
		('split_anim', {'name': '098a00', 'from': 22, 'to': 32}),  # 11

		('use_alternate_palette', {'idx': 1}),
		('split_anim', {'name': '099546', 'from': 0, 'to': 10}),   # 11

		('use_alternate_palette', {'idx': 0}),
		('split_anim', {'name': '09ae1a', 'from': 0, 'to': 10}),   # 11

		('use_alternate_palette', {'idx': 1}),
		('split_anim', {'name': '099546', 'from': 11, 'to': 21}),  # 11

		('use_alternate_palette', {'idx': 0}),
		('split_anim', {'name': '09ae1a', 'from': 20, 'to': 30}),  # 11

		('use_alternate_palette', {'idx': 1}),
		('split_anim', {'name': '099546', 'from': 70, 'to': 80}),  # 11
		('use_alternate_palette', {'idx': 0}),
		('split_anim', {'name': '099546', 'from': 81, 'to': 91}),  # 11

		('use_alternate_palette', {'idx': 1}),
		('split_anim', {'name': '09ae1a', 'from': 40, 'to': 50}),  # 11

		('use_alternate_palette', {'idx': 0}),
		('split_anim', {'name': '099546', 'from': 58, 'to': 68}),  # 11

		('use_alternate_palette', {'idx': 1}),
		('split_anim', {'name': '098a00', 'from': 14, 'to': 24}),  # 11
		('use_alternate_palette', {'idx': 0}),
		('split_anim', {'name': '098a00', 'from': 25, 'to': 35}),  # 11

		('use_alternate_palette', {'idx': 1}),
		('split_anim', {'name': '099546', 'from': 76, 'to': 86}),  # 11

		('use_alternate_palette', {'idx': 0}),
		('split_anim', {'name': '09ae1a', 'from': 11, 'to': 21}),  # 11

		('use_alternate_palette', {'idx': 1}),
		('split_anim', {'name': '099546', 'from': 5, 'to': 15}),   # 11

		('use_alternate_palette', {'idx': 0}),
		('split_anim', {'name': '09ae1a', 'from': 5, 'to': 15}),   # 11 (185 frames so far)

		('use_alternate_palette', {'idx': 1}),
		('split_anim', {'name': '099546', 'from': 70, 'to': 79}),  # 10
		('use_alternate_palette', {'idx': 0}),
		('split_anim', {'name': '099546', 'from': 80, 'to': 90}),  # 11

		('use_alternate_palette', {'idx': 1}),
		('split_anim', {'name': '09ae1a', 'from': 45, 'to': 55}),  # 11

		('use_alternate_palette', {'idx': 0}),
		('split_anim', {'name': '099546', 'from': 63, 'to': 74}),  # 12

		('use_alternate_palette', {'idx': 1}),
		('split_anim', {'name': '09ae1a', 'from': 0, 'to': 10}),   # 11

		('use_alternate_palette', {'idx': 0}),
		('split_anim', {'name': '099546', 'from': 39, 'to': 49}),  # 11

		('use_alternate_palette', {'idx': 1}),
		('split_anim', {'name': '09ae1a', 'from': 11, 'to': 21}),  # 11

		('use_alternate_palette', {'idx': 0}),
		('split_anim', {'name': '099546', 'from': 50, 'to': 60}),  # 11

		('use_alternate_palette', {'idx': 1}),
		('split_anim', {'name': '09ae1a', 'from': 22, 'to': 32}),  # 11

		('use_alternate_palette', {'idx': 0}),
		('split_anim', {'name': '099546', 'from': 61, 'to': 71}),  # 11

		('use_alternate_palette', {'idx': 1}),
		('split_anim', {'name': '09ae1a', 'from': 33, 'to': 43}),  # 11

		('use_alternate_palette', {'idx': 0}),
		('split_anim', {'name': '099546', 'from': 72, 'to': 82}),  # 11

		('use_alternate_palette', {'idx': 1}),
		('split_anim', {'name': '09ae1a', 'from': 16, 'to': 25}),  # 10

		('use_alternate_palette', {'idx': 0}),
		('split_anim', {'name': '099546', 'from': 27, 'to': 37}),  # 11

		('use_alternate_palette', {'idx': 1}),
		('split_anim', {'name': '09ae1a', 'from': 9, 'to': 19}),  # 11

		('use_alternate_palette', {'idx': 0}),
		('split_anim', {'name': '099546', 'from': 47, 'to': 57}),  # 11
		('use_alternate_palette', {'idx': 1}),
		('split_anim', {'name': '099546', 'from': 58, 'to': 68}),  # 11 -- 371 

		# The jumpers:  5647 - 6347 ~= 350 frames
		# measured at: 1897->2242 ~= 345
		('scene', {'name': 'jump-1', 'planes': (BITPLANE_1X1, BITPLANE_1X1, BITPLANE_1X1, BITPLANE_1X1, BITPLANE_1X1,
			BITPLANE_2X2), 'post_anim_clear_plane_mask': 0b11111, 'first_anim_frame_plane_copy_mask': 0b11110,
			'onion_skin': (0, 4)}),
		('clear', {'plane': 'all'}),

		('ilbm', {'name': 'data/static1.iff', 'x': 0, 'y': 0, 'w': 0.5, 'h': 0.5, 'plane': 5}),
		('ilbm', {'name': 'data/static2.iff', 'x': 0.5, 'y': 0, 'w': 0.5, 'h': 0.5, 'plane': 5}),
		('ilbm', {'name': 'data/static3.iff', 'x': 0, 'y': 0.5, 'w': 0.5, 'h': 0.5, 'plane': 5}),
		('ilbm', {'name': 'data/static4.iff', 'x': 0.5, 'y': 0.5, 'w': 0.5, 'h': 0.5, 'plane': 5}),
		('palette', {'values':
			(0xff000066, 0xff999999, 0xff9999aa, 0xff995522, 0xff99aacc, 0xff995522, 0xff996644, 0xff552200,
				0xff99bbdd, 0xff995522 , 0xff995522 , 0xff995522 , 0xff996655 , 0xff995522 , 0xff662200 , 0xff110000 ,
				0xffaaccff , 0xff998888 , 0xff995522 , 0xff994411 , 0xff995522 , 0xff995522 , 0xff995522 , 0xff442200 ,
				0xff997766 , 0xff883300 , 0xff995522 , 0xff441100 , 0xff773300 , 0xff331100 , 0xff221100)
		}),

		('starteffect', {'name': 'static2'}), # Static squares and circles

		('pause', {'ms': 5 * MS_PER_ANIM_FRAME}), 

		('split_anim', {'name': '09e1b8', 'from': 0, 'to': 40}), # somersault, almost exit
		('split_anim', {'name': '09f31a', 'from': 0, 'to': 50}), # upside down flip
		('split_anim', {'name': '0a0552', 'from': 0, 'to': 32}), # upside down flip
		('split_anim', {'name': '0a12f2', 'from': 0, 'to': 10}), 
		('split_anim', {'name': '09e1b8', 'from': 9, 'to': 51}), # somersault, almost exit
		('split_anim', {'name': '09d9c4', 'from': 0, 'to': 30}), 
		('split_anim', {'name': '0a12f2', 'from': 8, 'to': 17}),
		('split_anim', {'name': '0a0552', 'from': 0, 'to': 31}),
		('split_anim', {'name': '0a12f2', 'from': 0, 'to': 9}),
		('split_anim', {'name': '09f31a', 'from': 6, 'to': 48}),
		('split_anim', {'name': '09e1b8', 'from': 0, 'to': 40}),

		# The iris opening with shape morphs and dancers.
		# 6357->~7967 ~= 805 frames
		# measured at: 2243->3055 ~= 812
		('scene', {'name': 'iris-vs-glitchy', 'planes': (BITPLANE_1X1, BITPLANE_1X1, BITPLANE_1X1, BITPLANE_1X1, BITPLANE_1X1,
			BITPLANE_2X2),'post_anim_clear_plane_mask': 0b11111, 'first_anim_frame_plane_copy_mask': 0b11110,
			'onion_skin': (0, 4)}),

		# This is kind of silly, since 99.9% of the time we will just continue
		# on from the previous scene in which all of this stuff is already
		# loaded.
		('clear', {'plane': 'all'}),
		('ilbm', {'name': 'data/static1.iff', 'x': 0, 'y': 0, 'w': 0.5, 'h': 0.5, 'plane': 5}),
		('ilbm', {'name': 'data/static2.iff', 'x': 0.5, 'y': 0, 'w': 0.5, 'h': 0.5, 'plane': 5}),
		('ilbm', {'name': 'data/static3.iff', 'x': 0, 'y': 0.5, 'w': 0.5, 'h': 0.5, 'plane': 5}),
		('ilbm', {'name': 'data/static4.iff', 'x': 0.5, 'y': 0.5, 'w': 0.5, 'h': 0.5, 'plane': 5}),

		('alternate_palette', {'idx': 0, 'values':
			(0xff110044, 0xff1155cc, 0xff2255cc, 0xff0099ff, 0xff2244bb, 0xff0099ff, 0xff0088ee, 0xff66ccff,
			0xff2233bb, 0xff0099ff, 0xff0099ff, 0xff0099ff, 0xff1188ee, 0xff0099ff, 0xff55ccff, 0xffddffff,
			0xff3322aa, 0xff1166dd, 0xff0099ff, 0xff00aaff, 0xff0099ff, 0xff0099ff, 0xff0099ff, 0xff77ddff,
			0xff1177dd, 0xff22bbff, 0xff0099ff, 0xff99ddff, 0xff33bbff, 0xffaaeeff, 0xffbbeeff, 0xffeeffff)
		}),

		('alternate_palette', {'idx': 1, 'values':
			(0xff550000, 0xffeeaa33, 0xffddaa33, 0xffff6600, 0xffddbb44, 0xffff6600, 0xffff7711, 0xff993300, 
			0xffddcc44, 0xffff6600, 0xffff6600, 0xffff6600, 0xffee7711, 0xffff6600, 0xffaa3300, 0xff220000, 
			0xffccdd55, 0xffee9922, 0xffff6600, 0xffff5500, 0xffff6600, 0xffff6600, 0xffff6600, 0xff882200, 
			0xffee8822, 0xffdd4400, 0xffff6600, 0xff662200, 0xffcc4400, 0xff551100, 0xff441100, 0xff110000)
		}),

		('use_alternate_palette', {'idx': 0}),
		('starteffect', {'name': 'static2'}), # Static squares and circles
		('split_anim', {'name': '0aface', 'from': 0, 'to': 20}), # max 231

		('use_alternate_palette', {'idx': 1}),
		('scene_options', {'zoom': 2, 'distort': True}),
		('split_anim', {'name': '0b3274', 'from': 0, 'to': 21}),

		('use_alternate_palette', {'idx': 0}),
		('scene_options', {'zoom': 1, 'distort': False}),
		('split_anim', {'name': '0aface', 'from': 32, 'to': 54}), 

		('use_alternate_palette', {'idx': 1}),
		('scene_options', {'zoom': 2, 'distort': True}),
		('split_anim', {'name': '0b3274', 'from': 21, 'to': 43}),

		('use_alternate_palette', {'idx': 0}),
		('scene_options', {'zoom': 1, 'distort': False}),
		('split_anim', {'name': '0aface', 'from': 65, 'to': 87}), 

		# zoom out from very close -- NB we flip animations here! (well, retain the same one but flip colours)
		('use_alternate_palette', {'idx': 1}),
		('scene_options', {'zoom': 2, 'distort': False}),
		('split_anim', {'name': '0aface', 'from': 99, 'to': 121}),

		# Distorted dancer, white on blue, zoomed out.
		('use_alternate_palette', {'idx': 0}),
		('scene_options', {'zoom': 1, 'distort': True}),
		('split_anim', {'name': '0b3274', 'from': 43, 'to': 65}), 

		# Distorted dancer, black on red, zoomed in (frame06669.png)
		('use_alternate_palette', {'idx': 1}),
		('scene_options', {'zoom': 2, 'distort': True}),
		('split_anim', {'name': '0b3274', 'from': 65, 'to': 87}),

		# Distorted dancer, white on blue, zoomed out (frame06713.png)
		('use_alternate_palette', {'idx': 0}),
		('scene_options', {'zoom': 1, 'distort': True}),
		('split_anim', {'name': '0b3274', 'from': 65, 'to': 90}), 

		# Distorted dancer, black on red, zoomed in (frame06763.png)
		('use_alternate_palette', {'idx': 1}),
		('scene_options', {'zoom': 2, 'distort': True}),
		('split_anim', {'name': '0b3274', 'from': 90, 'to': 112}),

		# Distorted dancer, white on blue, zoomed out (frame06807.png)
		# This section is pretty lazy! Even the animation is re-used (it's the
		# first one)
		('use_alternate_palette', {'idx': 0}),
		('scene_options', {'zoom': 1, 'distort': True}),
		('split_anim', {'name': '0b3274', 'from': 112, 'to': 134}), 

		# morphing dancer, black on red, nondistorted, zoomed (frame06851.png)
		('use_alternate_palette', {'idx': 1}),
		('scene_options', {'zoom': 2, 'distort': False}),
		('split_anim', {'name': '0aface', 'from': 134, 'to': 156}),

		# Distorted dancer, white on blue, unzoomed (frame06895.png)
		('use_alternate_palette', {'idx': 0}),
		('scene_options', {'zoom': 1, 'distort': True}),
		('split_anim', {'name': '0b3274', 'from': 134, 'to': 156}), 

		# Distorted dancer, black on red, zoomed (frame06939.png)
		('use_alternate_palette', {'idx': 1}),
		('scene_options', {'zoom': 2, 'distort': True}),
		('split_anim', {'name': '0b3274', 'from': 156, 'to': 178}),

		# Distorted dancer, white on blue, unzoomed (frame06983.png)
		# checked! Above frames are not.
		('use_alternate_palette', {'idx': 0}),
		('scene_options', {'zoom': 1, 'distort': True}),
		('split_anim', {'name': '0b3274', 'from': 165, 'to': 187}),

		# morphing woman, black on red, zoomed? (frame07027)
		('use_alternate_palette', {'idx': 1}),
		('scene_options', {'zoom': 2, 'distort': False}),
		('split_anim', {'name': '0aface', 'from': 78, 'count': 22}),

		# morphing woman, white on blue, unzoomed (frame07071)
		('use_alternate_palette', {'idx': 0}),
		('scene_options', {'zoom': 1, 'distort': False}),
		('split_anim', {'name': '0aface', 'from': 110, 'count': 25}),

		# morphing woman, black on red, zoomed (frame07121) -- circle to shimmy
		('use_alternate_palette', {'idx': 1}),
		('scene_options', {'zoom': 2, 'distort': False}),
		('split_anim', {'name': '0aface', 'from': 143, 'count': 23}),

		# distorted dancer, white on blue, unzoomed (frame07165)
		('use_alternate_palette', {'idx': 0}),
		('scene_options', {'zoom': 1, 'distort': True}),
		('split_anim', {'name': '0b3274', 'from': 40, 'count': 22}),
		
		# morphing woman, black on red, zoomed, becoming mountains (frame07290)
		('use_alternate_palette', {'idx': 1}),
		('scene_options', {'zoom': 2, 'distort': False}),
		('split_anim', {'name': '0aface', 'from': 163, 'count': 22}),

		# distorted dancer, white on blue, unzoomed (frame07253)
		('use_alternate_palette', {'idx': 0}),
		('scene_options', {'zoom': 1, 'distort': True}),
		('split_anim', {'name': '0b3274', 'from': 66, 'count': 22}),

		# distorted dancer, black on red, zoomed (frame07297)
		('use_alternate_palette', {'idx': 1}),
		('scene_options', {'zoom': 2, 'distort': True}),
		('split_anim', {'name': '0b3274', 'from': 100, 'count': 24}),

		# morphing woman, white on blue, unzoomed (frame07345)
		('use_alternate_palette', {'idx': 0}),
		('scene_options', {'zoom': 1, 'distort': False}),
		('split_anim', {'name': '0aface', 'from': 132, 'count': 23}),

		# distorted dancer, black on red, zoomed (frame07389)
		('use_alternate_palette', {'idx': 1}),
		('scene_options', {'zoom': 2, 'distort': True}),
		('split_anim', {'name': '0b3274', 'from': 97, 'count': 22}),

		# distorted dancer, white on blue, unzoomed (frame07433)
		('use_alternate_palette', {'idx': 0}),
		('scene_options', {'zoom': 1, 'distort': True}),
		('split_anim', {'name': '0b3274', 'from': 33, 'count': 22}),

		# morphing woman, black on red, zoomed (frame07477)
		('use_alternate_palette', {'idx': 1}),
		('scene_options', {'zoom': 2, 'distort': False}),
		('split_anim', {'name': '0aface', 'from': 167, 'count': 22}),

		# distorted dancer, white on blue, unzoomed (frame07521)
		('use_alternate_palette', {'idx': 0}),
		('scene_options', {'zoom': 1, 'distort': True}),
		('split_anim', {'name': '0b3274', 'from': 33, 'count': 22}),

		# morphing woman, black on red, unzoomed (frame07565)
		('use_alternate_palette', {'idx': 1}),
		('scene_options', {'zoom': 1, 'distort': False}),
		('split_anim', {'name': '0aface', 'from': 68, 'count': 24}),

		# morphing woman, white on blue, unzoomed, emerging from mountains (frame07615)
		('use_alternate_palette', {'idx': 0}),
		('scene_options', {'zoom': 1, 'distort': False}),
		('split_anim', {'name': '0aface', 'from': 99, 'count': 22}),

		# morphing woman, black on red, zoomed, becoming a circle (frame07659)
		('use_alternate_palette', {'idx': 1}),
		('scene_options', {'zoom': 2, 'distort': False}),
		('split_anim', {'name': '0aface', 'from': 122, 'count': 22}),

		# distorted dancer, white on blue, unzoomed (frame07703)
		('use_alternate_palette', {'idx': 0}),
		('scene_options', {'zoom': 1, 'distort': True}),
		('split_anim', {'name': '0b3274', 'from': 113, 'count': 22}),

		# morphing woman, black on red, zoomed (frame07751)
		('use_alternate_palette', {'idx': 1}),
		('scene_options', {'zoom': 2, 'distort': False}),
		('split_anim', {'name': '0aface', 'from': 70, 'count': 21}),

		# morphing woman, white on blue, unzoomed, emerging from mountains again (frame07791)
		('use_alternate_palette', {'idx': 0}),
		('scene_options', {'zoom': 1, 'distort': False}),
		('split_anim', {'name': '0aface', 'from': 99, 'count': 22}),

		# distorted dancer, black on red, zoomed (frame07835)
		('use_alternate_palette', {'idx': 1}),
		('scene_options', {'zoom': 2, 'distort': True}),
		('split_anim', {'name': '0b3274', 'from': 21, 'count': 22}),

		# morphing woman, white on blue, unzoomed (frame07879)
		('use_alternate_palette', {'idx': 0}),
		('scene_options', {'zoom': 1, 'distort': False}),
		('split_anim', {'name': '0aface', 'from': 88, 'count': 22}),

		# morphing woman, black on red, iris opening (frame07925)
		('use_alternate_palette', {'idx': 1}),
		('scene_options', {'zoom': 1, 'distort': False}),
		('split_anim', {'name': '0aface', 'from': 5, 'count': 20}),

		# The nightmare is over

		# The fake 3D scene (frame07989)
		# from 7979 to 8793 ~= 407 frames (including fade-ins)
		# measured at: 3056->3462 ~= 406
		('scene', {'name': 'omg3d', 'planes': (BITPLANE_1X1, BITPLANE_1X1, BITPLANE_1X1, BITPLANE_1X1, BITPLANE_1X1),
			'onion_skin': (2, 3)
					  }),
		('starteffect', {'name': 'nothing'}),
		('clear', {'plane': 'all'}),
		#('palette', {'values': (0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff)}),
		#('fadeto', {'ms': 500, 'values': (0xff000000, 0xff000044, 0xff000066, 0xff000088, 0xff004400, 0xff004444, 0xff004466, 0xff004488, 0xff006600, 0xff006644, 0xff006666, 0xff006688, 0xff008800, 0xff008844, 0xff008866, 0xff008888, 0xffccdd55, 0xffee9922, 0xffff6600, 0xffff5500, 0xffff6600, 0xffff6600, 0xffff6600, 0xff882200, 0xffee8822, 0xffdd4400, 0xffff6600, 0xff662200, 0xffcc4400, 0xff551100, 0xff441100, 0xff110000)}),
		('palette', {'values': (0xff000000, 0xff000044, 0xff000066, 0xff000088, 0xff004400, 0xff004444, 0xff004466, 0xff004488, 0xff006600, 0xff006644, 0xff006666, 0xff006688, 0xff008800, 0xff008844, 0xff008866, 0xff008888, 0xffccdd55, 0xffee9922, 0xffff6600, 0xffff5500, 0xffff6600, 0xffff6600, 0xffff6600, 0xff882200, 0xffee8822, 0xffdd4400, 0xffff6600, 0xff662200, 0xffcc4400, 0xff551100, 0xff441100, 0xff110000)}),

		# couple of frames for fun
		('pause', {'ms': 5 * MS_PER_ANIM_FRAME}),

		('scene_options', {'flip_vertical': True, 'multidraw_3d': True}),
		('split_anim', {'name': '0b989c', 'from': 0, 'to': 135}),
		# Fade to white 
		('fadeto', {'ms': MS_PER_ANIM_FRAME * 9, 'values': (0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff)}),
		('split_anim', {'name': '0b989c', 'from': 136, 'to': 144}), 
		# Fade to red palette and flip horizontally.
		('scene_options', {'flip_vertical': True, 'flip_horizontal': True, 'multidraw_3d': True}),
		# Go backwards in red.
		('fadeto', {'ms': MS_PER_ANIM_FRAME * 10, 'values': (0xff000000, 0xff000022, 0xff000044, 0xff000066, 0xff440000, 0xff440022, 0xff440044, 0xff440066, 0xff660000, 0xff660022, 0xff660044, 0xff660066, 0xff880000, 0xff880022, 0xff880044, 0xff880066, 0xffccdd55, 0xffee9922, 0xffff6600, 0xffff5500, 0xffff6600, 0xffff6600, 0xffff6600, 0xff882200, 0xffee8822, 0xffdd4400, 0xffff6600, 0xff662200, 0xffcc4400, 0xff551100, 0xff441100, 0xff110000)}),
		('split_anim', {'name': '0b989c', 'from': 139, 'to': 10}),

		# Fade to white over 10 frames
		('fadeto', {'ms': MS_PER_ANIM_FRAME * 10, 'values': (0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff)}),
		('split_anim', {'name': '0b989c', 'from': 10, 'to': 0}),

		# fade to orange palette and flip vertical but not horizontal
		('fadeto', {'ms': MS_PER_ANIM_FRAME * 10, 'values': (0xff000000, 0xff002200, 0xff004400, 0xff006600, 0xff440000, 0xff442200, 0xff444400, 0xff446600, 0xff660000, 0xff662200, 0xff664400, 0xff666600, 0xff880000, 0xff882200, 0xff884400, 0xff886600, 0xffccdd55, 0xffee9922, 0xffff6600, 0xffff5500, 0xffff6600, 0xffff6600, 0xffff6600, 0xff882200, 0xffee8822, 0xffdd4400, 0xffff6600, 0xff662200, 0xffcc4400, 0xff551100, 0xff441100, 0xff110000)}),
		('split_anim', {'name': '0b989c', 'from': 0, 'to': 120}),

		# couple of frames for fun
		('pause', {'ms': 5 * MS_PER_ANIM_FRAME}),

		# static and dancers (straight cut)
		# from 8795 to 9833 = 519 frames
		# measured at: 3464->3963 ~= 499
		('scene', {'name': 'static-dancers', 'planes': (BITPLANE_1X1, BITPLANE_1X1, BITPLANE_1X1, BITPLANE_1X1, BITPLANE_1X1, BITPLANE_2X2), 'post_anim_clear_plane_mask': 0b11111, 'first_anim_frame_plane_copy_mask': 0b11110, 'onion_skin': (0, 4)}),
		('clear', {'plane': 'all'}),

		('ilbm', {'name': 'data/static1.iff', 'x': 0, 'y': 0, 'w': 0.5, 'h': 0.5, 'plane': 5}),
		('ilbm', {'name': 'data/static2.iff', 'x': 0.5, 'y': 0, 'w': 0.5, 'h': 0.5, 'plane': 5}),
		('ilbm', {'name': 'data/static3.iff', 'x': 0, 'y': 0.5, 'w': 0.5, 'h': 0.5, 'plane': 5}),
		('ilbm', {'name': 'data/static4.iff', 'x': 0.5, 'y': 0.5, 'w': 0.5, 'h': 0.5, 'plane': 5}),
		('alternate_palette', {'idx': 0, 'values': (0xff550022, 0xff885522, 0xff774422, 0xffddaa44, 0xff663322, 0xffcc9944, 0xffcc9944, 0xff886633, 0xff552211, 0xffcc9944, 0xffcc9944, 0xffcc9944, 0xffbb8833, 0xffcc9944, 0xff997733, 0xff441122, 0xff441111, 0xff996633, 0xffcc9944, 0xffddbb44, 0xffcc9944, 0xffcc9944, 0xffcc9944, 0xff885522, 0xffaa7733, 0xffbb9933, 0xffcc9944, 0xff774422, 0xffaa8833, 0xff663322, 0xff552222, 0xff440022) }),
		('use_alternate_palette', {'idx': 0}),
		('starteffect', {'name': 'static2'}), # Static squares and circles
		('split_anim', {'name': '0c0374', 'from': 0, 'to': 255}),

		# Same scene, but with annoying flashing effects. :)
		('alternate_palette', {'idx': 1, 'values': (0xffffffff, 0xff888888, 0xff999999, 0xff111111, 0xffaaaaaa, 0xff222222, 0xff222222, 0xff777777, 0xffcccccc, 0xff222222, 0xff222222, 0xff222222, 0xff444444, 0xff222222, 0xff666666, 0xffeeeeee, 0xffdddddd, 0xff666666, 0xff222222, 0xff000000, 0xff222222, 0xff222222, 0xff222222, 0xff999999, 0xff555555, 0xff333333, 0xff222222, 0xffaaaaaa, 0xff555555, 0xffbbbbbb, 0xffdddddd, 0xffffffff)}),

		('alternate_animated', {'name': '0c472a', 'from': 0, 'start_palette': 0, 'mode': 'linear', 'frames_per_flash': 11, 'repeat': 10}),

		('use_alternate_palette', {'idx': 0}),
		('split_anim', {'name': '0c472a', 'from': 101, 'to': 245}),

		# 10-frame fade to white. TODO use frame counts for fades rather than ms?
		('fadeto', {'ms': MS_PER_ANIM_FRAME * 8, 'values': (0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff)}),
		('split_anim', {'name': '0c472a', 'from': 246, 'to': 255}),

		# Hat-outline dancing scene
		# from 9835 to 11219 = 692 frames
		# measured at 3964->4656 ~= 692
		('scene', {'name': 'hat-outline-dancers', 'planes': (BITPLANE_1X1, BITPLANE_1X1, BITPLANE_1X1, BITPLANE_1X1, BITPLANE_1X1)}),
		('clear', {'plane': 'all'}),
		('scene_options', {'anim_outline': True}),
		('palette', {'values': (0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff552211, 0xffcc9944, 0xffcc9944, 0xffcc9944, 0xffbb8833, 0xffcc9944, 0xff997733, 0xff441122, 0xff441111, 0xff996633, 0xffcc9944, 0xffddbb44, 0xffcc9944, 0xffcc9944, 0xffcc9944, 0xff885522, 0xffaa7733, 0xffbb9933, 0xffcc9944, 0xff774422, 0xffaa8833, 0xff663322, 0xff552222, 0xff440022)}),
		('starteffect', {'name': 'copperpastels', 'values':
			["WWWW", "WWWY", "WWBW", "GWWW", "WYWW", "WWWB", "RYBW", "WWYW", "BWWW", ]}), 

		# Man, hat, intro
		('split_anim', {'name': '0cbd82', 'from': 0, 'count': 10}),

		# Woman, dancing, intro
		('split_anim', {'name': '0d0b52', 'from': 0, 'count': 32}),

		# Man, hat, continuing
		('split_anim', {'name': '0cbd82', 'from': 11, 'to': 32}),

		# Woman, continuing (second reel)
		('split_anim', {'name': '0d1872', 'from': 9, 'to': 32}),

		# nananananananananananananananana HAT MAN
		('split_anim', {'name': '0cbd82', 'from': 44, 'to': 65}),

		# woman, continuing (frame10055)
		('split_anim', {'name': '0d0b52', 'from': 11, 'to': 32}),

		# hat dude is back (second reel)
		('split_anim', {'name': '0cd850', 'from': 0, 'to': 21}),

		# woman reel 2
		('split_anim', {'name': '0d1872', 'from': 0, 'to': 21}),

		# Mr. Hat
		('split_anim', {'name': '0cd850', 'from': 22, 'to': 43}),

		# Woman reel 2 (frame10231)
		('split_anim', {'name': '0d1872', 'from': 11, 'to': 32}),

		('split_anim', {'name': '0cd850', 'from': 44, 'to': 65}),

		# frame10319
		('split_anim', {'name': '0d0b52', 'from': 0, 'to': 22}),

		# frame10363
		#('scene_options', {'anim_outline': True, 'flip_horizontal': True}),
		('split_anim', {'name': '0cf46e', 'from': 0, 'to': 21}),

		# frame 10407
		#('scene_options', {'anim_outline': True, 'flip_horizontal': False}),
		('split_anim', {'name': '0d0b52', 'from': 22, 'to': 43}),

		# frame 10451
		('split_anim', {'name': '0cd850', 'from': 33, 'to': 54}),
		
		# frame 10495
		#('scene_options', {'anim_outline': True, 'flip_horizontal': True}),
		('split_anim', {'name': '0cf46e', 'from': 22, 'to': 43}),

		#frame 10539
		#('scene_options', {'anim_outline': True, 'flip_horizontal': False}),
		('split_anim', {'name': '0d1872', 'from': 0, 'to': 32}),

		#frame 10605
		#('scene_options', {'anim_outline': True, 'flip_horizontal': True}),
		('split_anim', {'name': '0d2700', 'from': 0, 'to': 22}),

		#frame 10649
		('split_anim', {'name': '0d1872', 'from': 23, 'to': 33}),

		# Original does a horizontal flip here right in the middle of the animation. It looks terrible so assuming it's a bug
		('split_anim', {'name': '0d1872', 'from': 34, 'to': 43}),

		# frame10693
		('scene_options', {'anim_outline': True, 'flip_horizontal': True}),
		('split_anim', {'name': '0cbd82', 'from': 0, 'to': 21}),

		# frame10737
		('split_anim', {'name': '0d0b52', 'from': 0, 'to': 21}),

		# frame10781
		('split_anim', {'name': '0cbd82', 'from': 22, 'to': 43}),

		# frame10825
		('split_anim', {'name': '0d0b52', 'from': 22, 'to': 43}),

		# frame10869
		('split_anim', {'name': '0cd850', 'from': 0, 'to': 21}),

		# frame10913
		('split_anim', {'name': '0d1872', 'from': 0, 'to': 21}),

		# frame10957
		('split_anim', {'name': '0cd850', 'from': 22, 'to': 43}),

		# frame11001
		('split_anim', {'name': '0d1872', 'from': 23, 'to': 44}),

		# frame11045
		('split_anim', {'name': '0cbd82', 'from': 22, 'to': 43}),

		# frame11089
		('split_anim', {'name': '0d2700', 'from': 11, 'to': 32}),

		# frame11133
		('split_anim', {'name': '0d0b52', 'from': 0, 'to': 21}),

		# frame11177
		('split_anim', {'name': '0cbd82', 'from': 44, 'to': 64}),

		# New scene! Spotlights are back.
		# frames: 11221 to 11937 = 358 frames
		# measured: 4657->5014 ~= 357

		('scene', {'name': 'dance-5', 'planes': (BITPLANE_1X1, BITPLANE_2X2, BITPLANE_OFF, BITPLANE_OFF, BITPLANE_OFF)}),
		('clear', {'plane': 'all'}),
		# the white-on-orange palette
		('alternate_palette', {'idx': 0, 'values':
			(0xff000000, 0xff000000, 0xffaa9900, 0xffddeeff, 0xff770000, 0xffddeeff, 0xff550055, 0xff332200, 0xff552211, 0xffcc9944, 0xffcc9944, 0xffcc9944, 0xffbb8833, 0xffcc9944, 0xff997733, 0xff441122, 0xff441111, 0xff996633, 0xffcc9944, 0xffddbb44, 0xffcc9944, 0xffcc9944, 0xffcc9944, 0xff885522, 0xffaa7733, 0xffbb9933, 0xffcc9944, 0xff774422, 0xffaa8833, 0xff663322, 0xff552222, 0xff440022)}),

		# the orange-on-blue/white palette
		('alternate_palette', {'idx': 1, 'values':
			(0xff000000, 0xff000000, 0xff332200, 0xff550055, 0xffddeeff, 0xff770000, 0xff2255aa, 0xffaa9900, 0xff552211, 0xffcc9944, 0xffcc9944, 0xffcc9944, 0xffbb8833, 0xffcc9944, 0xff997733, 0xff441122, 0xff441111, 0xff996633, 0xffcc9944, 0xffddbb44, 0xffcc9944, 0xffcc9944, 0xffcc9944, 0xff885522, 0xffaa7733, 0xffbb9933, 0xffcc9944, 0xff774422, 0xffaa8833, 0xff663322, 0xff552222, 0xff440022)}),
		('starteffect', {'name': 'spotlights'}),

		# TODO frames_per_dancer -- we want to retain the dancer over two flashes.
		('alternate_animated', {'names': ('0d4488', '0d82fa', '0d3c00'), 'from': 0, 'counts': (170, 130, 50), 'start_palette': 0, 'mode': 'linear', 'frames_per_flash': 11, 'frames_per_dancer': 22, 'frames': 345}),

		# Finish the sequence.
		('split_anim', {'name': '0d3c00', 'from': 0, 'to': 12}),

		# The 'epilepsy' dancing: 11939-12917(ish) ~= 489 frames
		# TODO epilepsy effect is much worse in mine than in SOTA -- can't see inner colours.
		# measured: 5015->5503 ~= 488

		('scene', {'name': 'dance-final', 'planes': (BITPLANE_1X1, BITPLANE_2X2, BITPLANE_OFF, BITPLANE_OFF, BITPLANE_OFF)}),
		('clear', {'plane': 'all'}),
		('alternate_palette', {'idx': 1, 'values':
			(0xff000000, 0xff000000, 0xffffffff, 0xffddeeff, 0xffffffff, 0xffddeeff, 0xffffffff, 0xff332200, 0xff552211, 0xffcc9944, 0xffcc9944, 0xffcc9944, 0xffbb8833, 0xffcc9944, 0xff997733, 0xff441122, 0xff441111, 0xff996633, 0xffcc9944, 0xffddbb44, 0xffcc9944, 0xffcc9944, 0xffcc9944, 0xff885522, 0xffaa7733, 0xffbb9933, 0xffcc9944, 0xff774422, 0xffaa8833, 0xff663322, 0xff552222, 0xff440022)}),
		('use_alternate_palette', {'idx': 1}),
		('starteffect', {'name': 'spotlights'}),
		('scene_options', {'epilepsy': True}),

		# TODO frames_per_dancer -- we want to retain the dancer over two flashes.
		('alternate_animated', {'names': ('0d4488', '0d82fa', '0d3c00'), 'from': 0, 'counts': (170, 130, 50), 'start_palette': 0, 'mode': 'linear', 'strobe': False, 'frames_per_flash': 11, 'frames_per_dancer': 22, 'frames': 170 + 130 + 50}),

		('alternate_animated', {'names': ('0d4488', '0d82fa', '0d3c00'), 'from': 0, 'counts': (170, 130, 50), 'start_palette': 0, 'mode': 'linear', 'strobe': False, 'frames_per_flash': 11, 'frames_per_dancer': 22, 'frames': 129}),

		('fadeto', {'ms': 11 * MS_PER_ANIM_FRAME, 'values': (0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff)}),
		('split_anim', {'name': '0d4488', 'from': 0, 'to': 9}),

		('scene', {'name': 'phew', 'planes': (BITPLANE_1X1, BITPLANE_1X1, BITPLANE_OFF, BITPLANE_OFF, BITPLANE_OFF)}),
		('clear', {'plane': 'all'}),
		('starteffect', {'name': 'nothing'}),
		('palette', {'values': (0xff000000, 0xffffffff, 0xffaabbee, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xff552211, 0xffcc9944, 0xffcc9944, 0xffcc9944, 0xffbb8833, 0xffcc9944, 0xff997733, 0xff441122, 0xff441111, 0xff996633, 0xffcc9944, 0xffddbb44, 0xffcc9944, 0xffcc9944, 0xffcc9944, 0xff885522, 0xffaa7733, 0xffbb9933, 0xffcc9944, 0xff774422, 0xffaa8833, 0xff663322, 0xff552222, 0xff440022)}),

		('ilbm', {'name': 'data/phew.iff', 'display': 'fullscreen'}),
		('pause', {'ms': 1000}),

		('end', {}),

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
CMD_SCENE = 13
CMD_SCENE_INDEX = 14
CMD_MBIT = 15
CMD_SCENE_OPTIONS = 16

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

def encode_clear(args, state):
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

def encode_palette(args, state):
	# No-value palettes result in all-zero entries.
	values = list(args.get('values', ()))

	return 0, struct.pack(ENDIAN + 'II', CMD_PALETTE, 32) + _get_32_palette_entries(values)

def encode_alternate_palette(args, state):
	values = list(args.get('values', ()))

	return 0, struct.pack(ENDIAN + 'II', CMD_ALTERNATE_PALETTE, args['idx']) + _get_32_palette_entries(values)

def encode_use_alternate_palette(args, state):
	return 0, struct.pack(ENDIAN + 'II', CMD_USE_ALTERNATE_PALETTE, args['idx'])

def encode_fadeto(args, state):
	# This is implemented as a "fade to" command followed by the destination palette.
	ms = args['ms']
	if args.get('wait'):
		ms_taken = ms
	else:
		ms_taken = 0

	return ms_taken, struct.pack(ENDIAN + 'II', CMD_FADETO, ms) + encode_palette(args, state)[1]

def encode_anim(args, state):
	data_fn = 'data/%s_anim.bin' % (args['name'])
	get_file(data_fn)

	if 'transform_func' in args and data_fn not in state['wad']:
		xformed = args['transform_func'](data_fn)
		data_idx = state['wad'].add_bin(xformed, filename=data_fn)
	else:
		data_idx = state['wad'].add(data_fn)

	frame_from = args['from']
	frame_to   = args['to']
	bitplane = args.get('plane', 0)
	xor = args.get('xor', 1)

	ms = state['msperframe'] * (1 + abs(frame_to - frame_from))
	encoded = struct.pack(ENDIAN + 'IIHHHH', CMD_ANIM, data_idx, frame_from, frame_to, bitplane, xor)

	return ms, encoded

split_anim_map = None
def encode_split_anim(args, state):
	global split_anim_map
	if split_anim_map is None:
		get_file('data/split_map.json')
		with open('data/split_map.json', 'r') as h:
			split_anim_map = json.load(h)
	
	# frame_from..frame_to is a closed range.
	frame_from = args['from']
	frame_count = args.get('count')
	frame_to = (frame_from + frame_count - 1) if frame_count else args['to']

	if frame_to < frame_from:
		tmp = frame_to
		frame_to = frame_from
		frame_from = tmp
		backwards = True
	else:
		backwards = False

	bitplane = args.get('plane', 0)

	results = []

	split_anim_start_frame = 0
	for idx, num_split_anim_frames in enumerate(split_anim_map[args['name']]):
		split_anim_final_frame = (split_anim_start_frame + num_split_anim_frames) - 1

		if frame_from >= split_anim_start_frame and frame_from <= split_anim_final_frame:
			split_anim_name = '%s-%02d' % (args['name'], idx)
			split_anim_frame_from = frame_from - split_anim_start_frame
			split_anim_frame_to = min(split_anim_final_frame, frame_to) - split_anim_start_frame

			anim_args = {'name': split_anim_name,
					'from': split_anim_frame_from,
					'to': split_anim_frame_to,
					'xor': args.get('xor', 1),
					'plane': bitplane }

			if 'transform_func' in args:
				anim_args['transform_func'] = args['transform_func']

			results.append(('anim', anim_args))

			frames_displayed = (split_anim_frame_to - split_anim_frame_from) + 1
			frame_from += frames_displayed

			if frame_from > frame_to:
				break

		split_anim_start_frame += num_split_anim_frames
	else:
		raise RuntimeError("Couldn't find anims for requested frames")

	if backwards:
		results.reverse()
		for anim_cmd in results:
			tmp = anim_cmd[1]['to']
			anim_cmd[1]['to'] = anim_cmd[1]['from']
			anim_cmd[1]['from'] = tmp

	return results

def encode_end(args, state):
	return 0, struct.pack(ENDIAN + 'II', CMD_END, 1 if args.get('end_scene', False) else 0)

def encode_pause(args, state):
	return args['ms'], struct.pack(ENDIAN + 'I', CMD_PAUSE)

def encode_mod(args, state):
	if args['type'] == 'start':
		get_file(args['name'])
		packme = (CMD_MOD, 1, state['wad'].add(args['name']))
	elif args['type'] == 'stop':
		packme = (CMD_MOD, 2, 0)

	return 0, struct.pack(ENDIAN + 'III', *packme)

def encode_ilbm(args, state):
	get_file(args['name'])
	file_idx = state['wad'].add(args['name'])

	# display type: fullscreen = 0, centre = 1
	if 'display' in args:
		assert args['display'] == 'fullscreen'
		x, y, w, h = 0.0, 0.0, 1.0, 1.0
	else:
		x, y, w, h = args['x'], args['y'], args['w'], args['h']

	# Convert to a 0..240 scale (chosen because it's close to 255 but divisible by useful things like 2, 4, 8 etc)
	x = math.floor(x * 240); y = math.floor(y * 240)
	w = math.floor(w * 240); h = math.floor(h * 240)

	plane = args.get('plane', 0)

	# fade-in milliseconds 
	# if set, fade-ins happen from the currently-set palette, i.e. the ILBM is displayed without
	# changing the palette and a palette lerp is initiated to the ILBM colours. 
	fade_in_ms = args.get('fadein_ms', 0)

	return fade_in_ms, struct.pack(ENDIAN + 'IIBBBBII', CMD_ILBM, file_idx, x, y, w, h, fade_in_ms, plane)

def encode_mbit(args, state):
	# mbit is a compressed planar file format used by the Pebble demo
	get_file(args['name'])
	file_idx = state['wad'].add(args['name'])

	return 0, struct.pack(ENDIAN + 'II', CMD_MBIT, file_idx)

def encode_sound(args, state):
	get_file(args['name'])
	file_idx = state['wad'].add(args['name'])

	return 0, struct.pack(ENDIAN + 'II', CMD_SOUND, file_idx)

def pad_to_i32(block):
	if len(block) % 4 != 0:
		block += b'\00' * (4 - (len(block) % 4))
	
	return block

def encode_scene(args, state):
	planes_type = args['planes']

	if len(planes_type) == 5:
		planes_type = planes_type[:] + (BITPLANE_OFF,)

	assert len(planes_type) == 6

	# milliseconds per animation frame.
	msperframe = args.get('msperframe', MS_PER_ANIM_FRAME) 

	# Clear the following planes when an anim command finishes?
	post_anim_clear_plane_mask = args.get('post_anim_clear_plane_mask', 0)

	# Copy the anim plane to the following planes on first frame?
	first_anim_frame_plane_copy_mask = args.get('first_anim_frame_plane_copy_mask', 0)

	# Onion-skinning
	onion_skin = args.get('onion_skin')
	if onion_skin:
		onion_skin_cmd_high = SCENE_ONION_SKIN | onion_skin[0]
		onion_skin_cmd_low = onion_skin[1]
	else:
		onion_skin_cmd_high = onion_skin_cmd_low = 0

	# update the msperframe arg because it's used by animations
	state['msperframe'] = msperframe

	name = args['name'].encode('utf-8')

	encoded = pad_to_i32(struct.pack(ENDIAN + 'IBBBBBBBBBBBB', CMD_SCENE, *planes_type, msperframe,
		onion_skin_cmd_high, onion_skin_cmd_low, post_anim_clear_plane_mask, first_anim_frame_plane_copy_mask, len(name)) + name)
	return 0, encoded

def encode_scene_options(args, state):
	zoom = args.get('zoom', 1)
	distort = 1 if args.get('distort') else 0
	epilepsy = 1 if args.get('epilepsy') else 0
	flip_vertical = 1 if args.get('flip_vertical') else 0
	flip_horizontal = 1 if args.get('flip_horizontal') else 0
	anim_outline = 1 if args.get('anim_outline') else 0
	anim_3_behind_plane_1 = 1 if args.get('anim_3_behind_plane_1') else 0 # Show the anim, 3 frames behind, on plane 1
	multidraw_3d = 1 if args.get('multidraw_3d') else 0 # Use low nybble of draw command to determine which bitplane to draw on

	boolflags = (distort << 15) | (flip_vertical << 14) | (flip_horizontal << 13) | (anim_outline << 12) \
			| (anim_3_behind_plane_1 << 11) | (epilepsy << 10) | (multidraw_3d << 9)

	encoded = struct.pack(ENDIAN + 'IHH', CMD_SCENE_OPTIONS, zoom, boolflags)
	return 0, encoded

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

def _pack_copperpastels(palette_fade_ref, values):
	# Format is:
	#   int16_t index of palette to reference for fade-out and fade-in or -1 for no index
	#   uint16_t number of 4-byte values
	#   packed 4-byte values
	# where each 4-byte value has one byte for each of the four corners (top left, top right, bottom left, bottom right),
	# being W: white, R: red, Y: yellow, G: green, B: blue, X: black. 
	# EG YWWW is yellow in top-left, white everywhere else.
	num_values = len(values)
	values_bin = b''.join(value.encode('utf-8') for value in values)
	if len(values_bin) != num_values * 4:
		raise Exception("Pastels values must all be 4 bytes")

	return struct.pack(ENDIAN + "hH", palette_fade_ref, num_values) + values_bin

def encode_starteffect(args, state):
	effect_num = EFFECT_NUM[args['name']]
	#files_idx = [wad.add(filename) for filename in args.get('files', ())]
	#files_packed = struct.pack(ENDIAN + ('I' * len(files_idx)), *files_idx)

	# text block for VOTE! VOTE! VOTE! effect
	if args['name'] == 'votevotevote':
		args = pack_text_block(args.get('text', ()))
	elif args['name'] == 'copperpastels':
		args = _pack_copperpastels(args.get('palette_fade_ref', -1), args.get('values', ()))
	else:
		args = b''

	return 0, struct.pack(ENDIAN + 'II', CMD_STARTEFFECT, effect_num) + args

def encode_loadfont(args, state):
	get_file(args['name'])
	file_idx = state['wad'].add(args['name'])
	startchar = ord(args['startchar'])
	numchars = len(args['map']) // 4
	fontmap = struct.pack(ENDIAN + ('H' * len(args['map'])), *args['map'])
	packed = struct.pack(ENDIAN + 'IIII', CMD_LOADFONT, file_idx, startchar, numchars) + fontmap
	return 0, packed

def encode_demo_entry(entry, state):
	# return (tick count, encoded entry)
	command = entry[0]
	args = entry[1]

	encoder = globals()['encode_%s' % (command)]
	return encoder(args, state)

class SceneIndexEntry:
	def __init__(self, scene_tuples):
		self._length = None
		self.scene_count = len(scene_tuples)

		# Offset the tuple byte indices with our own length
		self.scene_tuples = [(scene[0], scene[1] + len(self), scene[2]) for scene in scene_tuples]

	def __len__(self):
		if self._length is None:
			length  = 4 # header: ms of this start (always 0)
			length += 4 # header: total length of this entry (including header)
			length += 4 # CMD_SCENE_INDEX
			length += 4 # number of indices
			length += 16 * (self.scene_count)  # array of (ms, byte-offset-from-choreography-start, 8-char-name)
			self._length = length

		return self._length

	def serialise(self):
		encoded = [struct.pack(ENDIAN + 'IIII', 0, len(self), CMD_SCENE_INDEX, len(self.scene_tuples))]
		for ms, offset, name in self.scene_tuples:
			encoded.append(struct.pack(ENDIAN + 'II8s', ms, offset, name.encode('utf-8')))

		return b''.join(encoded)

	def dump(self):
		print('Scenes:')
		for ms, _, name in self.scene_tuples:
			print('  %-22s %d' % (name, ms))

def get_demo_sequence(wad, choreography):
	# The time unit is milliseconds. Most animations run at 25fps or 40ms/frame.
	# This can be changed if necessary using the 'msperframe' key of a 'scene' entry.

	# Returns an encoded demo sequence.
	encoded = []

	# This is literally a bag of stuff passed around between encoders. The
	# scene command updates msperframe.
	# wad: this wad
	# msperframe: the most recent msperframe, from scene, or 40
	state = {
			'wad': wad,
			'msperframe': MS_PER_ANIM_FRAME,
	} 

	# Prepropcessors
	def preprocess_split_anim(choreography):
		for elem in choreography:
			if elem[0] == 'split_anim':
				for split_anim in encode_split_anim(elem[1], state):
					yield split_anim
			else:
				yield elem

	def preprocess_alternate_animated(choreography):
		for elem in choreography:
			if elem[0] == 'alternate_animated':
				args = elem[1]
				strobe = args.get('strobe', True)

				names = args.get('names')
				if not names:
					names = (args['name'],)

				frames_per_flash = args['frames_per_flash']
				if 'repeat' in args:
					num_frames = args['frames_per_flash'] * args['repeat']
				else:
					num_frames = args['frames']

				counts = args.get('counts')
				counts = list(counts) if counts else [num_frames]
				avail = counts[:]

				assert len(counts) == len(names)
				positions = [0 for i in range(len(counts))]

				# Assemble the frames list first, then output it. The idea is to preferentially pick the element
				# with the highest count, unless it's already been seen.
				def enumerate_counts(skip_idx=None):
					for idx in range(len(counts)):
						if skip_idx == idx or counts[idx] == 0:
							continue
						yield(idx, counts[idx])

				def max_count_idx(skip_idx=None):
					return max(enumerate_counts(skip_idx=skip_idx), key=lambda elem: elem[1])[0]

				palette_idx = args.get('start_palette', 0)
				last_selected_idx = -1
				while num_frames:
					name_idx = max_count_idx()
					if name_idx == last_selected_idx and len(counts) > 1:
						# We just did this one. Try to choose another.
						name_idx = max_count_idx(skip_idx=name_idx)

					frames_this_time = min(num_frames, frames_per_flash, counts[name_idx])
					to_frame = positions[name_idx] + frames_this_time - 1 # inclusive list

					split_anim_args = {
						'name': names[name_idx],
						'from': positions[name_idx],
						'to': to_frame
					}
					print('from frame', positions[name_idx], 'to', to_frame, 'avail', avail[name_idx])
					assert positions[name_idx] < avail[name_idx]
					assert to_frame < avail[name_idx]
					if strobe:
						yield('use_alternate_palette', {'idx': palette_idx})
					yield('split_anim', split_anim_args)
					palette_idx = 1 - palette_idx

					counts[name_idx] -= frames_this_time
					positions[name_idx] += frames_this_time
					num_frames -= frames_this_time
					last_selected_idx = name_idx
			else:
				yield elem
	
	def preprocess_scene_ends(choreography):
		first_scene = True
		for elem in choreography:
			if elem[0] == 'scene':
				if first_scene:
					first_scene = False
				else:
					yield('end', {'end_scene': True})

			yield elem

	# Preprocess animation splits. This is ugly...
	choreography = preprocess_split_anim(preprocess_alternate_animated(preprocess_scene_ends(choreography)))

	previous_start = 0 # ms
	previous_end = 0 # ms

	scene_start = [] # (ms, position-in-file, name)

	byte_position = 0
	for entry in choreography:

		this_entry_ms, encoded_entry = encode_demo_entry(entry, state)
		
		this_start = previous_end
		this_end = this_start + this_entry_ms

		#if entry[0] == 'after':
		#	this_start = previous_end
		#	this_end = this_start + this_entry_ms
		#elif entry[0] == 'simultaneously':
		#	this_start = previous_start
		#	this_end = max(previous_end, this_start + this_entry_ms)
		#else:
		#	raise NotImplementedError()

		if entry[0] == 'scene':
			# Add the scene to the scenes list
			scene_start.append((this_end, byte_position, entry[1]['name']))
	
		# Two-byte frame number, two-byte length (including frame # and length), then encoded data.
		print(this_start, entry)
		encoded_command = struct.pack(ENDIAN + 'II', this_start, len(encoded_entry) + 8) + encoded_entry
		encoded.append(encoded_command)
		byte_position += len(encoded_command)


		previous_start = this_start
		previous_end = this_end

	# Create the scene index and stick it at the start. The scene index consists of
	# (uint32_t ms, uint32_t byte-offset-inside-choreography)
	# ... for each scene.
	scene_start.append((0xffffffff, byte_position, 'end')) # EOF with special MS value
	scene_indices = SceneIndexEntry(scene_start)
	encoded.insert(0, scene_indices.serialise())

	scene_indices.dump()

	return b''.join(encoded)

def build_demo(choreography, filename):
	wad = Wad(ENDIAN)
	encoded = get_demo_sequence(wad, choreography)
	print("Choreography length: %d bytes" %(len(encoded),))
	wad.add_bin(encoded, is_choreography=True)

	dirname = os.path.dirname(filename)
	if dirname and not os.path.exists(dirname):
		os.makedirs(dirname)

	central_directory_size = wad.write(filename)
	print("wrote %s (central directory size: %d)" % (filename, central_directory_size))

if __name__ == '__main__':
	build_demo(DEMO, 'sota.wad')

