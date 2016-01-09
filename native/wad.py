"""
Make a big file what has smaller files inside it.

Format:

4 bytes: 'mess'
4 bytes: number of files
4 bytes: index of the choreography file
4 bytes: index (from start of file) to beginning of first file
4 bytes: length of first file
...
"""
import struct

class Wad:
	def __init__(self, endian):
		# endian in struct format, i.e. > or <
		self.files = [] # list of (size, data) tuples
		self.filename_to_idx = {}
		self.choreography_idx = -1
		self.endian = endian

	def add(self, filename, is_choreography=False):
		if filename in self.filename_to_idx:
			idx = self.filename_to_idx[filename]
		else:
			with open(filename, 'rb') as h:
				filedata = h.read()

			idx = self.add_bin(filedata, is_choreography=is_choreography)

		self.filename_to_idx[filename] = idx

		return idx

	def add_bin(self, filedata, is_choreography=False):
		idx = len(self.files)

		filelength = len(filedata)
		if len(filedata) % 4 != 0:
			filedata += b'\0' * (4 - (len(filedata) % 4))

		self.files.append((filelength, filedata))

		if is_choreography:
			self.choreography_idx = idx

		return idx

	def write(self, filename):
		# calculate sizes
		next_file_position = 4 + 4 + 4 + (len(self.files) * 4 * 2)

		with open(filename, 'wb') as h:
			h.write(b'mess')
			h.write(struct.pack(self.endian + 'II', len(self.files), self.choreography_idx))

			# write index
			for filelength, filedata in self.files:
				h.write(struct.pack(self.endian + 'II', next_file_position, filelength))
				next_file_position += len(filedata)

			# write data
			for filelength, filedata in self.files:
				h.write(filedata)

