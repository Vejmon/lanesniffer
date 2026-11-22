#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include <cstdio>

static constexpr uint BUTTON_PINS[] = {16, 17, 18,19};

static void gpio_callback(uint gpio, uint32_t events)
{
    if (events & GPIO_IRQ_EDGE_FALL) {
        printf("Button on GP%u triggered\n", gpio);
    }
}

int main()
{
    stdio_init_all();

    for (uint pin : BUTTON_PINS) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_up(pin);
    }

    // Pin 16 registers the callback. Pin 17 is enabled separately.
    gpio_set_irq_enabled_with_callback(
        16,
        GPIO_IRQ_EDGE_FALL,
        true,
        gpio_callback
    );
    gpio_set_irq_enabled(17, GPIO_IRQ_EDGE_FALL, true);

    if (watchdog_caused_reboot()) {
        printf("Rebooted by watchdog\n");
    }

    // Reset unless watchdog_update() is called every 1100 ms.
    watchdog_enable(1100, true);

    while (true) {
        // sleep_ms wakes periodically, so watchdog_update() is refreshed
        // even when neither button is pressed.
        sleep_ms(100);
        watchdog_update();
    }
}