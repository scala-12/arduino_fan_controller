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
#include <EEPROM.h>
#include <GyverPWM.h>
#include <mString.h>
#include <microDS18B20.h> /*
  использовать форк https://github.com/scala-12/microDS18B20:
  - из template аргумент DS_PIN перемещен в переменные, изменены конструкторы
    MicroDS18B20(uint8_t ds_pin, bool with_init) {
      DS_PIN = ds_pin;
      if (with_init) {
        pinMode(DS_PIN, INPUT);
        digitalWrite(DS_PIN, LOW);
      }
    }
    MicroDS18B20(uint8_t ds_pin) {
      MicroDS18B20(ds_pin, true);
    }
  - добавлен метод:
    uint8_t get_pin() {
      return DS_PIN;
    }
*/

#include "constants.h"
#include "functions.h"
#include "macros.h"

// настраиваемые параметры
const byte INPUTS_PINS[] = {A1, A2, A3};                            // пины входящих PWM
const byte OUTPUTS_PINS[][2] = {{3, 2}, {5, 4}, {9, 8}, {10, 11}};  // пины [выходящий PWM, RPM]
const byte SENSORS_PINS[] = {6, 7};                                 // пины датчиков температуры

#define COOLING_PIN A6 /* пин включения максимальной скорости */
// ^^^ настраиваемые параметры ^^^

// вычисляемые константы
const byte INPUTS_COUNT = get_arr_len(INPUTS_PINS);    // количество ШИМ входов
const byte OUTPUTS_COUNT = get_arr_len(OUTPUTS_PINS);  // количество ШИМ выходов
const byte SENSORS_COUNT = get_arr_len(SENSORS_PINS);  // количество датчиков температуры

// переменные
byte smooth_buffer[INPUTS_COUNT][BUFFER_SIZE_FOR_SMOOTH];  // буфер для сглаживания входящего сигнала
// TODO проверить нужен ли кеш или вычислять на ходу
byte percent_2duty_cache[OUTPUTS_COUNT][101];  // кеш преобразования процента скорости в PWM

MicroUART uart;          // интерфейс работы с серийным портом
byte smooth_index;       // шаг для сглаживания
bool cooling_on;         // режим максимальной скорости
uint32_t pwm_tmr;        // таймер для чтения ШИМ
mString<64> input_data;  // буфер чтения команды из серийного порта
boolean recieved_flag;   // флаг на чтение
boolean is_debug;        // флаг вывода технической информации
byte last_percent;       // последнее значение PWM для отрисовки на матрице

struct Settings {
  byte min_duties[OUTPUTS_COUNT];  // минимальный PWM для начала вращения
  byte min_pulses[INPUTS_COUNT];   // нижняя граница чувствительности к входящему PWM
  byte max_pulses[INPUTS_COUNT];   // верхняя граница чувствительности к входящему PWM
  byte max_temp;                   // верхняя граница чувсвительности температурного датчика
  byte min_temp;                   // нижняя граница чувсвительности температурного датчика
};
Settings settings;  // хранимые параметры

void init_output_params(bool is_first, bool init_rpm);
byte get_max_by_sensors(bool do_cmd_print);
byte stop_fans(byte ignored_bits, bool wait_stop);
boolean has_rpm(byte index, byte more_than_rpm = 0);
void apply_fan_pwm(byte index, byte duty);

#include "extras.h"

void MU_serialEvent() {
  // нужна для чтения буфера
}

void setup() {
  uart.begin(SERIAL_SPEED);
  uart.println("start");

  pinMode(COOLING_PIN, INPUT_PULLUP);
  for (byte i = 0; i < INPUTS_COUNT; ++i) {
    pinMode(INPUTS_PINS[i], INPUT);
    memset(smooth_buffer[i], 0, BUFFER_SIZE_FOR_SMOOTH);
  }
  for (byte i = 0; i < SENSORS_COUNT; ++i) {
    MicroDS18B20<> sensor(SENSORS_PINS[i]);
    sensor.requestTemp();
  }

  // обнуляем таймеры
  pwm_tmr = 0;

  smooth_index = 0;    // номер шага в буфере для сглаживания
  cooling_on = false;  // не режим продувки
  input_data = "";     // ощищаем буфер
  is_debug = false;    // не дебаг

  if (EEPROM.read(INIT_ADDR) != VERSION_NUMBER) {
    // если структура хранимых данных изменена, то делаем дефолт
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

    init_output_params(true, false);
  } else {
    EEPROM.get(0, settings);
    init_output_params(true, false);
  }
}

void loop() {
  read_and_exec_command(settings);

  uint32_t time = millis();
  if (cooling_on) {
    if (digitalRead(COOLING_PIN) == LOW) {
      cooling_on = false;
      uart.println("cooling OFF");
    }
  } else if (digitalRead(COOLING_PIN) == HIGH) {
    cooling_on = true;
    apply_pwm_4all(100);
    uart.println("cooling ON");
  } else if (check_diff(time, pwm_tmr, REFRESH_TIMEOUT)) {
    pwm_tmr = time;
    byte max_percent_by_pwm = get_max_percent_by_pwm();

    byte max_percent_by_sensors;
    set_max_percent_of_temp(settings, is_debug, max_percent_by_sensors);
    last_percent = max(max_percent_by_pwm, max_percent_by_sensors);

    apply_pwm_4all(last_percent);
  }
}

boolean has_rpm(byte index, byte more_than_rpm = 0) {
  uart.print("fan ");
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
    uart.print("stop fans, ");
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
    uart.print("stopped ");
  } else {
    uart.print("not stopped ");
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

void init_output_params(bool is_first, bool init_rpm) {
  if (is_first) {
    for (byte i = 0; i < OUTPUTS_COUNT; ++i) {
      pinMode(get_out_pin(i), OUTPUT);
      pinMode(get_rpm_pin(i), INPUT_PULLUP);

      PWM_frequency(get_out_pin(i), PULSE_FREQ, FAST_PWM);
    }
  }

  if (init_rpm) {
    byte start_duties[OUTPUTS_COUNT];
    // не будем искать минимальную скорость, если вентилятор не остановился или игнорируется
    byte ignored_bits = stop_fans(ignored_bits, false);
    for (byte i = 0; i < OUTPUTS_COUNT; ++i) {
      if (!bitRead(ignored_bits, i)) {
        // если остановился, то пробуем запустить
        apply_fan_pwm(i, MAX_DUTY);
      }
    }
    fixed_delay(3000);
    for (byte i = 0; i < OUTPUTS_COUNT; ++i) {
      if (!bitRead(ignored_bits, i) && !has_rpm(i)) {
        // если не запустился, то игнорируем его детальную настройку из-за отсутствия обратной связи
        bitSet(ignored_bits, i);
        settings.min_duties[i] = MIN_DUTY;
        uart.print("fan ");
        uart.print(get_out_pin(i));
        uart.println(" without RPM");
      } else {
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
        for (byte i = 0; i < OUTPUTS_COUNT; ++i) {
          if (!bitRead(ignored_bits, i)) {
            uart.print("fan ");
            uart.print(get_out_pin(i));
            uart.print(" binary search min duty [");
            uart.print(start_duties[i]);
            uart.print(", ");
            uart.print(settings.min_duties[i]);
            uart.println("]");
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
      for (byte _i = 0; _i < MAX_DUTY && (ready_bits != complete_bits); ++_i) {
        uart.print(_i);
        uart.print(", search step-by-step min duty ");
        print_bits(ready_bits, OUTPUTS_COUNT);
        uart.print(" != ");
        print_bits(complete_bits, OUTPUTS_COUNT);
        uart.println();

        for (byte i = 0; i < OUTPUTS_COUNT; ++i) {
          if (!bitRead(ready_bits, i)) {
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
              uart.print("fan ");
              uart.print(get_out_pin(i));
              uart.print(" min duty ");
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
  }

  for (byte i = 0; i < OUTPUTS_COUNT; ++i) {
    for (byte p = 0; p <= 100; ++p) {
      update_cached_duty(i, p, convert_by_sqrt(p, 0, 100, settings.min_duties[i], MAX_DUTY));
    }
  }

  for (byte i = 0; i < OUTPUTS_COUNT; ++i) {
    uart.print("values for ");
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
    uart.print("Fan ");
    uart.print(get_out_pin(index));
    uart.print(", duty ");
    uart.println(duty);
  }
}
