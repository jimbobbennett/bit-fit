#include <Arduino.h>
#include <ArduinoBLE.h>
#include <list>
#include <LSM6DS3.h>
#include <SPI.h>
#include <stdarg.h>
#include <Wire.h>

using namespace std;

// This header file comes from the exported Edge Impulse library
#include <seeed-xiao_inferencing.h>

// Create an instance of the LSM6DS3 accelerometer connected via I2C at address 0x6A
LSM6DS3 accelerometer(I2C_MODE, 0x6A);

// Accelerometer values need to be converted from G values to Meters per second
#define CONVERT_G_TO_MS2 9.80665f

// Define the BLE service with a unique ID
BLEService fitTrackService("0d5b7c3c-c235-4b57-bde8-ce079704ce9b");

// Define the BLS characteristic for sending the current activity.
// This is set to be readable, with notification if it changes.
BLEIntCharacteristic currentActivity("bc9f3db2-01f0-4c35-a46c-12cc46611ed8", BLERead | BLENotify);

// The different activities that can be detected
enum Activity
{
    None,
    Rowing,
    Running
};

// Set up the BLE radio transmitter
void setup_ble()
{
    // Start the BLE stack
    if (!BLE.begin())
    {
        Serial.println("Device error");

        while (1)
            ;
    }

    // Set the names
    BLE.setLocalName("FitTrack");
    BLE.setDeviceName("FitTrack");

    // Add the activity characteristic to the service
    fitTrackService.addCharacteristic(currentActivity);

    // Add the Fit Track service to the BLE radio
    BLE.addService(fitTrackService);

    // Start advertising the Fit Track service
    BLE.setAdvertisedService(fitTrackService);

    // Set the current activity to none
    currentActivity.writeValue(None);

    // start advertising
    BLE.advertise();

    Serial.println("Bluetooth device active, waiting for connections...");
}

// The setup code for the Arduino app. This is run once when the board boots up
void setup()
{
    // Start the serial connection
    Serial.begin(115200);

    // Begin the accelerometer
    if (accelerometer.begin() != 0)
    {
        Serial.println("Device error");

        while (1)
            ;
    }
    else
    {
        Serial.println("Device OK!");
    }

    // Start the BLE setup
    setup_ble();
}

// Get the current activity
Activity get_current_activity()
{
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
        return None;
    }

    // Run the classifier
    ei_impulse_result_t result = {0};
    err = run_classifier(&signal, &result);

    // Check for errors
    if (err != EI_IMPULSE_OK)
    {
        Serial.println("ERR: Failed to run classifier");
        return None;
    }

    // print the predictions
    Serial.println("Predictions:");
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++)
    {
        ei_printf("%s:  %.5f\n", result.classification[ix].label, result.classification[ix].value);
    }

    // The current activity is an activity with a probability of over 95%
    // So get the first one with an activity that high. If none are that high, then
    // the activity is None

    int activity_index = -1;

    for (int ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++)
    {
        if (result.classification[ix].value > 0.95)
        {
            activity_index = ix;
        }
    }

    // If we don't find an activity with a probability over 95%, then we have no activity
    // so return None
    if (activity_index == -1)
    {
        return None;
    }

    // Return the highest prediction as an Activity
    if (strcmp("Rowing", result.classification[activity_index].label) == 0)
    {
        return Rowing;
    }
    else if (strcmp("Running", result.classification[activity_index].label) == 0)
    {
        return Running;
    }
    else
    {
        return None;
    }
}

// Track the time to allow for a minimum time between updates
int previousMillis = 0;

// The last activity sent to the BLE service
int lastActivity = None;

// Updates the activity value for the BLE characteristic
void updateActivity(Activity activity)
{
    if (lastActivity != activity)
    {
        lastActivity = activity;

        currentActivity.writeValue(lastActivity);
    }
}

list<Activity> activity_history = {};

const int average_time = 15;
const int required_time_to_be_most_likely = 10;

// Activity detection can be inaccurate, so check over the past few seconds to see if we have
// an activity that is the current one an average of 75% of the time.
Activity get_most_likely_activity(Activity current_activity)
{
    // Add the activity to the history
    activity_history.push_back(current_activity);

    // Ensure we are limited to a fixed time of history data
    while (activity_history.size() > average_time)
    {
        activity_history.pop_front();
    }

    // If we don't have the required time of data, assume no activity
    if (activity_history.size() < average_time)
    {
        return None;
    }

    // Count the number of times each activity occurs
    int running_count = 0;
    int rowing_count = 0;
    int none_count = 0;

    for (list<Activity>::iterator it = activity_history.begin(); it != activity_history.end(); it++)
    {
        if (*it == Running)
        {
            running_count++;
        }
        else if (*it == Rowing)
        {
            rowing_count++;
        }
        else
        {
            none_count++;
        }
    }

    // If we have more than 75% of the time in the current activity, then we are confident
    // that we are in that activity
    if (running_count > required_time_to_be_most_likely)
    {
        return Running;
    }
    else if (rowing_count > required_time_to_be_most_likely)
    {
        return Rowing;
    }
    else
    {
        return None;
    }
}

// Logs the activity to the serial port
void log_activity(Activity activity)
{
    switch (activity)
    {
    case Rowing:
        Serial.println("Rowing");
        break;
    case Running:
        Serial.println("Running");
        break;
    case None:
        Serial.println("None");
        break;
    }
}

// The loop code for the Arduino app. This is run repeatedly
void loop()
{
    // wait for a BLE central
    BLEDevice central = BLE.central();

    // if a central is connected to the peripheral:
    if (central)
    {
        Serial.print("Connected to central: ");
        // print the central's BT address:
        Serial.println(central.address());
        // turn on the LED to indicate the connection:
        digitalWrite(LED_BUILTIN, LOW);

        // check the battery level every 200ms
        // while the central is connected:
        while (central.connected())
        {
            long currentMillis = millis();
            // if 1000ms have passed, check the activity level:
            if (currentMillis - previousMillis >= 1000)
            {
                previousMillis = currentMillis;

                Serial.println("Getting current activity...");

                // Get the current activity
                Activity activity = get_current_activity();

                // Log the current activity
                Serial.print("Current activity is ");
                log_activity(activity);

                activity = get_most_likely_activity(activity);

                // Log the most likely average activity
                Serial.print("Most likely average activity is ");
                log_activity(activity);

                // Update the activity in the BLE characteristic
                updateActivity(activity);
            }
        }
        // when the central disconnects, turn off the LED:
        digitalWrite(LED_BUILTIN, HIGH);
        Serial.print("Disconnected from central: ");
        Serial.println(central.address());
    }
}
