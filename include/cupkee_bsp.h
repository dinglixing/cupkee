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

#ifndef __CUPKEE_BSP_INC__
#define __CUPKEE_BSP_INC__

#include <stdint.h>

/****************************************************************/
/* hardware configure                                           */
/****************************************************************/
#define SYSTEM_TICKS_PRE_SEC                1000
#define SYSTEM_STACK_SIZE                   (8 * 1024)

/****************************************************************/
/* system define                                                */
/****************************************************************/
#define HW_STORAGE_BANK_BIN                 0
#define HW_STORAGE_BANK_BIN2                1
#define HW_STORAGE_BANK_APP                 2
#define HW_STORAGE_BANK_CFG                 3

#define HW_RESET_DEBUG                      1 // JTAG Enabled
#define HW_RESET_SAFE                       2 // JTAG Disabled
#define HW_RESET_UPGRADE                    4 // JTAG Disabled

enum HW_DIR {
    HW_DIR_IN,
    HW_DIR_OUT,
    HW_DIR_DUPLEX
};

typedef struct hw_info_t {
    uint32_t ram_sz;
    uint32_t rom_sz;
    void    *ram_base;
    void    *rom_base;
    uint32_t sys_freq;
} hw_info_t;

typedef struct hw_pindata_t {
    uint8_t id;  // used by bsp
    uint8_t data8;  // used by bsp
    uint16_t data16;   // used by bsp
    uint32_t data32;   // used by bsp

    uint32_t duration; // keep duration, before falling or rising edge
} hw_pindata_t;

/****************************************************************/
/* hardware interface to implement                              */
/****************************************************************/
void hw_setup_loader(hw_info_t *info);
void hw_setup(hw_info_t *info);
void hw_reset(int flags);

void hw_poll(void);
void hw_halt(void);
uint8_t hw_reset_flags(void);
int  hw_boot_state(void);

void hw_enter_critical(uint32_t *state);
void hw_exit_critical(uint32_t state);

void hw_cuid_get(uint8_t *cuid);
void hw_info_get(hw_info_t *);

/* MEMORY */
void  *hw_memory_alloc(size_t size, size_t align);
size_t hw_memory_size(void);

/* STORAGE */
intptr_t hw_storage_base(void);
int hw_storage_erase(uint32_t base, uint32_t size);
int hw_storage_program(uint32_t base, uint32_t len, const uint8_t *data);

/* GPIO */
int hw_gpio_enable(uint8_t bank, uint8_t port, uint8_t dir);
int hw_gpio_disable(uint8_t bank, uint8_t port);
int hw_gpio_listen(uint8_t bank, uint8_t port, hw_pindata_t *data);
int hw_gpio_ignore(uint8_t bank, uint8_t port);
int hw_gpio_get(uint8_t bank, uint8_t port);
int hw_gpio_set(uint8_t bank, uint8_t port, int v);
int hw_gpio_toggle(uint8_t bank, uint8_t port);

/* TIMER */
int  hw_timer_alloc(void);
void hw_timer_release(int inst);

int hw_timer_start(int inst, int id, int us);
int hw_timer_stop(int inst);
int hw_timer_update(int inst, int us);
int hw_timer_duration_get(int inst);
int hw_timer_start_aux(uint16_t us);

/* DEVICE */
int hw_device_setup(void);
int hw_device_setup_noirq(void);

#endif /* __CUPKEE_BSP_INC__ */

