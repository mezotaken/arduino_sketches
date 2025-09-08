import os
import sys
import threading
import serial
import dxcam
import numpy as np
from serial.tools import list_ports
from PyQt6 import QtWidgets, uic


def resource_path(relative_path):
    if hasattr(sys, '_MEIPASS'):
        return os.path.join(sys._MEIPASS, relative_path)
    return os.path.join(os.path.abspath('.'), relative_path)


class AmbilightSender:
    def __init__(self, leds_h, leds_w, strip_width, port):
        # Ширина полоска в пикселях, по которой берём цвета для ambilight
        self.strip_width = strip_width
        # Кол-во светодиодов на сторонах монитора
        self.leds_h = leds_h
        self.leds_w = leds_w
        # Подключение к ардуинке
        self.arduino = serial.Serial(port, baudrate=115200, timeout=3)
        data = self.arduino.read(1)
        if len(data) != 1 or data[0] != 100:
            raise ConnectionError('Connection error')
        # Объект для сбора кадров с монитора
        self.main_monitor = dxcam.create(0, 0)
        self.prev_main = self.main_monitor.grab()


    def get_avg_color(self, arr):
        return list(np.rint(np.mean(arr, axis=(0,1))).astype(np.uint8))


    def equal_split(self, num, div):
        res = [0]
        s = 0
        for i in range (div):
            s += num // div + (1 if i < num % div else 0)
            res.append(s)
        return res


    def send_colors(self, arduino, screen, strip_width, led_w, led_h):
        # Выгоднее отсылать сразу же, чем набрать сначала всё, а потом отсылать пачкой
        # Отклик получается меньше, визуально заметно. ФПС 35-40, больше не позволяет I/O с Ардуино
        # Оптимизирована отправка - только для половины светодиодов считаем и отправляем настоящие данные.
        # Между ними аппроксимируем, чтобы было в 2 раза меньше данных на усреднение и пересылку. Уменьшать далее уже менее профитно
        h = screen.shape[0]
        w = screen.shape[1]

        top_raw = screen[0:strip_width, 0:w]
        left_raw = screen[0:h, 0:strip_width]
        bottom_raw = screen[h-strip_width:h, 0:w]
        right_raw = screen[0:h, w-strip_width:w]

        horizontal = self.equal_split(w, led_w)
        vertical = self.equal_split(h, led_h)

        for i in range(led_h - 1, -1, -2):
            arduino.write(bytes(self.get_avg_color(right_raw[vertical[i]: vertical[i+1], :])))

        for i in range(led_w - 1, -1, -2):
            arduino.write(bytes(self.get_avg_color(top_raw[:, horizontal[i]: horizontal[i+1]])))

        for i in range(0, led_h, 2):
            arduino.write(bytes(self.get_avg_color(left_raw[vertical[i]: vertical[i+1], :])))

        for i in range(0, led_w, 2):
            arduino.write(bytes(self.get_avg_color(bottom_raw[:, horizontal[i]: horizontal[i+1]])))


    def start(self):
        self.sending_data = True
        while self.sending_data:
            main_screen = self.main_monitor.grab()
            if main_screen is not None:
                try:
                    self.send_colors(self.arduino, main_screen, self.strip_width, self.leds_w, self.leds_h)
                except serial.SerialException:
                    self.sending_data = False


    def stop(self):
        self.sending_data = False



class AmbilightApp(QtWidgets.QMainWindow):
    def __init__(self):
        self.data = None

        # Загрузка UI
        super().__init__()
        uic.loadUi(resource_path('ambilight_host.ui'), self)

        self.strip_width: QtWidgets.QSpinBox
        self.leds_h: QtWidgets.QSpinBox
        self.leds_w: QtWidgets.QSpinBox
        self.device: QtWidgets.QLabel
        self.state_control: QtWidgets.QPushButton
        self.state_control.toggled.connect(self.toggle_sending)

        self.amb_sender = None

        self.scan_control: QtWidgets.QPushButton
        self.scan_control.clicked.connect(self.scan)
        self.scan()


    def scan(self):
        self.device.setText('Не найдено')
        relevant_desc = 'USB-SERIAL CH340'
        for p in list_ports.comports():
            if relevant_desc in p.description:
                self.device.setText(p.device)
                break
        if self.device.text() == 'Не найдено':
            self.state_control.setDisabled(True)


    def toggle_sending(self, checked):
        if checked:
            if self.amb_sender is None:
                try:
                    self.amb_sender = AmbilightSender(self.leds_h.value(), self.leds_w.value(), self.strip_width.value(), self.device.text())
                except ConnectionError:
                    QtWidgets.QMessageBox(parent=self, title='Ошибка', text=f'Подключение к {self.device.text()} прошло неудачно').exec()
            threading.Thread(target=self.amb_sender.start, daemon=True).start()
        else:
            self.amb_sender.stop()


if __name__ == "__main__":
    app = QtWidgets.QApplication(sys.argv)
    window = AmbilightApp()
    window.show()
    sys.exit(app.exec())
