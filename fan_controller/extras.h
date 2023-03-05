#ifndef EXTRAS_FILE_INCLUDED
#define EXTRAS_FILE_INCLUDED

void save_settings(Settings settings);
void print_bits(byte bits, byte size);

// связанное с управлением PWM

void read_temps(Settings settings, InputsInfo& inputs_info, bool do_cmd_print = false);
void read_pulses(InputsInfo& inputs_info, bool do_cmd_print = false);
void read_and_exec_command(Settings settings, InputsInfo& inputs_info, mString<64>& cmd_data, bool& is_debug);
mString<4 * OUTPUTS_COUNT> get_duties_settings();
mString<3 * INPUTS_COUNT> get_pulses_settings(bool show_min);

#define get_out_pin(index) OUTPUTS_PINS[index][0]
#define get_rpm_pin(index) OUTPUTS_PINS[index][1]
#define get_cached_duty(index, percent) percent_2duty_cache[index][percent]
#define update_cached_duty(index, percent, duty) percent_2duty_cache[index][percent] = duty
#define apply_pwm_4all(percent)                            \
  for (byte __i__ = 0; __i__ < OUTPUTS_COUNT; ++__i__) {   \
    apply_fan_pwm(__i__, get_cached_duty(__i__, percent)); \
  }
#define convert_percent_2pulse(percent) map(percent, 0, 100, 0, PULSE_WIDTH)

// реализация

void read_temps(Settings settings, InputsInfo& inputs_info, bool do_cmd_print = false) {
  inputs_info.pwm_percent_by_sensor = settings.min_temp;
  inputs_info.str_sensors_values.clear();
  for (byte i = 0; i < SENSORS_COUNT; ++i) {
    MicroDS18B20<> sensor(SENSORS_PINS[i], false);
    if (do_cmd_print) {
      uart.print(F("temp "));
      uart.print(sensor.get_pin());
      uart.print(": ");
    }
    if (i != 0) {
      inputs_info.str_sensors_values.add(" ");
    }
    if (sensor.readTemp()) {
      inputs_info.sensors_values[i] = sensor.getTemp();
      if (do_cmd_print) {
        uart.println(inputs_info.sensors_values[i]);
      }
      if (inputs_info.sensors_values[i] < 10) {
        inputs_info.str_sensors_values.add(0);
      }
      inputs_info.str_sensors_values.add(inputs_info.sensors_values[i]);
      inputs_info.pwm_percent_by_sensor = max(inputs_info.sensors_values[i], inputs_info.pwm_percent_by_sensor);
    } else {
      if (do_cmd_print) {
        uart.println("error");
      }
      inputs_info.str_sensors_values.add("--");
    }
    sensor.requestTemp();
  }

  inputs_info.pwm_percent_by_sensor = convert_by_sqrt(inputs_info.pwm_percent_by_sensor, settings.min_temp, settings.max_temp, 0, 100);
}

mString<3 * INPUTS_COUNT> get_pulses_settings(bool is_min) {
  mString<3 * INPUTS_COUNT> values;
  for (byte i = 0; i < INPUTS_COUNT; ++i) {
    if (i != 0) {
      values.add(" ");
    }
    values.add(((is_min) ? settings.min_pulses : settings.max_pulses)[i]);
  }

  return values;
}

mString<4 * OUTPUTS_COUNT> get_duties_settings() {
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

  return values;
}

void read_pulses(InputsInfo& inputs_info, bool do_cmd_print = false) {
  inputs_info.str_pulses_values.clear();
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
    inputs_info.pulses_info[input_index].value = find_median<BUFFER_SIZE_ON_READ, byte>(buffer_on_read, PULSE_AVG_POWER, true);

    if (input_index != 0) {
      inputs_info.str_pulses_values.add(" ");
    }
    if (inputs_info.pulses_info[input_index].value < 10) {
      inputs_info.str_pulses_values.add(0);
    }
    inputs_info.str_pulses_values.add(inputs_info.pulses_info[input_index].value);
  }
  if (do_cmd_print) {
    uart.println(inputs_info.str_pulses_values.buf);
  }

  inputs_info.pwm_percent_by_sensor = 0;
  for (byte input_index = 0; input_index < INPUTS_COUNT; ++input_index) {
    byte smooth_pulse = find_median<BUFFER_SIZE_FOR_SMOOTH, byte>(inputs_info.pulses_info[input_index].smooths_buffer, true);
    byte pulse = constrain(smooth_pulse, settings.min_pulses[input_index], settings.max_pulses[input_index]);
    byte pulse_2percent = map(
        pulse,
        settings.min_pulses[input_index], settings.max_pulses[input_index],
        0, 100);
    inputs_info.pwm_percent_by_sensor = max(inputs_info.pwm_percent_by_sensor, pulse_2percent);

    if (do_cmd_print) {
      uart.print(F("input "));
      uart.print(INPUTS_PINS[input_index]);
      uart.print(F(": pulse "));
      uart.print(pulse);
      uart.print(F(" ("));
      uart.print(pulse_2percent);
      uart.print(F("%); avg smooth "));
      uart.print(smooth_pulse);
      uart.print(F(" ["));
      for (byte i = 0; i < BUFFER_SIZE_FOR_SMOOTH; ++i) {
        if (i != 0) {
          uart.print(F(", "));
        }
        if (i == inputs_info.smooth_index) {
          uart.print(F("{"));
        }
        uart.print(inputs_info.pulses_info[input_index].smooths_buffer[i]);
        if (i == inputs_info.smooth_index) {
          uart.print(F("}"));
        }
      }
      uart.println(F("]"));
    }
  }
  if (++inputs_info.smooth_index >= BUFFER_SIZE_FOR_SMOOTH) {
    inputs_info.smooth_index = 0;
  }
}

void read_and_exec_command(Settings settings, InputsInfo& inputs_info, mString<64>& cmd_data, bool& is_debug) {
  while (uart.available() > 0) {
    // чтение команды с серийного порта
    cmd_data.add((char)uart.read());
    recieved_flag = true;
    fixed_delay(20);
  }
  if (recieved_flag && str_length(cmd_data.buf) > 3) {
    if (cmd_data.startsWith(SHOW_PULSES_COMMAND)) {
      uart.print(SHOW_PULSES_COMMAND);
      uart.print(": ");
      uart.println(inputs_info.str_pulses_values.buf);
    } else if (cmd_data.startsWith(SHOW_TEMPS_COMMAND)) {
      uart.print(SHOW_TEMPS_COMMAND);
      uart.print(": ");
      uart.println(inputs_info.str_pulses_values.buf);
    } else if (cmd_data.startsWith(GET_MIN_TEMP_COMMAND) || cmd_data.startsWith(GET_MAX_TEMP_COMMAND)) {
      bool is_min_temp = cmd_data.startsWith(GET_MIN_TEMP_COMMAND);
      uart.print((is_min_temp) ? GET_MIN_TEMP_COMMAND : GET_MAX_TEMP_COMMAND);
      uart.print(": ");
      uart.println((is_min_temp) ? settings.min_temp : settings.max_temp);
    } else if (cmd_data.startsWith(GET_MIN_DUTIES_COMMAND)) {
      uart.print(GET_MIN_DUTIES_COMMAND);
      uart.print(": ");
      mString<4 * OUTPUTS_COUNT> values = get_duties_settings();
      uart.println(values.buf);
    } else if (cmd_data.startsWith(SAVE_PARAMS_COMMAND)) {
      uart.println(SAVE_PARAMS_COMMAND);
      save_settings(settings);
    } else if (cmd_data.startsWith(RESET_MIN_DUTIES_COMMAND)) {
      uart.println(RESET_MIN_DUTIES_COMMAND);
      init_output_params(false, true);
    } else if (cmd_data.startsWith(SWITCH_DEBUG_COMMAND)) {
      is_debug = !is_debug;
      mString<16> msg;
      msg.add("Debug mode ");
      msg.add((is_debug) ? "ON" : "OFF");
      uart.println(msg.buf);
    } else if (cmd_data.startsWith(GET_MIN_PULSES_COMMAND) || cmd_data.startsWith(GET_MAX_PULSES_COMMAND)) {
      bool is_min_pulse = cmd_data.startsWith(GET_MIN_PULSES_COMMAND);
      mString<3 * INPUTS_COUNT> values = get_pulses_settings(is_min_pulse);
      uart.print((is_min_pulse) ? GET_MIN_PULSES_COMMAND : GET_MAX_PULSES_COMMAND);
      uart.print(": ");
      uart.println(values.buf);
    } else if ((cmd_data.startsWith(SET_MIN_TEMP_COMMAND)) || cmd_data.startsWith(SET_MAX_TEMP_COMMAND)) {
      bool is_max_temp = cmd_data.startsWith(SET_MAX_TEMP_COMMAND);
      uart.print((is_max_temp) ? SET_MAX_TEMP_COMMAND : SET_MIN_TEMP_COMMAND);
      uart.println(": ");
      bool complete = false;
      char* params[2];
      byte split_count = cmd_data.split(params, ' ');
      byte temp_value;
      if (split_count >= 2) {
        mString<8> param;
        param.add(params[1]);
        temp_value = param.toInt();
        if (is_max_temp) {
          if ((settings.min_temp < temp_value) && (temp_value <= MAX_TEMP_VALUE)) {
            settings.max_temp = temp_value;
            complete = true;
          }
        } else if ((MIN_TEMP_VALUE <= temp_value) && (temp_value < settings.max_temp)) {
          settings.min_temp = temp_value;
          complete = true;
        }
      }
      if (complete) {
        uart.print(F("Temp value "));
        uart.println(temp_value);
      } else {
        uart.println("error");
      }
    } else if (cmd_data.startsWith(SET_MIN_DUTY_COMMAND)) {
      uart.print(SET_MIN_DUTY_COMMAND);
      uart.println(": ");
      bool complete = false;
      char* params[3];
      byte split_count = cmd_data.split(params, ' ');
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

          uart.print(F("Output "));
          uart.print(output_index);
          uart.print(" value ");
          uart.println(duty_value);
        }
      }
      if (!complete) {
        uart.println("error");
      }
    } else if (cmd_data.startsWith(SET_MAX_PULSE_COMMAND) || (cmd_data.startsWith(SET_MIN_PULSE_COMMAND))) {
      bool is_max_pulse = cmd_data.startsWith(SET_MAX_PULSE_COMMAND);
      uart.print((is_max_pulse) ? SET_MAX_PULSE_COMMAND : SET_MIN_PULSE_COMMAND);
      uart.println(": ");
      bool complete = false;
      char* params[3];
      byte split_count = cmd_data.split(params, ' ');
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
        uart.print(F("Input "));
        uart.print(input_index);
        uart.print(" value ");
        uart.println(pulse_value);
      } else {
        uart.println("error");
      }
    } else {
      uart.print(F("Unexpected command: "));
      uart.println(cmd_data.buf);
    }

    cmd_data.clear();
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
    uart.print(F("Settings save: "));
    EEPROM.put(0, settings);
    uart.println(F("complete"));
  } else {
    uart.println(F("Settings not changed = dont save it"));
  }
}

void print_bits(byte bits, byte size) {
  for (byte i = 0; i < size; ++i) {
    uart.print((bitRead(i, i)) ? 1 : 0);
  }
}

#endif
