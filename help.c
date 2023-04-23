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

/* Copyright (C) 2004-2023 Erik Auerswald <auerswal@unix-ag.uni-kl.de> */
/* Copyright (C) 2013 Cristiano Fontana <fontanacl@ornl.gov> */

/* standard things */
#include <stdio.h>          /* puts, printf, BUFSIZ, perror, FILE */

/* my headers */
#include "defines.h"        /* defines */

/* global variables */
extern int ssocr_foreground;
extern int ssocr_background;

/* functions */

/* print luminance keyword */
void print_lum_key(luminance_t lt, FILE *f)
{
  switch(lt) {
    case REC601: fprintf(f, "Rec601"); break;
    case REC709: fprintf(f, "Rec709"); break;
    case LINEAR: fprintf(f, "linear"); break;
    case MINIMUM: fprintf(f, "minimum"); break;
    case MAXIMUM: fprintf(f, "maximum"); break;
    case RED: fprintf(f, "red"); break;
    case GREEN: fprintf(f, "green"); break;
    case BLUE: fprintf(f, "blue"); break;
    default: fprintf(f, "UNKNOWN"); break;
  }
}

/* print help for luminance functions */
void print_lum_help(void)
{
  puts("rec601    use gamma corrected RGB values according to ITU-R Rec. BT.601-4");
  puts("rec709    use linear RGB values according to ITU-R Rec. BT.709");
  puts("linear    use (R+G+B)/3 as done by cvtool 0.0.1");
  puts("minimum   use min(R,G,B) as done by GNU Ocrad 0.14");
  puts("maximum   use max(R,G,B)");
  puts("red       use R value");
  puts("green     use G value");
  puts("blue      use B value");
}

/* get keyword corresponding to character set define */
const char *cs_key(charset_t cs)
{
  switch(cs) {
    case CS_FULL: return "full";
    case CS_DIGITS: return "digits";
    case CS_DECIMAL: return "decimal";
    case CS_HEXADECIMAL: return "hex";
    case CS_TT_ROBOT: return "tt_robot";
    default: return "UNKNOWN";
  }
}

/* print character set keyword */
void print_cs_key(charset_t cs, FILE *f)
{
  fputs(cs_key(cs), f);
}

/* print help for character sets */
void print_cs_help(void)
{
  puts("full      all characters supported by ssocr");
  puts("digits    digits 0 to 9");
  puts("decimal   digits 0 to 9, decimal point, minus");
  puts("hex       digits 0 to f, decimal point, minus");
  puts("tt_robot  digits and characters used by a Chinese table tennis robot");
}

/* print version */
void print_version(FILE *f)
{
  fprintf(f, "Seven Segment Optical Character Recognition Version %s\n",
             VERSION);
  fprintf(f, "Copyright (C) 2004-2023 by Erik Auerswald"
             " <auerswal@unix-ag.uni-kl.de>\n");
  fprintf(f, "This program comes with ABSOLUTELY NO WARRANTY\n");
  fprintf(f, "This is free software, and you are welcome to redistribute it"
             " under the terms\nof the GNU GPL (version 3 or later)\n");
}

/* print short usage */
void short_usage(char *name, FILE *f)
{
  fprintf(f, "\nUsage: %s [OPTION]... [COMMAND]... IMAGE\n", name);
  fprintf(f, "Use '%s --help' to print long help describing all options\n",
          name);
}

/* print usage */
void usage(char *name, FILE *f)
{
  print_version(f);
  fprintf(f, "\nUsage: %s [OPTION]... [COMMAND]... IMAGE\n", name);
  fprintf(f, "\nOptions: -h, --help               print this message\n");
  fprintf(f, "         -v, --verbose            talk about program execution\n");
  fprintf(f, "         -V, --version            print version information\n");
  fprintf(f, "         -t, --threshold=THRESH   use THRESH (in percent) to distinguish black\n");
  fprintf(f, "                                  from white\n");
  fprintf(f, "         -a, --absolute-threshold don't adjust threshold to image\n");
  fprintf(f, "         -T, --iter-threshold     use iterative thresholding method\n");
  fprintf(f, "         -n, --number-pixels=#    number of pixels needed to recognize a segment\n");
  fprintf(f, "         -i, --ignore-pixels=#    number of pixels ignored when searching digit\n");
  fprintf(f, "                                  boundaries\n");
  fprintf(f, "         -d, --number-digits=#    number of digits in image (-1 for auto)\n");
  fprintf(f, "         -r, --one-ratio=#        height/width ratio to recognize a \'one\'\n");
  fprintf(f, "         -m, --minus-ratio=#      width/height ratio to recognize a minus sign\n");
  fprintf(f, "         -H, --dec-h-ratio=#      max_dig_h/h ratio to recognize decimal point\n");
  fprintf(f, "         -W, --dec-w-ratio=#      max_dig_w/w ratio to recognize decimal point\n");
  fprintf(f, "         -o, --output-image=FILE  write processed image to FILE\n");
  fprintf(f, "         -O, --output-format=FMT  use output format FMT (Imlib2 formats)\n");
  fprintf(f, "         -p, --process-only       do image processing only, no OCR\n");
  fprintf(f, "         -D, --debug-image[=FILE] write a debug image to FILE or %s\n",DEBUG_IMAGE_NAME);
  fprintf(f, "         -P, --debug-output       print debug information\n");
  fprintf(f, "         -f, --foreground=COLOR   set foreground color (black or white)\n");
  fprintf(f, "         -b, --background=COLOR   set background color (black or white)\n");
  fprintf(f, "         -I, --print-info         print image dimensions and used lum values\n");
  fprintf(f, "         -g, --adjust-gray        use T1 and T2 from gray_stretch as\n");
  fprintf(f, "                                  percentages of used values\n");
  fprintf(f, "         -l, --luminance=KEYWORD  compute luminance using formula KEYWORD\n");
  fprintf(f, "                                  use -l help for list of KEYWORDS\n");
  fprintf(f, "         -s, --print-spaces       print spaces between more distant digits\n");
  fprintf(f, "         -A, --space-factor=FAC   relative distance to add spaces\n");
  fprintf(f, "         -G, --space-average      use average instead of minimum distance\n");
  fprintf(f, "                                  to determine spaces between digits\n");
  fprintf(f, "         -S, --ascii-art-segments print recognized segments a ASCII art\n");
  fprintf(f, "         -X, --print-as-hex       change output format to hexadecimal\n");
  fprintf(f, "         -C, --omit-decimal-point omit decimal points from output\n");
  fprintf(f, "         -c, --charset=KEYWORD    select recognized characters\n");
  fprintf(f, "                                  use -c help for list of KEYWORDS\n");
  fprintf(f, "\nCommands: dilation [N]            [N times] dilation algorithm"
             "\n                                  (set_pixels_filter with mask"
             " of 1 pixel)\n");
  fprintf(f, "          erosion [N]             [N times] erosion algorithm\n"
             "                                  (set_pixels_filter with mask"
             " of 9 pixels)\n");
  fprintf(f, "          closing [N]             closing algorithm\n");
  fprintf(f, "                                  ([N times] dilation then [N times] erosion)\n");
  fprintf(f, "          opening [N]             opening algorithm\n");
  fprintf(f, "                                  ([N times] erosion then [N times] dilation)\n");
  fprintf(f, "          remove_isolated         remove isolated pixels\n");
  fprintf(f, "          make_mono               make image monochrome\n");
  fprintf(f, "          grayscale               transform image to grayscale\n");
  fprintf(f, "          invert                  make inverted monochrome image\n");
  fprintf(f, "          gray_stretch T1 T2      stretch luminance values from [T1,T2] to\n");
  fprintf(f, "                                  [0,255] (use --adjust-gray for percentages)\n");
  fprintf(f, "          dynamic_threshold W H   make image monochrome w. dynamic thresholding\n");
  fprintf(f, "                                  with a window of width W and height H\n");
  fprintf(f, "          rgb_threshold           make image monochrome by setting every pixel\n");
  fprintf(f, "                                  with any values of red, green or blue below\n");
  fprintf(f, "                                  the threshold to black\n");
  fprintf(f, "          r_threshold             make image monochrome using only red channel\n");
  fprintf(f, "          g_threshold             make image monochrome using only green channel\n");
  fprintf(f, "          b_threshold             make image monochrome using only blue channel\n");
  fprintf(f, "          white_border [WIDTH]    make border of WIDTH (or 1) in background\n");
  fprintf(f, "                                  color\n");
  fprintf(f, "          shear OFFSET            shear image OFFSET pixels (at bottom) to the\n");
  fprintf(f, "                                  right\n");
  fprintf(f, "          rotate THETA            rotate image by THETA degrees\n");
  fprintf(f, "          mirror {horiz|vert}     mirror image horizontally or vertically\n");
  fprintf(f, "          crop X Y W H            crop image with upper left corner (X,Y) with\n");
  fprintf(f, "                                  width W and height H\n");
  fprintf(f, "          set_pixels_filter MASK  set pixels that have at least MASK neighbor\n");
  fprintf(f, "                                  pixels set (including checked position)\n");
  fprintf(f, "          keep_pixels_filter MASK keeps pixels that have at least MASK neighbor\n");
  fprintf(f, "                                  pixels set (not counting the checked pixel)\n");
  fprintf(f, "\nDefaults: needed pixels          = %2d\n", NEED_PIXELS);
  fprintf(f, "          ignored pixels         = %2d\n", IGNORE_PIXELS);
  fprintf(f, "          no. of digits          = %2d\n", NUMBER_OF_DIGITS);
  fprintf(f, "          threshold              = %5.2f\n", THRESHOLD);
  fprintf(f, "          foreground             = %s\n",
      (SSOCR_DEFAULT_FOREGROUND == SSOCR_BLACK) ? "black" : "white");
  fprintf(f, "          background             = %s\n",
      (SSOCR_DEFAULT_BACKGROUND == SSOCR_BLACK) ? "black" : "white");
  fprintf(f, "          luminance              = ");
  print_lum_key(DEFAULT_LUM_FORMULA, f); fprintf(f, "\n");
  fprintf(f, "          height/width threshold for digit one  = %2d\n", ONE_RATIO);
  fprintf(f, "          width/height threshold for minus sign = %2d\n", MINUS_RATIO);
  fprintf(f, "          max_dig_h/h threshold for decimal sep = %2d\n", DEC_H_RATIO);
  fprintf(f, "          max_dig_w/w threshold for decimal sep = %2d\n", DEC_W_RATIO);
  fprintf(f, "          space width factor     = %.2f\n", SPC_FAC);
  fprintf(f, "          character set          = ");
  print_cs_key(DEFAULT_CHARSET, f); fputs("\n", f);
  fprintf(f, "\nOperation: The IMAGE is read, the COMMANDs are processed in the sequence\n");
  fprintf(f, "           they are given, in the resulting image the given number of digits\n");
  fprintf(f, "           are searched and recognized, after which the recognized number is\n");
  fprintf(f, "           written to STDOUT.\n");
  fprintf(f, "           The recognition algorithm works with set or unset pixels and uses\n");
  fprintf(f, "           the given THRESHOLD to decide if a pixel is set or not.\n");
  fprintf(f, "           Use - for IMAGE to read the image from STDIN.\n\n");
  fprintf(f, "Exit Codes:  0 if correct number of digits have been recognized\n");
  fprintf(f, "             1 if a different number of digits have been found\n");
  fprintf(f, "             2 if one of the digits could not be recognized\n");
  fprintf(f, "             3 if successful image processing only\n");
  fprintf(f, "            42 if -h, -V, -l help, or -c help\n");
  fprintf(f, "            99 otherwise\n");
}
