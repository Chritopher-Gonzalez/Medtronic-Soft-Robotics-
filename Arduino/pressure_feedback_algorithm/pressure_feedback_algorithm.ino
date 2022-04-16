/*
 * @file    pressure_feedback_algorithm.ino
 * @author  CU Boulder Medtronic Team 7
 * @brief   Basic method to hit desired pressure in any channel
 */

#include <stdlib.h>
#include <string.h>
#include <Wire.h>
#include "Adafruit_MPRLS.h"

// Channel/pressure related defines
#define NUM_CHANNELS  3
#define ON 1
#define OFF 0
#define DEFAULT_PRESSURE 12.25

/*
 * IMPORTANT: These are two different tolerances used in the bang-bang controller.
 * The reason these are different is to try and reduce resonance and promote stability.
 * Downside is that signal tracking becomes choppy.
 * 
 * PRESSURE_TOLERANCE is how close your pressure sensor reading has to be to
 * the desired value in order to cause a state transition from INFLATE or DEFLATE
 * to HOLD.
 *
 * PRESSURE_HOLD_TOLERANCE is how far off the desired pressure you have to be
 * in order to cause a state transition from HOLD to DEFLATE or INFLATE
 *
 */
#define PRESSURE_TOLERANCE 0.03
#define PRESSURE_HOLD_TOLERANCE 0.05

// Solenoid Valves
#define SOLENOID_CLOSED LOW
#define SOLENOID_OPEN HIGH
// Because we are digitally manipulating these valves to simulate PWM,
// We define a cycle time and a duty cycle.
#define SOLENOID_CYCLE_TIME_MS 5000
#define POSITIVE_SOLENOID_DUTY_CYCLE .55
#define POSITIVE_SOLENOID_DUTY_CYCLE_INCREASED .65
#define NEGATIVE_SOLENOID_DUTY_CYCLE .35

/*
 * Various pump PWM values. Due to hardware variability,
 * we run some pumps higher than others.
 */
#define PUMP_PWM 130
#define PUMP_PWM_INCREASED 130
#define PUMP_OFF 0

// Serial related defines
#define MSG_HEADER_SIZE 2
#define MSG_BODY_SIZE 4
#define EXPECTED_MSG_LENGTH (MSG_HEADER_SIZE + MSG_BODY_SIZE)
// Special message for selecting which channels you want to run on
#define CHANNEL_SELECTION_MSG_LENGTH 3

// Enabled when pressure sensor functions are defined here in file
#define LOCAL_PRESSURE_SENSOR_FUNCTIONS

// Pin definitions for the pressure sensor
#define RESET_PIN  -1   // set to any GPIO pin # to hard-reset on begin()
#define EOC_PIN    -1   // set to any GPIO pin to read end-of-conversion by pin
#define TCAADDR    0x70 // address for the mux

// Instantiate mpr class for pressure sensors
Adafruit_MPRLS mpr = Adafruit_MPRLS(RESET_PIN, EOC_PIN);
 
// Possible States
typedef enum {
    INFLATE,
    HOLD,
    DEFLATE
} state;

// Data stored for each channel
struct channelData 
{
    uint8_t active;         // Whether or not channel is active
    state currentState;     // Current state of channel (defiend by 'state' enum)
    float currentPressure;  // Current pressure read from sensors
    float desiredPressure;  // Pressure that controller is trying to reach
    int positivePump;       // I/O Pins for all hardware for the channel
    int negativePump;
    int positiveSolenoid;
    int negativeSolenoid;
    int pumpPWM;            // PWM value for both pumps on the channel
    float positiveSolenoidDutyCycle;
};

channelData channels[NUM_CHANNELS] =
{
    {OFF, HOLD, DEFAULT_PRESSURE, DEFAULT_PRESSURE, 12, 11, 52, 53, PUMP_PWM, POSITIVE_SOLENOID_DUTY_CYCLE},  // Channel 0
    {OFF, HOLD, DEFAULT_PRESSURE, DEFAULT_PRESSURE, 10,  9, 50, 51, PUMP_PWM, POSITIVE_SOLENOID_DUTY_CYCLE_INCREASED},  // Channel 1
    {OFF, HOLD, DEFAULT_PRESSURE, DEFAULT_PRESSURE, 8,   7, 48, 49, PUMP_PWM_INCREASED, POSITIVE_SOLENOID_DUTY_CYCLE_INCREASED}, // Channel 2
};

/*
 * @name  setup
 * @desc  called once on startup
 */
void setup() {
    // scan for mux ports and begin serial communication
    scanner();
  
    // initialize all three pressures sensors
    sensor_initialization();
    
    for (int8_t cNum = 0; cNum < NUM_CHANNELS; cNum++)
    {
        pinMode(channels[cNum].positivePump, OUTPUT);
        pinMode(channels[cNum].negativePump, OUTPUT);
        pinMode(channels[cNum].positiveSolenoid, OUTPUT);
        pinMode(channels[cNum].negativeSolenoid, OUTPUT);
    }

    // Send confirmation on serial line
    Serial.println("Arduino Setup Complete");

    // Initialize all channels to default pressure in case any data is
    // persisting between differnet uploads
    for (int8_t cNum = 0; cNum < NUM_CHANNELS; cNum++)
    {
        channels[cNum].desiredPressure = DEFAULT_PRESSURE;
    }
}

/*
 * @name  loop
 * @desc  called indefinitely
 */
void loop() {

    // Read in serial command if needed...
    // Arduino serial buffer holds 64 bytes
    if (Serial.available() >= EXPECTED_MSG_LENGTH)
    {
        handleCommand();
    }

    // Update I/O if needed based on last command
    for (int8_t cNum = 0; cNum < NUM_CHANNELS; cNum++)
    {
        if(channels[cNum].active)
        {
            channels[cNum].currentPressure = get_pressure(mpr, cNum);

            // Switch for determining whether or not we need to change the state
            switch (channels[cNum].currentState)
            {
                case INFLATE:
                    if ((channels[cNum].currentPressure >= (channels[cNum].desiredPressure - PRESSURE_TOLERANCE)) &&
                        (channels[cNum].currentPressure <= (channels[cNum].desiredPressure + PRESSURE_TOLERANCE)) )
                    {
                        channels[cNum].currentState = HOLD;
                    }
                    else if (channels[cNum].currentPressure >= (channels[cNum].desiredPressure + PRESSURE_HOLD_TOLERANCE))
                    {
                        channels[cNum].currentState = DEFLATE;
                    }
                    break; 
                    
                case HOLD:
                    {
                        if (channels[cNum].currentPressure <= (channels[cNum].desiredPressure - PRESSURE_HOLD_TOLERANCE))
                        {
                            channels[cNum].currentState = INFLATE;
                        }
                        else if (channels[cNum].currentPressure > (channels[cNum].desiredPressure + PRESSURE_HOLD_TOLERANCE))
                        {
                            channels[cNum].currentState = DEFLATE;
                        }
                    }
                    break;
                    
                case DEFLATE:
                    if ((channels[cNum].currentPressure >= (channels[cNum].desiredPressure - PRESSURE_TOLERANCE)) &&
                        (channels[cNum].currentPressure <= (channels[cNum].desiredPressure + PRESSURE_TOLERANCE)) )
                    {
                        channels[cNum].currentState = HOLD;
                    }
                    else if (channels[cNum].currentPressure <= (channels[cNum].desiredPressure - PRESSURE_HOLD_TOLERANCE))
                    {
                        channels[cNum].currentState = INFLATE;
                    }
                    break;
            }     

            // Switch for acting on current state
            switch (channels[cNum].currentState)
            {
                case INFLATE:
                    // Pumps
                    analogWrite(channels[cNum].positivePump, channels[cNum].pumpPWM);
                    analogWrite(channels[cNum].negativePump, PUMP_OFF);
                    digitalWrite(channels[cNum].negativeSolenoid, SOLENOID_CLOSED);

                    // Simulate PWM on the solenoid valves by digitally manipulating them.
                    digitalWrite(channels[cNum].positiveSolenoid, SOLENOID_OPEN);
                    delayMicroseconds(channels[cNum].positiveSolenoidDutyCycle*SOLENOID_CYCLE_TIME_MS);
                    digitalWrite(channels[cNum].positiveSolenoid, SOLENOID_CLOSED);
                    delayMicroseconds((1-channels[cNum].positiveSolenoidDutyCycle)*SOLENOID_CYCLE_TIME_MS);
                    
                    break; 
                    
                case HOLD:
                    // Solenoids
                    digitalWrite(channels[cNum].positiveSolenoid, SOLENOID_CLOSED);
                    digitalWrite(channels[cNum].negativeSolenoid, SOLENOID_CLOSED);
                    
                    // Pumps
                    analogWrite(channels[cNum].positivePump, PUMP_OFF);
                    analogWrite(channels[cNum].negativePump, PUMP_OFF);
                    break;
                
                case DEFLATE:
                    // Solenoids
                    digitalWrite(channels[cNum].positiveSolenoid, SOLENOID_CLOSED);
                    analogWrite(channels[cNum].positivePump, PUMP_OFF);
                    analogWrite(channels[cNum].negativePump, channels[cNum].pumpPWM); 

                    // Simulate PWM on the solenoid valves by digitally manipulating them.
                    digitalWrite(channels[cNum].negativeSolenoid, SOLENOID_OPEN);
                    delayMicroseconds(NEGATIVE_SOLENOID_DUTY_CYCLE*SOLENOID_CYCLE_TIME_MS);
                    digitalWrite(channels[cNum].negativeSolenoid, SOLENOID_CLOSED);
                    delayMicroseconds((1-NEGATIVE_SOLENOID_DUTY_CYCLE)*SOLENOID_CYCLE_TIME_MS);
                    break;
            }
        } 
    }

}

/*
 * @name    handleCommand
 * @desc    Handle command if found in serial buffer
 * @param   None
 * @return  None
 */
void handleCommand(void)
{
    char header[2];
    char msg[4];

    // Read first two bytes as header
    header[0] = Serial.read();
    header[1] = Serial.read();

    // Read header to find channel number
    uint8_t cNum;
    if (strncmp(header, "c0", 2) == 0)
    {
        cNum = 0;
    }
    else if (strncmp(header, "c1", 2) == 0)
    {
        cNum = 1;
    }
    else if (strncmp(header, "c2", 2) == 0)
    {
        cNum = 2;
    }
    else if (strncmp(header, "sc", 2) == 0)
    {
        handleSelectionCommand();
        return;
    }
    else
    {
        Serial.println("CMD ERROR");
        return;
    }

    // Read in body of message
    for (int8_t i = 0; i < 4; i++)
    {
        msg[i] = Serial.read();
    }

    if (strncmp(msg, "read", 4) == 0)
    {
        // Read pressure command
        channels[cNum].currentPressure = get_pressure(mpr, cNum);
        String pVal = String(channels[cNum].currentPressure, 2);
        Serial.println(pVal);
    }
    else
    {
        // Set pressure command...
        // Contains desired PSI (implied decimal XX.XX) for channels that are enabled
        char pressure[5];
        sprintf(pressure, "%c%c.%c%c", msg[0], msg[1], msg[2], msg[3]);
        channels[cNum].desiredPressure = ((String(pressure)).toFloat());
        // Send confirmation back
        Serial.println("rx");
    }

}

/*
 * @name    handleSelctionCommand
 * @desc    Handle command for channel selection
 * @param   None
 * @return  None
 */
void handleSelectionCommand(void)
{
    for (int8_t cNum = 0; cNum < 3; cNum++)
    {
        char enabled = Serial.read();
        if (enabled == '1')
        {
            channels[cNum].active = ON;
        }
    }
    // Extra character currently not being used
    char reserved = Serial.read();

    // Send confirmation back
    Serial.println("rx");
}

#if defined(LOCAL_PRESSURE_SENSOR_FUNCTIONS)
/*
 * @name    scanner
 * @desc    Scan for connections to the multiplexer 
 * @param   None
 * @return  None
 */
void scanner()
{
    while (!Serial);
    delay(1000);
    Wire.begin();
    
    Serial.begin(115200);
    
    for (uint8_t t=0; t<8; t++) 
    {
        tcaselect(t);

        for (uint8_t addr = 0; addr<=127; addr++) 
        {
            if (addr == TCAADDR) 
            {
                continue;
            }
            Wire.beginTransmission(addr);
            Wire.endTransmission();
        }
    }
}

/*
 * @name    sensor_initialization
 * @desc    Used to initialize each of the pressure sensors
 * @param   None
 * @return  None
 */
void sensor_initialization()
{
    for (uint8_t i = 0; i < 3; i++)
    {
        tcaselect(i);
        if (!mpr.begin())
        {
            // Error Initializing sensor
            while(1);
        }
    }
}

/*
 * @name    tcaselect
 * @desc    Select device TCA is connected to
 * @param   i - which port you want to connect to
 * @return  None
 */
void tcaselect(uint8_t i) {
    if (i > 7)
    {
        return;
    }
   
    Wire.beginTransmission(TCAADDR);
    Wire.write(1 << i);
    Wire.endTransmission();  
}

/*
 * @name    get_pressure
 * @desc    Function to return the pressure readings
 * @param   readDelay - how frequent pressure will be logged
 * @return  None
 */ 
float get_pressure(Adafruit_MPRLS mpr, uint8_t channelNum)
{
    tcaselect(channelNum);
    float pressure_hPa0 = mpr.readPressure();
    float presssure_PSI = (pressure_hPa0 / 68.947572932);
    return presssure_PSI;
}
#endif // LOCAL_PRESSURE_SENSOR_FUNCTIONS
