import math

import build_demo
import libsotadisk

def compress_animation(filename):
	# Divide all extends by 2, so we go 0-127 rather than 0-255.

	with open(filename, 'rb') as h:
		reader = libsotadisk.ByteReader(h)
		indices, anims = libsotadisk.read_packed_animation(reader)

	#for drawcommand in anims.values():
	#	for command in drawcommand:
	#		if isinstance(command, libsotadisk.Vector):
	#			command.for_each_point(lambda x, y: (x // 2, y // 2))

	return libsotadisk.serialise_packed_animation(indices, anims)

CURATED_COLOURS = {
		0xff110022: 0xff000040,
		0xff221144: 0xff0000c0,
		0xff110033: 0xff000080,
}

def palette_boost(palette):
	# Some manual curation is required to make the SOTA palette look nice on
	# Pebble. The manual colours, CURATED_COLOURS, are above. For the rest, the
	# Pebble GColorFromHEX function truncates palette values (takes the top two
	# bits of each colour), which means low-intensity colours like #003 turn
	# into pure black, #000. Avoid this by rounding up instead.
	
	def boost_single(v):
		return v
		return min(255, math.ceil(v / 64) * 64)

	def boost(argb):
		if argb in CURATED_COLOURS:
			boosted = CURATED_COLOURS[argb]
		else:
			b = boost_single(argb & 0xff)
			g = boost_single((argb & 0xff00) >> 8)
			r = boost_single((argb & 0xff0000) >> 16)
			a = boost_single((argb & 0xff000000) >> 24)
			boosted = (a << 24) | (r << 16) | (g << 8) | b

		print("boost %08x -> %08x" % (argb, boosted))
		return boosted

	return [boost(elem) for elem in palette]

def transform_demo(demo):
	# Transform regular demo into watchface.
	watch = []

	for element in demo:
		discard = False

		# There's no 'simultaneously', only 'after' -- this isn't a hard
		# restriction, but 'simultaneously' is just used for fades and on the
		# watch those are gone (not enough colours!)
		if element[0] == 'simultaneously':
			element = ('after',) + element[1:]

		if element[1] in ('anim', 'split_anim'):
			element[2]['transform_func'] = compress_animation
			watch.append(element)
		elif element[1] == 'fadeto':
			# Convert all fades to instantaneous palette changes.
			watch.append((element[0], 'palette', {'values': palette_boost(element[2]['values'])}))
		elif element[1] == 'palette':
			watch.append((element[0], 'palette', {'values': palette_boost(element[2]['values'])}))
		elif element[1] == 'mod':
			# Remove music
			pass
		elif element[1] == 'sound':
			# Remove sound effects
			pass
		elif element[1] == 'pause':
			if element[2].get('watchface-hint') == 'keep':
				# No artificial delays -- but keep the one for "VOTE!VOTE!VOTE!"
				watch.append(element)
		elif element[1] == 'ilbm':
			# Keep images, remove fade-in delay, add one-frame pause.
			element[2]['name'] = element[2]['name'].replace('.iff', '.mbit')

			watch.append((element[0], 'mbit', element[2]))
			watch.append(('after', 'pause', {'ms': 1000}))
		elif element[1] == 'loadfont':
			element[2]['name'] = element[2]['name'].replace('.iff', '.mbit')
			# Watch font characters are half the size of regular.
			element[2]['map'] = [position // 2 for position in element[2]['map']]
			watch.append(element)
		else:
			watch.append(element)
	
	return watch

build_demo.build_watchface(transform_demo(build_demo.DEMO))

