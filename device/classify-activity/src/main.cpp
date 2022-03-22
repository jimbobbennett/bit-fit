// #include "LSM6DS3.h"
// #include "Wire.h"

// //Create a instance of class LSM6DS3
// LSM6DS3 myIMU(I2C_MODE, 0x6A);    //I2C device address 0x6A

// #define CONVERT_G_TO_MS2    9.80665f
// #define FREQUENCY_HZ        50
// #define INTERVAL_MS         (1000 / (FREQUENCY_HZ + 1))

// static unsigned long last_interval_ms = 0;

// void setup() {
//     // put your setup code here, to run once:
//     Serial.begin(115200);
//     while (!Serial);
//     //Call .begin() to configure the IMUs
//     if (myIMU.begin() != 0) {
//         Serial.println("Device error");
//     } else {
//         Serial.println("Device OK!");
//     }
// }

// void loop() {

//     if (millis() > last_interval_ms + INTERVAL_MS) {
//         last_interval_ms = millis();

//         Serial.print(myIMU.readFloatGyroX() * CONVERT_G_TO_MS2,4);
//         Serial.print('\t');
//         Serial.print(myIMU.readFloatGyroY() * CONVERT_G_TO_MS2,4);
//         Serial.print('\t');
//         Serial.println(myIMU.readFloatGyroZ() * CONVERT_G_TO_MS2,4);
//     }
// }

/* Edge Impulse Arduino examples
 * Copyright (c) 2021 EdgeImpulse Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* Includes ---------------------------------------------------------------- */
#include <SPI.h>
#include <seeed-xiao_inferencing.h>
#include <stdarg.h>
#include <Arduino.h>
#include <Wire.h>
#include <LSM6DS3.h>

/* Constant defines -------------------------------------------------------- */
#define CONVERT_G_TO_MS2 9.80665f

LSM6DS3 myIMU(I2C_MODE, 0x6A); // I2C device address 0x6A

/* Private variables ------------------------------------------------------- */
static bool debug_nn = false; // Set this to true to see e.g. features generated from the raw signal

void setup()
{
    // put your setup code here, to run once:
    Serial.begin(115200);
    Serial.println("Edge Impulse Inferencing Demo");

    if (myIMU.begin() != 0)
    {
        Serial.println("Device error");
    }
    else
    {
        Serial.println("Device OK!");
    }

    Serial.println();
    Serial.println("Acceleration in g's");
    Serial.println("X\tY\tZ");
    if (EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME != 3)
    {
        ei_printf("ERR: EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME should be equal to 3 (the 3 sensor axes)\n");
        return;
    }
    // #if CFG_DEBUG
    //     // Blocking wait for connection when debug mode is enabled via IDE
    //     while (!Serial)
    //         yield();
    // #endif
}

/**
 * @brief      Printf function uses vsnprintf and output using Arduino Serial
 *
 * @param[in]  format     Variable argument list
 */
void ei_printf(const char *format, ...)
{
    static char print_buf[1024] = {0};

    va_list args;
    va_start(args, format);
    int r = vsnprintf(print_buf, sizeof(print_buf), format, args);
    va_end(args);

    if (r > 0)
    {
        Serial.write(print_buf);
    }
}

/**
 * @brief      Get data and run inferencing
 *
 * @param[in]  debug  Get debug info if true
 */
void loop()
{
    ei_printf("\nStarting inferencing in 2 seconds...\n");

    delay(2000);

    ei_printf("Sampling...\n");

    // Allocate a buffer here for the values we'll read from the IMU
    float buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE] = {0};

    for (size_t ix = 0; ix < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; ix += 3)
    {
        // Determine the next tick (and then sleep later)
        uint64_t next_tick = micros() + (EI_CLASSIFIER_INTERVAL_MS * 1000);

        buffer[ix + 0] = myIMU.readFloatGyroX() * CONVERT_G_TO_MS2;
        buffer[ix + 1] = myIMU.readFloatGyroY() * CONVERT_G_TO_MS2;
        buffer[ix + 2] = myIMU.readFloatGyroZ() * CONVERT_G_TO_MS2;

        delayMicroseconds(next_tick - micros());
    }

    // Turn the raw buffer in a signal which we can the classify
    signal_t signal;
    int err = numpy::signal_from_buffer(buffer, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);
    if (err != 0)
    {
        ei_printf("Failed to create signal from buffer (%d)\n", err);
        return;
    }

    // Run the classifier
    ei_impulse_result_t result = {0};

    err = run_classifier(&signal, &result, debug_nn);
    if (err != EI_IMPULSE_OK)
    {
        ei_printf("ERR: Failed to run classifier (%d)\n", err);
        return;
    }

    // print the predictions
    ei_printf("Predictions ");
    ei_printf("(DSP: %d ms., Classification: %d ms., Anomaly: %d ms.)",
              result.timing.dsp, result.timing.classification, result.timing.anomaly);
    ei_printf(": \n");
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++)
    {
        ei_printf("    %s: %.5f\n", result.classification[ix].label, result.classification[ix].value);
    }
#if EI_CLASSIFIER_HAS_ANOMALY == 1
    ei_printf("    anomaly score: %.3f\n", result.anomaly);
#endif
    // if (result.classification[0].value > 0.7)
    // {
    //     Serial.write
    // }

    // if (result.classification[1].value > 0.7)
    // {
    //     digitalWrite(RED_ledPin, HIGH); //圆           蓝色
    //     digitalWrite(BLUE_ledPin, LOW);
    //     digitalWrite(GREEN_ledPin, HIGH);
    //     u8g2.clearBuffer();
    //     u8g2.drawCircle(63, 38, 22, U8G2_DRAW_ALL);
    //     u8g2.drawStr(48, 42, "circle");
    //     u8g2.sendBuffer();
    //     bleuart.write(buf2, 6);
    // }

    // if (result.classification[2].value > 0.7)
    // {
    //     digitalWrite(RED_ledPin, HIGH);
    //     digitalWrite(BLUE_ledPin, HIGH);
    //     digitalWrite(GREEN_ledPin, LOW); //左右             绿色
    //     u8g2.clearBuffer();
    //     u8g2.drawTriangle(44, 30, 44, 50, 31, 40);
    //     u8g2.drawTriangle(82, 30, 82, 50, 95, 40);
    //     u8g2.drawStr(46, 43, "swing");
    //     u8g2.sendBuffer();
    //     bleuart.write(buf3, 5);
    // }
}
