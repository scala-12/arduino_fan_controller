#ifndef MENU_CONSTS_FILE_INCLUDED
#define MENU_CONSTS_FILE_INCLUDED

#include <Arduino.h>

enum MainMenu : byte {
  PULSES_MENU,
  TEMP_MENU,
  OPTIC_MENU,
  DUTIES_MENU,
  SETS_MENU
};
#define MAIN_MENU_SIZE 5

enum PulsesMenu : byte {
  SHOW_PULSES,
  MIN_PULSES,
  MAX_PULSES
};
#define PULSES_MENU_SIZE 3

enum TempMenu : byte {
  SHOW_TEMP,
  MIN_TEMP,
  MAX_TEMP
};
#define TEMP_MENU_SIZE 3

enum OpticMenu : byte {
  SHOW_RPM,
  MIN_RPM,
  MAX_RPM
};
#define OPTIC_MENU_SIZE 3

enum DutiesMenu : byte {
  SHOW_DUTIES,
  MIN_DUTIES
};
#define DUTIES_MENU_SIZE 2

enum SetsMenu : byte {
  HOLD_COOL,
  MIN_DUTY_PERCENT,
  RESET_OUT,
  COMMIT
};
#define SETS_MENU_SIZE 4

#define MENU_LEVELS 5

#define MENU_TIMEOUT 10240

#endif
