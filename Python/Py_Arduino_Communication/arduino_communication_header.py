'''
 * @file    arduino_communication_header.py
 * @author  CU Boulder Medtronic Team 7
 * @brief   Methods to control Arduino communication
'''
import time
import serial as pys
import atexit
import logging
import serial.tools.list_ports


SETUP_WAIT_TIME_SEC = 10         # Forcing python to wait 10 seconds before asking the Arduino if connection
                                 # is complete. We want to avoid using the serial line while pressure sensors
                                 # Are being initialized on the arduino side. TODO: Determine if value can be decreased
SETUP_COMPLETE_MSG_LENGTH = 23   # Message length for confirmation sent back by Arduino
MSG_RECIEVED_BY_ARDUINO = "rx"   # Message sent back from Arduino when it has processed a serial message from Python
MSG_RECIEVED_BY_ARDUINO_LENGTH = 4 # Number of bytes expected when Arduino is sending back a confirmation
DEFAULT_PRESSURE_PSI = "1225"    # Pressure near atmospheric in psi, with implied decimal after 2

# Labeling channels
channel0 = 0
channel1 = 1
channel2 = 2

def getPort(deviceName):
    """ use to find port with the given port description

    Parameters
    ----------
    deviceName : string
        name of device

    Returns
    -------
    tuple
        Return tuple where the first element is port name as string, second is device name as string

    Raises
    ------
    NoPortsException
        If the matrix is not numerically invertible.

    """
    COMports = serial.tools.list_ports.comports()

    #check if there is aviable ports else throw exception
    if not COMports:
        raise Exception("Could Not Find Any Ports")

    for port in COMports:
        if deviceName in port.description:
            return (port.device, port.description)

    raise Exception("Could not find device with {} as device name".format(deviceName))

class arduino:
    def __init__(self, c0, c1, c2):
        # Enable channels (0 is OFF and 1 is ON)
        self.c0_enabled = c0
        self.c1_enabled = c1
        self.c2_enabled = c2

        self.ser = pys.Serial()
        self.startCommunication()

        atexit.register(self.close)

    def startCommunication(self):
        # Open Serial to Arduino
        self.ser.baudrate = 115200
        self.ser.port = getPort('Serial')[0]
        self.ser.open()
        if (self.ser.is_open != True):
            print("Could not open serial")
            quit()

        # Give time for Arduino to use serial line to setup pressure sensors
        print("Waiting for Arduino Initialization...")
        time.sleep(SETUP_WAIT_TIME_SEC)
        while True:
            if(self.ser.in_waiting >= SETUP_COMPLETE_MSG_LENGTH):
                arduinoSetup = self.ser.readline().decode('utf-8')
                print(arduinoSetup)
                if(arduinoSetup.rstrip() == "Arduino Setup Complete"):
                    print("Arduino Serial Established")
                    break

<<<<<<<< HEAD:Python/Py_Arduino_Communication/arduino_control/arduino_control.py
    def getActualPressure(self):
        '''
========
        # Turn on the channels we want to control
        enablePumps = 'sc{}{}{}'.format(self.c0_enabled, self.c1_enabled, self.c2_enabled)
        print("Writing command to arduino: ", enablePumps.encode('utf-8'))
        self.ser.write(enablePumps.encode('utf-8'))
        while True:
            if (self.ser.in_waiting == MSG_RECIEVED_BY_ARDUINO_LENGTH):
                if ((self.ser.readline().decode('utf-8')).rstrip() == MSG_RECIEVED_BY_ARDUINO):
                    break

    def getActualPressure(self, channelNum):
        '''     
>>>>>>>> 8b7f2ce (initial commit for 3 channel update):Python/Py_Arduino_Communication/arduino_communication_header.py
        Obtains actual pressure from pressure sensor
        '''
        readPressure = 'c{}{}'.format(channelNum, "read")
        self.ser.write(readPressure.encode('utf-8'))
        while True:
            if(self.ser.in_waiting > 4):
                P_act = float(self.ser.readline().decode('utf-8'))  # convert pressures from string to float
                break

<<<<<<<< HEAD:Python/Py_Arduino_Communication/arduino_control/arduino_control.py
        return self.P_act
========
        return P_act
>>>>>>>> 8b7f2ce (initial commit for 3 channel update):Python/Py_Arduino_Communication/arduino_communication_header.py

    def sendDesiredPressure(self, channelNum, desiredPressure):
        '''
        convert desiredPressure and send this pressure into the Arduino
        '''

        # Turn float pressure into strings of right format to send to Adruino
        desiredPressure = round(desiredPressure, 2) # round desired pressure to 2 decimal points
        lessThanTen = False                         # checks to see if the P_des is less than 10

        if desiredPressure < 10.0:
            lessThanTen = True

        command = str(desiredPressure)     # turn float into a string
        command = command.replace('.', '') # Remove decimal before sending over serial

        # append leading zero to pressures less than 10 psi
        if lessThanTen:
            command = '0' + command

        # add following zero if we are missing the second decimal point
        if len(command) == 3:
            command = command + '0'

        # Send over desired pressure to Arduino
        sendPressure = 'c{}{}'.format(channelNum, command)
        print("Writing command to arduino: ", sendPressure.encode('utf-8'))
        self.ser.write(sendPressure.encode('utf-8'))
        while True:
            if (self.ser.in_waiting == MSG_RECIEVED_BY_ARDUINO_LENGTH):
                if ((self.ser.readline().decode('utf-8')).rstrip() == MSG_RECIEVED_BY_ARDUINO):
                    break

    def close(self):
        # Send command to reset to default pressure before terminating
<<<<<<<< HEAD:Python/Py_Arduino_Communication/arduino_control/arduino_control.py
        self.ser.write(DEFAULT_PRESSURE_PSI.encode('utf-8'))
        self.ser.close()
========
        if self.c0_enabled:
            self.sendDesiredPressure(channel0, DEFAULT_PRESSURE_PSI)
        if self.c1_enabled:
            self.sendDesiredPressure(channel1, DEFAULT_PRESSURE_PSI)
        if self.c2_enabled:
            self.sendDesiredPressure(channel2, DEFAULT_PRESSURE_PSI)

        self.ser.close()
>>>>>>>> 8b7f2ce (initial commit for 3 channel update):Python/Py_Arduino_Communication/arduino_communication_header.py
