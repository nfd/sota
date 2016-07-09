import build_demo

WATCH_DEMO = [
		# Intro: hand moving out
		('after', 'clear', {'plane': 'all'}),
		('after', 'palette', {'values': (0xff110022, 0xff221144, 0xff110022, 0xff110033)}),
		('after', 'anim', {'name': 0x10800, 'from': 0, 'to': 50}),
		# Girl with gun displayed inside hand
		('after', 'anim', {'name': 0x2c00, 'from': 0, 'to': 141, 'plane': 1}),
		('after', 'clear', {'plane': 1}),
		# Hand zooms out; dancing Bond girls. Frame 51 is skipped because it's weird (draws two hands almost over each other)
		# xor is manually disabled -- this is probably also encoded in draw cmds but whatever.
		('after', 'anim', {'name': 0x10800, 'from': 52, 'to': 145, 'xor': 0}),
		('after', 'anim', {'name': 0x10800, 'from': 146, 'to': 334, 'xor': 1}),

		# Title: STATE OF THE ART, credits, dragon pic
		# TODO: use PDCs.
		#('after', 'ilbm', {'name': 'data/state.iff', 'display': 'fullscreen'}),
		('after', 'pause', {'ms': 1000}),
		#('after', 'ilbm', {'name': 'data/of.iff', 'display': 'fullscreen'}),
		('after', 'pause', {'ms': 1000}),
		#('after', 'ilbm', {'name': 'data/the.iff', 'display': 'fullscreen'}),
		('after', 'pause', {'ms': 1000}),
		#('after', 'ilbm', {'name': 'data/art.iff', 'display': 'fullscreen'}),
		('after', 'pause', {'ms': 1000}),

		#('after', 'ilbm', {'name': 'data/authors.iff', 'display': 'fullscreen', 'fadein_ms': 2000}),
		('after', 'pause', {'ms': 3000}),

		#('after', 'ilbm', {'name': 'data/dragon.iff', 'display': 'fullscreen', 'fadein_ms': 500}),
		('after', 'pause', {'ms': 4000}),

		# End of intro finally! fade 'er out.
		('after', 'fadeto', {'ms': 1000, 'values': ()}),

		# Morph to first dancer
		('after', 'clear', {'plane': 'all'}),
		('simultaneously', 'starteffect', {'name': 'spotlights'}),
		('after', 'palette', {'values': (0xff000000, 0xff000000, 0xff117700, 0xffdd0000, 0xff770077, 0xffdd7700, 0xff771111,
			0xffbbbb00)}),
		('simultaneously', 'anim', {'name': 0x67080, 'from': 0, 'to': 25}),
		('after', 'anim', {'name': 0x67230, 'from': 0, 'to': 195}),
		('after', 'anim', {'name': 0x67230, 'from': 196, 'to': 209}),
		('after', 'starteffect', {'name': 'nothing'}),

		# VOTE! VOTE! VOTE!
		('after', 'clear', {'plane': 'all'}),
		# black-on-white version
		('after', 'alternate_palette', {'idx': 0, 'values': (0xffeeffff, 0xff000000, 0xff779999, 0xffddeeee, 0xff114444, 0xff447777, 0xff003333, 0xffaaaadd)}),
		# white-on-black version
		('after', 'alternate_palette', {'idx': 1, 'values': (0xff000000, 0xffeeffff, 0xff336666, 0xff113333, 0xffaabbbb, 0xff778888, 0xffccdddd, 0xff224444)}),
		('after', 'loadfont', {'name': 'data/fontmap.iff', 'startchar': 'A', 'map' : build_demo.FONTMAP}),
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

		# Dancer 4: "crazy hips" dancer.
		('after', 'palette', {'values': (0xff110022, 0xff221144, 0xff221144, 0xff221144)}),
		('after', 'starteffect', {'name': 'copperpastels'}), # TODO also must show 3-delayed image, maybe by turning bitplanes off
		#('after', 'anim', {'name': 0x8da00, 'from': 0, 'to': 210}),
		('after', 'pause', {'ms': 3000}),


		('after', 'end', {}),
]

build_demo.build_watchface(WATCH_DEMO)

