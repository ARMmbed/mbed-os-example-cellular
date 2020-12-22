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
#include "CellularLog.h"

#ifndef CELLULAR_DEMO_TRACING_H_
#define CELLULAR_DEMO_TRACING_H_

#if MBED_CONF_MBED_TRACE_ENABLE
static PlatformMutex trace_mutex;

static void trace_wait()
{
    trace_mutex.lock();
}

static void trace_release()
{
    trace_mutex.unlock();
}

static char* trace_time(size_t ss)
{
    static char time_st[50];
    auto ms = std::chrono::time_point_cast<std::chrono::milliseconds>(Kernel::Clock::now()).time_since_epoch().count();
    snprintf(time_st, 49, "[%08llums]", ms);
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

#ifdef MBED_CONF_NSAPI_DEFAULT_CELLULAR_PLMN
    printf("\n\n[MAIN], plmn: %s\n", (MBED_CONF_NSAPI_DEFAULT_CELLULAR_PLMN ? MBED_CONF_NSAPI_DEFAULT_CELLULAR_PLMN : "NULL"));
#endif
}

static void trace_close()
{
    mbed_cellular_trace::mutex_wait_function_set(NULL);
    mbed_cellular_trace::mutex_release_function_set(NULL);

    mbed_trace_free();
}
#else
static void trace_open()
{
}

static void trace_close()
{
}
#endif // #if MBED_CONF_MBED_TRACE_ENABLE

#endif // CELLULAR_DEMO_TRACING_H_
