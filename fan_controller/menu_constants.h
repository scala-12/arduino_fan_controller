#ifndef MENU_CONSTS_FILE_INCLUDED
#define MENU_CONSTS_FILE_INCLUDED

/*
[?] level == 0: меню закрыто, значение не исползуется
[?][0] input
[?][0][0] актуальное значение
[?][0][1] min
[?][0][1][0] актуальное значения
[?][0][1][x] set x
[?][0][1][x][?] настройка x
[?][0][2] max
[?][0][2][0] актуальное значение
[?][0][2][x] set x
[?][0][2][x][?] настройка x
[?][1] temp
[?][1][0] актуальное значение
[?][1][1] min
[?][1][2] max
[?][2] optic
[?][2][0] актуальное значение
[?][2][1] min
[?][2][2] max
[?][3] output
[?][3][0] актуальное значение
[?][3][1] min
[?][3][1][0] актуальное значение
[?][3][1][x] set x
[?][3][1][x][?] настройка x
[?][4] settings
[?][4][0] cool_on_hold
[?][4][0][0] текущее значение == false
[?][4][0][1] текущее значение == true
[?][4][1] reset min outs
[?][4][2] commit
*/
#define SHOW_PARAM_VALUE 0
#define PULSES_MENU_1 0
#define PULSES_MENU_1_MIN_2 1
#define PULSES_MENU_1_MAX_2 2
#define PULSES_MENU_1_COUNT 3
#define TEMP_MENU_1 1
#define TEMP_MENU_1_MIN_2 1
#define TEMP_MENU_1_MAX_2 2
#define TEMP_MENU_1_COUNT 3
#define OPTIC_MENU_1 2
#define OPTIC_MENU_1_MIN_2 1
#define OPTIC_MENU_1_MAX_2 2
#define OPTIC_MENU_1_COUNT 3
#define DUTIES_MENU_1 3
#define DUTIES_MENU_1_MIN_2 1
#define DUTIES_MENU_1_COUNT 2
#define SETS_MENU_1 4
#define SETS_MENU_1_HOLD_COOL_2 0
#define SETS_MENU_1_RESET_OUT_2 1
#define SETS_MENU_1_COMMIT_2 2
#define SETS_MENU_1_COUNT 3
#define MENU_1_COUNT 5
#define MENU_LEVELS 5

#define MENU_TIMEOUT 10240

#endif
