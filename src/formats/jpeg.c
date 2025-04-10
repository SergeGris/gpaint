
#include <stddef.h>
#include <stdio.h>

#include <cairo.h>
#include <jpeglib.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "formats.h"

int jpeg_save (const char *filename, cairo_surface_t *surface, void *settings);
cairo_surface_t *jpeg_load (const char *filename);

static const gchar *extensions[] = {
  "jpg",
  "jpeg",
  "jpe",
  NULL
};

static const gchar *mime_types[] = {
  "image/jpeg",
  NULL,
};

const Codec gpaint_jpeg = {
  .name = "jpeg",
  .extensions = extensions,
  .mime_types = mime_types,
  .save = jpeg_save,
  .load = jpeg_load,
};

/* --- Custom error handler for libjpeg --- */
struct JPEGErrorManager
{
  struct jpeg_error_mgr pub; /* "public" fields */
  jmp_buf setjmp_buffer;     /* for return to caller */
};

METHODDEF (void)
my_error_exit (j_common_ptr cinfo)
{
  struct JPEGErrorManager *err = (struct JPEGErrorManager *) cinfo->err;
  /* Return control to the setjmp point */
  longjmp (err->setjmp_buffer, 1);
}

/* --- JPEG Save Implementation --- */
int
jpeg_save (const char *filename, cairo_surface_t *surface, void *settings)
{
  JPEGSettings *jpeg_settings = (JPEGSettings *) settings;
  FILE *fp = fopen (filename, "wb");
  if (!fp)
    {
      fprintf (stderr, "Unable to open %s for writing.\n", filename);
      return -1;
    }

  struct jpeg_compress_struct cinfo;
  struct JPEGErrorManager jerr;
  cinfo.err = jpeg_std_error (&jerr.pub);
  jerr.pub.error_exit = my_error_exit;
  if (setjmp (jerr.setjmp_buffer))
    {
      jpeg_destroy_compress (&cinfo);
      fclose (fp);
      fprintf (stderr, "JPEG compression error.\n");
      return -1;
    }

  jpeg_create_compress (&cinfo);
  jpeg_stdio_dest (&cinfo, fp);

  /* Get image dimensions and determine pixel format from the Cairo surface */
  int width = cairo_image_surface_get_width (surface);
  int height = cairo_image_surface_get_height (surface);
  cairo_format_t format = cairo_image_surface_get_format (surface);
  int pixel_components = 0; /* 1 for grayscale, 3 for RGB */

  if (format == CAIRO_FORMAT_A8)
    {
      pixel_components = 1; /* Grayscale */
    }
  else if (format == CAIRO_FORMAT_ARGB32 || format == CAIRO_FORMAT_RGB24)
    {
      pixel_components = 3; /* Color (drop alpha) */
    }
  else
    {
      fprintf (stderr, "Unsupported surface format for JPEG.\n");
      jpeg_destroy_compress (&cinfo);
      fclose (fp);
      return -1;
    }

  cinfo.image_width = width;
  cinfo.image_height = height;
  cinfo.input_components = pixel_components;
  cinfo.in_color_space = (pixel_components == 1) ? JCS_GRAYSCALE : JCS_RGB;

  jpeg_set_defaults (&cinfo);

  /* Apply settings if provided */
  if (jpeg_settings)
    {
      jpeg_set_quality (&cinfo, jpeg_settings->quality, TRUE);
      if (jpeg_settings->progressive)
        jpeg_simple_progression (&cinfo);
      if (jpeg_settings->optimize)
        cinfo.optimize_coding = TRUE;
      if (jpeg_settings->dpi > 0)
        {
          cinfo.density_unit = 1; /* inches */
          cinfo.X_density = (unsigned int) jpeg_settings->dpi;
          cinfo.Y_density = (unsigned int) jpeg_settings->dpi;
        }
    }

  jpeg_start_compress (&cinfo, TRUE);

  int stride = cairo_image_surface_get_stride (surface);
  unsigned char *cairo_data = cairo_image_surface_get_data (surface);

  /* Allocate a buffer for one scanline in RGB or grayscale format */
  JSAMPROW row_pointer[1];
  unsigned char *row = malloc (width * pixel_components);
  if (!row)
    {
      fprintf (stderr, "Memory allocation failed for scanline buffer.\n");
      jpeg_destroy_compress (&cinfo);
      fclose (fp);
      return -1;
    }

  for (int y = 0; y < height; y++)
    {
      unsigned char *src = cairo_data + y * stride;
      if (format == CAIRO_FORMAT_ARGB32)
        {
          uint32_t *pixel_ptr = (uint32_t *) src;
          for (int x = 0; x < width; x++)
            {
              uint32_t pixel = pixel_ptr[x];
              /* Cairo's ARGB32: 0xAARRGGBB. Ignore alpha. */
              unsigned char r = (pixel >> 16) & 0xff;
              unsigned char g = (pixel >> 8) & 0xff;
              unsigned char b = pixel & 0xff;
              if (pixel_components == 3)
                {
                  row[x * 3 + 0] = r;
                  row[x * 3 + 1] = g;
                  row[x * 3 + 2] = b;
                }
            }
        }
      else if (format == CAIRO_FORMAT_RGB24)
        {
          uint32_t *pixel_ptr = (uint32_t *) src;
          for (int x = 0; x < width; x++)
            {
              uint32_t pixel = pixel_ptr[x];
              /* RGB24 is stored in 32-bit with unused alpha (assumed opaque) */
              unsigned char r = (pixel >> 16) & 0xff;
              unsigned char g = (pixel >> 8) & 0xff;
              unsigned char b = pixel & 0xff;
              if (pixel_components == 3)
                {
                  row[x * 3 + 0] = r;
                  row[x * 3 + 1] = g;
                  row[x * 3 + 2] = b;
                }
            }
        }
      else if (format == CAIRO_FORMAT_A8)
        {
          /* For grayscale, copy the 8-bit intensity values */
          for (int x = 0; x < width; x++)
            {
              row[x] = src[x];
            }
        }
      row_pointer[0] = row;
      jpeg_write_scanlines (&cinfo, row_pointer, 1);
    }

  free (row);
  jpeg_finish_compress (&cinfo);
  jpeg_destroy_compress (&cinfo);
  fclose (fp);
  return 0;
}

/* --- JPEG Load Implementation --- */
cairo_surface_t *
jpeg_load (const char *filename)
{
  FILE *fp = fopen (filename, "rb");
  if (!fp)
    {
      fprintf (stderr, "Unable to open %s for reading.\n", filename);
      return NULL;
    }

  struct jpeg_decompress_struct cinfo;
  struct JPEGErrorManager jerr;
  cinfo.err = jpeg_std_error (&jerr.pub);
  jerr.pub.error_exit = my_error_exit;
  if (setjmp (jerr.setjmp_buffer))
    {
      jpeg_destroy_decompress (&cinfo);
      fclose (fp);
      fprintf (stderr, "JPEG decompression error.\n");
      return NULL;
    }

  jpeg_create_decompress (&cinfo);
  jpeg_stdio_src (&cinfo, fp);
  jpeg_read_header (&cinfo, TRUE);
  jpeg_start_decompress (&cinfo);

  int width = cinfo.output_width;
  int height = cinfo.output_height;
  int channels = cinfo.output_components; /* 1 for grayscale, 3 for color */

  /* We'll always convert the image to CAIRO_FORMAT_ARGB32. */
  cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
  if (cairo_surface_status (surface) != CAIRO_STATUS_SUCCESS)
    {
      fprintf (stderr, "Failed to create Cairo surface.\n");
      jpeg_destroy_decompress (&cinfo);
      fclose (fp);
      return NULL;
    }

  int stride = cairo_image_surface_get_stride (surface);
  unsigned char *cairo_data = cairo_image_surface_get_data (surface);

  JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray) ((j_common_ptr) &cinfo, JPOOL_IMAGE, width * channels, 1);

  while (cinfo.output_scanline < cinfo.output_height)
    {
      jpeg_read_scanlines (&cinfo, buffer, 1);
      int y = cinfo.output_scanline - 1;
      unsigned char *dst = cairo_data + y * stride;
      if (channels == 3)
        {
          for (int x = 0; x < width; x++)
            {
              unsigned char r = buffer[0][x * 3 + 0];
              unsigned char g = buffer[0][x * 3 + 1];
              unsigned char b = buffer[0][x * 3 + 2];
              ((uint32_t *) dst)[x] = (0xff << 24) | (r << 16) | (g << 8) | b;
            }
        }
      else if (channels == 1)
        {
          for (int x = 0; x < width; x++)
            {
              unsigned char gray = buffer[0][x];
              ((uint32_t *) dst)[x] = (0xff << 24) | (gray << 16) | (gray << 8) | gray;
            }
        }
    }

  cairo_surface_mark_dirty (surface);
  jpeg_finish_decompress (&cinfo);
  jpeg_destroy_decompress (&cinfo);
  fclose (fp);
  return surface;
}
