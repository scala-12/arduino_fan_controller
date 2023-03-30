#ifndef EXTRAS_FILE_INCLUDED
#define EXTRAS_FILE_INCLUDED

#include <Arduino.h>

#include "constants.h"

void save_settings(Settings& settings, Max7219Matrix& mtrx);
void print_bits(byte& bits, byte size);

// связанное с управлением PWM

void read_and_exec_command(Settings& settings, InputsInfo& inputs_info, Reciever& reciever, bool& is_debug, Max7219Matrix& mtrx);
mString<4 * OUTPUTS_COUNT> get_duties_settings();
mString<3 * INPUTS_COUNT> get_pulses_settings(bool show_min);

#define convert_percent_2pulse(percent) map(percent, 0, 100, 0, PULSE_WIDTH)

// связанное с LED матрицей

void mtrx_print(Max7219Matrix& mtrx, char* data, int8_t cursor_h = 0, int8_t cursor_v = 0);
void mtrx_refresh(Max7219Matrix& mtrx, bool do_check_timeout);
void clear_mtrx(Max7219Matrix& mtrx);
void add_matrix_text(Max7219Matrix& mtrx, char* chars);
void add_matrix_text_n_space_before(Max7219Matrix& mtrx, char* chars, bool add_space);
void set_matrix_text(Max7219Matrix& mtrx, char* chars);
void mtrx_slide_v(Max7219Matrix& mtrx, char* new_item_chars, bool top_dir);
void mtrx_slide_h(Max7219Matrix& mtrx, char* new_item_chars, char transition, bool is_letf);
void replace_mtrx_text(Max7219Matrix& mtrx, char* caption);
int8_t typewriter_slide_set_text(Max7219Matrix& mtrx, char* text, int8_t cursor = 0, bool is_first = false);
byte text_to_pixels_in_row(char* source);
void init_matrix(Max7219Matrix& mtrx);

#define mtrx_slide_up(mtrx, new_item) mtrx_slide_v(mtrx, new_item, true)
#define mtrx_slide_down(mtrx, new_item) mtrx_slide_v(mtrx, new_item, false)
#define mtrx_slide_left(mtrx, new_item_chars, transition) mtrx_slide_h(mtrx, new_item_chars, transition, true)
#define mtrx_slide_right(mtrx, new_item_chars, transition) mtrx_slide_h(mtrx, new_item_chars, transition, false)

// связанное с меню

void close_menu(Max7219Matrix& mtrx, Menu& menu);
void select_horizontal_menu(Menu& menu, byte& next_item_index);
void select_vertical_menu(Menu& menu, byte index);
void open_menu(Menu& menu);
void back_menu(Menu& menu);
void menu_tick(Settings& settings, byte buttons_state[CTRL_KEYS_COUNT], Menu& menu, Max7219Matrix& mtrx);
void menu_refresh(Settings& settings, InputsInfo& inputs_info, uint32_t time, Max7219Matrix& mtrx, Menu& menu);

// реализация

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
    uart_print(i);
    uart_print(" ");
    uart_println(settings.min_duties[i]);
    if (i != 0) {
      values.add(" ");
    }
    values.add(settings.min_duties[i]);
  }

  return values;
}

void read_and_exec_command(Settings& settings, InputsInfo& inputs_info, Reciever& reciever, bool& is_debug, Max7219Matrix& mtrx) {
#ifndef DONT_USE_UART
  while (uart.available() > 0) {
    // чтение команды с серийного порта
    reciever.data.add((char)uart.read());
    reciever.flag = true;
    fixed_delay(20);
  }
  if (reciever.flag && str_length(reciever.data.buf) > 3) {
    if (mtrx.data.indexOf("error") == 0) {
      mtrx_slide_down(mtrx, "");
    }
    if (reciever.data.startsWith(SHOW_PULSES_COMMAND)) {
      uart_print(SHOW_PULSES_COMMAND);
      uart_print(": ");
      uart_println(inputs_info.str_pulses_values.buf);
    } else if (reciever.data.startsWith(SHOW_TEMP_COMMAND)) {
      uart_print(SHOW_TEMP_COMMAND);
      uart_print(": ");
      mString<8> temp_value;
      if (inputs_info.temperature.available) {
        temp_value.add(inputs_info.temperature.value);
      } else {
        temp_value.add("--");
      }
      uart_println(temp_value.buf);
      set_matrix_text(mtrx, temp_value.buf);
    } else if (reciever.data.startsWith(SHOW_MIN_DUTY_PERCENT_COMMAND)) {
      uart_print(SHOW_MIN_DUTY_PERCENT_COMMAND);
      uart_print(": ");
      uart_println(settings.min_duty_percent);
    } else if (reciever.data.startsWith(SHOW_OPTICAL_COUNTER_COMMAND)) {
      uart_print(SHOW_OPTICAL_COUNTER_COMMAND);
      uart_print(": ");
      uart_println(inputs_info.optical.rpm);
      set_matrix_text(mtrx, inputs_info.optical.rpm);
    } else if (reciever.data.startsWith(GET_MIN_TEMP_COMMAND) || reciever.data.startsWith(GET_MAX_TEMP_COMMAND) || reciever.data.startsWith(GET_MIN_OPTICAL_COMMAND) || reciever.data.startsWith(GET_MAX_OPTICAL_COMMAND)) {
      bool is_min = reciever.data.startsWith(GET_MIN_TEMP_COMMAND) || reciever.data.startsWith(GET_MIN_OPTICAL_COMMAND);
      bool is_temp = reciever.data.startsWith(GET_MIN_TEMP_COMMAND) || reciever.data.startsWith(GET_MAX_TEMP_COMMAND);
      uart_print((is_min) ? ((is_temp) ? GET_MIN_TEMP_COMMAND : GET_MIN_OPTICAL_COMMAND) : ((is_temp) ? GET_MAX_TEMP_COMMAND : GET_MAX_OPTICAL_COMMAND));
      uart_print(": ");
      uart_println((is_min) ? settings.min_temp : settings.max_temp);
      set_matrix_text(mtrx, (is_temp) ? "T" : "O");
      add_matrix_text(mtrx, (is_min) ? "v" : "^");
      add_matrix_text_n_space_before(mtrx, (is_min) ? ((is_temp) ? settings.min_temp : settings.min_optic_rpm) : ((is_temp) ? settings.max_temp : settings.max_optic_rpm), true);
    } else if (reciever.data.startsWith(GET_MIN_DUTIES_COMMAND)) {
      uart_print(GET_MIN_DUTIES_COMMAND);
      uart_print(": ");
      mString<4 * OUTPUTS_COUNT> values = get_duties_settings();
      uart_println(values.buf);
      set_matrix_text(mtrx, values.buf);
    } else if (reciever.data.startsWith(SAVE_PARAMS_COMMAND)) {
      uart_println(SAVE_PARAMS_COMMAND);
      save_settings(settings, mtrx);
    } else if (reciever.data.startsWith(RESET_MIN_DUTIES_COMMAND)) {
      uart_println(RESET_MIN_DUTIES_COMMAND);
      init_output_params(false, true, mtrx);
    } else if (reciever.data.startsWith(SWITCH_DEBUG_COMMAND)) {
      is_debug = !is_debug;
      mString<16> msg;
      msg.add((is_debug) ? F("Debug mode ON") : F("Debug mode OFF"));
      uart_println(msg.buf);
      set_matrix_text(mtrx, msg.buf);
    } else if (reciever.data.startsWith(SWITCH_COOLING_HOLD_COMMAND)) {
      settings.cool_on_hold = !settings.cool_on_hold;
      mString<16> msg;
      msg.add((settings.cool_on_hold) ? F("Cooling on HOLD") : F("Ctrl on HOLD"));
      uart_println(msg.buf);
      set_matrix_text(mtrx, msg.buf);
    } else if (reciever.data.startsWith(GET_MIN_PULSES_COMMAND) || reciever.data.startsWith(GET_MAX_PULSES_COMMAND)) {
      bool is_min_pulse = reciever.data.startsWith(GET_MIN_PULSES_COMMAND);
      mString<3 * INPUTS_COUNT> values = get_pulses_settings(is_min_pulse);
      uart_print((is_min_pulse) ? GET_MIN_PULSES_COMMAND : GET_MAX_PULSES_COMMAND);
      uart_print(": ");
      uart_println(values.buf);
      set_matrix_text(mtrx, values.buf);
    } else if ((reciever.data.startsWith(SET_MIN_TEMP_COMMAND)) || reciever.data.startsWith(SET_MAX_TEMP_COMMAND) || (reciever.data.startsWith(SET_MIN_OPTICAL_COMMAND)) || reciever.data.startsWith(SET_MAX_OPTICAL_COMMAND)) {
      bool is_min = reciever.data.startsWith(SET_MIN_TEMP_COMMAND) || reciever.data.startsWith(SET_MIN_OPTICAL_COMMAND);
      bool is_temp = reciever.data.startsWith(SET_MIN_TEMP_COMMAND) || reciever.data.startsWith(SET_MAX_TEMP_COMMAND);

      uart_print((is_min) ? ((is_temp) ? SET_MIN_TEMP_COMMAND : SET_MIN_OPTICAL_COMMAND) : ((is_temp) ? SET_MAX_TEMP_COMMAND : SET_MAX_OPTICAL_COMMAND));
      uart_println(": ");
      bool complete = false;
      char* params[2];
      byte split_count = reciever.data.split(params, ' ');
      uint16_t value;
      if (split_count >= 2) {
        mString<8> param;
        param.add(params[1]);
        value = param.toInt();
        if (is_min) {
          if ((is_temp && value > MIN_TEMP_VALUE && value < settings.max_temp) || (!is_temp && value >= MIN_OPTIC_RPM_VALUE && value < settings.max_optic_rpm)) {
            if (is_temp) {
              settings.min_temp = value;
            } else {
              settings.min_optic_rpm = value;
            }
            complete = true;
          }
        } else if ((is_temp && settings.min_temp < value && value < MAX_TEMP_VALUE) || (!is_temp && value > settings.min_optic_rpm && value <= MAX_OPTIC_RPM_VALUE)) {
          if (is_temp) {
            settings.max_temp = value;
          } else {
            settings.max_optic_rpm = value;
          }
          complete = true;
        }
      }
      if (complete) {
        uart_print((is_temp) ? F("Temp value ") : F("Optical conter value "));
        uart_println(value);
        set_matrix_text(mtrx, (is_temp) ? "T" : "O");
        add_matrix_text(mtrx, (is_min) ? "v" : "^");
        add_matrix_text_n_space_before(mtrx, value, true);
      } else {
        set_matrix_text(mtrx, "error");
        uart_println("error");
      }
    } else if (reciever.data.startsWith(SET_MIN_DUTY_COMMAND)) {
      uart_print(SET_MIN_DUTY_COMMAND);
      uart_println(": ");
      bool complete = false;
      char* params[3];
      byte split_count = reciever.data.split(params, ' ');
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

          uart_print(F("Output "));
          uart_print(output_index);
          uart_print(" value ");
          uart_println(duty_value);

          set_matrix_text(mtrx, 'O');
          add_matrix_text(mtrx, output_index);
          add_matrix_text_n_space_before(mtrx, duty_value, true);
        }
      }
      if (!complete) {
        uart_println("error");
        set_matrix_text(mtrx, "error");
      }
    } else if (reciever.data.startsWith(SET_MIN_DUTY_PERCENT_COMMAND)) {
      uart_print(SET_MIN_DUTY_PERCENT_COMMAND);
      uart_println(": ");
      bool complete = false;
      char* params[2];
      byte split_count = reciever.data.split(params, ' ');
      byte percent_value;
      if (split_count >= 2) {
        mString<8> param;
        param.add(params[1]);
        percent_value = param.toInt();
        if (0 <= percent_value && percent_value <= 100) {
          settings.min_duty_percent = percent_value;
          complete = true;
          init_output_params(false, false, mtrx);

          uart_print(F("Min duty percent "));
          uart_println(percent_value);

          set_matrix_text(mtrx, "B");
          add_matrix_text_n_space_before(mtrx, percent_value, true);
        }
      }
      if (!complete) {
        uart_println("error");
        set_matrix_text(mtrx, "error");
      }
    } else if (reciever.data.startsWith(SET_MAX_PULSE_COMMAND) || (reciever.data.startsWith(SET_MIN_PULSE_COMMAND))) {
      bool is_max_pulse = reciever.data.startsWith(SET_MAX_PULSE_COMMAND);
      uart_print((is_max_pulse) ? SET_MAX_PULSE_COMMAND : SET_MIN_PULSE_COMMAND);
      uart_println(": ");
      bool complete = false;
      char* params[3];
      byte split_count = reciever.data.split(params, ' ');
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
        uart_print(F("Input "));
        uart_print(input_index);
        uart_print(" value ");
        uart_println(pulse_value);
        set_matrix_text(mtrx, 'I');
        add_matrix_text(mtrx, input_index);
        add_matrix_text(mtrx, (is_max_pulse) ? "^" : "v");
        add_matrix_text_n_space_before(mtrx, pulse_value, true);
      } else {
        set_matrix_text(mtrx, "error");
        uart_println("error");
      }
    } else {
      uart_print(F("Unexpected command: "));
      uart_println(reciever.data.buf);
      set_matrix_text(mtrx, "Wrong");
      fixed_delay(1500);
      mtrx_slide_down(mtrx, "");
    }

    reciever.data.clear();
    reciever.flag = false;
  }
#endif
}

void save_settings(Settings& settings, Max7219Matrix& mtrx) {
  Settings saved_sets;
  EEPROM.get(0, saved_sets);
  bool changed = saved_sets.max_temp != settings.max_temp || saved_sets.min_temp != settings.min_temp || saved_sets.cool_on_hold != settings.cool_on_hold || saved_sets.min_duty_percent != settings.min_duty_percent;
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
      fixed_delay(MTRX_REFRESH_MS);
      mtrx_refresh(mtrx, false);
    }
    uart_print(F("Settings save: "));
    EEPROM.put(0, settings);
    uart_println(F("complete"));
    mtrx_slide_right(mtrx, menu_caption.buf, '>');
  } else {
    uart_println(F("Settings not changed = dont save it"));
    mtrx_slide_left(mtrx, "saved", '>');
    fixed_delay(1500);
    mtrx_slide_right(mtrx, menu_caption.buf, '>');
  }
}

void print_bits(byte& bits, byte size) {
  for (byte i = 0; i < size; ++i) {
    uart_print((bitRead(i, i)) ? 1 : 0);
  }
}

void mtrx_print(Max7219Matrix& mtrx, char* data, int8_t cursor_h = 0, int8_t cursor_v = 0) {
  mString<MTRX_BUFFER> data_4split;
  data_4split.add(data);
  char* chars[MTRX_BUFFER];
  byte space_count = data_4split.split(chars, ' ');

  mString<MTRX_BUFFER> str;
  byte shift = 0;
  int8_t position = 0;
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

void mtrx_refresh(Max7219Matrix& mtrx, bool do_check_timeout) {
  bool need_refresh = false;
  if (do_check_timeout) {
    if (is_success_delay(mtrx.time, MTRX_REFRESH_MS)) {
      mtrx.time = millis();
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
  mtrx_refresh(mtrx, false);
}

void mtrx_slide_h(Max7219Matrix& mtrx, char* new_item_chars, char transition, bool is_letf) {
  for (byte i = 1; i < MTRX_PIXELS_IN_ROW + MTRX_PIXELS_IN_CHAR_BY_ROW; ++i) {
    fixed_delay(MTRX_SLIDE_DELAY_HORIZONTAL);
    mtrx.panel.clear();
    int8_t new_item_cursor = i - MTRX_PIXELS_IN_CHAR_BY_ROW - MTRX_PIXELS_IN_ROW;
    int8_t old_item_cursor = i;
    int8_t rect_cursor;
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
  mtrx_refresh(mtrx, false);
}

void replace_mtrx_text(Max7219Matrix& mtrx, char* caption) {
  if ((str_length(mtrx.data.buf) != str_length(caption)) || (text_to_pixels_in_row(caption) <= MTRX_PIXELS_IN_ROW)) {
    mtrx.changed = true;
  }
  mtrx.data.clear();
  mtrx.data.add(caption);
}

int8_t typewriter_slide_set_text(Max7219Matrix& mtrx, char* text, int8_t cursor = 0, bool is_first = false) {
  if (is_first) {
    cursor = MTRX_PIXELS_IN_ROW - (MTRX_INDENT << 1);
  }
  mString<MTRX_BUFFER> str;
  str.add(mtrx.data.buf);
  str.add(text);
  uint32_t tmr_start;
  uint32_t tmr;
  int8_t cursor_end;
  for (cursor_end = MTRX_PIXELS_IN_ROW - text_to_pixels_in_row(str.buf); cursor_end <= cursor; --cursor) {
    fixed_delay(MTRX_REFRESH_MS >> 2);
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

void close_menu(Max7219Matrix& mtrx, Menu& menu) {
  menu.level = 0;
  menu.prev_level = 1;
  menu.prev_cursor = 0;
  menu.is_printed = false;
  menu.everytime_refresh = false;
  menu.time = 0;
  memset(menu.cursor, 0, MENU_LEVELS);
  mtrx_slide_down(mtrx, "");
  clear_mtrx(mtrx);
}

void select_horizontal_menu(Menu& menu, byte& next_item_index) {
  menu.prev_level = menu.level;
  ++menu.level;
  menu.cursor[menu.level] = next_item_index;
  menu.prev_cursor = 0;
  menu.is_printed = false;
  menu.everytime_refresh = false;
}

void select_vertical_menu(Menu& menu, byte index) {
  menu.prev_level = menu.level;
  menu.prev_cursor = menu.cursor[menu.level];
  menu.cursor[menu.level] = index;
  menu.is_printed = false;
  menu.everytime_refresh = false;
}

void open_menu(Menu& menu) {
  menu.prev_level = 0;
  menu.prev_cursor = 0;
  ++menu.level;
  menu.cursor[menu.level] = 0;
  menu.is_printed = false;
  menu.everytime_refresh = false;
}

void back_menu(Menu& menu) {
  menu.prev_level = menu.level;
  menu.cursor[menu.level] = 0;
  --menu.level;
  menu.prev_cursor = menu.cursor[menu.level];
  menu.is_printed = false;
  menu.everytime_refresh = false;
}

void menu_tick(Settings& settings, byte buttons_state[CTRL_KEYS_COUNT], Menu& menu, Max7219Matrix& mtrx) {
  const byte UP_STATES = buttons_state[ButtonKey::UP];
  const byte DOWN_STATES = buttons_state[ButtonKey::SELECT];
  const byte SELECT_STATES = buttons_state[ButtonKey::DOWN];
  if (menu.level == 0) {
    // меню закрыто
    if (bitRead(SELECT_STATES, ButtonStateBit::HELD_1)) {
      open_menu(menu);
    }
  } else if (bitRead(DOWN_STATES, ButtonStateBit::HELD_1)) {
    if (menu.level == 1) {
      close_menu(mtrx, menu);
    } else {
      back_menu(menu);
    }
  } else if (bitRead(SELECT_STATES, ButtonStateBit::HOLD_0)) {
    switch (menu.level) {
      case 2: {
        if (menu.cursor[1] == MainMenu::SETS_MENU) {
          switch (menu.cursor[menu.level]) {
            case SetsMenu::RESET_OUT: {
              mString<MTRX_BUFFER> menu_caption = mtrx.data;
              mtrx_slide_down(mtrx, " ");
              init_output_params(false, true, mtrx);
              mtrx_slide_up(mtrx, menu_caption.buf);
              break;
            }
            case SetsMenu::COMMIT: {
              save_settings(settings, mtrx);
              break;
            }
          }
        }
        break;
      }
      case 3: {
        bool go_back = false;
        switch (menu.cursor[1]) {
          case MainMenu::TEMP_MENU: {
            if (menu.cursor[2] == TempMenu::MIN_TEMP || menu.cursor[2] == TempMenu::MAX_TEMP) {
              ((menu.cursor[2] == TempMenu::MIN_TEMP) ? (settings.min_temp) : settings.max_temp) = mtrx.data.toInt();
              go_back = true;
            }
            break;
          }
          case MainMenu::OPTIC_MENU: {
            if (menu.cursor[2] == OpticMenu::MIN_RPM || menu.cursor[2] == OpticMenu::MAX_RPM) {
              ((menu.cursor[2] == OpticMenu::MIN_RPM) ? (settings.min_optic_rpm) : settings.max_optic_rpm) = mtrx.data.toInt();
              go_back = true;
            }
            break;
          }
          case MainMenu::SETS_MENU: {
            switch (menu.cursor[2]) {
              case SetsMenu::HOLD_COOL: {
                settings.cool_on_hold = menu.cursor[3] == 1;
                go_back = true;
                break;
              }
              case SetsMenu::MIN_DUTY_PERCENT: {
                settings.min_duty_percent = mtrx.data.toInt();
                go_back = true;
                break;
              }
            }
            break;
          }
        }
        if (go_back) {
          back_menu(menu);
        }
        break;
      }
      case 4: {
        bool go_back = false;
        switch (menu.cursor[1]) {
          case MainMenu::PULSES_MENU: {
            if ((menu.cursor[2] == PulsesMenu::MIN_PULSES || menu.cursor[2] == PulsesMenu::MIN_PULSES) && (menu.cursor[3] != PulsesMenu::SHOW_PULSES)) {
              ((menu.cursor[2] == PulsesMenu::MIN_PULSES) ? settings.min_pulses : settings.max_pulses)[menu.cursor[3] - 1] = mtrx.data.toInt();
              go_back = true;
            }
            break;
          }
          case MainMenu::DUTIES_MENU: {
            if (menu.cursor[2] == DutiesMenu::MIN_DUTIES && (menu.cursor[3] != DutiesMenu::SHOW_DUTIES)) {
              settings.min_duties[menu.cursor[3] - 1] = mtrx.data.toInt();
              init_output_params(false, false, mtrx);
              go_back = true;
            }
            break;
          }
        }
        if (go_back) {
          back_menu(menu);
        }
        break;
      }
    }
  } else if (bitRead(SELECT_STATES, ButtonStateBit::CLICK)) {
    bool go_next = false;
    byte next_index = 0;
    switch (menu.level) {
      case 1: {
        go_next = true;
        break;
      }
      case 2: {
        switch (menu.cursor[1]) {
          case MainMenu::PULSES_MENU: {
            go_next = menu.cursor[menu.level] != PulsesMenu::SHOW_PULSES;
            break;
          }
          case MainMenu::DUTIES_MENU: {
            go_next = menu.cursor[menu.level] != DutiesMenu::SHOW_DUTIES;
            break;
          }
          case MainMenu::TEMP_MENU: {
            go_next = menu.cursor[2] == TempMenu::MIN_TEMP || menu.cursor[2] == TempMenu::MAX_TEMP;
            break;
          }
          case MainMenu::OPTIC_MENU: {
            go_next = menu.cursor[2] == OpticMenu::MIN_RPM || menu.cursor[2] == OpticMenu::MAX_RPM;
            break;
          }
          case MainMenu::SETS_MENU: {
            switch (menu.cursor[menu.level]) {
              case SetsMenu::HOLD_COOL: {
                go_next = true;
                next_index = (settings.cool_on_hold) ? 1 : 0;
                break;
              }
              case SetsMenu::MIN_DUTY_PERCENT: {
                go_next = true;
                break;
              }
            }
            break;
          }
        }
        break;
      }
      case 3: {
        switch (menu.cursor[1]) {
          case MainMenu::PULSES_MENU: {
            go_next = (menu.cursor[2] == PulsesMenu::MIN_PULSES || menu.cursor[2] == PulsesMenu::MAX_PULSES) && menu.cursor[menu.level] != PulsesMenu::SHOW_PULSES;
            break;
          }
          case MainMenu::DUTIES_MENU: {
            go_next = menu.cursor[2] == DutiesMenu::MIN_DUTIES && menu.cursor[menu.level] != DutiesMenu::SHOW_DUTIES;
            break;
          }
        }
        break;
      }
    }
    if (go_next) {
      select_horizontal_menu(menu, next_index);
    }
  } else if (bitRead(UP_STATES, ButtonStateBit::CLICK) || bitRead(DOWN_STATES, ButtonStateBit::CLICK) || bitRead(UP_STATES, ButtonStateBit::HOLD_0) || bitRead(DOWN_STATES, ButtonStateBit::HOLD_0)) {
    byte direction = 0;  // [0] - вверх, [1] - вниз
    switch (menu.level) {
      case 1: {
        if (bitRead(UP_STATES, ButtonStateBit::CLICK) && menu.cursor[menu.level] > 0) {
          bitSet(direction, 0);
        } else if (bitRead(DOWN_STATES, ButtonStateBit::CLICK) && menu.cursor[menu.level] < MAIN_MENU_SIZE - 1) {
          bitSet(direction, 1);
        }
        break;
      }
      case 2: {
        switch (menu.cursor[1]) {
          case MainMenu::PULSES_MENU: {
            if (bitRead(UP_STATES, ButtonStateBit::CLICK) && menu.cursor[menu.level] > 0) {
              bitSet(direction, 0);
            } else if (bitRead(DOWN_STATES, ButtonStateBit::CLICK) && menu.cursor[menu.level] < PULSES_MENU_SIZE - 1) {
              bitSet(direction, 1);
            }
            break;
          }
          case MainMenu::TEMP_MENU: {
            if (bitRead(UP_STATES, ButtonStateBit::CLICK) && menu.cursor[menu.level] > 0) {
              bitSet(direction, 0);
            } else if (bitRead(DOWN_STATES, ButtonStateBit::CLICK) && menu.cursor[menu.level] < TEMP_MENU_SIZE - 1) {
              bitSet(direction, 1);
            }
            break;
          }
          case MainMenu::OPTIC_MENU: {
            if (bitRead(UP_STATES, ButtonStateBit::CLICK) && menu.cursor[menu.level] > 0) {
              bitSet(direction, 0);
            } else if (bitRead(DOWN_STATES, ButtonStateBit::CLICK) && menu.cursor[menu.level] < OPTIC_MENU_SIZE - 1) {
              bitSet(direction, 1);
            }
            break;
          }
          case MainMenu::DUTIES_MENU: {
            if (bitRead(UP_STATES, ButtonStateBit::CLICK) && menu.cursor[menu.level] > 0) {
              bitSet(direction, 0);
            } else if (bitRead(DOWN_STATES, ButtonStateBit::CLICK) && menu.cursor[menu.level] < DUTIES_MENU_SIZE - 1) {
              bitSet(direction, 1);
            }
            break;
          }
          case MainMenu::SETS_MENU: {
            if (bitRead(UP_STATES, ButtonStateBit::CLICK) && menu.cursor[menu.level] > 0) {
              bitSet(direction, 0);
            } else if (bitRead(DOWN_STATES, ButtonStateBit::CLICK) && menu.cursor[menu.level] < SETS_MENU_SIZE - 1) {
              bitSet(direction, 1);
            }
            break;
          }
        }
        break;
      }
      case 3: {
        switch (menu.cursor[1]) {
          case MainMenu::PULSES_MENU: {
            if (menu.cursor[2] == PulsesMenu::MIN_PULSES || menu.cursor[2] == PulsesMenu::MAX_PULSES) {
              if (bitRead(UP_STATES, ButtonStateBit::CLICK) && menu.cursor[menu.level] > 0) {
                bitSet(direction, 0);
              } else if (bitRead(DOWN_STATES, ButtonStateBit::CLICK) && menu.cursor[menu.level] < INPUTS_COUNT) {
                bitSet(direction, 1);
              }
            }
            break;
          }
          case MainMenu::TEMP_MENU: {
            if (menu.cursor[2] == TempMenu::MIN_TEMP || menu.cursor[2] == TempMenu::MAX_TEMP) {
              byte value = mtrx.data.toInt();
              if ((bitRead(UP_STATES, ButtonStateBit::CLICK) || bitRead(UP_STATES, ButtonStateBit::HOLD_0)) && ((menu.cursor[2] == TempMenu::MIN_TEMP && value < settings.max_temp - 1) || (menu.cursor[2] == TempMenu::MAX_TEMP && value < MAX_TEMP_VALUE))) {
                mtrx.data.clear();
                mtrx.data.add(value + 1);
              } else if ((bitRead(DOWN_STATES, ButtonStateBit::CLICK) || bitRead(DOWN_STATES, ButtonStateBit::HOLD_0)) && ((menu.cursor[2] == TempMenu::MIN_TEMP && value > MIN_TEMP_VALUE) || (menu.cursor[2] == TempMenu::MAX_TEMP && value > settings.min_temp + 1))) {
                mtrx.data.clear();
                mtrx.data.add(value - 1);
              }
            }
            break;
          }
          case MainMenu::OPTIC_MENU: {
            if (menu.cursor[2] == OpticMenu::MIN_RPM || menu.cursor[2] == OpticMenu::MAX_RPM) {
              int value = mtrx.data.toInt();
              if ((bitRead(UP_STATES, ButtonStateBit::CLICK) || bitRead(UP_STATES, ButtonStateBit::HOLD_0)) && ((menu.cursor[2] == OpticMenu::MIN_RPM && value < settings.max_optic_rpm - 1) || (menu.cursor[2] == OpticMenu::MAX_RPM && value < MAX_OPTIC_RPM_VALUE))) {
                mtrx.data.clear();
                mtrx.data.add(value + 1);
              } else if ((bitRead(DOWN_STATES, ButtonStateBit::CLICK) || bitRead(DOWN_STATES, ButtonStateBit::HOLD_0)) && ((menu.cursor[2] == OpticMenu::MIN_RPM && value > MIN_OPTIC_RPM_VALUE) || (menu.cursor[2] == OpticMenu::MAX_RPM && value > settings.min_optic_rpm + 1))) {
                mtrx.data.clear();
                mtrx.data.add(value - 1);
              }
            }
            break;
          }
          case MainMenu::DUTIES_MENU: {
            if (menu.cursor[2] == DutiesMenu::MIN_DUTIES) {
              if (bitRead(UP_STATES, ButtonStateBit::CLICK) && menu.cursor[menu.level] > 0) {
                bitSet(direction, 0);
              } else if (bitRead(DOWN_STATES, ButtonStateBit::CLICK) && menu.cursor[menu.level] < OUTPUTS_COUNT) {
                bitSet(direction, 1);
              }
            }
            break;
          }
          case MainMenu::SETS_MENU: {
            switch (menu.cursor[2]) {
              case SetsMenu::HOLD_COOL: {
                if (bitRead(UP_STATES, ButtonStateBit::CLICK) && menu.cursor[menu.level] == 1) {
                  bitSet(direction, 0);
                } else if (bitRead(DOWN_STATES, ButtonStateBit::CLICK) && menu.cursor[menu.level] == 0) {
                  bitSet(direction, 1);
                }
                break;
              }
              case SetsMenu::MIN_DUTY_PERCENT: {
                int value = mtrx.data.toInt();
                if ((bitRead(UP_STATES, ButtonStateBit::CLICK) || bitRead(UP_STATES, ButtonStateBit::HOLD_0)) && value < 100) {
                  mtrx.data.clear();
                  mtrx.data.add(value + 1);
                } else if ((bitRead(DOWN_STATES, ButtonStateBit::CLICK) || bitRead(DOWN_STATES, ButtonStateBit::HOLD_0)) && value >= 1) {
                  mtrx.data.clear();
                  mtrx.data.add(value - 1);
                }
                break;
              }
            }
            break;
          }
        }
        break;
      }
      case 4: {
        switch (menu.cursor[1]) {
          case MainMenu::PULSES_MENU: {
            if ((menu.cursor[2] == PulsesMenu::MIN_PULSES || menu.cursor[2] == PulsesMenu::MAX_PULSES) && menu.cursor[3] != PulsesMenu::SHOW_PULSES) {
              byte value = mtrx.data.toInt();
              if ((bitRead(UP_STATES, ButtonStateBit::CLICK) || bitRead(UP_STATES, ButtonStateBit::HOLD_0)) && ((menu.cursor[2] == PulsesMenu::MIN_PULSES && value < settings.max_pulses[menu.cursor[3] - 1] - 1) || (menu.cursor[2] == PulsesMenu::MAX_PULSES && value < PULSE_WIDTH))) {
                mtrx.data.clear();
                mtrx.data.add(value + 1);
              } else if ((bitRead(DOWN_STATES, ButtonStateBit::CLICK) || bitRead(DOWN_STATES, ButtonStateBit::HOLD_0)) && ((menu.cursor[2] == PulsesMenu::MIN_PULSES && value > 0) || (menu.cursor[2] == PulsesMenu::MAX_PULSES && value > settings.min_pulses[menu.cursor[3] - 1] + 1))) {
                mtrx.data.clear();
                mtrx.data.add(value - 1);
              }
            }
            break;
          }
          case MainMenu::DUTIES_MENU: {
            if (menu.cursor[2] == DutiesMenu::MIN_DUTIES && menu.cursor[3] != DutiesMenu::SHOW_DUTIES) {
              byte value = mtrx.data.toInt();
              if ((bitRead(UP_STATES, ButtonStateBit::CLICK) || bitRead(UP_STATES, ButtonStateBit::HOLD_0)) && value < MAX_DUTY) {
                mtrx.data.clear();
                mtrx.data.add(value + 1);
              } else if ((bitRead(DOWN_STATES, ButtonStateBit::CLICK) || bitRead(DOWN_STATES, ButtonStateBit::HOLD_0)) && value > MIN_DUTY) {
                mtrx.data.clear();
                mtrx.data.add(value - 1);
              }
            }
            break;
          }
        }
        break;
      }
    }
    if (direction != 0) {
      select_vertical_menu(menu, menu.cursor[menu.level] + ((bitRead(direction, 0)) ? -1 : 1));
    }
  }

  if (menu.level != 0) {
    bool has_key_action = false;
    for (byte i = 0; !has_key_action && i < CTRL_KEYS_COUNT; ++i) {
      if (buttons_state[i] != 0) {
        has_key_action = true;
      }
    }
    if (has_key_action) {
      menu.time = millis();
    }
  }
}

void menu_refresh(Settings& settings, InputsInfo& inputs_info, uint32_t time, Max7219Matrix& mtrx, Menu& menu) {
  if (!menu.is_printed || menu.everytime_refresh) {
    if (menu.level != 0) {
      bool is_repeat = menu.everytime_refresh && menu.is_printed;
      menu.is_printed = true;
      menu.everytime_refresh = false;
      uart_print(menu.level);
      uart_print(": ");
      for (byte i = 1; i <= menu.level; ++i) {
        if (i != 1) {
          uart_print(" ");
        }
        uart_print(menu.cursor[i]);
      }
      uart_println();
      mString<MTRX_BUFFER> caption;
      switch (menu.level) {
        case 1: {
          switch (menu.cursor[menu.level]) {
            case MainMenu::PULSES_MENU: {
              caption = "input";
              break;
            }
            case MainMenu::TEMP_MENU: {
              caption = "temp";
              break;
            }
            case MainMenu::OPTIC_MENU: {
              caption = "optic";
              break;
            }
            case MainMenu::DUTIES_MENU: {
              caption = "outs";
              break;
            }
            case MainMenu::SETS_MENU: {
              caption = "sets";
              break;
            }
          }
          break;
        }
        case 2: {
          switch (menu.cursor[1]) {
            case MainMenu::PULSES_MENU: {
              switch (menu.cursor[menu.level]) {
                case PulsesMenu::SHOW_PULSES: {
                  for (byte i = 0; i < INPUTS_COUNT; ++i) {
                    if (i != 0) {
                      caption.add(" ");
                    }
                    caption.add(inputs_info.pulses_info[i].value);
                  }
                  uart_println(caption.buf);
                  menu.everytime_refresh = true;
                  break;
                }
                case PulsesMenu::MIN_PULSES: {
                  caption = "min";
                  break;
                }
                case PulsesMenu::MAX_PULSES: {
                  caption = "max";
                  break;
                }
              }
              break;
            }
            case MainMenu::TEMP_MENU: {
              switch (menu.cursor[menu.level]) {
                case TempMenu::SHOW_TEMP: {
                  if (inputs_info.temperature.available) {
                    caption.add(inputs_info.temperature.value);
                  } else {
                    caption.add("--");
                  }
                  menu.everytime_refresh = true;
                  break;
                }
                case TempMenu::MIN_TEMP: {
                  caption = "min";
                  break;
                }
                case TempMenu::MAX_TEMP: {
                  caption = "max";
                  break;
                }
              }
              break;
            }
            case MainMenu::OPTIC_MENU: {
              switch (menu.cursor[menu.level]) {
                case OpticMenu::SHOW_RPM: {
                  caption.add(inputs_info.optical.rpm);
                  menu.everytime_refresh = true;
                  break;
                }
                case OpticMenu::MIN_RPM: {
                  caption = "min";
                  break;
                }
                case OpticMenu::MAX_RPM: {
                  caption = "max";
                  break;
                }
              }
              break;
            }
            case MainMenu::DUTIES_MENU: {
              switch (menu.cursor[menu.level]) {
                case DutiesMenu::SHOW_DUTIES: {
                  if (inputs_info.cooling_on) {
                    caption = "100%";
                  } else {
                    if (inputs_info.pwm_percents.pulse < 10) {
                      caption.add(0);
                    }
                    caption.add(inputs_info.pwm_percents.pulse);
                    caption.add(" ");
                    if (inputs_info.pwm_percents.temperature < 10) {
                      caption.add(0);
                    }
                    caption.add(inputs_info.pwm_percents.temperature);
                    caption.add(" ");
                    if (inputs_info.pwm_percents.optical < 10) {
                      caption.add(0);
                    }
                    caption.add(inputs_info.pwm_percents.optical);
                  }

                  menu.everytime_refresh = true;
                  break;
                }
                case DutiesMenu::MIN_DUTIES: {
                  caption = "min";
                  break;
                }
              }
              break;
            }
            case MainMenu::SETS_MENU: {
              switch (menu.cursor[menu.level]) {
                case SetsMenu::HOLD_COOL: {
                  caption = "hold cool";
                  break;
                }
                case SetsMenu::RESET_OUT: {
                  caption = "reset ctrl";
                  break;
                }
                case SetsMenu::MIN_DUTY_PERCENT: {
                  caption = "min rpm %";
                  break;
                }
                case SetsMenu::COMMIT: {
                  caption = "commit";
                  break;
                }
              }
              break;
            }
          }
          break;
        }
        case 3: {
          switch (menu.cursor[1]) {
            case MainMenu::PULSES_MENU: {
              if (menu.cursor[2] == PulsesMenu::MIN_PULSES || menu.cursor[2] == PulsesMenu::MAX_PULSES) {
                if (menu.cursor[menu.level] == PulsesMenu::SHOW_PULSES) {
                  caption.add(get_pulses_settings(menu.cursor[2] == 1).buf);
                } else {
                  caption = "set ";
                  caption.add(menu.cursor[menu.level]);
                }
              }
              break;
            }
            case MainMenu::TEMP_MENU: {
              if (menu.cursor[2] == TempMenu::MIN_TEMP || menu.cursor[2] == TempMenu::MAX_TEMP) {
                if (mtrx.data.startsWith("min") || mtrx.data.startsWith("max")) {
                  mtrx.data.clear();
                  mtrx.data.add((menu.cursor[2] == TempMenu::MIN_TEMP) ? settings.min_temp : settings.max_temp);
                }
                byte value = mtrx.data.toInt();
                if (value < 10) {
                  caption.add(0);
                }
                caption.add(value);
                menu.everytime_refresh = true;
              }
              break;
            }
            case MainMenu::OPTIC_MENU: {
              if (menu.cursor[2] == OpticMenu::MIN_RPM || menu.cursor[2] == OpticMenu::MAX_RPM) {
                if (mtrx.data.startsWith("min") || mtrx.data.startsWith("max")) {
                  mtrx.data.clear();
                  mtrx.data.add((menu.cursor[2] == OpticMenu::MIN_RPM) ? settings.min_optic_rpm : settings.max_optic_rpm);
                }
                int value = mtrx.data.toInt();
                if (value < 10) {
                  caption.add(0);
                }
                if (value < 100) {
                  caption.add(0);
                }
                if (value < 1000) {
                  caption.add(0);
                }
                caption.add(value);
                menu.everytime_refresh = true;
              }
              break;
            }
            case MainMenu::DUTIES_MENU: {
              if (menu.cursor[2] == DutiesMenu::MIN_DUTIES) {
                if (menu.cursor[menu.level] == DutiesMenu::SHOW_DUTIES) {
                  caption.add(get_duties_settings().buf);
                } else {
                  caption = "set ";
                  caption.add(menu.cursor[menu.level]);
                }
              }
              break;
            }
            case MainMenu::SETS_MENU: {
              switch (menu.cursor[2]) {
                case SetsMenu::HOLD_COOL: {
                  if (menu.cursor[menu.level] == 0) {
                    caption.add("false");
                  } else {
                    caption.add("true");
                  }
                  break;
                }
                case SetsMenu::MIN_DUTY_PERCENT: {
                  byte value = mtrx.data.toInt();
                  if (value == 0 && mtrx.data.indexOf('0') != 0) {
                    if (settings.min_duty_percent < 10) {
                      caption.add(0);
                    }
                    caption.add(settings.min_duty_percent);
                  } else {
                    if (value < 10) {
                      caption.add(0);
                    }
                    caption.add(value);
                  }
                  menu.everytime_refresh = true;
                  break;
                }
              }
              break;
            }
          }
          break;
        }
        case 4: {
          switch (menu.cursor[1]) {
            case MainMenu::PULSES_MENU: {
              if ((menu.cursor[2] == PulsesMenu::MIN_PULSES || menu.cursor[2] == PulsesMenu::MAX_PULSES) && menu.cursor[3] != PulsesMenu::SHOW_PULSES) {
                if (mtrx.data.startsWith("set ")) {
                  mtrx.data.clear();
                  mtrx.data.add(((menu.cursor[2] == PulsesMenu::MIN_PULSES) ? settings.min_pulses : settings.max_pulses)[menu.cursor[3] - 1]);
                }
                byte value = mtrx.data.toInt();
                if (value < 10) {
                  caption.add(0);
                }
                caption.add(value);
                menu.everytime_refresh = true;
              }
              break;
            }
            case MainMenu::DUTIES_MENU: {
              if (menu.cursor[2] == DutiesMenu::MIN_DUTIES && menu.cursor[3] != DutiesMenu::SHOW_DUTIES) {
                if (mtrx.data.startsWith("set ")) {
                  mtrx.data.clear();
                  mtrx.data.add(settings.min_duties[menu.cursor[3] - 1]);
                }
                byte value = mtrx.data.toInt();
                if (value < 10) {
                  caption.add(0);
                }
                if (value < 100) {
                  caption.add(0);
                }
                caption.add(value);
                menu.everytime_refresh = true;
              }
              break;
            }
          }
          break;
        }
      }

      if (is_repeat) {
        replace_mtrx_text(mtrx, caption.buf);
      } else if (str_length(caption.buf) > 0) {
        if (menu.prev_level == 0) {
          mtrx_slide_up(mtrx, caption.buf);
        } else if (menu.level == menu.prev_level) {
          mtrx_slide_v(mtrx, caption.buf, menu.prev_cursor < menu.cursor[menu.level]);
        } else {
          mtrx_slide_h(mtrx, caption.buf, '>', menu.level > menu.prev_level);
        }
      } else if (menu.level == 0) {
        mtrx_slide_down(mtrx, "");
      }
    }
  } else if (is_success_delay(menu.time, MENU_TIMEOUT)) {
    close_menu(mtrx, menu);
  }
}

#endif
