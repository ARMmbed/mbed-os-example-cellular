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
#include "CellularNonIPSocket.h"
#include "CellularDevice.h"
#include "UDPSocket.h"
#include "CellularLog.h"

#include "rtos/EventFlags.h"
#include "events/EventQueue.h"
events::EventQueue &eq = *mbed_event_queue();

#define UDP 0
#define TCP 1
#define NONIP 2

#define LOCAL_UDP_PORT 3030

// Number of retries /
#define RETRY_COUNT 3

NetworkInterface *iface;

// Echo server hostname
const char *host_name = MBED_CONF_APP_ECHO_SERVER_HOSTNAME;

// Echo server port (same for TCP and UDP)
const int port = MBED_CONF_APP_ECHO_SERVER_PORT;

static rtos::Mutex trace_mutex;

nsapi_error_t connect_retcode = NSAPI_ERROR_OK;
uint8_t connect_retry_counter = 0;

int state = 0;
nsapi_error_t retcode = NSAPI_ERROR_NO_CONNECTION;
CellularDevice *device = NULL;
CellularContext *ctx = NULL;
bool recv_done = false;


nsapi_size_or_error_t send_recv_retcode;
#if MBED_CONF_APP_SOCK_TYPE == TCP
TCPSocket sock;
#elif MBED_CONF_APP_SOCK_TYPE == UDP
UDPSocket sock;
#elif MBED_CONF_APP_SOCK_TYPE == NONIP
CellularNonIPSocket sock;
#endif

static void main_machine();

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

//Thread dot_thread(osPriorityNormal, 512);

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
        //ThisThread::sleep_for(4000);
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

static void network_callback(nsapi_event_t ev, intptr_t ptr)
{
    if (ev >= NSAPI_EVENT_CELLULAR_STATUS_BASE && ev <= NSAPI_EVENT_CELLULAR_STATUS_END) {
        tr_info("[Main]: network_callback called with event: %d, err: %d, data: %d", ev, ((cell_callback_data_t*)ptr)->error, ((cell_callback_data_t*)ptr)->status_data);
    } else {
        tr_info("[Main]: network_callback called with event: %d, ptr: %d", ev, ptr);
    }

    if (ev == NSAPI_EVENT_CONNECTION_STATUS_CHANGE) {
        if (ptr == NSAPI_STATUS_GLOBAL_UP) {
            tr_info("Network callback: Connected to a network");

            if (!eq.call(main_machine)) {
                tr_error("Failed to start cellular sending!");
                device->stop();
                eq.break_dispatch();
            }
        }
    }
}


static void send();

static void receive();

void socket_event()
{
    tr_info("socket_event");
    if (!eq.call(receive)) {
        tr_error("Queue failed event!");
        eq.break_dispatch();
    }
}

void test_send_recv()
{
    send_recv_retcode = sock.open(iface);

    if (send_recv_retcode != NSAPI_ERROR_OK) {
#if MBED_CONF_APP_SOCK_TYPE == TCP
        print_function("TCPSocket.open() fails, code: %d\n", send_recv_retcode);
#elif MBED_CONF_APP_SOCK_TYPE == UDP
        print_function("UDPSocket.open() fails, code: %d\n", send_recv_retcode);
#endif
        eq.call(main_machine);
        return;
    }

    eq.call_in(10, callback(send));
}

void send()
{
    const char *echo_string = "TEST";

    //TODO: ASYNCHRONOUS HANDLING FOR DNS QUERY
    //SocketAddress sock_addr;

    /*send_recv_retcode = iface->gethostbyname(host_name, &sock_addr);
    if (send_recv_retcode != NSAPI_ERROR_OK) {
        print_function("Couldn't resolve remote host: %s, code: %d\n", host_name, send_recv_retcode);
        eq.call(main_machine);
        return;
    }*/

    SocketAddress sock_addr("52.215.34.155\0", port);

    sock.set_blocking(false);
    sock.sigio(socket_event);
//    sock.bind(LOCAL_UDP_PORT);

    send_recv_retcode = sock.sendto(sock_addr, (void*) echo_string, strlen(echo_string));
    if (send_recv_retcode < 0) {
        print_function("UDPSocket.sendto() fails, code: %d\n", send_recv_retcode);
        eq.call(main_machine);
        return;
    } else {
        print_function("UDP: Sent %d Bytes to %s\n", send_recv_retcode, host_name);
    }
}

void receive()
{
    if (!recv_done) {
        char recv_buf[4];
        SocketAddress sock_addr;

        tr_info("receive");

        int n = sock.recvfrom(&sock_addr, (void*) recv_buf, sizeof(recv_buf));

        sock.close();

        if (n > 0) {
            print_function("Received from echo server %d Bytes\n", n);
            send_recv_retcode = 0;
            recv_done = true;
        } else if (n == NSAPI_ERROR_WOULD_BLOCK ) {
            tr_info("ASYNC would block socket, wait for next event...");
        } else {
            send_recv_retcode = -1;
        }

        eq.call(main_machine);
    }
}


#include "CellularDevice.h"
#include "CellularInformation.h"
void use_info(NetworkInterface &iface)
{
    CellularContext &ctx = (CellularContext&)iface;
    CellularDevice *dev = ctx.get_device();

    CellularInformation *info = dev->open_information();

    char buf[50];
    size_t buf_size = 50;
    nsapi_error_t err = info->get_manufacturer(buf, buf_size);
    tr_info("get_manufacturer, err: %d, buf: %s, buf_size: %d", err, buf, buf_size);

    err = info->get_model(buf, buf_size);
    tr_info("get_model, err: %d, buf: %s, buf_size: %d", err, buf, buf_size);

    err = info->get_revision(buf, buf_size);
    tr_info("get_revision, err: %d, buf: %s, buf_size: %d", err, buf, buf_size);

    err = info->get_serial_number(buf, buf_size, CellularInformation::IMEI);
    tr_info("get_serial_number(IMEI), err: %d, buf: %s, buf_size: %d", err, buf, buf_size);

    err = info->get_imsi(buf, buf_size);
    tr_info("get_imsi, err: %d, buf: %s, buf_size: %d", err, buf, buf_size);

    err = info->get_iccid(buf, buf_size);
    tr_info("get_iccid, err: %d, buf: %s, buf_size: %d", err, buf, buf_size);
}

static void main_machine()
{
    if (state == 0) { //init
        recv_done = false;
        print_function("\n\nmbed-os-example-cellular\n");
        print_function("\n\nBuilt: %s, %s\n", __DATE__, __TIME__);
#ifdef MBED_CONF_NSAPI_DEFAULT_CELLULAR_PLMN
        print_function("\n\n[MAIN], plmn: %s\n", (MBED_CONF_NSAPI_DEFAULT_CELLULAR_PLMN ? MBED_CONF_NSAPI_DEFAULT_CELLULAR_PLMN : "NULL"));
#endif

        print_function("Establishing connection\n");
#if MBED_CONF_MBED_TRACE_ENABLE
        trace_open();
#else
        //dot_thread.start(dot_event);
#endif // #if MBED_CONF_MBED_TRACE_ENABLE

        device = CellularDevice::get_default_instance();
        ctx = device->create_context();
        iface = ctx;

        ctx->attach(callback(&network_callback));
        ctx->set_blocking(false);

        print_function("\nAssert iFace\n");
        MBED_ASSERT(iface);

        print_function("\niFace->set_default_parameters()\n");
        // sim pin, apn, credentials and possible plmn are taken automatically from json when using NetworkInterface::set_default_parameters()
        iface->set_default_parameters();
        state++;
        eq.call(main_machine);
    } else if (state == 1) { //Connecting
        print_function("\ndo_connect()\n");
        /* Attempt to connect to a cellular network */
        state++;
        connect_retcode = iface->connect();
    } else if (state == 2) {
        if (connect_retcode != NSAPI_ERROR_OK) {
            state++; //skip send test
        }
        state++;
        eq.call(main_machine);
    } else if (state == 3) {
        print_function("\ntest_send_recv()\n");
        state++;
        eq.call(test_send_recv);
    } else if (state == 4) {
        state++;
        eq.call(main_machine);
    } else if (state == 5) {
        print_function("\ndisconnect()\n");
        if (iface->disconnect() != NSAPI_ERROR_OK) {
            print_function("\n\n disconnect failed.\n\n");
        }
        state++;
        eq.call(main_machine);
    } else if (state == 6) {
        print_function("\nuse_info()\n");
        use_info(*iface);
        state++;
        eq.call(main_machine);
    } else {
        if (send_recv_retcode == NSAPI_ERROR_OK) {
            print_function("\n\nSuccess. Exiting \n\n");
        } else {
            print_function("\n\nFailure. Exiting \n\n");
        }

#if MBED_CONF_MBED_TRACE_ENABLE
        trace_close();
#else
        //dot_thread.terminate();
#endif // #if MBED_CONF_MBED_TRACE_ENABLE

        eq.break_dispatch(); //Stop loop
    }
}

int main()
{
    eq.call(main_machine);
    eq.dispatch_forever();
    printf("all done, exiting\n");

    return 0;
}
// EOF
