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
        print_function("Received from echo server %d Bytes: %s\n", n, recv_buf);
        return 0;
    }

    return -1;
}

#define BLOCKING_MODE 1
static int async_flag = 0;
enum CurrentOp {
	OP_INVALID,
    OP_DEVICE_READY,
    OP_SIM_READY,
    OP_REGISTER,
    OP_ATTACH
};
static CurrentOp op;
#include "Semaphore.h"
rtos::Semaphore semaphore;

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

    //retcode = nw->set_receive_period(1, CellularNetwork::EDRXEUTRAN_NB_S1_mode, 0x06);
    //tr_info("[MAIN] set_receive_period: %d", retcode);

    //retcode = dev->set_power_save_mode(40, 4);
    //tr_info("[MAIN] set_power_save_mode: %d", retcode);

    retcode = info->get_iccid(buf, 50);
    tr_info("[MAIN] get_iccid: %d, iccid: %s", retcode, buf);

    //retcode = dev->send_at_command("AT\r\n", 4);
}

void cellular_callback(nsapi_event_t ev, intptr_t ptr)
{
	// support for testing in non-blocking mode and step by step
	if (ev >= NSAPI_EVENT_CELLULAR_STATUS_BASE && ev <= NSAPI_EVENT_CELLULAR_STATUS_END) {
		cell_callback_data_t *ptr_data = (cell_callback_data_t *)ptr;
		cellular_connection_status_t cell_ev = (cellular_connection_status_t)ev;
		if (cell_ev == CellularDeviceReady && ptr_data->error == NSAPI_ERROR_OK && op == OP_DEVICE_READY) {
			semaphore.release();
		}
		if (cell_ev == CellularSIMStatusChanged && ptr_data->error == NSAPI_ERROR_OK &&
				   ptr_data->status_data == CellularDevice::SimStateReady && op == OP_SIM_READY) {
			semaphore.release();
		}
		if (cell_ev == CellularRegistrationStatusChanged &&
				(ptr_data->status_data == CellularNetwork::RegisteredHomeNetwork ||
				ptr_data->status_data == CellularNetwork::RegisteredRoaming ||
				ptr_data->status_data == CellularNetwork::AlreadyRegistered) &&
				ptr_data->error == NSAPI_ERROR_OK &&
				op == OP_REGISTER) {
			semaphore.release();
		}
		if(cell_ev == CellularAttachNetwork  && ptr_data->status_data == CellularNetwork::Attached &&
				ptr_data->error == NSAPI_ERROR_OK && op == OP_ATTACH) {
			semaphore.release();
		}
	}


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

void run_example(NetworkInterface* interface)
{
    iface = interface;
    CellularContext *ctx = (CellularContext *)iface;
    //ctx->set_pdp_type("IPV6");

    // sim pin, apn, credentials and possible plmn are taken automatically from json when using get_default_instance()
	MBED_ASSERT(iface);
	iface->set_blocking(BLOCKING_MODE);
	iface->attach(cellular_callback);
	nsapi_error_t retcode = NSAPI_ERROR_NO_CONNECTION;
	CellularDevice *dev = ctx->get_device();
	if (BLOCKING_MODE == 1) {
		for (int i = 0; i < 1; i++) {
			tr_info("[MAIN] START connect number: %d !", i);
			retcode = do_connect();
			if (retcode == NSAPI_ERROR_OK) {
				retcode = test_send_recv();
				if (retcode) {
				    tr_error("[MAIN] send/recv number: %d failed with %d!", i, retcode);
				    retcode = iface->disconnect();
				    if (retcode != NSAPI_ERROR_OK) {
				        tr_error("[MAIN] DisConnect number: %d failed with %d!", i, retcode);
				    }
					break;
				}
			} else {
				tr_error("[MAIN] Connect number: %d failed with %d!", i, retcode);
				break;
			}
			retcode = iface->disconnect();
			if (retcode != NSAPI_ERROR_OK) {
				tr_error("[MAIN] DisConnect number: %d failed with %d!", i, retcode);
				break;
			}
			tr_info("[MAIN] ITERATION %d DONE, wait 1 sec.", i);
			wait(1);
		}
	} else {
		for (int i = 0; i < 1; i++) {
			tr_info("[MAIN] START connect number: %d !", i);
			retcode = iface->connect();
			tr_info("[MAIN] connect: %d number: %d !", retcode, i);
			if (!retcode) {
				while (1) {
					//tr_error("[MAIN] waiting for connect...: async_flag: %d", async_flag);
					if (async_flag) {
						tr_info("[MAIN] connected");
						break;
					}
					wait(1);
				}
				async_flag = 0;
				retcode = test_send_recv();
				tr_info("[MAIN] test_send_recv: %d number: %d !", retcode, i);
			}
			retcode = iface->disconnect();
			tr_info("[MAIN] DisConnect: %d number: %d !", retcode, i);
			wait(1);
		}
	}

	print_stuff();
	wait(5);
	retcode = dev->hard_power_off();
	tr_info("[MAIN] hard_power_off: %d", retcode);
	if (retcode == NSAPI_ERROR_OK) {
		print_function("\n\nSuccess. Exiting \n\n");
	} else {
		print_function("\n\nFailure. Exiting \n\n");
	}
}

void run_step_example(NetworkInterface* interface)
{
	iface = interface;
	iface->set_blocking(BLOCKING_MODE);
	iface->attach(cellular_callback);
	CellularContext *ctx = (CellularContext *)iface;
	if (!BLOCKING_MODE) {
		op = OP_DEVICE_READY;
	} else {
		op = OP_INVALID;
	}
	nsapi_error_t err = ctx->set_device_ready();
	tr_info("set_device_ready: %d", err);
	int sema_err;
	if (!BLOCKING_MODE) {
		sema_err = semaphore.wait(50000);
		tr_info("sema_err: %d", sema_err);
		op = OP_SIM_READY;
	}

	err = ctx->set_sim_ready();
	tr_info("set_sim_ready: %d", err);
	if (!BLOCKING_MODE) {
		sema_err = semaphore.wait(50000);
		tr_info("sema_err: %d", sema_err);
		op = OP_REGISTER;
	}

	err = ctx->register_to_network();
	tr_info("register_to_network: %d", err);

	if (!BLOCKING_MODE) {
		sema_err = semaphore.wait(50000);
		tr_info("sema_err: %d", sema_err);
		op = OP_ATTACH;
	}
	wait(3);
	err = ctx->attach_to_network();
	tr_info("attach_to_network: %d", err);

	if (!BLOCKING_MODE) {
		sema_err = semaphore.wait(50000);
		tr_info("sema_err: %d", sema_err);
	}

	run_example(iface);
}

#include "RDA_8955_PPP.h"

int main()
{
#if MBED_CONF_MBED_TRACE_ENABLE
    trace_open();
#else
    dot_thread.start(dot_event);
#endif // #if MBED_CONF_MBED_TRACE_ENABLE

    print_function("\n\nmbed-os-example-cellular\n");

    // First with default interface
    print_function("\nEstablishing connection with default interface (nb-iot)...\n\n");
    run_example(NetworkInterface::get_default_instance());

#if MBED_CONF_APP_TEST_RDA8955
    // Then with secondary interface
    print_function("\nEstablishing connection with secondary interface (GPRS)...\n\n");
    UARTSerial serial(MBED_CONF_RDA_8955_PPP_TX, MBED_CONF_RDA_8955_PPP_RX, MBED_CONF_RDA_8955_PPP_BAUDRATE);
    RDA_8955_PPP device(&serial);
    CellularContext *context = device.create_context(NULL, NULL, MBED_CONF_CELLULAR_CONTROL_PLANE_OPT);
    MBED_ASSERT(context);
    context->set_default_parameters();
    run_example(context);
#endif

#if MBED_CONF_MBED_TRACE_ENABLE
    trace_close();
#else
    dot_thread.terminate();
#endif // #if MBED_CONF_MBED_TRACE_ENABLE

    return 0;
}
// EOF
