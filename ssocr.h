/* Seven Segment Optical Character Recognition */

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

/* Copyright (C) 2004-2008 Erik Auerswald <auerswal@unix-ag.uni-kl.de> */

#ifndef SSOCR2_H
#define SSOCR2_H

/* defines */

#define VERSION "2.7.0"

/* colours used by ssocr */
#define SSOCR_BLACK 0
#define SSOCR_WHITE 255

/* states */
#define FIND_DARK 0
#define FIND_LIGHT 1

/* boarder between dark and light */
#define MAXRGB 255 /* maximum RGB component value */
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

/* digits */
#define D_ZERO (ALL_SEGS & ~HORIZ_MID)
#define D_ONE (VERT_RIGHT_UP | VERT_RIGHT_DOWN)
#define D_TWO (ALL_SEGS & ~(VERT_LEFT_UP | VERT_RIGHT_DOWN))
#define D_THREE (ALL_SEGS & ~(VERT_LEFT_UP | VERT_LEFT_DOWN))
#define D_FOUR (ALL_SEGS & ~(HORIZ_UP | VERT_LEFT_DOWN | HORIZ_DOWN))
#define D_FIVE (ALL_SEGS & ~(VERT_RIGHT_UP | VERT_LEFT_DOWN))
#define D_SIX (ALL_SEGS & ~VERT_RIGHT_UP)
#define D_SEVEN (HORIZ_UP | VERT_RIGHT_UP | VERT_RIGHT_DOWN)
#define D_EIGHT ALL_SEGS
#define D_NINE (ALL_SEGS & ~VERT_LEFT_DOWN)
#define D_UNKNOWN 0

#define NUMBER_OF_DIGITS 6 /* in this special case */
#define SPACES 1 /* for segment_display string */

/* to find segment need # of pixels */
#define NEED_PIXELS 1

/* ignore # of pixels when checking a column fo black or white */
#define IGNORE_PIXELS 0

#define DEBUG_IMAGE_NAME "testbild.png"

/* doubles are assumed equal when they differ less than EPSILON */
#define EPSILON 0.0000001

/* types */

typedef struct {
  int x1,y1,x2,y2,digit;
} digit_struct;

typedef enum channel_e {
  CHAN_ALL,
  CHAN_RED,
  CHAN_GREEN,
  CHAN_BLUE
} channel_t;

typedef enum fg_bg_e {
  FG,
  BG
} fg_bg_t;

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

#define DEFAULT_LUM_FORMULA REC709 /* default luminance formula */

/* functions */

/* print usage */
void usage(char *name, FILE *f);

/* print help for luminance functions */
void print_lum_help(void);

/* print version */
void print_version(FILE *f);

/* parse luminance keyword */
luminance_t parse_lum(char *keyword);

/* print luminance keyword */
void print_lum_key(luminance_t lt, FILE *f);

/* set foreground color */
void set_fg_color(int color);

/* set background color */
void set_bg_color(int color);

/* set imlib color */
void ssocr_set_imlib_color(fg_bg_t color);

/* check if a pixel is set regarding current foreground/background colors */
int is_pixel_set(int value, double threshold);

/* set pixel if at least mask neighboring pixels (including the pixel to be set)
 * are set
 * a pixel is set if its luminance value is less than thresh
 * mask=1 is the standard dilation filter
 * mask=9 is the standard erosion filter */
Imlib_Image set_pixels_filter(Imlib_Image *source_image, double thresh,
                              luminance_t lt, int mask);

/* shortcut for dilation */
Imlib_Image dilation(Imlib_Image *source_image, double thresh, luminance_t lt);

/* shortcut for erosion */
Imlib_Image erosion(Imlib_Image *source_image, double thresh, luminance_t lt);

/* shortcut for closing */
Imlib_Image closing(Imlib_Image *source_image, double thresh, luminance_t lt,
                    int n);

/* shortcut for opening */
Imlib_Image opening(Imlib_Image *source_image, double thresh, luminance_t lt,
                    int n);

/* keep only pixels that have at least mask-1 neighbors set */
Imlib_Image keep_pixels_filter(Imlib_Image *source_image, double thresh,
                               luminance_t lt, int mask);

/* remove isolated pixels (shortcut for keep_pixels_filter with mask = 2) */
Imlib_Image remove_isolated(Imlib_Image *source_image, double thresh,
                            luminance_t lt);

/* grey stretching, i.e. lum<t1 => lum=0, lum>t2 => lum=100,
 * else lum=((lum-t1)*MAXRGB)/(t2-t1) */
Imlib_Image grey_stretch(Imlib_Image *source_image, double t1, double t2,
                         luminance_t lt);

/* use dynamic (aka adaptive) thresholding to create monochrome image */
Imlib_Image dynamic_threshold(Imlib_Image *source_image, double t,
                              luminance_t lt ,int ww, int wh);

/* make black and white */
Imlib_Image make_mono(Imlib_Image *source_image, double thresh, luminance_t lt);

/* set pixel to black (0,0,0) if R<T or G<T or R<T, T=thresh/100*255 */
Imlib_Image rgb_threshold(Imlib_Image *source_image, double thresh,
                          channel_t channel);

/* set pixel to black (0,0,0) if R<T, T=thresh/100*255 */
Imlib_Image r_threshold(Imlib_Image *source_image, double thresh);

/* set pixel to black (0,0,0) if G<T, T=thresh/100*255 */
Imlib_Image g_threshold(Imlib_Image *source_image, double thresh);

/* set pixel to black (0,0,0) if B<T, T=thresh/100*255 */
Imlib_Image b_threshold(Imlib_Image *source_image, double thresh);

/* make the border white */
Imlib_Image white_border(Imlib_Image *source_image, int width);

/* invert image */
Imlib_Image invert(Imlib_Image *source_image, double thresh, luminance_t lt);

/* shear the image
 * the top line is unchanged
 * the bottom line is moved offset pixels to the right
 * the other lines are moved yPos*offset/(height-1) pixels to the right
 * white pixels are inserted at the left side */
Imlib_Image shear(Imlib_Image *source_image, int offset);

/* rotate the image */
Imlib_Image rotate(Imlib_Image *source_image, double theta);

/* turn image to greyscale */
Imlib_Image greyscale(Imlib_Image *source_image, luminance_t lt);

/* crop image */
Imlib_Image crop(Imlib_Image *source_image, int x, int y, int w, int h);

/* adapt threshold to image values values */
double adapt_threshold(Imlib_Image *image, double thresh, luminance_t lt, int x,
                       int y, int w, int h, int absolute_threshold,
                       int iterative_threshold, int verbose, int debug_output);

/* compute dynamic threshold value from the rectangle (x,y),(x+w,y+h) of
 * source_image */
double get_threshold(Imlib_Image *source_image, double fraction, luminance_t lt,
                     int x, int y, int w, int h);

/* determine threshold by an iterative method */
double iterative_threshold(Imlib_Image *source_image, double thresh,
                           luminance_t lt, int x, int y, int w, int h);

/* get minimum grey value */
double get_minval(Imlib_Image *source_image, int x, int y, int w, int h,
                 luminance_t lt);

/* get maximum grey value */
double get_maxval(Imlib_Image *source_image, int x, int y, int w, int h,
                 luminance_t lt);

/* compute luminance from RGB values */
int get_lum(Imlib_Color *color, luminance_t lt);

/* compute luminance Y_709 from linear RGB values */
int get_lum_709(Imlib_Color *color);

/* compute luminance Y_601 from gamma corrected (non-linear) RGB values */
int get_lum_601(Imlib_Color *color);

/* compute luminance Y = (R+G+B)/3 */
int get_lum_lin(Imlib_Color *color);

/* compute luminance Y = min(R,G,B) as used in GNU Ocrad 0.14 */
int get_lum_min(Imlib_Color *color);

/* compute luminance Y = max(R,G,B) */
int get_lum_max(Imlib_Color *color);

/* compute luminance Y = R */
int get_lum_red(Imlib_Color *color);

/* compute luminance Y = G */
int get_lum_green(Imlib_Color *color);

/* compute luminance Y = B */
int get_lum_blue(Imlib_Color *color);

/* clip value thus that it is in the given interval [min,max] */
int clip(int value, int min, int max);

/* print image info */
void print_info(Imlib_Image *source_image);

#endif /* SSOCR2_H */
