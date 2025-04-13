
#include <avif/avif.h>
#include <cairo.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "formats.h"

static int avif_save (const char *filename, cairo_surface_t *surface, void *settings);
static cairo_surface_t *avif_load (const char *filename);

static const char *extensions[] = {
  "avif",
  NULL
};

static const gchar *mime_types[] = {
  "image/avif",
  NULL,
};

const Codec gpaint_avif = {
  .name = "avif",
  .extensions = extensions,
  .mime_types = mime_types,
  .save = avif_save,
  .load = avif_load
};

/*
 * avif_save:
 *   Saves the given Cairo surface as an AVIF image.
 *
 *   Parameters:
 *     filename - The output file name.
 *     surface  - The Cairo surface to encode (assumed CAIRO_FORMAT_ARGB32).
 *     settings - Optional settings (AVIFSettings*), or NULL.
 *
 *   Returns 0 on success, -1 on error.
 */
static int
avif_save (const char *filename, cairo_surface_t *surface, void *settings)
{
  int width = cairo_image_surface_get_width (surface);
  int height = cairo_image_surface_get_height (surface);
  int stride = cairo_image_surface_get_stride (surface);
  unsigned char *pixels = cairo_image_surface_get_data (surface);

  /* Create an avifImage in YUV420 format with 8-bit depth.
     This sets the chroma subsampling implicitly.
  */
  avifImage *avifImg = avifImageCreate (width, height, 8, AVIF_PIXEL_FORMAT_YUV420);
  if (!avifImg)
    {
      fprintf (stderr, "Failed to create avifImage.\n");
      return -1;
    }

  /* Wrap the Cairo surface’s pixel data into an avifRGBImage.
     NOTE: Cairo’s CAIRO_FORMAT_ARGB32 data is premultiplied 0xAARRGGBB.
     For a production codec you might need to unpremultiply.
  */
  avifRGBImage rgb;
  avifRGBImageSetDefaults (&rgb, avifImg); // Sets defaults based on avifImg’s properties.
  rgb.format = AVIF_RGB_FORMAT_RGBA;
  rgb.depth = 8;
  rgb.rowBytes = stride; // May be greater than width * 4
  rgb.pixels = pixels;

  avifResult res = avifImageRGBToYUV (avifImg, &rgb);
  if (res != AVIF_RESULT_OK)
    {
      fprintf (stderr, "avifImageRGBToYUV failed: %s\n", avifResultToString (res));
      avifImageDestroy (avifImg);
      return -1;
    }

  /* Create an AVIF encoder */
  avifEncoder *encoder = avifEncoderCreate ();
  if (!encoder)
    {
      fprintf (stderr, "Failed to create avifEncoder.\n");
      avifImageDestroy (avifImg);
      return -1;
    }
  encoder->maxThreads = 4;
  if (settings)
    {
      AVIFSettings *avifSettings = (AVIFSettings *) settings;
      encoder->minQuantizer = avifSettings->quality;
      encoder->maxQuantizer = avifSettings->quality;
    }
  else
    {
      encoder->minQuantizer = 30;
      encoder->maxQuantizer = 30;
    }

  /* Encode the image */
  avifRWData output = AVIF_DATA_EMPTY;
  res = avifEncoderWrite (encoder, avifImg, &output);
  if (res != AVIF_RESULT_OK)
    {
      fprintf (stderr, "avifEncoderWrite failed: %s\n", avifResultToString (res));
      avifRWDataFree (&output);
      avifEncoderDestroy (encoder);
      avifImageDestroy (avifImg);
      return -1;
    }

  /* Write the encoded data to file */
  FILE *fp = fopen (filename, "wb");
  if (!fp)
    {
      fprintf (stderr, "Failed to open file %s for writing.\n", filename);
      avifRWDataFree (&output);
      avifEncoderDestroy (encoder);
      avifImageDestroy (avifImg);
      return -1;
    }
  fwrite (output.data, 1, output.size, fp);
  fclose (fp);

  avifRWDataFree (&output);
  avifEncoderDestroy (encoder);
  avifImageDestroy (avifImg);
  return 0;
}

/*
 * avif_load:
 *   Loads an AVIF image from a file and converts it into a Cairo surface.
 *
 *   Parameters:
 *     filename - The input AVIF file.
 *
 *   Returns a new Cairo surface (CAIRO_FORMAT_ARGB32) on success, or NULL on error.
 */
static cairo_surface_t *
avif_load (const char *filename)
{
  /* Create an AVIF decoder */
  avifDecoder *decoder = avifDecoderCreate ();

  if (decoder == NULL)
    {
      fprintf (stderr, "Failed to create avifDecoder.\n");
      return NULL;
    }

  /* Create an empty avifImage for decoding.
     Passing 0 for width, height, and depth causes the decoder to fill these.
  */
  avifImage *image = avifImageCreate (0, 0, 0, AVIF_PIXEL_FORMAT_YUV420);
  if (image == NULL)
    {
      fprintf (stderr, "Failed to create avifImage for decoding.\n");
      avifDecoderDestroy (decoder);
      return NULL;
    }

  /* Decode the file.
     Note: The new API requires passing the avifImage pointer and the filename.  */
  avifResult res = avifDecoderReadFile (decoder, image, filename);

  if (res != AVIF_RESULT_OK)
    {
      fprintf (stderr, "avifDecoderReadFile failed: %s\n", avifResultToString (res));
      avifImageDestroy (image);
      avifDecoderDestroy (decoder);
      return NULL;
    }

  int width = image->width;
  int height = image->height;

  /* Prepare an avifRGBImage for conversion.
     We will convert the decoded YUV image to RGBA.
  */
  avifRGBImage rgb;
  avifRGBImageSetDefaults (&rgb, image);
  rgb.format = AVIF_RGB_FORMAT_RGBA;
  rgb.depth = 8;
  rgb.rowBytes = width * 4;
  rgb.pixels = malloc (height * rgb.rowBytes);
  if (!rgb.pixels)
    {
      fprintf (stderr, "Failed to allocate memory for RGB conversion.\n");
      avifImageDestroy (image);
      avifDecoderDestroy (decoder);
      return NULL;
    }

  res = avifImageYUVToRGB (image, &rgb);
  if (res != AVIF_RESULT_OK)
    {
      fprintf (stderr, "avifImageYUVToRGB failed: %s\n", avifResultToString (res));
      free (rgb.pixels);
      avifImageDestroy (image);
      avifDecoderDestroy (decoder);
      return NULL;
    }

  /* Create a new Cairo image surface in CAIRO_FORMAT_ARGB32.
     Convert each pixel from non-premultiplied RGBA to premultiplied ARGB.  */
  cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
  unsigned char *cairoData = cairo_image_surface_get_data (surface);
  int cairoStride = cairo_image_surface_get_stride (surface);

  for (int y = 0; y < height; y++)
    {
      uint32_t *destRow = (uint32_t *) (cairoData + y * cairoStride);
      unsigned char *srcRow = ((unsigned char *) rgb.pixels) + y * rgb.rowBytes;
      for (int x = 0; x < width; x++)
        {
          unsigned char r = srcRow[x * 4 + 0];
          unsigned char g = srcRow[x * 4 + 1];
          unsigned char b = srcRow[x * 4 + 2];
          unsigned char a = srcRow[x * 4 + 3];
          float alpha = a / 255.0f;
          uint8_t pr = (uint8_t) (r * alpha);
          uint8_t pg = (uint8_t) (g * alpha);
          uint8_t pb = (uint8_t) (b * alpha);
          destRow[x] = (a << 24) | (pr << 16) | (pg << 8) | (pb);
        }
    }

  cairo_surface_mark_dirty (surface);
  free (rgb.pixels);
  avifImageDestroy (image);
  avifDecoderDestroy (decoder);
  return surface;
}
