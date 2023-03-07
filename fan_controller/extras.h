#ifndef EXTRAS_FILE_INCLUDED
#define EXTRAS_FILE_INCLUDED

void save_settings(Settings settings, Max7219Matrix& mtrx);
void print_bits(byte bits, byte size);

// связанное с управлением PWM

void read_temps(Settings settings, InputsInfo& inputs_info, bool do_cmd_print = false);
void read_pulses(InputsInfo& inputs_info, bool do_cmd_print = false);
void read_and_exec_command(Settings settings, InputsInfo& inputs_info, mString<64>& cmd_data, bool& is_debug, Max7219Matrix& mtrx);
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

// связанное с LED матрицей

#define MTRX_PIXELS_IN_ROW (MTRX_PIXELS_IN_COLUMN * MTRX_COLUMS_COUNT)  // количество пикселей в ряд

void mtrx_print(Max7219Matrix& mtrx, char* data, int cursor_h = 0, int cursor_v = 0);
void mtrx_refresh(Max7219Matrix& mtrx, uint32_t time = -1);
void clear_mtrx(Max7219Matrix& mtrx);
void add_matrix_text(Max7219Matrix& mtrx, char* chars);
void add_matrix_text_n_space_before(Max7219Matrix& mtrx, char* chars, bool add_space);
void set_matrix_text(Max7219Matrix& mtrx, char* chars);
void mtrx_slide_v(Max7219Matrix& mtrx, char* new_item_chars, bool top_dir);
void mtrx_slide_h(Max7219Matrix& mtrx, char* new_item_chars, char transition, bool is_letf);
void replace_mtrx_text(Max7219Matrix& mtrx, char* caption);
int typewriter_slide_set_text(Max7219Matrix& mtrx, char* text, int cursor = 0, bool is_first = false);
byte text_to_pixels_in_row(char* source);
void init_matrix(Max7219Matrix& mtrx);

#define mtrx_slide_up(mtrx, new_item) mtrx_slide_v(mtrx, new_item, true)
#define mtrx_slide_down(mtrx, new_item) mtrx_slide_v(mtrx, new_item, false)
#define mtrx_slide_left(mtrx, new_item_chars, transition) mtrx_slide_h(mtrx, new_item_chars, transition, true)
#define mtrx_slide_right(mtrx, new_item_chars, transition) mtrx_slide_h(mtrx, new_item_chars, transition, false)

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

void read_and_exec_command(Settings settings, InputsInfo& inputs_info, mString<64>& cmd_data, bool& is_debug, Max7219Matrix& mtrx) {
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
      set_matrix_text(mtrx, inputs_info.str_sensors_values.buf);
    } else if (cmd_data.startsWith(GET_MIN_TEMP_COMMAND) || cmd_data.startsWith(GET_MAX_TEMP_COMMAND)) {
      bool is_min_temp = cmd_data.startsWith(GET_MIN_TEMP_COMMAND);
      uart.print((is_min_temp) ? GET_MIN_TEMP_COMMAND : GET_MAX_TEMP_COMMAND);
      uart.print(": ");
      uart.println((is_min_temp) ? settings.min_temp : settings.max_temp);
      set_matrix_text(mtrx, "T");
      add_matrix_text(mtrx, (is_min_temp) ? "v" : "^");
      add_matrix_text_n_space_before(mtrx, (is_min_temp) ? settings.min_temp : settings.max_temp, true);
    } else if (cmd_data.startsWith(GET_MIN_DUTIES_COMMAND)) {
      uart.print(GET_MIN_DUTIES_COMMAND);
      uart.print(": ");
      mString<4 * OUTPUTS_COUNT> values = get_duties_settings();
      uart.println(values.buf);
      set_matrix_text(mtrx, values.buf);
    } else if (cmd_data.startsWith(SAVE_PARAMS_COMMAND)) {
      uart.println(SAVE_PARAMS_COMMAND);
      save_settings(settings, mtrx);
    } else if (cmd_data.startsWith(RESET_MIN_DUTIES_COMMAND)) {
      uart.println(RESET_MIN_DUTIES_COMMAND);
      init_output_params(false, true, mtrx);
    } else if (cmd_data.startsWith(SWITCH_DEBUG_COMMAND)) {
      is_debug = !is_debug;
      mString<16> msg;
      msg.add((is_debug) ? F("Debug mode ON") : F("Debug mode OFF"));
      uart.println(msg.buf);
      set_matrix_text(mtrx, msg.buf);
    } else if (cmd_data.startsWith(GET_MIN_PULSES_COMMAND) || cmd_data.startsWith(GET_MAX_PULSES_COMMAND)) {
      bool is_min_pulse = cmd_data.startsWith(GET_MIN_PULSES_COMMAND);
      mString<3 * INPUTS_COUNT> values = get_pulses_settings(is_min_pulse);
      uart.print((is_min_pulse) ? GET_MIN_PULSES_COMMAND : GET_MAX_PULSES_COMMAND);
      uart.print(": ");
      uart.println(values.buf);
      set_matrix_text(mtrx, values.buf);
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
        set_matrix_text(mtrx, "T");
        add_matrix_text(mtrx, (is_max_temp) ? "^" : "v");
        add_matrix_text_n_space_before(mtrx, temp_value, true);
      } else {
        set_matrix_text(mtrx, "error");
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
          init_output_params(false, false, mtrx);

          uart.print(F("Output "));
          uart.print(output_index);
          uart.print(" value ");
          uart.println(duty_value);

          set_matrix_text(mtrx, 'O');
          add_matrix_text(mtrx, output_index);
          add_matrix_text_n_space_before(mtrx, duty_value, true);
        }
      }
      if (!complete) {
        uart.println("error");
        set_matrix_text(mtrx, "error");
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
        set_matrix_text(mtrx, 'I');
        add_matrix_text(mtrx, input_index);
        add_matrix_text(mtrx, (is_max_pulse) ? "^" : "v");
        add_matrix_text_n_space_before(mtrx, pulse_value, true);
      } else {
        set_matrix_text(mtrx, "error");
        uart.println("error");
      }
    } else {
      uart.print(F("Unexpected command: "));
      uart.println(cmd_data.buf);
      set_matrix_text(mtrx, "Wrong");
      fixed_delay(1500);
      mtrx_slide_down(mtrx, "");
    }

    cmd_data.clear();
    recieved_flag = false;
  }
}

void save_settings(Settings settings, Max7219Matrix& mtrx) {
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

  mString<MTRX_BUFFER> menu_caption = mtrx.data;
  if (changed) {
    mtrx_slide_left(mtrx, "save...", '>');
    for (byte i = 0; i < (2112 / MTRX_REFRESH_MS); ++i) {
      for (uint32_t tmr_start = millis(), tmr = millis(); !check_diff(tmr, tmr_start, MTRX_REFRESH_MS); tmr = millis()) {
      }
      mtrx_refresh(mtrx);
    }
    uart.print(F("Settings save: "));
    EEPROM.put(0, settings);
    uart.println(F("complete"));
    mtrx_slide_right(mtrx, menu_caption.buf, '>');
  } else {
    uart.println(F("Settings not changed = dont save it"));
    mtrx_slide_left(mtrx, "saved", '>');
    fixed_delay(1500);
    mtrx_slide_right(mtrx, menu_caption.buf, '>');
  }
}

void print_bits(byte bits, byte size) {
  for (byte i = 0; i < size; ++i) {
    uart.print((bitRead(i, i)) ? 1 : 0);
  }
}

void mtrx_print(Max7219Matrix& mtrx, char* data, int cursor_h = 0, int cursor_v = 0) {
  mString<MTRX_BUFFER> data_4split;
  data_4split.add(data);
  char* chars[MTRX_BUFFER];
  byte space_count = data_4split.split(chars, ' ');

  mString<MTRX_BUFFER> str;
  byte shift = 0;
  int position = 0;
  for (byte i = 0; i < space_count; ++i) {
    position = MTRX_INDENT + cursor_h + shift;
    mtrx.panel.setCursor(position, 1 + cursor_v);
    if (i != 0) {
      mtrx.panel.rect(position, 0, position - MTRX_PIXELS_IN_SPACE_BY_ROW, MTRX_PIXELS_IN_COLUMN - 1, GFX_CLEAR);
    }
    str.clear();
    str.add(chars[i]);
    mtrx.panel.print(str.buf);
    shift += str_length(str.buf) * MTRX_PIXELS_IN_CHAR_BY_ROW + MTRX_PIXELS_IN_SPACE_BY_ROW;
  }
}

void mtrx_refresh(Max7219Matrix& mtrx, uint32_t time = -1) {
  bool need_refresh = false;
  if (time != -1) {
    if (check_diff(time, mtrx.time, MTRX_REFRESH_MS)) {
      mtrx.time = time;
      need_refresh = true;
    }
  } else {
    need_refresh = true;
  }

  if (need_refresh) {
    byte border_position = text_to_pixels_in_row(mtrx.data.buf) - 1;
    if (border_position > MTRX_PIXELS_IN_ROW) {
      mtrx.panel.clear();
      if (mtrx.changed) {
        init_matrix(mtrx);
        mtrx.next_cursor = 1;
        mtrx.border_cursor = border_position - MTRX_PIXELS_IN_ROW;
      } else {
        if ((mtrx.cursor == 0) || (mtrx.cursor == mtrx.border_cursor)) {
          if (mtrx.border_delay_counter == MTRX_SLIDING_DELAY_TACTS) {
            mtrx.border_delay_counter = 0;
            if (mtrx.cursor == 0) {
              mtrx.next_cursor = 1;
            }
          } else {
            ++mtrx.border_delay_counter;
          }
        }
        if (mtrx.border_delay_counter == 0) {
          if ((mtrx.cursor < mtrx.next_cursor) && (mtrx.next_cursor != mtrx.border_cursor)) {
            mtrx.cursor = mtrx.next_cursor;
            ++mtrx.next_cursor;
          } else {
            mtrx.cursor = mtrx.next_cursor;
            --mtrx.next_cursor;
          }
        }
      }

      mtrx_print(mtrx, mtrx.data.buf, -mtrx.cursor);

      mtrx.panel.update();
    } else if (mtrx.changed) {
      init_matrix(mtrx);
      mtrx.panel.clear();
      mtrx_print(mtrx, mtrx.data.buf);
      mtrx.panel.update();
    }
  }
}

void init_matrix(Max7219Matrix& mtrx) {
  mtrx.changed = false;
  mtrx.cursor = 0;
  mtrx.next_cursor = 1;
  mtrx.border_delay_counter = 0;
  mtrx.border_cursor = 0;
}

void clear_mtrx(Max7219Matrix& mtrx) {
  mtrx.data.clear();
  mtrx.changed = true;
}

void add_matrix_text(Max7219Matrix& mtrx, char* chars) {
  mtrx.changed = true;
  mtrx.data.add(chars);
}

void add_matrix_text_n_space_before(Max7219Matrix& mtrx, char* chars, bool add_space) {
  if (add_space) {
    mtrx.data.add(' ');
  }
  add_matrix_text(mtrx, chars);
}

void set_matrix_text(Max7219Matrix& mtrx, char* chars) {
  clear_mtrx(mtrx);
  add_matrix_text(mtrx, chars);
}

void mtrx_slide_v(Max7219Matrix& mtrx, char* new_item_chars, bool top_dir) {
  for (byte i = 1; i <= MTRX_PIXELS_IN_COLUMN; ++i) {
    fixed_delay(MTRX_SLIDE_DELAY);
    mtrx.panel.clear();
    mtrx_print(mtrx, mtrx.data.buf, 0, (top_dir) ? -i : i);
    mtrx_print(mtrx, new_item_chars, 0, (top_dir) ? 1 + MTRX_PIXELS_IN_COLUMN - i : -1 - MTRX_PIXELS_IN_COLUMN + i);
    mtrx.panel.update();
  };
  set_matrix_text(mtrx, new_item_chars);
  fixed_delay(MTRX_SLIDE_DELAY);
  mtrx_refresh(mtrx);
}

void mtrx_slide_h(Max7219Matrix& mtrx, char* new_item_chars, char transition, bool is_letf) {
  for (byte i = 1; i < MTRX_PIXELS_IN_ROW + MTRX_PIXELS_IN_CHAR_BY_ROW; ++i) {
    fixed_delay(MTRX_SLIDE_DELAY_HORIZONTAL);
    mtrx.panel.clear();
    int new_item_cursor = i - MTRX_PIXELS_IN_CHAR_BY_ROW - MTRX_PIXELS_IN_ROW;
    int old_item_cursor = i;
    int rect_cursor;
    if (is_letf) {
      old_item_cursor *= -1;
      new_item_cursor *= -1;
      rect_cursor = new_item_cursor;

      mtrx_print(mtrx, mtrx.data.buf, old_item_cursor);
      mtrx.panel.rect(rect_cursor - MTRX_PIXELS_IN_CHAR_BY_ROW - (MTRX_INDENT << 1), 0, MTRX_PIXELS_IN_ROW - 1, MTRX_PIXELS_IN_COLUMN - 1, GFX_CLEAR);
      mtrx_print(mtrx, new_item_chars, new_item_cursor);
    } else {
      rect_cursor = old_item_cursor;

      mtrx_print(mtrx, new_item_chars, new_item_cursor);
      mtrx.panel.rect(rect_cursor - MTRX_PIXELS_IN_CHAR_BY_ROW - (MTRX_INDENT << 1), 0, MTRX_PIXELS_IN_ROW - 1, MTRX_PIXELS_IN_COLUMN - 1, GFX_CLEAR);
      mtrx_print(mtrx, mtrx.data.buf, old_item_cursor);
    }

    mString<2> _transition;
    _transition.add(transition);
    mtrx_print(mtrx, _transition.buf, rect_cursor - MTRX_PIXELS_IN_CHAR_BY_ROW - (MTRX_INDENT << 1));

    mtrx.panel.update();
  };
  set_matrix_text(mtrx, new_item_chars);
  fixed_delay(MTRX_SLIDE_DELAY_HORIZONTAL);
  mtrx_refresh(mtrx);
}

void replace_mtrx_text(Max7219Matrix& mtrx, char* caption) {
  if ((str_length(mtrx.data.buf) != str_length(caption)) || (text_to_pixels_in_row(caption) <= MTRX_PIXELS_IN_ROW)) {
    mtrx.changed = true;
  }
  mtrx.data.clear();
  mtrx.data.add(caption);
}

int typewriter_slide_set_text(Max7219Matrix& mtrx, char* text, int cursor = 0, bool is_first = false) {
  if (is_first) {
    cursor = MTRX_PIXELS_IN_ROW - (MTRX_INDENT << 1);
  }
  mString<MTRX_BUFFER> str;
  str.add(mtrx.data.buf);
  str.add(text);
  uint32_t tmr_start;
  uint32_t tmr;
  int cursor_end;
  for (cursor_end = MTRX_PIXELS_IN_ROW - text_to_pixels_in_row(str.buf); cursor_end <= cursor; --cursor) {
    for (tmr_start = millis(), tmr = millis(); !check_diff(tmr, tmr_start, MTRX_REFRESH_MS >> 2); tmr = millis()) {
    }
    mtrx_print(mtrx, str.buf, cursor);
    mtrx.panel.update();
  }
  if (text_to_pixels_in_row(str.buf) > MTRX_PIXELS_IN_ROW) {
    char chars[MTRX_BUFFER];
    str.substring(str_length(str.buf) - (MTRX_PIXELS_IN_ROW / MTRX_PIXELS_IN_CHAR_BY_ROW) - 1, str_length(str.buf) - 1, chars);
    str.clear();
    str.add(chars);
  }
  cursor = MTRX_PIXELS_IN_ROW - text_to_pixels_in_row(str.buf);
  mtrx.data.clear();
  mtrx.data.add(str);
  mtrx.changed = true;

  return cursor;
}

byte text_to_pixels_in_row(char* source) {
  if (strlen(source) != 0) {
    char* chars[MTRX_BUFFER];
    return MTRX_INDENT + (str_length(source) * MTRX_PIXELS_IN_CHAR_BY_ROW) - (find_char_count(source, ' ') * (MTRX_PIXELS_IN_CHAR_BY_ROW - MTRX_PIXELS_IN_SPACE_BY_ROW));
  }

  return MTRX_INDENT;
}

#endif
