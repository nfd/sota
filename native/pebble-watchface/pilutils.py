def decode_lousy_pil_palette_data(pil_palette_data):
	typ, dat = pil_palette_data

	palette = []

	if typ == 'RGB':
		assert len(dat) % 3 == 0
		for idx in range(0, len(dat), 3):
			palette.append((dat[idx], dat[idx + 1], dat[idx + 2]))
	else:
		raise NotImplementedError(typ)

	return palette

