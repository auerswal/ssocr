/* Seven Segment Optical Character Recognition Help Functions */

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

#ifndef SSOCR2_HELP_H
#define SSOCR2_HELP_H

/* functions */

/* print usage */
void usage(char *name, FILE *f);

/* print help for luminance functions */
void print_lum_help(void);

/* print version */
void print_version(FILE *f);

/* print luminance keyword */
void print_lum_key(luminance_t lt, FILE *f);

#endif /* SSOCR2_HELP_H */
