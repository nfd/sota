static inline size_t align(int amt, int align) {
	return amt + ((align - (amt % align)) % align);
}

