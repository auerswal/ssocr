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

/* Copyright (C) 2004-2025 Erik Auerswald <auerswal@unix-ag.uni-kl.de> */
/* Copyright (C) 2013 Cristiano Fontana <fontanacl@ornl.gov> */

/* ImLib2 Header */
#include <X11/Xlib.h>       /* needed by Imlib2.h */
#include <Imlib2.h>

/* standard things */
#include <limits.h>         /* INT_MAX */
#include <stdint.h>         /* SIZE_MAX */
#include <stdio.h>          /* puts, printf, BUFSIZ, perror, FILE */
#include <stdlib.h>         /* exit */

/* string manipulation */
#include <string.h>         /* memcpy, strchr, strdup, strlen */

/* option parsing */
#include <getopt.h>         /* getopt */
#include <unistd.h>         /* getopt, read, write, STDIN_FILENO */

/* file permissions */
#include <sys/stat.h>       /* umask */

/* my headers */
#include "defines.h"        /* defines */
#include "ssocr.h"          /* types */
#include "imgproc.h"        /* image processing */
#include "help.h"           /* online help */
#include "charset.h"        /* character set selection and printing */

/* global variables */
int ssocr_foreground = SSOCR_DEFAULT_FOREGROUND;
int ssocr_background = SSOCR_DEFAULT_BACKGROUND;

/* functions */

/* copy image from stdin to a temporary file and return the filename */
static char * tmp_imgfile(unsigned int flags)
{
  char *dir;
  char *name;
  size_t pattern_len;
  int handle;
  unsigned char buf[BUFSIZ];
  ssize_t read_count = 0, write_count = 0;
  size_t pat_suffix_len = strlen(DIR_SEP TMP_FILE_PATTERN);
  size_t dir_len;

  /* find a suitable place (directory) for the tmp file and create pattern */
  dir = getenv("TMP");
  if(dir && strlen(dir) >= SIZE_MAX - pat_suffix_len - 1) {
    dir = TMP_FILE_DIR;
    fprintf(stderr, "%s: warning: ignoring too long $TMP env variable\n",
                    PROG);
  } else if(!dir) {
    dir = TMP_FILE_DIR;
  }
  if(flags & DEBUG_OUTPUT) {
    fprintf(stderr, "using directory %s for temporary files\n", dir);
  }
  dir_len = strlen(dir);
  pattern_len = dir_len + pat_suffix_len + 1;
  if(pattern_len <= dir_len) {
    fputs(PROG ": error: temporary file name length overflow\n", stderr);
    exit(99);
  }
  name = calloc(pattern_len, sizeof(char));
  if(!name) {
    perror(PROG ": could not allocate memory for name of temporary file");
    exit(99);
  }
  memcpy(name, dir, dir_len);
  memcpy(name + dir_len, DIR_SEP TMP_FILE_PATTERN, pat_suffix_len);
  if(flags & DEBUG_OUTPUT)
    fprintf(stderr, "pattern for temporary file is %s\n", name);

  /* create temporary file */
  umask(S_IRWXG | S_IRWXO);
  handle = mkstemp(name);
  if(handle < 0) {
    perror(PROG ": could not create temporary file");
    exit(99);
  }

  /* copy image data from stdin to tmp file */
  while((read_count = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
    write_count = write(handle, buf, read_count);
    if (write_count <= 0) break;
  }
  close(handle); /* filehandle is no longer needed, Imlib2 uses filename */
  if(read_count < 0) {
    perror(PROG ": could not read image from standard input");
    exit(99);
  }
  if(write_count <= 0) {
    perror(PROG ": could not copy image data to temporary file");
    unlink(name);
    exit(99);
  }
  
  return name;
}

/* return number of foreground pixels in a scanline */
static unsigned int scanline(Imlib_Image *debug_image,
                             int x, int y, int len, direction_t dir,
                             color_struct d_color, double thresh,
                             luminance_t lt, unsigned int flags)
{
  Imlib_Color imlib_color, debug_color;
  int lum, i, ix=x, iy=y, start, end;
  unsigned int found_pixels = 0;
  start = (dir == HORIZONTAL) ? x : y;
  end = start + len;
  debug_color.red = d_color.R;
  debug_color.green = d_color.G;
  debug_color.blue = d_color.B;
  debug_color.alpha = d_color.A;
  for (i = start; i <= end; i++) {
    if (dir == HORIZONTAL) ix = i;
    else iy = i;
    imlib_image_query_pixel(ix, iy, &imlib_color);
    lum = get_lum(&imlib_color, lt);
    if(is_pixel_set(lum, thresh)) {
      if(flags & USE_DEBUG_IMAGE) {
        draw_color_pixel(debug_image, ix, iy, debug_color);
      }
      found_pixels++;
    }
  }
  return found_pixels;
}

/* print given number of space characters to given stream */
static void print_spaces(FILE *f, int n)
{
  int i;
  for (i = 0; i < n; i++) {
    fputc(' ', f);
  }
}

/* parse dimensions given as a string in the format "WxH" */
static int parse_width_height(const char *s, dimensions_struct *d)
{
  size_t l;
  const char *width_string;
  char *height_string;
  int w, h;
  if (!s || !d) {
    fputs(PROG ": error: parse_width_height() called with NULL pointer\n",
          stderr);
    return 1;
  }
  l = strlen(s);
  if (l == 0) {
    fputs(PROG ": error: parse_width_height() called with empty string\n",
          stderr);
    return 1;
  }
  width_string = s;
  height_string = strchr(s, 'x');
  if (!height_string) {
    fputs(PROG ": error: no 'x' in dimension specification\n", stderr);
    return 1;
  }
  if (width_string == height_string) {
    fputs(PROG ": error: width missing from dimension specification\n", stderr);
    return 1;
  }
  height_string++;
  if (strlen(height_string) == 0) {
    fputs(PROG ": error: height missing from dimension specification\n",stderr);
    return 1;
  }
  w = atoi(width_string);
  h = atoi(height_string);
  if (w < 1 || h < 1) {
    fprintf(stderr,
            PROG ": warning: ignoring mininmum character dimensions %dx%d\n",
            w, h);
    return 1;
  }
  d->w = w;
  d->h = h;
  return 0;
}

/* parse description of interval in one of two formats:
 * 1) a single number giving both upper and lower bound
 * 2) two numbers separated by a hyphen */
static int parse_interval(const char *s, interval_struct *i) {
  int min, max;
  char *upper;

  if (!s) {
    fputs(PROG ": error: empty interval description string\n", stderr);
    return 1;
  }
  min = atoi(s);
  if (min == -1) {
    i->min = i->max = min;
    return 0;
  }
  if (min < 1) {
    fputs(PROG ": error: lower interval bound cannot be less than 1\n", stderr);
    return 1;
  }
  upper = strchr(s, '-');
  if (!upper) {
    i->min = i->max = min;
    return 0;
  }
  if (upper == s) {
    fputs(PROG ": error: lower interval bound cannot be omitted\n", stderr);
    return 1;
  }
  max = atoi(++upper);
  if (max < min) {
    fputs(PROG ": error: upper bound less than lower bound\n", stderr);
    return 1;
  }
  i->min = min;
  i->max = max;
  return 0;
}

/*** main() ***/

int main(int argc, char **argv)
{
  Imlib_Image image=NULL; /* an image handle */
  Imlib_Image new_image=NULL; /* a temporary image handle */
  Imlib_Image debug_image=NULL; /* DEBUG */
  Imlib_Load_Error load_error=0; /* save Imlib2 error code on image I/O*/
  char *imgfile=NULL; /* filename of image file */
  int use_tmpfile=0; /* flag to know if temporary image file is used */

  int i, j, d;  /* iteration variables */
  size_t cur_digit_mem, new_digit_mem; /* for overflow checks */
  int unknown_digit=0; /* was one of the 6 found digits an unknown one? */
  int need_pixels = NEED_PIXELS; /* pixels needed to set segment in scanline */
  int min_segment = MIN_SEGMENT; /* minimum pixels needed for a segment */
  dimensions_struct min_char_dims; /* minimum character dimensions (W x H) */
  int potential_digits; /* number of potential digits after segmentation */
  interval_struct expected_digits; /* expect number of digits is inside this */
  int number_of_digits; /* number of digits found and accepted */
  int ignore_pixels = IGNORE_PIXELS; /* pixels to ignore when checking column */
  int one_ratio = ONE_RATIO; /* height/width > one_ratio => digit 'one' */
  int minus_ratio = MINUS_RATIO; /* height/width > minus_ratio => char 'minus'*/
  int dec_h_ratio = DEC_H_RATIO; /* max_dig_h/h > dec_h_ratio => possibly '.' */
  int dec_w_ratio = DEC_W_RATIO; /* max_dig_w/w > dec_w_ratio => possibly '.' */
  double spc_fac = SPC_FAC; /* add spaces if digit distance > spc_fac*min_dst */
  double thresh=THRESHOLD;  /* border between light and dark */
  int offset;  /* offset for shear */
  double theta; /* rotation angle */
  char *output_file=NULL; /* write processed image to file */
  char *output_fmt=NULL; /* use this format */
  char *debug_image_file=NULL; /* ...to this file */
  unsigned int flags=0; /* set by options, see #defines in .h file */
  luminance_t lt=DEFAULT_LUM_FORMULA; /* luminance function */
  charset_t charset=DEFAULT_CHARSET; /* character set */

  int w, h, lum;  /* width, height, pixel luminance */
  int col=UNKNOWN;  /* is column dark or light? */
  int row=UNKNOWN;  /* is row dark or light? */
  int dig_w;  /* width of digit part of image */
  int dig_h;  /* height of digit part of image */
  int max_dig_h=0, max_dig_w=0; /* maximum height & width of digits found */
  int widest_dig_is_one=0; /* set to one if the widest digit is a one */
  Imlib_Color color; /* Imlib2 RGBA color structure */
  /* state of search */
  int state = (ssocr_foreground == SSOCR_BLACK) ? FIND_DARK : FIND_LIGHT;
  digit_struct *digits=NULL; /* position of digits in image */
  int found_pixels=0; /* how many pixels are already found */
  color_struct d_color = {0, 0, 0, 0}; /* drawing color */

  /* initialize structures */
  min_char_dims.w = MIN_CHAR_W;
  min_char_dims.h = MIN_CHAR_H;
  expected_digits.min = expected_digits.max = NUMBER_OF_DIGITS;

  /* if we provided no arguments to the program exit */
  if (argc < 2) {
    usage(PROG, stderr);
    exit(99);
  }

  /* parse command line */
  while (1) {
    int option_index = 0;
    int c;
    static struct option long_options[] = {
      {"help", 0, 0, 'h'}, /* print help */
      {"version", 0, 0, 'V'}, /* show version */
      {"threshold", 1, 0, 't'}, /* set threshold (instead of THRESHOLD) */
      {"verbose", 0, 0, 'v'}, /* talk about programm execution */
      {"absolute-threshold", 0, 0, 'a'}, /* use treshold value as provided */
      {"iter-threshold", 0, 0, 'T'}, /* use treshold value as provided */
      {"number-pixels", 1, 0, 'n'}, /* pixels needed to regard segment as set */
      {"min-segment", 1, 0, 'N'}, /* minimum pixels needed for a segment */
      {"min-char-dims", 1, 0, 'M'}, /* minimum character (digit) dimensions */
      {"ignore-pixels", 1, 0, 'i'}, /* pixels ignored when searching digits */
      {"number-digits", 1, 0, 'd'}, /* number of digits in image */
      {"one-ratio", 1, 0, 'r'}, /* height/width threshold to recognize a one */
      {"minus-ratio", 1, 0, 'm'}, /* w/h threshold to recognize a minus sign */
      {"output-image", 1, 0, 'o'}, /* write processed image to given file */
      {"output-format", 1, 0, 'O'}, /* format of output image */
      {"debug-image", 2, 0, 'D'}, /* write a debug image */
      {"process-only", 0, 0, 'p'}, /* image processing only */
      {"debug-output", 0, 0, 'P'}, /* print debug output? */
      {"foreground", 1, 0, 'f'}, /* set foreground color */
      {"background", 1, 0, 'b'}, /* set background color */
      {"print-info", 0, 0, 'I'}, /* print image info */
      {"adjust-gray", 0, 0, 'g'}, /* use T1 and T2 as perecntages of used vals*/
      {"luminance", 1, 0, 'l'}, /* luminance formula */
      {"ascii-art-segments", 0, 0, 'S'}, /* print found segments in ASCII art */
      {"print-as-hex", 0, 0, 'X'}, /* change output format to hex */
      {"omit-decimal-point", 0, 0, 'C'}, /* omit decimal points from output */
      {"charset", 1, 0, 'c'}, /* select character set of display */
      {"dec-h-ratio", 1, 0, 'H'}, /* height ratio for decimal point detection */
      {"dec-w-ratio", 1, 0, 'W'}, /* width ratio for decimal point detection */
      {"print-spaces", 0, 0, 's'}, /* print spaces between distant digits */
      {"space-factor", 1, 0, 'A'}, /* relative distance to add spaces */
      {"space-average", 0, 0, 'G'}, /* avg instead of min dst for spaces */
      {"adapt-after-crop", 0, 0, 'F'}, /* don't adapt threshold before crop */
      {0, 0, 0, 0} /* terminate long options */
    };
    c = getopt_long (argc, argv,
                     "hVt:vaTn:N:i:d:r:m:M:o:O:D::pPf:b:Igl:SXCc:H:W:sA:GF",
                     long_options, &option_index);
    if (c == -1) break; /* leaves while (1) loop */
    switch (c) {
      case 'h':
        usage(PROG,stdout);
        exit (42);
        break;
      case 'V':
        print_version(stdout);
        exit (42);
        break;
      case 'v':
        flags |= VERBOSE;
        if(flags & DEBUG_OUTPUT) {
          fprintf(stderr, "flags & VERBOSE=%d\n", flags & VERBOSE);
        }
        break;
      case 't':
        if(optarg) {
          thresh = atof(optarg);
          if(flags & DEBUG_OUTPUT) {
            fprintf(stderr, "thresh = %f (default: %f)\n", thresh, THRESHOLD);
          }
          if(thresh < 0.0 || 100.0 < thresh) {
            thresh = THRESHOLD;
            if(flags & VERBOSE) {
              fprintf(stderr, "ignoring --treshold=%s\n", optarg);
            }
          }
          if(flags & DEBUG_OUTPUT) {
            fprintf(stderr, "thresh = %f (default: %f)\n", thresh, THRESHOLD);
          }
        }
        break;
      case 'a':
        flags |= ABSOLUTE_THRESHOLD; break;
      case 'T':
        flags |= DO_ITERATIVE_THRESHOLD; break;
      case 'n':
        if(optarg) {
          need_pixels = atoi(optarg);
          if(need_pixels < 1) {
            fprintf(stderr, PROG ": warning: ignoring --number-pixels=%s\n",
                    optarg);
            need_pixels = NEED_PIXELS;
          }
          if(flags & DEBUG_OUTPUT) {
            fprintf(stderr, "need_pixels = %d\n", need_pixels);
          }
        }
        break;
      case 'N':
        if(optarg) {
          min_segment = atoi(optarg);
          if(min_segment < 1) {
            fprintf(stderr, PROG ": warning: ignoring --min-segment=%s\n",
                    optarg);
            min_segment = MIN_SEGMENT;
            if(flags & DEBUG_OUTPUT) {
              fprintf(stderr, "min_segment = %d\n", min_segment);
            }
          } else {
            need_pixels = min_segment;
            if(flags & DEBUG_OUTPUT) {
              fprintf(stderr, "min_segment = need_pixels = %d\n", min_segment);
            }
          }
        }
        break;
      case 'M':
        if(optarg) {
          int ret;
          ret = parse_width_height(optarg, &min_char_dims);
          if (ret) {
            fprintf(stderr, PROG ": warning: ignoring --min-char-dims=%s\n",
                    optarg);
          }
          if(flags & DEBUG_OUTPUT) {
            fprintf(stderr, "min_char_dims = %dx%d\n", min_char_dims.w,
                    min_char_dims.h);
          }
        }
        break;
      case 'i':
        if(optarg) {
          ignore_pixels = atoi(optarg);
          if(ignore_pixels < 0) {
            fprintf(stderr, PROG ": warning: ignoring --ignore-pixels=%s\n",
                    optarg);
            ignore_pixels = IGNORE_PIXELS;
          }
        }
        break;
      case 'd':
        if(optarg) {
          int ret;
          ret = parse_interval(optarg, &expected_digits);
          if(ret) {
            fprintf(stderr, PROG ": warning: ignoring --number-digits=%s\n",
                    optarg);
            expected_digits.min = expected_digits.max = NUMBER_OF_DIGITS;
          }
          if (flags & DEBUG_OUTPUT) {
            fprintf(stderr, "expected_digits.min = %d\n", expected_digits.min);
            fprintf(stderr, "expected_digits.max = %d\n", expected_digits.max);
          }
        }
        break;
      case 'r':
        if(optarg) {
          one_ratio = atoi(optarg);
          if(one_ratio < 2) {
            fprintf(stderr, PROG ": warning: ignoring --one-ratio=%s\n",optarg);
            one_ratio = ONE_RATIO;
          }
        }
        break;
      case 'm':
        if(optarg) {
          minus_ratio = atoi(optarg);
          if(minus_ratio < 1) {
            fprintf(stderr, PROG ": warning: ignoring --minus-ratio=%s\n",
                    optarg);
            minus_ratio = MINUS_RATIO;
          }
        }
        break;
      case 'o':
        if(optarg) {
          output_file = strdup(optarg);
        }
        break;
      case 'O':
        if(optarg) {
          output_fmt = strdup(optarg);
        }
        break;
      case 'D':
        flags |= USE_DEBUG_IMAGE;
        if(optarg) {
          debug_image_file = strdup(optarg);
        } else {
          debug_image_file = strdup(DEBUG_IMAGE_NAME);
        }
        break;
      case 'p':
        flags |= PROCESS_ONLY; break;
      case 'P':
        flags |= (VERBOSE | DEBUG_OUTPUT); break;
      case 'f':
        if(optarg) {
          if(strcasecmp(optarg, "black") == 0) {
            set_fg_color(SSOCR_BLACK);
            set_bg_color(SSOCR_WHITE);
          } else if(strcasecmp(optarg, "white") == 0) {
            set_fg_color(SSOCR_WHITE);
            set_bg_color(SSOCR_BLACK);
          } else {
            fprintf(stderr, "%s: error: unknown foreground color %s,"
                            " color must be black or white\n", PROG, optarg);
            exit(99);
          }
        }
        break;
      case 'b':
        if(optarg) {
          if(strcasecmp(optarg, "black") == 0) {
            set_bg_color(SSOCR_BLACK);
            set_fg_color(SSOCR_WHITE);
          } else if(strcasecmp(optarg, "white") == 0) {
            set_bg_color(SSOCR_WHITE);
            set_fg_color(SSOCR_BLACK);
          } else {
            fprintf(stderr, "%s: error: unknown background color %s,"
                            " color must be black or white\n", PROG, optarg);
            exit(99);
          }
        }
        break;
      case 'I':
        flags |= PRINT_INFO;
        if(flags & DEBUG_OUTPUT) {
          fprintf(stderr, "flags & PRINT_INFO=%d\n", flags & PRINT_INFO);
        }
        break;
      case 'g':
        flags |= ADJUST_GRAY;
        if(flags & DEBUG_OUTPUT) {
          fprintf(stderr, "flags & ADJUST_GRAY=%d\n", flags & ADJUST_GRAY);
        }
        break;
      case 'l':
        if(optarg) {
          lt = parse_lum(optarg);
          if(lt == LUM_PARSE_ERROR) {
            fprintf(stderr,
                    PROG ": warning: ignoring unknown luminance formula '%s'\n",
                    optarg);
            lt = DEFAULT_LUM_FORMULA;
          }
        }
        break;
      case 'S':
        flags |= ASCII_ART_SEGMENTS;
        if(flags & DEBUG_OUTPUT) {
          fprintf(stderr, "flags & ASCII_ART_SEGMENTS=%d\n",
                  flags & ASCII_ART_SEGMENTS);
        }
        break;
      case 'X':
        flags |= PRINT_AS_HEX;
        if(flags & DEBUG_OUTPUT) {
          fprintf(stderr, "flags & PRINT_AS_HEX=%d\n",
                  flags & PRINT_AS_HEX);
        }
        break;
      case 'C':
        flags |= OMIT_DECIMAL;
        if(flags & DEBUG_OUTPUT) {
          fprintf(stderr, "flags & OMIT_DECIMAL=%d\n",
                  flags & OMIT_DECIMAL);
        }
        break;
      case 'c':
        if(optarg) {
          charset = parse_charset(optarg);
          if(charset == CS_PARSE_ERROR) {
            fprintf(stderr, PROG ": warning: ignoring unknown charset '%s'\n",
                    optarg);
            charset = DEFAULT_CHARSET;
          }
        }
        break;
      case 'H':
        if(optarg) {
          dec_h_ratio = atoi(optarg);
          if(dec_h_ratio < 2) {
            fprintf(stderr, PROG ": warning: ignoring --dec-h-ratio=%s\n",
                    optarg);
            dec_h_ratio = DEC_H_RATIO;
          }
        }
        break;
      case 'W':
        if(optarg) {
          dec_w_ratio = atoi(optarg);
          if(dec_w_ratio < 1) {
            fprintf(stderr, PROG ": warning: ignoring --dec-w-ratio=%s\n",
                    optarg);
            dec_w_ratio = DEC_W_RATIO;
          }
        }
        break;
      case 's':
        flags |= PRINT_SPACES;
        if(flags & DEBUG_OUTPUT) {
          fprintf(stderr, "flags & PRINT_SPACES=%d\n", flags & PRINT_SPACES);
        }
        break;
      case 'A':
        if(optarg) {
          spc_fac = atof(optarg);
          if(spc_fac < 1.0) {
            spc_fac = SPC_FAC;
            if(flags & (VERBOSE | DEBUG_OUTPUT)) {
              fprintf(stderr, "ignoring --space-factor=%s\n", optarg);
            }
          }
          if(flags & DEBUG_OUTPUT) {
            fprintf(stderr, "spc_fac = %f (default: %f)\n", spc_fac, SPC_FAC);
          }
        }
        break;
      case 'G':
        flags |= SPC_USE_AVG_DST;
        if(flags & DEBUG_OUTPUT) {
          fprintf(stderr, "flags & SPC_USE_AVG_DST=%d\n",
                          flags & SPC_USE_AVG_DST);
        }
        break;
      case 'F':
        flags |= ADAPT_AFTER_CROP;
        if(flags & DEBUG_OUTPUT) {
          fprintf(stderr, "flags & ADAPT_AFTER_CROP=%d\n",
                          flags & ADAPT_AFTER_CROP);
        }
        break;
      case '?':  /* missing argument or character not in optstring */
        short_usage(PROG,stderr);
        exit (2);
        break;
      default:   /* this should not be reached */
        if((c>31) && (c<127)) {
          fprintf (stderr, "%s: error: getopt returned unhandled character %c"
                           " (code %X)\n", PROG, c, c);
        } else {
          fprintf (stderr, "%s: error: getopt returned unhandled character code"
                           " %X\n", PROG, c);
        }
        short_usage(PROG, stderr);
        exit(99);
    }
  }
  if((flags & ABSOLUTE_THRESHOLD) && (flags & DO_ITERATIVE_THRESHOLD))
    fprintf(stderr, "%s: warning: -T has no effect due to -a\n", PROG);
  if(flags & DEBUG_OUTPUT) {
    fprintf(stderr, "================================================================================\n");
    fprintf(stderr, "VERSION=%s\n", VERSION);
    fprintf(stderr, "flags & VERBOSE=%d\nthresh=%f\n", flags & VERBOSE, thresh);
    fprintf(stderr, "flags & PRINT_INFO=%d\nflags & ADJUST_GRAY=%d\n",
            flags & PRINT_INFO, flags & ADJUST_GRAY);
    fprintf(stderr, "flags & ABSOLUTE_THRESHOLD=%d\n",flags&ABSOLUTE_THRESHOLD);
    fprintf(stderr, "flags & DO_ITERATIVE_THRESHOLD=%d\n",
                    flags & DO_ITERATIVE_THRESHOLD);
    fprintf(stderr, "flags & USE_DEBUG_IMAGE=%d\n", flags & USE_DEBUG_IMAGE);
    fprintf(stderr, "flags & DEBUG_OUTPUT=%d\n", flags & DEBUG_OUTPUT);
    fprintf(stderr, "flags & PROCESS_ONLY=%d\n", flags & PROCESS_ONLY);
    fprintf(stderr, "flags & ASCII_ART_SEGMENTS=%d\n",
                    flags & ASCII_ART_SEGMENTS);
    fprintf(stderr, "flags & PRINT_AS_HEX=%d\n", flags & PRINT_AS_HEX);
    fprintf(stderr, "flags & OMIT_DECIMAL=%d\n", flags & OMIT_DECIMAL);
    fprintf(stderr, "flags & PRINT_SPACES=%d\n", flags & PRINT_SPACES);
    fprintf(stderr, "flags & SPC_USE_AVG_DST=%d\n", flags & SPC_USE_AVG_DST);
    fprintf(stderr, "flags & ADAPT_AFTER_CROP=%d\n", flags & ADAPT_AFTER_CROP);
    fprintf(stderr, "need_pixels = %d\n", need_pixels);
    fprintf(stderr, "min_segment = %d\n", min_segment);
    fprintf(stderr, "min_char_dims = %dx%d\n",min_char_dims.w,min_char_dims.h);
    fprintf(stderr, "ignore_pixels = %d\n", ignore_pixels);
    fprintf(stderr, "expected_digits.min = %d\n", expected_digits.min);
    fprintf(stderr, "expected_digits.max = %d\n", expected_digits.max);
    fprintf(stderr, "foreground = %d (%s)\n", ssocr_foreground,
                    (ssocr_foreground == SSOCR_BLACK) ? "black" : "white");
    fprintf(stderr, "background = %d (%s)\n", ssocr_background,
                    (ssocr_background == SSOCR_BLACK) ? "black" : "white");
    fprintf(stderr, "luminance  = ");
    print_lum_key(lt, stderr); fprintf(stderr, "\n");
    fprintf(stderr, "charset    = ");
    print_cs_key(charset, stderr); fprintf(stderr, "\n");
    fprintf(stderr, "height/width threshold for one    = %d\n", one_ratio);
    fprintf(stderr, "width/height threshold for minus  = %d\n", minus_ratio);
    fprintf(stderr, "max_dig_h/h threshold for decimal = %d\n", dec_h_ratio);
    fprintf(stderr, "max_dig_w/w threshold for decimal = %d\n", dec_w_ratio);
    fprintf(stderr, "distance factor for adding spaces = %.2f\n", spc_fac);
    fprintf(stderr, "optind=%d argc=%d\n", optind, argc);
    fprintf(stderr, "================================================================================\n");
  }

  /* if no argument left exit the program */
  if(optind >= argc) {
    fprintf(stderr, "%s: error: no image filename given\n", PROG);
    short_usage(PROG, stderr);
    exit(99);
  }
  if(flags & DEBUG_OUTPUT) {
    fprintf(stderr, "argv[argc-1]=%s used as image file name\n", argv[argc-1]);
  }

  /* load the image */
  imgfile = argv[argc-1];
  if(strcmp("-", imgfile) == 0) /* read image from stdin? */ {
    if(flags & VERBOSE)
      fprintf(stderr, "using temporary file to hold data from stdin\n");
    use_tmpfile = 1;
    imgfile = tmp_imgfile(flags);
  }
  if(flags & VERBOSE) {
    fprintf(stderr, "loading image %s\n", imgfile);
  }
  image = imlib_load_image_with_error_return(imgfile, &load_error);
  if(use_tmpfile) {
    if(flags & VERBOSE)
      fprintf(stderr, "removing temporary image file %s\n", imgfile);
    unlink(imgfile);
    free(imgfile);
    imgfile = argv[argc-1];
  }
  if(!image) {
    fprintf(stderr, "%s: error: could not load image %s\n", PROG, imgfile);
    report_imlib_error(load_error);
    exit(99);
  }

  /* set the image we loaded as the current context image to work on */
  imlib_context_set_image(image);

  /* get image parameters */
  w = imlib_image_get_width();
  h = imlib_image_get_height();
  if((flags & DEBUG_OUTPUT) || (flags & PRINT_INFO)) {
    fprintf(stderr, "image width: %d\nimage height: %d\n",w,h);
  }

  /* get minimum and maximum "value" values */
  if((flags & DEBUG_OUTPUT) || (flags & PRINT_INFO)) {
    double min, max;
    get_minmaxval(&image, lt, &min, &max);
    fprintf(stderr, "%.2f <= lum <= %.2f (lum should be in [0,255])\n",
                    min, max);
  }

  /* process commands */
  if(flags & VERBOSE) /* then print found commands */ {
    if(optind >= argc-1) {
      fprintf(stderr, "no commands given, using image %s unmodified\n",
                      imgfile);
    } else {
      fprintf(stderr, "got commands");
      for(i=optind; i<argc-1; i++) {
        fprintf(stderr, " %s", argv[i]);
        if(flags & DEBUG_OUTPUT) {
          fprintf(stderr, " (argv[%d])", i);
        }
      }
      fprintf(stderr, "\n");
    }
  }
  if(optind < argc-1) /* then process commands */ {
    for(i=optind; i<argc-1; i++) {
      if(strcasecmp("dilation",argv[i]) == 0) {
        int n=atoi(argv[i+1]);
        if((n>0) && (i+1<argc-1)) {
          if(flags & VERBOSE) {
            fprintf(stderr, " processing dilation %d", n);
            if(flags & DEBUG_OUTPUT) {
              fprintf(stderr, " (from string %s)", argv[i+1]);
            }
            fprintf(stderr, "\n");
          }
          i++;
        } else {
          n = 1;
          if(flags & VERBOSE) fputs(" processing dilation (1)\n", stderr);
        }
        thresh = adapt_threshold(&image, thresh, lt, flags, INITIAL);
        new_image = dilation(&image, thresh, lt, n);
        imlib_context_set_image(image);
        imlib_free_image();
        image = new_image;
      } else if(strcasecmp("erosion",argv[i]) == 0) {
        int n=atoi(argv[i+1]);
        if((n>0) && (i+1<argc-1)) {
          if(flags & VERBOSE) {
            fprintf(stderr, " processing erosion %d", n);
            if(flags & DEBUG_OUTPUT) {
              fprintf(stderr, " (from string %s)", argv[i+1]);
            }
            fprintf(stderr, "\n");
          }
          i++;
        } else {
          n = 1;
          if(flags & VERBOSE) fputs(" processing erosion (1)\n", stderr);
        }
        thresh = adapt_threshold(&image, thresh, lt, flags, INITIAL);
        new_image = erosion(&image, thresh, lt, n);
        imlib_context_set_image(image);
        imlib_free_image();
        image = new_image;
      } else if(strcasecmp("opening",argv[i]) == 0) {
        int n=atoi(argv[i+1]);
        if((n>0) && (i+1<argc-1)) {
          if(flags & VERBOSE) {
            fprintf(stderr, " processing opening %d", n);
            if(flags & DEBUG_OUTPUT) {
              fprintf(stderr, " (from string %s)", argv[i+1]);
            }
            fprintf(stderr, "\n");
          }
          i++;
        } else {
          n = 1;
          if(flags & VERBOSE) fputs(" processing opening (1)\n", stderr);
        }
        thresh = adapt_threshold(&image, thresh, lt, flags, INITIAL);
        new_image = opening(&image, thresh, lt, n);
        imlib_context_set_image(image);
        imlib_free_image();
        image = new_image;
      } else if(strcasecmp("closing",argv[i]) == 0) {
        int n=atoi(argv[i+1]);
        if((n>0) && (i+1<argc-1)) {
          if(flags & VERBOSE) {
            fprintf(stderr, " processing closing %d", n);
            if(flags & DEBUG_OUTPUT) {
              fprintf(stderr, " (from string %s)", argv[i+1]);
            }
            fprintf(stderr, "\n");
          }
          i++;
        } else {
          n = 1;
          if(flags & VERBOSE) fputs(" processing closing (1)\n", stderr);
        }
        thresh = adapt_threshold(&image, thresh, lt, flags, INITIAL);
        new_image = closing(&image, thresh, lt, n);
        imlib_context_set_image(image);
        imlib_free_image();
        image = new_image;
      } else if(strcasecmp("remove_isolated",argv[i]) == 0) {
        if(flags & VERBOSE) fputs(" processing remove_isolated\n", stderr);
        thresh = adapt_threshold(&image, thresh, lt, flags, INITIAL);
        new_image = remove_isolated(&image, thresh, lt);
        imlib_context_set_image(image);
        imlib_free_image();
        image = new_image;
      } else if(strcasecmp("make_mono",argv[i]) == 0) {
        if(flags & VERBOSE) fputs(" processing make_mono\n", stderr);
        thresh = adapt_threshold(&image, thresh, lt, flags, INITIAL);
        new_image = make_mono(&image, thresh, lt);
        imlib_context_set_image(image);
        imlib_free_image();
        image = new_image;
      } else if(strcasecmp("white_border",argv[i]) == 0) {
        int bdwidth=atoi(argv[i+1]);
        if((bdwidth>0) && (i+1<argc-1)) {
          if(flags & VERBOSE) {
            fprintf(stderr, " processing white_border %d", bdwidth);
            if(flags & DEBUG_OUTPUT) {
              fprintf(stderr, " (from string %s)", argv[i+1]);
            }
            fprintf(stderr, "\n");
          }
          i++;
        } else {
          bdwidth = 1;
          if(flags & VERBOSE)
            fputs(" processing white_border (1)\n", stderr);
        }
        thresh = adapt_threshold(&image, thresh, lt, flags, INITIAL);
        new_image = white_border(&image, bdwidth);
        imlib_context_set_image(image);
        imlib_free_image();
        image = new_image;
      } else if(strcasecmp("shear",argv[i]) == 0) {
        if(flags & VERBOSE) {
          fprintf(stderr, " processing shear %d", atoi(argv[i+1]));
          if(flags & DEBUG_OUTPUT) {
            fprintf(stderr, " (from string %s)", argv[i+1]);
          }
          fprintf(stderr, "\n");
        }
        if(i+1<argc-1) {
          offset = (int) atoi(argv[++i]); /* sideeffect: increment i */
          thresh = adapt_threshold(&image, thresh, lt, flags, INITIAL);
          new_image = shear(&image, offset);
          imlib_context_set_image(image);
          imlib_free_image();
          image = new_image;
        } else {
          fprintf(stderr, "%s: error: shear command needs an argument\n", PROG);
          exit(99);
        }
      } else if(strcasecmp("set_pixels_filter",argv[i]) == 0) {
        int mask;
        if(flags & VERBOSE) {
          fprintf(stderr," processing set_pixels_filter %d", atoi(argv[i+1]));
          if(flags & DEBUG_OUTPUT) {
            fprintf(stderr, " (from string %s)", argv[i+1]);
          }
          fprintf(stderr, "\n");
        }
        if(i+1<argc-1) {
          mask = (int) atoi(argv[++i]); /* sideeffect: increment i */
          thresh = adapt_threshold(&image, thresh, lt, flags, INITIAL);
          new_image = set_pixels_filter(&image, thresh, lt, mask);
          imlib_context_set_image(image);
          imlib_free_image();
          image = new_image;
        } else {
          fprintf(stderr, "%s: error: set_pixels_filter command needs an:"
                          " argument\n", PROG);
          exit(99);
        }
      } else if(strcasecmp("keep_pixels_filter",argv[i]) == 0) {
        int mask;
        if(flags & VERBOSE) {
          fprintf(stderr," processing keep_pixels_filter %d",atoi(argv[i+1]));
          if(flags & DEBUG_OUTPUT) {
            fprintf(stderr, " (from string %s)", argv[i+1]);
          }
          fprintf(stderr, "\n");
        }
        if(i+1<argc-1) {
          mask = (int) atoi(argv[++i]); /* sideeffect: increment i */
          thresh = adapt_threshold(&image, thresh, lt, flags, INITIAL);
          new_image = keep_pixels_filter(&image, thresh, lt, mask);
          imlib_context_set_image(image);
          imlib_free_image();
          image = new_image;
        } else {
          fprintf(stderr, "%s: error: keep_pixels_filter command needs an"
                          " argument\n", PROG);
          exit(99);
        }
      } else if(strcasecmp("dynamic_threshold",argv[i]) == 0) {
        if(i+2<argc-1) {
          int ww, wh;
          ww = atoi(argv[i+1]);
          wh = atoi(argv[i+2]);
          if(flags & VERBOSE) {
            fprintf(stderr, " processing dynamic_threshold %d %d", ww, wh);
            if(flags & DEBUG_OUTPUT) {
              fprintf(stderr," (from strings %s and %s)",argv[i+1],argv[i+2]);
            }
            fprintf(stderr, "\n");
          }
          i+=2; /* skip the arguments to dynamic_threshold */
          thresh = adapt_threshold(&image, thresh, lt, flags, INITIAL);
          new_image = dynamic_threshold(&image, thresh, lt, ww, wh);
          imlib_context_set_image(image);
          imlib_free_image();
          image = new_image;
        } else {
          fprintf(stderr, "%s: error: dynamic_threshold command needs two"
                          " arguments\n", PROG);
          exit(99);
        }
      } else if(strcasecmp("rgb_threshold",argv[i]) == 0) {
        if(flags & VERBOSE) fputs(" processing rgb_threshold\n", stderr);
        thresh = adapt_threshold(&image, thresh, lt, flags, INITIAL);
        new_image = make_mono(&image, thresh, MINIMUM);
        imlib_context_set_image(image);
        imlib_free_image();
        image = new_image;
      } else if(strcasecmp("r_threshold",argv[i]) == 0) {
        if(flags & VERBOSE) fputs(" processing r_threshold\n", stderr);
        thresh = adapt_threshold(&image, thresh, lt, flags, INITIAL);
        new_image = make_mono(&image, thresh, RED);
        imlib_context_set_image(image);
        imlib_free_image();
        image = new_image;
      } else if(strcasecmp("g_threshold",argv[i]) == 0) {
        if(flags & VERBOSE) fputs(" processing g_threshold\n", stderr);
        thresh = adapt_threshold(&image, thresh, lt, flags, INITIAL);
        new_image = make_mono(&image, thresh, GREEN);
        imlib_context_set_image(image);
        imlib_free_image();
        image = new_image;
      } else if(strcasecmp("b_threshold",argv[i]) == 0) {
        if(flags & VERBOSE) fputs(" processing b_threshold\n", stderr);
        thresh = adapt_threshold(&image, thresh, lt, flags, INITIAL);
        new_image = make_mono(&image, thresh, BLUE);
        imlib_context_set_image(image);
        imlib_free_image();
        image = new_image;
      } else if(strcasecmp("invert",argv[i]) == 0) {
        if(flags & VERBOSE) fputs(" processing invert\n", stderr);
        thresh = adapt_threshold(&image, thresh, lt, flags, INITIAL);
        new_image = invert(&image, thresh, lt);
        imlib_context_set_image(image);
        imlib_free_image();
        image = new_image;
      } else if(strcasecmp("gray_stretch",argv[i]) == 0) {
        if(i+2<argc-1) {
          double t1, t2;
          t1 = atof(argv[i+1]);
          t2 = atof(argv[i+2]);
          if(flags & VERBOSE) {
            fprintf(stderr, " processing gray_stretch %.2f %.2f", t1, t2);
            if(flags & DEBUG_OUTPUT) {
              fprintf(stderr," (from strings %s and %s)",argv[i+1],argv[i+2]);
            }
            fprintf(stderr, "\n");
          }
          if(flags & ADJUST_GRAY) {
            double min=-1.0, max=-1.0;
            if(flags & VERBOSE) {
              fprintf(stderr, " adjusting T1=%.2f and T2=%.2f to image\n",
                              t1, t2);
            }
            get_minmaxval(&image, lt, &min, &max);
            t1 = min + t1/100.0 * (max - min);
            t2 = min + t2/100.0 * (max - min);
            if(flags & VERBOSE) {
              fprintf(stderr, " adjusted to T1=%.2f and T2=%.2f\n", t1, t2);
            }
          }
          i+=2; /* skip the arguments to gray_stretch */
          thresh = adapt_threshold(&image, thresh, lt, flags, INITIAL);
          new_image = gray_stretch(&image, t1, t2, lt);
          imlib_context_set_image(image);
          imlib_free_image();
          image = new_image;
        } else {
          fprintf(stderr, "%s: error: gray_stretch command needs two"
                          " arguments\n", PROG);
          exit(99);
        }
      } else if(strcasecmp("grayscale",argv[i]) == 0) {
        if(flags & VERBOSE) fputs(" processing grayscale\n", stderr);
        new_image = grayscale(&image, lt);
        imlib_context_set_image(image);
        imlib_free_image();
        image = new_image;
      } else if(strcasecmp("crop",argv[i]) == 0) {
        if(i+4<argc-1) {
          int x, y, cw, ch; /* cw = crop width, ch = crop height */
          x = atoi(argv[i+1]);
          y = atoi(argv[i+2]);
          cw = atoi(argv[i+3]);
          ch = atoi(argv[i+4]);
          if(flags & VERBOSE) {
            fprintf(stderr,
                    " cropping from (%d,%d) to (%d,%d) [width %d, height %d]",
                    x, y, x+cw, y+ch, cw, ch);
            if(flags & DEBUG_OUTPUT) {
              fprintf(stderr, " (from strings %s, %s, %s, and %s)", argv[i+1],
                      argv[i+2], argv[i+3], argv[i+4]);
            }
            fprintf(stderr, "\n");
          }
          i += 4; /* skip the arguments to crop */
          if(!(flags & ADAPT_AFTER_CROP))
            thresh = adapt_threshold(&image, thresh, lt, flags, INITIAL);
          new_image = crop(&image, x, y, cw, ch);
          imlib_context_set_image(image);
          imlib_free_image();
          image = new_image;
          imlib_context_set_image(image);
          /* get cropped image dimensions */
          w = imlib_image_get_width();
          h = imlib_image_get_height();
          if((flags & DEBUG_OUTPUT) || (flags & VERBOSE)) {
            fprintf(stderr, "  cropped image width: %d\n"
                            "  cropped image height: %d\n", w, h);
          }
          /* get minimum and maximum "value" values in cropped image */
          if((flags&DEBUG_OUTPUT) || (flags&PRINT_INFO) || (flags&VERBOSE)) {
            double min, max;
            get_minmaxval(&image, lt, &min, &max);
            fprintf(stderr, "  %.2f <= lum <= %.2f in cropped image"
                            " (lum should be in [0,255])\n", min, max);
          }
          /* adapt threshold to cropped image */
          thresh = adapt_threshold(&image, thresh, lt, flags, UPDATE);
        } else {
          fprintf(stderr, "%s: error: crop command needs 4 arguments\n", PROG);
          exit(99);
        }
      } else if(strcasecmp("rotate",argv[i]) == 0) {
        if(i+1<argc-1) {
          if(flags & VERBOSE) {
            fprintf(stderr, " processing rotate %f", atof(argv[i+1]));
            if(flags & DEBUG_OUTPUT) {
              fprintf(stderr, " (from string %s)", argv[i+1]);
            }
            fprintf(stderr, "\n");
          }
          theta = atof(argv[++i]); /* sideeffect: increment i */
          thresh = adapt_threshold(&image, thresh, lt, flags, INITIAL);
          new_image = rotate(&image, theta);
          imlib_context_set_image(image);
          imlib_free_image();
          image = new_image;
        } else {
          fprintf(stderr, "%s: error: rotate command needs an argument\n",
                          PROG);
          exit(99);
        }
      } else if(strcasecmp("mirror",argv[i]) == 0) {
        if(i+1<argc-1) {
          if(flags & VERBOSE) {
            fprintf(stderr, " processing mirror %s\n", argv[i+1]);
          }
          if(strncasecmp("horiz",argv[i+1],5) == 0) {
            new_image = mirror(&image, HORIZONTAL);
          } else if(strncasecmp("vert",argv[i+1],4) == 0) {
            new_image = mirror(&image, VERTICAL);
          } else {
            fprintf(stderr, "%s: error: argument to 'mirror' must be 'horiz'"
                            " or 'vert'\n", PROG);
            exit(99);
          }
          i++;
          imlib_context_set_image(image);
          imlib_free_image();
          image = new_image;
        } else {
          fprintf(stderr, "%s: error: mirror command needs argument 'horiz'"
                          " or 'vert'\n", PROG);
          exit(99);
        }
      } else {
        fprintf(stderr, " unknown command \"%s\"\n", argv[i]);
      }
    }
  }

  /* assure we are working with the current image */
  imlib_context_set_image(image);

  /* write image to file if requested */
  if(output_file) {
    save_image("output", image, output_fmt, output_file, flags);
  }

  /* exit if only image processing shall be done */
  if(flags & PROCESS_ONLY) exit(3);

  /* adapt threshold to image (unless this is already done) */
  thresh = adapt_threshold(&image, thresh, lt, flags, INITIAL);

  if(flags & USE_DEBUG_IMAGE) {
    /* copy processed image to debug image */
    debug_image = make_mono(&image, thresh, lt);
  }

  /* start image segmentation into possible characters / digits */
  if (flags & DEBUG_OUTPUT) {
    fputs("starting image segmentation\n", stderr);
    fputs("starting horizontal partitioning\n", stderr);
  }

  /* allocate memory for one seven segment digit */
  if(!(digits = calloc(1, sizeof(digit_struct)))) {
    perror(PROG ": digits = calloc()");
    exit(99);
  }

  /* horizontal partition */
  state = (ssocr_foreground == SSOCR_BLACK) ? FIND_DARK : FIND_LIGHT;
  d = 0;
  for(i=0; i<w; i++) {
    /* check if column is completely light or not */
    col = UNKNOWN;
    found_pixels = 0;
    for(j=0; j<h; j++) {
      imlib_image_query_pixel(i, j, &color);
      lum = get_lum(&color, lt);
      if(is_pixel_set(lum, thresh)) /* dark */ {
        found_pixels++;
        if(found_pixels > ignore_pixels) {
          /* 1 not ignored dark pixel darkens the whole column */
          col = (ssocr_foreground == SSOCR_BLACK) ? DARK : LIGHT;
        }
      } else if(col == UNKNOWN) /* light */ {
        col = (ssocr_foreground == SSOCR_BLACK) ? LIGHT : DARK;
      }
    }
    /* save digit position and draw partition line for DEBUG */
    if((state == ((ssocr_foreground == SSOCR_BLACK) ? FIND_DARK : FIND_LIGHT))
        && (col == ((ssocr_foreground == SSOCR_BLACK) ? DARK : LIGHT))) {
      /* beginning of digit */
      if (flags & DEBUG_OUTPUT) {
        fprintf(stderr, " start of potential digit %d in image column %d\n",
                d, i);
      }
      digits[d].x1 = i;
      digits[d].y1 = 0;
      if(flags & USE_DEBUG_IMAGE) {
        imlib_context_set_image(debug_image);
        imlib_context_set_color(255,0,0,255);/* red line for start of digit */
        imlib_image_draw_line(i,0,i,h-1,0);
        imlib_context_set_image(image);
      }
      state = (ssocr_foreground == SSOCR_BLACK) ? FIND_LIGHT : FIND_DARK;
    } else if((state ==
              ((ssocr_foreground == SSOCR_BLACK) ? FIND_LIGHT : FIND_DARK))
              && (col == ((ssocr_foreground == SSOCR_BLACK) ? LIGHT : DARK))){
      /* end of digit */
      if (flags & DEBUG_OUTPUT) {
        fprintf(stderr, " end of potential digit %d in image column %d\n",d,i);
      }
      digits[d].x2 = i;
      digits[d].y2 = h-1;
      if((d >= INT_MAX - 1) || (d < 0)) {
        fputs(PROG ": error: too many potential digits (integer overflow)\n",
              stderr);
        exit(99);
      }
      d++;
      if(flags & USE_DEBUG_IMAGE) {
        imlib_context_set_image(debug_image);
        imlib_context_set_color(0,0,255,255); /* blue line for end of digit */
        imlib_image_draw_line(i,0,i,h-1,0);
        imlib_context_set_image(image);
      }
      /* add memory for another digit */
      cur_digit_mem = d * sizeof(digit_struct);
      new_digit_mem = (d+1) * sizeof(digit_struct);
      if(new_digit_mem <= cur_digit_mem) {
        fputs(PROG ": error: size_t overflow (memory for digits)\n", stderr);
        exit(99);
      }
      if(!(digits = realloc(digits, new_digit_mem))) {
        perror(PROG ": digits = realloc()");
        exit(99);
      }
      /* initialize additional memory */
      memset(&digits[d], 0, sizeof(digit_struct));
      state = (ssocr_foreground == SSOCR_BLACK) ? FIND_DARK : FIND_LIGHT;
    }
  }

  /* after the loop above the program should be in state FIND_DARK,
   * i.e. after the last digit some light was found
   * if it is still searching for light then end the digit at the border of the
   * image */
  if(state == (ssocr_foreground == SSOCR_BLACK) ? FIND_LIGHT : FIND_DARK) {
    if (flags & DEBUG_OUTPUT) {
      fprintf(stderr, " end of potential digit %d in image column %d\n",d,w-1);
    }
    digits[d].x2 = w-1;
    digits[d].y2 = h-1;
    d++;
    state = (ssocr_foreground == SSOCR_BLACK) ? FIND_DARK : FIND_LIGHT;
  }

  /* horizontal partitioning has found "d" potential characters / digits */
  potential_digits = d;
  if(flags & DEBUG_OUTPUT) {
    fprintf(stderr, "horizontal partitioning found %d digit(s)\n",
            potential_digits);
  }

  /* find upper and lower boundaries of every digit */
  if (flags & DEBUG_OUTPUT) {
    fputs("looking for upper and lower digit boundaries\n", stderr);
  }
  for(d=0; d<potential_digits; d++) {
    int found_top=0;
    state = (ssocr_foreground == SSOCR_BLACK) ? FIND_DARK : FIND_LIGHT;
    /* start from top of image and scan rows for dark pixel(s) */
    for(j=0; j<h; j++) {
      row = UNKNOWN;
      found_pixels = 0;
      /* is row dark or light? */
      for(i=digits[d].x1; i<=digits[d].x2; i++) {
        imlib_image_query_pixel(i,j, &color);
        lum = get_lum(&color, lt);
        if(is_pixel_set(lum, thresh)) /* dark */ {
          found_pixels++;
          if(found_pixels > ignore_pixels) {
            /* 1 pixels darken row */
            row = (ssocr_foreground == SSOCR_BLACK) ? DARK : LIGHT;
          }
        } else if(row == UNKNOWN) {
          row = (ssocr_foreground == SSOCR_BLACK) ? LIGHT : DARK;
        }
      }
      /* save position of digit and draw partition line for DEBUG */
      if((state == ((ssocr_foreground == SSOCR_BLACK)?FIND_DARK:FIND_LIGHT))
          && (row == ((ssocr_foreground == SSOCR_BLACK) ? DARK : LIGHT))) {
        if(found_top) /* then we are searching for the bottom */ {
          digits[d].y2 = j;
          state = (ssocr_foreground == SSOCR_BLACK) ? FIND_LIGHT : FIND_DARK;
          if(flags & USE_DEBUG_IMAGE) {
            imlib_context_set_image(debug_image);
            imlib_context_set_color(0,255,0,255); /* green line */
            imlib_image_draw_line(digits[d].x1,digits[d].y2,
                                  digits[d].x2,digits[d].y2,0);
            imlib_context_set_image(image);
          }
        } else /* found the top line */ {
          digits[d].y1 = j;
          found_top = 1;
          state = (ssocr_foreground == SSOCR_BLACK) ? FIND_LIGHT : FIND_DARK;
          if(flags & USE_DEBUG_IMAGE) {
            imlib_context_set_image(debug_image);
            imlib_context_set_color(0,255,0,255); /* green line */
            imlib_image_draw_line(digits[d].x1,digits[d].y1,
                                  digits[d].x2,digits[d].y1,0);
            imlib_context_set_image(image);
          }
        }
      } else if((state ==
          ((ssocr_foreground == SSOCR_BLACK) ? FIND_LIGHT : FIND_DARK)) &&
          (row == ((ssocr_foreground == SSOCR_BLACK) ? LIGHT : DARK))) {
        /* found_top has to be 1 because otherwise we were still looking for
         * dark */
        digits[d].y2 = j;
        state = (ssocr_foreground == SSOCR_BLACK) ? FIND_DARK : FIND_LIGHT;
        if(flags & USE_DEBUG_IMAGE) {
          imlib_context_set_image(debug_image);
          imlib_context_set_color(0,255,0,255); /* green line */
          imlib_image_draw_line(digits[d].x1,digits[d].y2,
                                  digits[d].x2,digits[d].y2,0);
          imlib_context_set_image(image);
        }
      }
    }
    /* if we are still looking for light, use the bottom */
    if(state == ((ssocr_foreground == SSOCR_BLACK) ? FIND_LIGHT : FIND_DARK)){
      digits[d].y2 = h-1;
      state = (ssocr_foreground == SSOCR_BLACK) ? FIND_DARK : FIND_LIGHT;
      if(flags & USE_DEBUG_IMAGE) {
        imlib_context_set_image(debug_image);
        imlib_context_set_color(0,255,0,255); /* green line */
        imlib_image_draw_line(digits[d].x1,digits[d].y2,
                              digits[d].x2,digits[d].y2,0);
        imlib_context_set_image(image);
      }
    }
  }
  if (flags & DEBUG_OUTPUT) {
    fprintf(stderr, "image segmentation found %d potential digits\n",
            potential_digits);
  }

  /* image has been segmented into potential digits, ignore too small ones */
  if (min_char_dims.w > 1 || min_char_dims.h > 1) {
    int digit_count = 0, pos;
    digit_struct *tmp;
    if (flags & DEBUG_OUTPUT) {
      fputs("dropping too small potential digits\n", stderr);
    }
    /* count sufficiently large digits */
    for (d = 0; d < potential_digits; d++) {
      if (digits[d].x2 - digits[d].x1 >= min_char_dims.w &&
          digits[d].y2 - digits[d].y1 >= min_char_dims.h) {
        if (flags & DEBUG_OUTPUT) {
          fprintf(stderr, " keeping sufficiently large digit %d\n", d);
        }
        digit_count += 1;
      } else if (flags & DEBUG_OUTPUT) {
        fprintf(stderr, " dropping too small potential digit %d\n", d);
      }
    }
    if (flags & DEBUG_OUTPUT) {
      fprintf(stderr, "keeping %d of %d potential digits\n", digit_count,
              potential_digits);
    }
    /* at least one digit is required */
    if (digit_count < 1) {
      fputs(PROG ": error: no sufficiently large digits found\n", stderr);
      exit(1);
    }
    /* ensure we do not try to keep more digits than we have found */
    if(digit_count > potential_digits) {
      fprintf(stderr,
              PROG ": error: trying to keep more digits (%d) than found (%d)\n",
              digit_count, potential_digits);
      exit(99);
    }
    /* if potential digits are discarded, copy remaining ones to new memory */
    if(digit_count < potential_digits) {
      /* allocate memory for sufficiently large digits we want to keep */
      if(!(tmp = calloc(digit_count, sizeof(digit_struct)))) {
        perror(PROG ": tmp = calloc()");
        exit(99);
      }
      /* keep only sufficiently large digits */
      pos = 0;
      for (d = 0; d < potential_digits; d++) {
        if (digits[d].x2 - digits[d].x1 >= min_char_dims.w &&
            digits[d].y2 - digits[d].y1 >= min_char_dims.h) {
          if (pos >= digit_count) {
            fputs(PROG ": error copying digit information", stderr);
            exit(99);
          }
          memcpy(tmp + pos, digits + d, sizeof(digit_struct));
          pos++;
        }
      }
      free(digits);
      digits = tmp;
      potential_digits = digit_count;
    }
  }

  /* check if expected number of digits have been found */
  if ((expected_digits.min > -1) &&
      ((potential_digits < expected_digits.min) ||
       (potential_digits > expected_digits.max))) {
    if (expected_digits.min != expected_digits.max) {
      fprintf(stderr,
              PROG ": expected between %d and %d digits, but found %d\n",
              expected_digits.min, expected_digits.max, potential_digits);
    } else {
      fprintf(stderr, PROG ": expected %d digit%s, but found %d\n",
              expected_digits.min, expected_digits.min > 1 ? "s" : "",
              potential_digits);
    }
    imlib_free_image_and_decache();
    if(flags & USE_DEBUG_IMAGE) {
      save_image("debug", debug_image, output_fmt,debug_image_file,flags);
      imlib_context_set_image(debug_image);
      imlib_free_image_and_decache();
    }
    exit(1);
  }

  /* continue to work with the accepted number of characters / digits */
  number_of_digits = potential_digits;
  if (flags & DEBUG_OUTPUT) {
    fprintf(stderr, "image segmentation found %d digits\n", number_of_digits);
  }

  /* draw rectangles around accepted digits */
  if(flags & USE_DEBUG_IMAGE) {
    imlib_context_set_image(debug_image);
    imlib_context_set_color(128,128,128,255); /* gray line */
    for(d=0; d<number_of_digits; d++) {
      imlib_image_draw_rectangle(digits[d].x1, digits[d].y1,
          digits[d].x2-digits[d].x1, digits[d].y2-digits[d].y1);
    }
    imlib_context_set_image(image);
  }

  /* determine size of digit part of the image */
  dig_w = digits[number_of_digits-1].x2 - digits[0].x1;
  dig_h = digits[number_of_digits-1].y2 - digits[0].y1;
  if (flags & DEBUG_OUTPUT) {
    fprintf(stderr, "total width of digit area is %d\n", dig_w);
    fprintf(stderr, "total height of digit area is %d\n", dig_h);
  }

  /* determine maximum digit dimensions */
  for(d=0; d<number_of_digits; d++) {
    if(max_dig_w < digits[d].x2 - digits[d].x1)
      max_dig_w = digits[d].x2 - digits[d].x1;
    if(max_dig_h < digits[d].y2 - digits[d].y1)
      max_dig_h = digits[d].y2 - digits[d].y1;
  }
  if(flags & DEBUG_OUTPUT)
    fprintf(stderr, "digits are at most %d pixels wide and %d pixels high\n",
                    max_dig_w, max_dig_h);

  /* debug: write digit info to stderr */
  if(flags & DEBUG_OUTPUT) {
    fprintf(stderr, "found %d digits\n", number_of_digits);
    for(d=0; d<number_of_digits; d++) {
      fprintf(stderr, "digit %d: (%d,%d) -> (%d,%d), width: %d (%5.2f%%) "
                      "height: %d (%5.2f%%)\n",
                      d,
                      digits[d].x1, digits[d].y1, digits[d].x2, digits[d].y2,
                      digits[d].x2 - digits[d].x1,
                      ((digits[d].x2 - digits[d].x1) * 100.0) / dig_w,
                      digits[d].y2 - digits[d].y1,
                      ((digits[d].y2 - digits[d].y1) * 100.0) / dig_h
             );
      fprintf(stderr, "  height/width (int): ");
      if(digits[d].x1 == digits[d].x2) {
        fprintf(stderr, "NaN, max_dig_w/width (int): NaN, ");
      } else {
        fprintf(stderr, "%d, max_dig_w/width (int): %d, ",
                       (digits[d].y2-digits[d].y1)/(digits[d].x2-digits[d].x1),
                       max_dig_w / (digits[d].x2 - digits[d].x1)
              );
      }
      fprintf(stderr, "max_dig_h/height (int): ");
      if(digits[d].y1 == digits[d].y2) {
        fprintf(stderr, "NaN\n");
      } else {
        fprintf(stderr, "%d\n",
                        max_dig_h / (digits[d].y2 - digits[d].y1)
               );
      }
    }
  }

  /* at this point the digit 1 can be identified, because it is smaller than
   * the other digits */
  if(flags & DEBUG_OUTPUT)
    fputs("looking for digit 1\n",stderr);
  for(d=0; d<number_of_digits; d++) {
    /* skip digits too narrow for a segment */
    if(digits[d].x2 - digits[d].x1 < min_segment) {
      if(flags & DEBUG_OUTPUT)
        fprintf(stderr, " skipping too narrow digit %d with width %d\n", d,
                digits[d].x2 - digits[d].x1);
      continue;
    }
    /* if width of digit is less than 1/one_ratio of its height it is a 1
     * (the default 1/3 is arbitarily chosen -- normally seven segment
     * displays use digits that are 2 times as high as wide) */
    if((digits[d].y2-digits[d].y1+0.5)/(digits[d].x2-digits[d].x1) > one_ratio){
      if(flags & DEBUG_OUTPUT) {
        fprintf(stderr, " digit %d is a 1 (height/width = %d/%d = (int) %d)\n",
               d, digits[d].y2 - digits[d].y1, digits[d].x2 - digits[d].x1,
               (digits[d].y2 - digits[d].y1) / (digits[d].x2 - digits[d].x1));
      }
      digits[d].digit = D_ONE;
    }
  }

  /* identify a decimal point (or thousands separator) by relative size */
  if(flags & DEBUG_OUTPUT)
    fputs("looking for decimal points\n",stderr);
  for(d=0; d<number_of_digits; d++) {
    /* skip digits with zero width or height */
    if((digits[d].x1 == digits[d].x2) || (digits[d].y1 == digits[d].y2)) {
      if(flags & DEBUG_OUTPUT)
        fprintf(stderr, " skipping digit %d with zero width or height\n", d);
      continue;
    }
    /* if height of a digit is less than 1/5 of the maximum digit height,
     * and its width is less than 1/2 of the maximum digit width (the widest
     * digit might be a one), assume it is a decimal point */
    if((digits[d].digit == D_UNKNOWN) &&
       (max_dig_h / (digits[d].y2 - digits[d].y1) > dec_h_ratio) &&
       (max_dig_w / (digits[d].x2 - digits[d].x1) > dec_w_ratio)) {
      digits[d].digit = D_DECIMAL;
      if(flags & DEBUG_OUTPUT)
        fprintf(stderr, " digit %d is a decimal point\n", d);
    }
  }

  /* identify a minus sign */
  if(flags & DEBUG_OUTPUT)
    fputs("looking for minus signs\n",stderr);
  for(d=0; d<number_of_digits; d++) {
    /* skip digits too short for a segment */
    if(digits[d].y2 - digits[d].y1 < min_segment) {
      if(flags & DEBUG_OUTPUT)
        fprintf(stderr, " skipping too short digit %d with height %d\n", d,
                digits[d].y2 - digits[d].y1);
      continue;
    }
    /* if height of digit is less than 1/minus_ratio of its height it is a 1
     * (the default 1/3 is arbitarily chosen -- normally seven segment
     * displays use digits that are 2 times as high as wide) */
    if((digits[d].digit == D_UNKNOWN) &&
       ((digits[d].x2-digits[d].x1)/(digits[d].y2-digits[d].y1)>=minus_ratio)) {
      if(flags & DEBUG_OUTPUT) {
        fprintf(stderr,
               " digit %d is a minus (width/height = %d/%d = (int) %d)\n",
               d, digits[d].x2 - digits[d].x1, digits[d].y2 - digits[d].y1,
               (digits[d].x2 - digits[d].x1) / (digits[d].y2 - digits[d].y1));
      }
      digits[d].digit = D_MINUS;
    }
  }

  /* If the widest digit is a one, decimal points may be of the same width,
   * and may thus not be detected.  Now that minus signs have been selected,
   * if the widest digit still is a one (i.e., no minus signs), then decimal
   * separators may also be recognized by checking only the height, not the
   * width. */
  if(flags & DEBUG_OUTPUT)
    fputs("checking for special case of a one as widest character\n",stderr);
  /* check if the widest digit is a one */
  for(d=0; d<number_of_digits; d++) {
    /* skip digits with zero width or height */
    if((digits[d].x1 == digits[d].x2) || (digits[d].y1 == digits[d].y2)) {
      if(flags & DEBUG_OUTPUT)
        fprintf(stderr, " skipping digit %d with zero width or height\n", d);
      continue;
    }
    if((digits[d].digit == D_ONE) && (digits[d].x2-digits[d].x1 >= max_dig_w)) {
      widest_dig_is_one = 1;
      if(flags & DEBUG_OUTPUT)
        fputs(" widest digit is a one -> additional decimal point search\n",
              stderr);
      break;
    }
  }
  if(!widest_dig_is_one) {
    if(flags & DEBUG_OUTPUT)
      fputs(" widest digit is not a one, skipping extra decimal point search\n",
            stderr);
  } else {
    /* widest digit is a one, thus decimal seperators may have been missed:
     * identify a decimal point (or thousands separator) by relative height */
    if(flags & DEBUG_OUTPUT)
      fputs("looking for decimal points again\n",stderr);
    for(d=0; d<number_of_digits; d++) {
      /* skip digits with zero width or height */
      if((digits[d].x1 == digits[d].x2) || (digits[d].y1 == digits[d].y2)) {
        if(flags & DEBUG_OUTPUT)
          fprintf(stderr, " skipping digit %d with zero width or height\n", d);
        continue;
      }
      /* if height of a digit is less than 1/5 of the maximum digit height,
       * and its width is less than 1/2 of the maximum digit width (the widest
       * digit might be a one), assume it is a decimal point */
      if((digits[d].digit == D_UNKNOWN) &&
         (max_dig_h / (digits[d].y2 - digits[d].y1) > dec_h_ratio)) {
        digits[d].digit = D_DECIMAL;
        if(flags & DEBUG_OUTPUT)
          fprintf(stderr, " digit %d is a decimal point\n", d);
      }
    }
  }

  /* now the digits are located and they have to be identified */
  if(flags & DEBUG_OUTPUT)
    fputs("starting scanline based recognition for remaining digits\n", stderr);
  /* iterate over digits */
  for(d=0; d<number_of_digits; d++) {
    int d_height=0; /* height of digit */
    /* skip digits too small to contain a segment */
    if((digits[d].x2 - digits[d].x1 < min_segment) ||
       (digits[d].y2 - digits[d].y1 < min_segment)) {
      if(flags & DEBUG_OUTPUT)
        fprintf(stderr, " skipping digit %d smaller than minimum segment\n", d);
      continue;
    }
    /* skip already recognized digits */
    if(digits[d].digit == D_UNKNOWN) {
      int middle = (digits[d].x1 + digits[d].x2) / 2;
      int quarter = digits[d].y1 + (digits[d].y2 - digits[d].y1) / 4;
      int three_quarters = digits[d].y1 + 3 * (digits[d].y2 - digits[d].y1) / 4;
      found_pixels=0; /* how many pixels are already found */
      d_height = digits[d].y2 - digits[d].y1;
      /* check horizontal segments (vertical scan, x == middle) */
      d_color.R = d_color.A = 255;
      d_color.G = d_color.B = 0;
      found_pixels = scanline(&debug_image, middle, digits[d].y1,
                              d_height/3, VERTICAL, d_color, thresh, lt, flags);
      if(found_pixels >= need_pixels) {
        digits[d].digit |= HORIZ_UP; /* add upper segment */
      }
      d_color.G = d_color.A = 255;
      d_color.R = d_color.B = 0;
      found_pixels = scanline(&debug_image, middle,
                              digits[d].y1 + d_height/3, d_height/3, VERTICAL,
                              d_color, thresh, lt, flags);
      if(found_pixels >= need_pixels) {
        digits[d].digit |= HORIZ_MID; /* add middle segment */
      }
      d_color.B = d_color.A = 255;
      d_color.R = d_color.G = 0;
      found_pixels = scanline(&debug_image, middle,
                              digits[d].y1 + 2*d_height/3, d_height/3, VERTICAL,
                              d_color, thresh, lt, flags);
      if(found_pixels >= need_pixels) {
        digits[d].digit |= HORIZ_DOWN; /* add lower segment */
      }
      /* check upper vertical segments (horizontal scan, y == quarter) */
      d_color.R = d_color.A = 255;
      d_color.G = d_color.B = 0;
      found_pixels = scanline(&debug_image, digits[d].x1, quarter,
                              (digits[d].x2 - digits[d].x1) / 2, HORIZONTAL,
                              d_color, thresh, lt, flags);
      if (found_pixels >= need_pixels) {
        digits[d].digit |= VERT_LEFT_UP; /* add upper left segment */
      }
      d_color.G = d_color.A = 255;
      d_color.R = d_color.B = 0;
      found_pixels = scanline(&debug_image,
                              (digits[d].x1 + digits[d].x2) / 2 + 1,
                              quarter, (digits[d].x2 - digits[d].x1) / 2 - 1,
                              HORIZONTAL, d_color, thresh, lt, flags);
      if (found_pixels >= need_pixels) {
        digits[d].digit |= VERT_RIGHT_UP; /* add upper right segment */
      }
      /* check lower vertical segments (horizontal scan, y == three_quarters) */
      d_color.R = d_color.A = 255;
      d_color.G = d_color.B = 0;
      found_pixels = scanline(&debug_image, digits[d].x1,
                              three_quarters, (digits[d].x2 - digits[d].x1) / 2,
                              HORIZONTAL, d_color, thresh, lt, flags);
      if (found_pixels >= need_pixels) {
        digits[d].digit |= VERT_LEFT_DOWN; /* add lower left segment */
      }
      d_color.G = d_color.A = 255;
      d_color.R = d_color.B = 0;
      found_pixels = scanline(&debug_image,
                              (digits[d].x1 + digits[d].x2) / 2 + 1,
                              three_quarters, (digits[d].x2-digits[d].x1)/2 - 1,
                              HORIZONTAL, d_color, thresh, lt, flags);
      if (found_pixels >= need_pixels) {
        digits[d].digit |= VERT_RIGHT_DOWN; /* add lower right segment */
      }
    }
  }

  /* check spacing of digits when --print-spaces is given and there are more
   * more than two digits
  */
  if ((flags & PRINT_SPACES) && (number_of_digits > 2)) {
    int min_dst, avg_dst, dst_sum, cur_dst, base_dst, num_spc;
    if (flags & DEBUG_OUTPUT) {
      fputs("looking for white space\n", stderr);
    }

    /* determine distance between digits */
    min_dst = dst_sum = digits[1].x2 - digits[0].x2;
    if (flags & DEBUG_OUTPUT) {
      fprintf(stderr, " distance between digits 0 and 1 is %d\n", min_dst);
    }
    for (i = 2; i < number_of_digits; i++) {
      cur_dst = digits[i].x2 - digits[i-1].x2;
      if (flags & DEBUG_OUTPUT) {
        fprintf(stderr, " distance between digits %d and %d is %d\n",
                       i-1, i, cur_dst);
      }
      if (cur_dst < min_dst) {
        min_dst = cur_dst;
      }
      dst_sum += cur_dst;
    }
    avg_dst = dst_sum / (number_of_digits - 1);
    base_dst = (flags & SPC_USE_AVG_DST) ? avg_dst : min_dst;
    if (base_dst < 1) {
      base_dst = 1;
    }
    if (flags & DEBUG_OUTPUT) {
      fprintf(stderr, " minimum digit distance: %d\n", min_dst);
      fprintf(stderr, " average digit distance: %d\n", avg_dst);
      fprintf(stderr, " adding spaces for distance greater than: %d\n",
                      (int) (spc_fac * base_dst));
    }

    /* determine number of spaces after each digit */
    for (i = 0; i < (number_of_digits - 1); i++) {
      num_spc = (int) ((digits[i+1].x2 - digits[i].x2) / (spc_fac * base_dst));
      if (num_spc > 0) {
        if (flags & DEBUG_OUTPUT) {
          fprintf(stderr, " adding %d space character(s) after digit %d\n",
                          num_spc, i);
        }
        digits[i].spaces = num_spc;
      }
    }
  }

  /* print found segments as ASCII art if debug output is enabled
   * or ASCII art output is requested explicitely
   * example digits known by ssocr:
   *   _      _  _       _   _  _   _   _   _
   *  | |  |  _| _| |_| |_  |_   | | | |_| |_|
   *  |_|  | |_  _|   |  _| |_|  |   | |_|  _|
  */
  if(flags & (DEBUG_OUTPUT | ASCII_ART_SEGMENTS)) {
    fputs("Display as seen by ssocr:\n", stderr);
    /* top row */
    for(i=0; i<number_of_digits; i++) {
      fputc(' ', stderr);
      fputc(' ', stderr);
      digits[i].digit & HORIZ_UP ? fputc('_', stderr) : fputc(' ', stderr);
      fputc(' ', stderr);
      print_spaces(stderr, digits[i].spaces * 3);
    }
    fputc('\n', stderr);
    /* middle row */
    for(i=0; i<number_of_digits; i++) {
      fputc(' ', stderr);
      digits[i].digit & VERT_LEFT_UP ? fputc('|', stderr) : fputc(' ', stderr);
      digits[i].digit & HORIZ_MID ? fputc('_', stderr) :
        digits[i].digit == D_MINUS ? fputc('_', stderr) : fputc(' ', stderr);
      digits[i].digit & VERT_RIGHT_UP ? fputc('|', stderr) : fputc(' ', stderr);
      print_spaces(stderr, digits[i].spaces * 3);
    }
    fputc('\n', stderr);
    /* bottom row */
    for(i=0; i<number_of_digits; i++) {
      fputc(' ', stderr);
      digits[i].digit&VERT_LEFT_DOWN ? fputc('|', stderr) : fputc(' ', stderr);
      digits[i].digit&HORIZ_DOWN ? fputc('_', stderr) : 
        digits[i].digit == D_DECIMAL ? fputc('.', stderr) : fputc(' ', stderr);
      digits[i].digit&VERT_RIGHT_DOWN ? fputc('|', stderr) : fputc(' ', stderr);
      print_spaces(stderr, digits[i].spaces * 3);
    }
    fputs("\n\n", stderr);
  }

  /* print digits */
  if(flags & PRINT_AS_HEX) {
    for(i=0; i<number_of_digits; i++) {
      if(i > 0) putchar(':');
      printf("%02x", digits[i].digit);
      print_spaces(stdout, digits[i].spaces);
    }
  } else {
    init_charset(charset);
    for(i=0; i<number_of_digits; i++) {
      unknown_digit += print_digit(digits[i].digit, flags);
      print_spaces(stdout, digits[i].spaces);
    }
  }
  putchar('\n');

  /* clean up... */
  imlib_free_image_and_decache();
  if(flags & USE_DEBUG_IMAGE) {
    save_image("debug", debug_image, output_fmt, debug_image_file, flags);
    imlib_context_set_image(debug_image);
    imlib_free_image_and_decache();
  }

  /* determin error code */
  if(unknown_digit) {
    exit(2);
  } else {
    exit(0);
  }
}
