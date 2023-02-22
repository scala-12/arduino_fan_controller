// from Alex Gyver site
bool digital_read_fast(uint8_t pin) {
  if (pin < 8) {
    return bitRead(PIND, pin);
  } else if (pin < 14) {
    return bitRead(PINB, pin - 8);
  } else if (pin < 20) {
    return bitRead(PINC, pin - 14);  // Return pin state
  }
}

byte find_median(byte* buffer, byte size) {
  for (int j = 0; j < size; ++j) {
    byte temp = buffer[j];
    byte ind = j;
    for (byte i = j + 1; i < size; ++i) {
      if (temp > buffer[i]) {
        temp = buffer[i];
        ind = i;
      }
    }
    buffer[ind] = buffer[j];
    buffer[j] = temp;
  }

  return buffer[(int)size / 2];
}