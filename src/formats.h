#pragma once

#include <cairo.h>
#include <glib.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>

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

static inline int
save_image (const char *path, cairo_surface_t *surface, int fps, GError **error)
{
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
}

G_END_DECLS
