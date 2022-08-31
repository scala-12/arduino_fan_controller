#include <TimerOne.h>  // для управления ШИМ; может выдавать ошибку в заивисимости WProgram.h
#include "extra_functions.h"

// #define DEBUG_MODE_ON

class SignalController;

enum SpeedMode {  // тип выбора значения PWM
  INPUT_IMPULSE,  // на основе входных PWM
  MAX_ALWAYS,     // максимально значение выходного PWM; входные сигналы не используются
  MIN_ALWAYS,     // минимально значение выходного PWM; входные сигналы не используются
};

enum MutationMode {  // вариант вычисления выходных PWM из входных PWM (для SpeedMode::INPUT_IMPULSE)
  MAX_PERCENT,       // max PWM из входных на все выходы
  MAX_AND_AVERAGED,  // max PWM на группу, второй усредняется
  IMMUTABLE_VALUE,   // прямоток PWM
};

// пины настройки минимального DUTY
const byte MIN_DUTY_PIN_1 = A0;
const byte MIN_DUTY_PIN_2 = A1;

// пины настройки минимального PULSE
const byte MIN_PULSE_PIN_1 = A2;
const byte MIN_PULSE_PIN_2 = A3;

// пины входного PWM
const byte PWM_IN_PIN_1 = 2;
const byte PWM_IN_PIN_2 = 3;

// пины режима скорости
const byte SPEED_MODE_PIN_1 = 4;
const byte SPEED_MODE_PIN_2 = 5;

// пины PWM сигнала по группам
const byte PWM_OUT_PIN_1 = 9;
const byte PWM_OUT_PIN_2 = 10;

SpeedMode speed_mode;        // режим скорости
MutationMode mutation_mode;  // режим вычисления выходных PWM
bool is_odd_tact;       // такт измерений
bool is_check_tact;     // такт проверки режимов

// информация о сигналах
SignalController* controllers[2];

void update_pulse_2duty_by_tact();
void apply_pwm_by_tact(byte percent_1, byte percent_2);
byte read_pulse_by_tact();

SpeedMode read_speed_mode();

class SignalController {
 public:
  const static byte FAN_PERIOD = 40;  // период сигнала в микросекундах
 private:
  const static byte _BUFFER_SIZE = 5;
  const static int _MAX_DUTY = 1023;  // максимальное значение заполнения
  const static byte _PULSE_SENSE_WIDTH = 4;  // из множества {4, 5, 8, 10}

  byte _pwm_in_pin;
  byte _pwm_out_pin;
  byte _duty_pin;
  byte _pulse_pin;
  byte _buffer_step;
  byte _filtered_pulse;
  byte _pulses_buffer[_BUFFER_SIZE];
  int _pulse_2duty[FAN_PERIOD + 1];
  int _min_duty;
  byte _min_pulse;
  byte _pulse_2percent[FAN_PERIOD + 1];
  byte _percent_2pulse[101];

 public:
  SignalController(byte pwm_in_pin, byte pwm_out_pin, byte duty_pin, byte pulse_pin);
  void update_pulse_2duty();
  int get_min_duty();
  byte read_pulse();
  int get_duty_by_percent(byte percent);
  byte get_percent();
  int get_avg_pulse();
  void apply_pwm_by_duty(int duty);
};

void setup() {
  Timer1.initialize(SignalController::FAN_PERIOD);  // частота ШИМ ~25 кГц

  Serial.begin(9600);

  pinMode(MIN_DUTY_PIN_1, INPUT);
  pinMode(MIN_DUTY_PIN_2, INPUT);
  pinMode(PWM_IN_PIN_1, INPUT);
  pinMode(PWM_IN_PIN_2, INPUT);
  pinMode(SPEED_MODE_PIN_1, INPUT_PULLUP);
  pinMode(SPEED_MODE_PIN_2, INPUT_PULLUP);
  pinMode(PWM_OUT_PIN_1, OUTPUT);
  pinMode(PWM_OUT_PIN_2, OUTPUT);

  is_odd_tact = false;
  is_check_tact = true;

  speed_mode = read_speed_mode();
  mutation_mode = MutationMode::MAX_PERCENT;

  controllers[0] = new SignalController(PWM_IN_PIN_1, PWM_OUT_PIN_1, MIN_DUTY_PIN_1, MIN_PULSE_PIN_1);
  controllers[1] = new SignalController(PWM_IN_PIN_2, PWM_OUT_PIN_2, MIN_DUTY_PIN_2, MIN_PULSE_PIN_2);

  controllers[0]->read_pulse();
  controllers[1]->read_pulse();
}

void loop() {
  is_odd_tact = !is_odd_tact;
  if (is_odd_tact) {
    is_check_tact = !is_check_tact;
    if (is_check_tact) {
      speed_mode = read_speed_mode();
    }
  }
  update_pulse_2duty_by_tact();

  byte percent_1;
  byte percent_2;
  if (speed_mode == SpeedMode::INPUT_IMPULSE) {
#ifdef DEBUG_MODE_ON
    Serial.println("speed mode: input impulse");
#endif

    read_pulse_by_tact();
    percent_1 = controllers[0]->get_percent();
    percent_2 = controllers[1]->get_percent();
    if (mutation_mode == MutationMode::MAX_PERCENT) {
      if (percent_1 > percent_2) {
        percent_2 = percent_1;
      } else {
        percent_1 = percent_2;
      }
#ifdef DEBUG_MODE_ON
      Serial.print("mutation mode: MAX_PERCENT (pulse ");
      Serial.print(percent_1);
      Serial.println(")");
#endif
    } else if (mutation_mode == MutationMode::MAX_AND_AVERAGED) {
#ifdef DEBUG_MODE_ON
      Serial.print("mutation mode: avaraged (");
      Serial.print(percent_1);
      Serial.print("\t");
      Serial.print(percent_2);
      Serial.print(")\t->\t(");
#endif
      if (percent_1 > percent_2) {
        percent_2 = (percent_1 + percent_2) / 2;
      } else {
        percent_1 = (percent_1 + percent_2) / 2;
      }
#ifdef DEBUG_MODE_ON
      Serial.print(percent_1);
      Serial.print("\t");
      Serial.print(percent_2);
      Serial.println(")");
#endif
    } else {
      Serial.println("mutation mode: ERROR, immutable");
    }
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

void SignalController::update_pulse_2duty() {
  int duty_pin_value = analogReadFast(this->_duty_pin);
  int min_duty = min(SignalController::_MAX_DUTY - 1, abs(duty_pin_value));
  int pulse_pin_value = analogReadFast(this->_pulse_pin);
  byte min_pulse = min(
      SignalController::FAN_PERIOD - 1,
      map(
          min(SignalController::_MAX_DUTY, abs(pulse_pin_value)),
          0, SignalController::_MAX_DUTY,
          0, SignalController::FAN_PERIOD));
  if ((abs(min_duty - this->_min_duty) > 15) || (abs(min_pulse - this->_min_pulse) > 1)) {
    Serial.print(this->_pulse_pin);
    Serial.print(": pulse ");
    Serial.print(min_pulse);
    Serial.print("(");
    Serial.print(this->_min_pulse);
    Serial.print("), duty ");
    Serial.print(min_duty);
    Serial.print("(");
    Serial.print(this->_min_duty);
    Serial.print(")");
    Serial.println();

    this->_min_duty = min_duty;
    this->_min_pulse = min_pulse;
    // TODO: оптимизировать
    int prev_percent = -1;
    int min_percent = -1;
    for (byte i = 0;
         i <= SignalController::FAN_PERIOD;
         ++i) {
      byte pulse_multiplier = (i + SignalController::_PULSE_SENSE_WIDTH) /
                              SignalController::_PULSE_SENSE_WIDTH;
      byte pulse_value = min(
          pulse_multiplier * SignalController::_PULSE_SENSE_WIDTH, SignalController::FAN_PERIOD);
      byte percent;
      int duty;
      if (pulse_value < min_pulse) {
        duty = min_duty;
        percent = 0;
      } else {
        duty = map(
            pulse_value,
            min_pulse, SignalController::FAN_PERIOD,
            min_duty, SignalController::_MAX_DUTY);
        percent = map(
            pulse_value,
            min_pulse, SignalController::FAN_PERIOD,
            0, 100);
        if (prev_percent != -1) {
          for (byte j = prev_percent + 1;
               j <= percent;
               ++j) {
            this->_percent_2pulse[j] = pulse_value;
          }
        }
      }
      this->_pulse_2duty[i] = duty;
      this->_pulse_2percent[i] = percent;
      if (prev_percent != percent) {
        prev_percent = percent;
      }
      if ((min_percent == -1) && (i == min_pulse)) {
        min_percent = percent;
      }
    }
    if (min_percent != -1) {
      for (byte j = 0; j <= min_percent; ++j) {
        this->_percent_2pulse[j] = min_pulse;
      }
    }
  }
}

SignalController::SignalController(byte pwm_in_pin, byte pwm_out_pin, byte duty_pin, byte pulse_pin) {
  this->_pwm_in_pin = pwm_in_pin;
  this->_pwm_out_pin = pwm_out_pin;
  this->_duty_pin = duty_pin;
  this->_pulse_pin = pulse_pin;
  this->_buffer_step = 0;
  this->_filtered_pulse = SignalController::FAN_PERIOD;
  this->_min_duty = SignalController::_MAX_DUTY;
  this->_min_pulse = SignalController::FAN_PERIOD;
  for (byte i = 0; i < SignalController::_BUFFER_SIZE; ++i) {
    this->_pulses_buffer[i] = SignalController::FAN_PERIOD;
  }
  for (byte i = 0; i <= SignalController::FAN_PERIOD; ++i) {
    this->_pulse_2percent[i] = 100;
    this->_pulse_2duty[i] = SignalController::_MAX_DUTY;
  }
  for (byte i = 0; i <= 100; ++i) {
    this->_percent_2pulse[i] = SignalController::FAN_PERIOD;
  }
  this->update_pulse_2duty();
}

int SignalController::get_duty_by_percent(byte percent) {
  return this->_pulse_2duty[this->_percent_2pulse[percent]];
}

byte SignalController::get_percent() {
  return this->_pulse_2percent[this->_filtered_pulse];
}

byte SignalController::read_pulse() {
  byte pulse;
  if (digitalReadFast(this->_pwm_in_pin) == LOW) {
    pulse = pulseIn(this->_pwm_in_pin, HIGH, 100);
  } else {
    pulse = SignalController::FAN_PERIOD - pulseIn(this->_pwm_in_pin, LOW, 100);
  }

  this->_pulses_buffer[this->_buffer_step] = pulse;

  if (++this->_buffer_step >= SignalController::_BUFFER_SIZE) {
    this->_buffer_step = 0;
  }

  // возможно, хватит и использования по трем значениям
  this->_filtered_pulse = median_filter5(pulse, this->_pulses_buffer);
}

void SignalController::apply_pwm_by_duty(int duty) {
  Timer1.pwm(this->_pwm_out_pin, duty);
}

int SignalController::get_avg_pulse() {
  return this->_filtered_pulse;
}

int SignalController::get_min_duty() {
  return this->_min_duty;
}

void update_pulse_2duty_by_tact() {
  ((is_odd_tact) ? controllers[0] : controllers[1])->update_pulse_2duty();
#ifdef DEBUG_MODE_ON
  Serial.print("min_duty_");
  Serial.print((is_odd_tact) ? "1" : "2");
  Serial.print(": ");
  Serial.println(((is_odd_tact) ? controllers[0] : controllers[1])->get_min_duty());
#endif
}

void apply_pwm_by_tact(byte percent_1, byte percent_2) {
  int duty = controllers[(is_odd_tact) ? 0 : 1]
                 ->get_duty_by_percent((is_odd_tact) ? percent_1 : percent_2);
  controllers[(is_odd_tact) ? 0 : 1]->apply_pwm_by_duty(duty);
#ifdef DEBUG_MODE_ON
  Serial.print("out_");
  Serial.print((is_odd_tact) ? "1" : "2");
  Serial.print(":\t");
  Serial.print(duty);
  Serial.print("(");
  Serial.print((is_odd_tact) ? percent_1 : percent_2);
  Serial.print("%)");
  Serial.println();
#endif
}

byte read_pulse_by_tact() {
  byte pulse = controllers[(is_odd_tact) ? 0 : 1]->read_pulse();
#ifdef DEBUG_MODE_ON
  Serial.print("in_");
  Serial.print((is_odd_tact) ? "1" : "2");
  Serial.print(": ");
  Serial.print(pulse);
  Serial.print(" (");
  Serial.print(controllers[(is_odd_tact) ? 0 : 1]->get_avg_pulse());
  Serial.println(")");
#endif
  return pulse;
}

SpeedMode read_speed_mode() {
  if (digitalReadFast(SPEED_MODE_PIN_1) == LOW) {
    return SpeedMode::INPUT_IMPULSE;
  }
  if (digitalReadFast(SPEED_MODE_PIN_2) == LOW) {
    return SpeedMode::MIN_ALWAYS;
  }
  return SpeedMode::MAX_ALWAYS;
}
