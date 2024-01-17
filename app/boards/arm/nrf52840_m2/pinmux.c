/*
 * Copyright (c) 2023 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/sys_io.h>
#include <zephyr/devicetree.h>
#include <hal/nrf_gpio.h>

#define _PINNUM(port, pin) ((port)*32 + (pin))
#define BATTERY_ENABLE _PINNUM(0, 28)
#define CHARGING_DETECT _PINNUM(0, 3)

static const struct device *p0 = DEVICE_DT_GET(DT_NODELABEL(gpio0));

static inline void power_off(void) {
    nrf_gpio_pin_write(BATTERY_ENABLE, false); // turn off the external power source
}

static inline bool is_charging(void) {
    // 0: charging
    return nrf_gpio_pin_read(CHARGING_DETECT) == 0;
}

static void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    // wait the button stable at 0 so the keyboard won't be powered up accidentally
    k_sleep(K_SECONDS(1));

    if (!is_charging()) {
        power_off();
    }
}

static int pinmux_nrf52840_m2_init(const struct device *port) {
    ARG_UNUSED(port);

    // back button
    // NOTE: if used to power off the keyboard, make sure the action is done
    // AFTER the button is released.
    //
    // To wake up the keyboard from sleep with this button, must use interrupt.
    // To avoid the callback triggered after keyboard woke up(powering off the
    // keyboard), must use GPIO_INT_EDGE_FALLING(trigger on button pressed).
    gpio_pin_interrupt_configure(p0, 27, GPIO_INT_EDGE_FALLING);

    // GPIO 0.28(LDO control) is already configured by bootloader,
    // so configuring here is just an ensurance.
    // Making it a push-pull output will improve power stability.
    nrf_gpio_cfg(BATTERY_ENABLE, NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT,
                 NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_S0S1, NRF_GPIO_PIN_NOSENSE);
    nrf_gpio_pin_write(BATTERY_ENABLE, true);

    // this is the pin for charging detection
    nrf_gpio_cfg_input(CHARGING_DETECT, NRF_GPIO_PIN_PULLUP);

    // setup the interrupts to poweroff the keyboard
    static struct gpio_callback button_cb;
    gpio_init_callback(&button_cb, button_pressed, BIT(27));
    gpio_add_callback(p0, &button_cb);

    return 0;
}

SYS_INIT(pinmux_nrf52840_m2_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
