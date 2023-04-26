// коррекция времени из-за изменения частоты Timer0
#define TIME_CORRECTOR(func) (((func >> 4) * 10) >> 4) /* коррекция time/25.6 */
#define micros() TIME_CORRECTOR(micros())              /* коррекция micros */
#define millis() TIME_CORRECTOR(millis())              /* коррекция millis */

// настраиваемые параметры

#define MTRX_PANELS_COUNT 4 /* количество модулей матрицы в ряд */
#define MTRX_ROWS_COUNT 1   /* количество рядов модулей матрицы */
#define MTRX_BRIGHT 0       /* яркость матрицы [0..15] */

// #define DONT_USE_UART /* не использовать UART */
#ifdef OCR3A
// #define USE_TIMER_3A /* использование выхода TX для ШИМ, RX - чтение RPM */
#ifdef USE_TIMER_3A
#define DONT_USE_UART
#endif
#endif
// ^^^ настраиваемые параметры ^^^

#ifndef DONT_USE_UART
#define MU_RX_BUF 64 /* размер буфера */
#define MU_PRINT
#include <MicroUART.h>
MicroUART uart;  // интерфейс работы с серийным портом
#define uart_print(value) uart.print(value)
#define uart_println(value) uart.println(value)
#else
#define uart_print(value)
#define uart_println(value)
#endif

#include <AnalogKey.h>
#include <EEPROM.h>
#include <EncButton2.h>
#include <GyverMAX7219.h>
#include <GyverPWM.h>           /* используется с коммитами из pull-requests #4 и #5 (https://github.com/GyverLibs/GyverPWM/pulls) */
#include <SpectrumCalculator.h> /* https://github.com/scala-12/SpectrumCalculator */
#include <mString.h>
#include <microDS18B20.h>

#include "constants.h"
#include "functions.h"
#include "macros.h"

#define get_out_pin(index) OUTPUTS_PINS[index][0]
#define get_rpm_pin(index) OUTPUTS_PINS[index][1]

// вычисляемые константы
const byte INPUTS_COUNT = get_arr_len(INPUTS_PINS);    // количество ШИМ входов
const byte OUTPUTS_COUNT = get_arr_len(OUTPUTS_PINS);  // количество ШИМ выходов

// переменные

struct {
  AnalogKey<COOL_MODES_PIN, COOL_MODES_COUNT, cool_modes_map> keys;  // клавиши выбора режима проветривания
  EncButton2<VIRT_BTN, EB_TICK> buttons[COOL_MODES_COUNT];           // кнопки выбора режима проветривания
  CoolMode mode;
} cool_modes_keyboard;

struct {
  AnalogKey<ANALOG_KEYS_PIN, CTRL_KEYS_COUNT, buttons_map> keys;  // клавиши управления
  EncButton2<VIRT_BTN, EB_TICK> buttons[CTRL_KEYS_COUNT];         // кнопки управления
  uint32_t time;                                                  // время окончания чтения
  bool ticks_over;                                                // ожидание нажатия закончено
  byte states[CTRL_KEYS_COUNT];                                   // состояние кнопок
} ctrl_keyboard;

struct InputsInfo {
  uint32_t time;                                // время чтения входящих сигналов
  bool cooling_on;                              // режим максимальной скорости
  byte smooth_index;                            // шаг для сглаживания
  mString<3 * INPUTS_COUNT> str_pulses_values;  // строка с значениями входящих ШИМ

  struct {
    byte smooths_buffer[BUFFER_SIZE_FOR_SMOOTH];  // буфер для сглаживания входящего сигнала
    byte value;                                   // последнее значение PWM для отрисовки на матрице
  } pulses_info[INPUTS_COUNT];

  struct {
    MicroDS18B20<TEMP_SENSOR_PIN> sensor;  // датчик температуры
    byte value;                            // значения датчика температуры
    bool available;                        // датчик температуры активен
  } temperature;

  struct {
    byte pin;                                         // пин оптического датчика
    bool state;                                       // встречен разделитель
    uint16_t counter;                                 // количество вращений на текущий момент
    uint16_t rpm;                                     // скорость, преобразованное из количества вращений за отведенное время
    uint16_t smooths_buffer[BUFFER_SIZE_FOR_SMOOTH];  // буфер для сглаживания сигнала
  } optical;

  struct {
    byte pulse;        // результат преобразования входящих ШИМ в выходной ШИМ
    byte temperature;  // результат преобразования температуры в ШИМ
    byte optical;      // результат преобразования скорости вращения в ШИМ
  } pwm_percents;
};
InputsInfo inputs_info;

// отказ от кеша экономит 300 байт
byte percent_2duty_cache[OUTPUTS_COUNT][101];  // кеш преобразования процента скорости в PWM
#define convert_percent_2duty(index, percent) percent_2duty_cache[index][percent]

boolean is_debug;  // флаг вывода технической информации
struct Reciever {
  mString<64> data;  // буфер чтения команды из серийного порта
  boolean flag;      // флаг на чтение
};
Reciever reciever;

SpectrumCalculator spectrum_calculator(VISUALS_COUNT, MTRX_PIXELS_IN_ROW, MTRX_PIXELS_IN_COLUMN);
const byte DRAW_OFFSET = (MTRX_PIXELS_IN_ROW - spectrum_calculator.get_used_columns()) >> 1;

struct Settings {
  byte min_duties[OUTPUTS_COUNT];  // минимальный PWM для начала вращения
  byte min_pulses[INPUTS_COUNT];   // нижняя граница чувствительности к входящему PWM
  byte max_pulses[INPUTS_COUNT];   // верхняя граница чувствительности к входящему PWM
  byte max_temp;                   // верхняя граница чувсвительности температурного датчика
  byte min_temp;                   // нижняя граница чувсвительности температурного датчика
  uint16_t max_optic_rpm;          // верхняя граница чувсвительности оптического датчика
  uint16_t min_optic_rpm;          // нижняя граница чувсвительности оптического датчика
  bool cool_on_hold;               // состояние кнопки для активации режима продувки
  bool do_fill_visual;             // заливка графика
  byte min_duty_percent;           // процент от минимальной скорости вращения. если меньше 100, то вентилятор может и остановиться
};
Settings settings;  // хранимые параметры

struct Max7219Matrix {
  mString<MTRX_BUFFER> data;  // буфер вывода на матрицу
  bool changed;               // флаг изменения буфера
  int8_t cursor;              // позиция курсора
  int8_t next_cursor;         // позиция курсора
  int8_t border_cursor;
  byte border_delay_counter;
  uint32_t time;  // время прошлого обновления отображения
  MAX7219<MTRX_PANELS_COUNT, MTRX_ROWS_COUNT, MTRX_CS_PIN, MTRX_DATA_PIN, MTRX_CLOCK_PIN> panel;
};
Max7219Matrix mtrx;

struct Menu {
  byte level;
  byte prev_level;
  byte cursor[MENU_LEVELS];
  byte prev_cursor;
  bool is_printed;
  bool everytime_refresh;
  uint32_t time;
};
Menu menu;

void init_output_params(bool is_first, bool init_rpm, Max7219Matrix& mtrx);
byte stop_fans(byte ignored_bits, bool wait_stop);
boolean has_rpm(byte index, byte more_than_rpm = 0);
void apply_fan_pwm(byte index, byte duty);
void read_temp();
void apply_pwm_4all(byte percent);
void read_pulses();

#include "extras.h"

void MU_serialEvent() {
  // нужна для чтения буфера
}

void setup() {
#ifndef DONT_USE_UART
  uart.begin(SERIAL_SPEED);
  uart_println(F("start"));
#endif

  for (byte i = 0; i < INPUTS_COUNT; ++i) {
    pinMode(INPUTS_PINS[i], INPUT);
    memset(inputs_info.pulses_info[i].smooths_buffer, 0, BUFFER_SIZE_FOR_SMOOTH);
  }

  inputs_info.temperature.sensor.requestTemp();

  cool_modes_keyboard.mode = CoolMode::STANDART;
  cool_modes_keyboard.keys.setWindow(1024 / ((COOL_MODES_COUNT << 1) - 1));
  ctrl_keyboard.keys.setWindow(1024 / ((CTRL_KEYS_COUNT << 1) - 1));
  for (byte i = 0; i < CTRL_KEYS_COUNT; ++i) {
    ctrl_keyboard.states[i] = 0;
  }

  inputs_info.optical.pin = OPTICAL_SENSOR_PIN;
  pinMode(inputs_info.optical.pin, INPUT);
  inputs_info.optical.counter = 0;
  inputs_info.optical.rpm = 0;
  inputs_info.optical.state = false;
  memset(inputs_info.optical.smooths_buffer, 0, BUFFER_SIZE_FOR_SMOOTH);

  mtrx.panel.begin();
  mtrx.panel.setBright(MTRX_BRIGHT);
  mtrx.panel.textDisplayMode(GFX_REPLACE);
  mtrx.time = 0;
  init_matrix(mtrx);

  // обнуляем таймеры
  inputs_info.time = 0;
  ctrl_keyboard.time = 0;

  inputs_info.smooth_index = 0;    // номер шага в буфере для сглаживания
  inputs_info.cooling_on = false;  // не режим продувки
  is_debug = false;                // не дебаг
  reciever.data = "";              // ощищаем буфер
  reciever.flag = false;

  if (EEPROM.read(INIT_ADDR) != VERSION_NUMBER) {
    // если структура хранимых данных изменена, то делаем дефолт
    uart_println(F("new data version"));
    settings.min_duty_percent = 100;
    settings.cool_on_hold = true;
    settings.max_optic_rpm = DEFAULT_MAX_OPTIC_RPM;
    settings.min_optic_rpm = DEFAULT_MIN_OPTIC_RPM;
    settings.max_temp = DEFAULT_MAX_TEMP;
    settings.min_temp = DEFAULT_MIN_TEMP;
    for (byte i = 0; i < OUTPUTS_COUNT; ++i) {
      settings.min_duties[i] = MAX_DUTY;
    }
    for (byte i = 0; i < INPUTS_COUNT; ++i) {
      settings.min_pulses[i] = convert_percent_2pulse(DEFAULT_MIN_PERCENT);
      settings.max_pulses[i] = convert_percent_2pulse(DEFAULT_MAX_PERCENT);
    }
    EEPROM.put(0, settings);
    EEPROM.write(INIT_ADDR, VERSION_NUMBER);

    init_output_params(true, false, mtrx);
  } else {
    uart_println(F("load data from EEPROM"));
    EEPROM.get(0, settings);
    init_output_params(true, false, mtrx);
  }
  close_menu(mtrx, menu);

  spectrum_calculator.set_round_range(2);
}

void loop() {
  read_and_exec_command(settings, inputs_info, reciever, is_debug, mtrx);

  if (inputs_info.optical.state != digital_read_fast(inputs_info.optical.pin)) {
    inputs_info.optical.state = !inputs_info.optical.state;
    if (inputs_info.optical.state) {
      ++inputs_info.optical.counter;
    }
  }

  ctrl_keyboard.ticks_over = true;
  for (byte i = 0; i < CTRL_KEYS_COUNT; ++i) {
    switch (ctrl_keyboard.buttons[i].tick(ctrl_keyboard.keys.status(i))) {
      case 5: {
        bitSet(ctrl_keyboard.states[i], ButtonStateBit::CLICK);
        ctrl_keyboard.buttons[i].resetState();
        if (is_debug) {
          uart_print("click ");
          uart_println(i);
        }
        break;
      }
      case 6: {
        if (ctrl_keyboard.buttons[i].held(1)) {
          bitSet(ctrl_keyboard.states[i], ButtonStateBit::HELD_1);
          if (is_debug) {
            uart_print("held ");
            uart_println(i);
          }
        } else if (ctrl_keyboard.buttons[i].hold(0)) {
          bitSet(ctrl_keyboard.states[i], ButtonStateBit::HOLD_0);
          if (is_debug) {
            uart_print("hold ");
            uart_println(i);
          }
        }
        break;
      }
    }
    if (ctrl_keyboard.buttons[i].busy()) {
      if (ctrl_keyboard.ticks_over && !(bitRead(ctrl_keyboard.states[i], ButtonStateBit::HOLD_0) || bitRead(ctrl_keyboard.states[i], ButtonStateBit::HELD_1))) {
        ctrl_keyboard.ticks_over = false;
      }
    }
  }

  uint32_t time = millis();
  if (ctrl_keyboard.ticks_over && is_success_delay(ctrl_keyboard.time, MTRX_REFRESH_MS >> 1)) {
    ctrl_keyboard.time = time;

    menu_tick(settings, ctrl_keyboard.states, menu, mtrx);

    for (byte i = 0; i < CTRL_KEYS_COUNT; ++i) {
      if (i != 2 && bitRead(ctrl_keyboard.states[i], ButtonStateBit::HOLD_0) && ctrl_keyboard.buttons[i].busy()) {
        ctrl_keyboard.states[i] = 0;
        bitSet(ctrl_keyboard.states[i], ButtonStateBit::HOLD_0);
      } else {
        ctrl_keyboard.states[i] = 0;
      }
    }
  }

  menu_refresh(settings, inputs_info, time, mtrx, menu);
  mtrx_refresh(mtrx, true);
  if (!menu.is_printed) {
    spectrum_calculator.tick();
    if (!is_success_delay(menu.time, MENU_TIMEOUT) || spectrum_calculator.is_changed()) {
      byte values_a[spectrum_calculator.get_used_columns()];
      byte values_b[spectrum_calculator.get_used_columns()];
      spectrum_calculator.pull_max_positions(values_a, values_b);
      mtrx.panel.clear();
      if (settings.do_fill_visual) {
        for (byte i = 0; i < spectrum_calculator.get_used_columns(); ++i) {
          mtrx.panel.fastLineV(i + DRAW_OFFSET, MTRX_PIXELS_IN_COLUMN, MTRX_PIXELS_IN_COLUMN - max(values_a[i], values_b[i]));
        }
      } else {
        for (byte i = 0; i < spectrum_calculator.get_used_columns(); ++i) {
          mtrx.panel.fastLineV(i + DRAW_OFFSET, MTRX_PIXELS_IN_COLUMN - values_a[i], MTRX_PIXELS_IN_COLUMN - values_b[i]);
        }
      }

      mtrx.panel.update();
    }
  }

  bool do_inputs_refresh = is_success_delay(inputs_info.time, SENSE_REFRESH_MS);
  if (do_inputs_refresh) {
    inputs_info.time = time;

    inputs_info.optical.smooths_buffer[inputs_info.smooth_index] = inputs_info.optical.counter;
    inputs_info.optical.rpm = find_median<BUFFER_SIZE_FOR_SMOOTH, uint16_t>(inputs_info.optical.smooths_buffer, true) * (1000 / SENSE_REFRESH_MS) * 60;
    inputs_info.optical.counter = 0;
    inputs_info.pwm_percents.optical = convert_by_sqrt(inputs_info.optical.rpm, settings.min_optic_rpm, settings.max_optic_rpm, 0, 100);

    read_pulses();
    read_temp();

    byte visual_data[VISUALS_COUNT];
    visual_data[0] = inputs_info.pwm_percents.optical;
    visual_data[1] = calculate_avg(calculate_avg(inputs_info.pwm_percents.optical, inputs_info.pwm_percents.temperature), inputs_info.pwm_percents.pulse);
    visual_data[2] = inputs_info.pwm_percents.temperature;
    visual_data[3] = inputs_info.pwm_percents.pulse;

    spectrum_calculator.put_signals(visual_data);

    cool_modes_keyboard.mode = CoolMode::STANDART;
    for (byte i = 0; i < COOL_MODES_COUNT && cool_modes_keyboard.mode == CoolMode::STANDART; ++i) {
      switch (cool_modes_keyboard.buttons[i].tick(cool_modes_keyboard.keys.status(i))) {
        case 7: {
          if (!cool_modes_keyboard.buttons[i].busy()) {
            break;
          }
          // break; not used
        }
        case 6: {
          cool_modes_keyboard.mode = CoolMode(i);
          break;
        }
      }
    }

    if ((settings.cool_on_hold && cool_modes_keyboard.mode == CoolMode::MAX) || (!settings.cool_on_hold && cool_modes_keyboard.mode == CoolMode::STANDART)) {
      if (!inputs_info.cooling_on) {
        inputs_info.cooling_on = true;
        apply_pwm_4all(100);
        uart_println(F("cooling ON"));
      }
    } else if (inputs_info.cooling_on) {
      inputs_info.cooling_on = false;
      uart_println(F("cooling OFF"));
    } else {
      byte pwm_percent_value = max(inputs_info.pwm_percents.pulse, inputs_info.pwm_percents.temperature);
      if (cool_modes_keyboard.mode != CoolMode::WITHOUT_OPTIC) {
        pwm_percent_value = max(pwm_percent_value, inputs_info.pwm_percents.optical);
      }
      apply_pwm_4all(pwm_percent_value);
    }
  }
}

boolean has_rpm(byte index, byte more_than_rpm = 0) {
  uart_print(F("fan "));
  uart_print(get_out_pin(index));

  unsigned long rpm = pulseIn(get_rpm_pin(index), HIGH, 500000);
  if (rpm == 0) {
    rpm = pulseIn(get_rpm_pin(index), LOW, 1500000);
    if (rpm == 0) {
      rpm = (digital_read_fast(get_rpm_pin(index)) == LOW) ? 1 : 0;
    }
  }

  uart_print(" ");
  uart_println(rpm);

  return rpm > more_than_rpm;
}

/**
 *  останавливает вентиляторы, за исключением указанных в ignored_bits
 *  возвращает в битах неостановленные вентиляторы за исключением игнорируемых
 */
byte stop_fans(byte ignored_bits, bool wait_stop) {
  byte running_bits = 0;
  byte complete_bits = 0;
  for (byte i = 0; i < OUTPUTS_COUNT; ++i) {
    if (bitRead(ignored_bits, i)) {
      bitSet(complete_bits, i);
    } else {
      bitSet(running_bits, i);
      apply_fan_pwm(i, MIN_DUTY);
    }
  }

  byte _i = 0;
  byte max_count_index = 20;
  for (; ((running_bits | ignored_bits) != complete_bits) && (_i < max_count_index || wait_stop); ++_i) {
    uart_print(F("stop fans, "));
    uart_print(_i);
    uart_print(": ");
    print_bits(running_bits, OUTPUTS_COUNT);
    uart_print(", ");
    print_bits(ignored_bits, OUTPUTS_COUNT);
    uart_print(", ");
    print_bits(complete_bits, OUTPUTS_COUNT);
    uart_println();
    if (!wait_stop) {
      fixed_delay(500);
    }
    for (byte i = 0; i < OUTPUTS_COUNT; ++i) {
      if (bitRead(running_bits, i) && !bitRead(ignored_bits, i)) {
        if (!has_rpm(i)) {
          bitClear(running_bits, i);
        }
      }
    }
  }

  if (wait_stop || _i < max_count_index) {
    uart_print(F("stopped "));
  } else {
    uart_print(F("not stopped "));
  }

  print_bits(running_bits, OUTPUTS_COUNT);
  uart_print(", ");
  print_bits(ignored_bits, OUTPUTS_COUNT);
  uart_print(", ");
  print_bits(complete_bits, OUTPUTS_COUNT);
  uart_println();
  uart_println();

  return running_bits;
}

void init_output_params(bool is_first, bool init_rpm, Max7219Matrix& mtrx) {
  if (is_first) {
    for (byte i = 0; i < OUTPUTS_COUNT; ++i) {
#ifdef OCR3A
      init_timer_pin(get_out_pin(i));
#else
      pinMode(get_out_pin(i), OUTPUT);
#endif
      pinMode(get_rpm_pin(i), INPUT_PULLUP);

      PWM_frequency(get_out_pin(i), (1000000 / PULSE_WIDTH), FAST_PWM);  // (1s -> mcs) / (период шим)
    }
  }

  if (init_rpm) {
    byte start_duties[OUTPUTS_COUNT];
    // не будем искать минимальную скорость, если вентилятор не остановился или игнорируется
    int8_t cursor = typewriter_slide_set_text(mtrx, "stop", 0, true);
    byte ignored_bits = stop_fans(ignored_bits, false);
    for (byte i = 0; i < OUTPUTS_COUNT; ++i) {
      if (!bitRead(ignored_bits, i)) {
        // если остановился, то пробуем запустить
        apply_fan_pwm(i, MAX_DUTY);

        cursor = typewriter_slide_set_text(mtrx, "-", cursor);
      } else {
        cursor = typewriter_slide_set_text(mtrx, "1", cursor);
      }
    }
    fixed_delay(3000);
    cursor = typewriter_slide_set_text(mtrx, ";run", cursor);
    for (byte i = 0; i < OUTPUTS_COUNT; ++i) {
      if (!bitRead(ignored_bits, i) && !has_rpm(i)) {
        // если не запустился, то игнорируем его детальную настройку из-за отсутствия обратной связи
        bitSet(ignored_bits, i);
        settings.min_duties[i] = MIN_DUTY;
        uart_print("fan ");
        uart_print(get_out_pin(i));
        uart_println(F(" without RPM"));
        cursor = typewriter_slide_set_text(mtrx, "-", cursor);
      } else {
        cursor = typewriter_slide_set_text(mtrx, "1", cursor);
        settings.min_duties[i] = MAX_DUTY;
        start_duties[i] = MIN_DUTY;
      }
    }

    byte complete_bits = (1 << OUTPUTS_COUNT) - 1;
    if (ignored_bits != complete_bits) {
      for (byte _i = 0; _i < 7; ++_i) {
        stop_fans(ignored_bits, true);
        byte middle_duties[OUTPUTS_COUNT];
        for (byte i = 0; i < OUTPUTS_COUNT; ++i) {
          if (!bitRead(ignored_bits, i)) {
            middle_duties[i] = ((settings.min_duties[i] - start_duties[i]) >> 1) + 1 + start_duties[i];
            apply_fan_pwm(i, middle_duties[i]);
          }
        }
        fixed_delay(700);
        mString<8> str;
        for (byte i = 0; i < OUTPUTS_COUNT; ++i) {
          if (!bitRead(ignored_bits, i)) {
            uart_print("fan ");
            uart_print(get_out_pin(i));
            uart_print(F(" binary search min duty ["));
            uart_print(start_duties[i]);
            uart_print(", ");
            uart_print(settings.min_duties[i]);
            uart_println("]");

            str.clear();
            str.add(" ");
            str.add(i + 1);
            str.add("_");
            str.add(middle_duties[i]);
            cursor = typewriter_slide_set_text(mtrx, str.buf, cursor);

            if ((has_rpm(i))) {
              settings.min_duties[i] = middle_duties[i];
            } else {
              start_duties[i] = middle_duties[i];
            }
          }
        }
      }
      stop_fans(ignored_bits, true);

      byte ready_bits = ignored_bits;
      mString<8> str;
      for (byte _i = 0; _i < MAX_DUTY && (ready_bits != complete_bits); ++_i) {
        uart_print(_i);
        uart_print(F(", search step-by-step min duty "));
        print_bits(ready_bits, OUTPUTS_COUNT);
        uart_print(" != ");
        print_bits(complete_bits, OUTPUTS_COUNT);
        uart_println();

        for (byte i = 0; i < OUTPUTS_COUNT; ++i) {
          if (!bitRead(ready_bits, i)) {
            str.clear();
            str.add(" ");
            str.add(i + 1);
            str.add("_");
            str.add(settings.min_duties[i]);
            cursor = typewriter_slide_set_text(mtrx, str.buf, cursor);

            apply_fan_pwm(i, settings.min_duties[i]);
            if (settings.min_duties[i] == MAX_DUTY) {
              bitSet(ready_bits, i);
            }
          }
        }
        fixed_delay(500);
        byte pre_ready_bits = ready_bits;
        for (byte i = 0; i < OUTPUTS_COUNT; ++i) {
          if (!bitRead(pre_ready_bits, i) && has_rpm(i)) {
            bitSet(pre_ready_bits, i);
          }
        }
        byte changed_bits = pre_ready_bits ^ ready_bits;
        if (changed_bits != 0) {
          stop_fans(changed_bits, true);
          fixed_delay(3000);
          for (byte i = 0; i < OUTPUTS_COUNT; ++i) {
            if (bitRead(changed_bits, i)) {
              apply_fan_pwm(i, settings.min_duties[i]);
            }
          }
          fixed_delay(3000);
          for (byte i = 0; i < OUTPUTS_COUNT; ++i) {
            if (bitRead(changed_bits, i) && has_rpm(i, 50)) {
              bitSet(ready_bits, i);

              str.clear();
              str.add(" ");
              str.add(i + 1);
              str.add("_");
              str.add(settings.min_duties[i]);
              str.add("!");
              cursor = typewriter_slide_set_text(mtrx, str.buf, cursor);

              uart_print("fan ");
              uart_print(get_out_pin(i));
              uart_print(F(" min duty "));
              uart_println(settings.min_duties[i]);
            }
          }
        }
        for (byte i = 0; i < OUTPUTS_COUNT; ++i) {
          if (!bitRead(ready_bits, i)) {
            settings.min_duties[i] += ((settings.min_duties[i] == MAX_DUTY - 2)) ? 1 : 2;
          }
        }
      }
    }
    typewriter_slide_set_text(mtrx, "ok      ", cursor);
    fixed_delay(2048);
    set_matrix_text(mtrx, "ok");
    mtrx_slide_down(mtrx, "");
    menu.time = millis();
  }

  for (byte i = 0; i < OUTPUTS_COUNT; ++i) {
    percent_2duty_cache[i][0] = settings.min_duties[i] * (settings.min_duty_percent / 100);
    for (byte p = 1; p <= 99; ++p) {
      percent_2duty_cache[i][p] = convert_by_sqrt(p, 0, 100, settings.min_duties[i], MAX_DUTY);
    }
    percent_2duty_cache[i][100] = MAX_DUTY;
  }

  for (byte i = 0; i < OUTPUTS_COUNT; ++i) {
    uart_print(F("values for "));
    uart_print(get_out_pin(i));
    uart_println(":");
    for (byte p = 0, j = 0; p <= 100; ++p, ++j) {
      if (j == 10) {
        j = 0;
        uart_println();
      }
      uart_print(p);
      uart_print("% ");
      uart_print(convert_percent_2duty(i, p));
      uart_print("\t");
    }
    uart_println();
  }
}

void apply_fan_pwm(byte index, byte duty) {
  PWM_set(get_out_pin(index), duty);
  if (is_debug) {
    uart_print(F("Fan "));
    uart_print(get_out_pin(index));
    uart_print(F(", duty "));
    uart_println(duty);
  }
}

void read_temp() {
  inputs_info.temperature.available = inputs_info.temperature.sensor.readTemp();
  if (inputs_info.temperature.available) {
    inputs_info.temperature.value = inputs_info.temperature.sensor.getTemp();
    inputs_info.pwm_percents.temperature = convert_by_sqrt(inputs_info.temperature.value, settings.min_temp, settings.max_temp, 0, 100);
    if (is_debug) {
      uart_println(inputs_info.temperature.value);
    }
  } else {
    inputs_info.temperature.value = settings.min_temp;
    inputs_info.pwm_percents.temperature = 0;
    if (is_debug) {
      uart_println("error");
    }
  }
  inputs_info.temperature.sensor.requestTemp();
}

void apply_pwm_4all(byte percent) {
  for (byte i = 0; i < OUTPUTS_COUNT; ++i) {
    apply_fan_pwm(i, convert_percent_2duty(i, percent));
  }
}

void read_pulses() {
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
  if (is_debug) {
    uart_println(inputs_info.str_pulses_values.buf);
  }

  inputs_info.pwm_percents.pulse = 0;
  for (byte input_index = 0; input_index < INPUTS_COUNT; ++input_index) {
    byte smooth_pulse = find_median<BUFFER_SIZE_FOR_SMOOTH, byte>(inputs_info.pulses_info[input_index].smooths_buffer, true);
    byte pulse = constrain(smooth_pulse, settings.min_pulses[input_index], settings.max_pulses[input_index]);
    byte pulse_2percent = map(
        pulse,
        settings.min_pulses[input_index], settings.max_pulses[input_index],
        0, 100);
    inputs_info.pwm_percents.pulse = max(inputs_info.pwm_percents.pulse, pulse_2percent);

    if (is_debug) {
      uart_print(F("input "));
      uart_print(INPUTS_PINS[input_index]);
      uart_print(F(": pulse "));
      uart_print(pulse);
      uart_print(F(" ("));
      uart_print(pulse_2percent);
      uart_print(F("%); avg smooth "));
      uart_print(smooth_pulse);
      uart_print(F(" ["));
      for (byte i = 0; i < BUFFER_SIZE_FOR_SMOOTH; ++i) {
        if (i != 0) {
          uart_print(F(", "));
        }
        if (i == inputs_info.smooth_index) {
          uart_print(F("{"));
        }
        uart_print(inputs_info.pulses_info[input_index].smooths_buffer[i]);
        if (i == inputs_info.smooth_index) {
          uart_print(F("}"));
        }
      }
      uart_println(F("]"));
    }
  }
  if (++inputs_info.smooth_index >= BUFFER_SIZE_FOR_SMOOTH) {
    inputs_info.smooth_index = 0;
  }
}
