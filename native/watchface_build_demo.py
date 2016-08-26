import build_demo
import libsotadisk

def compress_animation(filename):
	# Divide all extends by 2, so we go 0-127 rather than 0-255.

	with open(filename, 'rb') as h:
		reader = libsotadisk.ByteReader(h)
		indices, anims = libsotadisk.read_packed_animation(reader)

	for drawcommand in anims.values():
		for command in drawcommand:
			if isinstance(command, libsotadisk.Vector):
				command.for_each_point(lambda x, y: (x // 2, y // 2))

	return libsotadisk.serialise_packed_animation(indices, anims)

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
			watch.append((element[0], 'palette', {'values': element[2]['values']}))
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
			# TODO turn these into mbit images
			if 'fadein_ms' in element[2]:
				del element[2]['fadein_ms']

			watch.append(element)
			watch.append(('after', 'pause', {'ms': 40}))
		else:
			watch.append(element)
	
	return watch

build_demo.build_watchface(transform_demo(build_demo.DEMO))

