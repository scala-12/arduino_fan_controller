#include <TimerOne.h>  // для управления ШИМ; может выдавать ошибку в заивисимости WProgram.h

// TODO: экспонометр для инициализации MIN_DUTY
const word MIN_DUTY = 450;   // минимальное значение заполнения
const word MAX_DUTY = 1023;  // максимальное значение заполнения
const byte FAN_PERIOD = 40;  // период сигнала в микросекундах

byte pulse_sense_width;  // из множества {4, 5, 8, 10}
word pulse2Duty[FAN_PERIOD + 1];

bool debug_mode = false;

float median_filter5(byte value, byte* buffer);
byte read_pwm();

class InputSignalInfo;
class OutputSignalController;

class InputSignalInfo {
 private:
  const static byte _BUFFER_SIZE = 5;
  byte _pin;
  byte _buffer_step;
  byte _filtered_pulse;
  byte _pulses_buffer[_BUFFER_SIZE];

 public:
  InputSignalInfo(byte pin) {
    this->_pin = pin;
    this->_buffer_step = 0;
    this->_filtered_pulse = FAN_PERIOD;
    for (byte i = 0; i < InputSignalInfo::_BUFFER_SIZE; ++i) {
      this->_pulses_buffer[i] = FAN_PERIOD;
    }
  };

  /** найти заполнение сигнала */
  word get_duty() {
    word duty = pulse2Duty[this->_filtered_pulse];
    if (debug_mode) {
      Serial.print("pulse2Duty ");
      Serial.print(this->_pin);
      Serial.print(":\t");
      Serial.print(this->_filtered_pulse);
      Serial.print(" -> ");
      Serial.println(duty);
    }

    return duty;
  };

  /** чтение текущего PWM */
  byte read_pulse() {
    byte pulse;
    if (digitalRead(this->_pin) == LOW) {
      pulse = pulseIn(this->_pin, HIGH, 100);
    } else {
      pulse = FAN_PERIOD - pulseIn(this->_pin, LOW, 100);
    }

    this->_pulses_buffer[this->_buffer_step] = pulse;

    if (++this->_buffer_step >= InputSignalInfo::_BUFFER_SIZE) {
      this->_buffer_step = 0;
    }

    // возможно, хватит и использования по трем значениям
    this->_filtered_pulse = median_filter5(pulse, this->_pulses_buffer);

    if (debug_mode) {
      Serial.print("in ");
      Serial.print(this->_pin);
      Serial.print(": ");
      Serial.print(pulse);
      Serial.print(" (");
      Serial.print(this->_filtered_pulse);
      Serial.println(")");
    }
  };
};

class OutputSignalController {
 private:
  byte _pin;

 public:
  OutputSignalController(byte pin) {
    this->_pin = pin;
  };

  void apply_pwm(word duty) {
    if (debug_mode) {
      Serial.print("out ");
      Serial.print(this->_pin);
      Serial.print(":\t");
      Serial.println(duty);
    }
    Timer1.pwm(this->_pin, duty);
  };
};

enum SpeedMode {  // тип выбора значения PWM
  INPUT_IMPULSE,  // на основе входных PWM
  MAX_ALWAYS,     // максимально значение выходного PWM; входные сигналы не используются
  MIN_ALWAYS,     // минимально значение выходного PWM; входные сигналы не используются
};

enum MutationMode {  // вариант вычисления выходных PWM из входных PWM (для SpeedMode::INPUT_IMPULSE)
  MAX_ONLY,          // max PWM из входных на все выходы
  MAX_AND_AVERAGED,  // max PWM на группу, второй усредняется
  IMMUTABLE_VALUE,   // прямоток PWM
};

bool is_odd_tact;       // такт измерений
bool is_check_tact;     // такт проверки режимов
bool use_only_input_1;  // использовать только первый входной сигнал PWM

SpeedMode speed_mode;        // режим скорости
MutationMode mutation_mode;  // режим вычисления выходных PWM

const byte PWM_IN_PIN_1 = 2;  // пин первого входного PWM
const byte PWM_IN_PIN_2 = 3;  // пин второго входного PWM

const byte SPEED_MODE_PIN_1 = 4;     // первый пин режима скорости
const byte SPEED_MODE_PIN_2 = 5;     // второй пин режима скорости
const byte MUTATION_MODE_PIN_1 = 6;  // первый пин взаимного влияния сигналов
const byte MUTATION_MODE_PIN_2 = 7;  // второй пин взаимного влияния сигналов

const byte PWM_IN_PIN_2_DISSABLED_PIN = 8;  // использовать только один вход на две выходные группы

const byte PWM_OUT_PIN_1 = 9;   // пин PWM сигнала для первой группы
const byte PWM_OUT_PIN_2 = 10;  // пин PWM сигнала для второй группы

const byte DEBUG_MODE_PIN = 11;  // пин включения debug-режима

const byte DUTY_SIZE_PIN_1 = 12;  // пин включения debug-режима
const byte DUTY_SIZE_PIN_2 = 13;  // пин включения debug-режима

InputSignalInfo* input_info1 = new InputSignalInfo(PWM_IN_PIN_1);  // информация о первом входном сигнале
InputSignalInfo* input_info2;                                      // информация о втором входном сигнале

OutputSignalController* output_controller1 = new OutputSignalController(PWM_OUT_PIN_1);  // управление первым выходным сигналом
OutputSignalController* output_controller2 = new OutputSignalController(PWM_OUT_PIN_2);  // управление вторым выходным сигналом

SpeedMode read_speed_mode() {
  if (digitalRead(SPEED_MODE_PIN_1) == 0) {
    return SpeedMode::MAX_ALWAYS;
  } else if (digitalRead(SPEED_MODE_PIN_2) == 0) {
    return SpeedMode::MIN_ALWAYS;
  } else {
    return SpeedMode::INPUT_IMPULSE;
  }
}

MutationMode read_mutation_mode() {
  if (digitalRead(MUTATION_MODE_PIN_1) == 0) {
    return MutationMode::MAX_ONLY;
  } else if (digitalRead(MUTATION_MODE_PIN_2) == 0) {
    return MutationMode::IMMUTABLE_VALUE;
  } else {
    return MutationMode::MAX_AND_AVERAGED;
  }
}

void setup() {
  Timer1.initialize(FAN_PERIOD);  // частота ШИМ ~25 кГц

  Serial.begin(9600);

  pinMode(PWM_IN_PIN_1, INPUT);
  pinMode(PWM_IN_PIN_2, INPUT);
  pinMode(SPEED_MODE_PIN_1, INPUT_PULLUP);
  pinMode(SPEED_MODE_PIN_2, INPUT_PULLUP);
  pinMode(MUTATION_MODE_PIN_1, INPUT_PULLUP);
  pinMode(MUTATION_MODE_PIN_2, INPUT_PULLUP);
  pinMode(PWM_IN_PIN_2_DISSABLED_PIN, INPUT_PULLUP);
  pinMode(PWM_OUT_PIN_1, OUTPUT);
  pinMode(PWM_OUT_PIN_2, OUTPUT);
  pinMode(DEBUG_MODE_PIN, INPUT_PULLUP);
  pinMode(DUTY_SIZE_PIN_1, INPUT_PULLUP);
  pinMode(DUTY_SIZE_PIN_2, INPUT_PULLUP);

  if (digitalRead(DUTY_SIZE_PIN_1) == 0) {
    if (digitalRead(DUTY_SIZE_PIN_2) == 0) {
      pulse_sense_width = 10;
    } else {
      pulse_sense_width = 8;
    }
  } else if (digitalRead(DUTY_SIZE_PIN_2) == 0) {
    pulse_sense_width = 5;
  } else {
    pulse_sense_width = 4;
  }

  for (byte i = 0; i < FAN_PERIOD; ++i) {
    byte pulse_multiplier = (i + pulse_sense_width) / pulse_sense_width;
    pulse2Duty[i] = max(map(pulse_multiplier * pulse_sense_width, 0, FAN_PERIOD, 0, MAX_DUTY), MIN_DUTY);
  }
  pulse2Duty[FAN_PERIOD + 1] = pulse2Duty[FAN_PERIOD];

  is_odd_tact = false;
  is_check_tact = true;
  speed_mode = read_speed_mode();
  mutation_mode = read_mutation_mode();
  use_only_input_1 = digitalRead(PWM_IN_PIN_2_DISSABLED_PIN) == 0;

  debug_mode = digitalRead(DEBUG_MODE_PIN) == 0;

  if (!use_only_input_1) {
    input_info2 = new InputSignalInfo(PWM_IN_PIN_2);
    input_info2->read_pulse();
  } else {
    input_info2 = input_info1;
  }
  input_info1->read_pulse();
}

void loop() {
  is_odd_tact = !is_odd_tact;
  if (is_odd_tact) {
    is_check_tact = !is_check_tact;
    if (is_check_tact) {
      speed_mode = read_speed_mode();
      mutation_mode = read_mutation_mode();
    }
  }

  word duty_value1;
  word duty_value2;
  if (speed_mode == SpeedMode::INPUT_IMPULSE) {
    if (debug_mode) {
      Serial.println("speed mode: input impulse");
    }
    read_pwm();
    duty_value1 = input_info1->get_duty();
    if (!use_only_input_1) {
      duty_value2 = input_info2->get_duty();
      if (mutation_mode == MutationMode::MAX_ONLY) {
        word max_duty = max(duty_value1, duty_value2);
        duty_value1 = max_duty;
        duty_value2 = max_duty;
        if (debug_mode) {
          Serial.print("mutation mode: max_only (");
          Serial.print(max_duty);
          Serial.println(")");
        }
      } else if (mutation_mode == MutationMode::MAX_AND_AVERAGED) {
        word avaraged_value = (duty_value1 + duty_value2) / 2;
        if (duty_value1 > duty_value2) {
          duty_value2 = avaraged_value;
        } else {
          duty_value1 = avaraged_value;
        }
        if (debug_mode) {
          Serial.print("mutation mode: avaraged (");
          Serial.print(duty_value1);
          Serial.print("\t");
          Serial.print(duty_value2);
          Serial.println(")");
        }
      } else if (debug_mode) {
        Serial.println("mutation mode: immutable");
      }
    } else {
      duty_value2 = duty_value1;
    }
  } else {
    if (debug_mode) {
      Serial.print("speed mode: ");
      Serial.println((speed_mode == SpeedMode::MAX_ALWAYS) ? "MAX_DUTY" : "MIN_DUTY");
    }
    duty_value1 = (speed_mode == SpeedMode::MAX_ALWAYS) ? MAX_DUTY : MIN_DUTY;
    duty_value2 = duty_value1;
  }

  output_controller1->apply_pwm(duty_value1);
  output_controller2->apply_pwm(duty_value2);
}

float median_filter5(byte value, byte* buffer) {
  byte min_value = buffer[0];
  byte max_value = buffer[0];
  byte values3[3];
  byte count_values = 0;
  for (byte i = 1; i < 5; ++i) {
    if (min_value >= buffer[i]) {
      if (min_value != max_value) {
        values3[count_values] = min_value;
        count_values += 1;
      }
      min_value = buffer[i];
    } else if (max_value <= buffer[i]) {
      if (min_value != max_value) {
        values3[count_values] = max_value;
        count_values += 1;
      }
      max_value = buffer[i];
    } else {
      values3[count_values] = buffer[i];
      count_values += 1;
    }
  }

  byte middle;
  if (count_values > 0) {
    for (byte i = count_values - 1; i < 3; ++i) {
      values3[i] = min_value;
    }
    if ((values3[0] <= values3[1]) && (values3[0] <= values3[2])) {
      middle = min(values3[1], values3[2]);
    } else {
      if ((values3[1] <= values3[0]) && (values3[1] <= values3[2])) {
        middle = min(values3[0], values3[2]);
      } else {
        middle = min(values3[0], values3[1]);
      }
    }
  } else {
    middle = min_value;
  }

  return middle;
}

byte read_pwm() {
  InputSignalInfo* input_info = (is_odd_tact) ? input_info1 : input_info2;
  return input_info->read_pulse();
}
