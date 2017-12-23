import struct

def _chunk(name, data):
	if len(data) & 1:
		data += b'\0'

	return name + struct.pack('>I', len(data)) + data

def _chunk_bmhd(width, height, num_planes):
	bmhd = struct.pack('>HHHHBBBBHBBHH',
			width,
			height,
			0, 0, # origin
			num_planes, # numplanes
			0, # mask
			1, # compression (0 = none, 1 = RLE)
			0, # pad
			0, # transclr
			1, 1, # aspect ratio
			320, 200) # size of target display
	
	return _chunk(b'BMHD', bmhd)

def _rle_compress(row_bytes):
	idx = 0
	out = []

	unsure = []
	unsure_rle_count = 0

	def output_unsure():
		assert unsure
		
		# This may end with a run of same-valued data. If it is more than 3 same values, output it as compressed.
		end_idx = len(unsure) - 1
		while end_idx >= 0 and unsure[end_idx] == unsure[-1]:
			end_idx -= 1

		# Don't bother if it's a small number of compressed bytes.
		if len(unsure) - end_idx < 4:
			end_idx = len(unsure) - 1

		if end_idx >= 0:
			# end_idx now points to the final value to output *uncompressed*
			out.append(end_idx) # read 'value + 1' bytes
			out.extend(unsure[:end_idx + 1])

		count = (len(unsure) - end_idx) - 1
		if count > 0:
			assert count >= 2
			out.append(257 - count)
			out.append(unsure[-1])

		unsure.clear()

	for b in row_bytes:
		if unsure:
			if b != unsure[-1] and unsure_rle_count:
				# We have some compression going on, but not here...
				output_unsure()
				unsure_rle_count = 0
				unsure.append(b)
				continue
			else:
				if b == unsure[-1]:
					unsure_rle_count += 1

				unsure.append(b)
		else:
			unsure.append(b)
	
	if unsure:
		output_unsure()

	return bytes(out)


def _chunk_body(height, num_planes, data):
	imagedata = []
	for row in range(height):
		for plane in range(num_planes):
			row_bytes = next(data)
			if(len(row_bytes) % 2) == 1:
				row_bytes += b'\0'

			imagedata.append(_rle_compress(row_bytes))
	
	imagedata = b''.join(imagedata)

	return _chunk(b'BODY', imagedata)


def _plane_interleaver(width, planes):
	#assert width % 16 == 0
	idx = 0
	width_bytes = width // 8

	while idx < len(planes[0]):
		for plane in planes:
			yield plane[idx : idx + width_bytes]

		idx += width_bytes

def to_iff(planes, width, height):
	"""
	"""
	num_planes = len(planes)

	chunks = b'ILBM' + _chunk_bmhd(width, height, num_planes) + _chunk_body(height, len(planes), _plane_interleaver(width, planes))
	container = b'FORM' + struct.pack('>I', len(chunks)) + chunks

	return container

if __name__ == '__main__':
	#b = _rle_compress(bytes([0, 1, 2, 3, 4, 5, 6, 7]))
	#b = _rle_compress(bytes([0, 0, 0, 0, 0, 0, 0, 0]))
	b = _rle_compress(bytes([1, 0, 1, 1, 1, 0, 0, 0, 0]))
	print(list(b))

