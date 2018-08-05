/* Seven Segment Optical Character Recognition Character Set Functions */

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

/* Copyright (C) 2018 Erik Auerswald <auerswal@unix-ag.uni-kl.de> */

/* standard things */
#include <stdio.h>          /* puts, printf, BUFSIZ, perror, FILE */
#include <stdlib.h>         /* exit */

/* my headers */
#include "defines.h"        /* defines */
#include "help.h"           /* character set keyword functions */

/* print a digit according to charset, return 1 if unknown, else 0 */
int print_digit(int digit, charset_t charset, unsigned int flags)
{
  int unknown_digit = 0;

  if (charset != CS_FULL) {
    fprintf(stderr, "%s: error: charset %s is not implemented\n",
                    PROG, cs_key(charset));
    exit(99);
  }
  switch(digit) {
    case D_ZERO: putchar('0'); break;
    case D_ONE: putchar('1'); break;
    case D_TWO: putchar('2'); break;
    case D_THREE: putchar('3'); break;
    case D_FOUR: putchar('4'); break;
    case D_FIVE: putchar('5'); break;
    case D_SIX: putchar('6'); break;
    case D_SEVEN: /* fallthrough */
    case D_ALTSEVEN: putchar('7'); break;
    case D_EIGHT: putchar('8'); break;
    case D_NINE: /* fallthrough */
    case D_ALTNINE: putchar('9'); break;
    case D_DECIMAL: if(!(flags & OMIT_DECIMAL)) putchar('.'); break;
    case D_MINUS: putchar('-'); break;
    case D_HEX_A: putchar('a'); break;
    case D_HEX_b: putchar('b'); break;
    case D_HEX_C: /* fallthrough */
    case D_HEX_c: putchar('c'); break;
    case D_HEX_d: putchar('d'); break;
    case D_HEX_E: putchar('e'); break;
    case D_HEX_F: putchar('f'); break;
    case D_U: putchar('u'); break;
    case D_T: putchar('t'); break;
    case D_L: putchar('l'); break;
    case D_H: putchar('h'); break;
    case D_R: putchar('r'); break;
    case D_P: putchar('p'); break;
    case D_N: putchar('n'); break;
    /* finding a digit with no set segments is not supposed to happen */
    case D_UNKNOWN: putchar(' '); unknown_digit++; break;
    default: putchar('_'); unknown_digit++; break;
  }
  return unknown_digit;
}
