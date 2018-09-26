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

#include "cupkee.h"

#define PIN_INVALID    0xFF
#define GROUP_DEF_SIZE 32

#define BANK_OF(pin)  (pin_control.pin_map[pin].bank)
#define PORT_OF(pin)  (pin_control.pin_map[pin].port)

typedef struct pin_group_t {
    uint8_t max;
    uint8_t num;
    uint8_t *pin;
} pin_group_t;

typedef struct pin_data_t {
    list_head_t link;

    uint8_t events;

    /* wave generation */
    uint32_t wave_period[2];
    uint32_t wave_ticks[2];
    uint32_t wave_last_ticks;
    uint32_t wave_edges;
    cupkee_wave_engine_t wave_engine;
    void    *wave_engine_data;

    /* event callback */
    void *entry;
    cupkee_callback_t handler;

    /* hardware data */
    hw_pindata_t bsp_data;
} pin_data_t;

typedef struct pin_control_t {
    uint8_t pin_num;

    const cupkee_pinmap_t *pin_map;
    list_head_t *wake_pool;
    pin_data_t **pin_data;
} pin_control_t;

static pin_control_t pin_control;

static inline int pin_is_valid(uint8_t pin) {
    return pin < pin_control.pin_num;
}

static inline pin_data_t *pin_peek_data(int pin)
{
    return pin_control.pin_data[pin];
}

static pin_data_t *pin_get_data(int pin)
{
    pin_data_t *pdata = pin_control.pin_data[pin];

    if (!pdata) {
        pdata = cupkee_malloc(sizeof(pin_data_t));
        if (pdata) {
            memset(pdata, 0, sizeof(pin_data_t));

            list_head_init(&pdata->link);
            pdata->bsp_data.id = pin;

            pin_control.pin_data[pin] = pdata;
        }
    }

    return pdata;
}

static int pin_event_handle_set(int pin, cupkee_callback_t handler, void *entry)
{
    pin_data_t *pdata = pin_get_data(pin);

    if (!pdata) {
        return -CUPKEE_ERESOURCE;
    }

    if (pdata->handler) {
        return -CUPKEE_EBUSY;
    }

    pdata->entry   = entry;
    pdata->handler = handler;

    return 0;
}

static void pin_event_handle_clear(int pin)
{
    pin_data_t *pdata = pin_control.pin_data[pin];

    if (pdata && pdata->handler) {
        void *entry = pdata->entry;
        cupkee_callback_t callback = pdata->handler;

        pdata->entry = NULL;
        pdata->handler = NULL;

        callback(entry, CUPKEE_EVENT_PIN_IGNORE, pin);
    }
}

static int pin_wave_start(
    int pin, pin_data_t *pdata,
    uint32_t first, uint32_t second,
    cupkee_wave_engine_t engine, void *data
) {
    uint32_t curr_ticks;
    uint8_t index;

    (void) pin;

    curr_ticks = _cupkee_auxticks;
    if (engine) {
        if (0 == (first = engine(0, first, data))) {
            return 0;
        }
    }
    pdata->wave_period[0] = first;
    pdata->wave_period[1] = second;
    pdata->wave_ticks[0] = cupkee_auxticks_of(first);
    pdata->wave_ticks[1] = cupkee_auxticks_of(second);
    pdata->wave_engine = engine;
    pdata->wave_engine_data = data;

    pdata->wave_edges = 0;
    pdata->wave_last_ticks = curr_ticks;

    index = (curr_ticks + pdata->wave_ticks[0]) & CUPKEE_PIN_SCHEDULE_MASK;
    list_add_tail(&pdata->link, &pin_control.wake_pool[index]);

    return 0;
}

int cupkee_pin_setup(void)
{
    pin_control.pin_num = 0;
    pin_control.pin_map = NULL;
    pin_control.pin_data = NULL;

    return 0;
}

void cupkee_pin_event_dispatch(uint16_t id, uint8_t code)
{
    if (id < pin_control.pin_num) {
        pin_data_t *pdata = pin_control.pin_data[id];

        if (pdata && pdata->handler) {
            if (code > 0 && (pdata->events & CUPKEE_EVENT_PIN_RISING)) {
                pdata->handler(pdata->entry, CUPKEE_EVENT_PIN_RISING, id);
            } else
            if (code == 0 && (pdata->events & CUPKEE_EVENT_PIN_FALLING)) {
                pdata->handler(pdata->entry, CUPKEE_EVENT_PIN_FALLING, id);
            }
        }
    }
}

void cupkee_pin_schedule(uint32_t curr_ticks)
{
    if (pin_control.wake_pool) {
        uint8_t index = curr_ticks & CUPKEE_PIN_SCHEDULE_MASK;
        list_head_t *curr, *next, *head = &pin_control.wake_pool[index];

        list_for_each_safe(curr, next, head) {
            pin_data_t *pdata = CUPKEE_CONTAINER_OF(curr, pin_data_t, link);
            uint8_t pin = pdata->bsp_data.id;

            index = pdata->wave_edges & 1;
            if (curr_ticks - pdata->wave_last_ticks >= pdata->wave_ticks[index]) {
                uint32_t next_ticks;

                hw_gpio_toggle(BANK_OF(pin), PORT_OF(pin));

                list_del(&pdata->link);
                pdata->wave_edges += 1;

                index ^= 1;
                if (pdata->wave_engine) {
                    uint32_t next_us = pdata->wave_period[index];
                    uint32_t us = pdata->wave_engine(pdata->wave_edges, next_us,
                                                    pdata->wave_engine_data);
                    if (us != next_us) {
                        pdata->wave_period[index] = us;
                        pdata->wave_ticks[index] = cupkee_auxticks_of(us);
                    }
                }
                next_ticks = pdata->wave_ticks[index];

                if (next_ticks) {
                    index = (curr_ticks + next_ticks) & CUPKEE_PIN_SCHEDULE_MASK;
                    pdata->wave_last_ticks = curr_ticks;
                    list_add_tail(&pdata->link, &pin_control.wake_pool[index]);
                } else {
                    pdata->wave_engine = NULL;
                    list_head_init(&pdata->link);
                }
            }
        }
    }
}

int cupkee_pin_map(uint8_t num, const cupkee_pinmap_t *map)
{
    void *buf;
    size_t size;
    int i;

    if (num < 1 || !map) {
        return -CUPKEE_EINVAL;
    }

    if (pin_control.pin_num > 0) {
        return -CUPKEE_EBUSY;
    }

    size = sizeof(pin_data_t *) * num + sizeof(list_head_t) * CUPKEE_PIN_SCHEDULE_POOL;
    if (NULL == (buf = cupkee_malloc(size))) {
        return -CUPKEE_ERESOURCE;
    }
    memset(buf, 0, size);

    pin_control.pin_num = num;
    pin_control.pin_map = map;
    pin_control.wake_pool = buf;
    pin_control.pin_data = (pin_data_t **)(((intptr_t)buf) + (sizeof(list_head_t) * CUPKEE_PIN_SCHEDULE_POOL));

    for (i = 0; i < CUPKEE_PIN_SCHEDULE_POOL; ++i) {
        list_head_init(&pin_control.wake_pool[i]);
    }

    return 0;
}

int cupkee_pin_mode_get(int pin)
{
    if (pin_is_valid(pin)) {
        return hw_gpio_mode_get(BANK_OF(pin), PORT_OF(pin));
    } else {
        return -CUPKEE_EINVAL;
    }
}

int cupkee_pin_mode_set(int pin, int mode)
{
    if (pin_is_valid(pin)) {
        if (mode == CUPKEE_PIN_MODE_AIN) {
            return hw_adc_start(BANK_OF(pin), PORT_OF(pin));
        } else
        if (mode == CUPKEE_PIN_MODE_AOUT) {
            return hw_dac_start(BANK_OF(pin), PORT_OF(pin));
        } else {
            return hw_gpio_mode_set(BANK_OF(pin), PORT_OF(pin), mode);
        }
    } else {
        return -CUPKEE_EINVAL;
    }
}

int cupkee_pin_get(int pin)
{
    if (pin_is_valid(pin)) {
        return hw_gpio_get(BANK_OF(pin), PORT_OF(pin));
    } else {
        return -CUPKEE_EINVAL;
    }
}

int cupkee_pin_set(int pin, int v)
{
    if (pin_is_valid(pin)) {
        return hw_gpio_set(BANK_OF(pin), PORT_OF(pin), v);
    } else {
        return -CUPKEE_EINVAL;
    }
}

int cupkee_pin_get_analog(int pin, float *v)
{
    if (pin_is_valid(pin)) {
        return hw_adc_get(BANK_OF(pin), PORT_OF(pin), v);
    } else {
        return -CUPKEE_EINVAL;
    }
}

int cupkee_pin_set_analog(int pin, float v)
{
    if (pin_is_valid(pin)) {
        return hw_dac_set(BANK_OF(pin), PORT_OF(pin), v);
    } else {
        return -CUPKEE_EINVAL;
    }
}

int cupkee_pin_toggle(int pin)
{
    if (pin_is_valid(pin)) {
        return hw_gpio_toggle(BANK_OF(pin), PORT_OF(pin));
    } else {
        return 0;
    }
}

uint32_t cupkee_pin_duration(int pin)
{
    if (pin_is_valid(pin)) {
        pin_data_t *pdata = pin_control.pin_data[pin];
        if (pdata) {
            return pdata->bsp_data.duration << CUPKEE_AUXTICKS_SHIFT;
        }
    }
    return 0;
}

int cupkee_pin_wave_get(int pin, int i)
{
    pin_data_t *pdata;

    if (!pin_is_valid(pin)) {
        return -CUPKEE_EINVAL;
    }

    pdata = pin_get_data(pin);
    if (!pdata || list_is_empty(&pdata->link)) {
        return 0;
    }

    return pdata->wave_period[i & 1];
}

int cupkee_pin_wave_gen(
    int pin,
    uint32_t first, uint32_t second,
    cupkee_wave_engine_t engine, void *data
) {
    pin_data_t *pdata;

    if (!pin_is_valid(pin) || (engine == NULL && first == 0)) {
        return -CUPKEE_EINVAL;
    }

    pdata = pin_get_data(pin);
    if (!pdata) {
        return -CUPKEE_ERESOURCE;
    }

    if (!list_is_empty(&pdata->link)) {
        return -CUPKEE_EBUSY;
    }

    return pin_wave_start(pin, pdata, first, second, engine, data);
}

int cupkee_pin_wave_update(int pin, uint32_t first)
{
    pin_data_t *pdata;

    if (!pin_is_valid(pin) || first == 0) {
        return -CUPKEE_EINVAL;
    }

    pdata = pin_get_data(pin);
    if (!pdata) {
        return -CUPKEE_ERESOURCE;
    }

    if (list_is_empty(&pdata->link)) {
        return pin_wave_start(pin, pdata, first, first, NULL, NULL);
    } else {
        uint32_t period = pdata->wave_period[0] + pdata->wave_period[1];

        if (period < first) {
            return -CUPKEE_EINVAL;
        } else {
            uint32_t second = period - first;
            pdata->wave_period[0] = first;
            pdata->wave_ticks[0] = cupkee_auxticks_of(first);
            pdata->wave_period[1] = second;
            pdata->wave_ticks[1] = cupkee_auxticks_of(second);

            return 0;
        }
    }
}

int cupkee_pin_wave_stop(int pin, int v)
{
    pin_data_t *pdata;

    if (!pin_is_valid(pin)) {
        return -CUPKEE_EINVAL;
    }

    pdata = pin_peek_data(pin);
    if (!pdata || list_is_empty(&pdata->link)) {
        hw_gpio_set(BANK_OF(pin), PORT_OF(pin), v);
        return 0;
    }

    if (pdata->wave_engine) {
        pdata->wave_engine(pdata->wave_edges, 0, pdata->wave_engine_data);
        pdata->wave_engine = NULL;
        pdata->wave_engine_data = NULL;
    }
    list_del(&pdata->link);
    list_head_init(&pdata->link);

    hw_gpio_set(BANK_OF(pin), PORT_OF(pin), v);

    return 0;
}

int cupkee_pin_wave_set(int pin, int start, uint32_t first, uint32_t second)
{
    pin_data_t *pdata;

    if (first == 0) {
        return cupkee_pin_wave_stop(pin, !start);
    } else
    if (second == 0) {
        return cupkee_pin_wave_stop(pin, start);
    }

    if (!pin_is_valid(pin)) {
        return -CUPKEE_EINVAL;
    }

    pdata = pin_get_data(pin);
    if (!pdata) {
        return -CUPKEE_ERESOURCE;
    }

    if (list_is_empty(&pdata->link)) {
        cupkee_pin_set(pin, start);
        return pin_wave_start(pin, pdata, first, second, NULL, NULL);
    } else {
        pdata->wave_period[0] = first;
        pdata->wave_ticks[0] = cupkee_auxticks_of(first);
        pdata->wave_period[1] = second;
        pdata->wave_ticks[1] = cupkee_auxticks_of(second);

        return 0;
    }
}

int cupkee_pin_listen(int pin, int events, cupkee_callback_t handler, void *entry)
{
    events &= CUPKEE_EVENT_PIN_RISING | CUPKEE_EVENT_PIN_FALLING;

    if (events && pin_is_valid(pin)) {
        pin_data_t *pdata;
        int err;

        err = pin_event_handle_set(pin, handler, entry);
        if (err) {
            return err;
        }
        pdata = pin_control.pin_data[pin];

        err = hw_gpio_listen(BANK_OF(pin), PORT_OF(pin), &pdata->bsp_data);
        if (err) {
            pin_event_handle_clear(pin);
        }
        pdata->events = events;
        return 0;
    } else {
        return -CUPKEE_EINVAL;
    }
}

int cupkee_pin_ignore(int pin)
{
    if (pin_is_valid(pin)) {
        pin_event_handle_clear(pin);
        return hw_gpio_ignore(BANK_OF(pin), PORT_OF(pin));
    } else {
        return -CUPKEE_EINVAL;
    }
}

