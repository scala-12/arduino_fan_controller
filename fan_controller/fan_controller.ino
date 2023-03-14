// коррекция времени из-за изменения частоты Timer0
#define TIME_CORRECTOR(func) (((func >> 4) * 10) >> 4) /* коррекция time/25.6 */
#define micros() TIME_CORRECTOR(micros())              /* коррекция micros */
#define millis() TIME_CORRECTOR(millis())              /* коррекция millis */
#define fixed_delay(ms)                                /* иммитация delay(ms) через цикл с корректированными функциями времени */ \
  for (uint32_t _tmr_start = millis(), _timer = 0; abs(_timer) < ms; _timer = millis() - _tmr_start) {                                                                                \
  }

// настройка UART
#define MU_RX_BUF 64 /* размер буфера */
#define MU_PRINT
#include <MicroUART.h>
// ^^^ настройка UART ^^^
#include <AnalogKey.h>
#include <EEPROM.h>
#include <EncButton2.h>
#include <GyverMAX7219.h>
#include <GyverPWM.h>
#include <mString.h>
#include <microDS18B20.h>

#include "constants.h"
#include "functions.h"
#include "macros.h"

// настраиваемые параметры
const byte INPUTS_PINS[] = {A1, A2, A3};                            // пины входящих PWM
const byte OUTPUTS_PINS[][2] = {{3, 2}, {5, 4}, {9, 8}, {10, 11}};  // пины [выходящий PWM, RPM]

int16_t buttons_map[CTRL_KEYS_COUNT] = {317, 1016, 636};  // уровни клавиш UP, SELECT, DOWN

#define TEMP_SENSOR_PIN 6    /* пин датчика температуры */
#define OPTICAL_SENSOR_PIN 7 /* пин оптического энкодера */

#define MTRX_CS_PIN 12    /*CS-пин матрицы*/
#define MTRX_CLOCK_PIN 13 /*Clk-пин матрицы*/
#define MTRX_DATA_PIN A0  /*DIn-пин матрицы*/

#define MTRX_COLUMS_COUNT 4 /*количесиво модулей матрицы в ряд*/
#define MTRX_BRIGHT 0       /*яркость матрицы [0..15]*/
// ^^^ настраиваемые параметры ^^^

// вычисляемые константы
const byte INPUTS_COUNT = get_arr_len(INPUTS_PINS);    // количество ШИМ входов
const byte OUTPUTS_COUNT = get_arr_len(OUTPUTS_PINS);  // количество ШИМ выходов

// переменные

struct {
  AnalogKey<COOLING_PIN, 1> keys;            // клавиши включения режима проветривания
  EncButton2<VIRT_BTN, EB_TICK> buttons[1];  // кнопки включения режима проветривания
} cooling_keyboard;

struct {
  AnalogKey<ANALOG_KEYS_PIN, CTRL_KEYS_COUNT, buttons_map> keys;  // клавиши управления
  EncButton2<VIRT_BTN, EB_TICK> buttons[CTRL_KEYS_COUNT];         // кнопки управления
  uint32_t time;                                                  // время окончания чтения
  bool ticks_over;                                                // ожидание нажатия закончено
  byte states[CTRL_KEYS_COUNT];                                   // состояние кнопок
} ctrl_keyboard;

struct InputsInfo {
  struct {
    byte smooths_buffer[BUFFER_SIZE_FOR_SMOOTH];  // буфер для сглаживания входящего сигнала
    byte value;                                   // последнее значение PWM для отрисовки на матрице
  } pulses_info[INPUTS_COUNT];
  byte smooth_index;                            // шаг для сглаживания
  mString<3 * INPUTS_COUNT> str_pulses_values;  // строка с значениями входящих ШИМ
  struct {
    MicroDS18B20<TEMP_SENSOR_PIN> sensor;  // датчик температуры
    byte value;                            // значения датчика температуры
    bool available;                        // датчик температуры активен
  } temperature;
  byte pwm_percent_by_pulse;                    // результат преобразования входящих ШИМ в выходной ШИМ
  byte pwm_percent_by_temp;                     // результат преобразования температуры в ШИМ
  byte pwm_percent_by_optic;                    // результат преобразования скорости вращения в ШИМ
  struct {
    byte pin;                                    // пин оптического датчика
    bool state;                                  // встречен разделитель
    int counter;                                 // количество вращений на текущий момент
    int rpm;                                     // скорость, преобразованное из количества вращений за отведенное время
    int smooths_buffer[BUFFER_SIZE_FOR_SMOOTH];  // буфер для сглаживания сигнала
  } optical;
};
InputsInfo inputs_info;

// TODO проверить нужен ли кеш или вычислять на ходу
byte percent_2duty_cache[OUTPUTS_COUNT][101];  // кеш преобразования процента скорости в PWM

MicroUART uart;         // интерфейс работы с серийным портом
bool cooling_on;        // режим максимальной скорости
uint32_t pwm_tmr;       // таймер для чтения ШИМ
mString<64> cmd_data;   // буфер чтения команды из серийного порта
boolean recieved_flag;  // флаг на чтение
boolean is_debug;       // флаг вывода технической информации

struct Settings {
  byte min_duties[OUTPUTS_COUNT];  // минимальный PWM для начала вращения
  byte min_pulses[INPUTS_COUNT];   // нижняя граница чувствительности к входящему PWM
  byte max_pulses[INPUTS_COUNT];   // верхняя граница чувствительности к входящему PWM
  byte max_temp;                   // верхняя граница чувсвительности температурного датчика
  byte min_temp;                   // нижняя граница чувсвительности температурного датчика
  int max_optic_rpm;               // верхняя граница чувсвительности оптического датчика
  int min_optic_rpm;               // нижняя граница чувсвительности оптического датчика
  bool cool_on_hold;               // состояние кнопки для активации режима продувки
  byte min_duty_percent;           // процент от минимальной скорости вращения. если меньше 100, то вентилятор может и остановиться
};
Settings settings;  // хранимые параметры

struct Max7219Matrix {
  mString<MTRX_BUFFER> data;  // буфер вывода на матрицу
  bool changed;               // флаг изменения буфера
  byte cursor;                // позиция курсора
  byte next_cursor;           // позиция курсора
  byte border_cursor;
  byte border_delay_counter;
  uint32_t time;  // время прошлого обновления отображения
  MAX7219<MTRX_COLUMS_COUNT, MTRX_ROWS_COUNT, MTRX_CS_PIN, MTRX_DATA_PIN, MTRX_CLOCK_PIN> panel;
};

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

#include "extras.h"

Max7219Matrix mtrx;

void MU_serialEvent() {
  // нужна для чтения буфера
}

void setup() {
  uart.begin(SERIAL_SPEED);
  uart.println(F("start"));

  for (byte i = 0; i < INPUTS_COUNT; ++i) {
    pinMode(INPUTS_PINS[i], INPUT);
    memset(inputs_info.pulses_info[i].smooths_buffer, 0, BUFFER_SIZE_FOR_SMOOTH);
  }

  inputs_info.temperature.sensor.requestTemp();

  cooling_keyboard.keys.attach(0, 1023);
  cooling_keyboard.keys.setWindow(200);
  for (byte i = 0; i < CTRL_KEYS_COUNT; ++i) {
    ctrl_keyboard.states[i] = 0;
    ctrl_keyboard.keys.setWindow(200);
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
  pwm_tmr = 0;
  ctrl_keyboard.time = 0;

  inputs_info.smooth_index = 0;  // номер шага в буфере для сглаживания
  cooling_on = false;            // не режим продувки
  cmd_data = "";                 // ощищаем буфер
  is_debug = false;              // не дебаг

  if (EEPROM.read(INIT_ADDR) != VERSION_NUMBER) {
    // если структура хранимых данных изменена, то делаем дефолт
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
    EEPROM.get(0, settings);
    init_output_params(true, false, mtrx);
  }
  close_menu(mtrx, menu);
}

void loop() {
  read_and_exec_command(settings, inputs_info, cmd_data, is_debug, mtrx);

  uint32_t time = millis();
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
        bitSet(ctrl_keyboard.states[i], CLICK_BIT);
        ctrl_keyboard.buttons[i].resetState();
        if (is_debug) {
          uart.print("click ");
          uart.println(i);
        }
        break;
      }
      case 6: {
        if (ctrl_keyboard.buttons[i].held(1)) {
          bitSet(ctrl_keyboard.states[i], HELD_1_BIT);
          if (is_debug) {
            uart.print("held ");
            uart.println(i);
          }
        } else if (ctrl_keyboard.buttons[i].hold(0)) {
          bitSet(ctrl_keyboard.states[i], HOLD_0_BIT);
          if (is_debug) {
            uart.print("hold ");
            uart.println(i);
          }
        }
        break;
      }
    }
    if (ctrl_keyboard.buttons[i].busy()) {
      if (ctrl_keyboard.ticks_over && !(bitRead(ctrl_keyboard.states[i], HOLD_0_BIT) || bitRead(ctrl_keyboard.states[i], HELD_1_BIT))) {
        ctrl_keyboard.ticks_over = false;
      }
    }
  }
  if (ctrl_keyboard.ticks_over && check_diff(time, ctrl_keyboard.time, MTRX_REFRESH_MS >> 1)) {
    ctrl_keyboard.time = time;

    menu_tick(settings, ctrl_keyboard.states, menu, mtrx);

    for (byte i = 0; i < CTRL_KEYS_COUNT; ++i) {
      if (i != 2 && bitRead(ctrl_keyboard.states[i], HOLD_0_BIT) && ctrl_keyboard.buttons[i].busy()) {
        ctrl_keyboard.states[i] = 0;
        bitSet(ctrl_keyboard.states[i], HOLD_0_BIT);
      } else {
        ctrl_keyboard.states[i] = 0;
      }
    }
  }

  menu_refresh(settings, inputs_info, time, mtrx, menu);
  mtrx_refresh(mtrx, time);
  byte cool_button_state = cooling_keyboard.buttons[0].tick(cooling_keyboard.keys.status(0));
  bool is_hold_button = cool_button_state == 6 || (cool_button_state == 7 && cooling_keyboard.buttons[0].busy());
  if (settings.cool_on_hold == is_hold_button) {
    if (!cooling_on) {
      cooling_on = true;
      apply_pwm_4all(100);
      uart.println(F("cooling ON"));
    }
  } else if (cooling_on) {
    cooling_on = false;
    uart.println(F("cooling OFF"));
  } else if (check_diff(time, pwm_tmr, SENSE_REFRESH_MS)) {
    pwm_tmr = time;

    inputs_info.optical.smooths_buffer[inputs_info.smooth_index] = inputs_info.optical.counter;
    inputs_info.optical.rpm = find_median<BUFFER_SIZE_FOR_SMOOTH, int>(inputs_info.optical.smooths_buffer, true) * (1000 / SENSE_REFRESH_MS) * 60;
    inputs_info.optical.counter = 0;
    inputs_info.pwm_percent_by_optic = convert_by_sqrt(inputs_info.optical.rpm, settings.min_optic_rpm, settings.max_optic_rpm, 0, 100);

    read_pulses(inputs_info, is_debug);
    read_temp();
    byte max_percent = max(inputs_info.pwm_percent_by_pulse, inputs_info.pwm_percent_by_temp);
    max_percent = max(max_percent, inputs_info.pwm_percent_by_optic);

    apply_pwm_4all(max_percent);
  }
}

boolean has_rpm(byte index, byte more_than_rpm = 0) {
  uart.print(F("fan "));
  uart.print(get_out_pin(index));

  unsigned long rpm = pulseIn(get_rpm_pin(index), HIGH, 500000);
  if (rpm == 0) {
    rpm = pulseIn(get_rpm_pin(index), LOW, 1500000);
    if (rpm == 0) {
      rpm = (digital_read_fast(get_rpm_pin(index)) == LOW) ? 1 : 0;
    }
  }

  uart.print(" ");
  uart.println(rpm);

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
    uart.print(F("stop fans, "));
    uart.print(_i);
    uart.print(": ");
    print_bits(running_bits, OUTPUTS_COUNT);
    uart.print(", ");
    print_bits(ignored_bits, OUTPUTS_COUNT);
    uart.print(", ");
    print_bits(complete_bits, OUTPUTS_COUNT);
    uart.println();
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
    uart.print(F("stopped "));
  } else {
    uart.print(F("not stopped "));
  }

  print_bits(running_bits, OUTPUTS_COUNT);
  uart.print(", ");
  print_bits(ignored_bits, OUTPUTS_COUNT);
  uart.print(", ");
  print_bits(complete_bits, OUTPUTS_COUNT);
  uart.println();
  uart.println();

  return running_bits;
}

void init_output_params(bool is_first, bool init_rpm, Max7219Matrix& mtrx) {
  if (is_first) {
    for (byte i = 0; i < OUTPUTS_COUNT; ++i) {
      pinMode(get_out_pin(i), OUTPUT);
      pinMode(get_rpm_pin(i), INPUT_PULLUP);

      PWM_frequency(get_out_pin(i), (1000000 / PULSE_WIDTH), FAST_PWM);  // (1s -> mcs) / (период шим)
    }
  }

  if (init_rpm) {
    byte start_duties[OUTPUTS_COUNT];
    // не будем искать минимальную скорость, если вентилятор не остановился или игнорируется
    int cursor = typewriter_slide_set_text(mtrx, "stop", 0, true);
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
        uart.print("fan ");
        uart.print(get_out_pin(i));
        uart.println(F(" without RPM"));
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
            uart.print("fan ");
            uart.print(get_out_pin(i));
            uart.print(F(" binary search min duty ["));
            uart.print(start_duties[i]);
            uart.print(", ");
            uart.print(settings.min_duties[i]);
            uart.println("]");

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
        uart.print(_i);
        uart.print(F(", search step-by-step min duty "));
        print_bits(ready_bits, OUTPUTS_COUNT);
        uart.print(" != ");
        print_bits(complete_bits, OUTPUTS_COUNT);
        uart.println();

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

              uart.print("fan ");
              uart.print(get_out_pin(i));
              uart.print(F(" min duty "));
              uart.println(settings.min_duties[i]);
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
    update_cached_duty(i, 0, settings.min_duties[i] * (settings.min_duty_percent / 100));
    for (byte p = 1; p <= 99; ++p) {
      update_cached_duty(i, p, convert_by_sqrt(p, 0, 100, settings.min_duties[i], MAX_DUTY));
    }
    update_cached_duty(i, 100, MAX_DUTY);
  }

  for (byte i = 0; i < OUTPUTS_COUNT; ++i) {
    uart.print(F("values for "));
    uart.print(get_out_pin(i));
    uart.println(":");
    for (byte p = 0, j = 0; p <= 100; ++p, ++j) {
      if (j == 10) {
        j = 0;
        uart.println();
      }
      uart.print(p);
      uart.print("% ");
      uart.print(get_cached_duty(i, p));
      uart.print("\t");
    }
    uart.println();
  }
}

void apply_fan_pwm(byte index, byte duty) {
  PWM_set(get_out_pin(index), duty);
  if (is_debug) {
    uart.print(F("Fan "));
    uart.print(get_out_pin(index));
    uart.print(F(", duty "));
    uart.println(duty);
  }
}

void read_temp() {
  inputs_info.temperature.available = inputs_info.temperature.sensor.readTemp();
  if (inputs_info.temperature.available) {
    inputs_info.temperature.value = inputs_info.temperature.sensor.getTemp();
    inputs_info.pwm_percent_by_temp = convert_by_sqrt(inputs_info.temperature.value, settings.min_temp, settings.max_temp, 0, 100);
    if (is_debug) {
      uart.println(inputs_info.temperature.value);
    }
  } else {
    inputs_info.temperature.value = settings.min_temp;
    inputs_info.pwm_percent_by_temp = 0;
    if (is_debug) {
      uart.println("error");
    }
  }
  inputs_info.temperature.sensor.requestTemp();
}
