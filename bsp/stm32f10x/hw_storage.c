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

void hw_setup_storage(void)
{
}

static inline uint32_t hw_storage_size(void)
{
    return desig_get_flash_size() * 1024;
}

intptr_t hw_storage_base(void)
{
    return FLASH_BASE;
}

int hw_storage_erase(uint32_t base, uint32_t size)
{
    uint32_t flash_size = hw_storage_size();
    uint32_t flash_sector_size = (flash_size) >= 256 * 1024 ? (1 << 11) : (1 << 10);
    uint32_t flash_sector_mask = flash_sector_size - 1;

    uint32_t end = base + size;

    if ((base & flash_sector_mask) || base < FLASH_BASE ||
        (size & flash_sector_mask) || end > FLASH_BASE + flash_size) {
        return -CUPKEE_EINVAL;
    }

    flash_unlock();
    while (base < end) {
        flash_erase_page(base);
        if (flash_get_status_flags() != FLASH_SR_EOP) {
            return -1;
        }
        base += flash_sector_size;
    }
    flash_lock();

    return 0;
}

int hw_storage_program(uint32_t base, uint32_t size, const uint8_t *data)
{
    uint32_t flash_tail = FLASH_BASE + hw_storage_size();
    uint32_t padding = base % sizeof(uint32_t);
    uint32_t pos = 0;

    if (base < FLASH_BASE || base + size > flash_tail) {
        return -CUPKEE_EINVAL;
    }

    flash_unlock();

    if (padding) {
        uint32_t val = *((uint32_t *) (base - padding));
        uint8_t *ptr = (uint8_t *)&val;

        base -= padding;
        while (padding < sizeof(uint32_t)) {
            ptr[padding++] = data[pos++];
        }

        flash_program_word(base, val);
        base += sizeof(uint32_t);
    }

    while (pos + sizeof(uint32_t) < size) {
        uint32_t val;
        uint8_t *ptr = (uint8_t *)&val;

        ptr[0] = data[pos++];
        ptr[1] = data[pos++];
        ptr[2] = data[pos++];
        ptr[3] = data[pos++];
        flash_program_word(base, val);

        base += sizeof(uint32_t);
    }

    if (pos < size) {
        uint32_t val = 0xFFFFFFFF, i = 0;
        uint8_t *ptr = (uint8_t *)&val;
        while (pos < size) {
            ptr[i++] = data[pos++];
        }
        flash_program_word(base, val);
    }

    flash_lock();

    return size;
}

