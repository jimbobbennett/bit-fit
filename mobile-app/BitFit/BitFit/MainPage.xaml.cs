using System;
using System.Text;
using System.Threading.Tasks;
using Microsoft.Azure.Devices.Client;
using Microsoft.Azure.Devices.Provisioning.Client;
using Microsoft.Azure.Devices.Provisioning.Client.Transport;
using Microsoft.Azure.Devices.Shared;
using Plugin.BLE;
using Plugin.BLE.Abstractions.Contracts;
using Plugin.BLE.Abstractions.EventArgs;
using Plugin.BLE.Abstractions.Exceptions;
using Xamarin.Essentials;
using Xamarin.Forms;

namespace BitFit
{
    public partial class MainPage : ContentPage
    {
        // Put your IoT Central connection details here
        const string deviceId = "";
        const string idScope = "";
        const string key = "";

        // The bluetooth device for the BItFit fitness trascker
        IDevice fitTrack;

        // The FitTrack bluetooth service on the device
        IService fitTrackService;

        // The BLE characteristic for the activity measurement
        ICharacteristic activityCharacteristic;

        // The BLE adapter abstraction
        IAdapter adapter;

        // The IoT Central device client
        DeviceClient deviceClient;

        // The current activity sent by the BLE device
        //
        // The activity values are:
        // 0 - None
        // 1 - Rowing
        // 2 - Running
        int currentActivity;

        public MainPage()
        {
            InitializeComponent();

            // Get the BLE abstraction
            adapter = CrossBluetoothLE.Current.Adapter;
        }

        /// <summary>
        /// Called when a device is detected by the BLE adapter.
        /// This checks to see if the device is the FitTrack BItFit tracker, and if so connects to the device.
        /// Once connected, the FitTrack service and activity characeristic are found.
        /// The current value is read from the charachteristic, and updatesd are subscribed to.
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        async void OnDeviceDetected(object sender, DeviceEventArgs e)
        {
            // Check we are connected to the FitTrack
            if (e.Device.Name == "FitTrack")
            {
                // Store the device
                fitTrack = e.Device;

                try
                {
                    // Connect to the device
                    await adapter.ConnectToDeviceAsync(fitTrack);

                    // Update the UI to show we are connected
                    BleConnectionLabel.Text = "Connected";
                    BleConnectionLabel.TextColor = Color.Green;

                    // Get the service and characteristic
                    fitTrackService = await fitTrack.GetServiceAsync(Guid.Parse("0d5b7c3c-c235-4b57-bde8-ce079704ce9b"));
                    activityCharacteristic = await fitTrackService.GetCharacteristicAsync(Guid.Parse("bc9f3db2-01f0-4c35-a46c-12cc46611ed8"));

                    // Read the current activity. Data is read as bytes, and the activity is a single byte value
                    var bytes = await activityCharacteristic.ReadAsync();
                    currentActivity = bytes[0];

                    // Update the current activity on the UI and in IoT Central
                    await UpdateActivity();

                    // Subscribe to updates on the characteristic
                    activityCharacteristic.ValueUpdated += ValueUpdated;
                    await activityCharacteristic.StartUpdatesAsync();

                }
                catch (DeviceConnectionException ex)
                {
                    // Could not connect to device, so update the UI
                    BleConnectionLabel.Text = "Disconnected";
                    BleConnectionLabel.TextColor = Color.Red;
                }
            }
        }

        /// <summary>
        /// Called when we get a notification from the BLE device that the value for the
        /// characteristic has been updated
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private async void ValueUpdated(object sender, CharacteristicUpdatedEventArgs e)
        {
            // Get the new value
            var bytes = e.Characteristic.Value;

            // Check if it has been updated from the last activity we had
            if (currentActivity != bytes[0])
            {
                // Update the current activity
                currentActivity = bytes[0];
                await UpdateActivity();
            }
        }

        /// <summary>
        /// Updates the current activity on the UI and in IoT Central
        /// </summary>
        /// <returns></returns>
        private async Task UpdateActivity()
        {
            // Conver the activity number to a text value
            string activityText = GetActivityText();

            // Update the UI to show the activity on the main thread
            MainThread.BeginInvokeOnMainThread(() =>
            {
                CurrentActivityLabel.Text = activityText;
            });

            // Send the activity to IoT Central
            await SendCurrentActivityToIoTCentral();
        }

        /// <summary>
        /// Send the current activity to IoT Central
        /// </summary>
        /// <returns></returns>
        private async Task SendCurrentActivityToIoTCentral()
        {
            // Check we are connected
            if (deviceClient is not null)
            {
                // Get the activity value as text
                string activityText = GetActivityText();

                // BUild a telemetry message
                var telemetryPayload = $"{{ \"CurrentActivity\": \"{activityText}\" }}";
                var message = new Message(Encoding.UTF8.GetBytes(telemetryPayload))
                {
                    ContentEncoding = "utf-8",
                    ContentType = "application/json",
                };

                // Send the telemetry message
                await deviceClient.SendEventAsync(message);
            }
        }

        /// <summary>
        /// Converts the current activity to text
        /// The activity values are:
        /// 0 - None
        /// 1 - Rowing
        /// 2 - Running
        /// </summary>
        /// <returns></returns>
        private string GetActivityText()
        {
            string activityText = "None";

            switch (currentActivity)
            {
                case 1:
                    activityText = "Rowing";
                    break;
                case 2:
                    activityText = "Running";
                    break;
            }

            return activityText;
        }

        /// <summary>
        /// Called when the page is appearing.
        /// In here we connect to IoT Central and start listening to IoT devices
        /// </summary>
        protected override async void OnAppearing()
        {
            base.OnAppearing();

            // Run a background thread to connect to IoT Central
            var _ = Task.Run(async () =>
            {
                // Provision the device - use the DPS in IoT Central to get the underlying IoT Hub for this device
                var dpsRegistrationResult = await ProvisionDeviceAsync();

                // Create an authentication method to auth the device with a symmetric key
                var authMethod = new DeviceAuthenticationWithRegistrySymmetricKey(dpsRegistrationResult.DeviceId, key);

                // Create the IoT Central client
                deviceClient = DeviceClient.Create(dpsRegistrationResult.AssignedHub, authMethod, TransportType.Amqp);

                // Update the UI on the main thread to show we are connected to IoT Central
                MainThread.BeginInvokeOnMainThread(() =>
                {
                    IoTConnectionLabel.Text = "Connected";
                    IoTConnectionLabel.TextColor = Color.Green;
                });

                // Update the current activity in IoT Central
                await SendCurrentActivityToIoTCentral();
            });

            // Listen for device discovery from the BLE adpater
            adapter.DeviceDiscovered += OnDeviceDetected;

            // Scan for devices
            await adapter.StartScanningForDevicesAsync();
        }

        /// <summary>
        /// Provision this device as an IoT device in IoT Central
        /// </summary>
        /// <returns></returns>
        private static async Task<DeviceRegistrationResult> ProvisionDeviceAsync()
        {
            // Get a provider for the symmetric key to authenticate the device
            var symmetricKeyProvider = new SecurityProviderSymmetricKey(deviceId, key, null);

            // Set up the transport using AMQP to communicate with IoT Central
            var amqpTransportHandler = new ProvisioningTransportHandlerAmqp();

            // Create a provisioning client to provision the device
            var pdc = ProvisioningDeviceClient.Create("global.azure-devices-provisioning.net", idScope,
                symmetricKeyProvider, amqpTransportHandler);

            // Register this device with the provisioning service
            return await pdc.RegisterAsync();
        }

        /// <summary>
        /// Handle the refresh button clicked event and refresh the BLE device connection
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private async void Button_Clicked(object sender, EventArgs e)
        {
            await adapter.StartScanningForDevicesAsync();
            // If we are already connected, disconnect
            if (fitTrack is not null)
            {
                await adapter.DisconnectDeviceAsync(fitTrack);
            }

            if (activityCharacteristic is not null)
            {
                activityCharacteristic.ValueUpdated -= ValueUpdated;
                await activityCharacteristic.StopUpdatesAsync();
            }

            await adapter.StopScanningForDevicesAsync();
            await adapter.StartScanningForDevicesAsync();
        }
    }
}
