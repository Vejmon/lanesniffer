#include "pico/stdlib.h"
#include "hardware/rtc.h"

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

void usb_chars_available(void *) {
    // Keep the callback short; parse and update the RTC in the main loop.
    usb_rx_pending = true;
}

// Convert Unix seconds (UTC) to the calendar format expected by the Pico RTC.
bool unix_seconds_to_datetime(uint64_t timestamp, datetime_t *datetime) {
    if (timestamp > MAX_UNIX_TIMESTAMP) {
        return false;
    }

    const uint64_t days_since_epoch = timestamp / 86400;
    const uint32_t seconds_today = static_cast<uint32_t>(timestamp % 86400);

    // Gregorian civil-date conversion, with 1970-01-01 as the Unix epoch.
    const int64_t z = static_cast<int64_t>(days_since_epoch) + 719468;
    const int64_t era = z / 146097;
    const uint32_t day_of_era = static_cast<uint32_t>(z - era * 146097);
    const uint32_t year_of_era = (day_of_era - day_of_era / 1460 + day_of_era / 36524 - day_of_era / 146096) / 365;
    int year = static_cast<int>(year_of_era) + static_cast<int>(era * 400);
    const uint32_t day_of_year = day_of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
    const uint32_t month_part = (5 * day_of_year + 2) / 153;
    const uint32_t day = day_of_year - (153 * month_part + 2) / 5 + 1;
    const int month = static_cast<int>(month_part) + (month_part < 10 ? 3 : -9);
    year += month <= 2;

    datetime->year = static_cast<int16_t>(year);
    datetime->month = static_cast<int8_t>(month);
    datetime->day = static_cast<int8_t>(day);
    datetime->dotw = static_cast<int8_t>((days_since_epoch + 4) % 7); // Sunday = 0
    datetime->hour = static_cast<int8_t>(seconds_today / 3600);
    datetime->min = static_cast<int8_t>((seconds_today / 60) % 60);
    datetime->sec = static_cast<int8_t>(seconds_today % 60);
    return true;
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
            datetime_t datetime{};
            if (unix_seconds_to_datetime(value, &datetime)) {
                rtc_set_datetime(&datetime);
                printf("time_ok\n");
                fflush(stdout);
            }
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

int main() {
    stdio_init_all();
    stdio_set_chars_available_callback(usb_chars_available, nullptr);
    rtc_init();

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
