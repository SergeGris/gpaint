#include <cairo.h>
#include <gif_lib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "formats.h"

static int gif_save (const char *filename, cairo_surface_t *surface, void *settings);
static cairo_surface_t *gif_load (const char *filename);

static const char *extensions[] = {
  "gif",
  NULL,
};

static const gchar *mime_types[] = {
  "image/gif",
  NULL,
};

const Codec gpaint_gif = {
  .name = "gif",
  .extensions = extensions,
  .mime_types = mime_types,
  .save = gif_save,
  .load = gif_load
};

/*
 * gif_save:
 *   Saves the Cairo surface as a single-frame GIF image.
 *
 *   Parameters:
 *     filename - The output file name.
 *     surface  - The Cairo surface to encode. (Assumed to be CAIRO_FORMAT_ARGB32)
 *     settings - Optional pointer to GIFSettings (currently ignored).
 *
 *   Returns 0 on success or -1 on error.
 */
int
gif_save (const char *filename, cairo_surface_t *surface, void *settings)
{
  (void) settings; // No settings used in this simple example

  int width = cairo_image_surface_get_width (surface);
  int height = cairo_image_surface_get_height (surface);
  int stride = cairo_image_surface_get_stride (surface);
  unsigned char *data = cairo_image_surface_get_data (surface);

  /* Create a global color map using a fixed 216-color (web-safe) palette.
     The palette is built by dividing each RGB channel into 6 levels.
  */
  int numColors = 6 * 6 * 6; // 216 colors
  ColorMapObject *colorMap = GifMakeMapObject (numColors, NULL);
  if (!colorMap)
    {
      fprintf (stderr, "gif_save: Failed to create color map.\n");
      return -1;
    }
  int color = 0;
  for (int r = 0; r < 6; r++)
    {
      for (int g = 0; g < 6; g++)
        {
          for (int b = 0; b < 6; b++)
            {
              colorMap->Colors[color].Red = r * 51; // 0, 51, …, 255
              colorMap->Colors[color].Green = g * 51;
              colorMap->Colors[color].Blue = b * 51;
              color++;
            }
        }
    }

  int error;
  GifFileType *gif = EGifOpenFileName (filename, FALSE, &error);
  if (!gif)
    {
      fprintf (stderr, "gif_save: Failed to open file for writing: %s\n", GifErrorString (error));
      GifFreeMapObject (colorMap);
      return -1;
    }

  /* Write the screen descriptor. Bit depth is set to 8, and the color map is global. */
  if (EGifPutScreenDesc (gif, width, height, 8, 0, colorMap) == GIF_ERROR)
    {
      fprintf (stderr, "gif_save: Error writing screen descriptor: %s\n", GifErrorString (gif->Error));
      EGifCloseFile (gif, &error);
      GifFreeMapObject (colorMap);
      return -1;
    }

  /* Write the image descriptor.
     We write the entire image at the origin.
  */
  if (EGifPutImageDesc (gif, 0, 0, width, height, FALSE, NULL) == GIF_ERROR)
    {
      fprintf (stderr, "gif_save: Error writing image descriptor: %s\n", GifErrorString (gif->Error));
      EGifCloseFile (gif, &error);
      GifFreeMapObject (colorMap);
      return -1;
    }

  /* Loop over every pixel in the Cairo surface.
     For each pixel, extract R, G, B values (ignoring alpha), and map them into the palette:
       r_index = r / 51, g_index = g / 51, b_index = b / 51
       index = r_index * 36 + g_index * 6 + b_index
  */
  for (int y = 0; y < height; y++)
    {
      unsigned char *rowData = (unsigned char *) (data + y * stride);
      for (int x = 0; x < width; x++)
        {
          uint32_t pixel = ((uint32_t *) rowData)[x];
          unsigned char r = (pixel >> 16) & 0xFF;
          unsigned char g = (pixel >> 8) & 0xFF;
          unsigned char b = pixel & 0xFF;
          int r_index = r / 51; // 0..5
          int g_index = g / 51;
          int b_index = b / 51;
          int index = r_index * 36 + g_index * 6 + b_index; // should be 0..215
          if (EGifPutPixel (gif, index) == GIF_ERROR)
            {
              fprintf (stderr, "gif_save: Error writing pixel at (%d,%d): %s\n", x, y, GifErrorString (gif->Error));
              EGifCloseFile (gif, &error);
              GifFreeMapObject (colorMap);
              return -1;
            }
        }
    }

  if (EGifCloseFile (gif, &error) == GIF_ERROR)
    {
      fprintf (stderr, "gif_save: Error closing GIF file: %s\n", GifErrorString (error));
      GifFreeMapObject (colorMap);
      return -1;
    }
  GifFreeMapObject (colorMap);
  return 0;
}

/*
 * gif_load:
 *   Loads the first frame of a GIF image from a file and returns it as a Cairo surface.
 *   The surface is in CAIRO_FORMAT_ARGB32 with opaque alpha (0xff).
 *
 *   Parameters:
 *     filename - The input GIF file name.
 *
 *   Returns a new Cairo surface, or NULL on error.
 */
cairo_surface_t *
gif_load (const char *filename)
{
  int error;
  GifFileType *gif = DGifOpenFileName (filename, &error);
  if (!gif)
    {
      fprintf (stderr, "gif_load: Failed to open GIF file for reading: %s\n", GifErrorString (error));
      return NULL;
    }
  if (DGifSlurp (gif) == GIF_ERROR)
    {
      fprintf (stderr, "gif_load: DGifSlurp failed: %s\n", GifErrorString (gif->Error));
      DGifCloseFile (gif, &error);
      return NULL;
    }
  if (gif->ImageCount < 1)
    {
      fprintf (stderr, "gif_load: No images found in GIF file.\n");
      DGifCloseFile (gif, &error);
      return NULL;
    }
  SavedImage *frame = &gif->SavedImages[0];
  GifImageDesc *imgDesc = &frame->ImageDesc;
  int width = imgDesc->Width;
  int height = imgDesc->Height;
  cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
  if (cairo_surface_status (surface) != CAIRO_STATUS_SUCCESS)
    {
      DGifCloseFile (gif, &error);
      return NULL;
    }
  int stride = cairo_image_surface_get_stride (surface);
  unsigned char *dest = cairo_image_surface_get_data (surface);

  /* Use the local colormap if present, otherwise the global colormap */
  ColorMapObject *colorMap = (imgDesc->ColorMap != NULL) ? imgDesc->ColorMap : gif->SColorMap;
  if (!colorMap)
    {
      fprintf (stderr, "gif_load: No color map available.\n");
      cairo_surface_destroy (surface);
      DGifCloseFile (gif, &error);
      return NULL;
    }

  /* frame->RasterBits holds the indices for each pixel.
     Loop through each pixel and convert using the colormap.
  */
  for (int y = 0; y < height; y++)
    {
      uint32_t *destRow = (uint32_t *) (dest + y * stride);
      for (int x = 0; x < width; x++)
        {
          int index = frame->RasterBits[y * width + x];
          GifColorType color = colorMap->Colors[index];
          destRow[x] = (0xff << 24) | (color.Red << 16) | (color.Green << 8) | color.Blue;
        }
    }
  cairo_surface_mark_dirty (surface);
  DGifCloseFile (gif, &error);
  return surface;
}
