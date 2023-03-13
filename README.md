# Автоматический реобас

## Описание

Устройство для управления несколькими вентиляторами высокочастотными ШИМ сигналами на основе источников сигнала разного типа: 3 ШИМ, 1 датчик температуры, 1 оптический датчик. Может быть использовано от 0 источников каждого типа.

В каждой выходной группе произоводится поиск минимальной скорости. После этого вентилятор не останавливается, а поддерживает минимальное вращение.

Подразумевается использования следующих источников:
- ШИМ сигналы с разъемов CPU_FAN и SYS_FAN,
- ШИМ сигнал GPU_FAN (не рекомендуется),
- скорость вращения вентилятора видеокарты через оптический датчик,
- датчик температуры, размещенный в нагреваемом месте.

Выходные сигналы делятся на 4 группы. Минимальный сигнал соответствует минимальной скорости вращения подключенного вентилятора, далее кубическая зависимость от заполнения ШИМ.

## Используемые компоненты

Состав основного устройства:
- Arduino pro mini с измененными частотами таймеров,
- 1 высокоточный датчик температуры Dallas DS18B20,
- 1 кнопка с фиксацией,
- 1 оптический щелевой датчик, который надо переделать для чтения сверху,
- 4 разъема для подключения вентиляторов 4pin.

Состав дополнительного модуля управления:
- 3 кнопки без фиксации,
- 3 резистора 5 Ом,
- 1 резистор 50 кОм,
- модуль из 4 матриц MAX7219.

## Предполагаемое использование

- при нехватке разъемов на материнской плате,
- установка минимальной скорости вращения,
- управление всеми вентиляторами сразу,
- чтение скорости вращения вентилятора видеокарты,
- переключение в режим с максимальной скоростью.

## Настройка

Для взаимодействия используется серийный порт на скорости `115200` или выносной дисплей с кнопками.

Доступные команды:
- `show_pulses` - показать последние значение ШИМ сигналов
- `show_temp` - показать последнее значения датчика температуры
- `show_optic` - показать последнее значение оптического датчика
- `set_min_temp V` - установить минимальную температуру равной V
- `set_max_temp V` - установить максимальную температуру равной V
- `get_min_temp` - показать текущую минимальную температуру
- `get_max_temp` - показать текущую максимальную температуру
- `set_min_optical V` - установить минимальную скорость для оптического датчика
- `set_max_optical V` - установить максимальную скорость для оптического датчика
- `get_min_optical` - показать минимальную скорость для оптического датчика
- `get_max_optical` - показать максимальную скорость для оптического датчика
- `get_min_pulses` - показать минимальные значения ШИМ
- `get_max_pulses` - показать максимальные значения ШИМ
- `save_params` - сохранить настройки в память
- `set_max_pulse K V` - установить максимальное значение ШИМ (V) для входа (K)
- `set_min_pulse K V` - установить максимальное значение ШИМ (V) для входа (K)
- `reset_min_duties` - поиск минимальной скорости вращения вентиляторов
- `set_min_duty K V` - установление абсолютного минимального значение скважности (V) для выхода (K)
- `get_min_duties K V` - показать абсолютное минимальные значения скважности для выходов
- `switch_debug` - переключение режима вывода в серийный порт системных данных
- `switch_cooling_hold` - переключение значения уровня включения максимальной скорости (на нажатую кнопку или отпущенную)

## TODO list

- найти готовое решение оптического датчика, а не переделывать из щелевого
- режим полного отключения вентиляторов
