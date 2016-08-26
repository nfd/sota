import build_demo

def transform_demo(demo):
	# Transform regular demo into watchface.
	watch = []

	for element in demo:
		discard = False

		if element[1] == 'fadeto':
			# Convert all fades to instantaneous palette changes.
			element = (element[0], 'palette', {'values': element[2]['values']})
		elif element[1] == 'mod':
			# Remove music
			discard = True
		elif element[1] == 'sound':
			# Remove sound effects
			discard = True
		elif element[1] == 'pause':
			# No artificial delays -- but keep the one for "VOTE!VOTE!VOTE!"
			discard = element[2].get('watchface-hint') != 'keep'
	
		if not discard:
			watch.append(element)
	
	return watch

build_demo.build_watchface(transform_demo(build_demo.DEMO))

