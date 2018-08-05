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

/* array for character set */
static char charset_array[CHARSET_MAX + 1];

/* initialize the character set array with the given character set */
void init_charset(charset_t cs)
{
  int i;

  for(i = 0; i < CHARSET_MAX + 1; i++) {
    charset_array[i] = '_';
  }
  switch(cs) {
    case CS_FULL:
      charset_array[D_UNKNOWN] = ' ';
      charset_array[D_ZERO] = '0';
      charset_array[D_ONE] = '1';
      charset_array[D_TWO] = '2';
      charset_array[D_THREE] = '3';
      charset_array[D_FOUR] = '4';
      charset_array[D_FIVE] = '5';
      charset_array[D_SIX] = '6';
      charset_array[D_SEVEN] = '7';
      charset_array[D_ALTSEVEN] = '7';
      charset_array[D_EIGHT] = '8';
      charset_array[D_NINE] = '9';
      charset_array[D_ALTNINE] = '9';
      charset_array[D_DECIMAL] = '.';
      charset_array[D_MINUS] = '-';
      charset_array[D_HEX_A] = 'a';
      charset_array[D_HEX_b] = 'b';
      charset_array[D_HEX_C] = 'c';
      charset_array[D_HEX_c] = 'c';
      charset_array[D_HEX_d] = 'd';
      charset_array[D_HEX_E] = 'e';
      charset_array[D_HEX_F] = 'f';
      charset_array[D_U] = 'u';
      charset_array[D_T] = 't';
      charset_array[D_L] = 'l';
      charset_array[D_H] = 'h';
      charset_array[D_R] = 'r';
      charset_array[D_P] = 'p';
      charset_array[D_N] = 'n';
      break;
    case CS_DIGITS:
      charset_array[D_UNKNOWN] = ' ';
      charset_array[D_ZERO] = '0';
      charset_array[D_ONE] = '1';
      charset_array[D_TWO] = '2';
      charset_array[D_THREE] = '3';
      charset_array[D_FOUR] = '4';
      charset_array[D_FIVE] = '5';
      charset_array[D_SIX] = '6';
      charset_array[D_HEX_b] = '6';
      charset_array[D_SEVEN] = '7';
      charset_array[D_ALTSEVEN] = '7';
      charset_array[D_EIGHT] = '8';
      charset_array[D_NINE] = '9';
      charset_array[D_ALTNINE] = '9';
      break;
    case CS_DECIMAL:
      charset_array[D_UNKNOWN] = ' ';
      charset_array[D_ZERO] = '0';
      charset_array[D_ONE] = '1';
      charset_array[D_TWO] = '2';
      charset_array[D_THREE] = '3';
      charset_array[D_FOUR] = '4';
      charset_array[D_FIVE] = '5';
      charset_array[D_SIX] = '6';
      charset_array[D_HEX_b] = '6';
      charset_array[D_SEVEN] = '7';
      charset_array[D_ALTSEVEN] = '7';
      charset_array[D_EIGHT] = '8';
      charset_array[D_NINE] = '9';
      charset_array[D_ALTNINE] = '9';
      charset_array[D_DECIMAL] = '.';
      charset_array[D_MINUS] = '-';
      break;
    case CS_HEXADECIMAL:
      charset_array[D_UNKNOWN] = ' ';
      charset_array[D_ZERO] = '0';
      charset_array[D_ONE] = '1';
      charset_array[D_TWO] = '2';
      charset_array[D_THREE] = '3';
      charset_array[D_FOUR] = '4';
      charset_array[D_FIVE] = '5';
      charset_array[D_SIX] = '6';
      charset_array[D_SEVEN] = '7';
      charset_array[D_ALTSEVEN] = '7';
      charset_array[D_EIGHT] = '8';
      charset_array[D_NINE] = '9';
      charset_array[D_ALTNINE] = '9';
      charset_array[D_DECIMAL] = '.';
      charset_array[D_MINUS] = '-';
      charset_array[D_HEX_A] = 'a';
      charset_array[D_HEX_b] = 'b';
      charset_array[D_HEX_C] = 'c';
      charset_array[D_HEX_c] = 'c';
      charset_array[D_HEX_d] = 'd';
      charset_array[D_HEX_E] = 'e';
      charset_array[D_HEX_F] = 'f';
      break;
    default:
      fprintf(stderr, "%s: error: charset %s is not implemented\n",
		      PROG, cs_key(cs));
      exit(99);
      break;
  }
}

/* print a digit according to charset, return 1 if unknown, else 0 */
int print_digit(int digit, unsigned int flags)
{
  int unknown_digit = 0;
  char c = '_';

  if(digit <= CHARSET_MAX) {
    c = charset_array[digit];
  }
  if(c == '_' || c == ' ') {
    unknown_digit = 1;
  }
  if(!((c == '.') && (flags & OMIT_DECIMAL))) {
    putchar(c);
  }

  return unknown_digit;
}
