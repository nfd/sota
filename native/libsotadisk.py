# Read vectors from the SOTA disk.

# Played around with the format a bit to try to save space.
# - went from (x, y) to (x, x, x...), (y, y, y, ...) to give
# more predictability to data for compression. Insignificant
# improvements.
# - stored only difference between points rather than the point
# itself -- wrote out short ints rather than bytes (-255 to 255);
# slight increase in size, maybe 20%
# - saturated at (-127, 128). This saved 25%, which isn't enough
# to work on a format which doesn't throw away the upper bit.
import struct
import os
from collections import namedtuple

class DrawCommands:
	def __init__(self, commands, position=None):
		self.commands = commands
		self.position = position

	def serialise(self):
		flat = [len(self.commands)]

		for command in self.commands:
			flat.extend(command.serialise())

		return flat

	def tween_references(self):
		for command in self.commands:
			print(command)
			if isinstance(command, Tween):
				yield command.from_absolute
				yield command.to_absolute
	
	def has_tweens(self):
		for cmd in self.commands:
			if isinstance(cmd, Tween):
				return True
		else:
			return False

	def __iter__(self):
		return iter(self.commands)

#	def has_points_at_position(self, position):
#		for cmd in self.commands:
#			if isinstance(cmd, Vector) and cmd.points_position == position:
#				return True
#
#		return False
#
#	def has_tweens_referencing(self, draw_commands):
#		for cmd in self.commands:
#			if isinstance(cmd, Tween):
#				if draw_commands.has_points_at_position(cmd.from_absolute) \
#					or draw_commands.has_points_at_position(cmd.to_absolute):
#						return True
#
#		return False

	def __len__(self):
		return len(self.serialise())

class Vector:
	def __init__(self, cmd_minor, points, points_position=None):
		self.cmd_minor = cmd_minor
		self.points = points
		self.points_position = points_position

	@classmethod
	def deserialise(cls, cmd_minor, read):
		num_points = read(1)
		points_offset = read.tell()
		points = list(read(num_points * 2))
		#points = [point // 2 for point in points]
		return cls(cmd_minor, points, points_position=points_offset)

	def serialise(self):
		assert len(self.points) % 2 == 0
		return struct.pack('>BB', self.cmd_minor, len(self.points) // 2) \
				+ bytes(self.points)

	def __repr__(self):
		return '<Vector: %d, %r>' % (self.cmd_minor,
				self.points)

	def for_each_point(self, func):
		for idx in range(0, len(self.points), 2):
			x, y = func(self.points[idx], self.points[idx + 1])
			self.points[idx] = x
			self.points[idx + 1] = y

class Tween:
	def __init__(self, cmd_minor, from_, to_, t, count, position=None):
		self.cmd_minor = cmd_minor
		self.from_ = from_
		self.to_ = to_
		self.t = t
		self.count = count

		if position is not None:
			self.from_absolute = position - self.from_
			self.to_absolute = position + self.to_ + 2

	@classmethod
	def deserialise(cls, cmd_minor, read):
		position = read.tell()
		return cls(cmd_minor, *struct.unpack('>HHBB', read(6)),
				position=position)

	def serialise(self):
		return struct.pack('>BHHBB', self.cmd_minor,
				self.from_, self.to_, self.t, self.count)

	def __repr__(self):
		return '<Tween: %d, %d, %d, %d, %d>' % (self.cmd_minor,
				self.from_, self.to_, self.t, self.count)

def split_draw_commands(read_func, position_offset=0):
	# return (num_vectors, [(type, data), ...])
	position = read_func.tell()

	num_commands = read_func(1)

	commands = []

	for i in range(num_commands):
		cmd_minor = read_func(1)

		if cmd_minor & 0xf0 == 0xd0:
			# Drawing command
			commands.append(Vector.deserialise(cmd_minor, read_func))
		elif cmd_minor in (0xe6, 0xe7, 0xe8, 0xf2):
			# Tweening
			commands.append(Tween.deserialise(cmd_minor, read_func))
		else:
			print("Unknown cmd %02x @%06x" % (cmd_minor, read_func.tell() - 1))
			raise NotImplementedError()

	return DrawCommands(commands, position=position - position_offset)

def read_packed_animation(read_func):
	num_indices = struct.unpack(">H", read_func(2))[0]
	indices = []
	anims = {} # per index

	for idx in range(num_indices):
		indices.append(struct.unpack('>H', read_func(2))[0])

	position_offset = read_func.tell()

	while not read_func.eof:
		anim = split_draw_commands(read_func, position_offset=position_offset)
		anims[anim.position] = anim

	return indices, anims

def serialise_packed_animation(indices, anims):
	serialised = [struct.pack('>H', len(indices))]

	for idx in indices:
		serialised.append(struct.pack('>H', idx))

	for idx in indices:
		serialised.append(bytes(anims[idx].serialise()))

	return b''.join(serialised)

class ByteReader:
	def __init__(self, handle):
		self.handle = handle
		self._eof = False
		self.size = os.fstat(self.handle.fileno()).st_size

	@property
	def eof(self):
		return self._eof

	def tell(self):
		return self.handle.tell()

	def __call__(self, num):
		b = self.handle.read(num)
		if len(b) < num:
			self._eof = True

		if self.handle.tell() == self.size:
			self._eof = True

		if num == 1 and b:
			return struct.unpack('B', b)[0]
		else:
			return b

def read_draw_command(handle):
	# Assuming handle is a file positioned at the start
	# of a draw command, read the entire thing (command
	# and data) and return it. Handle is positioned
	# after the final byte of the draw command.
	read_func = ByteReader(handle)

	cmds = split_draw_commands(read_func)

	# Just flatten the data again for storage.
	return cmds.serialise()

def getscript(handle):
	# Assuming handle is a file positioned at the start of a
	# set of script indices, read the indices and draw commands
	# and return a dict containing this data.

	script = {}

	# Read the index first.
	startpos = handle.tell()

	num_indices = struct.unpack('>H', handle.read(2))[0]
	indices = []

	for i in range(num_indices + 1):
		index = struct.unpack('>I', handle.read(4))[0]
		indices.append(index)

	# find the smallest index and subtract to get 0-based indices.
	script_offset = min(indices)
	for i in range(len(indices)):
		indices[i] -= script_offset
		assert indices[i] >= 0

	# Read the script. Note that indices may repeat, hence the dict.
	for index in indices:
		pos = startpos + index + script_offset
		if index not in script:
			handle.seek(pos)
			script[index] = read_draw_command(handle)
			if script[index] is None:
				print("Early exit")
				break

	# Combine all the script portions into one byte array.
	max_length = max(script.keys())
	max_length += len(script[max_length])

	all_scripts = [0] * max_length

	for index, thescript in script.items():
		all_scripts[index:index+len(thescript)] = thescript

	return {'indices': indices, 'data': all_scripts}

