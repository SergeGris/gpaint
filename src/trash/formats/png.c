#include <cairo.h>
#include <png.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "formats.h"

static int png_save (const char *filename, cairo_surface_t *surface, void *settings);
static cairo_surface_t *png_load (const char *filename);

static const char *extensions[] = {
  "png",
  NULL,
};

static const gchar *mime_types[] = {
  "image/png",
  NULL,
};

const Codec gpaint_png = {
  .name = "png",
  .extensions = extensions,
  .mime_types = mime_types,
  .save = png_save,
  .load = png_load
};

static int
png_save (const char *filename, cairo_surface_t *surface, void *settings)
{
  PNGSettings *png_settings = (PNGSettings *) settings;
  FILE *fp = fopen (filename, "wb");

  if (!fp)
    {
      fprintf (stderr, "Unable to open %s for writing.\n", filename);
      return -1;
    }

  /* Initialize libpng write structures */
  png_structp png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

  if (!png_ptr)
    {
      fclose (fp);
      fprintf (stderr, "png_create_write_struct failed.\n");
      return -1;
    }

  png_infop info_ptr = png_create_info_struct (png_ptr);

  if (!info_ptr)
    {
      fclose (fp);
      png_destroy_write_struct (&png_ptr, (png_infopp) NULL);
      fprintf (stderr, "png_create_info_struct failed.\n");
      return -1;
    }

  if (setjmp (png_jmpbuf (png_ptr)))
    {
      /* Error during PNG creation */
      png_destroy_write_struct (&png_ptr, &info_ptr);
      fclose (fp);
      fprintf (stderr, "Error during PNG creation.\n");
      return -1;
    }

  png_init_io (png_ptr, fp);

  /* Determine surface properties */
  int width = cairo_image_surface_get_width (surface);
  int height = cairo_image_surface_get_height (surface);
  cairo_format_t format = cairo_image_surface_get_format (surface);
  int channels = 0;
  int color_type = 0;

  /* Setup based on the cairo surface type */
  if (format == CAIRO_FORMAT_ARGB32 || format == CAIRO_FORMAT_RGB24)
    {
      /* Although both are stored in 32-bit, for RGB24 we treat alpha as fully opaque */
      channels = 4;
      color_type = PNG_COLOR_TYPE_RGBA;
    }
  else if (format == CAIRO_FORMAT_A8)
    {
      channels = 1;
      color_type = PNG_COLOR_TYPE_GRAY;
    }
  else
    {
      fprintf (stderr, "Unsupported surface format.\n");
      png_destroy_write_struct (&png_ptr, &info_ptr);
      fclose (fp);
      return -1;
    }

  /* Adjust for settings: ignore alpha if requested */
  if (png_settings && png_settings->ignore_alpha && channels == 4)
    {
      channels = 3;
      color_type = PNG_COLOR_TYPE_RGB;
    }

  /* Write PNG header */
  png_set_IHDR (png_ptr, info_ptr, width, height, 8, color_type, (png_settings ? png_settings->interlace : PNG_INTERLACE_NONE), PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

  /* Additional settings */
  if (png_settings)
    {
      png_set_compression_level (png_ptr, png_settings->compression_level);
      png_set_filter (png_ptr, 0, png_settings->filter_flags);
      if (png_settings->gamma > 0)
        png_set_gAMA (png_ptr, info_ptr, png_settings->gamma);
      if (png_settings->dpi > 0)
        {
          /* Convert dpi to pixels per meter: 1 meter = 39.3701 inches */
          int ppm = (int) (png_settings->dpi * 39.3701);
          png_set_pHYs (png_ptr, info_ptr, ppm, ppm, PNG_RESOLUTION_METER);
        }
      /* Note: Palette optimization would require additional processing
         to convert truecolor images to palette images. This code does not
         implement that conversion but reserves the flag for future use. */
    }

  png_write_info (png_ptr, info_ptr);

  /* Get raw image data from the cairo surface */
  int stride = cairo_image_surface_get_stride (surface);
  unsigned char *cairo_data = cairo_image_surface_get_data (surface);

  /* Allocate temporary row buffer */
  unsigned char *row = malloc (width * channels);
  if (!row)
    {
      fprintf (stderr, "Memory allocation failed for row buffer.\n");
      png_destroy_write_struct (&png_ptr, &info_ptr);
      fclose (fp);
      return -1;
    }

  /* Write out each row, converting the data based on the surface type */
  for (int y = 0; y < height; y++)
    {
      unsigned char *src = cairo_data + y * stride;

      if (format == CAIRO_FORMAT_ARGB32)
        {
          uint32_t *pixel_ptr = (uint32_t *) src;
          for (int x = 0; x < width; x++)
            {
              uint32_t pixel = pixel_ptr[x];
              unsigned char a = (pixel >> 24) & 0xff;
              unsigned char r = (pixel >> 16) & 0xff;
              unsigned char g = (pixel >> 8) & 0xff;
              unsigned char b = pixel & 0xff;
              if (png_settings && png_settings->ignore_alpha)
                {
                  row[x * 3 + 0] = r;
                  row[x * 3 + 1] = g;
                  row[x * 3 + 2] = b;
                }
              else
                {
                  row[x * 4 + 0] = r;
                  row[x * 4 + 1] = g;
                  row[x * 4 + 2] = b;
                  row[x * 4 + 3] = a;
                }
            }
        }
      else if (format == CAIRO_FORMAT_RGB24)
        {
          uint32_t *pixel_ptr = (uint32_t *) src;
          for (int x = 0; x < width; x++)
            {
              uint32_t pixel = pixel_ptr[x];
              /* For RGB24, we assume alpha is implicitly opaque */
              unsigned char r = (pixel >> 16) & 0xff;
              unsigned char g = (pixel >> 8) & 0xff;
              unsigned char b = pixel & 0xff;

              if (png_settings && png_settings->ignore_alpha)
                {
                  row[x * 3 + 0] = r;
                  row[x * 3 + 1] = g;
                  row[x * 3 + 2] = b;
                }
              else
                {
                  row[x * 4 + 0] = r;
                  row[x * 4 + 1] = g;
                  row[x * 4 + 2] = b;
                  row[x * 4 + 3] = 0xff; // opaque
                }
            }
        }
      else if (format == CAIRO_FORMAT_A8)
        {
          /* Grayscale: simply copy the 8-bit alpha values */
          for (int x = 0; x < width; x++)
            row[x] = src[x];
        }
      png_write_row (png_ptr, row);
    }
  free (row);

  png_write_end (png_ptr, info_ptr);
  png_destroy_write_struct (&png_ptr, &info_ptr);
  fclose (fp);
  return 0;
}

/* --- PNG Load Implementation --- */
static cairo_surface_t *
png_load (const char *filename)
{
  return cairo_image_surface_create_from_png (filename);
  /* FILE *fp = fopen(filename, "rb"); */
  /* if (!fp) { */
  /*     fprintf(stderr, "Unable to open %s for reading.\n", filename); */
  /*     return NULL; */
  /* } */

  /* /\* Initialize libpng read structures *\/ */
  /* png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL); */
  /* if (!png_ptr) { */
  /*     fclose(fp); */
  /*     return NULL; */
  /* } */
  /* png_infop info_ptr = png_create_info_struct(png_ptr); */
  /* if (!info_ptr) { */
  /*     fclose(fp); */
  /*     png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL); */
  /*     return NULL; */
  /* } */
  /* if (setjmp(png_jmpbuf(png_ptr))) { */
  /*     png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL); */
  /*     fclose(fp); */
  /*     fprintf(stderr, "Error during PNG read.\n"); */
  /*     return NULL; */
  /* } */

  /* png_init_io(png_ptr, fp); */
  /* png_read_info(png_ptr, info_ptr); */

  /* int width = png_get_image_width(png_ptr, info_ptr); */
  /* int height = png_get_image_height(png_ptr, info_ptr); */
  /* png_byte color_type = png_get_color_type(png_ptr, info_ptr); */
  /* png_byte bit_depth = png_get_bit_depth(png_ptr, info_ptr); */

  /* /\* Force palette, grayscale, etc. to 8-bit RGBA or gray *\/ */
  /* if (bit_depth == 16) */
  /*     png_set_strip_16(png_ptr); */
  /* if (color_type == PNG_COLOR_TYPE_PALETTE) */
  /*     png_set_palette_to_rgb(png_ptr); */
  /* if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) */
  /*     png_set_expand_gray_1_2_4_to_8(png_ptr); */
  /* if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) */
  /*     png_set_tRNS_to_alpha(png_ptr); */

  /* /\* For RGB images, ensure we have an alpha channel if needed *\/ */
  /* if (color_type == PNG_COLOR_TYPE_RGB) */
  /*     png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER); */
  /* if (color_type == PNG_COLOR_TYPE_GRAY) */
  /*     png_set_gray_to_rgb(png_ptr); */

  /* png_read_update_info(png_ptr, info_ptr); */

  /* /\* Create a cairo surface to hold the image. */
  /*  * We use CAIRO_FORMAT_ARGB32 which is 32-bit (premultiplied). *\/ */
  /* cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height); */
  /* if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) { */
  /*     fprintf(stderr, "Failed to create cairo surface.\n"); */
  /*     png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL); */
  /*     fclose(fp); */
  /*     return NULL; */
  /* } */

  /* int stride = cairo_image_surface_get_stride(surface); */
  /* unsigned char *cairo_data = cairo_image_surface_get_data(surface); */

  /* /\* Allocate row pointers for libpng *\/ */
  /* png_bytep *row_pointers = malloc(sizeof(png_bytep) * height); */
  /* for (int y = 0; y < height; y++) { */
  /*     row_pointers[y] = malloc(png_get_rowbytes(png_ptr, info_ptr)); */
  /* } */

  /* png_read_image(png_ptr, row_pointers); */
  /* fclose(fp); */

  /* /\* Convert PNG RGBA rows to cairo ARGB32 format *\/ */
  /* for (int y = 0; y < height; y++) { */
  /*     png_bytep row = row_pointers[y]; */
  /*     uint32_t *dest = (uint32_t*)(cairo_data + y * stride); */
  /*     for (int x = 0; x < width; x++) { */
  /*         int idx = x * 4; */
  /*         unsigned char r = row[idx + 0]; */
  /*         unsigned char g = row[idx + 1]; */
  /*         unsigned char b = row[idx + 2]; */
  /*         unsigned char a = row[idx + 3]; */
  /*         /\* Pack into ARGB format (note: proper premultiplication is recommended) *\/ */
  /*         dest[x] = (a << 24) | (r << 16) | (g << 8) | b; */
  /*     } */
  /*     free(row); */
  /* } */
  /* free(row_pointers); */

  /* cairo_surface_mark_dirty(surface); */
  /* png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL); */
  /* return surface; */
}
