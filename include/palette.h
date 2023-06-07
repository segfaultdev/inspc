#ifndef __PALETTE_H__

// (Taken and modified from XRoar's source, copyright notice appended)

/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2011  Ciaran Anscomb
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdint.h>

typedef struct color_t color_t;
typedef struct palette_t palette_t;

struct color_t {
  float y, chb, b, a;
};

struct palette_t {
  float blank_y, white_y, black_level, rgb_black_level;
  color_t palette[12];
};

const extern palette_t palettes[];

void palette_to_rgb(const palette_t palette, int is_pal, int color, uint32_t *argb);

#endif
