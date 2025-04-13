#pragma once
#include <cairo.h>
#include <glib.h>

#include <cairo/cairo.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>

G_BEGIN_DECLS

typedef enum
{
  GPAINT_FORMAT_ERROR_INSUFFICIENT_MEMORY,
  GPAINT_FORMAT_ERROR_CORRUPT_IMAGE,
  GPAINT_FORMAT_ERROR_FAILED,
  GPAINT_FORMAT_ERROR_AUTH
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
  const char *extensions[3];
  enum AVCodecID codec_id;
} gpaint_formats[] =
  {
    { .extensions = { "png", NULL }, .codec_id = AV_CODEC_ID_PNG },
    { .extensions = { "jpeg", NULL }, .codec_id = AV_CODEC_ID_JPEGLS },
    { .extensions = { "tiff", NULL }, .codec_id = AV_CODEC_ID_TIFF },
    { .extensions = { "gif", NULL }, .codec_id = AV_CODEC_ID_GIF },
    { .extensions = { "bmp", NULL }, .codec_id = AV_CODEC_ID_BMP },
    { .extensions = { "av1", NULL }, .codec_id = AV_CODEC_ID_AV1 },
    { .extensions = { "mp4", NULL }, .codec_id = AV_CODEC_ID_H264 },
    { .extensions = { "webp", NULL }, .codec_id = AV_CODEC_ID_WEBP },
  };

gboolean
save_surfaces_with_ffmpeg (const char         *filename,
                           GList              *surfaces,
                           enum AVCodecID      codec_id,
                           int                 fps,
                           GError            **error);

//int save_image_with_ffmpeg (const char *filename, cairo_surface_t *surface, enum AVCodecID codec_id, int fps);
cairo_surface_t *load_image_to_cairo_surface (const char *filename);

static inline int
save_image (const char *path, cairo_surface_t *surface, int fps, GError **error)
{
  const gchar *ext = strrchr (path, '.');

  if (!ext)
    ext = "png";
  else
    ext++;

  GList surfaces =
    {
      .data = surface,
      .next = NULL,
      .prev = NULL,
    };

  for (size_t i = 0; i < G_N_ELEMENTS (gpaint_formats); i++)
    for (size_t j = 0; gpaint_formats[i].extensions[j]; j++)
      if (g_ascii_strcasecmp (ext, gpaint_formats[i].extensions[j]) == 0)
        return save_surfaces_with_ffmpeg (path, &surfaces, gpaint_formats[i].codec_id, fps, error);

  // TODO
  return FALSE;
}

G_END_DECLS
