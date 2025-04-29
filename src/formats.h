#pragma once

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <cairo.h>
#include <glib.h>

#if HAVE_FFMPEG
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>

#include "utils.h"

G_BEGIN_DECLS

typedef enum
{
  GPAINT_FORMAT_ERROR_INSUFFICIENT_MEMORY,
  GPAINT_FORMAT_ERROR_CORRUPT_IMAGE,
  GPAINT_FORMAT_ERROR_FAILED,
  GPAINT_FORMAT_ERROR_UNSUPPORTED_PIXEL_FORMAT,
  GPAINT_FORMAT_ERROR_FFMPEG_INTERNAL_ERROR,
  GPAINT_FORMAT_ERROR_UNSUPPORTED_CODEC,
} GpaintFormatError;

extern GQuark gpaint_format_error_quark (void);

#define GPAINT_FORMAT_ERROR gpaint_format_error_quark ()

typedef struct
{
  const gchar *name;
  const gchar **extensions;
  const gchar **mime_types;
  const guchar *signature;
  const gchar *description;
  int (*save) (const char *filename, cairo_surface_t *surface, void *settings);
  cairo_surface_t *(*load) (const char *filename);
} Codec;

static const struct
{
  const char *extensions[4];
  enum AVCodecID codec_id;
  enum AVPixelFormat pix_fmt;
  const char *default_options;
  gboolean supports_animation;
} gpaint_formats[] =
  {
    { .extensions = { "png" },                .codec_id = AV_CODEC_ID_PNG,   .pix_fmt = AV_PIX_FMT_RGBA, .default_options = "compression_level=9" },
    { .extensions = { "jpeg", "jpg", "jpe" }, .codec_id = AV_CODEC_ID_MJPEG, .pix_fmt = AV_PIX_FMT_YUVJ420P },
    { .extensions = { "tiff", "tif" },        .codec_id = AV_CODEC_ID_TIFF,  .pix_fmt = AV_PIX_FMT_RGBA,    },
    { .extensions = { "gif" },                .codec_id = AV_CODEC_ID_GIF,   .pix_fmt = AV_PIX_FMT_RGB8, .supports_animation = TRUE },
    { .extensions = { "bmp" },                .codec_id = AV_CODEC_ID_BMP,   .pix_fmt = AV_PIX_FMT_BGRA },
    { .extensions = { "xbm" },                .codec_id = AV_CODEC_ID_XBM,   .pix_fmt = AV_PIX_FMT_MONOWHITE },
  };

//{ .extensions = { "avif" },        .codec_id = AV_CODEC_ID_AV1,  .pix_fmt = AV_PIX_FMT_YUV420P   },
//{ .extensions = { "mp4" },         .codec_id = AV_CODEC_ID_H264, .pix_fmt = AV_PIX_FMT_YUV420P   },
// TODO mpeg4, hdr, hevc, apng, xbm, jpeg, av1, webp, xpm

extern gboolean save_surfaces_with_ffmpeg (const char *filename, GList *surfaces, enum AVCodecID codec_id, int fps, const char *options_string, GError **error);

// int save_image_with_ffmpeg (const char *filename, cairo_surface_t *surface,
// enum AVCodecID codec_id, int fps);
extern cairo_surface_t *load_image_to_cairo_surface (const char *filename);
#endif

static inline int
load_image (cairo_surface_t **surface, const char *path)
{
#if HAVE_FFMPEG
  *surface = load_image_to_cairo_surface (path);
  return TRUE;
#else
  *surface = cairo_image_surface_create_from_png (path);
  cairo_status_t status = cairo_surface_status (*surface);

  if (status != CAIRO_STATUS_SUCCESS)
    {
      /* Handle the error (e.g., show a message to the user) */
      g_printerr ("Failed to load image: %s\n", cairo_status_to_string (status));
      g_clear_pointer (surface, cairo_surface_destroy);
    }
  return TRUE;
#endif
}

static inline int
save_image (const char *path, cairo_surface_t *surface, int fps, GError **error)
{
#if HAVE_FFMPEG
  const gchar *ext = strrchr (path, '.');

  if (!ext)
    ext = "png";
  else
    ext++;

  GList surfaces = {
    .data = surface,
    .next = NULL,
    .prev = NULL,
  };

  for (size_t i = 0; i < G_N_ELEMENTS (gpaint_formats); i++)
    for (size_t j = 0; gpaint_formats[i].extensions[j]; j++)
      if (g_ascii_strcasecmp (ext, gpaint_formats[i].extensions[j]) == 0)
        return save_surfaces_with_ffmpeg (path, &surfaces, gpaint_formats[i].codec_id, fps, gpaint_formats[i].default_options, error);

  abort ();
  // TODO
  return FALSE;
#else
  const gchar *ext = strrchr (path, '.');

  if (!ext)
    ext = "png";
  else
    {
    ext++;
    if (g_ascii_strcasecmp (ext, "png") != 0)
      {
      g_warning ("supported only png format");
      return FALSE;
      }
    }

  cairo_status_t status = cairo_surface_write_to_png (surface, path);
  if (status != CAIRO_STATUS_SUCCESS)
    g_warning ("Failed to save PNG image: %s", cairo_status_to_string (status));
  return TRUE;
#endif
}

G_END_DECLS
