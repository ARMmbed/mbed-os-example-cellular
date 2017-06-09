# Example cellular application for mbed OS

This is a simple example based upon mbed-os celular APIs that demonstrates a simple TCP or UDP echo transaction with a public echo server. 

## Getting started

This particular Cellular application uses a Cellular network and network-socket APIs that are provided as a part of [mbed-os](github.com/armmbed/mbed-os). 

The program uses a [generic celular modem driver](https://github.com/ARMmbed/mbed-os/tree/master/features/netsocket/cellular/generic_modem_driver) utilizing an external IP stack (LWIP) standard 3GPP AT 27.007 AT commands to setup the cellular modem and registers to the network.
 After registration, the driver opens up a PPP (point-to-point protocol) pipe using LWIP with the cellular modem and connects to internet. This driver currently supports UART data connection type only between your cellular modem and MCU. 

For more information on ARM mbed-os cellular APIs and porting guide, please visit [mbed-os Cellular Docs](https://docs.mbed.com/docs/mbed-os-api-reference/en/latest/APIs/communication/cellular/).

### Download the application

```sh
$ mbed import mbed-os-example-cellular
$ cd mbed-os-example-cellular

#OR

$ git clone git@github.com:ARMmbed/mbed-os-example-cellular.git
$ cd mbed-os-example-cellular
```

### Change the network and SIM credentials

See the file `mbed_app.json` in te root directory of your application. This file contains all the user specific configurations needed by your application.
Provide the pin code for your SIM crad as well as any APN settings if needed, e.g., 

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

You can choose which socket type the application should use, e.g.,

```json

     "sock-type": "TCP",

```

### Turning Modem AT echo trace on

If you like details and wish to know about all the AT interactions between the modem and your driver, turn on the modem AT echo trace. 
Set `modem_trace` field value to be true. 

```json
        "modem_trace": {
            "help": "Turns AT command trace on/off from the cellular modem, defaults to off",
            "value": true
        },
```

### Board support

The [generic celular modem driver](https://github.com/ARMmbed/mbed-os/tree/master/features/netsocket/cellular/generic_modem_driver) used by this application is written using only a standard AT command set and the uses PPP with a full fledge mbed-supported  external IP stack. These abilities make the driver essentially generic, i.e., non-vendor specific. However, this particular driver is written for onboard-modem types, i.e., the modem exists on the mbed-enabled target as oppose to plugin-modules (shields). For more details, please check documentation  [mbed-os Cellular Docs](https://docs.mbed.com/docs/mbed-os-api-reference/en/latest/APIs/communication/cellular/).    

Examples of mbed-enabled boards with onboard modem chips can be, [u-blox C027](https://developer.mbed.org/platforms/u-blox-C027/) and [MultiTech MTS Dragonfly](https://developer.mbed.org/platforms/MTS-Dragonfly/).

## Compiling the application

Use mbed-cli commands to generate a binary for the application, e.g., in case of GCC use the following command:

```sh
$ mbed compile -m YOUR_TARGET_WITH_MODEM -t GCC_ARM
```

## Running the application

Drag and drop the application binary from `BUILD/YOUR_TARGET_WITH_MODEM/GCC_ARM/mbed-os-example-cellular.bin` to your mbed-enabled target hardware which appears as a USB device on your host machine.

Attatch a serial console emulator of your choice (e.g., putty, minicom, screen etc) to your USB device, set the baudrate to be 115200 and reset your board by pressing reset button.

You should see an output similar to this:

```
mbed-os-example-cellular, Connecting...
                                                                             
                                                                            
Connection Established.
UDP: Sent 4 Bytes to echo.u-blox.com
Received from echo server 4 Bytes
                                                            
                                                            
Success. Exiting

```   

