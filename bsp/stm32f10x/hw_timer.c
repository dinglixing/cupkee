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

#define HW_TIMER_NUM    4

typedef struct hw_timer_t {
    uint8_t inused;
    uint8_t ccr_x;
    uint16_t ccr_hi;
    uint16_t ccr_lo;
    int16_t object_id;
} hw_timer_t;

static hw_timer_t  device_controls[HW_TIMER_NUM];
static const uint32_t device_base[] = {TIM2, TIM3, TIM4, TIM5};
static const uint32_t device_rcc[] = {RCC_TIM2, RCC_TIM3, RCC_TIM4, RCC_TIM5};
static const uint32_t device_irq[] = {NVIC_TIM2_IRQ, NVIC_TIM3_IRQ, NVIC_TIM4_IRQ, NVIC_TIM5_IRQ};

static inline hw_timer_t *hw_device(unsigned inst) {
    if (inst < HW_TIMER_NUM) {
        return &device_controls[inst];
    } else {
        return NULL;
    }
}

static void hw_timer_setup(hw_timer_t *timer, uint32_t base, int us)
{
    uint16_t ccr_hi, ccr_lo;

    ccr_hi = us / 50000;
    ccr_lo = us % 50000;

    timer->ccr_hi = ccr_hi;
    timer->ccr_lo = ccr_lo;
    timer->ccr_x  = ccr_hi;
    TIM_ARR(base) = ccr_hi ? 50000 : ccr_lo;
}

int hw_timer_alloc(void)
{
    int inst;

    for (inst = 0; inst < HW_TIMER_NUM; inst++) {
        hw_timer_t *timer = hw_device(inst);

        if (!timer->inused) {
            timer->inused = 1;
            rcc_periph_clock_enable(device_rcc[inst]);

            return inst;
        }
    }

    return -1;
}

void hw_timer_release(int inst)
{
    hw_timer_t *timer = hw_device(inst);

    if (timer && timer->inused) {
        hw_timer_stop(inst);
        rcc_periph_clock_disable(device_rcc[inst]);
        timer->inused = 0;
    }
}

int hw_timer_start(int inst, int id, int us)
{
    hw_timer_t *timer = hw_device(inst);
    uint32_t base;

    if (!timer || !timer->inused) {
        return -CUPKEE_EINVAL;
    }

    if (us < 1) {
        us = 1;
    }

    timer->object_id = id;

    base = device_base[inst];
    TIM_SR(base) = 0;
    TIM_CNT(base) = 0;
    TIM_PSC(base) = 48; // 1 us
    TIM_DIER(base) = TIM_DIER_UIE;

    hw_timer_setup(timer, base, us);

    nvic_enable_irq(device_irq[inst]);
    TIM_CR1(base) = TIM_CR1_CEN;

    return 0;
}

int hw_timer_start_aux(uint16_t us)
{
    int inst = hw_timer_alloc();
    hw_timer_t *timer = hw_device(inst);
    uint32_t base;

    if (!timer) {
        return -CUPKEE_EINVAL;
    }

    if (us < 1) {
        us = 1;
    }

    timer->object_id = -2;
    timer->ccr_x  = 0;
    timer->ccr_hi = 0;
    timer->ccr_lo = us;

    base = device_base[inst];
    TIM_SR(base) = 0;
    TIM_CNT(base) = 0;
    TIM_PSC(base) = 48; // 1 us
    TIM_DIER(base) = TIM_DIER_UIE;
    TIM_ARR(base) = us - 1;

    nvic_enable_irq(device_irq[inst]);
    TIM_CR1(base) = TIM_CR1_CEN;

    return 0;
}

int hw_timer_stop(int inst)
{
    hw_timer_t *timer = hw_device(inst);
    uint32_t base;

    if (!timer || !timer->inused) {
        return -CUPKEE_EINVAL;
    }

    nvic_enable_irq(device_irq[inst]);

    base = device_base[inst];
    TIM_CR1(base) &= ~TIM_CR1_CEN;

    return 0;
}

int hw_timer_update(int inst, int us)
{
    hw_timer_t *timer = hw_device(inst);
    uint32_t base;

    if (!timer || !timer->inused) {
        return -CUPKEE_EINVAL;
    }
    base = device_base[inst];

    us = us < 1 ? 1 : us;
    hw_timer_setup(timer, base, us);

    return 0;
}

int hw_timer_duration_get(int inst)
{
    hw_timer_t *timer = hw_device(inst);
    uint32_t base;

    if (!timer || !timer->inused) {
        return -CUPKEE_EINVAL;
    }

    base = device_base[inst];
    return TIM_CNT(base) * 20;
}

void hw_setup_timer(void)
{
    int i;

    for (i = 0; i < HW_TIMER_NUM; i++) {
        device_controls[i].inused = 0;
        device_controls[i].object_id = -1;
    }
}

static inline void timer_isr(int x) {
    hw_timer_t *timer = hw_device(x);
    uint32_t    base = device_base[x];

    TIM_SR(base) &= ~TIM_SR_UIF;
    if (timer->object_id == -2) {
        _cupkee_auxticks += 1;
        cupkee_event_post_auxtick();
    } else
    if (timer->inused) {
        if (timer->ccr_x) {
            if (--timer->ccr_x == 0) {
                if (timer->ccr_lo) {
                    TIM_ARR(base) = timer->ccr_lo;
                    return;
                } else {
                    timer->ccr_x = timer->ccr_hi;
                }
            } else {
                return;
            }
        } else {
            if (timer->ccr_hi) {
                timer->ccr_x = timer->ccr_hi;
                TIM_ARR(base) = 50000;
            }
        }
        cupkee_timer_rewind(device_controls[x].object_id);
    }
}

void tim2_isr(void)
{
    timer_isr(0);
}

void tim3_isr(void)
{
    timer_isr(1);
}

void tim4_isr(void)
{
    timer_isr(2);
}

void tim5_isr(void)
{
    timer_isr(3);
}

