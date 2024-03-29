#ifndef FUNCTIONS_FILE_INCLUDED
#define FUNCTIONS_FILE_INCLUDED

#include <Arduino.h>

#include "macros.h"

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

template <byte SIZE, typename T>
T find_median(T buffer[SIZE], byte avarage_radius, bool do_sort) {
  T sorted[SIZE];
  for (byte i = 0; i < SIZE; ++i) {
    sorted[i] = buffer[i];
  }

  for (byte j = 0; j < SIZE; ++j) {
    T temp = sorted[j];
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

  float sum = sorted[middle_index];
  for (byte i = 1; i <= avarage_radius; ++i) {
    sum += (sorted[middle_index + i] - sum) * 0.2;
    sum += (sorted[middle_index - i] - sum) * 0.2;
  }

  if (do_sort) {
    for (byte i = 0; i < SIZE; ++i) {
      buffer[i] = sorted[i];
    }
  }

  return sum;
}

template <byte SIZE, typename T>
T find_median(T buffer[SIZE]) {
  return find_median<SIZE, T>(buffer, 0, false);
}

template <byte SIZE, typename T>
T find_median(T buffer[SIZE], bool with_simple_avg) {
  return find_median<SIZE, T>(buffer, (with_simple_avg) ? 1 : 0, false);
}

// с сайта, но переделанно https://forum.amperka.ru/threads/Подсчёт-числа-символов-в-строке.19457/
uint16_t str_length(char* source) {
  uint16_t source_len = strlen(source);
  uint16_t result = 0;
  unsigned char source_char;
  char m[2] = {'0', '\0'};
  for (uint16_t i = 0; i < source_len; ++i, ++result) {
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

byte find_char_count(char* chars, char letter) {
  uint16_t from = 0;
  byte counter = 0;
  mString<get_arr_len(chars)> data;
  data.add(chars);
  for (int i = 0; i != -1;) {
    i = data.indexOf(letter, from);
    if (i != -1) {
      from = i + 1;
      ++counter;
    }
  }

  return counter;
}

void fixed_delay(uint32_t ms) {
  uint32_t tmr_start = millis();
  uint32_t tmr_end = tmr_start + ms;
  if (tmr_start > tmr_end) {
    while (tmr_end < millis()) {
    }
  }

  while (tmr_end > millis()) {
  }
}

// хранить не время последнего обновления, а время следующего
bool is_success_delay(uint32_t& last_time, uint32_t timeout) {
  uint32_t current = millis();

  uint32_t before_over = -1 - last_time;
  if (before_over > timeout) {
    return ((max(current, last_time) - min(current, last_time)) >= timeout);
  }

  return before_over + timeout <= current;
}

byte calculate_avg(byte a, byte b) {
  return ((a ^ b) >> 1) + (a & b);
}

#endif
