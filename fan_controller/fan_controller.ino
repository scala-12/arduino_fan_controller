#include <TimerOne.h>  // для управления ШИМ; может выдавать ошибку в заивисимости WProgram.h

#include "extra_functions.h"

// #define DEBUG_MODE_ON
// #define DEBUG_OUTPUT
// #define DEBUG_INPUT

// #define USE_ONLY_ONE_OUTPUT // использовать только первый выход; второй не используется. Режим USE_MAX_PERCENT принудительно
#define USE_MAX_PERCENT       // вычисленные сигналы сравниваются и выбирается максимальный на обе группы
// #define USE_MAX_AND_AVERAGED  // минимальный сигнал заменяется на средний, сигналы отправляются на соответствующие группы
// #define MIN_SPEED_ONLY  // минимальный сигнал заменяется на средний, сигналы отправляются на соответствующие группы
#define WAKE_UP_PERCENT 75 // процент для выбора DUTY из диапазона[min_duty..MAX_DUTY]
#define MANUAL_SETTINGS  // для работы необходимо задать значения MIN_PULSE и MIN_DUTY ниже
#define MIN_PULSE_1  // 0..(FAN_PERIOD-1)
#define MIN_PULSE_2  // 0..(FAN_PERIOD-1)
#define MIN_DUTY_1   // 0..(MAX_DUTY-1)
#define MIN_DUTY_2   // 0..(MAX_DUTY-1)
#define PULSE_SENSE_WIDTH 4  // из множества {4, 5, 8, 10}


#ifdef USE_ONLY_ONE_OUTPUT
  #define USE_MAX_PERCENT
  #ifdef MIN_DUTY_1
    #define MIN_DUTY_2 MIN_DUTY_1
  #endif
#endif
#ifdef USE_MAX_PERCENT
  #undef USE_MAX_AND_AVERAGED
#endif
#ifdef DEBUG_MODE_ON
  #define DEBUG_OUTPUT
  #define DEBUG_INPUT
#endif

enum SpeedMode {  // тип выбора значения PWM
  INPUT_IMPULSE,  // на основе входных PWM
  MAX_ALWAYS,     // максимально значение выходного PWM; входные сигналы не используются
  MIN_ALWAYS,     // минимально значение выходного PWM; входные сигналы не используются
};

#define MAX_DUTY 1023  // максимальное значение заполнения
#define FAN_PERIOD 40  // период сигнала в микросекундах
#define BUFFER_SIZE 5  // размер буфера
#define current_tact_index ((is_odd_tact) ? 0 : 1)
#define current_ctrl ctrls[current_tact_index]

// пины режима скорости
struct {
  byte speed_by_input_pin;
  byte min_speed_pin;
} SPEED_PINS = {4, 5};

SpeedMode speed_mode;  // режим скорости
bool is_odd_tact;      // такт измерений
struct {
  unsigned long speed_mode; // счетчик такта проверки режимов
  unsigned long pwm_read;  // счетчик для такта чтения входного PWM
} tack_counter = {0, 0};

struct SignalController {
  byte pwm_in_pin;   // пин входящего PWM
  byte pwm_out_pin;  // пин выходящего PWM
  byte duty_pin;     // пин настройки минимального значения DUTY через энкодер
  byte pulse_pin;    // пин настройки минимального значения PULSE через энкодер
  byte buffer_step;  // шаг в буффере; буфер записывается циклически
  byte filtered_pulse;  // среднее значение PULSE, вычисленное по медиане
  byte pulses_buffer[BUFFER_SIZE];  // буффер для вычисления FILTERED_PULSE
  int pulse_2duty[FAN_PERIOD + 1];  // кеш; соответствие PULSE->DUTY (пример: 0..40->0..1023)
  int min_duty;                     // минимальное значение DUTY; меньше этого значения PWM быть не может
  struct {
    int duty;     // импульс для пробуждения вентилятора
    unsigned long counter;  // отчитывание тактов для пробуждения
    unsigned long repeats;  // количество тактов для пробуждения
  } wake_up;
  byte min_pulse;                   // минимальное значение PULSE; если входящий PWM меньше, то ему будет назначено MIN-DUTY
  byte pulse_2percent[FAN_PERIOD + 1]; // кеш; соответствие PULSE->DUTY_PERCENT (пример: 0..40->0..100)
  byte percent_2pulse[101];            // кеш; соответствие DUTY_PERCENT->PULSE (пример: 0..100->0..40)
};

// информация о сигналах
SignalController ctrls[2];

#define get_percent(controller) controller.pulse_2percent[controller.filtered_pulse]
#define get_duty_by_percent(controller, percent) controller.pulse_2duty[controller.percent_2pulse[percent]]
void apply_pwm_by_tact(byte percent_1, byte percent_2);
byte update_pulse_in_by_tact();

SpeedMode read_speed_mode();
void calculate_caches(byte ctrl_index);  // вычисление переменных для работы с DUTY и PULSE
void refresh_caches(byte ctrl_index);
byte update_pulse_in(byte ctrl_index);
void set_min_values(byte ctrl_index, int min_duty, byte min_pulse);  // установить минимальные значения DUTY и PULSE
SignalController init_controller(
    byte ctrl_index, byte pwm_in_pin, byte pwm_out_pin,
    byte duty_pin, byte pulse_pin,
    int min_duty, byte min_pulse
);

void setup() {
  Timer1.initialize(FAN_PERIOD);  // частота ШИМ ~25 кГц
  Serial.begin(9600);

  SignalController ctrl_1 = init_controller(
      0, 2, 9, A0, A2,
#ifdef MANUAL_SETTINGS
      MIN_DUTY_1, MIN_PULSE_1
#else
      MAX_DUTY - 1, FAN_PERIOD - 1
#endif
  );
  SignalController ctrl_2 = init_controller(
      1, 3,
#ifdef USE_ONLY_ONE_OUTPUT
      9,
#else
      10,
#endif
      A1, A3,
#ifdef MANUAL_SETTINGS
      MIN_DUTY_2, MIN_PULSE_2
#else
      MAX_DUTY - 1, FAN_PERIOD - 1
#endif
  );

  pinMode(ctrl_1.duty_pin, INPUT);
  pinMode(ctrl_2.duty_pin, INPUT);
  pinMode(ctrl_1.pwm_in_pin, INPUT);
  pinMode(ctrl_2.pwm_in_pin, INPUT);
  pinMode(ctrl_1.pwm_out_pin, OUTPUT);
  pinMode(ctrl_2.pwm_out_pin, OUTPUT);
  pinMode(SPEED_PINS.min_speed_pin, INPUT_PULLUP);
  pinMode(SPEED_PINS.speed_by_input_pin, INPUT_PULLUP);

  is_odd_tact = false;

  speed_mode = read_speed_mode();

  calculate_caches(0);
  calculate_caches(1);

  update_pulse_in(0);
  update_pulse_in(1);

  for (int j = 0; j < 2; ++j) {
    for (int i = 0; i < 100; ++i) {
      Serial.print(i);
      Serial.print("%>");
      Serial.print(ctrls[j].percent_2pulse[i]);
      if ((i + 1) % 10 == 0) {
        Serial.println();
      } else {
        Serial.print("\t");
      }
    }
  }
}

void loop() {
  ++tack_counter.pwm_read;
  ++tack_counter.speed_mode;
  is_odd_tact = !is_odd_tact;
  if (tack_counter.speed_mode >= 100019) {
    tack_counter.speed_mode = 0;
    speed_mode = read_speed_mode();
  }

#ifndef MANUAL_SETTINGS
  refresh_caches(current_tact_index);
#endif

  byte percent_1;
  byte percent_2;
  if (speed_mode == SpeedMode::INPUT_IMPULSE) {
#ifdef DEBUG_MODE_ON
    Serial.println("speed mode: input impulse");
#endif
    if (tack_counter.pwm_read >= 72547) {
      tack_counter.pwm_read = 0;
      update_pulse_in_by_tact();
    }
    percent_1 = get_percent(ctrls[0]);
    percent_2 = get_percent(ctrls[1]);
#ifdef USE_MAX_PERCENT
    if (percent_1 > percent_2) {
      percent_2 = percent_1;
    } else {
      percent_1 = percent_2;
    }
  #ifdef DEBUG_MODE_ON
    Serial.print("mutation mode: MAX_PERCENT (pulse ");
    Serial.print(percent_1);
    Serial.println("%)");
  #endif
#elif defined(USE_MAX_AND_AVERAGED)
  #ifdef DEBUG_MODE_ON
    Serial.print("mutation mode: avaraged (");
    Serial.print(percent_1);
    Serial.print("%\t");
    Serial.print(percent_2);
    Serial.print("%)\t->\t(");
  #endif
    if (percent_1 > percent_2) {
      percent_2 = (percent_1 + percent_2) / 2;
    } else {
      percent_1 = (percent_1 + percent_2) / 2;
    }
  #ifdef DEBUG_MODE_ON
    Serial.print(percent_1);
    Serial.print("%\t");
    Serial.print(percent_2);
    Serial.println("%)");
  #endif
#else
    Serial.println("mutation mode: ERROR, immutable");
#endif
  } else {
#ifdef DEBUG_MODE_ON
    Serial.print("speed mode: ");
    Serial.println((speed_mode == SpeedMode::MAX_ALWAYS) ? "MAX_SPEED" : "MIN_SPEED");
#endif
    percent_1 = (speed_mode == SpeedMode::MAX_ALWAYS) ? 100 : 0;
    percent_2 = percent_1;
  }

  apply_pwm_by_tact(percent_1, percent_2);
}

void set_min_values(byte ctrl_index, int min_duty, byte min_pulse) {
  ctrls[ctrl_index].min_duty = min_duty;
  ctrls[ctrl_index].wake_up.duty = map(WAKE_UP_PERCENT, 0, 100, min_duty, MAX_DUTY);
  ctrls[ctrl_index].min_pulse = min_pulse;
}

void refresh_caches(byte ctrl_index) {
#ifdef MANUAL_SETTINGS
  calculate_caches(ctrl_index);
#else
  int duty_pin_value = analogRead(ctrls[ctrl_index].duty_pin);
  int min_duty = min(MAX_DUTY - 1, abs(duty_pin_value));
  int pulse_pin_value = analogRead(ctrls[ctrl_index].pulse_pin);
  byte min_pulse = min(
      FAN_PERIOD - 1,
      map(
          min(MAX_DUTY, abs(pulse_pin_value)),
          0, MAX_DUTY,
          0, FAN_PERIOD));
  if ((abs(min_duty - ctrls[ctrl_index].min_duty) > 15) || (abs(min_pulse - ctrls[ctrl_index].min_pulse) > 1)) {
    Serial.print(ctrls[ctrl_index].pulse_pin);
    Serial.print(": pulse ");
    Serial.print(min_pulse);
    Serial.print("(");
    Serial.print(ctrls[ctrl_index].min_pulse);
    Serial.print("), duty ");
    Serial.print(min_duty);
    Serial.print("(");
    Serial.print(ctrls[ctrl_index].min_duty);
    Serial.print(")");
    Serial.println();

    set_min_values(ctrl_index, min_duty, min_pulse);
    calculate_caches(ctrl_index);
  }
#endif
}

void calculate_caches(byte ctrl_index) {
  // TODO: оптимизировать
  int prev_percent = -1;
  int min_percent = -1;
  for (byte i = 0; i <= ctrls[ctrl_index].min_pulse; ++i) {
    ctrls[ctrl_index].pulse_2duty[i] = ctrls[ctrl_index].min_duty;
    ctrls[ctrl_index].pulse_2percent[i] = 0;
    if (prev_percent != -1) {
      prev_percent = 0;
      min_percent = 0;
    }
  }
  ctrls[ctrl_index].percent_2pulse[0] = ctrls[ctrl_index].min_pulse;
  for (byte i = ctrls[ctrl_index].min_pulse + 1; i <= FAN_PERIOD; ++i) {
    byte pulse_multiplier = (i + PULSE_SENSE_WIDTH) / PULSE_SENSE_WIDTH;
    byte pulse_value = min(pulse_multiplier * PULSE_SENSE_WIDTH, FAN_PERIOD);
    int duty = map(
        pulse_value,
        ctrls[ctrl_index].min_pulse, FAN_PERIOD,
        ctrls[ctrl_index].min_duty, MAX_DUTY);
    byte percent = map(
        pulse_value,
        ctrls[ctrl_index].min_pulse, FAN_PERIOD,
        0, 100);
    if (prev_percent != -1) {
      for (byte j = prev_percent + 1;
            j <= percent;
            ++j) {
        ctrls[ctrl_index].percent_2pulse[j] = pulse_value;
      }
    }
    ctrls[ctrl_index].pulse_2duty[i] = duty;
    ctrls[ctrl_index].pulse_2percent[i] = percent;
    if (prev_percent != percent) {
      prev_percent = percent;
    }
    if ((min_percent == -1) && (i == ctrls[ctrl_index].min_pulse)) {
      min_percent = percent;
    }
  }
  if (min_percent != -1) {
    for (byte j = 0; j <= min_percent; ++j) {
      ctrls[ctrl_index].percent_2pulse[j] = ctrls[ctrl_index].min_pulse;
    }
  }

  for (int i = 0; i < 100; ++i) {
    Serial.print(i);
    Serial.print("%>");
    Serial.print(ctrls[ctrl_index].percent_2pulse[i]);
    if ((i + 1) % 10 == 0) {
      Serial.println();
    } else {
    Serial.print("\t");
    }
  }
}

SignalController init_controller(
    byte ctrl_index,
    byte pwm_in_pin, byte pwm_out_pin,
    byte duty_pin, byte pulse_pin,
    int min_duty, byte min_pulse
) {
  ctrls[ctrl_index].pwm_in_pin = pwm_in_pin;
  ctrls[ctrl_index].pwm_out_pin = pwm_out_pin;
  ctrls[ctrl_index].duty_pin = duty_pin;
  ctrls[ctrl_index].pulse_pin = pulse_pin;
  ctrls[ctrl_index].buffer_step = 0;
  ctrls[ctrl_index].filtered_pulse = FAN_PERIOD;
  ctrls[ctrl_index].pulse_2duty[FAN_PERIOD + 1];
  set_min_values(ctrl_index, min_duty, min_pulse);
  ctrls[ctrl_index].wake_up.counter = 0;
  ctrls[ctrl_index].wake_up.repeats = 0;

  for (byte i = 0; i < BUFFER_SIZE; ++i) {
    ctrls[ctrl_index].pulses_buffer[i] = FAN_PERIOD;
  }
  for (byte i = 0; i <= FAN_PERIOD; ++i) {
    ctrls[ctrl_index].pulse_2percent[i] = 100;
    ctrls[ctrl_index].pulse_2duty[i] = MAX_DUTY;
  }
  for (byte i = 0; i <= 100; ++i) {
    ctrls[ctrl_index].percent_2pulse[i] = FAN_PERIOD;
  }

  return ctrls[ctrl_index];
}

byte update_pulse_in(byte ctrl_index) {
  byte pulse;
  if (digitalRead(ctrls[ctrl_index].pwm_in_pin) == LOW) {
    pulse = pulseIn(ctrls[ctrl_index].pwm_in_pin, HIGH, 100);
  } else {
    pulse = FAN_PERIOD - pulseIn(ctrls[ctrl_index].pwm_in_pin, LOW, 100);
  }

  ctrls[ctrl_index].pulses_buffer[ctrls[ctrl_index].buffer_step] = min(pulse, FAN_PERIOD);

  if (++ctrls[ctrl_index].buffer_step >= BUFFER_SIZE) {
    ctrls[ctrl_index].buffer_step = 0;
  }

  // возможно, хватит и использования по трем значениям
  ctrls[ctrl_index].filtered_pulse = median_filter5(pulse, ctrls[ctrl_index].pulses_buffer);

  return pulse;
}

void apply_pwm_by_tact(byte percent_1, byte percent_2) {
  int duty = get_duty_by_percent(current_ctrl, (is_odd_tact) ? percent_1 : percent_2);
  if (current_ctrl.wake_up.counter > 1000003) {
    duty = max(current_ctrl.wake_up.duty, duty);
    ++current_ctrl.wake_up.repeats;
    if (current_ctrl.wake_up.repeats > 10007) {
      current_ctrl.wake_up.repeats = 0;
      current_ctrl.wake_up.counter = 0;
    }
  } else {
    ++current_ctrl.wake_up.counter;
  }
  Timer1.pwm(current_ctrl.pwm_out_pin, duty);
#ifdef DEBUG_OUTPUT
  Serial.print("out_");
  Serial.print(current_tact_index);
  Serial.print(":\t");
  Serial.print(duty);
  Serial.print("(");
  Serial.print((is_odd_tact) ? percent_1 : percent_2);
  Serial.print("%)");
  Serial.println();
#endif
}

byte update_pulse_in_by_tact() {
  byte pulse = update_pulse_in(current_tact_index);
#ifdef DEBUG_INPUT
  Serial.print("in_");
  Serial.print(current_tact_index);
  Serial.print(": ");
  Serial.print(pulse);
  Serial.print(" (");
  Serial.print(current_ctrl.filtered_pulse);
  Serial.println(")");
#endif
  return pulse;
}

SpeedMode read_speed_mode() {
#ifdef MIN_SPEED_ONLY
    return SpeedMode::MIN_ALWAYS;
#else
  if (digitalRead(SPEED_PINS.speed_by_input_pin) == LOW) {
    return SpeedMode::INPUT_IMPULSE;
  }
  if (digitalRead(SPEED_PINS.min_speed_pin) == LOW) {
    return SpeedMode::MIN_ALWAYS;
  }
  return SpeedMode::MAX_ALWAYS;
#endif
}
