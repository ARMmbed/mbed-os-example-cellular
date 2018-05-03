# Example cellular application for Mbed OS

This is an example based on `mbed-os` cellular APIs that demonstrates a TCP or UDP echo transaction with a public echo server.

## Getting started

This particular cellular application uses a cellular network and network-socket APIs that are part of [`mbed-os`](https://github.com/ARMmbed/mbed-os).

The program uses a [generic cellular modem driver](https://github.com/ARMmbed/mbed-os/tree/master/features/netsocket/cellular/generic_modem_driver) using an external IP stack (LWIP) standard 3GPP AT 27.007 AT commands to setup the cellular modem and registers to the network.

After registration, the driver opens a point-to-point protocol (PPP) pipe using LWIP with the cellular modem and connects to internet. This driver currently supports UART data connection type only between your cellular modem and MCU.

For more information on Arm Mbed OS cellular APIs and porting guide, please visit the [Mbed OS cellular API](https://os.mbed.com/docs/latest/reference/cellular-api.html) and [contributing documentation](https://os.mbed.com/docs/latest/reference/contributing-connectivity.html#cellularinterface).

### Download the application

```sh
$ mbed import mbed-os-example-cellular
$ cd mbed-os-example-cellular

#OR

$ git clone git@github.com:ARMmbed/mbed-os-example-cellular.git
$ cd mbed-os-example-cellular
```

### Change the network and SIM credentials

See the file `mbed_app.json` in the root directory of your application. This file contains all the user specific configurations your application needs. Provide the pin code for your SIM card, as well as any APN settings if needed. For example:

```json
        "sim-pin-code": {
            "help": "SIM PIN code",
            "value": "\"1234\""
        },
        "apn": {
            "help": "The APN string to use for this SIM/network, set to 0 if none",
            "value": "\"internet\""
        },
        "username": {
            "help": "The user name string to use for this APN, set to zero if none",
            "value": 0
        },
        "password": {
            "help": "The password string to use for this APN, set to 0 if none",
            "value": 0
        }
```

### Selecting socket type (TCP or UDP)


You can choose which socket type the application should use; however, please note that TCP is a more reliable transmission protocol. For example:


```json

     "sock-type": "TCP",

```

### Turning modem AT echo trace on

If you like details and wish to know about all the AT interactions between the modem and your driver, turn on the modem AT echo trace. Set the `modem_trace` field value to be true.

```json
        "modem_trace": {
            "help": "Turns AT command trace on/off from the cellular modem, defaults to off",
            "value": true
        },
```

### Turning on the tracing and trace level

If you like to add more traces or follow the current ones you can turn traces on by changing `mbed-trace.enable` in mbed_app.json

```"target_overrides": {
        "*": {
            "target.features_add": ["LWIP", "COMMON_PAL"],
            "mbed-trace.enable": false,
```

Changing the trace level is done also in mbed_add.json by changing value in `trace-level`

 ```"trace-level": {
            "help": "Options are TRACE_LEVEL_ERROR,TRACE_LEVEL_WARN,TRACE_LEVEL_INFO,TRACE_LEVEL_DEBUG",
            "macro_name": "MBED_TRACE_MAX_LEVEL",
            "value": "TRACE_LEVEL_INFO"
        }
```

### Board support

The [generic cellular modem driver](https://github.com/ARMmbed/mbed-os/tree/master/features/netsocket/cellular/generic_modem_driver) this application uses was written using only a standard AT command set. It uses PPP with an Mbed-supported external IP stack. These abilities make the driver essentially generic, or nonvendor specific. However, this particular driver is for onboard-modem types. In other words, the modem exists on the Mbed Enabled target as opposed to plug-in modules (shields). For more details, please see our [Mbed OS cellular documentation](https://os.mbed.com/docs/latest/reference/cellular-api.html).

Examples of Mbed Enabled boards with onboard modem chips include [u-blox C027](https://os.mbed.com/platforms/u-blox-C027/) and [MultiTech MTS Dragonfly](https://os.mbed.com/platforms/MTS-Dragonfly/).

## Compiling the application

Use Mbed CLI commands to generate a binary for the application. For example, in the case of GCC, use the following command:

```sh
$ mbed compile -m YOUR_TARGET_WITH_MODEM -t GCC_ARM
```

## Running the application

Drag and drop the application binary from `BUILD/YOUR_TARGET_WITH_MODEM/GCC_ARM/mbed-os-example-cellular.bin` to your Mbed Enabled target hardware, which appears as a USB device on your host machine.

Attach a serial console emulator of your choice (for example, PuTTY, Minicom or screen) to your USB device. Set the baudrate to 115200 bit/s, and reset your board by pressing the reset button.

You should see an output similar to this:

```
mbed-os-example-cellular
Establishing connection ......

Connection Established.
TCP: connected with echo.mbedcloudtesting.com server
TCP: Sent 4 Bytes to echo.mbedcloudtesting.com
Received from echo server 4 Bytes


Success. Exiting
```

## Troubleshooting

If you have problems, you can review the [documentation](https://os.mbed.com/docs/latest/tutorials/debugging.html) for suggestions on what could be wrong and how to fix it.
