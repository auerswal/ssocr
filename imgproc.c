/* Seven Segment Optical Character Recognition Image Processing Functions */

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
#include <string.h>         /* strcpy */

/* sin, cos */
#include <math.h>

/* my headers */
#include "defines.h"        /* defines */
#include "imgproc.h"        /* image processing */
#include "help.h"           /* online help */

/* global variables */
extern int ssocr_foreground;
extern int ssocr_background;

/* functions */

/*** image processing ***/

/* set foreground color */
void set_fg_color(int color)
{
  ssocr_foreground = color;
}

/* set background color */
void set_bg_color(int color)
{
  ssocr_background = color;
}

/* set imlib color */
void ssocr_set_color(fg_bg_t color)
{
  switch(color) {
    case FG:
      imlib_context_set_color(ssocr_foreground, ssocr_foreground,
                              ssocr_foreground, 255);
      break;
    case BG:
      imlib_context_set_color(ssocr_background, ssocr_background,
                              ssocr_background, 255);
      break;
    default:
      fprintf(stderr, "error: ssocr_set_color(): unknown color %d\n",
          color);
      exit(99);
      break;
  }
}

/* draw a fore- or background pixel */
void draw_pixel(Imlib_Image *image, int x, int y, fg_bg_t color)
{
  Imlib_Image *current_image; /* save current image */

  current_image = imlib_context_get_image();
  imlib_context_set_image(image);
  ssocr_set_color(color);
  imlib_image_draw_pixel(x,y,0);
  imlib_context_set_image(current_image);
}

/* draw a foreground pixel */
void draw_fg_pixel(Imlib_Image *image, int x, int y)
{
  draw_pixel(image, x, y, FG);
}

/* draw a background pixel */
void draw_bg_pixel(Imlib_Image *image, int x, int y)
{
  draw_pixel(image, x, y, BG);
}

/* check if a pixel is set regarding current foreground/background colors */
int is_pixel_set(int value, double threshold)
{
  switch(ssocr_foreground) {
    case SSOCR_BLACK:
      if(value < threshold/100.0*MAXRGB) {
        return 1;
      } else {
        return 0;
      }
      break;
    case SSOCR_WHITE:
      if(value >= threshold/100.0*MAXRGB) {
        return 1;
      } else {
        return 0;
      }
      break;
    default:
      fprintf(stderr, "error: is_pixel_set(): foreground color neither black"
                      " nor white\n");
      exit(99);
      break;
  }
}

/* set pixels that have at least mask pixels around it set (including the
 * examined pixel itself) to black (foreground), all other pixels to white
 * (background) */
Imlib_Image set_pixels_filter(Imlib_Image *source_image, double thresh,
                              luminance_t lt, int mask)
{
  Imlib_Image new_image; /* construct filtered image here */
  Imlib_Image current_image; /* save image pointer */
  int height, width; /* image dimensions */
  int x,y,i,j; /* iteration variables */
  int set_pixel; /* should  pixel be set or not? */
  Imlib_Color color;
  int lum; /* luminance value of pixel */

  /* save pointer to current image */
  current_image = imlib_context_get_image();

  /* create a new image */
  imlib_context_set_image(*source_image);
  height = imlib_image_get_height();
  width = imlib_image_get_width();
  new_image = imlib_clone_image();

  /* check for every pixel if it should be set in filtered image */
  for(x=0; x<width; x++) {
    for(y=0; y<height; y++) {
      set_pixel=0;
      for(i=x-1; i<=x+1; i++) {
        for(j=y-1; j<=y+1; j++) {
          if(i>=0 && i<width && j>=0 && j<height) { /* i,j inside image? */
            imlib_image_query_pixel(i, j, &color);
            lum = get_lum(&color, lt);
            if(is_pixel_set(lum, thresh)) {
              set_pixel++;
            }
          }
        }
      }
      /* set pixel if at least mask pixels around it are set */
      if(set_pixel >= mask) {
        draw_fg_pixel(new_image, x, y);
      } else {
        draw_bg_pixel(new_image, x, y);
      }
    }
  }

  /* restore image from before function call */
  imlib_context_set_image(current_image);

  /* return filtered image */
  return new_image;
}

Imlib_Image dilation(Imlib_Image *source_image, double thresh, luminance_t lt)
{
  return set_pixels_filter(source_image, thresh, lt, 1);
}

Imlib_Image erosion(Imlib_Image *source_image, double thresh, luminance_t lt)
{
  return set_pixels_filter(source_image, thresh, lt, 9);
}

Imlib_Image closing(Imlib_Image *source_image,double thresh,luminance_t lt,int n)
{
  int i;
  Imlib_Image temp_image1, temp_image2;
  /* dilation n times */
  imlib_context_set_image(*source_image);
  temp_image1 = temp_image2 = imlib_clone_image();
  for(i=0; i<n; i++) {
    temp_image2 = dilation(&temp_image1, thresh, lt);
    imlib_context_set_image(temp_image1);
    imlib_free_image();
    temp_image1 = temp_image2;
  }
  /* erosion n times */
  for(i=0; i<n; i++) {
    temp_image2 = erosion(&temp_image1, thresh, lt);
    imlib_context_set_image(temp_image1);
    imlib_free_image();
    temp_image1 = temp_image2;
  }
  return temp_image2;
}

Imlib_Image opening(Imlib_Image *source_image,double thresh,luminance_t lt,int n)
{
  int i;
  Imlib_Image temp_image1, temp_image2;
  /* erosion n times */
  imlib_context_set_image(*source_image);
  temp_image1 = temp_image2 = imlib_clone_image();
  for(i=0; i<n; i++) {
    temp_image2 = erosion(&temp_image1, thresh, lt);
    imlib_context_set_image(temp_image1);
    imlib_free_image();
    temp_image1 = temp_image2;
  }
  /* dilation n times */
  for(i=0; i<n; i++) {
    temp_image2 = dilation(&temp_image1, thresh, lt);
    imlib_context_set_image(temp_image1);
    imlib_free_image();
    temp_image1 = temp_image2;
  }
  return temp_image2;
}

/* set pixels with (brightness) value lower than threshold that have more than
 * mask pixels around it set (including the examined pixel itself) to black
 * (foreground), set pixels with (brightness) value lower than threshold that
 * less or equal pixels around it set to white (background) */
Imlib_Image keep_pixels_filter(Imlib_Image *source_image, double thresh,
                               luminance_t lt, int mask)
{
  Imlib_Image new_image; /* construct filtered image here */
  Imlib_Image current_image; /* save image pointer */
  int height, width; /* image dimensions */
  int x,y,i,j; /* iteration variables */
  int set_pixel; /* should  pixel be set or not? */
  Imlib_Color color;
  int lum; /* luminance value of pixel */

  /* save pointer to current image */
  current_image = imlib_context_get_image();

  /* create a new image */
  imlib_context_set_image(*source_image);
  height = imlib_image_get_height();
  width = imlib_image_get_width();
  new_image = imlib_clone_image();

  /* draw white (background) rectangle to clear new image */
  imlib_context_set_image(new_image);
  ssocr_set_color(BG);
  imlib_image_draw_rectangle(0, 0, width, height);
  imlib_context_set_image(*source_image);

  /* check for every pixel if it should be set in filtered image */
  for(x=0; x<width; x++) {
    for(y=0; y<height; y++) {
      set_pixel=0;
      imlib_image_query_pixel(x, y, &color);
      lum = get_lum(&color, lt);
      if(is_pixel_set(lum, thresh)) { /* only test neighbors of set pixels */
        for(i=x-1; i<=x+1; i++) {
          for(j=y-1; j<=y+1; j++) {
            if(i>=0 && i<width && j>=0 && j<height) { /* i,j inside image? */
              imlib_image_query_pixel(i, j, &color);
              lum = get_lum(&color, lt);
              if(is_pixel_set(lum, thresh)) {
                set_pixel++;
              }
            }
          }
        }
      }
      /* set pixel if at least mask pixels around it are set */
      /* mask = 1 keeps all pixels */
      if(set_pixel > mask) {
        draw_fg_pixel(new_image, x, y);
      } else {
        draw_bg_pixel(new_image, x, y);
      }
    }
  }

  /* restore image from before function call */
  imlib_context_set_image(current_image);

  /* return filtered image */
  return new_image;
}

Imlib_Image remove_isolated(Imlib_Image *source_image, double thresh,
                            luminance_t lt)
{
  return keep_pixels_filter(source_image, thresh, lt, 1);
}

/* gray stretching, i.e. lum<t1 => lum=0, lum>t2 => lum=100,
 * else lum=((lum-t1)*MAXRGB)/(t2-t1) */
Imlib_Image gray_stretch(Imlib_Image *source_image, double t1, double t2,
                         luminance_t lt)
{
  Imlib_Image new_image; /* construct filtered image here */
  Imlib_Image current_image; /* save image pointer */
  int height, width; /* image dimensions */
  int x,y; /* iteration variables */
  Imlib_Color color;
  int lum; /* luminance value of pixel */

  /* do nothing if t1>=t2 */
  if(t1 >= t2) {
    fprintf(stderr, "error: gray_stretch(): t1=%.2f >= t2=%.2f\n", t1, t2);
    exit(99);
  }

  /* check if 0 < t1,t2 < MAXRGB */
  if(t1 <= 0.0) {
    fprintf(stderr, "error: gray_stretch(): t1=%.2f <= 0.0\n", t1);
    exit(99);
  }
  if(t2 >= MAXRGB) {
    fprintf(stderr, "error: gray_stretch(): t2=%.2f >= %d.0\n", t2, MAXRGB);
    exit(99);
  }

  /* save pointer to current image */
  current_image = imlib_context_get_image();

  /* create a new image */
  imlib_context_set_image(*source_image);
  height = imlib_image_get_height();
  width = imlib_image_get_width();
  new_image = imlib_clone_image();

  /* gray stretch image */
  for(x=0; x<width; x++) {
    for(y=0; y<height; y++) {
      imlib_image_query_pixel(x, y, &color);
      lum = get_lum(&color, lt);
      imlib_context_set_image(new_image);
      if(lum<=t1) {
        imlib_context_set_color(0, 0, 0, color.alpha);
      } else if(lum>=t2) {
        imlib_context_set_color(MAXRGB, MAXRGB, MAXRGB, color.alpha);
      } else {
        imlib_context_set_color(clip(((lum-t1)*255)/(t2-t1),0,255),
                                clip(((lum-t1)*255)/(t2-t1),0,255),
                                clip(((lum-t1)*255)/(t2-t1),0,255),
                                color.alpha);
      }
      imlib_image_draw_pixel(x, y, 0);
      imlib_context_set_image(*source_image);
    }
  }

  /* restore image from before function call */
  imlib_context_set_image(current_image);

  /* return filtered image */
  return new_image;
}

/* use dynamic (aka adaptive) local thresholding to create monochrome image */
/* ww and wh are the width and height of the rectangle used to find the
 * threshold value */
Imlib_Image dynamic_threshold(Imlib_Image *source_image,double t,luminance_t lt,
                              int ww, int wh)
{
  Imlib_Image new_image; /* construct filtered image here */
  Imlib_Image current_image; /* save image pointer */
  int height, width; /* image dimensions */
  int x,y; /* iteration variables */
  Imlib_Color color;
  int lum;
  double thresh;

  /* save pointer to current image */
  current_image = imlib_context_get_image();

  /* create a new image */
  imlib_context_set_image(*source_image);
  height = imlib_image_get_height();
  width = imlib_image_get_width();
  new_image = imlib_clone_image();

  /* check for every pixel if it should be set in filtered image */
  for(x=0; x<width; x++) {
    for(y=0; y<height; y++) {
      imlib_image_query_pixel(x, y, &color);
      lum = get_lum(&color, lt);
      thresh = get_threshold(source_image, t/100.0, lt, x-ww/2, y-ww/2, ww, wh);
      if(is_pixel_set(lum, thresh)) {
        draw_fg_pixel(new_image, x, y);
      } else {
        draw_bg_pixel(new_image, x, y);
      }
    }
  }

  /* restore image from before function call */
  imlib_context_set_image(current_image);

  /* return filtered image */
  return new_image;
}

/* use simple thresholding to generate monochrome image */
Imlib_Image make_mono(Imlib_Image *source_image, double thresh, luminance_t lt)
{
  Imlib_Image new_image; /* construct filtered image here */
  Imlib_Image current_image; /* save image pointer */
  int height, width; /* image dimensions */
  int x,y; /* iteration variables */
  Imlib_Color color;
  int lum; /* pixel luminance */

  /* save pointer to current image */
  current_image = imlib_context_get_image();

  /* create a new image */
  imlib_context_set_image(*source_image);
  height = imlib_image_get_height();
  width = imlib_image_get_width();
  new_image = imlib_clone_image();

  /* check for every pixel if it should be set in filtered image */
  for(x=0; x<width; x++) {
    for(y=0; y<height; y++) {
      imlib_image_query_pixel(x, y, &color);
      lum = get_lum(&color, lt);
      if(is_pixel_set(lum, thresh)) {
        draw_fg_pixel(new_image, x, y);
      } else {
        draw_bg_pixel(new_image, x, y);
      }
    }
  }

  /* restore image from before function call */
  imlib_context_set_image(current_image);

  /* return filtered image */
  return new_image;
}

/* adapt threshold to image values values */
double adapt_threshold(Imlib_Image *image, double thresh, luminance_t lt, int x,
                       int y, int w, int h, int flags)
{
  double t = thresh;
  if(!(flags & ABSOLUTE_THRESHOLD)) {
    if(flags & DEBUG_OUTPUT)
      fprintf(stderr, "adjusting threshold to image: %f ->", t);
    t = get_threshold(image, thresh/100.0, lt, x, y, w, h);
    if(flags & DEBUG_OUTPUT)
      fprintf(stderr, " %f\n", t);
    if(flags & DO_ITERATIVE_THRESHOLD) {
      if(flags & DEBUG_OUTPUT)
        fprintf(stderr, "doing iterative_thresholding: %f ->", t);
      t = iterative_threshold(image, t, lt, x, y, w, h);
      if(flags & DEBUG_OUTPUT)
        fprintf(stderr, " %f\n", t);
    }
  }
  if((flags & VERBOSE) || (flags & DEBUG_OUTPUT)) {
    fprintf(stderr, "using threshold %.2f\n", t);
  }
  return t;
}

/* compute dynamic threshold value from the rectangle (x,y),(x+w,y+h) of
 * source_image */
double get_threshold(Imlib_Image *source_image, double fraction, luminance_t lt,
                    int x, int y, int w, int h)
{
  Imlib_Image current_image; /* save image pointer */
  int height, width; /* image dimensions */
  int xi,yi; /* iteration variables */
  Imlib_Color color;
  int lum; /* luminance of pixel */
  double minval=(double)MAXRGB, maxval=0.0;

  /* save pointer to current image */
  current_image = imlib_context_get_image();

  /* get image dimensions */
  imlib_context_set_image(*source_image);
  height = imlib_image_get_height();
  width = imlib_image_get_width();

  /* special value -1 for width or height means image width/height */
  if(w == -1) w = width;
  if(h == -1) h = width;

  /* assure valid coordinates */
  if(x+w > width) x = width-w;
  if(y+h > height) y = height-h;
  if(x<0) x=0;
  if(y<0) y=0;

  /* find the threshold value to differentiate between dark and light */
  for(xi=0; (xi<w) && (xi<width); xi++) {
    for(yi=0; (yi<h) && (yi<height); yi++) {
      imlib_image_query_pixel(xi, yi, &color);
      lum = get_lum(&color, lt);
      if(lum < minval) minval = lum;
      if(lum > maxval) maxval = lum;
    }
  }

  /* restore image from before function call */
  imlib_context_set_image(current_image);

  return (minval + fraction * (maxval - minval)) * 100 / MAXRGB;
}

/* determine threshold by an iterative method */
double iterative_threshold(Imlib_Image *source_image, double thresh,
                           luminance_t lt, int x, int y, int w, int h)
{
  Imlib_Image current_image; /* save image pointer */
  int height, width; /* image dimensions */
  int xi,yi; /* iteration variables */
  Imlib_Color color;
  int lum; /* luminance of pixel */
  unsigned int size_white, size_black; /* size of black and white groups */
  unsigned long int sum_white, sum_black; /* sum of black and white groups */
  unsigned int avg_white, avg_black; /* average values of black and white */
  double old_thresh; /* old threshold computed by last iteration step */
  double new_thresh; /* new threshold computed by current iteration step */
  int thresh_lum; /* luminance value of threshold */

  /* normalize threshold (was given as a percentage) */
  new_thresh = thresh / 100.0;

  /* save pointer to current image */
  current_image = imlib_context_get_image();

  /* get image dimensions */
  imlib_context_set_image(*source_image);
  height = imlib_image_get_height();
  width = imlib_image_get_width();

  /* special value -1 for width or height means image width/height */
  if(w == -1) w = width;
  if(h == -1) h = width;

  /* assure valid coordinates */
  if(x+w > width) x = width-w;
  if(y+h > height) y = height-h;
  if(x<0) x=0;
  if(y<0) y=0;

  /* find the threshold value to differentiate between dark and light */
  do {
    thresh_lum = MAXRGB * new_thresh;
    old_thresh = new_thresh;
    size_black = sum_black = size_white = sum_white = 0;
    for(xi=0; (xi<w) && (xi<width); xi++) {
      for(yi=0; (yi<h) && (yi<height); yi++) {
        imlib_image_query_pixel(xi, yi, &color);
        lum = get_lum(&color, lt);
        if(lum <= thresh_lum) {
          size_black++;
          sum_black += lum;
        } else {
          size_white++;
          sum_white += lum;
        }
      }
    }
    if(!size_white) {
      fprintf(stderr, "iterative_threshold(): error: no white pixels\n");
      imlib_context_set_image(current_image);
      return thresh;
    }
    if(!size_black) {
      fprintf(stderr, "iterative_threshold(): error: no black pixels\n");
      imlib_context_set_image(current_image);
      return thresh;
    }
    avg_white = sum_white / size_white;
    avg_black = sum_black / size_black;
    new_thresh = (avg_white + avg_black) / (2.0 * MAXRGB);
    /*fprintf(stderr, "iterative_threshold(): new_thresh = %f\n", new_thresh);*/
  } while(fabs(new_thresh - old_thresh) > EPSILON);

  /* restore image from before function call */
  imlib_context_set_image(current_image);

  return new_thresh * 100;
}

/* get minimum lum value */
double get_minval(Imlib_Image *source_image, int x, int y, int w, int h,
                 luminance_t lt)
{
  Imlib_Image current_image; /* save image pointer */
  int height, width; /* image dimensions */
  int xi,yi; /* iteration variables */
  Imlib_Color color; /* Imlib2 RGBA color structure */
  int minval = MAXRGB;
  int lum = 0;

  /* save pointer to current image */
  current_image = imlib_context_get_image();

  /* get image dimensions */
  imlib_context_set_image(*source_image);
  height = imlib_image_get_height();
  width = imlib_image_get_width();

  /* special value -1 for width or height means image width/height */
  if(w == -1) w = width;
  if(h == -1) h = width;

  /* assure valid coordinates */
  if(x+w > width) x = width-w;
  if(y+h > height) y = height-h;
  if(x<0) x=0;
  if(y<0) y=0;

  /* find the minimum value in the image */
  for(xi=0; (xi<w) && (xi<width); xi++) {
    for(yi=0; (yi<h) && (yi<height); yi++) {
      imlib_image_query_pixel(xi, yi, &color);
      lum = clip(get_lum(&color, lt),0,255);
      if(lum < minval) minval = lum;
    }
  }

  /* restore image from before function call */
  imlib_context_set_image(current_image);

  return minval;
}

/* get maximum luminance value */
double get_maxval(Imlib_Image *source_image, int x, int y, int w, int h,
                 luminance_t lt)
{
  Imlib_Image current_image; /* save image pointer */
  int height, width; /* image dimensions */
  int xi,yi; /* iteration variables */
  Imlib_Color color; /* Imlib2 RGBA color structure */
  int lum = 0;
  int maxval = 0;

  /* save pointer to current image */
  current_image = imlib_context_get_image();

  /* get image dimensions */
  imlib_context_set_image(*source_image);
  height = imlib_image_get_height();
  width = imlib_image_get_width();

  /* special value -1 for width or height means image width/height */
  if(w == -1) w = width;
  if(h == -1) h = width;

  /* assure valid coordinates */
  if(x+w > width) x = width-w;
  if(y+h > height) y = height-h;
  if(x<0) x=0;
  if(y<0) y=0;

  /* find the minimum value in the image */
  for(xi=0; (xi<w) && (xi<width); xi++) {
    for(yi=0; (yi<h) && (yi<height); yi++) {
      imlib_image_query_pixel(xi, yi, &color);
      lum = clip(get_lum(&color, lt),0,255);
      if(lum > maxval) maxval = lum;
    }
  }

  /* restore image from before function call */
  imlib_context_set_image(current_image);

  return maxval;
}

/* draw a white (background) border around image, overwriting image contents
 * beneath border*/
Imlib_Image white_border(Imlib_Image *source_image, int bdwidth)
{
  Imlib_Image new_image; /* construct filtered image here */
  Imlib_Image current_image; /* save image pointer */
  int height, width; /* image dimensions */
  int x,y; /* coordinates of upper left corner of rectangles */

  /* save pointer to current image */
  current_image = imlib_context_get_image();

  /* create a new image */
  imlib_context_set_image(*source_image);
  height = imlib_image_get_height();
  width = imlib_image_get_width();
  new_image = imlib_clone_image();

  /* assure border width has a legal value */
  if(bdwidth > width/2) bdwidth = width/2;
  if(bdwidth > height/2) bdwidth = height/2;

  /* draw white (background) rectangle around new image */
  for(x=0, y=0; x<bdwidth; x++, y++) {
    imlib_context_set_image(new_image);
    ssocr_set_color(BG);
    imlib_image_draw_rectangle(x, y, width-2*x, height-2*y);
  }

  /* restore image from before function call */
  imlib_context_set_image(current_image);

  /* return filtered image */
  return new_image;
}

/* shear the image
 * the top line is unchanged
 * the bottom line is moved offset pixels to the right
 * the other lines are moved yPos*offset/(height-1) pixels to the right
 * white pixels are inserted at the left side */
Imlib_Image shear(Imlib_Image *source_image, int offset)
{
  Imlib_Image new_image; /* construct filtered image here */
  Imlib_Image current_image; /* save image pointer */
  int height, width; /* image dimensions */
  int x,y; /* iteration variables */
  int shift; /* current shift-width */
  Imlib_Color color_return; /* for imlib_query_pixel() */

  /* save pointer to current image */
  current_image = imlib_context_get_image();

  /* create a new image */
  imlib_context_set_image(*source_image);
  height = imlib_image_get_height();
  width = imlib_image_get_width();
  new_image = imlib_clone_image();

  /* move every line to the right */
  for(y=1; y<height; y++) {
    shift = y * offset / (height-1);
    /* copy pixels */
    for(x=width-1; x>=shift; x--) {
      imlib_image_query_pixel(x-shift, y, &color_return);
      imlib_context_set_image(new_image);
      imlib_context_set_color(color_return.red, color_return.green,
                              color_return.blue, color_return.alpha);
      imlib_image_draw_pixel(x,y,0);
      imlib_context_set_image(*source_image);
    }
    /* fill with background */
    for(x=0; x<shift; x++) {
      draw_bg_pixel(new_image, x, y);
    }
  }

  /* restore image from before function call */
  imlib_context_set_image(current_image);

  /* return filtered image */
  return new_image;
}

/* rotate the image */
Imlib_Image rotate(Imlib_Image *source_image, double theta)
{
  Imlib_Image new_image; /* construct filtered image here */
  Imlib_Image current_image; /* save image pointer */
  int height, width; /* image dimensions */
  int x,y; /* iteration variables / target coordinates */
  int sx,sy; /* source coordinates */
  Imlib_Color color_return; /* for imlib_query_pixel() */

  /* save pointer to current image */
  current_image = imlib_context_get_image();

  /* create a new image */
  imlib_context_set_image(*source_image);
  height = imlib_image_get_height();
  width = imlib_image_get_width();
  new_image = imlib_clone_image();

  /* convert theta from degrees to radians */
  theta = theta / 360 * 2.0 * M_PI;

  /* create rotated image
   * (some parts of the original image will be lost) */
  for(x = 0; x < width; x++) {
    for(y = 0; y < height; y++) {
      sx = (x-width/2) * cos(theta) + (y-height/2) * sin(theta) + width/2;
      sy = (y-height/2) * cos(theta) - (x-width/2) * sin(theta) + height/2;
      if((sx >= 0) && (sx <= width) && (sy >= 0) && (sy <= height)) {
        imlib_image_query_pixel(sx, sy, &color_return);
        imlib_context_set_image(new_image);
        imlib_context_set_color(color_return.red, color_return.green, color_return.blue, color_return.alpha);
      } else {
        imlib_context_set_image(new_image);
        ssocr_set_color(BG);
      }
      imlib_image_draw_pixel(x,y,0);
      imlib_context_set_image(*source_image);
    }
  }

  /* restore image from before function call */
  imlib_context_set_image(current_image);

  /* return filtered image */
  return new_image;
}

/* turn image to grayscale */
Imlib_Image grayscale(Imlib_Image *source_image, luminance_t lt)
{
  Imlib_Image new_image; /* construct grayscale image here */
  Imlib_Image current_image; /* save image pointer */
  int height, width; /* image dimensions */
  int x,y; /* iteration variables */
  Imlib_Color color; /* Imlib2 color structure */
  int lum=0;

  /* save pointer to current image */
  current_image = imlib_context_get_image();

  /* create a new image */
  imlib_context_set_image(*source_image);
  height = imlib_image_get_height();
  width = imlib_image_get_width();
  new_image = imlib_clone_image();

  /* transform image to grayscale */
  for(x=0; x<width; x++) {
    for(y=0; y<height; y++) {
      imlib_context_set_image(*source_image);
      imlib_image_query_pixel(x, y, &color);
      imlib_context_set_image(new_image);
      lum = clip(get_lum(&color, lt),0,255);
      imlib_context_set_color(lum, lum, lum, color.alpha);
      imlib_image_draw_pixel(x, y, 0);
    }
  }

  /* restore image from before function call */
  imlib_context_set_image(current_image);

  /* return filtered image */
  return new_image;
}

/* use simple thresholding to generate an inverted monochrome image */
Imlib_Image invert(Imlib_Image *source_image, double thresh, luminance_t lt)
{
  Imlib_Image new_image; /* construct filtered image here */
  Imlib_Image current_image; /* save image pointer */
  int height, width; /* image dimensions */
  int x,y; /* iteration variables */
  Imlib_Color color;
  int lum; /* pixel luminance */

  /* save pointer to current image */
  current_image = imlib_context_get_image();

  /* create a new image */
  imlib_context_set_image(*source_image);
  height = imlib_image_get_height();
  width = imlib_image_get_width();
  new_image = imlib_clone_image();

  /* check for every pixel if it should be set in filtered image */
  for(x=0; x<width; x++) {
    for(y=0; y<height; y++) {
      imlib_image_query_pixel(x, y, &color);
      lum = get_lum(&color, lt);
      if(is_pixel_set(lum, thresh)) {
        draw_bg_pixel(new_image, x, y);
      } else {
        draw_fg_pixel(new_image, x, y);
      }
    }
  }

  /* restore image from before function call */
  imlib_context_set_image(current_image);

  /* return filtered image */
  return new_image;
}

/* crop image */
Imlib_Image crop(Imlib_Image *source_image, int x, int y, int w, int h)
{
  Imlib_Image new_image; /* construct filtered image here */
  Imlib_Image current_image; /* save image pointer */
  int width, height; /* source image dimensions */

  /* save pointer to current image */
  current_image = imlib_context_get_image();

  /* get width and height of source image */
  imlib_context_set_image(*source_image);
  width = imlib_image_get_width();
  height = imlib_image_get_height();

  /* get sane values */
  if(x < 0) x = 0;
  if(y < 0) y = 0;
  if(x >= width) x = width - 1;
  if(y >= height) y = height - 1;
  if(x + w > width) w = width - x;
  if(y + h > height) h = height - x;

  /* create the new image */
  imlib_context_set_image(*source_image);
  new_image = imlib_create_cropped_image(x, y, w, h);

  /* restore image from before function call */
  imlib_context_set_image(current_image);

  /* return filtered image */
  return new_image;
}

/* compute luminance from RGB values */
int get_lum(Imlib_Color *color, luminance_t lt)
{
  switch(lt) {
    case REC709:  return get_lum_709(color);
    case REC601:  return get_lum_601(color);
    case LINEAR:  return get_lum_lin(color);
    case MINIMUM: return get_lum_min(color);
    case MAXIMUM: return get_lum_max(color);
    case RED:     return get_lum_red(color);
    case GREEN:   return get_lum_green(color);
    case BLUE:    return get_lum_blue(color);
    default:
      fprintf(stderr, "error: get_lum(): unknown transfer function no. %d\n",
              lt);
      exit(99);
  }
}

/* compute luminance Y_709 from linear RGB values */
int get_lum_709(Imlib_Color *color)
{
  return 0.2125*color->red + 0.7154*color->green + 0.0721*color->blue;
}

/* compute luminance Y_601 from gamma corrected (non-linear) RGB values */
int get_lum_601(Imlib_Color *color)
{
  return 0.299*color->red + 0.587*color->green + 0.114*color->blue;
}

/* compute luminance Y = (R+G+B)/3 */
int get_lum_lin(Imlib_Color *color)
{
  return (color->red + color->green + color->blue) / 3;
}

/* compute luminance Y = min(R,G,B) as used in GNU Ocrad 0.14 */
int get_lum_min(Imlib_Color *color)
{
  return (color->red < color->green) ?
           ((color->red < color->blue) ? color->red : color->blue) :
           ((color->green < color->blue) ? color->green : color->blue);
}

/* compute luminance Y = max(R,G,B) */
int get_lum_max(Imlib_Color *color)
{
  return (color->red > color->green) ?
           ((color->red > color->blue) ? color->red : color->blue) :
           ((color->green > color->blue) ? color->green : color->blue);
}

/* compute luminance Y = R */
int get_lum_red(Imlib_Color *color)
{
  return color->red;
}

/* compute luminance Y = G */
int get_lum_green(Imlib_Color *color)
{
  return color->green;
}

/* compute luminance Y = B */
int get_lum_blue(Imlib_Color *color)
{
  return color->blue;
}

/* clip value thus that it is in the given interval [min,max] */
int clip(int value, int min, int max)
{
  return (value < min) ? min : ((value > max) ? max : value);
}

/* save image to file */
void save_image(const char *image_type, Imlib_Image *image, const char *fmt,
                const char *filename, int flags)
{
  const char *tmp;
  Imlib_Image *current_image;
  Imlib_Load_Error save_error=0;
  const char *const stdout_file = "/proc/self/fd/1";

  current_image = imlib_context_get_image();
  imlib_context_set_image(image);

  /* interpret - as STDOUT */
  if(strcmp("-", filename) == 0)
    filename = stdout_file;
  /* get file format for image */
  if(fmt) { /* use provided format string */
    tmp = fmt;
  } else { /* use file name extension */
    tmp = strrchr(filename, '.');
    if(tmp)
      tmp++;
  }
  if(tmp) {
    if(flags & VERBOSE)
      fprintf(stderr, "using %s format for %s image\n", tmp, image_type);
    imlib_image_set_format(tmp);
  } else { /* use png as default */
    if(flags & VERBOSE)
      fprintf(stderr, "using png format for %s image\n", image_type);
    imlib_image_set_format("png");
  }
  /* write image to disk */
  if(flags & VERBOSE)
    fprintf(stderr, "writing %s image to file %s\n", image_type, filename);
  imlib_save_image_with_error_return(filename, &save_error);
  if(save_error && save_error != IMLIB_LOAD_ERROR_NONE) {
    fprintf(stderr, "error saving image file %s\n", filename);
    report_imlib_error(save_error);
  }
  imlib_context_set_image(current_image);
}

/* parse KEYWORD from --luminace option */
luminance_t parse_lum(char *keyword)
{
  if(strcasecmp(keyword, "help") == 0) {
    print_lum_help();
    exit(42);
  } else if(strcasecmp(keyword, "rec601") == 0) {
    return REC601;
  } else if(strcasecmp(keyword, "rec709") == 0) {
    return REC709;
  } else if(strcasecmp(keyword, "linear") == 0) {
    return LINEAR;
  } else if(strcasecmp(keyword, "minimum") == 0) {
    return MINIMUM;
  } else if(strcasecmp(keyword, "maximum") == 0) {
    return MAXIMUM;
  } else if(strcasecmp(keyword, "red") == 0) {
    return RED;
  } else if(strcasecmp(keyword, "green") == 0) {
    return GREEN;
  } else if(strcasecmp(keyword, "blue") == 0) {
    return BLUE;
  } else {
    return DEFAULT_LUM_FORMULA;
  }
}

/* report Imlib2 load/save error to stderr */
void report_imlib_error(Imlib_Load_Error error)
{
  fputs("  Imlib2 error code: ",stderr);
  switch (error) {
    case IMLIB_LOAD_ERROR_NONE:
      fputs("IMLIB_LOAD_ERROR_NONE\n", stderr);
      break;
    case IMLIB_LOAD_ERROR_FILE_DOES_NOT_EXIST:
      fputs("IMLIB_LOAD_ERROR_FILE_DOES_NOT_EXIST\n", stderr);
      break;
    case IMLIB_LOAD_ERROR_FILE_IS_DIRECTORY:
      fputs("IMLIB_LOAD_ERROR_FILE_IS_DIRECTORY\n", stderr);
      break;
    case IMLIB_LOAD_ERROR_PERMISSION_DENIED_TO_READ:
      fputs("IMLIB_LOAD_ERROR_PERMISSION_DENIED_TO_READ\n", stderr);
      break;
    case IMLIB_LOAD_ERROR_NO_LOADER_FOR_FILE_FORMAT:
      fputs("IMLIB_LOAD_ERROR_NO_LOADER_FOR_FILE_FORMAT\n", stderr);
      break;
    case IMLIB_LOAD_ERROR_PATH_TOO_LONG:
      fputs("IMLIB_LOAD_ERROR_PATH_TOO_LONG\n", stderr);
      break;
    case IMLIB_LOAD_ERROR_PATH_COMPONENT_NON_EXISTANT:
      fputs("IMLIB_LOAD_ERROR_PATH_COMPONENT_NON_EXISTANT\n", stderr);
      break;
    case IMLIB_LOAD_ERROR_PATH_COMPONENT_NOT_DIRECTORY:
      fputs("IMLIB_LOAD_ERROR_PATH_COMPONENT_NOT_DIRECTORY\n", stderr);
      break;
    case IMLIB_LOAD_ERROR_PATH_POINTS_OUTSIDE_ADDRESS_SPACE:
      fputs("IMLIB_LOAD_ERROR_PATH_POINTS_OUTSIDE_ADDRESS_SPACE\n", stderr);
      break;
    case IMLIB_LOAD_ERROR_TOO_MANY_SYMBOLIC_LINKS:
      fputs("IMLIB_LOAD_ERROR_TOO_MANY_SYMBOLIC_LINKS\n", stderr);
      break;
    case IMLIB_LOAD_ERROR_OUT_OF_MEMORY:
      fputs("IMLIB_LOAD_ERROR_OUT_OF_MEMORY\n", stderr);
      break;
    case IMLIB_LOAD_ERROR_OUT_OF_FILE_DESCRIPTORS:
      fputs("IMLIB_LOAD_ERROR_OUT_OF_FILE_DESCRIPTORS\n", stderr);
      break;
    case IMLIB_LOAD_ERROR_PERMISSION_DENIED_TO_WRITE:
      fputs("IMLIB_LOAD_ERROR_PERMISSION_DENIED_TO_WRITE\n", stderr);
      break;
    case IMLIB_LOAD_ERROR_OUT_OF_DISK_SPACE:
      fputs("IMLIB_LOAD_ERROR_OUT_OF_DISK_SPACE\n", stderr);
      break;
    case IMLIB_LOAD_ERROR_UNKNOWN:
      fputs("IMLIB_LOAD_ERROR_UNKNOWN\n", stderr);
      break;
    default:
      fprintf(stderr, "unknown error code %d, please report\n", error);
      break;
  }
}
