"""
Split animations to a maximum chunk size of 4k. Compress each chunk with zlib.
"""
import libsotadisk
import glob
import struct
import os
import json

SOURCE_ANIMS = glob.glob('native/data/??????_anim.bin')
COMPRESSED_ANIM_DIR = 'native/data'

MAX_ANIM_SIZE = 4 * 1024

if not os.path.exists(COMPRESSED_ANIM_DIR):
	os.makedirs(COMPRESSED_ANIM_DIR)

class Rewind(Exception):
	def __init__(self, rewind_amount):
		self.rewind_amount = rewind_amount

class GrowableAnimation:
	def __init__(self):
		self.indices = []
		self.anims = {} # index : data
		self.anims_length = 0

	def __len__(self):
		return 2 + (2 * len(self.indices)) + self.anims_length

	def add(self, anims, index):
		anim = anims[index]

		if self._new_length(index, anim) > MAX_ANIM_SIZE:
			self._rewind()

		if index not in self.anims:
			self.anims[index] = anim
			self.anims_length += len(anim)

		self.indices.append(index)

	def _rewind(self):
		# This animation block is full. We must remove any tween animations from the end of the
		# block, because they will reference data in the next block.
		rewind_count = 0

		while self.indices:
			anim = self.anims[self.indices[-1]]
			if anim.has_tweens():
				rewind_count += 1
				self.indices.pop()
			else:
				break

		raise Rewind(rewind_count)

	def _new_length(self, index, anim):
		new_length = 2 # size of new index

		if index not in self.anims:
			new_length += len(anim) # Also add animation

		return new_length + len(self)

	def serialise(self):
		serialised = [struct.pack('>H', len(self.indices))]

		anims_to_store = [key for key in self.anims.keys() if key in self.indices]
		anims_to_store.sort()

		# Rewrite indices to match animation data. Indices are based from the
		# start of animation data.
		new_indices = {} # map old to new

		new_idx_current = 0
		for orig_idx in anims_to_store:
			new_indices[orig_idx] = new_idx_current
			new_idx_current += len(self.anims[orig_idx])

		# Now add the new indices
		for orig_idx in self.indices:
			serialised.append(struct.pack('>H', new_indices[orig_idx]))

		# Now add the data.
		for orig_idx in anims_to_store:
			serialised.append(bytes(self.anims[orig_idx].serialise()))

		return new_indices, b''.join(serialised)

	def empty(self):
		return len(self.indices) == 0

def split_anim(source_filename):
	print('Split %s' % (source_filename))
	with open(source_filename, 'rb') as h:
		source_indices, source_anims = libsotadisk.read_packed_animation(libsotadisk.ByteReader(h))

	anim = GrowableAnimation()

	# Keep adding stuff until the anim gets too large.
	source_idx_idx = 0
	while source_idx_idx < len(source_indices):
		source_index = source_indices[source_idx_idx]

		try:
			anim.add(source_anims, source_index)
		except Rewind as e:
			yield anim
			anim = GrowableAnimation()
			source_idx_idx -= e.rewind_amount
		else:
			source_idx_idx += 1

		assert len(anim) <= MAX_ANIM_SIZE

	if not anim.empty():
		yield anim
	
def main():
	anim_split_map = {}

	for source in sorted(SOURCE_ANIMS):
		basename = os.path.basename(source).split('_anim')[0]
		dest_base = os.path.join(COMPRESSED_ANIM_DIR, basename)

		anim_split_map[basename] = []

		for idx, anims in enumerate(split_anim(source)):
			filename = '%s-%02x_anim.bin' % (dest_base, idx)
			old_to_new_dict, anim_data = anims.serialise()

			print("   ", filename, len(anims.indices))
			with open(filename, 'wb') as h:
				h.write(anim_data)

			anim_split_map[basename].append(len(anims.indices))
	
	with open(os.path.join(COMPRESSED_ANIM_DIR, 'split_map.json'), 'w') as h:
		json.dump(anim_split_map, h)

if __name__ == '__main__':
	main()

