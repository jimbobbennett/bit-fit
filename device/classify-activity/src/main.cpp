#include <SPI.h>
#include <stdarg.h>
#include <Arduino.h>
#include <Wire.h>
#include <LSM6DS3.h>

// This header file comes from the exported Edge Impulse library
#include <seeed-xiao_inferencing.h>

// Create an instance of the LSM6DS3 accelerometer connected via I2C at address 0x6A
LSM6DS3 accelerometer(I2C_MODE, 0x6A);

// Accelerometer values need to be converted from G values to Meters per second
#define CONVERT_G_TO_MS2 9.80665f

// The setup code for the Arduino app. This is run once when the board boots up
void setup()
{
    // Start the serial connection
    Serial.begin(115200);

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
    Serial.println();
    Serial.println("Sampling...");

    // Allocate a buffer here for the values we'll read from the IMU
    float buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE] = {0};

    // Read values from the accelerometer to fill up a frame buffer of the required size
    for (size_t ix = 0; ix < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; ix += 3)
    {
        // Determine the next tick (and then sleep later)
        uint64_t next_tick = micros() + (EI_CLASSIFIER_INTERVAL_MS * 1000);

        // Read the accelerometer data into the buffer
        buffer[ix + 0] = accelerometer.readFloatGyroX() * CONVERT_G_TO_MS2;
        buffer[ix + 1] = accelerometer.readFloatGyroY() * CONVERT_G_TO_MS2;
        buffer[ix + 2] = accelerometer.readFloatGyroZ() * CONVERT_G_TO_MS2;

        // Sleep to ensure we are reading at 50Hz, the expected data rate of the model
        delayMicroseconds(next_tick - micros());
    }

    // Turn the raw buffer in a signal which we can the classify
    signal_t signal;
    int err = numpy::signal_from_buffer(buffer, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);
    if (err != 0)
    {
        Serial.println("Failed to create signal from buffer");
        return;
    }

    // Run the classifier
    ei_impulse_result_t result = {0};
    err = run_classifier(&signal, &result);

    // Check for errors
    if (err != EI_IMPULSE_OK)
    {
        Serial.println("ERR: Failed to run classifier");
        return;
    }

    // print the predictions
    Serial.println("Predictions:");
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++)
    {
        ei_printf("%s:  %.5f\n", result.classification[ix].label, result.classification[ix].value);
    }
}
