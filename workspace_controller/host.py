import numpy as np
import serial
import time
import dxcam


def get_avg_color(arr):
    return list(np.rint(np.mean(arr, axis=(0,1))).astype(np.uint8))


def equal_split(num, div):
    res = [0]
    s = 0
    for i in range (div):
        s += num // div + (1 if i < num % div else 0)
        res.append(s)
    return res

# Выгоднее отсылать сразу же, чем набрать сначала всё, а потом отсылать пачкой
# Отклик получается меньше, визуально заметно. ФПС 35-40, больше не позволяет I/O с Ардуино
# Оптимизирована отправка - только для половины светодиодов считаем и отправляем настоящие данные.
# Между ними аппроксимируем, чтобы было в 2 раза меньше данных на усреднение и пересылку. Уменьшать далее уже менее профитно
def color_picker(arduino, screen, strip_width, led_w, led_h):
    h = screen.shape[0]
    w = screen.shape[1]

    top_raw = screen[0:strip_width, 0:w]
    left_raw = screen[0:h, 0:strip_width]
    bottom_raw = screen[h-strip_width:h, 0:w]
    right_raw = screen[0:h, w-strip_width:w]

    horizontal = equal_split(w, led_w)
    vertical = equal_split(h, led_h)

    for i in range(led_h - 1, -1, -2):
        arduino.write(bytes(get_avg_color(right_raw[vertical[i]: vertical[i+1], :])))

    for i in range(led_w - 1, -1, -2):
        arduino.write(bytes(get_avg_color(top_raw[:, horizontal[i]: horizontal[i+1]])))
        
    for i in range(0, led_h, 2):
        arduino.write(bytes(get_avg_color(left_raw[vertical[i]: vertical[i+1], :])))      

    for i in range(0, led_w, 2):
        arduino.write(bytes(get_avg_color(bottom_raw[:, horizontal[i]: horizontal[i+1]])))

# Порт зависит от разъёма USB
arduino = serial.Serial(port='COM4', baudrate=115200, timeout=0.1) 
main_monitor = dxcam.create(0, 0)
prev_main = main_monitor.grab()

# Полоска в пикселях, по которой берём цвета для ambilight
strip_width = 256

# Всего - 136 светодиодов по периметру монитора
leds_h = 24
leds_w = 44

# Необходима задержка, т.к. подсоединение по COM-порту всегда ребутает ардуинку,
# И если она не успеет загрузится к моменту, когда надо начинать читать, 
# то начальная часть данных отправится в мусор, появится оффсет и цвета будут не те
time.sleep(3)

while True:
    main_screen = main_monitor.grab()
    if main_screen is not None:
        color_picker(arduino, main_screen, 256, 44, 24)