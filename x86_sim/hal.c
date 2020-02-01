//
// BSD 3-Clause License
//
// Hardware Abstraction Layer (HAL) targeted to Raspberry Pi and
// Dragino LoRa/GPS HAT
//
// Copyright (c) 2017, Wolfgang Klenk
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// * Neither the name of the copyright holder nor the names of its
//   contributors may be used to endorse or promote products derived from
//  this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <assert.h>
#include <time.h>

#include "lmic.h"
#include "sx1276sim.h"

#define OUTPUT 0
#define INPUT 1
#define LOW 0
#define HIGH 1
#define pinMode(a,b) printf("GPIO mode: %d %s\n", a, (b) ? "in" : "out")
#define digitalWrite(a,b) printf("GPIO write: %d %s\n", a , (b) ? "hi" : "lo")
#define digitalRead(a) printf("GPIO read: %d\n", a)

// Note: WiringPi Pin Numbering Schema
const int WIRING_PI_PIN_NSS = 6;
const int WIRING_PI_PIN_RST = 0;
const int WIRING_PI_PIN_DIO[3] = { 7, 4, 5 };

// Note: BCM Pin Numbering Schema
const int BCM_PIN_DIO[3] = { 4, 23, 24 };

// Local function prototypes
void hal_time_init();
void timespec_diff(struct timespec *start, struct timespec *stop, struct timespec *result);

// Used to store the current time
static struct timespec ts_start;

static s4_t sleep_interval_ms = 0;

void hal_init () {
   hal_time_init();

#ifdef DEBUG_HAL
   fprintf(stdout, "%09d HAL: Initializing ...\n", osticks2ms(hal_ticks()));
#endif

   // Pin Direction
   pinMode(WIRING_PI_PIN_NSS, OUTPUT);
   pinMode(WIRING_PI_PIN_RST, INPUT);
}

void hal_failed () {
   fprintf(stderr, "%09d HAL: Failed. Aborting.\n", osticks2ms(hal_ticks()));

   exit(EXIT_FAILURE);
}

// set radio NSS pin to given value
void hal_pin_nss (u1_t val) {
   sim_setNss(val);
}

// switch between radio RX/TX
void hal_pin_rxtx (u1_t val) {
#ifdef DEBUG_HAL
   val > 0 ? fprintf(stdout, "%09d HAL: Sending ...\n", osticks2ms(hal_ticks())) : fprintf(stdout, "%09d HAL: Receiving ...\n", osticks2ms(hal_ticks()));
#endif

   // Nothing to do. There is no such pin in the Lora/GPS HAT module.
}

// set radio RST pin to given value (or keep floating!)
void hal_pin_rst (u1_t val) {
#ifdef DEBUG_HAL
   fprintf(stdout, "%09d HAL: Set radio RST pin to 0x%02x\n", osticks2ms(hal_ticks()), val);
#endif

   if(val == 0 || val == 1) { // drive pin
      pinMode(WIRING_PI_PIN_RST, OUTPUT);
      digitalWrite(WIRING_PI_PIN_RST, val==0 ? LOW : HIGH);
   } else { // keep pin floating
      pinMode(WIRING_PI_PIN_RST, INPUT);
   }
}

// perform 8-bit SPI transaction.
// write given byte outval to radio, read byte from radio and return value.
u1_t hal_spi (u1_t out) {
   return sim_spiTransaction(out);
}

void hal_disableIRQs () {
}

void hal_enableIRQs () {
}

// store current timer value of the system.
void hal_time_init() {
   int rc = clock_gettime(CLOCK_MONOTONIC_RAW, &ts_start);
   if (rc < 0) {
      fprintf(stderr, "HAL: Cannot initialize timer: %s\n", strerror(errno));
      hal_failed();
   }
}

// return 32 bit system time in ticks.
// OSTICKS_PER_SEC has to be defined, else the default 32768 is used.
// OSTICKS_PER_SEC must be in range [10000:64516]. One tick must be 15.5us .. 100us long.
u4_t hal_ticks () {

   struct timespec ts;
   int rc = clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
   if (rc < 0) {
      fprintf(stderr, "HAL: Cannot get current time: %s\n", strerror(errno));
      hal_failed();
   }

   struct timespec ts_diff;
   timespec_diff(&ts_start, &ts, &ts_diff);

   ostime_t ticks_sec = sec2osticks(ts_diff.tv_sec);
   ostime_t ticks_nsec = us2osticks(ts_diff.tv_nsec / 1000);

   u4_t ticks = ticks_sec + ticks_nsec;

   return ticks;
}

// busy wait until timestamp (in ticks) is reached
void hal_waitUntil (u4_t target_ticks) {
#ifdef DEBUG_HAL
   fprintf(stdout, "%09d HAL: Wait until %09d ms\n", osticks2ms(hal_ticks()), osticks2ms(target_ticks));
#endif

   // TODO: Deal with tick counter overflow

   u4_t current_ticks = hal_ticks();
   s4_t diff_ticks = target_ticks - current_ticks;

   if (diff_ticks > 0) {
      s4_t diff_us = osticks2us(diff_ticks);
      usleep(diff_us);
   }
}

void timespec_diff(struct timespec *start, struct timespec *stop, struct timespec *result)
{
    if ((stop->tv_nsec - start->tv_nsec) < 0) {
        result->tv_sec = stop->tv_sec - start->tv_sec - 1;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
    } else {
        result->tv_sec = stop->tv_sec - start->tv_sec;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec;
    }
}

// Used for scheduled jobs:
// If target time is reached, then 1 is returned and the scheduled job is executed.
// If 0 is returned, then control goes back into the LMIC main loop.
u1_t hal_checkTimer (u4_t target_ticks) {

   u4_t current_ticks = hal_ticks();
   s4_t diff_ticks = target_ticks - current_ticks;
   s4_t diff_ms = osticks2ms(diff_ticks);

   if (diff_ticks <= 0) { // Reached target ticks
      return 1; // Execute scheduled job
   } else if (diff_ms < 21) {
      // We have not reached the target ticks, but we also don't
      // want to execute the scheduled job now. By returning 0 the code
      // will continue looping in the LMIC main loop.
      sleep_interval_ms = 0;
      return 0;
   } else {
      sleep_interval_ms = diff_ms - 20;
      assert(sleep_interval_ms > 0);

      // Use (diff_ms - 20) as timeout, to reduce the risk that the linux OS
      // causes the timeout longer as desired.
      // Let the code actively loop in the LMIC main loop until the final target time is reached.

      return 0; // Will cause a "sleep" in the LMIC main loop
   }
}

void hal_sleep () {
   if (sim_isInt()) {
      radio_irq_handler(0);
   }
}

