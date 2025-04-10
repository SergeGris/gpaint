
#pragma once

#include <cairo.h>
#include <glib.h>

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
  /* Save a cairo_surface_t to file using optional settings */
  int (*save) (const char *filename, cairo_surface_t *surface, void *settings);
  /* Load a cairo_surface_t from file */
  cairo_surface_t *(*load) (const char *filename);
} Codec;

typedef struct
{
  int compression_level; /* 0 (no compression) to 9 (max compression) */
  int filter_flags;      /* PNG filtering (e.g., PNG_FILTER_NONE, PNG_FILTER_SUB, etc.) */
  int interlace;         /* 0 for no interlace, PNG_INTERLACE_ADAM7 for interlaced output */
  double gamma;          /* Gamma correction value (e.g., 0.45455 for sRGB) */
  double dpi;            /* Resolution in dots per inch (optional) */
  int ignore_alpha;      /* If nonzero and surface has alpha, output as RGB (or gray) without alpha */
  int optimize_palette;  /* If nonzero, attempt to optimize output as a palette image */
                         /* Additional settings can be added here */
} PNGSettings;

typedef struct
{
  int quality;          /* Quality 1 (worst) to 100 (best) */
  gboolean progressive; /* Nonzero to enable progressive mode */
  gboolean optimize;    /* Nonzero to optimize Huffman tables */
  double dpi;           /* Dots per inch (optional, > 0 to set) */
                        /* Additional settings can be added here */
} JPEGSettings;

typedef struct
{
  int compression; /* e.g., COMPRESSION_NONE, COMPRESSION_LZW, etc. */
  double dpi;      /* Resolution in dots per inch */
} TIFFSettings;

typedef struct
{
  int quality; // Quality (quantizer) value (0-100, lower is better quality)
} AVIFSettings;

/* Settings structure for GIF codec (currently unused – placeholder) */
typedef struct {
    int dummy; // You can later add quality or dithering parameters
} GIFSettings;

extern int save_image (const char *filename, cairo_surface_t *surface, void *settings);
extern cairo_surface_t *load_image (const char *filename);

G_END_DECLS
