/* GPLv2 License
 *
 * Copyright (C) 2016-2018 Lixing Ding <ding.lixing@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 **/

#include "hardware.h"

#define PIN_MASK        0x3F

static uint16_t hw_gpio_used[GPIO_BANK_MAX];
static hw_pindata_t *hw_gpio_data[GPIO_PIN_MAX];

static const uint32_t hw_gpio_rcc[GPIO_BANK_MAX] = {
    RCC_GPIOA, RCC_GPIOB, RCC_GPIOC, RCC_GPIOD, RCC_GPIOE, RCC_GPIOF, RCC_GPIOF
};
static const uint32_t hw_gpio_bank[GPIO_BANK_MAX] = {
    GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, GPIOF, GPIOG
};

int hw_setup_gpio(void)
{
    int i;

    /* initial all pin useable */
    for (i = 0; i < GPIO_BANK_MAX; i++) {
        hw_gpio_used[i] = 0;
    }

    /* initial all port isr useable */
    for (i = 0; i < GPIO_PIN_MAX; i++) {
        hw_gpio_data[i] = NULL;
    }

    /* initial pin device control blocks */

    return 0;
}

static inline int hw_gpio_use(int bank, uint16_t pins)
{
    if (bank >= GPIO_BANK_MAX) {
        return -1;
    }

    if (!hw_gpio_used[bank]) {
        rcc_periph_clock_enable(hw_gpio_rcc[bank]);
    }

    hw_gpio_used[bank] |= pins;

    return 0;
}

int hw_gpio_unuse(int bank, uint16_t pins)
{
    uint32_t bank_base;
    if (bank >= GPIO_BANK_MAX) {
        return -1;
    }

    bank_base = hw_gpio_bank[bank];
    GPIO_ODR(bank_base) &= ~pins;
    gpio_set_mode(bank_base, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, pins);

    hw_gpio_used[bank] &= ~pins;
    if (!hw_gpio_used[bank]) {
        rcc_periph_clock_disable(hw_gpio_rcc[bank]);
    }

    return 0;
}

int hw_gpio_setup(int bank, uint16_t pins, uint8_t mode, uint8_t cnf, uint8_t pullup)
{
    uint32_t bank_base = hw_gpio_bank[bank];

    if (hw_gpio_use(bank, pins)) {
        return -CUPKEE_EINVAL;
    }

    gpio_set_mode(bank_base, mode, cnf, pins);

    if (pullup) {
        GPIO_ODR(bank_base) |= pins;
    } else {
        GPIO_ODR(bank_base) &= ~pins;
    }

    return 0;
}

int hw_gpio_mode_set(uint8_t bank, uint8_t port, uint8_t mode)
{
    uint16_t ports;

    if (bank >= GPIO_BANK_MAX || port >= GPIO_PIN_MAX) {
        return -CUPKEE_EINVAL;
    }

    ports = 1 << port;
    switch (mode) {
    case CUPKEE_PIN_MODE_NE:
        return hw_gpio_unuse(bank, ports);
    case CUPKEE_PIN_MODE_IN:
        return hw_gpio_setup(bank, ports,
                             GPIO_MODE_INPUT,
                             GPIO_CNF_INPUT_FLOAT, 0);
    case CUPKEE_PIN_MODE_OUT:
        return hw_gpio_setup(bank, ports,
                             GPIO_MODE_OUTPUT_10_MHZ,
                             GPIO_CNF_OUTPUT_PUSHPULL, 0);
    case CUPKEE_PIN_MODE_AIN:
    case CUPKEE_PIN_MODE_AOUT:
        return -CUPKEE_EINVAL;

    case CUPKEE_PIN_MODE_IN_PULLUP:
        return hw_gpio_setup(bank, ports,
                             GPIO_MODE_INPUT,
                             GPIO_CNF_INPUT_PULL_UPDOWN, 1);
    case CUPKEE_PIN_MODE_IN_PULLDOWN:
        return hw_gpio_setup(bank, ports,
                             GPIO_MODE_INPUT,
                             GPIO_CNF_INPUT_PULL_UPDOWN, 0);
    case CUPKEE_PIN_MODE_OPENDRAIN:
        return hw_gpio_setup(bank, ports,
                             GPIO_MODE_OUTPUT_10_MHZ,
                             GPIO_CNF_OUTPUT_OPENDRAIN, 0);
    default:
        return -CUPKEE_EINVAL;
    }
}

int hw_gpio_mode_get(uint8_t bank, uint8_t port)
{
    uint8_t mod, cnf, odr;
    uint32_t base;

    if (bank >= GPIO_BANK_MAX || port >= GPIO_PIN_MAX) {
        return -CUPKEE_EINVAL;
    }

    base = hw_gpio_bank[bank];
    if (port >= 8) {
        mod = (GPIO_CRL(base) >> (port * 4)) & 0xf;
    } else {
        mod = (GPIO_CRH(base) >> ((port - 8) * 4)) & 0xf;
    }
    cnf = mod >> 2;
    mod = mod & 0x3;
    odr = (GPIO_ODR(base) >> port) & 1;

    if (mod) { // Output
        switch(cnf) {
        case 0: return CUPKEE_PIN_MODE_OUT;
        case 1: return CUPKEE_PIN_MODE_OPENDRAIN;
        default: return CUPKEE_PIN_MODE_ALTFN;
        }
    } else { // input
        switch(cnf) {
        case 0: return CUPKEE_PIN_MODE_AIN;
        case 1: return CUPKEE_PIN_MODE_IN;
        case 2: return odr ? CUPKEE_PIN_MODE_IN_PULLUP : CUPKEE_PIN_MODE_IN_PULLDOWN;
        default: return CUPKEE_PIN_MODE_NE;
        }
    }
}

int hw_gpio_set(uint8_t bank, uint8_t port, int v)
{
    if (bank < GPIO_BANK_MAX && port < GPIO_PIN_MAX) {
        uint32_t bank_base = hw_gpio_bank[bank];

        GPIO_BSRR(bank_base) = v ? (1 << port) : (1 << (port + 16));
        return 1;
    } else {
        return -CUPKEE_EINVAL;
    }
}

int hw_gpio_get(uint8_t bank, uint8_t port)
{
    if (bank < GPIO_BANK_MAX && port < GPIO_PIN_MAX) {
        uint32_t bank_base = hw_gpio_bank[bank];
        return (GPIO_IDR(bank_base) & (1 << port)) != 0;
    } else {
        return -CUPKEE_EINVAL;
    }
}

int hw_gpio_toggle(uint8_t bank, uint8_t port)
{
    if (bank < GPIO_BANK_MAX && port < GPIO_PIN_MAX) {
        uint32_t bank_base = hw_gpio_bank[bank];

        gpio_toggle(bank_base, 1 << port);
        return 0;
    } else {
        return -CUPKEE_EINVAL;
    }
}

int hw_gpio_listen(uint8_t bank, uint8_t port, hw_pindata_t *data)
{
    if (bank < GPIO_BANK_MAX && port < GPIO_PIN_MAX) {
        hw_pindata_t *pdata = hw_gpio_data[port];
        uint32_t exti;

        if (pdata) {
            if (pdata != data) {
                return -CUPKEE_ERESOURCE;
            }
        } else {
            pdata = hw_gpio_data[port] = data;
            // Use data8 to keep pin bank
            pdata->data8 = bank;
        }

        pdata->data32 = _cupkee_systicks;
        exti = 1 << port;
        rcc_periph_clock_enable(RCC_AFIO);
        if (port < 5) {
            nvic_enable_irq(NVIC_EXTI0_IRQ + port);
        } else
        if (port < 10) {
            nvic_enable_irq(NVIC_EXTI9_5_IRQ);
        } else {
            nvic_enable_irq(NVIC_EXTI15_10_IRQ);
        }

        exti_select_source(exti, hw_gpio_bank[bank]);
        exti_set_trigger(exti, EXTI_TRIGGER_BOTH);
        exti_enable_request(exti);

        return 0;
    } else {
        return -CUPKEE_EINVAL;
    }
}

int hw_gpio_ignore(uint8_t bank, uint8_t port)
{
    if (bank < GPIO_BANK_MAX && port < GPIO_PIN_MAX) {
        hw_pindata_t *pdata = hw_gpio_data[port];

        if (!pdata || pdata->data8 != bank) {
            return 0;
        }

        hw_gpio_data[port] = NULL;

        if (port < 5) {
            nvic_disable_irq(NVIC_EXTI0_IRQ + port);
        } else
        if (port < 10) {
            nvic_disable_irq(NVIC_EXTI9_5_IRQ);
        } else {
            nvic_disable_irq(NVIC_EXTI15_10_IRQ);
        }

        exti_disable_request(1 << port);
        rcc_periph_clock_enable(RCC_AFIO);

        return 0;
    } else {
        return -CUPKEE_EINVAL;
    }
}

static inline void exti_isr_handler(int port)
{
    hw_pindata_t *pdata = hw_gpio_data[port];

    pdata->duration = _cupkee_auxticks - pdata->data32;
    pdata->data32 = _cupkee_auxticks;

    cupkee_event_post_pin(pdata->id, (GPIO_IDR(hw_gpio_bank[pdata->data8]) >> port) & 1);
}

void exti0_isr(void)
{
    exti_isr_handler(0);
    EXTI_PR = EXTI0;
}

void exti1_isr(void)
{
    exti_isr_handler(1);
    EXTI_PR = EXTI1;
}

void exti2_isr(void)
{
    exti_isr_handler(2);
    EXTI_PR = EXTI2;
}

void exti3_isr(void)
{
    exti_isr_handler(3);
    EXTI_PR = EXTI3;
}

void exti4_isr(void)
{
    exti_isr_handler(4);
    EXTI_PR = EXTI4;
}

void exti9_5_isr(void)
{
    uint16_t bits = EXTI_PR & 0x3E0;
    int i;

    for (i = 5; i < 10; i++) {
        if (bits & (1 << i)) {
            exti_isr_handler(i);
        }
    }
    EXTI_PR = bits;
}

void exti15_10_isr(void)
{
    uint16_t bits = EXTI_PR & 0xFC00;
    int i;

    for (i = 10; i < 16; i++) {
        if (bits & (1 << i)) {
            exti_isr_handler(i);
        }
    }
    EXTI_PR = bits;
}

