#include "pico/stdlib.h"

#include <cstdint>

#ifndef PICO_DEFAULT_LED_PIN
#error "The selected board does not define PICO_DEFAULT_LED_PIN"
#endif

namespace {
constexpr uint INPUT_PIN_A = 16;
constexpr uint INPUT_PIN_B = 21;
constexpr uint LED_PIN = PICO_DEFAULT_LED_PIN;
constexpr uint64_t BLINK_INTERVAL_US = 250'000;
}

int main() {
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
