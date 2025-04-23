
#include "config.h"

#include "formats.h"

#if HAS_PNG
extern const Codec gpaint_png;
#endif

#if HAS_JPEG
extern const Codec gpaint_jpeg;
#endif

#if HAS_TIFF
extern const Codec gpaint_tiff;
#endif

#if HAS_AVIF
extern const Codec gpaint_avif;
#endif

#if HAS_GIF
extern const Codec gpaint_gif;
#endif

static const Codec *gpaint_codecs[] = {
#if HAS_PNG
  &gpaint_png,
#endif
#if HAS_JPEG
  &gpaint_jpeg,
#endif
#if HAS_TIFF
  &gpaint_tiff,
#endif
#if HAS_AVIF
  &gpaint_avif,
#endif
#if HAS_GIF
  &gpaint_gif,
#endif
};

int
save_image (const char *filename, cairo_surface_t *surface, void *settings)
{
  g_assert (filename != NULL);

  const gchar *ext = strrchr (filename, '.');
  g_assert (ext);

  for (size_t i = 0; i < G_N_ELEMENTS (gpaint_codecs); i++)
    for (size_t j = 0; gpaint_codecs[i]->extensions[j] != NULL; j++)
      if (g_ascii_strcasecmp (ext + 1, gpaint_codecs[i]->extensions[j]) == 0)
        return gpaint_codecs[i]->save (filename, surface, settings);

  return -1;
}

cairo_surface_t *
load_image (const char *filename)
{
  g_assert (filename != NULL);

  const gchar *ext = strrchr (filename, '.');
  g_assert (ext);

  for (size_t i = 0; i < G_N_ELEMENTS (gpaint_codecs); i++)
    for (size_t j = 0; gpaint_codecs[i]->extensions[j] != NULL; j++)
      if (g_ascii_strcasecmp (ext + 1, gpaint_codecs[i]->extensions[j]) == 0)
        return gpaint_codecs[i]->load (filename);

  return NULL;
}
