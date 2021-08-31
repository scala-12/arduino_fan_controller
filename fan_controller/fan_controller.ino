#include <TimerOne.h>  // для управления ШИМ; может выдавать ошибку в заивисимости WProgram.h

const byte MIN_DUTY = 200;   // минимальное значение заполнения
const word MAX_DUTY = 1023;  // максимальное значение заполнения
const byte FAN_PERIOD = 40;  // период сигнала в микросекундах

const float SMOOTHING_FACTOR = 0.2;  // коэффициент усреднения

const bool IS_DEBUG = false;

class InputSignalInfo {
 private:
  byte _pin;
  float _avg_pulse;
  byte _last_pulse;

 public:
  InputSignalInfo(byte pin) {
    this->_pin = pin;
    this->_avg_pulse = FAN_PERIOD;
    this->_last_pulse = FAN_PERIOD;
  };

  /** вычисление заполнения; если значение импульса не изменялось, то показывается последнее сохраненное */
  word calculate_duty() {
    word value = max(map(this->_avg_pulse, 0, FAN_PERIOD, 0, MAX_DUTY), MIN_DUTY);

    if (IS_DEBUG) {
      Serial.print("calculate duty ");
      Serial.print(this->_pin);
      Serial.print(":\t");
      Serial.print(this->_avg_pulse);
      Serial.print(" -> ");
      Serial.println(value);
    }

    return value;
  };

  /** чтение текущего PWM */
  byte read_pulse() {
    int value = pulseIn(this->_pin, HIGH, FAN_PERIOD);
    if (value == 0) {
      value = (digitalRead(this->_pin) == 0) ? 0 : FAN_PERIOD;
    } else if (value > FAN_PERIOD) {
      value = FAN_PERIOD;
    }
    this->_last_pulse = value;

    this->_avg_pulse = ((1 - SMOOTHING_FACTOR) * this->_avg_pulse) + (SMOOTHING_FACTOR * value);
    if (IS_DEBUG) {
      Serial.print("in ");
      Serial.print(this->_pin);
      Serial.print(": ");
      Serial.print(value);
      Serial.print(" (");
      Serial.print(this->_avg_pulse);
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
    if (IS_DEBUG) {
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

byte read_pwm() {
  InputSignalInfo* input_info = (is_odd_tact) ? input_info1 : input_info2;
  return input_info->read_pulse();
}

void setup() {
  Timer1.initialize(FAN_PERIOD);  // частота ШИМ ~25 кГц

  if (IS_DEBUG) {
    Serial.begin(9600);
  }

  pinMode(PWM_IN_PIN_1, INPUT);
  pinMode(PWM_IN_PIN_2, INPUT);
  pinMode(SPEED_MODE_PIN_1, INPUT_PULLUP);
  pinMode(SPEED_MODE_PIN_2, INPUT_PULLUP);
  pinMode(MUTATION_MODE_PIN_1, INPUT_PULLUP);
  pinMode(MUTATION_MODE_PIN_2, INPUT_PULLUP);
  pinMode(PWM_IN_PIN_2_DISSABLED_PIN, INPUT_PULLUP);
  pinMode(PWM_OUT_PIN_1, OUTPUT);
  pinMode(PWM_OUT_PIN_2, OUTPUT);

  is_odd_tact = false;
  is_check_tact = true;
  speed_mode = read_speed_mode();
  mutation_mode = read_mutation_mode();
  use_only_input_1 = digitalRead(PWM_IN_PIN_2_DISSABLED_PIN) == 0;

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
    if (IS_DEBUG) {
      Serial.println("speed mode: input impulse");
    }
    read_pwm();
    duty_value1 = input_info1->calculate_duty();
    if (!use_only_input_1) {
      duty_value2 = input_info2->calculate_duty();
      if (mutation_mode == MutationMode::MAX_ONLY) {
        word max_duty = max(duty_value1, duty_value2);
        duty_value1 = max_duty;
        duty_value2 = max_duty;
        if (IS_DEBUG) {
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
        if (IS_DEBUG) {
          Serial.print("mutation mode: avaraged (");
          Serial.print(duty_value1);
          Serial.print("\t");
          Serial.print(duty_value2);
          Serial.println(")");
        }
      } else if (IS_DEBUG) {
        Serial.println("mutation mode: immutable");
      }
    } else {
      duty_value2 = duty_value1;
    }
  } else {
    if (IS_DEBUG) {
      Serial.print("speed mode: ");
      Serial.println((speed_mode == SpeedMode::MAX_ALWAYS) ? "MAX_DUTY" : "MIN_DUTY");
    }
    duty_value1 = (speed_mode == SpeedMode::MAX_ALWAYS) ? MAX_DUTY : MIN_DUTY;
    duty_value2 = duty_value1;
  }

  output_controller1->apply_pwm(duty_value1);
  output_controller2->apply_pwm(duty_value2);
}
