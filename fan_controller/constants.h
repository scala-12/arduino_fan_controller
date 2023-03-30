#ifndef CONSTS_FILE_INCLUDED
#define CONSTS_FILE_INCLUDED

#include <Arduino.h>

#include "menu_constants.h"

// системные параметры, лучше менять
#define MAX_DUTY 254               /* максимальное значение заполнения */
#define MIN_DUTY 1                 /* минимальное значение заполнения; если диапазон более 254 значений, то PWM не реагирует на изменения */
#define PWM_RES 8                  /* битность PWM */
#define PULSE_WIDTH 40             /* период сигнала в микросекундах */
#define BUFFER_SIZE_ON_READ 15     /* размер буфера на чтение PWM сигнала */
#define BUFFER_SIZE_FOR_SMOOTH 5   /* размер буфера для сглаживания входящего сигнала */
#define DEFAULT_MIN_PERCENT 30     /* нижняя граница чувствительности к входящему PWM */
#define DEFAULT_MAX_PERCENT 65     /* верхняя граница чувствительности к входящему PWM */
#define DEFAULT_MIN_TEMP 28        /* нижняя граница чувсвительности температурного датчика */
#define DEFAULT_MAX_TEMP 38        /* верхняя граница чувсвительности температурного датчика */
#define DEFAULT_MIN_OPTIC_RPM 0    /* нижняя граница чувствительности оптического датчика */
#define DEFAULT_MAX_OPTIC_RPM 1500 /* верхняя граница чувствительности оптического датчика */
#define MAX_OPTIC_RPM_VALUE 5000   /* максимальное значение для оптического датчика */
#define MIN_OPTIC_RPM_VALUE 0      /* минимальное значение для оптического датчика */
#define MAX_TEMP_VALUE 50          /* максимальное значение для датчика температуры */
#define MIN_TEMP_VALUE 10          /* минимальное значение для датчика температуры */
#define SERIAL_SPEED 115200        /* скорость серийного порта */
#define SENSE_REFRESH_MS 2000      /* таймаут чтения входящих сигналов */
#define VERSION_NUMBER 8           /* версия структуры данных, хранящихся в памяти */
#define INIT_ADDR 1023             /* ячейка памяти с информацией о структуре хранящихся данных */
#define PULSE_AVG_POWER 1          /* радиус усреднения медианны для входящего сигнала */
#define COOLING_PIN A6             /* пин включения максимальной скорости */

const byte OUTPUTS_PINS[][2] = {{3, 4}, {5, 6}, {9, 8}, {10, 11}};  // пины [выходящий PWM, RPM]

const byte INPUTS_PINS[] = {A1, A2, A3};  // пины входящих PWM

#define TEMP_SENSOR_PIN A4    /* пин датчика температуры */
#define OPTICAL_SENSOR_PIN A5 /* пин оптического энкодера */

#define COOLING_PIN A6     /* пин включения максимальной скорости */
#define ANALOG_KEYS_PIN A7 /* пин подключения клавиш */

#define MTRX_CS_PIN 12    /* CS-пин матрицы */
#define MTRX_CLOCK_PIN 13 /* Clk-пин матрицы */
#define MTRX_DATA_PIN A0  /* DIn-пин матрицы */

#define MTRX_PIXELS_IN_CHAR_BY_ROW 6
#define MTRX_PIXELS_IN_SPACE_BY_ROW (MTRX_PIXELS_IN_CHAR_BY_ROW >> 1)
#define MTRX_PIXELS_IN_COLUMN 8
#define MTRX_PIXELS_IN_ROW (MTRX_PIXELS_IN_COLUMN * MTRX_PANELS_COUNT)  // количество пикселей в ряд
#define MTRX_INDENT 1
#define MTRX_REFRESH_MS 64
#define MTRX_SLIDING_DELAY_TACTS 8
#define THE_ULTIMATE_QUESTION_OF_LIFE_THE_UNIVERSE_AND_EVERYTHING 42
#define MTRX_BUFFER THE_ULTIMATE_QUESTION_OF_LIFE_THE_UNIVERSE_AND_EVERYTHING
#define MTRX_SLIDE_DELAY (MTRX_REFRESH_MS >> 1)
#define MTRX_SLIDE_DELAY_HORIZONTAL (MTRX_SLIDE_DELAY / MTRX_PANELS_COUNT)

#define CTRL_KEYS_COUNT 3 /* количество клавиш для управления */
enum ButtonStateBit : byte {
  CLICK,
  HOLD_0,
  HELD_1
};
enum ButtonKey : byte {
  UP,
  SELECT,
  DOWN
};
int16_t buttons_map[CTRL_KEYS_COUNT] = {317, 1016, 636};  // уровни клавиш UP, SELECT, DOWN

#ifndef DONT_USE_UART
#define SHOW_PULSES_COMMAND "show_pulses"
#define SHOW_TEMP_COMMAND "show_temp"
#define SHOW_OPTICAL_COUNTER_COMMAND "show_optic"
#define SHOW_MIN_DUTY_PERCENT_COMMAND "show_min_duty_percent"
#define SET_MIN_DUTY_PERCENT_COMMAND "set_min_duty_percent"
#define SET_MIN_TEMP_COMMAND "set_min_temp"
#define SET_MAX_TEMP_COMMAND "set_max_temp"
#define GET_MIN_TEMP_COMMAND "get_min_temp"
#define GET_MAX_TEMP_COMMAND "get_max_temp"
#define SET_MIN_OPTICAL_COMMAND "set_min_optical"
#define SET_MAX_OPTICAL_COMMAND "set_max_optical"
#define GET_MIN_OPTICAL_COMMAND "get_min_optical"
#define GET_MAX_OPTICAL_COMMAND "get_max_optical"
#define GET_MIN_PULSES_COMMAND "get_min_pulses"
#define GET_MAX_PULSES_COMMAND "get_max_pulses"
#define SAVE_PARAMS_COMMAND "save_params"
#define SET_MAX_PULSE_COMMAND "set_max_pulse"
#define SET_MIN_PULSE_COMMAND "set_min_pulse"
#define RESET_MIN_DUTIES_COMMAND "reset_min_duties"
#define SET_MIN_DUTY_COMMAND "set_min_duty"
#define GET_MIN_DUTIES_COMMAND "get_min_duties"
#define SWITCH_DEBUG_COMMAND "switch_debug"
#define SWITCH_COOLING_HOLD_COMMAND "switch_cooling_hold"
#endif
// ^^^ системные переменные, нельзя менять ^^^

#endif
