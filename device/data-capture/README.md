# Data capture

The Seeed XIAO BLE is not natively supported in Edge Impulse. This means to capture data, you need to code on the device to send accelerometer data to the serial port, and the Edge Impulse data forwarder to forward this data on to your Edge Impulse project.

This project is the code you need to run on the device. Build and upload this, then run the data forwarder using:

```sh
edge-impulse-data-forwarder
```

You will need the [Edge Impulse CLI](https://docs.edgeimpulse.com/docs/cli-installation) installed to run this.
