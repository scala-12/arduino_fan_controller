#include <TimerOne.h>  // для управления ШИМ; может выдавать ошибку в заивисимости WProgram.h

#include "extra_functions.h"

// #define DEBUG_MODE_ON

// вариант вычисления выходных PWM из входных PWM (для SpeedMode::INPUT_IMPULSE)
// выбрать только один
// #define USE_MAX_PERCENT       // вычисленные сигналы сравниваются и выбирается максимальный на обе группы
// #define USE_MAX_AND_AVERAGED  // минимальный сигнал заменяется на средний, сигналы отправляются на соответствующие группы

#define MIN_SPEED_ONLY  // минимальный сигнал заменяется на средний, сигналы отправляются на соответствующие группы

#define MANUAL_SETTINGS  // для работы необходимо задать значения MIN_PULSE и MIN_DUTY ниже
#define MIN_PULSE_1 5
#define MIN_PULSE_2 15
#define MIN_DUTY_1 440
#define MIN_DUTY_2 154

enum SpeedMode {  // тип выбора значения PWM
  INPUT_IMPULSE,  // на основе входных PWM
  MAX_ALWAYS,     // максимально значение выходного PWM; входные сигналы не используются
  MIN_ALWAYS,     // минимально значение выходного PWM; входные сигналы не используются
};

#define MAX_DUTY 1023  // максимальное значение заполнения
#define FAN_PERIOD 40  // период сигнала в микросекундах
#define BUFFER_SIZE 5  // размер буфера
#define PULSE_SENSE_WIDTH 4  // из множества {4, 5, 8, 10}
#define current_tact_index ((is_odd_tact) ? 0 : 1)
#define current_ctrl ctrls[current_tact_index]

// пины режима скорости
struct {
  byte speed_by_input_pin;
  byte min_speed_pin;
} SPEED_PINS = {4, 5};

SpeedMode speed_mode;  // режим скорости
bool is_odd_tact;      // такт измерений
unsigned long checks_tact_counter; // счетчик такта проверки режимов
unsigned long reads_tact_counter;  // счетчик для такта чтения входного PWM

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
      1, 3, 10, A1, A3,
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
  checks_tact_counter = 0;
  reads_tact_counter = 0;

  speed_mode = read_speed_mode();

  refresh_caches(0);
  refresh_caches(1);

  update_pulse_in(0);
  update_pulse_in(1);
}

void loop() {
  ++reads_tact_counter;
  ++checks_tact_counter;
  is_odd_tact = !is_odd_tact;
  if (checks_tact_counter >= 44986) {
    checks_tact_counter = 0;
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
    if (reads_tact_counter >= 70000) {  // счетное число значит нечетный такт; нужно для чередования контоллеров
      reads_tact_counter = 0;
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

void refresh_caches(byte ctrl_index) {
#ifdef MANUAL_SETTINGS
  calculate_caches(ctrl_index);
#else
  int duty_pin_value = analogReadFast(ctrls[ctrl_index].duty_pin);
  int min_duty = min(MAX_DUTY - 1, abs(duty_pin_value));
  int pulse_pin_value = analogReadFast(ctrls[ctrl_index].pulse_pin);
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

    ctrls[ctrl_index].min_duty = min_duty;
    ctrls[ctrl_index].min_pulse = min_pulse;
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
  if (digitalReadFast(ctrls[ctrl_index].pwm_in_pin) == LOW) {
    pulse = pulseIn(ctrls[ctrl_index].pwm_in_pin, HIGH, 100);
  } else {
    pulse = FAN_PERIOD - pulseIn(ctrls[ctrl_index].pwm_in_pin, LOW, 100);
  }

  ctrls[ctrl_index].pulses_buffer[ctrls[ctrl_index].buffer_step] = pulse;

  if (++ctrls[ctrl_index].buffer_step >= BUFFER_SIZE) {
    ctrls[ctrl_index].buffer_step = 0;
  }

  // возможно, хватит и использования по трем значениям
  ctrls[ctrl_index].filtered_pulse = median_filter5(pulse, ctrls[ctrl_index].pulses_buffer);
}

void apply_pwm_by_tact(byte percent_1, byte percent_2) {
  int duty = get_duty_by_percent(current_ctrl, (is_odd_tact) ? percent_1 : percent_2);
  Timer1.pwm(current_ctrl.pwm_out_pin, duty);
#ifdef DEBUG_MODE_ON
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
#ifdef DEBUG_MODE_ON
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
  if (digitalReadFast(SPEED_PINS.speed_by_input_pin) == LOW) {
    return SpeedMode::INPUT_IMPULSE;
  }
  if (digitalReadFast(SPEED_PINS.min_speed_pin) == LOW) {
    return SpeedMode::MIN_ALWAYS;
  }
  return SpeedMode::MAX_ALWAYS;
#endif
}
