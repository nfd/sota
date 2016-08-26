static inline int byte_get_bit(uint8_t byte, uint8_t bit) {
  return ((byte) >> bit) & 1;
}

