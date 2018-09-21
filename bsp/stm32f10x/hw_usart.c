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

#define USART_MAX       5
#define USART_RE        5
#define USART_TE        5

typedef struct hw_uart_t {
    uint8_t flags;
    void   *entry;
} hw_uart_t;

static hw_uart_t uarts[USART_MAX];

static const uint32_t reg_base[] = {
    USART1, USART2, USART3, UART4, UART5
};
static const uint32_t rcc_base[] = {
    RCC_USART1, RCC_USART2, RCC_USART3, RCC_UART4, RCC_UART5
};

static int uart_gpio_setup(int inst)
{
    uint8_t rx_mod = GPIO_MODE_INPUT;
    uint8_t rx_cnf = GPIO_CNF_INPUT_PULL_UPDOWN;
    uint8_t tx_mod = GPIO_MODE_OUTPUT_50_MHZ;
    uint8_t tx_cnf = GPIO_CNF_OUTPUT_ALTFN_PUSHPULL;

    switch(inst) {
    case 0:
        hw_gpio_setup(0, GPIO_USART1_RX, rx_mod, rx_cnf, 0);
        hw_gpio_setup(0, GPIO_USART1_TX, tx_mod, tx_cnf, 0);
        break;
    case 1:
        hw_gpio_setup(0, GPIO_USART2_RX, rx_mod, rx_cnf, 0);
        hw_gpio_setup(0, GPIO_USART2_TX, tx_mod, tx_cnf, 0);
        break;
    case 2:
        hw_gpio_setup(1, GPIO_USART3_RX, rx_mod, rx_cnf, 0);
        hw_gpio_setup(1, GPIO_USART3_TX, tx_mod, tx_cnf, 0);
        break;
    case 3:
        hw_gpio_setup(2, GPIO_UART4_RX, rx_mod, rx_cnf, 0);
        hw_gpio_setup(2, GPIO_UART4_TX, tx_mod, tx_cnf, 0);
        break;
    case 4:
        hw_gpio_setup(3, GPIO_UART5_RX, rx_mod, rx_cnf, 0);
        hw_gpio_setup(2, GPIO_UART5_TX, tx_mod, tx_cnf, 0);
        break;
    default: return -1;
    }

    return CUPKEE_OK;
}

static inline hw_uart_t *uart_block(int inst) {
    return (unsigned)inst < USART_MAX ? &uarts[inst] : NULL;
}

static inline int uart_has_data(int inst) {
    return USART_SR(reg_base[inst]) & USART_SR_RXNE;
}

static inline int uart_not_busy(int inst) {
    return USART_SR(reg_base[inst]) & USART_SR_TXE;
}

static inline uint8_t uart_data_get(int inst) {
    return USART_DR(reg_base[inst]);
}

static inline void uart_data_put(int inst, uint8_t data) {
    USART_DR(reg_base[inst]) = data;
}

static int uart_reset(int inst)
{
    hw_uart_t *uart = uart_block(inst);

    if (uart) {
        usart_disable(reg_base[inst]);
        uart->entry = NULL;

        return 0;
    } else {
        return -CUPKEE_EINVAL;
    }
}

static int uart_setup(int inst, void *entry)
{
    hw_uart_t *uart = uart_block(inst);
    cupkee_struct_t *conf;
    uint32_t baudrate, databits, stopbits, parity;
    int n;

    if (!uart) {
        return -CUPKEE_EINVAL;
    }
    conf = cupkee_device_config(entry);
    if (!conf) {
        return -CUPKEE_ERROR;
    }

    cupkee_struct_get_int(conf, 0, &n);
    baudrate = n;

    cupkee_struct_get_int(conf, 1, &n);
    databits = n;

    cupkee_struct_get_int(conf, 2, &n);
    if (n == 1) {
        parity = USART_PARITY_ODD;
    } else
    if (n == 2) {
        parity = USART_PARITY_EVEN;
    } else {
        parity = USART_PARITY_NONE;
    }

    cupkee_struct_get_int(conf, 3, &n);
    if (n == 2) {
        stopbits = USART_STOPBITS_2;
    } else {
        stopbits = USART_STOPBITS_1;
    }

    if (CUPKEE_OK != uart_gpio_setup(inst)) {
        return -CUPKEE_ERESOURCE;
    }

    rcc_periph_clock_enable(rcc_base[inst]);
	usart_set_baudrate(reg_base[inst], baudrate);
	usart_set_databits(reg_base[inst], databits);
	usart_set_stopbits(reg_base[inst], stopbits);
	usart_set_parity  (reg_base[inst], parity);
	usart_set_mode    (reg_base[inst], USART_MODE_TX_RX);
	usart_set_flow_control(reg_base[inst], USART_FLOWCONTROL_NONE);
    usart_enable(reg_base[inst]);

    uart->entry = entry;

    return 0;
}

static int uart_request(int inst)
{
    if ((unsigned)inst >= USART_MAX || uarts[inst].flags) {
        return -CUPKEE_EINVAL;
    }

    uarts[inst].flags = HW_FL_USED;
    uarts[inst].entry = NULL;

    return 0;
}

static int uart_release(int inst)
{
    hw_uart_t *uart = uart_block(inst);

    if (uart) {
        uart_reset(inst);
        uart->flags = 0;
        return 0;
    } else {
        return -1;
    }
}

static int uart_poll(int inst)
{
    hw_uart_t *uart = uart_block(inst);

    if (uart) {
        uint8_t data;

        if (uart->flags & HW_FL_RXE) {
            while (uart_has_data(inst)) {
                data = uart_data_get(inst);
                if (1 != cupkee_device_push(uart->entry, 1, &data)) {
                    uart->flags &= ~HW_FL_RXE;
                    break;
                }
            }
        }

        if (uart->flags & HW_FL_TXE) {
            while (uart_not_busy(inst)) {
                if(1 != cupkee_device_pull(uart->entry, 1, &data)) {
                    uart->flags &= ~HW_FL_TXE;
                    break;
                }
                uart_data_put(inst, data);
            }
        }
        return 0;
    } else {
        return -CUPKEE_EINVAL;
    }
}

static int uart_write(int inst, size_t n, const void *data)
{
    hw_uart_t *uart = uart_block(inst);

    if (n && data) { // sync write
        const uint8_t *ptr = data;
        size_t i = 0;

        while (i < n) {
            while (!uart_not_busy(inst)) {
            }
            uart_data_put(inst, ptr[i++]);
        }
        return i;
    } else {
        uart->flags |= HW_FL_TXE;
        return 0;
    }
}

static int uart_read(int inst, size_t n, void *data)
{
    hw_uart_t *uart = uart_block(inst);

    if (n && data) {
        uint32_t begin = cupkee_systicks();
        uint8_t *ptr = data;
        size_t i = 0;

        while (i < n) {
            while (!uart_has_data(inst)) {
                if (cupkee_systicks() - begin > 1000) {
                    return -CUPKEE_ETIMEOUT;
                }
            }
            ptr[i++] = uart_data_get(inst);
        }

        return i;
    } else {
        uart->flags |= HW_FL_RXE;
        return 0;
    }
}

static const char *parity_options[] = {
    "none", "odd", "even"
};

static const cupkee_struct_desc_t conf_desc[] = {
    {
        .name = "baudrate",
        .type = CUPKEE_STRUCT_UINT32
    },
    {
        .name = "databits",
        .type = CUPKEE_STRUCT_UINT8
    },
    {
        .name = "parity",
        .type = CUPKEE_STRUCT_OPT,
        .size = 3,
        .opt_names = parity_options
    },
    {
        .name = "stopbits",
        .type = CUPKEE_STRUCT_UINT8
    },
};

static cupkee_struct_t *uart_conf_init(void *curr)
{
    cupkee_struct_t *conf;

    if (curr) {
        conf = curr;
    } else {
        conf = cupkee_struct_alloc(4, conf_desc);
    }

    if (conf) {
        cupkee_struct_set_uint(conf, 0, 115200);
        cupkee_struct_set_uint(conf, 1, 8);
        cupkee_struct_set_string(conf, 2, "None");
        cupkee_struct_set_uint(conf, 3, 1);
    }

    return conf;
}

static const cupkee_driver_t uart_driver = {
    .request = uart_request,
    .release = uart_release,
    .reset   = uart_reset,
    .setup   = uart_setup,
    .poll    = uart_poll,

    .read    = uart_read,
    .write   = uart_write,
};

static const cupkee_device_desc_t hw_device_uart = {
    .name = "uart",
    .inst_max = USART_MAX,
    .conf_init = uart_conf_init,
    .driver = &uart_driver
};

void hw_setup_usart(void)
{
    int i;

    for (i = 0; i < USART_MAX; i++) {
        uarts[i].flags = 0;
    }

    cupkee_device_register(&hw_device_uart);
}

