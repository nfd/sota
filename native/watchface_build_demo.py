import build_demo

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

		if element[1] == 'fadeto':
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

