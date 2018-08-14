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

void  hw_setup_dac(void)
{
}

int   hw_dac_start(uint8_t bank, uint8_t port)
{
    (void) bank;
    (void) port;
    return -CUPKEE_EIMPLEMENT;
}

int   hw_dac_stop(uint8_t bank, uint8_t port)
{
    (void) bank;
    (void) port;
    return -CUPKEE_EIMPLEMENT;
}

int   hw_dac_set(uint8_t bank, uint8_t port, float v)
{
    (void) bank;
    (void) port;
    (void) v;
    return -CUPKEE_EIMPLEMENT;
}

