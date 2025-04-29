/* color_entity.h */

#include <glib-object.h>
#include <cairo.h>

/* Declare an abstract base type GpaintColor */
G_DECLARE_DERIVABLE_TYPE(GpaintColor, gpaint_color, GPAINT, COLOR, GObject);

/* Virtual-method class struct */
struct _GpaintColorClass {
    GObjectClass parent_class;
    /* Virtual methods for setting/getting color and comparing */
    void      (*set_color_at) (GpaintColor *self, cairo_surface_t *surface, int x, int y);
    void      (*get_color_at) (GpaintColor *self, cairo_surface_t *surface, int x, int y);
    gboolean  (*compare)      (GpaintColor *self, GpaintColor *other);
};

/* Public wrappers to call the virtual methods */
void      gpaint_color_set_color_at(GpaintColor *self, cairo_surface_t *surface, int x, int y);
void      gpaint_color_get_color_at(GpaintColor *self, cairo_surface_t *surface, int x, int y);
gboolean  gpaint_color_compare   (GpaintColor *self, GpaintColor *other);

/* gpaint_color_argb32.h */

/* #include "gpaint_color.h" */

/* Declare a final type GpaintColorARGB32 */
#define GPAINT_COLOR_ARGB32_TYPE (gpaint_color_argb32_get_type())
G_DECLARE_FINAL_TYPE(GpaintColorARGB32, gpaint_color_argb32, GPAINT, COLOR_ARGB32, GpaintColor)

/* Instance structure: includes parent and RGBA fields */
struct _GpaintColorARGB32 {
    GpaintColor parent_instance;
    guint8 r, g, b, a;
};

/* gpaint_color_a8.h */

/* #include "gpaint_color.h" */

#define GPAINT_COLOR_TYPE_A8 (gpaint_color_a8_get_type())
G_DECLARE_FINAL_TYPE(GpaintColorA8, gpaint_color_a8, GPAINT, COLOR_A8, GpaintColor)

/* Instance struct: single alpha component */
struct _GpaintColorA8 {
    GpaintColor parent_instance;
    guint8 alpha;
};
