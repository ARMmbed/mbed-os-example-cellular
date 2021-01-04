![](./resources/official_armmbed_example_badge.png)
# Cellular Example

This is an example based on `mbed-os` cellular APIs that demonstrates a TCP or UDP echo transaction with a public echo server.

(Note: To see this example in a rendered form you can import into the Arm Mbed Online Compiler, please see
[the documentation](https://os.mbed.com/docs/mbed-os/latest/apis/cellular-api.html#cellular-example-connection-establishment).)

## Getting started

This particular cellular application uses a cellular network and network-socket APIs that are part of [`mbed-os`](https://github.com/ARMmbed/mbed-os).

The program uses a [cellular modem driver](https://github.com/ARMmbed/mbed-os/tree/master/connectivity/cellular/include/cellular/framework/API)
using an external IP stack standard 3GPP AT 27.007 AT commands to setup the cellular modem and registers to the network.

After registration, the driver opens a point-to-point protocol (PPP) pipe with the cellular modem and connects
to internet. This driver currently supports UART data connection type only between your cellular modem and MCU.

For more information on Arm Mbed OS cellular APIs and porting guide, please visit the
[Mbed OS cellular API](https://os.mbed.com/docs/mbed-os/latest/apis/cellular-networking.html) and
[Mbed OS cellular porting guide](https://os.mbed.com/docs/mbed-os/latest/porting/cellular-device-porting.html).

### Board support

Currently supported boards with onboard modem chips can be found under Mbed OS
[/targets folder](https://github.com/ARMmbed/mbed-os/tree/master/targets).
You can find all cellular specific onboard modems by searching an overridden function
`CellularDevice::get_target_default_instance()`.

Currently supported modem drivers can be found under cellular
[/drivers folder](https://github.com/ARMmbed/mbed-os/tree/master/connectivity/drivers/cellular).

For a cellular shield, you need to define which shield to use with `provide-default`, and also how the shield is connected
to the Mbed OS board. For example, a generic AT/PPP modem would add from the `GENERIC_AT3GPP/mbed_lib.json` file to your
`mbed_app.json`:

```
    "target_overrides": {
       "GENERIC_AT3GPP.provide-default": true,
       "GENERIC_AT3GPP.tx": "<tx-pinmap>",
       "GENERIC_AT3GPP.rx": "<rx-pinmap>"
    }
```

## Building and flashing the example

### To build the example

Clone the repository containing example:

```
git clone https://github.com/ARMmbed/mbed-os-example-cellular.git
```

**Tip:** If you don't have git installed, you can
[download a zip file](https://github.com/ARMmbed/mbed-os-example-cellular/archive/master.zip) of the repository.

Update the source tree:

```
cd mbed-os-example-cellular
mbed deploy
```

Run the build:

```mbed compile -t <ARM | GCC_ARM> -m <YOUR_TARGET>```

### To flash the example onto your board

Connect your mbed board to your computer over USB. It appears as removable storage.

When you run the `mbed compile` command above, mbed cli creates a .bin or a .hex file (depending on your target) in
```BUILD/<target-name>/<toolchain>``` under the example's directory. Drag and drop the file to the removable storage.

Alternatively you may launch compilation with `-f` flag to have mbed tools attempt to flash your board.
The tools will flash the binary to all targets that match the board specified by '-m' parameter.

### Change the network and SIM credentials

See the file `mbed_app.json` in the root directory of your application. This file contains all the user specific
configurations your application needs. Provide the pin code for your SIM card, as well as any other cellular settings,
or `null` if not used. For example:

```json
    "target_overrides": {
        "*": {
            "nsapi.default-cellular-sim-pin": "\"1234\"",
```

### Selecting socket type (TCP, UDP or NONIP)

You can choose which socket type the application should use; however, please note that TCP is a more reliable
transmission protocol. For example:

```json

     "sock-type": "TCP",

```

### Turning modem AT echo trace on

If you like details and wish to know about all the AT interactions between the modem and your driver, turn on the modem
AT echo trace:

```json
        "cellular.debug-at": true
```

### Turning on the tracing and trace level

If you like to add more traces or follow the current ones you can turn traces on by changing `mbed-trace.enable` in
mbed_app.json:

```"target_overrides": {
        "*": {
            "mbed-trace.enable": true,
```

After you have defined `mbed-trace.enable: true`, you can set trace levels by changing value in `trace-level`:

 ```"trace-level": {
            "help": "Options are TRACE_LEVEL_ERROR,TRACE_LEVEL_WARN,TRACE_LEVEL_INFO,TRACE_LEVEL_DEBUG",
            "macro_name": "MBED_TRACE_MAX_LEVEL",
            "value": "TRACE_LEVEL_INFO"
        }
```

## Running the example

When example application is running information about activity is printed over the serial connection.

**Note:** The default serial baudrate has been set to 9600.

Please have a client open and connected to the board. You may use:

- [Tera Term](https://ttssh2.osdn.jp/index.html.en) for windows

- screen or minicom for Linux (example usage: `screen /dev/serial/<your board> 9600`)

- mbed tools has a terminal command `mbed term -b 9600`

### Expected output

You should see an output similar to this:

```
mbed-os-example-cellular
Establishing connection
Connection Established.
TCP: connected with echo.mbedcloudtesting.com server
TCP: Sent 4 Bytes to echo.mbedcloudtesting.com
Received from echo server 4 Bytes
Success. Exiting
```

### Troubleshooting

* Make sure the fields `nsapi.default-cellular-sim-pin`, `nsapi.default-cellular-plmn`, `nsapi.default-cellular-apn`,
  `nsapi.default-cellular-username` and `nsapi.default-cellular-password` from the `mbed_app.json` file are filled in
  correctly. The correct values should appear in the user manual of the board if using eSIM or in the details of the
  SIM card if using normal SIM.
* Enable trace flag to have access to debug information `"mbed-trace.enable": true` and `"cellular.debug-at": true`.
* Error Message: Assertion failed: iface usually means that a default modem is not defined, e.g.
  `"GENERIC_AT3GPP.provide-default": true`
* If the modem does not respond to (AT) queries, check that UART pins (tx, rx, rts, cts) are connected and defined,
  e.g. `"GENERIC_AT3GPP.tx": "<tx-pinmap>"`, ...
* It is a common case that a modem seems to connect fine with just USB power, but actually it needs to have an external
  power supply for a data connection.
* Try both `TCP` and `UDP` socket types.
* Try both `"lwip.ppp-enabled": true` and `"lwip.ppp-enabled": false`.
* The modem may support only a fixed baud-rate, such as `"platform.default-serial-baud-rate": 9600`.
* The modem and network may only support IPv6 in which case `"lwip.ipv6-enabled": true` shall be defined.
* The SIM and modem must have compatible cellular technology (3G, 4G, NB-IoT, ...) supported and cellular network available.
* Enable CIoT optimization for NONIP socket `control-plane-opt: true`.

If you have problems to get started with debugging, you can review the
[documentation](https://os.mbed.com/docs/latest/tutorials/debugging.html) for suggestions on what could be wrong and how to fix it.

## License and contributions

The software is provided under Apache-2.0 license. Contributions to this project are accepted under the same license.
Please see [contributing.md](CONTRIBUTING.md) for more info.

This project contains code from other projects. The original license text is included in those source files.
They must comply with our license guide
