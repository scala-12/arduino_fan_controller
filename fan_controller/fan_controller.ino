// коррекция времени из-за изменения частоты Timer0
#define TIME_CORRECTOR(func) (((func >> 4) * 10) >> 4) /* коррекция time/25.6 */
#define micros() TIME_CORRECTOR(micros())              /* коррекция micros */
#define millis() TIME_CORRECTOR(millis())              /* коррекция millis */
#define fixed_delay(ms)                                /* иммитация delay(ms) через цикл с корректированными функциями времени */ \
  for (uint32_t _tmr_start = millis(), _timer = 0; abs(_timer) < ms; _timer = millis() - _tmr_start) {                                                                                \
  }

#include <EEPROM.h>
#include <GyverPWM.h>
#include <mString.h>
#include <microDS18B20.h>

#include "extra_functions.h"

// системные переменные, нельзя менять
// настройка UART
#define MU_RX_BUF 64 /* размер буфера */
#define MU_PRINT
#include <MicroUART.h>
// ^^^ настройка UART ^^^
MicroUART uart;                            /* интерфейс работы с серийным портом */
#define MAX_DUTY 254                       /* максимальное значение заполнения */
#define MIN_DUTY 1                         /* минимальное значение заполнения; если диапазон более 254 значений, то PWM не реагирует на изменения */
#define PWM_RES 8                          /* битность PWM */
#define PULSE_WIDTH 40                     /* период сигнала в микросекундах */
#define PULSE_FREQ (1000000 / PULSE_WIDTH) /* частота сигнала */
#define BUFFER_SIZE_ON_READ 31             /* размер буфера на чтение PWM сигнала */
#define BUFFER_SIZE_FOR_SMOOTH 5           /* размер буфера для сглаживания входящего сигнала */
#define DEFAULT_MIN_PERCENT 30             /* нижняя граница чувствительности к входящему PWM */
#define DEFAULT_MAX_PERCENT 65             /* верхняя граница чувствительности к входящему PWM */
#define DEFAULT_MIN_TEMP 28                /* нижняя граница чувсвительности температурного датчика */
#define DEFAULT_MAX_TEMP 38                /* верхняя граница чувсвительности температурного датчика */
#define SERIAL_SPEED 115200                /* скорость серийного порта */
#define REFRESH_TIMEOUT 3000               /* таймаут чтения входящего ЩИМ */
#define VERSION_NUMBER 4                   /* версия структуры данных, хранящихся в памяти */
#define INIT_ADDR 1023                     /* ячейка памяти с информацией о структуре хранящихся данных */
#define PULSE_AVG_POWER 1                  /* радиус усреднения медианны для входящего сигнала */

#define READ_PULSE_COMMAND "read_pulses"
#define READ_TEMPS_COMMAND "read_temps"
#define SET_MIN_TEMP_COMMAND "set_min_temp"
#define SET_MAX_TEMP_COMMAND "set_max_temp"
#define GET_MIN_TEMP_COMMAND "get_min_temp"
#define GET_MAX_TEMP_COMMAND "get_max_temp"
#define GET_MIN_PULSES_COMMAND "get_min_pulses"
#define GET_MAX_PULSES_COMMAND "get_max_pulses"
#define SAVE_PARAMS_COMMAND "save_params"
#define SET_MAX_PULSE_COMMAND "set_max_pulse"
#define SET_MIN_PULSE_COMMAND "set_min_pulse"
#define RESET_MIN_DUTIES_COMMAND "reset_min_duties"
#define SET_MIN_DUTY_COMMAND "set_min_duty"
#define GET_MIN_DUTIES_COMMAND "get_min_duties"
#define SWITCH_DEBUG_COMMAND "switch_debug"
// ^^^ системные переменные, нельзя менять ^^^

#define INPUTS_COUNT 3 /*количество ШИМ входов*/
#define INPUTS_INFO \
  { A1, A2, A3 } /*пины входящих ШИМ*/

#define OUTPUTS_COUNT 4 /*количество ШИМ выходов*/
// [параметр][номер выхода]. параметры: pwm_pin, rpm_pin
#define OUTPUTS_INFO               \
  {                                \
    {3, 9, 10, 5}, { 2, 11, 6, 4 } \
  }

#define COOLING_PIN A0         /* пин включения максимальной скорости */
#define RESET_DUTY_CACHE_PIN 7 /* пин hard-reset */
// ^^^ настраиваемые параметры ^^^

// переменные
bool cooling_on;         // режим максимальной скорости
uint32_t pwm_tmr;        // таймер для чтения ШИМ
uint32_t tmr_100;        // таймер раз в 100мс
byte reset_counter;      // счетчик для hard-reset
mString<64> input_data;  // буфер чтения команды из серийного порта
boolean recieved_flag;   // флаг на чтение
boolean is_debug;        // флаг вывода технической информации

struct InputParams {
  byte pwm_pin;  // пин входящего ШИМ сигнала
  byte pulse;    // текущий PWM
};

struct OutputParams {
  byte pwm_pin;                   // пин выходного ШИМ сигнала
  byte rpm_pin;                   // пин тахометра
  byte percent_2duty_cache[101];  // кеш преобразования процента в скважность
};


struct {                           // хранимые параметры
  byte min_duties[OUTPUTS_COUNT];  // минимальный PWM для начала вращения
  byte min_pulses[INPUTS_COUNT];   // нижняя граница чувствительности к входящему PWM
  byte max_pulses[INPUTS_COUNT];   // верхняя граница чувствительности к входящему PWM
  byte max_temp;                   // верхняя граница чувсвительности температурного датчика
  byte min_temp;                   // нижняя граница чувсвительности температурного датчика
};

// информация о сигналах
InputParams inputs[INPUTS_COUNT];
OutputParams outputs[OUTPUTS_COUNT];
// TODO сделать форк чтобы использовать в массиве
MicroDS18B20<8> sensor_1;
MicroDS18B20<12> sensor_2;

Settings settings;

void init_input_params();
void init_output_params(bool is_first, bool init_rpm);
byte get_max_percent_by_pwm();
byte get_max_by_sensors(bool show_temp);
byte stop_fans(byte ignored_bits, bool wait_stop);
boolean has_rpm(byte index);
boolean has_rpm(byte index, byte more_than_rpm);
void init_sensors();

#define convert_percent_2pulse(percent) map(percent, 0, 100, 0, PULSE_WIDTH)
#define apply_fan_pwm(index, duty)       \
  PWM_set(outputs[index].pwm_pin, duty); \
  if (is_debug) {                        \
    uart.print("Fan ");                  \
    uart.print(outputs[index].pwm_pin);  \
    uart.print(", duty ");               \
    uart.println(duty);                  \
  }
#define apply_pwm_4all(percent)                                \
  uart.print("pwm percent:");                                  \
  uart.println(percent);                                       \
  for (byte i = 0; i < OUTPUTS_COUNT; ++i) {                   \
    apply_fan_pwm(i, outputs[i].percent_2duty_cache[percent]); \
  }

void MU_serialEvent() {
  // нужна для чтения буфера
}

void setup() {
  uart.begin(SERIAL_SPEED);
  uart.println("start");

  pinMode(COOLING_PIN, INPUT_PULLUP);
  pinMode(RESET_DUTY_CACHE_PIN, INPUT_PULLUP);

  // обнуляем таймеры
  pwm_tmr = 0;
  tmr_100 = 0;

  reset_counter = 0;   // счетчик продолжительности нажатия кнопки hard-reset
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

    init_output_params(true, true);
    EEPROM.put(0, settings);
    EEPROM.write(INIT_ADDR, VERSION_NUMBER);
  } else {
    EEPROM.get(0, settings);
    init_output_params(true, false);
  }
  init_input_params();
  init_sensors();
}

void loop() {
  while (uart.available() > 0) {
    // чтение команды с серийного порта
    input_data.add((char)uart.read());
    recieved_flag = true;
    fixed_delay(20);
  }
  if (recieved_flag && input_data.length() > 3) {
    if (input_data.startsWith(READ_PULSE_COMMAND)) {
      uart.print(READ_PULSE_COMMAND);
      uart.println(": ");
      for (byte i = 0; i < INPUTS_COUNT; ++i) {
        uart.print(i);
        uart.print(" ");
        uart.println(inputs[i].pulse);
      }
    } else if (input_data.startsWith(READ_TEMPS_COMMAND)) {
      get_max_by_sensors(true);
    } else if (input_data.startsWith(GET_MIN_TEMP_COMMAND) || input_data.startsWith(GET_MAX_TEMP_COMMAND)) {
      bool is_min_temp = input_data.startsWith(GET_MIN_TEMP_COMMAND);
      uart.print(input_data.buf);
      uart.print(": ");
      uart.println((is_min_temp) ? settings.min_temp : settings.max_temp);
    } else if (input_data.startsWith(GET_MIN_DUTIES_COMMAND)) {
      uart.print(GET_MIN_DUTIES_COMMAND);
      uart.println(": ");
      for (byte i = 0; i < OUTPUTS_COUNT; ++i) {
        uart.print(i);
        uart.print(" ");
        uart.println(settings.min_duties[i]);
      }
    } else if (input_data.startsWith(SAVE_PARAMS_COMMAND)) {
      uart.print(SAVE_PARAMS_COMMAND);
      uart.print(": complete");
      EEPROM.put(0, settings);
    } else if (input_data.startsWith(RESET_MIN_DUTIES_COMMAND)) {
      init_output_params(false, true);
    } else if (input_data.startsWith(SWITCH_DEBUG_COMMAND)) {
      is_debug = !is_debug;
      uart.print("Debug mode ");
      uart.println((is_debug) ? "ON" : "OFF");
    } else if (input_data.startsWith(GET_MIN_PULSES_COMMAND) || input_data.startsWith(GET_MAX_PULSES_COMMAND)) {
      bool is_min_pulse = input_data.startsWith(GET_MIN_PULSES_COMMAND);
      uart.print(input_data.buf);
      uart.println(": ");
      for (byte i = 0; i < INPUTS_COUNT; ++i) {
        uart.print(i);
        uart.print(" ");
        uart.println(((is_min_pulse) ? settings.min_pulses : settings.max_pulses)[i]);
      }
    } else if ((input_data.startsWith(SET_MIN_TEMP_COMMAND)) || input_data.startsWith(SET_MAX_TEMP_COMMAND)) {
      bool is_max_temp = input_data.startsWith(SET_MAX_TEMP_COMMAND);
      uart.print((is_max_temp) ? SET_MAX_TEMP_COMMAND : SET_MIN_TEMP_COMMAND);
      uart.println(": ");
      bool complete = false;
      char* params[2];
      byte split_count = input_data.split(params, ' ');
      byte sensor_index;
      byte temp_value;
      if (split_count >= 2) {
        mString<8> param;
        add_chars_to_mstring(param, params[2]);
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
      if (complete) {
        uart.print("Sensor ");
        uart.print(sensor_index);
        uart.print(" value ");
        uart.println(temp_value);
      } else {
        uart.println("error");
      }
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
        add_chars_to_mstring(param, params[1]);
        output_index = param.toInt();
        param.clear();
        add_chars_to_mstring(param, params[2]);
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
        add_chars_to_mstring(param, params[1]);
        input_index = param.toInt();
        param.clear();
        add_chars_to_mstring(param, params[2]);
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

  uint32_t time = millis();
  if (abs(time - tmr_100) >= 1000) {
    tmr_100 = time;

    if ((reset_counter != 0) && digital_read_fast(RESET_DUTY_CACHE_PIN) == HIGH) {
      reset_counter = 0;
    }
  } else {
    if (digital_read_fast(RESET_DUTY_CACHE_PIN) == LOW) {
      if (reset_counter == 255) {
        init_output_params(false, true);
      } else {
        ++reset_counter;
      }
    }
  }

  if (cooling_on) {
    if (digitalRead(COOLING_PIN) == LOW) {
      cooling_on = false;
      uart.println("cooling OFF");
    }
  } else if (digitalRead(COOLING_PIN) == HIGH) {
    cooling_on = true;
    apply_pwm_4all(100);
    uart.println("cooling ON");
  } else if (abs(time - pwm_tmr) >= REFRESH_TIMEOUT) {
    pwm_tmr = time;
    byte max_percent_by_pwm = get_max_percent_by_pwm();
    byte max_percent_by_sensors = get_max_by_sensors(is_debug);
    byte percent = max(max_percent_by_pwm, max_percent_by_sensors);

    apply_pwm_4all(percent);
  }
}

void init_input_params() {
  byte inputs_info[INPUTS_COUNT] = INPUTS_INFO;
  for (byte i = 0; i < INPUTS_COUNT; ++i) {
    inputs[i].pwm_pin = inputs_info[i];

    pinMode(inputs[i].pwm_pin, INPUT);

    inputs[i].pulse = 0;
  }
}

boolean has_rpm(byte index, byte more_than_rpm) {
  uart.print("fan ");
  uart.print(outputs[index].pwm_pin);

  unsigned long rpm = pulseIn(outputs[index].rpm_pin, HIGH, 500000);
  if (rpm == 0) {
    rpm = pulseIn(outputs[index].rpm_pin, LOW, 1500000);
    if (rpm == 0) {
      rpm = (digital_read_fast(outputs[index].rpm_pin) == LOW) ? 1 : 0;
    }
  }

  uart.print(" ");
  uart.println(rpm);

  return rpm > more_than_rpm;
}

boolean has_rpm(byte index) {
  return has_rpm(index, 0);
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
    byte outputs_info[][OUTPUTS_COUNT] = OUTPUTS_INFO;
    for (byte i = 0; i < OUTPUTS_COUNT; ++i) {
      outputs[i].pwm_pin = outputs_info[0][i];
      outputs[i].rpm_pin = outputs_info[1][i];

      pinMode(outputs[i].pwm_pin, OUTPUT);
      pinMode(outputs[i].rpm_pin, INPUT_PULLUP);

      PWM_frequency(outputs[i].pwm_pin, PULSE_FREQ, FAST_PWM);
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
        uart.print(outputs[i].pwm_pin);
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
            uart.print(outputs[i].pwm_pin);
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
              uart.print(outputs[i].pwm_pin);
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
      outputs[i].percent_2duty_cache[p] = convert_by_sqrt(p, 0, 100, settings.min_duties[i], MAX_DUTY);
    }
  }

  uart.println("Outputs created");
  for (byte i = 0; i < OUTPUTS_COUNT; ++i) {
    uart.print("values for ");
    uart.print(outputs[i].pwm_pin);
    uart.println(":");
    for (byte p = 0, j = 0; p <= 100; ++p, ++j) {
      if (j == 10) {
        j = 0;
        uart.println();
      }
      uart.print(p);
      uart.print("% ");
      uart.print(outputs[i].percent_2duty_cache[p]);
      uart.print("\t");
    }
    uart.println();
  }
}

byte get_max_percent_by_pwm() {
  byte buffer[BUFFER_SIZE];
  byte last_buffer_index = BUFFER_SIZE - 1;
  byte percent = 0;
  for (byte input_index = 0; input_index < INPUTS_COUNT; ++input_index) {
    for (byte i = 0; i < last_buffer_index; ++i) {
      buffer[i] = pulseIn(inputs[input_index].pwm_pin, HIGH, (PULSE_WIDTH << 1));
      if ((buffer[i] == 0) && (digital_read_fast(inputs[input_index].pwm_pin) == HIGH)) {
        buffer[i] = PULSE_WIDTH;
      } else {
        buffer[i] = constrain(buffer[i], 0, PULSE_WIDTH);
      }
    }
    buffer[last_buffer_index] = inputs[input_index].pulse;
    inputs[input_index].pulse = find_median<BUFFER_SIZE>(buffer, PULSE_AVG_POWER, true);

    byte pulse = constrain(inputs[input_index].pulse, settings.min_pulses[input_index], settings.max_pulses[input_index]);
    byte pulse_2percent = map(
        pulse,
        settings.min_pulses[input_index], settings.max_pulses[input_index],
        0, 100);
    percent = max(percent, pulse_2percent);

    if (is_debug) {
      uart.print("input ");
      uart.print(inputs[input_index].pwm_pin);
      uart.print(": ");
      uart.print(inputs[input_index].pulse);
      uart.print("(");
      uart.print(pulse);
      uart.print(", ");
      uart.print(pulse_2percent);
      uart.print("%), from [");
      for (byte i = 0; i < BUFFER_SIZE; ++i) {
        if (i != 0) {
          uart.print(", ");
        }
        uart.print(buffer[i]);
      }
      uart.println("]");
    }
  }

  return percent;
}

void init_sensors() {
  sensor_1.requestTemp();
  sensor_2.requestTemp();
}

byte get_max_by_sensors(bool show_temp) {
  byte max_temp = settings.min_temp;
  if (show_temp) uart.print("sensor_1 temp: ");
  if (sensor_1.readTemp()) {
    max_temp = sensor_1.getTemp();
    if (show_temp) uart.println(max_temp);
  } else {
    if (show_temp) uart.println("error");
  }

  if (show_temp) uart.print("sensor_2 temp: ");
  if (sensor_2.readTemp()) {
    byte temp = sensor_2.getTemp();
    max_temp = max(max_temp, temp);
    if (show_temp) uart.println(temp);
  } else {
    if (show_temp) uart.println("error");
  }
  init_sensors();

  return convert_by_sqrt(max_temp, settings.min_temp, settings.max_temp, 0, 100);
}
