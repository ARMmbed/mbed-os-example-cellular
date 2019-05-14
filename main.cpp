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
#include "CellularContext.h"
#include "CellularNetwork.h"
#include "CellularInformation.h"

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
//static const char *if_name;

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
nsapi_error_t test_send_recv(CellularContext *ctx)
{
    nsapi_size_or_error_t retcode;
#if MBED_CONF_APP_SOCK_TYPE == TCP
    TCPSocket sock;
#else
    UDPSocket sock;
#endif

    retcode = sock.open(ctx);
    // FOR MULTIHOMING we need to set sockopt to bind it to correct interface
    char ifn[4];
    tr_debug("Settings socket options to interface %s", ctx->get_interface_name(ifn));
    retcode = sock.setsockopt(NSAPI_SOCKET, NSAPI_BIND_TO_DEVICE, ctx->get_interface_name(ifn), strlen(ctx->get_interface_name(ifn)));

    //tr_debug("Settings socket options to interface %s", if_name);
    //retcode = sock.setsockopt(NSAPI_SOCKET, NSAPI_BIND_TO_DEVICE, if_name, strlen(if_name));

    if (retcode != NSAPI_ERROR_OK) {
#if MBED_CONF_APP_SOCK_TYPE == TCP
        print_function("TCPSocket.open() fails, code: %d\n", retcode);
#else
        print_function("UDPSocket.open() fails, code: %d\n", retcode);
#endif
        return -1;
    }

    SocketAddress sock_addr;
    //retcode = ctx->gethostbyname(host_name, &sock_addr); // or next line could be tested also. Both work but do we need to later for multihoming?
    retcode = ctx->gethostbyname(host_name, &sock_addr, NSAPI_UNSPEC, ctx->get_interface_name(ifn));
    //retcode = ctx->gethostbyname(host_name, &sock_addr, NSAPI_UNSPEC, if_name);

    if (retcode != NSAPI_ERROR_OK) {
        print_function("Couldn't resolve remote host: %s, code: %d\n", host_name, retcode);
        return -1;
    } else {
        tr_debug("sockaddr.ip: %s", sock_addr.get_ip_address());
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

#define BLOCKING_MODE 1
static int async_flag = 0;

void cellular_callback(nsapi_event_t ev, intptr_t ptr)
{
    if (ev >= NSAPI_EVENT_CELLULAR_STATUS_BASE && ev <= NSAPI_EVENT_CELLULAR_STATUS_END) {
        cell_callback_data_t *data = (cell_callback_data_t *)ptr;
        cellular_connection_status_t st = (cellular_connection_status_t)ev;
        tr_error("[MAIN], Cellular event: %d, final try: %d", ev, data->final_try);
        if (st == CellularRILATResponse) {
            tr_error("[MAIN AT RESPONSE: %s, len: %d", (char*)data->data, data->status_data);
        }
    } else {
        tr_error("[MAIN], General event: %d", ev);
    }

    if (ev == NSAPI_EVENT_CONNECTION_STATUS_CHANGE && ptr == NSAPI_STATUS_GLOBAL_UP) {
        async_flag = 1;
        tr_error("[MAIN], cellular_callback, GLOBAL UP, async_flag: %d", async_flag);
    }
    if (ev == NSAPI_EVENT_CONNECTION_STATUS_CHANGE && ptr == NSAPI_STATUS_DISCONNECTED) {
        tr_error("[MAIN], cellular_callback, DISCONNECTED");
    }
}

void print_stuff()
{
    CellularContext *ctx = (CellularContext *)iface;
    CellularDevice *dev = ctx->get_device();
    CellularNetwork* nw = dev->open_network();

    int rssi = -1, ber = -1;
    int retcode = nw->get_signal_quality(rssi, &ber);
    tr_info("[MAIN] get_signal_quality, err: %d, rssi: %d, ber: %d", retcode, rssi, ber);

    char buf[50];
    CellularInformation *info = dev->open_information();
    retcode = info->get_serial_number(buf, 50, CellularInformation::IMEI);
    tr_info("[MAIN] err: %d, IMEI: %s", retcode, buf);

    retcode = info->get_serial_number(buf, 50, CellularInformation::IMEISV);
    tr_info("[MAIN] err: %d, IMEISV: %s", retcode, buf);

    retcode = info->get_imsi(buf, 50);
    tr_info("[MAIN] err: %d, imsi: %s", retcode, buf);

    CellularNetwork::NWRegisteringMode mode = CellularNetwork::NWModeDeRegister;
    retcode = nw->get_network_registering_mode(mode);
    tr_info("[MAIN] err: %d, nwmode: %d", retcode, mode);

    retcode = nw->set_receive_period(1, CellularNetwork::EDRXEUTRAN_NB_S1_mode, 0x06);
    tr_info("[MAIN] set_receive_period: %d", retcode);

    //retcode = dev->set_power_save_mode(40, 4);
    //tr_info("[MAIN] set_power_save_mode: %d", retcode);

    retcode = info->get_iccid(buf, 50);
    tr_info("[MAIN] get_iccid: %d, iccid: %s", retcode, buf);

    //retcode = dev->send_at_command("AT\r\n", 4);
}

#include "SAMSUNG_S5JS100_RIL.h"

nsapi_error_t test_data(CellularDevice *device, CellularContext *ctx, const char *plmn)
{
    device->set_plmn(plmn);
    return ctx->connect();
}

int main()
{
    print_function("\n\nmbed-os-example-cellular\n");
    print_function("Establishing connection\n");
#if MBED_CONF_MBED_TRACE_ENABLE
    trace_open();
#else
    dot_thread.start(dot_event);
#endif // #if MBED_CONF_MBED_TRACE_ENABLE


    SAMSUNG_S5JS100_RIL *s1 = new SAMSUNG_S5JS100_RIL();
    CellularContext *ctx1 = s1->create_context(NULL, "iot", false);
    CellularContext *ctx2 = s1->create_context(NULL, "iot2", false);

    nsapi_error_t err = test_data(s1, ctx1,  NULL);
    if (err) {
        tr_error("Connect1 failed with: %d", err);
        err = ctx1->disconnect();
        tr_debug("disconnect 1: %d", err);
    }
    if (err == 0) {
        // !!! CHANGE PLMN TO DIFFERENT IF NEEDED
        err = test_data(s1, ctx2, NULL);
        if (err) {
            tr_error("Connect2 failed with: %d", err);
        }
    }

    if (err == 0) {
        err = test_send_recv(ctx1);
        tr_debug("test and send 1: %d", err);
        err = test_send_recv(ctx2);
        tr_debug("test and send 2: %d", err);
    }

    err = ctx1->disconnect();
    tr_debug("disconnect 1: %d", err);
    err = ctx2->disconnect();
    tr_debug("disconnect 2: %d" ,err);
    delete s1;
    wait(2);
    tr_info("MAIN OUT");
#if MBED_CONF_MBED_TRACE_ENABLE
    trace_close();
#endif // #if MBED_CONF_MBED_TRACE_ENABLE
    return 0;

}
// EOF
