from machine import Pin, PWM
import utime  

pwm = PWM(Pin(17))
pwm.freq(5000)

while True:
    # Ramp up
    for duty in range(65535):
        pwm.duty_u16(duty)
        utime.sleep_us(1)  # 1 microsecond delay
    
    # Ramp down
    for duty in range(65535, 0, -1):
        pwm.duty_u16(duty)
        utime.sleep_us(1)  # 1 microsecond delay


