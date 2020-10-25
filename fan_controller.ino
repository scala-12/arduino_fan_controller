#include <TimerOne.h>

#define FAN1_INPUT 2
#define FAN2_INPUT 3
#define MODE1_INPUT 4
#define MODE2_INPUT 5
#define FAN2_DISSABLED_INPUT 6
#define FANS_OUTPUT 9
#define FAN_PERIOD 40
#define MIN_DUTY 50
#define MAX_DUTY 1023

struct FanInfo {
  byte uptime;
  byte input_pin;
};

enum Mode {
  NORMAL_MODE,
  MAX_MODE,
  MIN_MODE,
};

int current_duty = MIN_DUTY;
boolean is_enabled_fan2;
FanInfo fan1_info = FanInfo {0, FAN1_INPUT};
FanInfo fan2_info = FanInfo {0, FAN2_INPUT};
 
void setup() {
  pinMode(MODE1_INPUT, INPUT_PULLUP);
  pinMode(MODE2_INPUT, INPUT_PULLUP);
  pinMode(FAN2_DISSABLED_INPUT, INPUT_PULLUP);
  pinMode(fan1_info.input_pin, INPUT);
  pinMode(fan2_info.input_pin, INPUT);
  Timer1.initialize(FAN_PERIOD); // частота ШИМ ~25 кГц
  
  is_enabled_fan2 = (digitalRead(FAN2_DISSABLED_INPUT) == 0);
}
 
void loop() {
  int mode1 = digitalRead(MODE1_INPUT);
  int mode2 = digitalRead(MODE2_INPUT);
  Mode current_mode;
  if (mode1 == 0) {
    current_mode = MAX_MODE;
  } else if (mode2 == 0) {
    current_mode = MIN_MODE;
  } else {
    current_mode = NORMAL_MODE;
  }
  
  if (current_mode == NORMAL_MODE) {
    fan1_info = updateFanInfo(fan1_info);
    if (is_enabled_fan2) {
     fan2_info = updateFanInfo(fan2_info); 
    }
    applyFanCtrlByMaxInput();
  } else if (current_mode == MAX_MODE) {
    if (current_duty != MAX_DUTY) {
      setDuty(MAX_DUTY);
    }
  } else if (current_mode == MIN_MODE) {
    if (current_duty != MIN_DUTY) {
      setDuty(MIN_DUTY);
    }
  }
}

FanInfo updateFanInfo(FanInfo fan_info) {
  int value = pulseIn(fan_info.input_pin, HIGH, FAN_PERIOD * 2);
  if (value == 0) {
    value = (digitalRead(fan_info.input_pin) == 0) ? 0 : FAN_PERIOD;
  } else if (value > FAN_PERIOD) {
    value = FAN_PERIOD;
  }

  if (value != fan_info.uptime) {
    fan_info.uptime = value;
  }

  return fan_info;
}

void applyFanCtrlByMaxInput() {
  int max_uptime = (is_enabled_fan2 || (fan1_info.uptime > fan2_info.uptime)) ? fan1_info.uptime : fan2_info.uptime;
  
  int pwm_duty = map(max_uptime, 0, FAN_PERIOD, 0, MAX_DUTY);
  if (pwm_duty < MIN_DUTY) {
    if (current_duty != MIN_DUTY) {
      setDuty(MIN_DUTY);
    }
  } else if (current_duty != pwm_duty) {
    setDuty(pwm_duty);
  }
}

void setDuty(int pwm_duty) {
  current_duty = pwm_duty;
  Timer1.pwm(FANS_OUTPUT, pwm_duty);
}
