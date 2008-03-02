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

/* Copyright (C) 2004-2007 Erik Auerswald <auerswal@unix-ag.uni-kl.de> */

/* ImLib2 Header */
#include <X11/Xlib.h>       /* needed by Imlib2.h */
#include <Imlib2.h>

/* standard things */
#include <stdio.h>          /* puts, printf, BUFSIZ, perror, FILE */
#include <stdlib.h>         /* exit */

/* string manipulation */
#include <string.h>         /* strcpy */

/* option parsing */
#include <getopt.h>         /* getopt */
#include <unistd.h>         /* getopt */

/* sin, cos */
#include <math.h>

/* my headers */
#include "ssocr.h"          /* defines */

/* global variables */
int ssocr_foreground = SSOCR_BLACK;
int ssocr_background = SSOCR_WHITE;
int debug_output=0; /* print debug output? */

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
void ssocr_set_imlib_color(fg_bg_t color)
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
      fprintf(stderr, "error: ssocr_set_imlib_color(): unknown color %d\n",
	  color);
      exit(99);
      break;
  }
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
	/* draw a black (foreground) pixel */
	imlib_context_set_image(new_image);
	ssocr_set_imlib_color(FG);
	imlib_image_draw_pixel(x,y,0);
	imlib_context_set_image(*source_image);
      } else {
	/* draw a white (background) pixel */
	imlib_context_set_image(new_image);
	ssocr_set_imlib_color(BG);
	imlib_image_draw_pixel(x,y,0);
	imlib_context_set_image(*source_image);
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
  for(i=0; i<n; i++)
  {
    temp_image2 = dilation(&temp_image1, thresh, lt);
    imlib_context_set_image(temp_image1);
    imlib_free_image();
    temp_image1 = temp_image2;
  }
  /* erosion n times */
  for(i=0; i<n; i++)
  {
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
  for(i=0; i<n; i++)
  {
    temp_image2 = erosion(&temp_image1, thresh, lt);
    imlib_context_set_image(temp_image1);
    imlib_free_image();
    temp_image1 = temp_image2;
  }
  /* dilation n times */
  for(i=0; i<n; i++)
  {
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
  ssocr_set_imlib_color(BG);
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
	/* draw a black (foreground) pixel */
	imlib_context_set_image(new_image);
	ssocr_set_imlib_color(FG);
	imlib_image_draw_pixel(x,y,0);
	imlib_context_set_image(*source_image);
      } else {
	/* draw a white (background) pixel */
	imlib_context_set_image(new_image);
	ssocr_set_imlib_color(BG);
	imlib_image_draw_pixel(x,y,0);
	imlib_context_set_image(*source_image);
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

/* grey stretching, i.e. lum<t1 => lum=0, lum>t2 => lum=100,
 * else lum=((lum-t1)*MAXRGB)/(t2-t1) */
Imlib_Image grey_stretch(Imlib_Image *source_image, double t1, double t2,
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
    fprintf(stderr, "error: grey_stretch(): t1=%.2f >= t2=%.2f\n", t1, t2);
    exit(99);
  }

  /* save pointer to current image */
  current_image = imlib_context_get_image();

  /* create a new image */
  imlib_context_set_image(*source_image);
  height = imlib_image_get_height();
  width = imlib_image_get_width();
  new_image = imlib_clone_image();

  /* check if 0 < t1,t2 < MAXRGB */
  if(t1 <= 0.0) {
    fprintf(stderr, "error: grey_stretch(): t1=%.2f <= 0.0\n", t1);
    exit(99);
  }
  if(t2 >= MAXRGB) {
    fprintf(stderr, "error: grey_stretch(): t2=%.2f >= %d.0\n", t2, MAXRGB);
    exit(99);
  }

  /* grey stretch image */
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

/* use dynamic (aka adaptive) thresholding to create monochrome image */
/* ww and wh are the width and height of the rectangle used to find the
 * threshold value */
Imlib_Image dynamic_threshold(Imlib_Image *source_image, double t, luminance_t lt
                              ,int ww, int wh)
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
  for(x=0; x<width; x++)
  {
    for(y=0; y<height; y++)
    {
      imlib_image_query_pixel(x, y, &color);
      lum = get_lum(&color, lt);
      thresh = get_threshold(source_image, t/100.0, lt, x-ww/2, y-ww/2, ww, wh);
      if(is_pixel_set(lum, thresh))
      {
	/* draw a black (foreground) pixel */
	imlib_context_set_image(new_image);
	ssocr_set_imlib_color(FG);
	imlib_image_draw_pixel(x,y,0);
	imlib_context_set_image(*source_image);
      }
      else
      {
	/* draw a white (background) pixel */
	imlib_context_set_image(new_image);
	ssocr_set_imlib_color(BG);
	imlib_image_draw_pixel(x,y,0);
	imlib_context_set_image(*source_image);
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
	/* draw a black (foreground) pixel */
	imlib_context_set_image(new_image);
	ssocr_set_imlib_color(FG);
	imlib_image_draw_pixel(x,y,0);
	imlib_context_set_image(*source_image);
      } else {
	/* draw a white (background) pixel */
	imlib_context_set_image(new_image);
	ssocr_set_imlib_color(BG);
	imlib_image_draw_pixel(x,y,0);
	imlib_context_set_image(*source_image);
      }
    }
  }

  /* restore image from before function call */
  imlib_context_set_image(current_image);

  /* return filtered image */
  return new_image;
}

/* set pixel to black (0,0,0) if R<T or G<T or R<T, T=thresh/100*255 */
Imlib_Image rgb_threshold(Imlib_Image *source_image, double thresh,
                          channel_t channel)
{
  Imlib_Image new_image; /* construct filtered image here */
  Imlib_Image current_image; /* save image pointer */
  int height, width; /* image dimensions */
  int x,y; /* iteration variables */
  Imlib_Color pixel; /* alpha, red, green, blue */
  int T = (255 * thresh) / 100;
  int set_pixel=0; /* decide if pixel shall be set, i.e. black (foreground) */

  /* save pointer to current image */
  current_image = imlib_context_get_image();

  /* create new image */
  imlib_context_set_image(*source_image);
  height = imlib_image_get_height();
  width = imlib_image_get_width();
  new_image = imlib_clone_image();

  /* check for every pixel if it should be set in filtered image */
  for(x=0; x<width; x++)
  {
    for(y=0; y<height; y++)
    {
      imlib_image_query_pixel(x, y, &pixel);
      set_pixel=0;
      switch(channel) {
	case CHAN_ALL:
	  if((pixel.red<T) || (pixel.blue<T) || (pixel.green<T))
	    set_pixel=1;
	  break;
	case CHAN_RED:   if(pixel.red<T) set_pixel=1; break;
	case CHAN_GREEN: if(pixel.green<T) set_pixel=1; break;
	case CHAN_BLUE:  if(pixel.blue<T) set_pixel=1; break;
	default:
	  fprintf(stderr, "warning: rgb_threshold(): unknown channel %d\n",
	          channel);
	  break;
      }
      if(set_pixel)
      {
	/* draw a black (foreground) pixel */
	imlib_context_set_image(new_image);
	ssocr_set_imlib_color(FG);
	imlib_image_draw_pixel(x,y,0);
	imlib_context_set_image(*source_image);
      }
      else
      {
	/* draw a white (background) pixel */
	imlib_context_set_image(new_image);
	ssocr_set_imlib_color(BG);
	imlib_image_draw_pixel(x,y,0);
	imlib_context_set_image(*source_image);
      }
    }
  }

  /* restore image from before function call */
  imlib_context_set_image(current_image);

  /* return filtered image */
  return new_image;
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
  if(w == -1)
    w = width;
  if(h == -1)
    h = width;

  /* assure valid coordinates */
  if(x+w > width)
    x = width-w;
  if(y+h > height)
    y = height-h;
  if(x<0)
    x=0;
  if(y<0)
    y=0;

  /* find the threshold value to differentiate between dark and light */
  for(xi=0; (xi<w) && (xi<width); xi++)
  {
    for(yi=0; (yi<h) && (yi<height); yi++)
    {
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
  if(w == -1)
    w = width;
  if(h == -1)
    h = width;

  /* assure valid coordinates */
  if(x+w > width)
    x = width-w;
  if(y+h > height)
    y = height-h;
  if(x<0)
    x=0;
  if(y<0)
    y=0;

  /* find the minimum value in the image */
  for(xi=0; (xi<w) && (xi<width); xi++)
  {
    for(yi=0; (yi<h) && (yi<height); yi++)
    {
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
  if(w == -1)
    w = width;
  if(h == -1)
    h = width;

  /* assure valid coordinates */
  if(x+w > width)
    x = width-w;
  if(y+h > height)
    y = height-h;
  if(x<0)
    x=0;
  if(y<0)
    y=0;

  /* find the minimum value in the image */
  for(xi=0; (xi<w) && (xi<width); xi++)
  {
    for(yi=0; (yi<h) && (yi<height); yi++)
    {
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
  if(bdwidth > width/2)
    bdwidth = width/2;
  if(bdwidth > height/2)
    bdwidth = height/2;

  /* draw white (background) rectangle around new image */
  for(x=0, y=0; x<bdwidth; x++, y++) {
    imlib_context_set_image(new_image);
    ssocr_set_imlib_color(BG);
    imlib_image_draw_rectangle(x, y, width-2*x, height-2*y);
  }

  /* restore image from before function call */
  imlib_context_set_image(current_image);

  /* return filtered image */
  return new_image;
}

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
  for(y=1; y<height; y++)
  {
    shift = y * offset / (height-1);
    /* copy pixels */
    for(x=width-1; x>0+shift; x--)
    {
      imlib_image_query_pixel(x-shift, y, &color_return);
      imlib_context_set_image(new_image);
      imlib_context_set_color(color_return.red, color_return.green, color_return.blue, color_return.alpha);
      imlib_image_draw_pixel(x,y,0);
      imlib_context_set_image(*source_image);
    }
    /* fill with white (background) */
    imlib_context_set_image(new_image);
    ssocr_set_imlib_color(BG);
    for(x=0; x<shift; x++)
    {
      imlib_image_draw_pixel(x,y,0);
    }
    imlib_context_set_image(*source_image);
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
        ssocr_set_imlib_color(BG);
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

/* turn image to greyscale */
Imlib_Image greyscale(Imlib_Image *source_image, luminance_t lt)
{
  Imlib_Image new_image; /* construct greyscale image here */
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

  /* transform image to greyscale */
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
  for(x=0; x<width; x++)
  {
    for(y=0; y<height; y++)
    {
      imlib_image_query_pixel(x, y, &color);
      lum = get_lum(&color, lt);
      if(is_pixel_set(lum, thresh))
      {
	/* draw a white (background) pixel */
	imlib_context_set_image(new_image);
	ssocr_set_imlib_color(BG);
	imlib_image_draw_pixel(x,y,0);
	imlib_context_set_image(*source_image);
      }
      else
      {
	/* draw a black (foreground) pixel */
	imlib_context_set_image(new_image);
	ssocr_set_imlib_color(FG);
	imlib_image_draw_pixel(x,y,0);
	imlib_context_set_image(*source_image);
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
  if(x < 0)
    x = 0;
  if(y < 0)
    y = 0;
  if(x >= width)
    x = width - 1;
  if(y >= height)
    y = height - 1;
  if(x + w > width)
    w = width - x;
  if(y + h > height)
    h = height - x;

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

/*** print version or online help ***/

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

/* print version */
void print_version(FILE *f)
{
  fprintf(f, "Seven Segment Optical Character Recognition Version %s\n",
          VERSION);
  fprintf(f, "Copyright (C) 2004-2007 by Erik Auerswald"
             " <auerswal@unix-ag.uni-kl.de>\n");
  fprintf(f, "This program comes with ABSOLUTELY NO WARRANTY\n");
  fprintf(f, "This is free software, and you are welcome to redistribute it"
             " under the terms\nof the GPL version 3\n");
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
  fprintf(f, "         -n, --number-pixels=#    number of pixels needed to recognize a segment\n");
  fprintf(f, "         -i, --ignore-pixels=#    number of pixels ignored when searching digit\n");
  fprintf(f, "                                  boundaries\n");
  fprintf(f, "         -d, --number-digits=#    number of digits in image\n");
  fprintf(f, "         -o, --output-image=FILE  write processed image to FILE\n");
  fprintf(f, "         -O, --output-format=FMT  use output format FMT (Imlib2 formats)\n");
  fprintf(f, "         -p, --process-only       do image processing only, no OCR\n");
  fprintf(f, "         -D, --debug-image[=FILE] write a debug image to FILE or %s\n",DEBUG_IMAGE_NAME);
  fprintf(f, "         -P, --debug-output       print debug information\n");
  fprintf(f, "         -f, --foreground=COLOR   set foreground color (black or white)\n");
  fprintf(f, "         -b, --background=COLOR   set foreground color (black or white)\n");
  fprintf(f, "         -I, --print-info         print image dimensions and used lum values\n");
  fprintf(f, "         -g, --adjust-grey        use T1 and T2 as percentages of used values\n");
  fprintf(f, "         -l, --luminance=KEYWORD  compute luminance using formula KEYWORD\n");
  fprintf(f, "                                  use -l help for list of KEYWORDS\n");
  fprintf(f, "\nCommands: dilation                dilation algorithm (with mask of 1 pixel)\n");
  fprintf(f, "          erosion                 erosion algorithm (with mask of 9 pixels)\n");
  fprintf(f, "          closing [N]             closing algorithm\n");
  fprintf(f, "                                  ([N times] dilation then [N times] erosion)\n");
  fprintf(f, "          opening [N]             opening algorithm\n");
  fprintf(f, "                                  ([N times] erosion then [N times] dilation)\n");
  fprintf(f, "          remove_isolated         remove isolated pixels\n");
  fprintf(f, "          make_mono               make image monochrome\n");
  fprintf(f, "          greyscale               transform image to greyscale\n");
  fprintf(f, "          invert                  make inverted monochrome image\n");
  fprintf(f, "          grey_stretch T1 T2      stretch luminance values\n");
  fprintf(f, "                                  from [T1,T2] to [0,255]\n");
  fprintf(f, "          dynamic_threshold W H   make image monochrome w. dynamic thresholding\n");
  fprintf(f, "                                  with a window of width W and height H\n");
  fprintf(f, "          rgb_threshold           make image monochrome by setting every pixel\n");
  fprintf(f, "                                  with any values of red, green or blue below\n");
  fprintf(f, "                                  below the threshold to black\n");
  fprintf(f, "          r_threshold             make image monochrome using only red channel\n");
  fprintf(f, "          g_threshold             make image monochrome using only green channel\n");
  fprintf(f, "          b_threshold             make image monochrome using only blue channel\n");
  fprintf(f, "          white_border [WIDTH]    make border of WIDTH (or 1) of image have\n");
  fprintf(f, "                                  background color\n");
  fprintf(f, "          shear OFFSET            shear image OFFSET pixels (at bottom) to the\n");
  fprintf(f, "                                  right\n");
  fprintf(f, "          rotate THETA            rotate image by THETA degrees\n");
  fprintf(f, "          crop X Y W H            crop image with upper left corner (X,Y) with\n");
  fprintf(f, "                                  width W and height H\n");
  fprintf(f, "          set_pixels_filter MASK  set pixels that have at least MASK neighbor\n");
  fprintf(f, "                                  pixels set (including checked position)\n");
  fprintf(f, "          keep_pixels_filter MASK keeps pixels that have at least MASK neighbor\n");
  fprintf(f, "                                  pixels set (not counting the checked pixel)\n");
  fprintf(f, "\nDefaults: needed pixels  = %2d\n", NEED_PIXELS);
  fprintf(f, "          ignored pixels = %2d\n", IGNORE_PIXELS);
  fprintf(f, "          no. of digits  = %2d\n", NUMBER_OF_DIGITS);
  fprintf(f, "          threshold      = %5.2f\n", THRESHOLD);
  fprintf(f, "          foreground     = %s\n",
      (ssocr_foreground == SSOCR_BLACK) ? "black" : "white");
  fprintf(f, "          background     = %s\n",
      (ssocr_background == SSOCR_BLACK) ? "black" : "white");
  fprintf(f, "          luminance      = ");
  print_lum_key(DEFAULT_LUM_FORMULA, f); fprintf(f, "\n");
  fprintf(f, "\nOperation: The IMAGE is read, the COMMANDs are processed in the sequence\n");
  fprintf(f, "           they are given, in the resulting image the given number of digits\n");
  fprintf(f, "           are searched and recognized, after which the recognized number is\n");
  fprintf(f, "           written to STDOUT.\n");
  fprintf(f, "           The recognition algorithm works with set or unset pixels and uses\n");
  fprintf(f, "           the given THRESHOLD to decide if a pixel is set or not.\n\n");
  fprintf(f, "Exit Codes:  0 if correct number of digits have been recognized\n");
  fprintf(f, "             1 if a different number of digits have been found\n");
  fprintf(f, "             2 if one of the digits could not be recognized\n");
  fprintf(f, "             3 if successful image processing only\n");
  fprintf(f, "            42 if -h, -V, or -l help\n");
  fprintf(f, "            99 otherwise\n");
}

/*** main() ***/

int main(int argc, char **argv)
{
  Imlib_Image image=NULL; /* an image handle */
  Imlib_Image new_image=NULL; /* a temporary image handle */
  Imlib_Image debug_image=NULL; /* DEBUG */

  int i, j, d;  /* iteration variables */
  int unknown_digit=0; /* was one of the 6 found digits an unknown one? */
  int need_pixels = NEED_PIXELS; /*pixels needed to recognize a segment as set*/
  int number_of_digits = NUMBER_OF_DIGITS; /* look for this many digits */
  int ignore_pixels = IGNORE_PIXELS; /* pixels to ignore when checking column */
  double thresh=THRESHOLD;  /* border between light and dark */
  int offset;  /* offset for shear */
  double theta; /* rotation angle */
  int absolute_threshold=0; /* absolute threshold given? */
  int verbose=0;  /* be verbose? */
  char *output_file=NULL; /* wrie processed image to file */
  char *output_fmt=NULL; /* use this format */
  int use_debug_image=0; /* write a debug image... */
  char *debug_image_file=NULL; /* ...to this file */
  int process_only=0; /* image processing only (no OCR)? */
  int print_info=0; /* print image info? */
  int adjust_grey=0; /* use T1 and T2 as percentages of used luminance values*/
  luminance_t lt=DEFAULT_LUM_FORMULA; /* luminance function */

  /* if we provided no arguments to the program exit */
  if (argc < 2)
  {
    usage(argv[0], stderr);
    exit(99);
  }

  /* parse command line */
  while (1)
  {
    int option_index = 0;
    char c;
    static struct option long_options[] = {
      {"help", 0, 0, 'h'}, /* print help */
      {"version", 0, 0, 'V'}, /* show version */
      {"threshold", 1, 0, 't'}, /* set threshold (instead of THRESHOLD) */
      {"verbose", 0, 0, 'v'}, /* talk about programm execution */
      {"absolute-threshold", 0, 0, 'a'}, /* use treshold value as provided */
      {"number-pixels", 1, 0, 'n'}, /* pixels needed to regard segment as set */
      {"ignore-pixels", 1, 0, 'i'}, /* pixels ignored when searching digits */
      {"number-digits", 1, 0, 'd'}, /* number of digits in image */
      {"output-image", 1, 0, 'o'}, /* write processed image to given file */
      {"output-format", 1, 0, 'O'}, /* format of output image */
      {"debug-image", 2, 0, 'D'}, /* write a debug image */
      {"process-only", 0, 0, 'p'}, /* image processing only */
      {"debug-output", 0, 0, 'P'}, /* print debug output? */
      {"foreground", 1, 0, 'f'}, /* set foreground color */
      {"background", 1, 0, 'b'}, /* set background color */
      {"print-info", 0, 0, 'I'}, /* print image info */
      {"adjust-grey", 0, 0, 'g'}, /* use T1 and T2 as perecntages of used vals*/
      {"luminance", 1, 0, 'l'}, /* luminance formula */
      {0, 0, 0, 0} /* terminate long options */
    };
    c = getopt_long (argc, argv, "hVt:van:i:d:o:O:D::pPf:b:Igl:",
	             long_options, &option_index);
    if (c == -1)
      break; /* leaves while (1) loop */
    switch (c)
    {
      case 'h':
	usage(argv[0],stdout);
	exit (42);
	break;
      case 'V':
	print_version(stdout);
	exit (42);
	break;
      case 'v':
	verbose=1;
	if(debug_output) {
	  printf("verbose=%d\n", verbose);
	}
	break;
      case 't':
	if(optarg)
	{
	  thresh = atof(optarg);
	  if(debug_output) {
	    printf("thresh = %f (default: %f)\n", thresh, THRESHOLD);
	  }
	  if(thresh < 0.0 || 100.0 < thresh)
	  {
	    thresh = THRESHOLD;
	    if(verbose) {
	      printf("ignoring --treshold=%s\n", optarg);
	    }
	  }
	  if(debug_output) {
	    printf("thresh = %f (default: %f)\n", thresh, THRESHOLD);
	  }
	}
	break;
      case 'a':
	absolute_threshold=1;
	break;
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
	  if(number_of_digits < 0) {
	    fprintf(stderr, "warning: ignoring --number-digits=%s\n", optarg);
	    number_of_digits = NUMBER_OF_DIGITS;
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
	use_debug_image = 1;
	if(optarg) {
	  debug_image_file = strdup(optarg);
	} else {
	  debug_image_file = strdup(DEBUG_IMAGE_NAME);
	}
	break;
      case 'p':
	process_only = 1;
	break;
      case 'P':
	debug_output = 1;
	break;
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
	print_info=1;
	if(debug_output) {
	  printf("print_info=%d\n", print_info);
	}
	break;
      case 'g':
	adjust_grey=1;
	if(debug_output) {
	  printf("adjust_grey=%d\n", adjust_grey);
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
  if(debug_output) {
    printf("================================================================================\n");
    printf("verbose=%d\nthresh=%f\n", verbose, thresh);
    printf("print_info=%d\nadjust_grey=%d\n", print_info, adjust_grey);
    printf("absolute_threshold=%d\n", absolute_threshold);
    printf("need_pixels = %d\n", need_pixels);
    printf("ignore_pixels = %d\n", ignore_pixels);
    printf("number_of_digits = %d\n", number_of_digits);
    printf("foreground = %d (%s)\n", ssocr_foreground,
	(ssocr_foreground == SSOCR_BLACK) ? "black" : "white");
    printf("background = %d (%s)\n", ssocr_background,
	(ssocr_background == SSOCR_BLACK) ? "black" : "white");
    printf("luminance  = ");
    print_lum_key(lt, stdout); printf("\n");
    printf("optind=%d argc=%d\n", optind, argc);
    printf("================================================================================\n");
  }

  /* if no argument left exit the program */
  if(optind >= argc)
  {
    fprintf(stderr, "error: no image filename given\n");
    usage(argv[0], stderr);
    exit(99);
  }
  if(debug_output) {
    printf("argv[argc-1]=%s used as image file name\n", argv[argc-1]);
  }

  /* load the image */
  if(verbose)
  {
    printf("loading image %s\n", argv[argc-1]);
  }
  image = imlib_load_image_immediately_without_cache(argv[argc-1]);

  /* if the load was successful */
  if (image)
  {
    /* some variables */
    int w, h, lum;  /* width, height, pixel luminance */
    int col=UNKNOWN;  /* is column dark or light? */
    int row=UNKNOWN;  /* is row dark or light? */
    int dig_w;  /* width of digit part of image */
    Imlib_Color color; /* Imlib2 RGBA color structure */
    /* state of search */
    int state = (ssocr_foreground == SSOCR_BLACK) ? FIND_DARK : FIND_LIGHT;
    digit_struct *digits=NULL; /* position of digits in image */
    int found_pixels=0; /* how many pixels are already found */
    char *tmp=NULL; /* used to find filename extension */

    if(!(digits = calloc(number_of_digits, sizeof(digit_struct)))) {
      perror("digits = calloc()");
      exit(99);
    }

    /* initialize some vars */
    for(i=0; i<number_of_digits; i++)
    {
      digits[i].x1 = digits[i].x2 = digits[i].y1 = digits[i].y2 = 0;
      digits[i].digit=D_UNKNOWN;
    }

    /* set the image we loaded as the current context image to work on */
    imlib_context_set_image(image);

    /* get image parameters */
    w = imlib_image_get_width();
    h = imlib_image_get_height();
    if(debug_output || print_info) {
      printf("image width: %d\nimage height: %d\n",w,h);
    }

    /* get minimum and maximum "value" values */
    if(debug_output || print_info) {
      printf("%.2f <= lum <= %.2f (lum should be in [0,255])\n",
             get_minval(&image, 0, 0, -1, -1, lt),
	     get_maxval(&image, 0, 0, -1, -1, lt));
    }

    /* adapt threshold to image */
    if(!absolute_threshold) {
      thresh = get_threshold(&image, thresh/100.0, lt, 0, 0, -1, -1);
    }
    if(verbose || debug_output) {
      printf("using threshold %.2f\n", thresh);
    }

    /* process commands */
    if(verbose) /* then print found commands */
    {
      if(optind >= argc-1)
      {
	printf("no commands given, using image %s unmodified\n", argv[argc-1]);
      }
      else
      {
	printf("got commands");
	for(i=optind; i<argc-1; i++)
	{
	  printf(" %s", argv[i]);
	  if(debug_output) {
	    printf(" (argv[%d])", i);
	  }
	}
	printf("\n");
      }
    }
    if(optind < argc-1) /* then process commands */
    {
      for(i=optind; i<argc-1; i++)
      {
	if(strcasecmp("dilation",argv[i]) == 0)
	{
	  if(verbose) puts(" processing dilation");
	  new_image = dilation(&image, thresh, lt);
	  imlib_context_set_image(image);
	  imlib_free_image();
	  image = new_image;
	}
	else if(strcasecmp("erosion",argv[i]) == 0)
	{
	  if(verbose) puts(" processing erosion");
	  new_image = erosion(&image, thresh, lt);
	  imlib_context_set_image(image);
	  imlib_free_image();
	  image = new_image;
	}
	else if(strcasecmp("opening",argv[i]) == 0)
	{
	  int n=atoi(argv[i+1]);
	  if((n>0) && (i+1<argc-1))
	  {
	    if(verbose)
	    {
	      printf(" processing opening %d", n);
	      if(debug_output) {
		printf(" (from string %s)", argv[i+1]);
	      }
	      printf("\n");
	    }
	    i++;
	    new_image = opening(&image, thresh, lt, n);
	  }
	  else
	  {
	    if(verbose) puts(" processing opening (1)");
	    new_image = opening(&image, thresh, lt, 1);
	  }
	  imlib_context_set_image(image);
	  imlib_free_image();
	  image = new_image;
	}
	else if(strcasecmp("closing",argv[i]) == 0)
	{
	  int n=atoi(argv[i+1]);
	  if((n>0) && (i+1<argc-1))
	  {
	    if(verbose)
	    {
	      printf(" processing closing %d", n);
	      if(debug_output) {
		printf(" (from string %s)", argv[i+1]);
	      }
	      printf("\n");
	    }
	    i++;
	    new_image = closing(&image, thresh, lt, n);
	  }
	  else
	  {
	    if(verbose) puts(" processing closing (1)");
	    new_image = closing(&image, thresh, lt, 1);
	  }
	  imlib_context_set_image(image);
	  imlib_free_image();
	  image = new_image;
	}
	else if(strcasecmp("remove_isolated",argv[i]) == 0)
	{
	  if(verbose) puts(" processing remove_isolated");
	  new_image = remove_isolated(&image, thresh, lt);
	  imlib_context_set_image(image);
	  imlib_free_image();
	  image = new_image;
	}
	else if(strcasecmp("make_mono",argv[i]) == 0)
	{
	  if(verbose) puts(" processing make_mono");
	  new_image = make_mono(&image, thresh, lt);
	  imlib_context_set_image(image);
	  imlib_free_image();
	  image = new_image;
	}
	else if(strcasecmp("white_border",argv[i]) == 0)
	{
	  int bdwidth=atoi(argv[i+1]);
	  if((bdwidth>0) && (i+1<argc-1)) {
	    if(verbose) {
	      printf(" processing white_border %d", bdwidth);
	      if(debug_output) {
		printf(" (from string %s)", argv[i+1]);
	      }
	      printf("\n");
	    }
	    new_image = white_border(&image, bdwidth);
	    i++;
	  }
	  else {
	    if(verbose)
	      puts(" processing white_border (1)");
	    new_image = white_border(&image, 1);
	  }
	  imlib_context_set_image(image);
	  imlib_free_image();
	  image = new_image;
	}
	else if(strcasecmp("shear",argv[i]) == 0)
	{
	  if(verbose)
	  {
	    printf(" processing shear %d", atoi(argv[i+1]));
	    if(debug_output) {
	      printf(" (from string %s)", argv[i+1]);
	    }
	    printf("\n");
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
	}
	else if(strcasecmp("set_pixels_filter",argv[i]) == 0)
	{
	  int mask;
	  if(verbose)
	  {
	    printf(" processing set_pixels_filter %d", atoi(argv[i+1]));
	    if(debug_output) {
	      printf(" (from string %s)", argv[i+1]);
	    }
	    printf("\n");
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
	}
	else if(strcasecmp("keep_pixels_filter",argv[i]) == 0)
	{
	  int mask;
	  if(verbose)
	  {
	    printf(" processing keep_pixels_filter %d", atoi(argv[i+1]));
	    if(debug_output) {
	      printf(" (from string %s)", argv[i+1]);
	    }
	    printf("\n");
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
	}
	else if(strcasecmp("dynamic_threshold",argv[i]) == 0)
	{
	  if(i+2<argc-1) {
	    int ww, wh;
	    ww = atoi(argv[i+1]);
	    wh = atoi(argv[i+2]);
	    if(verbose) {
	      printf(" processing dynamic_threshold %d %d", ww, wh);
	      if(debug_output) {
		printf(" (from strings %s and %s)", argv[i+1], argv[i+2]);
	      }
	      printf("\n");
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
	}
	else if(strcasecmp("rgb_threshold",argv[i]) == 0)
	{
	  if(verbose) puts(" processing rgb_threshold");
	  new_image = rgb_threshold(&image, thresh, CHAN_ALL);
	  imlib_context_set_image(image);
	  imlib_free_image();
	  image = new_image;
	}
	else if(strcasecmp("r_threshold",argv[i]) == 0)
	{
	  if(verbose) puts(" processing r_threshold");
	  new_image = rgb_threshold(&image, thresh, CHAN_RED);
	  imlib_context_set_image(image);
	  imlib_free_image();
	  image = new_image;
	}
	else if(strcasecmp("g_threshold",argv[i]) == 0)
	{
	  if(verbose) puts(" processing g_threshold");
	  new_image = rgb_threshold(&image, thresh, CHAN_GREEN);
	  imlib_context_set_image(image);
	  imlib_free_image();
	  image = new_image;
	}
	else if(strcasecmp("b_threshold",argv[i]) == 0)
	{
	  if(verbose) puts(" processing b_threshold");
	  new_image = rgb_threshold(&image, thresh, CHAN_BLUE);
	  imlib_context_set_image(image);
	  imlib_free_image();
	  image = new_image;
	}
	else if(strcasecmp("invert",argv[i]) == 0)
	{
	  if(verbose) puts(" processing invert");
	  new_image = invert(&image, thresh, lt);
	  imlib_context_set_image(image);
	  imlib_free_image();
	  image = new_image;
	}
	else if(strcasecmp("grey_stretch",argv[i]) == 0)
	{
	  if(i+2<argc-1) {
	    double t1, t2;
	    t1 = atof(argv[i+1]);
	    t2 = atof(argv[i+2]);
	    if(verbose) {
	      printf(" processing grey_stretch %.2f %.2f", t1, t2);
	      if(debug_output) {
		printf(" (from strings %s and %s)", argv[i+1], argv[i+2]);
	      }
	      printf("\n");
	    }
	    if(adjust_grey) {
	      double min=-1.0, max=-1.0;
	      if(verbose) {
		printf(" adjusting T1=%.2f and T2=%.2f to image\n", t1, t2);
	      }
	      min = get_minval(&image, 0, 0, -1, -1, lt);
	      max = get_maxval(&image, 0, 0, -1, -1, lt);
	      t1 = min + t1/100.0 * (max - min);
	      t2 = min + t2/100.0 * (max - min);
	      if(verbose) {
		printf(" adjusted to T1=%.2f and T2=%.2f\n", t1,t2);
	      }
	    }
	    i+=2; /* skip the arguments to grey_stretch */
	    new_image = grey_stretch(&image, t1, t2, lt);
	    imlib_context_set_image(image);
	    imlib_free_image();
	    image = new_image;
	  } else {
	    fprintf(stderr,
		"error: grey_stretch command needs two arguments\n");
	    exit(99);
	  }
	}
	else if(strcasecmp("greyscale",argv[i]) == 0)
	{
	  if(verbose) puts(" processing greyscale");
	  new_image = greyscale(&image, lt);
	  imlib_context_set_image(image);
	  imlib_free_image();
	  image = new_image;
	}
	else if(strcasecmp("crop",argv[i]) == 0)
	{
	  if(i+4<argc-1) {
	    int x, y, cw, ch; /* lw = crop width, lh = crop height */
	    x = atoi(argv[i+1]);
	    y = atoi(argv[i+2]);
	    cw = atoi(argv[i+3]);
	    ch = atoi(argv[i+4]);
	    if(verbose) {
	      printf(" cropping from (%d,%d) to (%d,%d) [width %d, height %d]",
	              x, y, x+cw, y+ch, cw, ch);
	      if(debug_output) {
		printf(" (from strings %s, %s, %s, and %s)", argv[i+1],
		       argv[i+2], argv[i+3], argv[i+4]);
	      }
	      printf("\n");
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
	    if(debug_output || verbose) {
	      printf("  cropped image width: %d\n"
	             "  cropped image height: %d\n", w, h);
	    }
            /* get minimum and maximum "value" values in cropped image */
            if(debug_output || print_info || verbose) {
	      printf("  %.2f <= lum <= %.2f in cropped image"
		     " (lum should be in [0,255])\n",
		     get_minval(&image, 0, 0, -1, -1, lt),
		     get_maxval(&image, 0, 0, -1, -1, lt));
	    }
	    /* adapt threshold to cropped image */
	    if(!absolute_threshold) {
	      thresh = get_threshold(&image, thresh/100.0, lt, 0, 0, -1, -1);
	    }
	    if(verbose || debug_output) {
	      printf("  using threshold %.2f after cropping\n", thresh);
	    }
	  } else {
	    fprintf(stderr, "error: crop command needs 4 arguments\n");
	    exit(99);
	  }
	}
	else if(strcasecmp("rotate",argv[i]) == 0)
	{
	  if(verbose)
	  {
	    printf(" processing rotate %f", atof(argv[i+1]));
	    if(debug_output) {
	      printf(" (from string %s)", argv[i+1]);
	    }
	    printf("\n");
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
	}
	else
	{
	  printf(" unknown command \"%s\"\n", argv[i]);
	}
      }
    }

    /* assure we are working with the current image */
    imlib_context_set_image(image);

    /* write image to file if requested */
    if(output_file) {
      /* get file format for output image */
      if(output_fmt) { /* use provided format string */
	tmp = output_fmt;
      } else { /* use file name extension */
	tmp = strrchr(output_file, '.');
	if(tmp)
          tmp++;
      }
      if(tmp) {
	if(verbose) printf("saving output image in %s format to file %s\n",
	                   tmp, output_file);
	imlib_image_set_format(tmp);
      } else { /* use png as default */
	if(verbose) printf("saving output image in png format to file %s\n",
	                   output_file);
	imlib_image_set_format("png");
      }
      /* write output image to disk */
      imlib_save_image(output_file);
    }

    /* exit if only image processing shall be done */
    if(process_only) exit(3);

    if(use_debug_image) {
      /* copy processed image to debug image */
      debug_image = imlib_clone_image();
    }

    /* horizontal partition */
    state = (ssocr_foreground == SSOCR_BLACK) ? FIND_DARK : FIND_LIGHT;
    d = 0;
    for(i=0; i<w; i++)
    {
      /* check if column is completely light or not */
      col = UNKNOWN;
      found_pixels = 0;
      for(j=0; j<h; j++)
      {
	imlib_image_query_pixel(i, j, &color);
	lum = get_lum(&color, lt);
	if(is_pixel_set(lum, thresh)) /* dark */
	{
	  found_pixels++;
	  if(found_pixels > ignore_pixels)
	  {
	    /* 1 dark pixels darken the whole column */
	    col = (ssocr_foreground == SSOCR_BLACK) ? DARK : LIGHT;
	  }
	}
	else if(col == UNKNOWN) /* light */
	{
	  col = (ssocr_foreground == SSOCR_BLACK) ? LIGHT : DARK;
	}
      }
      /* save digit position and draw partition line for DEBUG */
      if((state == ((ssocr_foreground == SSOCR_BLACK) ? FIND_DARK : FIND_LIGHT))
	  && (col == ((ssocr_foreground == SSOCR_BLACK) ? DARK : LIGHT)))
	/* beginning of digit */
      {
	if(d>=number_of_digits)
	{
	  fprintf(stderr, "found too many digits (%d)\n", d+1);
	  if(use_debug_image) {
	    /* get file format for debug image */
	    if(output_fmt) { /* use provided format string */
	      tmp = output_fmt;
	    } else { /* use file name extension */
	      tmp = strrchr(debug_image_file, '.');
	      if(tmp)
                tmp++;
	    }
	    if(tmp) {
	      if(verbose) printf("saving debug image in %s format to file %s\n",
				 tmp, debug_image_file);
	      imlib_image_set_format(tmp);
	    } else { /* use png as default */
	      if(verbose) printf("saving debug image in png format to file %s\n"
                                 , debug_image_file);
	      imlib_image_set_format("png");
	    }
	    /* write debug image to disk */
	    imlib_context_set_image(debug_image);
	    imlib_save_image(debug_image_file);
	    imlib_free_image_and_decache();
	    imlib_context_set_image(image);
	  }
	  imlib_free_image_and_decache();
	  exit(1);
	}
	digits[d].x1 = i;
	digits[d].y1 = 0;
	if(use_debug_image) {
	  imlib_context_set_image(debug_image);
	  imlib_context_set_color(255,0,0,255);/* red line for start of digit */
	  imlib_image_draw_line(i,0,i,h-1,0);
	  imlib_context_set_image(image);
	}
	state = (ssocr_foreground == SSOCR_BLACK) ? FIND_LIGHT : FIND_DARK;
      }
      else if((state ==
	        ((ssocr_foreground == SSOCR_BLACK) ? FIND_LIGHT : FIND_DARK))
		&& (col == ((ssocr_foreground == SSOCR_BLACK) ? LIGHT : DARK))) /* end of digit */
      {
	digits[d].x2 = i;
	digits[d].y2 = h-1;
	d++;
	if(use_debug_image) {
	  imlib_context_set_image(debug_image);
	  imlib_context_set_color(0,0,255,255); /* blue line for end of digit */
	  imlib_image_draw_line(i,0,i,h-1,0);
	  imlib_context_set_image(image);
	}
	state = (ssocr_foreground == SSOCR_BLACK) ? FIND_DARK : FIND_LIGHT;
      }
    }

    /* after the loop above the program should be in state FIND_DARK,
     * i.e. after the last digit some light was found
     * if it is still searching for light end the digit at the border of the
     * image */
    if(state == (ssocr_foreground == SSOCR_BLACK) ? FIND_LIGHT : FIND_DARK)
    {
      digits[d].x2 = w-1;
      digits[d].y2 = h-1;
      d++;
      state = (ssocr_foreground == SSOCR_BLACK) ? FIND_DARK : FIND_LIGHT;
    }
    if(d != number_of_digits)
    {
      fprintf(stderr, "found only %d of %d digits\n", d, number_of_digits);
      if(use_debug_image) {
	/* get file format for debug image */
	if(output_fmt) { /* use provided format string */
	  tmp = output_fmt;
	} else { /* use file name extension */
	  tmp = strrchr(debug_image_file, '.');
          if(tmp)
            tmp++;
	}
	if(tmp) {
	  if(verbose) printf("saving debug image in %s format to file %s\n",
			     tmp, debug_image_file);
	  imlib_image_set_format(tmp);
	} else { /* use png as default */
	  if(verbose) printf("saving debug image in png format to file %s\n",
			     debug_image_file);
	  imlib_image_set_format("png");
	}
	/* write debug image to disk */
	imlib_context_set_image(debug_image);
	imlib_save_image(debug_image_file);
	imlib_free_image_and_decache();
	imlib_context_set_image(image);
      }
      imlib_free_image_and_decache();
      exit(1);
    }
    dig_w = digits[number_of_digits-1].x2 - digits[0].x1;

    if(debug_output) {
      printf("found %d digits\n", d);
      for(d=0; d<number_of_digits; d++)
      {
	printf("digit %d: (%d,%d) -> (%d,%d), width: %d (%f%%)\n", d,
	    digits[d].x1, digits[d].y1, digits[d].x2, digits[d].y2,
	    digits[d].x2 - digits[d].x1,
	    ((digits[d].x2 - digits[d].x1) * 100.0) / dig_w);
      }
    }

    /* at this point the digit 1 can be identified, because it is smaller than
     * the other digits */
    for(i=0; i<number_of_digits; i++)
    {
#if 0
      /* if width of digit is less than 1/2 of (whole width/number_of_digits)
       * it is a 1 (this works for more than 1 digit only)
       * this cannot dscriminate between the two vertical segments to the
       * right or to the left */
      if((digits[i].x2 - digits[i].x1) < (dig_w / (2*number_of_digits)))
      {
	digits[i].digit = D_ONE;
      }
#else
      /* if width of digit is less than 1/3 of its height it is a 1
       * (1/3 is arbitarily chosen -- normally seven segment displays use
       * digits that are 2 times as high as wide) */
      if((digits[i].y2 - digits[i].y1) / (digits[i].x2 - digits[i].x1) > 2) {
	if(debug_output) {
	  printf("digit %d is a 1 (height/width = %d/%d = (int) %d)\n",
	      i, digits[i].y2 - digits[i].y1, digits[i].x2 - digits[i].x1,
	      (digits[i].y2 - digits[i].y1) / (digits[i].x2 - digits[i].x1));
	}
	digits[i].digit = D_ONE;
      }
#endif
    }

    /* find upper and lower boundaries of every digit */
    for(d=0; d<number_of_digits; d++)
    {
      int found_top=0;
      state = (ssocr_foreground == SSOCR_BLACK) ? FIND_DARK : FIND_LIGHT;
      /* start from top of image and scan rows for dark pixel(s) */
      for(j=0; j<h; j++)
      {
	row = UNKNOWN;
	found_pixels = 0;
	/* is row dark or light? */
	for(i=digits[d].x1; i<=digits[d].x2; i++)
	{
	  imlib_image_query_pixel(i,j, &color);
	  lum = get_lum(&color, lt);
	  if(is_pixel_set(lum, thresh)) /* dark */
	  {
	    found_pixels++;
	    if(found_pixels > ignore_pixels)
	    {
	      /* 1 pixels darken row */
	      row = (ssocr_foreground == SSOCR_BLACK) ? DARK : LIGHT;
	    }
	  }
	  else if(row == UNKNOWN)
	  {
	    row = (ssocr_foreground == SSOCR_BLACK) ? LIGHT : DARK;
	  }
	}
	/* save position of digit and draw partition line for DEBUG */
	if((state == ((ssocr_foreground == SSOCR_BLACK)?FIND_DARK:FIND_LIGHT))
	    && (row == ((ssocr_foreground == SSOCR_BLACK) ? DARK : LIGHT)))
	{
	  if(found_top) /* then we are searching for the bottom */
	  {
	    digits[d].y2 = j;
	    state = (ssocr_foreground == SSOCR_BLACK) ? FIND_LIGHT : FIND_DARK;
	    if(use_debug_image) {
	      imlib_context_set_image(debug_image);
	      imlib_context_set_color(0,255,0,255); /* green line */
	      imlib_image_draw_line(digits[d].x1,digits[d].y2,
				    digits[d].x2,digits[d].y2,0);
	      imlib_context_set_image(image);
	    }
	  }
	  else /* found the top line */
	  {
	    digits[d].y1 = j;
	    found_top = 1;
	    state = (ssocr_foreground == SSOCR_BLACK) ? FIND_LIGHT : FIND_DARK;
	    if(use_debug_image) {
	      imlib_context_set_image(debug_image);
	      imlib_context_set_color(0,255,0,255); /* green line */
	      imlib_image_draw_line(digits[d].x1,digits[d].y1,
				    digits[d].x2,digits[d].y1,0);
	      imlib_context_set_image(image);
	    }
	  }
	}
	else if((state ==
	    ((ssocr_foreground == SSOCR_BLACK) ? FIND_LIGHT : FIND_DARK)) &&
	    (row == ((ssocr_foreground == SSOCR_BLACK) ? LIGHT : DARK)))
	{
	  /* found_top has to be 1 because otherwise we were still looking for
	   * dark */
	  digits[d].y2 = j;
	  state = (ssocr_foreground == SSOCR_BLACK) ? FIND_DARK : FIND_LIGHT;
	  if(use_debug_image) {
	    imlib_context_set_image(debug_image);
	    imlib_context_set_color(0,255,0,255); /* green line */
	    imlib_image_draw_line(digits[d].x1,digits[d].y2,
				    digits[d].x2,digits[d].y2,0);
	    imlib_context_set_image(image);
	  }
	}
      }
      /* if we are still looking for light, use the bottom */
      if(state == ((ssocr_foreground == SSOCR_BLACK) ? FIND_LIGHT : FIND_DARK))
      {
	digits[d].y2 = h-1;
	state = (ssocr_foreground == SSOCR_BLACK) ? FIND_DARK : FIND_LIGHT;
	if(use_debug_image) {
	  imlib_context_set_image(debug_image);
	  imlib_context_set_color(0,255,0,255); /* green line */
	  imlib_image_draw_line(digits[d].x1,digits[d].y2,
				digits[d].x2,digits[d].y2,0);
	  imlib_context_set_image(image);
	}
      }
    }
    if(use_debug_image) {
    /* draw rectangles around digits */
      imlib_context_set_image(debug_image);
      imlib_context_set_color(128,128,128,255); /* grey line */
      for(d=0; d<number_of_digits; d++)
      {
	imlib_image_draw_rectangle(digits[d].x1, digits[d].y1,
	    digits[d].x2-digits[d].x1, digits[d].y2-digits[d].y1);
      }
      imlib_context_set_image(image);
    }

    /* now the digits are located and they have to be identified */
    /* iterate over digits */
    for(d=0; d<number_of_digits; d++)
    {
      int middle=0, quarter=0, three_quarters=0; /* scanlines */
      int d_height=0, d_width=0; /* height and width of digit */
      /* if digits[d].digit == D_ONE do nothing */
      if((digits[d].digit == D_UNKNOWN))
      {
	int third=1; /* in which third we are */
	int half;
	found_pixels=0; /* how many pixels are already found */
	d_height = digits[d].y2 - digits[d].y1;
	d_width = digits[d].x2 - digits[d].x1;
	/* check horizontal segments */
	/* vertical scan at x == middle */
	middle = (digits[d].x1 + digits[d].x2) / 2;
	for(j=digits[d].y1; j<=digits[d].y2; j++)
	{
	  imlib_image_query_pixel(middle, j, &color);
	  lum = get_lum(&color, lt);
	  if(is_pixel_set(lum, thresh)) /* dark i.e. pixel is set */
	  {
	    if(use_debug_image) {
	      imlib_context_set_image(debug_image);
	      if(third == 1)
	      {
		imlib_context_set_color(255,0,0,255);
	      }
	      else if(third == 2)
	      {
		imlib_context_set_color(0,255,0,255);
	      }
	      else if(third == 3)
	      {
		imlib_context_set_color(0,0,255,255);
	      }
	      imlib_image_draw_pixel(middle,j,0);
	      imlib_context_set_image(image);
	    }
	    found_pixels++;
	  }
	  /* pixels in first third count towards upper segment */
	  if(j > digits[d].y1 + d_height/3 && third == 1)
	  {
	    if(found_pixels >= need_pixels)
	    {
	      digits[d].digit |= HORIZ_UP; /* add upper segment */
	    }
	    found_pixels = 0;
	    third++;
	  }
	  /* pixels in second third count towards middle segment */
	  else if(j > digits[d].y1 + 2*d_height/3 && third == 2)
	  {
	    if(found_pixels >= need_pixels)
	    {
	      digits[d].digit |= HORIZ_MID; /* add middle segment */
	    }
	    found_pixels = 0;
	    third++;
	  }
	}
	/* found_pixels contains pixels of last third */
	if(found_pixels >= need_pixels)
	{
	  digits[d].digit |= HORIZ_DOWN; /* add lower segment */
	}
	found_pixels = 0;
	/* check upper vertical segments */
	half=1; /* in which half we are */
	quarter = digits[d].y1 + (digits[d].y2 - digits[d].y1) / 4;
	for(i=digits[d].x1; i<=digits[d].x2; i++)
	{
	  imlib_image_query_pixel(i, quarter, &color);
	  lum = get_lum(&color, lt);
	  if(is_pixel_set(lum, thresh)) /* dark i.e. pixel is set */
	  {
	    if(use_debug_image) {
	      if(half == 1)
	      {
		imlib_context_set_color(255,0,0,255);
	      }
	      else if(half == 2)
	      {
		imlib_context_set_color(0,255,0,255);
	      }
	      imlib_context_set_image(debug_image);
	      imlib_image_draw_pixel(i,quarter,0);
	      imlib_context_set_image(image);
	    }
	    found_pixels++;
	  }
	  if(i >= middle && half == 1)
	  {
	    if(found_pixels >= need_pixels)
	    {
	      digits[d].digit |= VERT_LEFT_UP;
	    }
	    found_pixels = 0;
	    half++;
	  }
	}
	if(found_pixels >= need_pixels)
	{
	  digits[d].digit |= VERT_RIGHT_UP;
	}
	found_pixels = 0;
	half = 1;
	/* check lower vertical segments */
	half=1; /* in which half we are */
	three_quarters = digits[d].y1 + 3 * (digits[d].y2 - digits[d].y1) / 4;
	for(i=digits[d].x1; i<=digits[d].x2; i++)
	{
	  imlib_image_query_pixel(i, three_quarters, &color);
	  lum = get_lum(&color, lt);
	  if(is_pixel_set(lum, thresh)) /* dark i.e. pixel is set */
	  {
	    if(use_debug_image) {
	      if(half == 1)
	      {
		imlib_context_set_color(255,0,0,255);
	      }
	      else if(half == 2)
	      {
		imlib_context_set_color(0,255,0,255);
	      }
	      imlib_context_set_image(debug_image);
	      imlib_image_draw_pixel(i,three_quarters,0);
	      imlib_context_set_image(image);
	    }
	    found_pixels++;
	  }
	  if(i >= middle && half == 1)
	  {
	    if(found_pixels >= need_pixels)
	    {
	      digits[d].digit |= VERT_LEFT_DOWN;
	    }
	    found_pixels = 0;
	    half++;
	  }
	}
	if(found_pixels >= need_pixels)
	{
	  digits[d].digit |= VERT_RIGHT_DOWN;
	}
	found_pixels = 0;
      }
    }

    /* print digits */
    for(i=0; i<number_of_digits; i++)
    {
      switch(digits[i].digit)
      {
	case D_ZERO: putchar('0'); break;
	case D_ONE: putchar('1'); break;
	case D_TWO: putchar('2'); break;
	case D_THREE: putchar('3'); break;
	case D_FOUR: putchar('4'); break;
	case D_FIVE: putchar('5'); break;
	case D_SIX: putchar('6'); break;
	case D_SEVEN: putchar('7'); break;
	case D_EIGHT: putchar('8'); break;
	case D_NINE: putchar('9'); break;
	case D_UNKNOWN: putchar(' '); unknown_digit++; break;
	default: putchar('?'); unknown_digit++; break;
      }
    }
    putchar('\n');

    if(use_debug_image) {
      /* get file format for debug image */
      if(output_fmt) { /* use provided format string */
	tmp = output_fmt;
      } else { /* use file name extension */
	tmp = strrchr(debug_image_file, '.');
	if(tmp)
          tmp++;
      }
      if(tmp) {
	if(verbose) printf("saving debug image in %s format to file %s\n",
	                   tmp, debug_image_file);
	imlib_image_set_format(tmp);
      } else { /* use png as default */
	if(verbose) printf("saving debug image in png format to file %s\n",
	                   debug_image_file);
	imlib_image_set_format("png");
      }
      /* write debug image to disk */
      imlib_context_set_image(debug_image);
      imlib_save_image(debug_image_file);
      imlib_free_image_and_decache();
      imlib_context_set_image(image);
    }

    /* clean up... */
    imlib_free_image_and_decache();
  } else {
    fprintf(stderr, "could not load image %s\n", argv[argc-1]);
    exit(99);
  }

  /* determin error code */
  if(unknown_digit) {
    exit(2);
  } else {
    exit(0);
  }
}
