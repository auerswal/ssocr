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

/* Copyright (C) 2004-2013 Erik Auerswald <auerswal@unix-ag.uni-kl.de> */

/* ImLib2 Header */
#include <X11/Xlib.h>       /* needed by Imlib2.h */
#include <Imlib2.h>

/* standard things */
#include <stdio.h>          /* puts, printf, BUFSIZ, perror, FILE */
#include <stdlib.h>         /* exit */

/* string manipulation */
#include <string.h>         /* strcpy, strdup */

/* option parsing */
#include <getopt.h>         /* getopt */
#include <unistd.h>         /* getopt */

/* file permissions */
#include <sys/stat.h>       /* umask */

/* my headers */
#include "defines.h"        /* defines */
#include "ssocr.h"          /* types */
#include "imgproc.h"        /* image processing */
#include "help.h"           /* online help */

/* global variables */
int ssocr_foreground = SSOCR_BLACK;
int ssocr_background = SSOCR_WHITE;

/* functions */

/* copy image from stdin to a temporary file and return the filename */
static char * tmp_imgfile(int flags)
{
  char *dir;
  char *name;
  size_t pattern_len;
  int handle;
  unsigned char buf;
  ssize_t count = 0;

  /* find a suitable place (directory) for the tmp file and create pattern */
  dir = getenv("TMP");
  if(dir) {
    if(flags & DEBUG_OUTPUT)
      fprintf(stderr, "using %s for temporary files, from $TMP env variable\n",
              dir);
    pattern_len = strlen(dir) + strlen(DIR_SEP) + strlen(TMP_FILE_PATTERN) + 1;
  } else {
    if(flags & DEBUG_OUTPUT)
      fprintf(stderr, "using " TMP_FILE_DIR " for temporary files\n");
    pattern_len = strlen(TMP_FILE_DIR) + strlen(DIR_SEP)
                + strlen(TMP_FILE_PATTERN) + 1;
  }
  name = malloc(pattern_len);
  if(!name) {
    perror("could not allocate memory for name of temporary file");
    exit(99);
  }
  if(dir)
    name = strcat(name, dir);
  else
    name = strcat(name, TMP_FILE_DIR);
  name = strcat(name, DIR_SEP);
  name = strcat(name, TMP_FILE_PATTERN);
  if(flags & DEBUG_OUTPUT)
    fprintf(stderr, "pattern for temporary file is %s\n", name);

  /* create temporary file */
  umask(S_IRWXG | S_IRWXO);
  handle = mkstemp(name);
  if(handle < 0) {
    perror("could not create temporary file");
    exit(99);
  }

  /* copy image data from stdin to tmp file */
  while((fread(&buf, sizeof(char), 1, stdin)) > 0) {
    count = write(handle, &buf, 1);
    if (count <= 0) break;
  }
  close(handle); /* filehandle is no longer needed, Imlib2 uses filename */
  if(ferror(stdin) || (count <= 0)) {
    perror("could not copy image data to temporary file");
    unlink(name);
    exit(99);
  }
  
  return name;
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
  int unknown_digit=0; /* was one of the 6 found digits an unknown one? */
  int need_pixels = NEED_PIXELS; /*pixels needed to recognize a segment as set*/
  int number_of_digits = NUMBER_OF_DIGITS; /* look for this many digits */
  int ignore_pixels = IGNORE_PIXELS; /* pixels to ignore when checking column */
  int one_ratio = ONE_RATIO; /* height/width > one_ratio => digit 'one' */
  double thresh=THRESHOLD;  /* border between light and dark */
  int offset;  /* offset for shear */
  double theta; /* rotation angle */
  char *output_file=NULL; /* wrie processed image to file */
  char *output_fmt=NULL; /* use this format */
  char *debug_image_file=NULL; /* ...to this file */
  int flags=0; /* set by options, see #defines on .h file */
  luminance_t lt=DEFAULT_LUM_FORMULA; /* luminance function */

  int w, h, lum;  /* width, height, pixel luminance */
  int col=UNKNOWN;  /* is column dark or light? */
  int row=UNKNOWN;  /* is row dark or light? */
  int dig_w;  /* width of digit part of image */
  int dig_h;  /* height of digit part of image */
  int max_dig_h=0, max_dig_w=0; /* maximum height & width of digits found */
  Imlib_Color color; /* Imlib2 RGBA color structure */
  /* state of search */
  int state = (ssocr_foreground == SSOCR_BLACK) ? FIND_DARK : FIND_LIGHT;
  digit_struct *digits=NULL; /* position of digits in image */
  int found_pixels=0; /* how many pixels are already found */

  /* if we provided no arguments to the program exit */
  if (argc < 2) {
    usage(argv[0], stderr);
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
      {"ignore-pixels", 1, 0, 'i'}, /* pixels ignored when searching digits */
      {"number-digits", 1, 0, 'd'}, /* number of digits in image */
      {"one-ratio", 1, 0, 'r'}, /* wheight/width threshold to recognize a one */
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
      {0, 0, 0, 0} /* terminate long options */
    };
    c = getopt_long (argc, argv, "hVt:vaTn:i:d:r:o:O:D::pPf:b:Igl:",
                     long_options, &option_index);
    if (c == -1) break; /* leaves while (1) loop */
    switch (c) {
      case 'h':
        usage(argv[0],stdout);
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
            fprintf(stderr, "warning: ignoring --number-pixels=%s\n", optarg);
            need_pixels = NEED_PIXELS;
          }
        }
        break;
      case 'i':
        if(optarg) {
          ignore_pixels = atoi(optarg);
          if(ignore_pixels < 0) {
            fprintf(stderr, "warning: ignoring --ignore-pixels=%s\n", optarg);
            ignore_pixels = IGNORE_PIXELS;
          }
        }
        break;
      case 'd':
        if(optarg) {
          number_of_digits = atoi(optarg);
          if((number_of_digits < 1) && (number_of_digits != -1)) {
            fprintf(stderr, "warning: ignoring --number-digits=%s\n", optarg);
            number_of_digits = NUMBER_OF_DIGITS;
          }
        }
        break;
      case 'r':
        if(optarg) {
          one_ratio = atoi(optarg);
          if(one_ratio < 2) {
            fprintf(stderr, "warning: ignoring --one-ratio=%s\n", optarg);
            one_ratio = ONE_RATIO;
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
        flags |= DEBUG_OUTPUT; break;
      case 'f':
        if(optarg) {
          if(strcasecmp(optarg, "black") == 0) {
            set_fg_color(SSOCR_BLACK);
            set_bg_color(SSOCR_WHITE);
          } else if(strcasecmp(optarg, "white") == 0) {
            set_fg_color(SSOCR_WHITE);
            set_bg_color(SSOCR_BLACK);
          } else {
            fprintf(stderr, "error: unknown foreground color %s,"
                            " color must be black or white\n", optarg);
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
            fprintf(stderr, "error: unknown background color %s,"
                            " color must be black or white\n", optarg);
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
        }
        break;
      case '?':  /* missing argument or character not in optstring */
        usage(argv[0],stderr);
        exit (2);
        break;
      default:   /* this should not be reached */
        if((c>31) && (c<127)) {
          fprintf (stderr, 
                   "error: getopt returned unhandled character %c (code %X)\n",
                   c, c);
        } else {
          fprintf (stderr,
                   "error: getopt returned unhandled character code %X\n", c);
        }
        usage(argv[0], stderr);
        exit(99);
    }
  }
  if(flags & DEBUG_OUTPUT) {
    fprintf(stderr, "================================================================================\n");
    fprintf(stderr, "flags & VERBOSE=%d\nthresh=%f\n", flags & VERBOSE, thresh);
    fprintf(stderr, "flags & PRINT_INFO=%d\nflags & ADJUST_GRAY=%d\n",
            flags & PRINT_INFO, flags & ADJUST_GRAY);
    fprintf(stderr, "flags & ABSOLUTE_THRESHOLD=%d\n",flags&ABSOLUTE_THRESHOLD);
    fprintf(stderr, "flags & DO_ITERATIVE_THRESHOLD=%d\n",
                    flags & DO_ITERATIVE_THRESHOLD);
    fprintf(stderr, "need_pixels = %d\n", need_pixels);
    fprintf(stderr, "ignore_pixels = %d\n", ignore_pixels);
    fprintf(stderr, "number_of_digits = %d\n", number_of_digits);
    fprintf(stderr, "foreground = %d (%s)\n", ssocr_foreground,
                    (ssocr_foreground == SSOCR_BLACK) ? "black" : "white");
    fprintf(stderr, "background = %d (%s)\n", ssocr_background,
                    (ssocr_background == SSOCR_BLACK) ? "black" : "white");
    fprintf(stderr, "luminance  = ");
    print_lum_key(lt, stderr); fprintf(stderr, "\n");
    fprintf(stderr, "height/width threshold = %d\n", one_ratio);
    fprintf(stderr, "optind=%d argc=%d\n", optind, argc);
    fprintf(stderr, "================================================================================\n");
  }

  /* if no argument left exit the program */
  if(optind >= argc) {
    fprintf(stderr, "error: no image filename given\n");
    usage(argv[0], stderr);
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
    fprintf(stderr, "could not load image %s\n", imgfile);
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
    fprintf(stderr, "%.2f <= lum <= %.2f (lum should be in [0,255])\n",
                    get_minval(&image, 0, 0, -1, -1, lt),
                    get_maxval(&image, 0, 0, -1, -1, lt));
  }

  /* adapt threshold to image */
  thresh = adapt_threshold(&image, thresh, lt, 0, 0, -1, -1, flags);

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
        if(flags & VERBOSE) fputs(" processing dilation\n", stderr);
        new_image = dilation(&image, thresh, lt);
        imlib_context_set_image(image);
        imlib_free_image();
        image = new_image;
      } else if(strcasecmp("erosion",argv[i]) == 0) {
        if(flags & VERBOSE) fputs(" processing erosion\n", stderr);
        new_image = erosion(&image, thresh, lt);
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
          new_image = opening(&image, thresh, lt, n);
        } else {
          if(flags & VERBOSE) fputs(" processing opening (1)\n", stderr);
          new_image = opening(&image, thresh, lt, 1);
        }
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
          new_image = closing(&image, thresh, lt, n);
        } else {
          if(flags & VERBOSE) fputs(" processing closing (1)\n", stderr);
          new_image = closing(&image, thresh, lt, 1);
        }
        imlib_context_set_image(image);
        imlib_free_image();
        image = new_image;
      } else if(strcasecmp("remove_isolated",argv[i]) == 0) {
        if(flags & VERBOSE) fputs(" processing remove_isolated\n", stderr);
        new_image = remove_isolated(&image, thresh, lt);
        imlib_context_set_image(image);
        imlib_free_image();
        image = new_image;
      } else if(strcasecmp("make_mono",argv[i]) == 0) {
        if(flags & VERBOSE) fputs(" processing make_mono\n", stderr);
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
          new_image = white_border(&image, bdwidth);
          i++;
        } else {
          if(flags & VERBOSE)
            fputs(" processing white_border (1)\n", stderr);
          new_image = white_border(&image, 1);
        }
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
          new_image = shear(&image, offset);
          imlib_context_set_image(image);
          imlib_free_image();
          image = new_image;
        } else {
          fprintf(stderr, "error: shear command needs an argument\n");
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
          new_image = set_pixels_filter(&image, thresh, lt, mask);
          imlib_context_set_image(image);
          imlib_free_image();
          image = new_image;
        } else {
          fprintf(stderr,
              "error: set_pixels_filter command needs an argument\n");
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
          new_image = keep_pixels_filter(&image, thresh, lt, mask);
          imlib_context_set_image(image);
          imlib_free_image();
          image = new_image;
        } else {
          fprintf(stderr,
              "error: keep_pixels_filter command needs an argument\n");
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
          new_image = dynamic_threshold(&image, thresh, lt, ww, wh);
          imlib_context_set_image(image);
          imlib_free_image();
          image = new_image;
        } else {
          fprintf(stderr,
              "error: dynamic_threshold command needs two arguments\n");
          exit(99);
        }
      } else if(strcasecmp("rgb_threshold",argv[i]) == 0) {
        if(flags & VERBOSE) fputs(" processing rgb_threshold\n", stderr);
        new_image = make_mono(&image, thresh, MINIMUM);
        imlib_context_set_image(image);
        imlib_free_image();
        image = new_image;
      } else if(strcasecmp("r_threshold",argv[i]) == 0) {
        if(flags & VERBOSE) fputs(" processing r_threshold\n", stderr);
        new_image = make_mono(&image, thresh, RED);
        imlib_context_set_image(image);
        imlib_free_image();
        image = new_image;
      } else if(strcasecmp("g_threshold",argv[i]) == 0) {
        if(flags & VERBOSE) fputs(" processing g_threshold\n", stderr);
        new_image = make_mono(&image, thresh, GREEN);
        imlib_context_set_image(image);
        imlib_free_image();
        image = new_image;
      } else if(strcasecmp("b_threshold",argv[i]) == 0) {
        if(flags & VERBOSE) fputs(" processing b_threshold\n", stderr);
        new_image = make_mono(&image, thresh, BLUE);
        imlib_context_set_image(image);
        imlib_free_image();
        image = new_image;
      } else if(strcasecmp("invert",argv[i]) == 0) {
        if(flags & VERBOSE) fputs(" processing invert\n", stderr);
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
            min = get_minval(&image, 0, 0, -1, -1, lt);
            max = get_maxval(&image, 0, 0, -1, -1, lt);
            t1 = min + t1/100.0 * (max - min);
            t2 = min + t2/100.0 * (max - min);
            if(flags & VERBOSE) {
              fprintf(stderr, " adjusted to T1=%.2f and T2=%.2f\n", t1, t2);
            }
          }
          i+=2; /* skip the arguments to gray_stretch */
          new_image = gray_stretch(&image, t1, t2, lt);
          imlib_context_set_image(image);
          imlib_free_image();
          image = new_image;
        } else {
          fprintf(stderr,
              "error: gray_stretch command needs two arguments\n");
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
          int x, y, cw, ch; /* lw = crop width, lh = crop height */
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
          imlib_context_set_image(image);
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
            fprintf(stderr, "  %.2f <= lum <= %.2f in cropped image"
                            " (lum should be in [0,255])\n",
                            get_minval(&image, 0, 0, -1, -1, lt),
                            get_maxval(&image, 0, 0, -1, -1, lt));
          }
          /* adapt threshold to cropped image */
          thresh = adapt_threshold(&image, thresh, lt, 0, 0, -1, -1, flags);
        } else {
          fprintf(stderr, "error: crop command needs 4 arguments\n");
          exit(99);
        }
      } else if(strcasecmp("rotate",argv[i]) == 0) {
        if(flags & VERBOSE) {
          fprintf(stderr, " processing rotate %f", atof(argv[i+1]));
          if(flags & DEBUG_OUTPUT) {
            fprintf(stderr, " (from string %s)", argv[i+1]);
          }
          fprintf(stderr, "\n");
        }
        if(i+1<argc-1) {
          theta = atof(argv[++i]); /* sideeffect: increment i */
          new_image = rotate(&image, theta);
          imlib_context_set_image(image);
          imlib_free_image();
          image = new_image;
        } else {
          fprintf(stderr, "error: rotate command needs an argument\n");
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

  if(flags & USE_DEBUG_IMAGE) {
    /* copy processed image to debug image */
    debug_image = imlib_clone_image();
  }

  /* allocate memory for seven segment digits */
  if(number_of_digits > -1) {
    if(!(digits = calloc(number_of_digits, sizeof(digit_struct)))) {
      perror("digits = calloc()");
      exit(99);
    }
  } else {
    if(!(digits = calloc(1, sizeof(digit_struct)))) {
      perror("digits = calloc()");
      exit(99);
    }
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
          /* 1 dark pixels darken the whole column */
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
      if((number_of_digits > -1) && (d >= number_of_digits)) {
        fprintf(stderr, "found too many digits (%d)\n", d+1);
        imlib_free_image_and_decache();
        if(flags & USE_DEBUG_IMAGE) {
          save_image("debug", debug_image, output_fmt,debug_image_file,flags);
          imlib_context_set_image(debug_image);
          imlib_free_image_and_decache();
        }
        exit(1);
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
      digits[d].x2 = i;
      digits[d].y2 = h-1;
      d++;
      if(flags & USE_DEBUG_IMAGE) {
        imlib_context_set_image(debug_image);
        imlib_context_set_color(0,0,255,255); /* blue line for end of digit */
        imlib_image_draw_line(i,0,i,h-1,0);
        imlib_context_set_image(image);
      }
      /* if number of digits is not known, add memory for another digit */
      if(!(digits = realloc(digits, (d+1) * sizeof(digit_struct)))) {
        perror("digits = realloc()");
        exit(99);
      }
      /* initialize additional memory */
      memset(&digits[d], 0, sizeof(digit_struct));
      state = (ssocr_foreground == SSOCR_BLACK) ? FIND_DARK : FIND_LIGHT;
    }
  }

  /* after the loop above the program should be in state FIND_DARK,
   * i.e. after the last digit some light was found
   * if it is still searching for light end the digit at the border of the
   * image */
  if(state == (ssocr_foreground == SSOCR_BLACK) ? FIND_LIGHT : FIND_DARK) {
    digits[d].x2 = w-1;
    digits[d].y2 = h-1;
    d++;
    state = (ssocr_foreground == SSOCR_BLACK) ? FIND_DARK : FIND_LIGHT;
  }
  if((number_of_digits > -1) && (d != number_of_digits)) {
    fprintf(stderr, "found only %d of %d digits\n", d, number_of_digits);
    imlib_free_image_and_decache();
    if(flags & USE_DEBUG_IMAGE) {
      save_image("debug", debug_image, output_fmt, debug_image_file, flags);
      imlib_context_set_image(debug_image);
      imlib_free_image_and_decache();
    }
    exit(1);
  } else if(number_of_digits == -1) {
    number_of_digits = d;
    if(flags & DEBUG_OUTPUT) {
      fprintf(stderr, "auto detecting number of digits: %d\n", d);
    }
  }
  dig_w = digits[number_of_digits-1].x2 - digits[0].x1;
  dig_h = digits[number_of_digits-1].y2 - digits[0].y1;

  /* find upper and lower boundaries of every digit */
  for(d=0; d<number_of_digits; d++) {
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
  if(flags & USE_DEBUG_IMAGE) {
  /* draw rectangles around digits */
    imlib_context_set_image(debug_image);
    imlib_context_set_color(128,128,128,255); /* gray line */
    for(d=0; d<number_of_digits; d++) {
      imlib_image_draw_rectangle(digits[d].x1, digits[d].y1,
          digits[d].x2-digits[d].x1, digits[d].y2-digits[d].y1);
    }
    imlib_context_set_image(image);
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
    fprintf(stderr, "found %d digits\n", d);
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
  for(i=0; i<number_of_digits; i++) {
    /* skip digits with zero width */
    if(digits[i].x1 == digits[i].x2) continue;
    /* if width of digit is less than 1/one_ratio of its height it is a 1
     * (the default 1/3 is arbitarily chosen -- normally seven segment
     * displays use digits that are 2 times as high as wide) */
    if((digits[i].y2-digits[i].y1)/(digits[i].x2-digits[i].x1) > one_ratio) {
      if(flags & DEBUG_OUTPUT) {
        fprintf(stderr, "digit %d is a 1 (height/width = %d/%d = (int) %d)\n",
               i, digits[i].y2 - digits[i].y1, digits[i].x2 - digits[i].x1,
               (digits[i].y2 - digits[i].y1) / (digits[i].x2 - digits[i].x1));
      }
      digits[i].digit = D_ONE;
    }
  }

  /* identify a decimal point (or thousands separator) by relative size */
  for(d=0; d<number_of_digits; d++) {
    /* skip digits with zero width */
    if((digits[d].x1 == digits[d].x2) || (digits[d].y1 == digits[d].y2))
      continue;
    /* if height of a digit is less than 1/5 of the maximum digit height,
     * and its width is less than 1/2 of the maximum digit width (the widest
     * digit might be a one), assume it is a decimal point */
    if((digits[d].digit == D_UNKNOWN) &&
       (max_dig_h / (digits[d].y2 - digits[d].y1) > 5) &&
       (max_dig_w / (digits[d].x2 - digits[d].x1) > 2)) {
      digits[d].digit = D_DECIMAL;
      if(flags & DEBUG_OUTPUT)
        fprintf(stderr, "digit %d is a decimal point\n", d);
    }
  }

  /* now the digits are located and they have to be identified */
  /* iterate over digits */
  for(d=0; d<number_of_digits; d++) {
    int middle=0, quarter=0, three_quarters=0; /* scanlines */
    int d_height=0; /* height of digit */
    /* if digits[d].digit == D_ONE do nothing */
    if(digits[d].digit == D_UNKNOWN) {
      int third=1; /* in which third we are */
      int half;
      found_pixels=0; /* how many pixels are already found */
      d_height = digits[d].y2 - digits[d].y1;
      /* check horizontal segments */
      /* vertical scan at x == middle */
      middle = (digits[d].x1 + digits[d].x2) / 2;
      for(j=digits[d].y1; j<=digits[d].y2; j++) {
        imlib_image_query_pixel(middle, j, &color);
        lum = get_lum(&color, lt);
        if(is_pixel_set(lum, thresh)) /* dark i.e. pixel is set */ {
          if(flags & USE_DEBUG_IMAGE) {
            imlib_context_set_image(debug_image);
            if(third == 1) {
              imlib_context_set_color(255,0,0,255);
            } else if(third == 2) {
              imlib_context_set_color(0,255,0,255);
            } else if(third == 3) {
              imlib_context_set_color(0,0,255,255);
            }
            imlib_image_draw_pixel(middle,j,0);
            imlib_context_set_image(image);
          }
          found_pixels++;
        }
        /* pixels in first third count towards upper segment */
        if(j >= digits[d].y1 + d_height/3 && third == 1) {
          if(found_pixels >= need_pixels) {
            digits[d].digit |= HORIZ_UP; /* add upper segment */
          }
          found_pixels = 0;
          third++;
        } else if(j >= digits[d].y1 + 2*d_height/3 && third == 2) {
        /* pixels in second third count towards middle segment */
          if(found_pixels >= need_pixels) {
            digits[d].digit |= HORIZ_MID; /* add middle segment */
          }
          found_pixels = 0;
          third++;
        }
      }
      /* found_pixels contains pixels of last third */
      if(found_pixels >= need_pixels) {
        digits[d].digit |= HORIZ_DOWN; /* add lower segment */
      }
      found_pixels = 0;
      /* check upper vertical segments */
      half=1; /* in which half we are */
      quarter = digits[d].y1 + (digits[d].y2 - digits[d].y1) / 4;
      for(i=digits[d].x1; i<=digits[d].x2; i++) {
        imlib_image_query_pixel(i, quarter, &color);
        lum = get_lum(&color, lt);
        if(is_pixel_set(lum, thresh)) /* dark i.e. pixel is set */ {
          if(flags & USE_DEBUG_IMAGE) {
            if(half == 1) {
              imlib_context_set_color(255,0,0,255);
            } else if(half == 2) {
              imlib_context_set_color(0,255,0,255);
            }
            imlib_context_set_image(debug_image);
            imlib_image_draw_pixel(i,quarter,0);
            imlib_context_set_image(image);
          }
          found_pixels++;
        }
        if(i >= middle && half == 1) {
          if(found_pixels >= need_pixels) {
            digits[d].digit |= VERT_LEFT_UP;
          }
          found_pixels = 0;
          half++;
        }
      }
      if(found_pixels >= need_pixels) {
        digits[d].digit |= VERT_RIGHT_UP;
      }
      found_pixels = 0;
      half = 1;
      /* check lower vertical segments */
      half=1; /* in which half we are */
      three_quarters = digits[d].y1 + 3 * (digits[d].y2 - digits[d].y1) / 4;
      for(i=digits[d].x1; i<=digits[d].x2; i++) {
        imlib_image_query_pixel(i, three_quarters, &color);
        lum = get_lum(&color, lt);
        if(is_pixel_set(lum, thresh)) /* dark i.e. pixel is set */ {
          if(flags & USE_DEBUG_IMAGE) {
            if(half == 1) {
              imlib_context_set_color(255,0,0,255);
            } else if(half == 2) {
              imlib_context_set_color(0,255,0,255);
            }
            imlib_context_set_image(debug_image);
            imlib_image_draw_pixel(i,three_quarters,0);
            imlib_context_set_image(image);
          }
          found_pixels++;
        }
        if(i >= middle && half == 1) {
          if(found_pixels >= need_pixels) {
            digits[d].digit |= VERT_LEFT_DOWN;
          }
          found_pixels = 0;
          half++;
        }
      }
      if(found_pixels >= need_pixels) {
        digits[d].digit |= VERT_RIGHT_DOWN;
      }
      found_pixels = 0;
    }
  }

  /* print found segments as ASCII art if debug output is enabled
   * example digits known by ssocr:
   *   _      _  _       _   _  _   _   _   _
   *  | |  |  _| _| |_| |_  |_   | | | |_| |_|
   *  |_|  | |_  _|   |  _| |_|  |   | |_|  _|
  */
  if(flags & DEBUG_OUTPUT) {
    fputs("Display as seen by ssocr:\n", stderr);
    /* top row */
    for(i=0; i<number_of_digits; i++) {
      fputc(' ', stderr);
      fputc(' ', stderr);
      digits[i].digit & HORIZ_UP ? fputc('_', stderr) : fputc(' ', stderr);
      fputc(' ', stderr);
    }
    fputc('\n', stderr);
    /* middle row */
    for(i=0; i<number_of_digits; i++) {
      fputc(' ', stderr);
      digits[i].digit & VERT_LEFT_UP ? fputc('|', stderr) : fputc(' ', stderr);
      digits[i].digit & HORIZ_MID ? fputc('_', stderr) : fputc(' ', stderr);
      digits[i].digit & VERT_RIGHT_UP ? fputc('|', stderr) : fputc(' ', stderr);
    }
    fputc('\n', stderr);
    /* bottom row */
    for(i=0; i<number_of_digits; i++) {
      fputc(' ', stderr);
      digits[i].digit&VERT_LEFT_DOWN ? fputc('|', stderr) : fputc(' ', stderr);
      digits[i].digit&HORIZ_DOWN ? fputc('_', stderr) : 
        digits[i].digit == D_DECIMAL ? fputc('.', stderr) : fputc(' ', stderr);
      digits[i].digit&VERT_RIGHT_DOWN ? fputc('|', stderr) : fputc(' ', stderr);
    }
    fputs("\n\n", stderr);
  }

  /* print digits */
  for(i=0; i<number_of_digits; i++) {
    switch(digits[i].digit) {
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
      case D_DECIMAL: putchar('.'); break;
      case D_HEX_A: putchar('a'); break;
      case D_HEX_b: putchar('b'); break;
      case D_HEX_C: /* fallthrough */
      case D_HEX_c: putchar('c'); break;
      case D_HEX_d: putchar('d'); break;
      case D_HEX_E: putchar('e'); break;
      case D_HEX_F: putchar('f'); break;
      /* finding a digit with no set segments is not supposed to happen */
      case D_UNKNOWN: putchar(' '); unknown_digit++; break;
      default: putchar('_'); unknown_digit++; break;
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
