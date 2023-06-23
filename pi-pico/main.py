from machine import Pin, PWM, ADC, SoftI2C, I2C
from bmp180 import *
import utime
import _thread

PINR = 13
PIN_Y1 = 12
PIN_Y2 = 11
PIN_Y3 = 10

ledR = Pin(PINR,Pin.OUT)
ledY1 = Pin(PIN_Y1,Pin.OUT)
ledY2 = Pin(PIN_Y2,Pin.OUT)
ledY3 = Pin(PIN_Y3,Pin.OUT)

Alt_Error_Pin = Pin(21, Pin.OUT)
Bat_Error_Pin = Pin(20, Pin.OUT)


def starting_sequence():
    Alt_Error_Pin.low()
    Bat_Error_Pin.low()
    led = [ledR, ledY1, ledY2, ledY3]
    for i in range(4):
        led[i].value(1)
        utime.sleep_ms(500)
            
    for j in range(4):
        led[j].value(0)
        


def measure_voltage(adcPin):
    conversion_factor = 3.3/(65535)
    return adcPin.read_u16() * conversion_factor

pwm_exist = False
def voltage_indicate(adcPin, ledR_a, ledY1_a, ledY2_a, ledY3_a):
    
    global pwm_exist
    if (pwm_exist):
        # clear PWM Pin
        ledY1 = Pin(PIN_Y1,Pin.OUT)
        
        pwm_exist = False
    
    
    # Find average voltage
    meas_num = 10
    volt_sum = 0
    for i in range(meas_num):
        volt_sum += measure_voltage(adcPin)
        
    volt = (volt_sum/meas_num)*2
    print(volt)
    
    if (volt > 4):
        #turn on ledY1_a, ledY2_a, ledY3_a
        ledY1_a.value(1)
        ledY2_a.value(1)
        ledY3_a.value(1)
            
    elif (volt > 3.7):
        #turn on ledY1_a, ledY2_a
        #turn off ledY3_a
        ledY1_a.value(1)
        ledY2_a.value(1)
        ledY3_a.value(0)
        
    elif (volt > 3.55):
        # turn on ledY1_a
        # turn off ledY2_a, ledY3_a
        ledY1_a.value(1)
        ledY2_a.value(0)
        ledY3_a.value(0)
            
    elif (volt > 3.3):
        ledY1_a.value(0)
        ledY2_a.value(0)
        ledY3_a.value(0)
            
        # global pwm_exist
        pwm_exist = True
        
        MAX_DUTY_CYCLE = 2**16 - 1 #unsigned int (65535)
        frequency = 10 #hertz (cycles/second)
        period = 1/frequency
        duty_cycle_percentage = 50.0 #percentage of the time the pin is high
        duty_cycle = MAX_DUTY_CYCLE * (duty_cycle_percentage/100)
        pwm = PWM(ledY1_a)
        pwm.freq(frequency)
        pwm.duty_u16(int(duty_cycle))
    else:
        return False
    
    return True


        
    
# Sensor
CALIBRATION_TIME_IN_MS = 1000

def AverageAltitude(bmp, measTime = 10):
    '''Sensor bmp take average altitude over a period of measTime'''
    time = 0
    height = 0
    i = 0
    while True:
        height += float(bmp.altitude)
        i += 1
        time += 1
        if (time > measTime):
            break
    return height/i


def CalibrateAltitude(bmp):
    '''Calculate initial (reference height) by taking average over a period spceifed in CALIBRATION_TIME_IN_MS'''
    refAlt = AverageAltitude(bmp, CALIBRATION_TIME_IN_MS)
    flag = True
    print("Alt: " + str(refAlt))
    return refAlt

ALT_MAX = 1.82 # meters - 1.82m = 6ft
ALT_MIN = -2

# Set up
def main():
    '''main code sequence'''
    #Variable declaration and assignment
    i2c = SoftI2C(scl = Pin(5), sda = Pin(4))
    bmp = BMP180(i2c)
    global pwm_exist, ledY1
    
    # Reset all LED to 0
    ledR.value(0)
    ledY1.value(0)
    ledY2.value(0)
    ledY3.value(0)
    
    # Reset all Error Pin
    Bat_Error_Pin.low()
    Alt_Error_Pin.low()
    
    # Calculate reference (initial) altitude
    refAlt = CalibrateAltitude(bmp)
    starting_sequence()
    
    # Define Pin for Battery Reading
    BatteryRead = ADC(27)
    
    # Buffer measurements for Altimeter
    for i in range(5):
        AverageAltitude(bmp)
    
    # initialize error count for altimeter
    error_count = 0
    
    # Offset for calibration
    offset = 0.1
    
    while True:
        # ALTIMETER CHECK
        alt = AverageAltitude(bmp)
        print (alt - refAlt) #print out altitude for debugging/monitoring
        
        if ((alt - refAlt) > (ALT_MAX+offset)) or ((alt - refAlt) < (ALT_MIN+offset)):
            error_count = error_count + 1 # update error count
        else:
            error_count = 0
        
        if (error_count > 10):
            #trigger error mechanism when error > 10
            print("Altitude")
            Alt_Error_Pin.high() #Communicate to ESP32
            break
        
        
        # VOLTAGE CHECK
        if (not voltage_indicate(BatteryRead, ledR, ledY1, ledY2, ledY3)):
            Bat_Error_Pin.high() #Communicate to ESP32
            print("Battery")
            break
    
    # Pico Error mechanism
    if (pwm_exist):
        #clear pwm pin
        ledY1 = Pin(PIN_Y1,Pin.OUT)
    ledR.value(1)
    ledY1.value(0)
    ledY2.value(0)
    ledY3.value(0)
    return

    
    
