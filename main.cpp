/*
 * Copyright (c) 2017 ARM Limited. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mbed.h"
#include "common_functions.h"
#include "UDPSocket.h"
#include "OnboardCellularInterface.h"

#define UDP 0
#define TCP 1

// SIM pin code goes here
#ifndef MBED_CONF_APP_SIM_PIN_CODE
# define MBED_CONF_APP_SIM_PIN_CODE    "1234"
#endif

#ifndef MBED_CONF_APP_APN
# define MBED_CONF_APP_APN         "internet"
#endif
#ifndef MBED_CONF_APP_USERNAME
# define MBED_CONF_APP_USERNAME    NULL
#endif
#ifndef MBED_CONF_APP_PASSWORD
# define MBED_CONF_APP_PASSWORD    NULL
#endif

// Number of retries /
#define RETRY_COUNT 3



// CellularInterface object
OnboardCellularInterface iface;

// Echo server hostname
const char *host_name = "echo.mbedcloudtesting.com";

// Echo server port (same for TCP and UDP)
const int port = 7;

Mutex PrintMutex;
Thread dot_thread;

#define PRINT_TEXT_LENGTH 128
char print_text[PRINT_TEXT_LENGTH];
void print_function(const char *input_string)
{
    PrintMutex.lock();
    printf("%s", input_string);
    fflush(NULL);
    PrintMutex.unlock();
}

void dot_event()
{

    while (true) {
        wait(4);
        if (!iface.is_connected()) {
            print_function(".");
        } else {
            break;
        }
    }

}


/**
 * Connects to the Cellular Network
 */
nsapi_error_t do_connect()
{
    nsapi_error_t retcode;
    uint8_t retry_counter = 0;

    while (!iface.is_connected()) {

        retcode = iface.connect();
        if (retcode == NSAPI_ERROR_AUTH_FAILURE) {
            print_function("\n\nAuthentication Failure. Exiting application\n");
            return retcode;
        } else if (retcode != NSAPI_ERROR_OK) {
            snprintf(print_text, PRINT_TEXT_LENGTH, "\n\nCouldn't connect: %d, will retry\n", retcode);
            print_function(print_text);
            retry_counter++;
            continue;
        } else if (retcode != NSAPI_ERROR_OK && retry_counter > RETRY_COUNT) {
            snprintf(print_text, PRINT_TEXT_LENGTH, "\n\nFatal connection failure: %d\n", retcode);
            print_function(print_text);
            return retcode;
        }

        break;
    }

    print_function("\n\nConnection Established.\n");

    return NSAPI_ERROR_OK;
}

/**
 * Opens a UDP or a TCP socket with the given echo server and performs an echo
 * transaction retrieving current.
 */
nsapi_error_t test_send_recv()
{
    nsapi_size_or_error_t retcode;
#if MBED_CONF_APP_SOCK_TYPE == TCP
    TCPSocket sock;
#else
    UDPSocket sock;
#endif

    retcode = sock.open(&iface);
    if (retcode != NSAPI_ERROR_OK) {
        snprintf(print_text, PRINT_TEXT_LENGTH, "UDPSocket.open() fails, code: %d\n", retcode);
        print_function(print_text);
        return -1;
    }

    SocketAddress sock_addr;
    retcode = iface.gethostbyname(host_name, &sock_addr);
    if (retcode != NSAPI_ERROR_OK) {
        snprintf(print_text, PRINT_TEXT_LENGTH, "Couldn't resolve remote host: %s, code: %d\n", host_name,
               retcode);
        print_function(print_text);
        return -1;
    }

    sock_addr.set_port(port);

    sock.set_timeout(15000);
    int n = 0;
    const char *echo_string = "TEST";
    char recv_buf[4];
#if MBED_CONF_APP_SOCK_TYPE == TCP
    retcode = sock.connect(sock_addr);
    if (retcode < 0) {
        snprintf(print_text, PRINT_TEXT_LENGTH, "TCPSocket.connect() fails, code: %d\n", retcode);
        print_function(print_text);
        return -1;
    } else {
        snprintf(print_text, PRINT_TEXT_LENGTH, "TCP: connected with %s server\n", host_name);
        print_function(print_text);
    }
    retcode = sock.send((void*) echo_string, sizeof(echo_string));
    if (retcode < 0) {
        snprintf(print_text, PRINT_TEXT_LENGTH, "TCPSocket.send() fails, code: %d\n", retcode);
        print_function(print_text);
        return -1;
    } else {
        snprintf(print_text, PRINT_TEXT_LENGTH, "TCP: Sent %d Bytes to %s\n", retcode, host_name);
        print_function(print_text);
    }

    n = sock.recv((void*) recv_buf, sizeof(recv_buf));
#else

    retcode = sock.sendto(sock_addr, (void*) echo_string, sizeof(echo_string));
    if (retcode < 0) {
        snprintf(print_text, PRINT_TEXT_LENGTH, "UDPSocket.sendto() fails, code: %d\n", retcode);
        print_function(print_text);
        return -1;
    } else {
        snprintf(print_text, PRINT_TEXT_LENGTH, "UDP: Sent %d Bytes to %s\n", retcode, host_name);
        print_function(print_text);
    }

    n = sock.recvfrom(&sock_addr, (void*) recv_buf, sizeof(recv_buf));
#endif

    sock.close();

    if (n > 0) {
        snprintf(print_text, PRINT_TEXT_LENGTH, "Received from echo server %d Bytes\n", n);
        print_function(print_text);
        return 0;
    }

    return -1;
}

int main()
{

    iface.modem_debug_on(MBED_CONF_APP_MODEM_TRACE);
    /* Set Pin code for SIM card */
    iface.set_sim_pin(MBED_CONF_APP_SIM_PIN_CODE);

    /* Set network credentials here, e.g., APN*/
    iface.set_credentials(MBED_CONF_APP_APN, MBED_CONF_APP_USERNAME, MBED_CONF_APP_PASSWORD);

    print_function("\n\nmbed-os-example-cellular\n");
    print_function("Establishing connection ");
    dot_thread.start(dot_event);

    /* Attempt to connect to a cellular network */
    if (do_connect() == NSAPI_ERROR_OK) {
        nsapi_error_t retcode = test_send_recv();
        if (retcode == NSAPI_ERROR_OK) {
            print_function("\n\nSuccess. Exiting \n\n");
            return 0;
        }
    }

    print_function("\n\nFailure. Exiting \n\n");
    return -1;
}
// EOF
