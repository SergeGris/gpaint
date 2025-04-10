#include <cairo.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <tiffio.h>

#include "formats.h"

static int tiff_save (const char *filename, cairo_surface_t *surface, void *settings);
static cairo_surface_t *tiff_load (const char *filename);

static const char *extensions[] = {
  "tiff",
  "tif",
  NULL
};

static const gchar *mime_types[] = {
  "image/tiff",
  NULL,
};

const Codec gpaint_tiff = {
  .name = "tiff",
  .extensions = extensions,
  .mime_types = mime_types,
  .save = tiff_save,
  .load = tiff_load,
};

/* Save a Cairo surface to a TIFF file */
static int
tiff_save (const char *filename, cairo_surface_t *surface, void *settings)
{
  TIFFSettings *tiff_settings = (TIFFSettings *) settings;
  TIFF *tif = TIFFOpen (filename, "w");
  if (!tif)
    {
      fprintf (stderr, "Cannot open TIFF file for writing.\n");
      return -1;
    }
  int width = cairo_image_surface_get_width (surface);
  int height = cairo_image_surface_get_height (surface);
  cairo_format_t format = cairo_image_surface_get_format (surface);
  if (format != CAIRO_FORMAT_ARGB32 && format != CAIRO_FORMAT_RGB24)
    {
      fprintf (stderr, "Unsupported surface format for TIFF.\n");
      TIFFClose (tif);
      return -1;
    }

  /* Set standard TIFF tags */
  TIFFSetField (tif, TIFFTAG_IMAGEWIDTH, width);
  TIFFSetField (tif, TIFFTAG_IMAGELENGTH, height);
  TIFFSetField (tif, TIFFTAG_BITSPERSAMPLE, 8);
  TIFFSetField (tif, TIFFTAG_SAMPLESPERPIXEL, 4);
  TIFFSetField (tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
  TIFFSetField (tif, TIFFTAG_COMPRESSION, tiff_settings ? tiff_settings->compression : COMPRESSION_NONE);
  TIFFSetField (tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
  if (tiff_settings && tiff_settings->dpi > 0)
    {
      TIFFSetField (tif, TIFFTAG_XRESOLUTION, tiff_settings->dpi);
      TIFFSetField (tif, TIFFTAG_YRESOLUTION, tiff_settings->dpi);
      TIFFSetField (tif, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);
    }

  int stride = cairo_image_surface_get_stride (surface);
  unsigned char *data = cairo_image_surface_get_data (surface);
  unsigned char *row = g_malloc (width * 4);
  if (!row)
    {
      TIFFClose (tif);
      return -1;
    }
  for (int y = 0; y < height; y++)
    {
      unsigned char *src = data + y * stride;
      uint32_t *pixels = (uint32_t *) src;
      for (int x = 0; x < width; x++)
        {
          uint32_t pixel = pixels[x];
          unsigned char a = (pixel >> 24) & 0xff;
          unsigned char r = (pixel >> 16) & 0xff;
          unsigned char g = (pixel >> 8) & 0xff;
          unsigned char b = pixel & 0xff;
          row[x * 4 + 0] = r;
          row[x * 4 + 1] = g;
          row[x * 4 + 2] = b;
          row[x * 4 + 3] = a;
        }
      if (TIFFWriteScanline (tif, row, y, 0) < 0)
        {
          free (row);
          TIFFClose (tif);
          return -1;
        }
    }
  g_free (row);
  TIFFClose (tif);
  return 0;
}

/* Load a TIFF file into a Cairo surface */
static cairo_surface_t *
tiff_load (const char *filename)
{
  TIFF *tif = TIFFOpen (filename, "r");
  if (!tif)
    {
      fprintf (stderr, "Cannot open TIFF file for reading.\n");
      return NULL;
    }
  uint32_t width, height;
  TIFFGetField (tif, TIFFTAG_IMAGEWIDTH, &width);
  TIFFGetField (tif, TIFFTAG_IMAGELENGTH, &height);
  cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
  if (cairo_surface_status (surface) != CAIRO_STATUS_SUCCESS)
    {
      TIFFClose (tif);
      return NULL;
    }

  int stride = cairo_image_surface_get_stride (surface);
  unsigned char *data = cairo_image_surface_get_data (surface);
  unsigned char *row = g_malloc (width * 4);

  if (!row)
    {
      cairo_surface_destroy (surface);
      TIFFClose (tif);
      return NULL;
    }

  for (uint32_t y = 0; y < height; y++)
    {
      if (TIFFReadScanline (tif, row, y, 0) < 0)
        {
          free (row);
          cairo_surface_destroy (surface);
          TIFFClose (tif);
          return NULL;
        }

      uint32_t *dest = (uint32_t *) (data + y * stride);

      for (uint32_t x = 0; x < width; x++)
        {
          unsigned char r = row[x * 4 + 0];
          unsigned char g = row[x * 4 + 1];
          unsigned char b = row[x * 4 + 2];
          unsigned char a = row[x * 4 + 3];
          dest[x] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
  g_free (row);
  cairo_surface_mark_dirty (surface);
  TIFFClose (tif);
  return surface;
}
