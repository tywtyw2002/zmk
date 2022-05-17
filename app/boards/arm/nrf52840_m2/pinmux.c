/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <kernel.h>
#include <device.h>
#include <init.h>
#include <drivers/gpio.h>
#include <sys/sys_io.h>
#include <devicetree.h>

static void button_pressed(const struct device *dev, struct gpio_callback *cb,
               uint32_t pins)
{
    const struct device *p0 = device_get_binding("GPIO_0");
    gpio_pin_set(p0, 28, 0);

}

static void configure_button(const struct device *gpio)
{
    static struct gpio_callback button_cb;

    gpio_pin_configure(gpio, 27, GPIO_INPUT);
    gpio_pin_interrupt_configure(gpio, 27, GPIO_INT_EDGE_TO_ACTIVE);

    gpio_init_callback(&button_cb, button_pressed, BIT(27));

    gpio_add_callback(gpio, &button_cb);
}


static int pinmux_nrf52840_m2_init(const struct device *port) {
    ARG_UNUSED(port);

    const struct device *p0 = device_get_binding("GPIO_0");
    gpio_pin_configure(p0, 28, GPIO_OUTPUT);
    gpio_pin_set(p0, 28, 1);

    configure_button(p0);

    // const struct device *p1 = device_get_binding("GPIO_1");
    // gpio_pin_configure(p1, 4, GPIO_OUTPUT);
    // gpio_pin_set(p1, 4, 0);

    // configure_button(p0);

    return 0;
}

SYS_INIT(pinmux_nrf52840_m2_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);