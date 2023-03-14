#ifndef MACROS_FILE_INCLUDED
#define MACROS_FILE_INCLUDED

#define get_arr_len(arr) (sizeof(arr) / sizeof(arr[0])) /* найти длину массива */

#define convert_by_sqrt(x, min_x, max_x, min_y, max_y) map(sqrt(map(constrain(x, min_x, max_x), min_x, max_x, 0, 900)), 0, 30, min_y, max_y)

#endif
