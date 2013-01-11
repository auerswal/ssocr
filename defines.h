/* Seven Segment Optical Character Recognition Constant Definitions */

/*  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* Copyright (C) 2004-2013 Erik Auerswald <auerswal@unix-ag.uni-kl.de> */

#ifndef SSOCR2_DEFINES_H
#define SSOCR2_DEFINES_H

/* version number */
#define VERSION "2.13.3"

/* states */
#define FIND_DARK 0
#define FIND_LIGHT 1

/* boarder between dark and light */
#define THRESHOLD 50.0
#define DARK 0
#define LIGHT 1
#define UNKNOWN 2

/* segments
 *
 *  1     -
 * 2 3   | |
 *  4     -
 * 5 6   | |
 *  7     -
 *
 *  */
#define HORIZ_UP 1
#define VERT_LEFT_UP 2
#define VERT_RIGHT_UP 4
#define HORIZ_MID 8
#define VERT_LEFT_DOWN 16
#define VERT_RIGHT_DOWN 32
#define HORIZ_DOWN 64
#define ALL_SEGS 127
#define DECIMAL 128

/* digits */
#define D_ZERO (ALL_SEGS & ~HORIZ_MID)
#define D_ONE (VERT_RIGHT_UP | VERT_RIGHT_DOWN)
#define D_TWO (ALL_SEGS & ~(VERT_LEFT_UP | VERT_RIGHT_DOWN))
#define D_THREE (ALL_SEGS & ~(VERT_LEFT_UP | VERT_LEFT_DOWN))
#define D_FOUR (ALL_SEGS & ~(HORIZ_UP | VERT_LEFT_DOWN | HORIZ_DOWN))
#define D_FIVE (ALL_SEGS & ~(VERT_RIGHT_UP | VERT_LEFT_DOWN))
#define D_SIX (ALL_SEGS & ~VERT_RIGHT_UP)
#define D_SEVEN (HORIZ_UP | VERT_RIGHT_UP | VERT_RIGHT_DOWN)
#define D_ALTSEVEN (VERT_LEFT_UP | D_SEVEN)
#define D_EIGHT ALL_SEGS
#define D_NINE (ALL_SEGS & ~VERT_LEFT_DOWN)
#define D_DECIMAL DECIMAL
#define D_HEX_A (ALL_SEGS & ~HORIZ_DOWN)
#define D_HEX_b (ALL_SEGS & ~(HORIZ_UP | VERT_RIGHT_UP))
#define D_HEX_C (ALL_SEGS & ~(VERT_RIGHT_UP | HORIZ_MID | VERT_RIGHT_DOWN))
#define D_HEX_c (HORIZ_MID | VERT_LEFT_DOWN | HORIZ_DOWN)
#define D_HEX_d (ALL_SEGS & ~(HORIZ_UP | VERT_LEFT_UP))
#define D_HEX_E (ALL_SEGS & ~(VERT_RIGHT_UP | VERT_RIGHT_DOWN))
#define D_HEX_F (ALL_SEGS & ~(VERT_RIGHT_UP | VERT_RIGHT_DOWN | HORIZ_DOWN))
#define D_UNKNOWN 0

#define NUMBER_OF_DIGITS 6 /* in this special case */

/* a decimal point (or thousands separator) is recognized by height less
 * than <maximum digit height> / DEC_H_RATIO and width less than
 * <maximum digit width> / DEC_W_RATIO */
#define DEC_H_RATIO 7
#define DEC_W_RATIO 2 /* needs to work with just ones in the display, too */

/* a one is recognized by a height/width ratio > ONE_RATIO (as ints) */
#define ONE_RATIO 2

/* to find segment need # of pixels */
#define NEED_PIXELS 1

/* ignore # of pixels when checking a column fo black or white */
#define IGNORE_PIXELS 0

#define DIR_SEP "/"
#define TMP_FILE_DIR "/tmp"
#define TMP_FILE_PATTERN "ssocr.img.XXXXXX"
#define DEBUG_IMAGE_NAME "testbild.png"

/* flags set by options */
#define ABSOLUTE_THRESHOLD 1
#define DO_ITERATIVE_THRESHOLD (1<<1)
#define VERBOSE (1<<2)
#define USE_DEBUG_IMAGE (1<<3)
#define PROCESS_ONLY (1<<4)
#define PRINT_INFO (1<<5)
#define ADJUST_GRAY (1<<6)
#define DEBUG_OUTPUT (1<<7)

/* colors used by ssocr */
#define SSOCR_BLACK 0
#define SSOCR_WHITE 255

/* maximum RGB component value */
#define MAXRGB 255

/* doubles are assumed equal when they differ less than EPSILON */
#define EPSILON 0.0000001

/* default luminance formula */
#define DEFAULT_LUM_FORMULA REC709

/* foreground and background */
typedef enum fg_bg_e {
  FG,
  BG
} fg_bg_t;

/* luminance types resp. formulas */
typedef enum luminance_e {
  REC601,
  REC709,
  LINEAR,
  MINIMUM,
  MAXIMUM,
  RED,
  GREEN,
  BLUE
} luminance_t;

#endif /* SSOCR2_DEFINES_H */
