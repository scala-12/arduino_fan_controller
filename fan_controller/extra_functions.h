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

template <byte SIZE>
byte find_median(byte buffer[SIZE], byte avarage_radius, bool do_sort) {
  byte* sorted;
  if (do_sort) {
    sorted = buffer;
  } else {
    byte buffer_copy[SIZE];
    for (byte i = 0; i < SIZE; ++i) {
      buffer_copy[i] = buffer[i];
    }
    sorted = buffer_copy;
  }

  for (int j = 0; j < SIZE; ++j) {
    byte temp = sorted[j];
    byte ind = j;
    for (byte i = j + 1; i < SIZE; ++i) {
      if (temp > sorted[i]) {
        temp = sorted[i];
        ind = i;
      }
    }
    sorted[ind] = sorted[j];
    sorted[j] = temp;
  }

  byte middle_index = SIZE >> 1;
  if (avarage_radius <= 0) {
    return sorted[middle_index];
  }

  int sum = sorted[middle_index];
  for (byte i = 1; i <= avarage_radius; ++i) {
    sum += sorted[middle_index + i] + sorted[middle_index - i];
  }

  return sum / ((avarage_radius << 1) + 1);
}

template <byte SIZE>
byte find_median(byte* buffer) {
  return find_median<SIZE>(buffer, 0, false);
}

template <byte SIZE>
byte find_median(byte* buffer, bool with_simple_avg) {
  return find_median<SIZE>(buffer, (with_simple_avg) ? 1 : 0, false);
}

#define convert_by_sqrt(x, min_x, max_x, min_y, max_y) map(sqrt(map(constrain(x, min_x, max_x), min_x, max_x, 0, 900)), 0, 30, min_y, max_y)
#define print_bits(bits, size)                                    \
  for (byte __counter__ = 0; __counter__ < size; ++__counter__) { \
    uart.print((bitRead(bits, __counter__)) ? 1 : 0);             \
  }

// с сайта, но переделанно https://forum.amperka.ru/threads/Подсчёт-числа-символов-в-строке.19457/
uint16_t str_length(char* source) {
  int source_len = strlen(source);
  int result = 0;
  unsigned char source_char;
  char m[2] = {'0', '\0'};
  for (int i = 0; i < source_len; ++i, ++result) {
    source_char = source[i];

    if (source_char >= 0xBF) {
      if (source_char == 0xD0) {
        ++i;
        if (source[i] == 0x81) {
          source_char = 0xA8;
        } else if (source[i] >= 0x90 && source[i] <= 0xBF) {
          source_char = source[i] + 0x2F;
        } else {
          source_char = source[i];
        }
      } else if (source_char == 0xD1) {
        ++i;
        if (source[i] == 0x91) {
          source_char = 0xB7;
        } else if (source[i] >= 0x80 && source[i] <= 0x8F) {
          source_char = source[i] + 0x6F;
        } else {
          source_char = source[i];
        }
      }
    }
    m[0] = source_char;
  }

  return result;
}
