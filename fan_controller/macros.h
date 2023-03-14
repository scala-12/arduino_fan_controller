#ifndef MACROS_FILE_INCLUDED
#define MACROS_FILE_INCLUDED

#define get_arr_len(arr) (sizeof(arr) / sizeof(arr[0])) /* найти длину массива */

#define convert_by_sqrt(x, min_x, max_x, min_y, max_y) map(sqrt(map(constrain(x, min_x, max_x), min_x, max_x, 0, 900)), 0, 30, min_y, max_y)

#define check_diff(int_1, int_2, diff) ((max(int_1, int_2) - min(int_1, int_2)) > diff)

#define fixed_delay(ms) /* иммитация delay(ms) через цикл с корректированными функциями времени */ \
  for (uint32_t _tmr_start = millis(), _timer = 0; abs(_timer) < ms; _timer = millis() - _tmr_start) {                                                 \
  }

#endif
