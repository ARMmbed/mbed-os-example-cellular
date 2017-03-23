#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>
#include "mbed.h"
#include "BufferedSerial.h"

#define UBLOX           0
#define MTS_DRAGONFLY   1

#if MBED_CONF_APP_PLATFORM == UBLOX
#include "UbloxCellularInterface.h"
#include "ublox_low_level_api.h"
UbloxCellularInterface *iface;
#elif MBED_CONF_APP_PLATFORM == MTS_DRAGONFLY
#include "DragonFlyCellularInterface.h"
DragonFlyCellularInterface *iface;
#endif
#include "UDPSocket.h"
#include "common_functions.h"
#if defined(FEATURE_COMMON_PAL)
#include "mbed_trace.h"
#define TRACE_GROUP "MAIN"
#else
#define tr_debug(...) (void(0)) //dummies if feature common pal is not added
#define tr_info(...)  (void(0)) //dummies if feature common pal is not added
#define tr_error(...) (void(0)) //dummies if feature common pal is not added
#endif //defined(FEATURE_COMMON_PAL)

UDPSocket *socket;
static const char *host_name = "2.pool.ntp.org";
static const int port = 123;
static Mutex mtx;
static nsapi_error_t connection_down_reason = 0;

void ppp_connection_down_cb(nsapi_error_t error)
{
    switch (error) {
        case NSAPI_ERROR_CONNECTION_LOST:
        case NSAPI_ERROR_NO_CONNECTION:
            tr_debug("Carrier/Connection lost");
            break;
        case NSAPI_ERROR_CONNECTION_TIMEOUT:
            tr_debug("Connection timed out.");
            break;
        case NSAPI_ERROR_AUTH_FAILURE:
            tr_debug("Authentication failure");
            break;
    }

    connection_down_reason = error;
}

static void lock()
{
    mtx.lock();
}

static void unlock()
{
    mtx.unlock();
}

// main() runs in its own thread in the OS
// (note the calls to wait below for delays)

int do_ntp()
{
    int ntp_values[12] = { 0 };
    time_t TIME1970 = 2208988800U;

    UDPSocket sock;

    int ret = sock.open(iface);
    if (ret) {
        tr_error("UDPSocket.open() fails, code: %d", ret);
        return -1;
    }

    SocketAddress nist;
    ret = iface->gethostbyname(host_name, &nist);
    if (ret) {
        tr_error("Couldn't resolve remote host: %s, code: %d", host_name, ret);
        return -1;
    }
    nist.set_port(port);

    tr_info("UDP: NIST server %s address: %s on port %d", host_name,
           nist.get_ip_address(), nist.get_port());

    memset(ntp_values, 0x00, sizeof(ntp_values));
    ntp_values[0] = '\x1b';

    sock.set_timeout(5000);

    int ret_send = sock.sendto(nist, (void*) ntp_values, sizeof(ntp_values));
    tr_debug("UDP: Sent %d Bytes to NTP server", ret_send);

    const int n = sock.recvfrom(&nist, (void*) ntp_values, sizeof(ntp_values));
    sock.close();

    if (n > 0) {
        tr_debug("UDP: Recved from NTP server %d Bytes", n);
        tr_debug("UDP: Values returned by NTP server:");
        for (size_t i = 0; i < sizeof(ntp_values) / sizeof(ntp_values[0]);
                ++i) {
            tr_debug("\t[%02d] 0x%" PRIX32, i,
                   common_read_32_bit((uint8_t*) &(ntp_values[i])));
            if (i == 10) {
                const time_t timestamp = common_read_32_bit(
                        (uint8_t*) &(ntp_values[i])) - TIME1970;
                struct tm *local_time = localtime(&timestamp);
                if (local_time) {
                    char time_string[25];
                    if (strftime(time_string, sizeof(time_string),
                                 "%a %b %d %H:%M:%S %Y", local_time) > 0) {
                        tr_info("NTP timestamp is %s", time_string);
                    }
                }
            }
        }
        return 0;
    }

    if (n == NSAPI_ERROR_WOULD_BLOCK) {
        return -1;
    }

    return -1;
}

#if MBED_CONF_APP_PLATFORM == UBLOX
UbloxCellularInterface my_iface(false, true);
#elif MBED_CONF_APP_PLATFORM == MTS_DRAGONFLY
DragonFlyCellularInterface my_iface(false);
#endif

nsapi_error_t connection()
{
    nsapi_error_t retcode;
    bool disconnected = false;

    while (!iface->isConnected()) {

        retcode = iface->connect();
        if (retcode == NSAPI_ERROR_AUTH_FAILURE) {
            tr_error("Authentication Failure. Exiting application");
            return retcode;
        } else if (retcode != NSAPI_ERROR_OK) {
            tr_error("Couldn't connect: %d", retcode);
            continue;
        }

        break;
    }

    tr_info("Connection Established.");

    return NSAPI_ERROR_OK;
}

int getTime()
{
    int retcode = -1;
    if (iface->isConnected()) {
        retcode = do_ntp();
    } else {
        /* Determine why the network is down */
        tr_warn("Connection down: %d", connection_down_reason);

        if (connection_down_reason == NSAPI_ERROR_AUTH_FAILURE) {
            tr_debug("Authentication Error");
        } else if (connection_down_reason == NSAPI_ERROR_NO_CONNECTION
                || NSAPI_ERROR_CONNECTION_LOST) {
            tr_debug("Carrier lost");
        } else if (connection_down_reason == NSAPI_ERROR_CONNECTION_TIMEOUT) {
            tr_debug("Connection timed out");
        }

        return -1;
    }

    return 0;
}

int main()
{
    mbed_trace_init();

    mbed_trace_mutex_wait_function_set(lock);
    mbed_trace_mutex_release_function_set(unlock);

    nsapi_error_t retcode = NSAPI_ERROR_OK;

    iface = &my_iface;
    iface->set_SIM_pin("1234");

    iface->set_credentials("internet");

    iface->connection_lost_notification_cb(ppp_connection_down_cb);

   retcode  = connection();
   if (retcode == NSAPI_ERROR_AUTH_FAILURE) {
       tr_error("Authentication Failure. Exiting application");
       return -1;
   }

   if (getTime() == 0) {
       tr_info("Done.");
   }

   return 0;
}

