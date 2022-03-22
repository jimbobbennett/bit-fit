# BitFit

![3 cartoon raccoons on a 3-person bike](./img/BIT_TEAMWORK.png)

BitFit is a new Fitness tracker, from Bit - the mascot of the Microsoft Developer Relations team!

BitFit consists of a wearable using the [Seeed XIAO BLE sense board](https://www.seeedstudio.com/Seeed-XIAO-BLE-Sense-nRF52840-p-5253.html), a microcontroller that supports bluetooth as well as a range of sensors. This is then paired to a mobile app, and sent to Azure IoT Central.

The wearable detects what activity you are doing using a Tiny ML model built using [Edge Impulse](https://www.edgeimpulse.com).

## Project structure

The project is divided into 3 sections:

* `device` - this folder contains device code for capturing data to train a model, detecting teh activity the user is doing, and sending this over bluetooth
* `data` - this folder contains the data used to train the model, exported from Edge Impulse
* `mobile-app` - this folder contains the mobile app code.

### Device code

The device code is built and deployed using PlatformIO and Visual Studio Code.

Out of the box, PlatformIO doesn't currently support the Seeed XIAO board, so you need to make a few changes to PlatformIO by following the guide in [the Working with Seeed XIAO BLE Sense and PlatformIO IDE blog post](https://medium.com/@alwint3r/working-with-seeed-xiao-ble-sense-and-platformio-ide-5c4da3ab42a3) from [Alwin Arrasyid](https://medium.com/@alwint3r).

### Data

The data in this folder was generated using the XIAO BLE Sense and Edge Impulse. Each data item has 1 second of motion data, either rowing or running.

This data is divided into 2 sections, training and testing data. The training data was used to train the model, the testing data to test it

### Mobile app
