from machine import Pin, PWM

class Motor:
    def __init__(self, ena_pin, in1_pin, in2_pin, freq=5000):  # Changed default to 100 Hz
        self.ena = PWM(Pin(ena_pin))
        self.in1 = Pin(in1_pin, Pin.OUT)
        self.in2 = Pin(in2_pin, Pin.OUT)
        self.ena.freq(freq)
        
    def update_speed(self, speed):  # speed -100 - +100
        if speed > 0:
            self.in1.value(1)
            self.in2.value(0)
        else:
            self.in1.value(0)
            self.in2.value(1)
        duty = abs(int(speed * 655.35))
        self.ena.duty_u16(duty)
        
    def stop(self):
        self.in1.value(0)
        self.in2.value(0)
        self.ena.duty_u16(0)