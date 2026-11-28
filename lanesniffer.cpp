#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "pico/binary_info.h"
#include "pico/critical_section.h"   // ← add this
#include <cstdio>

bi_decl(bi_ptr_string(
    BINARY_INFO_MAKE_TAG('V', 'J'),
    0x4c534e01,
    current_epoch_millis,
    "0",
    14
));

static constexpr uint BUTTON_PINS[] = {16, 17, 15, 14};

static critical_section_t print_cs;  // ← add this

static void gpio_callback(uint gpio, uint32_t events)
{
    if (events & GPIO_IRQ_EDGE_FALL) {
        critical_section_enter_blocking(&print_cs);
        printf("GP%u:%llu\n", gpio, (unsigned long long)time_us_64());
        critical_section_exit(&print_cs);
    }
}

int main()
{
    stdio_init_all();
    critical_section_init(&print_cs);  // ← add this (claims a HW spinlock)

    for (uint pin : BUTTON_PINS) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_up(pin);
    }

    gpio_set_irq_enabled_with_callback(
        16, GPIO_IRQ_EDGE_FALL, true, gpio_callback
    );
    gpio_set_irq_enabled(17, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(15, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(14, GPIO_IRQ_EDGE_FALL, true);

    if (watchdog_caused_reboot()) {
        printf("Rebooted by watchdog\n");
    }

    watchdog_enable(1100, true);

    while (true) {
        sleep_ms(100);
        watchdog_update();
    }
}   