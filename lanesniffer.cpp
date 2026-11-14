#include "pico/stdlib.h"

#include <cstdint>
#include <cstdio>

#ifndef PICO_DEFAULT_LED_PIN
#error "The selected board does not define PICO_DEFAULT_LED_PIN"
#endif

namespace {
constexpr uint INPUT_PIN_A = 16;
constexpr uint INPUT_PIN_B = 21;
constexpr uint LED_PIN = PICO_DEFAULT_LED_PIN;
constexpr uint64_t BLINK_INTERVAL_US = 250'000;
constexpr uint64_t MAX_UNIX_TIMESTAMP = 253402300799ULL; // 9999-12-31 23:59:59 UTC

volatile bool usb_rx_pending = false;
uint64_t unix_epoch_at_sync = 0;
uint64_t uptime_at_sync_us = 0;
bool time_is_set = false;

void usb_chars_available(void *) {
    // Keep the callback short; parse and update the software clock in the main loop.
    usb_rx_pending = true;
}

void process_usb_timestamp_char(int ch) {
    // Host sends one decimal Unix timestamp in seconds, terminated by LF.
    static uint64_t value = 0;
    static bool has_digit = false;
    static bool invalid = false;

    if (ch == '\r') {
        return; // Accept CRLF as well as LF.
    }

    if (ch == '\n') {
        if (has_digit && !invalid) {
            unix_epoch_at_sync = value;
            uptime_at_sync_us = time_us_64();
            time_is_set = true;
            printf("time_ok\n");
            fflush(stdout);
        }
        value = 0;
        has_digit = false;
        invalid = false;
        return;
    }

    if (ch < '0' || ch > '9') {
        invalid = true;
        return;
    }

    const uint64_t digit = static_cast<uint64_t>(ch - '0');
    if (value > (MAX_UNIX_TIMESTAMP - digit) / 10) {
        invalid = true;
        return;
    }
    value = value * 10 + digit;
    has_digit = true;
}

void poll_usb_timestamp() {
    if (!usb_rx_pending) {
        return;
    }
    usb_rx_pending = false;

    int ch;
    while ((ch = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
        process_usb_timestamp_char(ch);
    }
}
}

// Returns false until the host has supplied a valid timestamp.
bool get_unix_time(uint64_t *timestamp) {
    if (!time_is_set || timestamp == nullptr) {
        return false;
    }

    *timestamp = unix_epoch_at_sync + (time_us_64() - uptime_at_sync_us) / 1'000'000;
    return true;
}

int main() {
    stdio_init_all();
    stdio_set_chars_available_callback(usb_chars_available, nullptr);

    gpio_init(INPUT_PIN_A);
    gpio_set_dir(INPUT_PIN_A, GPIO_IN);
    gpio_pull_down(INPUT_PIN_A);

    gpio_init(INPUT_PIN_B);
    gpio_set_dir(INPUT_PIN_B, GPIO_IN);
    gpio_pull_down(INPUT_PIN_B);

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, false);

    bool was_triggered = false;
    bool led_on = false;
    uint64_t last_toggle_us = 0;

    while (true) {
        poll_usb_timestamp();

        const bool triggered = gpio_get(INPUT_PIN_A) || gpio_get(INPUT_PIN_B);
        const uint64_t now_us = time_us_64();

        if (!triggered) {
            if (led_on) {
                gpio_put(LED_PIN, false);
                led_on = false;
            }
        } else if (!was_triggered) {
            // Start the blink immediately when either input first goes high.
            led_on = true;
            gpio_put(LED_PIN, true);
            last_toggle_us = now_us;
        } else if (now_us - last_toggle_us >= BLINK_INTERVAL_US) {
            led_on = !led_on;
            gpio_put(LED_PIN, led_on);
            last_toggle_us = now_us;
        }

        was_triggered = triggered;
        sleep_ms(10);
    }
}
