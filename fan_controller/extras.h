#ifndef EXTRAS_FILE_INCLUDED
#define EXTRAS_FILE_INCLUDED

void save_settings(Settings settings);
void print_bits(byte bits, byte size);

// связанное с управлением PWM

byte get_max_percent_by_pwm();
char* set_max_percent_of_temp(Settings& settings, bool do_cmd_print, byte& callback_variable);
byte* get_pulses_array();
void read_and_exec_command(Settings settings);
char* get_pulses();
char* get_duties_settings();
char* get_pulses_settings(bool show_min);

#define get_out_pin(index) OUTPUTS_PINS[index][0]
#define get_rpm_pin(index) OUTPUTS_PINS[index][1]
#define get_cached_duty(index, percent) percent_2duty_cache[index][percent]
#define update_cached_duty(index, percent, duty) percent_2duty_cache[index][percent] = duty
#define apply_pwm_4all(percent)                            \
  for (byte __i__ = 0; __i__ < OUTPUTS_COUNT; ++__i__) {   \
    apply_fan_pwm(__i__, get_cached_duty(__i__, percent)); \
  }
#define convert_percent_2pulse(percent) map(percent, 0, 100, 0, PULSE_WIDTH)

char* set_max_percent_of_temp(Settings& settings, bool do_cmd_print, byte& callback_variable) {
  callback_variable = settings.min_temp;

  mString<3 * SENSORS_COUNT> values;
  for (byte __i__ = 0; __i__ < SENSORS_COUNT; ++__i__) {
    MicroDS18B20<> sensor(SENSORS_PINS[__i__], false);
    if (do_cmd_print) {
      uart.print("temp ");
      uart.print(sensor.get_pin());
      uart.print(": ");
    }
    if (__i__ != 0) {
      values.add(" ");
    }
    if (sensor.readTemp()) {
      byte temp = sensor.getTemp();
      if (do_cmd_print) {
        uart.println(temp);
      }
      if (temp < 10) {
        values.add(0);
      }
      values.add(temp);
      callback_variable = max(temp, callback_variable);
    } else {
      if (do_cmd_print) {
        uart.println("error");
      }
      values.add("--");
    }
    sensor.requestTemp();
  }
  callback_variable = convert_by_sqrt(callback_variable, settings.min_temp, settings.max_temp, 0, 100);

  return values.buf;
}

char* get_pulses_settings(bool show_min) {
  uart.print((show_min) ? GET_MIN_PULSES_COMMAND : GET_MAX_PULSES_COMMAND);
  uart.println(": ");
  mString<3 * INPUTS_COUNT> values;
  for (byte i = 0; i < INPUTS_COUNT; ++i) {
    uart.print(i);
    uart.print(" ");
    uart.println(((show_min) ? settings.min_pulses : settings.max_pulses)[i]);
    if (i != 0) {
      values.add(" ");
    }
    values.add(((show_min) ? settings.min_pulses : settings.max_pulses)[i]);
  }

  return values.buf;
}

char* get_duties_settings() {
  uart.print(GET_MIN_DUTIES_COMMAND);
  uart.println(": ");
  mString<4 * OUTPUTS_COUNT> values;
  for (byte i = 0; i < OUTPUTS_COUNT; ++i) {
    uart.print(i);
    uart.print(" ");
    uart.println(settings.min_duties[i]);
    if (i != 0) {
      values.add(" ");
    }
    values.add(settings.min_duties[i]);
  }

  return values.buf;
}

char* get_pulses() {
  uart.print(SHOW_PULSES_COMMAND);
  uart.println(": ");
  byte* pulses = get_pulses_array();
  mString<3 * INPUTS_COUNT> values;
  for (byte i = 0; i < INPUTS_COUNT; ++i) {
    uart.print(i);
    uart.print(" ");
    uart.println(pulses[i]);
    if (i != 0) {
      values.add(" ");
    }
    if (pulses[i] < 10) {
      values.add(0);
    }
    values.add(pulses[i]);
  }

  return values.buf;
}

byte* get_pulses_array() {
  byte array[INPUTS_COUNT];
  for (byte input_index = 0; input_index < INPUTS_COUNT; ++input_index) {
    byte buffer_on_read[BUFFER_SIZE_ON_READ];
    for (byte i = 0; i < BUFFER_SIZE_ON_READ; ++i) {
      buffer_on_read[i] = pulseIn(INPUTS_PINS[input_index], HIGH, (PULSE_WIDTH << 1));
      if ((buffer_on_read[i] == 0) && (digital_read_fast(INPUTS_PINS[input_index]) == HIGH)) {
        buffer_on_read[i] = PULSE_WIDTH;
      } else {
        buffer_on_read[i] = constrain(buffer_on_read[i], 0, PULSE_WIDTH);
      }
    }
    array[input_index] = find_median<BUFFER_SIZE_ON_READ>(buffer_on_read, PULSE_AVG_POWER, true);
  }

  return array;
}

void read_and_exec_command(Settings settings) {
  while (uart.available() > 0) {
    // чтение команды с серийного порта
    input_data.add((char)uart.read());
    recieved_flag = true;
    fixed_delay(20);
  }
  if (recieved_flag && str_length(input_data.buf) > 3) {
    if (input_data.startsWith(SHOW_PULSES_COMMAND)) {
      get_pulses();
    } else if (input_data.startsWith(READ_TEMPS_COMMAND)) {
      byte percent;
      set_max_percent_of_temp(settings, true, percent);
    } else if (input_data.startsWith(GET_MIN_TEMP_COMMAND) || input_data.startsWith(GET_MAX_TEMP_COMMAND)) {
      bool is_min_temp = input_data.startsWith(GET_MIN_TEMP_COMMAND);
      uart.print(input_data.buf);
      uart.print(": ");
      uart.println((is_min_temp) ? settings.min_temp : settings.max_temp);
    } else if (input_data.startsWith(GET_MIN_DUTIES_COMMAND)) {
      get_duties_settings();
    } else if (input_data.startsWith(SAVE_PARAMS_COMMAND)) {
      uart.print(SAVE_PARAMS_COMMAND);
      uart.print(": complete");
      save_settings(settings);
    } else if (input_data.startsWith(RESET_MIN_DUTIES_COMMAND)) {
      init_output_params(false, true);
    } else if (input_data.startsWith(SWITCH_DEBUG_COMMAND)) {
      is_debug = !is_debug;
      mString<16> msg;
      msg.add("Debug mode ");
      msg.add((is_debug) ? "ON" : "OFF");
      uart.println(msg.buf);
    } else if (input_data.startsWith(GET_MIN_PULSES_COMMAND) || input_data.startsWith(GET_MAX_PULSES_COMMAND)) {
      get_pulses_settings(input_data.startsWith(GET_MIN_PULSES_COMMAND));
    } else if ((input_data.startsWith(SET_MIN_TEMP_COMMAND)) || input_data.startsWith(SET_MAX_TEMP_COMMAND)) {
      bool is_max_temp = input_data.startsWith(SET_MAX_TEMP_COMMAND);
      uart.print((is_max_temp) ? SET_MAX_TEMP_COMMAND : SET_MIN_TEMP_COMMAND);
      uart.println(": ");
      bool complete = false;
      char* params[2];
      byte split_count = input_data.split(params, ' ');
      byte temp_value;
      if (split_count >= 2) {
        mString<8> param;
        param.add(params[1]);
        temp_value = param.toInt();
        if (is_max_temp) {
          if ((settings.min_temp < temp_value) && (temp_value < 80)) {
            settings.max_temp = temp_value;
            complete = true;
          }
        } else if ((10 < temp_value) && (temp_value < settings.max_temp)) {
          settings.min_temp = temp_value;
          complete = true;
        }
      }
      mString<32> msg;
      if (complete) {
        msg.add("Temp value ");
        msg.add(temp_value);
      } else {
        msg.add("error");
      }
      uart.println(msg.buf);
    } else if (input_data.startsWith(SET_MIN_DUTY_COMMAND)) {
      uart.print(SET_MIN_DUTY_COMMAND);
      uart.println(": ");
      bool complete = false;
      char* params[3];
      byte split_count = input_data.split(params, ' ');
      byte output_index;
      byte duty_value;
      if (split_count >= 3) {
        mString<8> param;
        param.add(params[1]);
        output_index = param.toInt();
        param.clear();
        param.add(params[2]);
        duty_value = param.toInt();
        if ((output_index < OUTPUTS_COUNT) && (MIN_DUTY <= duty_value) && (duty_value <= MAX_DUTY)) {
          settings.min_duties[output_index] = duty_value;
          complete = true;
          init_output_params(false, false);

          uart.print("Output ");
          uart.print(output_index);
          uart.print(" value ");
          uart.println(duty_value);
        }
      }
      if (!complete) {
        uart.println("error");
      }
    } else if (input_data.startsWith(SET_MAX_PULSE_COMMAND) || (input_data.startsWith(SET_MIN_PULSE_COMMAND))) {
      bool is_max_pulse = input_data.startsWith(SET_MAX_PULSE_COMMAND);
      uart.print(input_data.buf);
      uart.println(": ");
      bool complete = false;
      char* params[3];
      byte split_count = input_data.split(params, ' ');
      byte input_index;
      byte pulse_value;
      if (split_count >= 3) {
        mString<8> param;
        param.add(params[1]);
        input_index = param.toInt();
        param.clear();
        param.add(params[2]);
        pulse_value = param.toInt();
        if (input_index < INPUTS_COUNT) {
          if (is_max_pulse) {
            if ((settings.min_pulses[input_index] < pulse_value) && (pulse_value <= PULSE_WIDTH)) {
              settings.max_pulses[input_index] = pulse_value;
              complete = true;
            }
          } else {
            if ((0 <= pulse_value) && (pulse_value < settings.max_pulses[input_index])) {
              settings.min_pulses[input_index] = pulse_value;
              complete = true;
            }
          }
        }
      }
      if (complete) {
        uart.print("Input ");
        uart.print(input_index);
        uart.print(" value ");
        uart.println(pulse_value);
      } else {
        uart.println("error");
      }
    } else {
      uart.print("Unexpected command: ");
      uart.println(input_data.buf);
    }

    input_data.clear();
    recieved_flag = false;
  }
}

void save_settings(Settings settings) {
  Settings saved_sets;
  EEPROM.get(0, saved_sets);
  bool changed = saved_sets.max_temp != settings.max_temp || saved_sets.min_temp != settings.min_temp;
  for (byte i = 0; i < OUTPUTS_COUNT && !changed; ++i) {
    if (saved_sets.min_duties[i] != settings.min_duties[i]) {
      changed = true;
    }
  }
  for (byte i = 0; i < INPUTS_COUNT && !changed; ++i) {
    if (saved_sets.min_pulses[i] != settings.min_pulses[i] || saved_sets.max_pulses[i] != settings.max_pulses[i]) {
      changed = true;
    }
  }

  if (changed) {
    EEPROM.put(0, settings);
  }
}

byte get_max_percent_by_pwm() {
  byte percent = 0;
  byte* pulses = get_pulses_array();
  for (byte input_index = 0; input_index < INPUTS_COUNT; ++input_index) {
    smooth_buffer[input_index][smooth_index] = pulses[input_index];
    byte smooth_pulse = find_median<BUFFER_SIZE_FOR_SMOOTH>(smooth_buffer[input_index], true);
    byte pulse = constrain(smooth_pulse, settings.min_pulses[input_index], settings.max_pulses[input_index]);
    byte pulse_2percent = map(
        pulse,
        settings.min_pulses[input_index], settings.max_pulses[input_index],
        0, 100);
    percent = max(percent, pulse_2percent);

    if (is_debug) {
      uart.print("input ");
      uart.print(INPUTS_PINS[input_index]);
      uart.print(": pulse ");
      uart.print(pulse);
      uart.print(" (");
      uart.print(pulse_2percent);
      uart.print("%); avg smooth ");
      uart.print(smooth_pulse);
      uart.print(" [");
      for (byte i = 0; i < BUFFER_SIZE_FOR_SMOOTH; ++i) {
        if (i != 0) {
          uart.print(", ");
        }
        if (i == smooth_index) {
          uart.print("{");
        }
        uart.print(smooth_buffer[input_index][i]);
        if (i == smooth_index) {
          uart.print("}");
        }
      }
      uart.println("]");
    }
  }
  if (++smooth_index >= BUFFER_SIZE_FOR_SMOOTH) {
    smooth_index = 0;
  }

  return percent;
}

void print_bits(byte bits, byte size) {
  for (byte i = 0; i < size; ++i) {
    uart.print((bitRead(i, i)) ? 1 : 0);
  }
}

#endif
