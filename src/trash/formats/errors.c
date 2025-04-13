#include "formats.h"

static const GErrorEntry gpaint_format_error_entries[] = {
  { GPAINT_FORMAT_ERROR_INSUFFICIENT_MEMORY, "Insufficient memory to load image" },
  { GPAINT_FORMAT_ERROR_CORRUPT_IMAGE, "Corrupt image" },
  { GPAINT_FORMAT_ERROR_FAILED, "TODO" },
  { GPAINT_FORMAT_ERROR_AUTH, "Authentication required" }
};

GQuark
gpaint_format_error_quark (void)
{
  return g_error_domain_register_static ("gpaint-format-error-domain",
                                         gpaint_format_error_entries,
                                         G_N_ELEMENTS (gpaint_format_error_entries));
}
