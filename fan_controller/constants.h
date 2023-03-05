#ifndef CONSTS_FILE_INCLUDED
#define CONSTS_FILE_INCLUDED

// системные параметры, лучше менять
#define MAX_DUTY 254                          /* максимальное значение заполнения */
#define MIN_DUTY 1                            /* минимальное значение заполнения; если диапазон более 254 значений, то PWM не реагирует на изменения */
#define PWM_RES 8                             /* битность PWM */
#define PULSE_WIDTH 40                        /* период сигнала в микросекундах */
#define PULSE_FREQ (1000000 / PULSE_WIDTH)    /* частота сигнала */
#define BUFFER_SIZE_ON_READ 15                /* размер буфера на чтение PWM сигнала */
#define BUFFER_SIZE_FOR_SMOOTH 5              /* размер буфера для сглаживания входящего сигнала */
#define DEFAULT_MIN_PERCENT 30                /* нижняя граница чувствительности к входящему PWM */
#define DEFAULT_MAX_PERCENT 65                /* верхняя граница чувствительности к входящему PWM */
#define DEFAULT_MIN_TEMP 28                   /* нижняя граница чувсвительности температурного датчика */
#define DEFAULT_MAX_TEMP 38                   /* верхняя граница чувсвительности температурного датчика */
#define MAX_TEMP_VALUE 50                     /* максимальное значение для датчика температуры */
#define MIN_TEMP_VALUE 10                     /* минимальное значение для датчика температуры */
#define SERIAL_SPEED 115200                   /* скорость серийного порта */
#define PWM_READ_TIMEOUT 2000                 /* таймаут чтения входящего ЩИМ */
#define PWM_READ_HZ (1000 / PWM_READ_TIMEOUT) /* частота чтения входящего ЩИМ */
#define VERSION_NUMBER 4                      /* версия структуры данных, хранящихся в памяти */
#define INIT_ADDR 1023                        /* ячейка памяти с информацией о структуре хранящихся данных */
#define PULSE_AVG_POWER 1                     /* радиус усреднения медианны для входящего сигнала */

#define SHOW_PULSES_COMMAND "show_pulses"
#define SHOW_TEMPS_COMMAND "show_temps"
#define SET_MIN_TEMP_COMMAND "set_min_temp"
#define SET_MAX_TEMP_COMMAND "set_max_temp"
#define GET_MIN_TEMP_COMMAND "get_min_temp"
#define GET_MAX_TEMP_COMMAND "get_max_temp"
#define GET_MIN_PULSES_COMMAND "get_min_pulses"
#define GET_MAX_PULSES_COMMAND "get_max_pulses"
#define SAVE_PARAMS_COMMAND "save_params"
#define SET_MAX_PULSE_COMMAND "set_max_pulse"
#define SET_MIN_PULSE_COMMAND "set_min_pulse"
#define RESET_MIN_DUTIES_COMMAND "reset_min_duties"
#define SET_MIN_DUTY_COMMAND "set_min_duty"
#define GET_MIN_DUTIES_COMMAND "get_min_duties"
#define SWITCH_DEBUG_COMMAND "switch_debug"
// ^^^ системные переменные, нельзя менять ^^^

#endif
