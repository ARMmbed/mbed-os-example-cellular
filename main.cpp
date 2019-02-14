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
#include "CellularLog.h"

#define UDP 0
#define TCP 1

// Number of retries /
#define RETRY_COUNT 3

NetworkInterface *iface;

// Echo server hostname
const char *host_name = MBED_CONF_APP_ECHO_SERVER_HOSTNAME;

// Echo server port (same for TCP and UDP)
const int port = MBED_CONF_APP_ECHO_SERVER_PORT;

static rtos::Mutex trace_mutex;

#if MBED_CONF_MBED_TRACE_ENABLE
static void trace_wait()
{
    trace_mutex.lock();
}

static void trace_release()
{
    trace_mutex.unlock();
}

static char time_st[50];

static char* trace_time(size_t ss)
{
    snprintf(time_st, 49, "[%08llums]", Kernel::get_ms_count());
    return time_st;
}

static void trace_open()
{
    mbed_trace_init();
    mbed_trace_prefix_function_set( &trace_time );

    mbed_trace_mutex_wait_function_set(trace_wait);
    mbed_trace_mutex_release_function_set(trace_release);

    mbed_cellular_trace::mutex_wait_function_set(trace_wait);
    mbed_cellular_trace::mutex_release_function_set(trace_release);
}

static void trace_close()
{
    mbed_cellular_trace::mutex_wait_function_set(NULL);
    mbed_cellular_trace::mutex_release_function_set(NULL);

    mbed_trace_free();
}
#endif // #if MBED_CONF_MBED_TRACE_ENABLE

Thread dot_thread(osPriorityNormal, 512);

void print_function(const char *format, ...)
{
    trace_mutex.lock();
    va_list arglist;
    va_start( arglist, format );
    vprintf(format, arglist);
    va_end( arglist );
    trace_mutex.unlock();
}

void dot_event()
{
    while (true) {
        ThisThread::sleep_for(4000);
        if (iface && iface->get_connection_status() == NSAPI_STATUS_GLOBAL_UP) {
            break;
        } else {
            trace_mutex.lock();
            printf(".");
            fflush(stdout);
            trace_mutex.unlock();
        }
    }
}

/**
 * Connects to the Cellular Network
 */
nsapi_error_t do_connect()
{
    nsapi_error_t retcode = NSAPI_ERROR_OK;
    uint8_t retry_counter = 0;

    while (iface->get_connection_status() != NSAPI_STATUS_GLOBAL_UP) {
        retcode = iface->connect();
        if (retcode == NSAPI_ERROR_AUTH_FAILURE) {
            print_function("\n\nAuthentication Failure. Exiting application\n");
        } else if (retcode == NSAPI_ERROR_OK) {
            print_function("\n\nConnection Established.\n");
        } else if (retry_counter > RETRY_COUNT) {
            print_function("\n\nFatal connection failure: %d\n", retcode);
        } else {
            print_function("\n\nCouldn't connect: %d, will retry\n", retcode);
            retry_counter++;
            continue;
        }
        break;
    }
    return retcode;
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

    retcode = sock.open(iface);
    if (retcode != NSAPI_ERROR_OK) {
#if MBED_CONF_APP_SOCK_TYPE == TCP
        print_function("TCPSocket.open() fails, code: %d\n", retcode);
#else
        print_function("UDPSocket.open() fails, code: %d\n", retcode);
#endif
        return -1;
    }

    SocketAddress sock_addr;
    retcode = iface->gethostbyname(host_name, &sock_addr);
    if (retcode != NSAPI_ERROR_OK) {
        print_function("Couldn't resolve remote host: %s, code: %d\n", host_name, retcode);
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
        print_function("TCPSocket.connect() fails, code: %d\n", retcode);
        return -1;
    } else {
        print_function("TCP: connected with %s server\n", host_name);
    }
    retcode = sock.send((void*) echo_string, sizeof(echo_string));
    if (retcode < 0) {
        print_function("TCPSocket.send() fails, code: %d\n", retcode);
        return -1;
    } else {
        print_function("TCP: Sent %d Bytes to %s\n", retcode, host_name);
    }

    n = sock.recv((void*) recv_buf, sizeof(recv_buf));
#else

    retcode = sock.sendto(sock_addr, (void*) echo_string, sizeof(echo_string));
    if (retcode < 0) {
        print_function("UDPSocket.sendto() fails, code: %d\n", retcode);
        return -1;
    } else {
        print_function("UDP: Sent %d Bytes to %s\n", retcode, host_name);
    }

    n = sock.recvfrom(&sock_addr, (void*) recv_buf, sizeof(recv_buf));
#endif

    sock.close();

    if (n > 0) {
        print_function("Received from echo server %d Bytes\n", n);
        return 0;
    }

    return -1;
}

int main()
{
    print_function("\n\nmbed-os-example-cellular\n");
    print_function("\n\nBuilt: %s, %s\n", __DATE__, __TIME__);
#ifdef MBED_CONF_NSAPI_DEFAULT_CELLULAR_PLMN
    print_function("\n\n[MAIN], plmn: %s\n", MBED_CONF_NSAPI_DEFAULT_CELLULAR_PLMN);
#endif

    print_function("Establishing connection\n");
#if MBED_CONF_MBED_TRACE_ENABLE
    trace_open();
#else
    dot_thread.start(dot_event);
#endif // #if MBED_CONF_MBED_TRACE_ENABLE

    // sim pin, apn, credentials and possible plmn are taken atuomtically from json when using get_default_instance()
    iface = NetworkInterface::get_default_instance();
    MBED_ASSERT(iface);

    nsapi_error_t retcode = NSAPI_ERROR_NO_CONNECTION;

    /* Attempt to connect to a cellular network */
    if (do_connect() == NSAPI_ERROR_OK) {
        retcode = test_send_recv();
    }

    if (iface->disconnect() != NSAPI_ERROR_OK) {
        print_function("\n\n disconnect failed.\n\n");
    }

    if (retcode == NSAPI_ERROR_OK) {
        print_function("\n\nSuccess. Exiting \n\n");
    } else {
        print_function("\n\nFailure. Exiting \n\n");
    }

#if MBED_CONF_MBED_TRACE_ENABLE
    trace_close();
#else
    dot_thread.terminate();
#endif // #if MBED_CONF_MBED_TRACE_ENABLE

    return 0;
}
// EOF
