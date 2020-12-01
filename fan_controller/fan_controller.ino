#include <TimerOne.h>

const byte FAN_INPUT1 = 2;
const byte FAN_INPUT2 = 3;
const byte INPUT_MODE1 = 4;
const byte INPUT_MODE2 = 5;
const byte OUTPUT_MODE1 = 6;
const byte OUTPUT_MODE2 = 7;
const byte FAN_INPUT2_DISSABLED = 8;
const byte FANS_OUTPUT1 = 9;
const byte FANS_OUTPUT2 = 10;
const byte FAN_PERIOD = 40;
const byte MIN_DUTY = 50;
const word MAX_DUTY = 1023;

// параметры входного сигнала
struct FanInputInfo {
  byte pulse_time;  // период отсутствия импульса
  byte pin;
  bool is_odd_tack;  // такт, на котором получен сигнал
};
// параметры выходного сигнала
struct FansOutputInfo {
  byte pin;
  word duty;  // коэффициент заполнения PWM
};
// сгруппированные параметры выходного и входного сигнала
struct FanGroupInfo {
  FanInputInfo* input;
  FansOutputInfo* output;
};
// тип выходного сигнала
enum InputMode {
  PWM_MODE,   // управление по входному сигналу PWM
  MAX_SPEED,  // максимальная скорость
  MIN_SPEED,  // минимальная скорость
};
// вариант выбора выходного сигнала
enum OutputMode {
  MAX_ONLY,    // выбирать максимальный сигнал из двух входящих
  DIFFERENCE,  // максимальный сигнал на соответствующую ему группу, минимальный сигнал усредняется и отправляется на соответствующую группу
  DIRECT,      // каждый сигнал на соответствующий выход
};

// инициализация структур хранения сигналов
FanInputInfo info_input1 = FanInputInfo{0, FAN_INPUT1, false};
FanInputInfo info_input2 = FanInputInfo{0, FAN_INPUT2, false};
FansOutputInfo info_output1 = FansOutputInfo{FANS_OUTPUT1, MIN_DUTY};
FansOutputInfo info_output2 = FansOutputInfo{FANS_OUTPUT2, MIN_DUTY};
FanGroupInfo group1 = FanGroupInfo{&info_input1, &info_output1};
FanGroupInfo group2 = FanGroupInfo{&info_input2, &info_output2};

FanGroupInfo output_groups[] = {group1, group2};  // все группы параметров сигналов

OutputMode output_mode = OutputMode::DIFFERENCE;  // первичная настройка выходного сигнала

bool is_odd_tack = true;      // текущий такт (всего два, четный и нечетный)
bool only_one_input = false;  // количество входных сигналов (первичное значение - 2)

void setup() {
  Timer1.initialize(FAN_PERIOD);  // частота ШИМ ~25 кГц

  // инициализация пинов настройки
  pinMode(INPUT_MODE1, INPUT_PULLUP);
  pinMode(INPUT_MODE2, INPUT_PULLUP);
  pinMode(OUTPUT_MODE1, INPUT_PULLUP);
  pinMode(OUTPUT_MODE2, INPUT_PULLUP);
  pinMode(FAN_INPUT2_DISSABLED, INPUT_PULLUP);

  // инициализация входящих ШИМ пинов
  pinMode(info_input1.pin, INPUT);
  only_one_input = (digitalRead(FAN_INPUT2_DISSABLED) == 0);
  if (only_one_input) {
    pinMode(info_input2.pin, INPUT);  // используется два входных пина
  } else {
    group2.input = &info_input1;  // используется один входной пин
  }

  // инициализация режима выходного сигнала
  if (digitalRead(OUTPUT_MODE1) == 0) {
    output_mode = OutputMode::MAX_ONLY;
  } else {
    if (digitalRead(OUTPUT_MODE2) == 0) {
      output_mode = OutputMode::DIRECT;
    } else {
      output_mode = OutputMode::DIFFERENCE;
    }
  }
}

// получить тип выходного сигнала
InputMode readInputMode() {
  if (digitalRead(INPUT_MODE1) == 0) {
    return InputMode::MAX_SPEED;
  } else {
    if (digitalRead(INPUT_MODE2) == 0) {
      return InputMode::MIN_SPEED;
    } else {
      return InputMode::PWM_MODE;
    }
  }
}

// обновить информацию о входном сигнале в текущем такте
void updateInputInfo() {
  InputMode input_mode = readInputMode();
  is_odd_tack = !is_odd_tack;

  if (input_mode == InputMode::PWM_MODE) {
    for (int i = 0; i < 2; i++) {
      FanInputInfo* input_info = output_groups[i].input;
      if (is_odd_tack != input_info->is_odd_tack) {
        int value = pulseIn(input_info->pin, HIGH, FAN_PERIOD * 2);
        if (value == 0) {
          value = (digitalRead(input_info->pin) == 0) ? 0 : FAN_PERIOD;
        } else if (value > FAN_PERIOD) {
          value = FAN_PERIOD;
        }

        if (value != input_info->pulse_time) {
          input_info->pulse_time = value;
        }
        input_info->is_odd_tack = is_odd_tack;
      }
    }

    if (!only_one_input) {
      if (output_mode == OutputMode::MAX_ONLY) {
        if (group1.input->pulse_time > group2.input->pulse_time) {
          group2.input->pulse_time = group1.input->pulse_time;
        } else {
          group1.input->pulse_time = group2.input->pulse_time;
        }
      } else if (output_mode == OutputMode::DIFFERENCE) {
        if (group1.input->pulse_time > group2.input->pulse_time) {
          group2.input->pulse_time = (group2.input->pulse_time + group1.input->pulse_time) / 2;
        } else {
          group1.input->pulse_time = (group2.input->pulse_time + group1.input->pulse_time) / 2;
        }
      }
    }
  } else {
    int value = (input_mode == InputMode::MAX_SPEED) ? FAN_PERIOD : 0;
    for (int i = 0; i < 2; i++) {
      FanInputInfo* input_info = output_groups[i].input;
      if (is_odd_tack != input_info->is_odd_tack) {
        if (value != input_info->pulse_time) {
          input_info->pulse_time = value;
        }
        input_info->is_odd_tack = is_odd_tack;
      }
    }
  }
}

void applyDuty(FanGroupInfo group) {
  Timer1.pwm(group.output->pin, group.output->duty);
}

void loop() {
  updateInputInfo();
  applyDuty(group1);
  applyDuty(group2);
}
