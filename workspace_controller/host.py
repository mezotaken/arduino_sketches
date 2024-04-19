import numpy as np
import serial
import time
import dxcam

def capture_screenshot(camera):
    
    return camera.grab()

def get_avg_color(arr):
    return np.rint(np.mean(arr, axis=(0,1))).astype(int)

def equal_split(num, div):
    res = [0]
    s = 0
    for i in range (div):
        s += num // div + (1 if i < num % div else 0)
        res.append(s)
    return res

def get_colors(screen, strip_width, led_w, led_h, is_clockwise):
    h = screen.shape[0]
    w = screen.shape[1]

    top_raw = screen[0:strip_width, 0:w]
    left_raw = screen[0:h, 0:strip_width]
    bottom_raw = screen[h-strip_width:h, 0:w]
    right_raw = screen[0:h, w-strip_width:w]


    horizontal = equal_split(w, led_w)
    vertical = equal_split(h, led_h)
    
    top = np.zeros((led_w, 3)).astype(int)
    bottom = np.zeros((led_w, 3)).astype(int)
    left = np.zeros((led_h, 3)).astype(int)
    right = np.zeros((led_h, 3)).astype(int)

    for i in range(led_w):
        top[i] = get_avg_color(top_raw[:, horizontal[i]: horizontal[i+1]])
        bottom[i] = get_avg_color(bottom_raw[:, horizontal[i]: horizontal[i+1]])
        
    for i in range(led_h):
        left[i] = get_avg_color(left_raw[vertical[i]: vertical[i+1], :])
        right[i] = get_avg_color(right_raw[vertical[i]: vertical[i+1], :])

    res = list()

    if is_clockwise:
        res = left[::-1].flatten().tolist() + top.flatten().tolist() + right.flatten().tolist() + bottom[::-1].flatten().tolist()
    else:
        res = right[::-1].flatten().tolist() + top[::-1].flatten().tolist() + left.flatten().tolist() + bottom.flatten().tolist()
    return res

arduino = serial.Serial(port='COM4', baudrate=115200, timeout=0.1) 
main_monitor = dxcam.create(0, 0)
prev_main = main_monitor.grab()
while True:
    

    main_screen = main_monitor.grab()

    if main_screen is None:
        main_screen = prev_main
    else:
        prev_main = main_screen
    
    main_colors = get_colors(main_screen, 270, 44, 24, is_clockwise=False)

    a = arduino.write(bytes(main_colors))
