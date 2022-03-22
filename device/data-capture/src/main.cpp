#include "LSM6DS3.h"
#include "Wire.h"

// Create an instance of the LSM6DS3 accelerometer connected via I2C at address 0x6A
LSM6DS3 accelerometer(I2C_MODE, 0x6A);

// Accelerometer values need to be converted from G values to Meters per second
#define CONVERT_G_TO_MS2 9.80665f

// Data needs to be sent at a standard frequency of 50Hz so the data
// forwarder can capture data at a fixed rate
#define FREQUENCY_HZ 50
#define INTERVAL_MS (1000 / (FREQUENCY_HZ + 1))

// Track the last time data was sent to the serial port to fix the output at 50Hz
static unsigned long last_interval_ms = 0;

// The setup code for the Arduino app. This is run once when the board boots up
void setup()
{
    // Start the serial output and wait for a connection on the other side
    Serial.begin(115200);
    while (!Serial);

    // Begin the accelerometer
    if (accelerometer.begin() != 0)
    {
        Serial.println("Device error");
    }
    else
    {
        Serial.println("Device OK!");
    }
}

// The loop code for the Arduino app. This is run repeatedly
void loop()
{
    // Check if we have been enough time to send data at 50Hz
    if (millis() > last_interval_ms + INTERVAL_MS)
    {
        // Get the current time stamp
        last_interval_ms = millis();

        // Print accelerometer data to the serial port for the data forwarder to read
        Serial.print(accelerometer.readFloatGyroX() * CONVERT_G_TO_MS2, 4);
        Serial.print('\t');
        Serial.print(accelerometer.readFloatGyroY() * CONVERT_G_TO_MS2, 4);
        Serial.print('\t');
        Serial.println(accelerometer.readFloatGyroZ() * CONVERT_G_TO_MS2, 4);
    }
}