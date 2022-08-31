// from Alex Gyver site

bool digitalReadFast(uint8_t pin) {
  if (pin < 8) {
    return bitRead(PIND, pin);
  } else if (pin < 14) {
    return bitRead(PINB, pin - 8);
  } else if (pin < 20) {
    return bitRead(PINC, pin - 14);    // Return pin state
  }
}

// ВНИМАНИЕ! Нужное опорное установлено DEFAULT, можно изменить на своё
uint16_t analogReadFast(uint8_t pin) {
  pin = ((pin < 8) ? pin : pin - 14);    // analogRead(2) = analogRead(A2)
  ADMUX = (DEFAULT<< 6) | pin;    // Set analog MUX & reference
  bitSet(ADCSRA, ADSC);            // Start 
  while (ADCSRA & (1 << ADSC));        // Wait
  return ADC;                // Return result
}

byte median_filter5(byte value, byte* buffer) {
  byte min_value = buffer[0];
  byte max_value = buffer[0];
  byte values3[3];
  byte count_values = 0;
  for (byte i = 1; i < 5; ++i) {
    if (min_value >= buffer[i]) {
      if (min_value != max_value) {
        values3[count_values] = min_value;
        count_values += 1;
      }
      min_value = buffer[i];
    } else if (max_value <= buffer[i]) {
      if (min_value != max_value) {
        values3[count_values] = max_value;
        count_values += 1;
      }
      max_value = buffer[i];
    } else {
      values3[count_values] = buffer[i];
      count_values += 1;
    }
  }

  byte middle;
  if (count_values > 0) {
    for (byte i = count_values - 1; i < 3; ++i) {
      values3[i] = min_value;
    }
    if ((values3[0] <= values3[1]) && (values3[0] <= values3[2])) {
      middle = min(values3[1], values3[2]);
    } else {
      if ((values3[1] <= values3[0]) && (values3[1] <= values3[2])) {
        middle = min(values3[0], values3[2]);
      } else {
        middle = min(values3[0], values3[1]);
      }
    }
  } else {
    middle = min_value;
  }

  return middle;
}