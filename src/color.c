/* gpaint_color.c */

#include "color.h"

#define GPAINT_TYPE_COLOR (gpaint_color_get_type())
G_DEFINE_TYPE(GpaintColor, gpaint_color, G_TYPE_OBJECT)

/* Initialize the GpaintColor class; mark it abstract if desired */
static void gpaint_color_class_init(GpaintColorClass *klass) {
    /* No default implementations; subclasses should override */
}

static void gpaint_color_init(GpaintColor *self) {
    /* Base instance init: nothing needed here */
}

/* Public method implementations that dispatch to the vfuncs */
void gpaint_color_set_color_at(GpaintColor *self, cairo_surface_t *surface, int x, int y) {
    g_return_if_fail(GPAINT_IS_COLOR(self));
    GpaintColorClass *klass = GPAINT_COLOR_GET_CLASS(self);
    if (klass->set_color_at)
        klass->set_color_at(self, surface, x, y);
}

void gpaint_color_get_color_at(GpaintColor *self, cairo_surface_t *surface, int x, int y) {
    g_return_if_fail(GPAINT_IS_COLOR(self));
    GpaintColorClass *klass = GPAINT_COLOR_GET_CLASS(self);
    if (klass->get_color_at)
        klass->get_color_at(self, surface, x, y);
}

gboolean gpaint_color_compare(GpaintColor *self, GpaintColor *other) {
    g_return_val_if_fail(GPAINT_IS_COLOR(self) && GPAINT_IS_COLOR(other), FALSE);
    GpaintColorClass *klass = GPAINT_COLOR_GET_CLASS(self);
    if (klass->compare)
        return klass->compare(self, other);
    return FALSE;
}

/* gpaint_color_argb32.c */

/* #include "gpaint_color_argb32.h" */

/* Define the type and subclass init */
#define GPAINT_TYPE_COLOR_ARGB32 (gpaint_color_argb32_get_type())
G_DEFINE_TYPE(GpaintColorARGB32, gpaint_color_argb32, GPAINT_TYPE_COLOR)

/* vfunc: set the pixel at (x,y) to this object's RGBA color */
static void gpaint_color_argb32_set_color_at(GpaintColor *self,
                                             cairo_surface_t *surface,
                                             int x, int y) {
    GpaintColorARGB32 *obj = GPAINT_COLOR_ARGB32(self);
    /* Ensure we have an image surface of ARGB32 format */
    if (cairo_surface_get_type(surface) != CAIRO_SURFACE_TYPE_IMAGE ||
        cairo_image_surface_get_format(surface) != CAIRO_FORMAT_ARGB32) {
        g_warning("Surface is not CAIRO_FORMAT_ARGB32");
        return;
    }
    /* Access raw pixel buffer */
    guint8 *data = cairo_image_surface_get_data(surface);
    int stride = cairo_image_surface_get_stride(surface);
    /* Compute the offset for pixel (x,y) */
    guint8 *pixel = data + y * stride + x * 4;
    /* Pack channels into premultiplied ARGB (alpha-highest byte) */
    pixel[0] = obj->b; /* Blue */
    pixel[1] = obj->g; /* Green */
    pixel[2] = obj->r; /* Red */
    pixel[3] = obj->a; /* Alpha (transparent=0) */
    cairo_surface_mark_dirty(surface);
}

/* vfunc: read the pixel at (x,y) into this object's RGBA fields */
static void gpaint_color_argb32_get_color_at(GpaintColor *self,
                                             cairo_surface_t *surface,
                                             int x, int y) {
    GpaintColorARGB32 *obj = GPAINT_COLOR_ARGB32(self);
    if (cairo_surface_get_type(surface) != CAIRO_SURFACE_TYPE_IMAGE ||
        cairo_image_surface_get_format(surface) != CAIRO_FORMAT_ARGB32) {
        g_warning("Surface is not CAIRO_FORMAT_ARGB32");
        return;
    }
    guint8 *data = cairo_image_surface_get_data(surface);
    int stride = cairo_image_surface_get_stride(surface);
    guint8 *pixel = data + y * stride + x * 4;
    /* Unpack (premultiplied) ARGB components */
    obj->b = pixel[0];
    obj->g = pixel[1];
    obj->r = pixel[2];
    obj->a = pixel[3];
}

/* vfunc: compare two ARGB32 colors for equality */
static gboolean gpaint_color_argb32_compare(GpaintColor *self, GpaintColor *other) {
    if (!GPAINT_IS_COLOR_ARGB32(other))
        return FALSE;
    GpaintColorARGB32 *c1 = GPAINT_COLOR_ARGB32(self);
    GpaintColorARGB32 *c2 = GPAINT_COLOR_ARGB32(other);
    return (c1->r == c2->r && c1->g == c2->g && c1->b == c2->b && c1->a == c2->a);
}

/* Class initialization: override the virtual methods */
static void gpaint_color_argb32_class_init(GpaintColorARGB32Class *klass) {
    GpaintColorClass *entity_class = GPAINT_COLOR_CLASS(klass);
    entity_class->set_color_at = gpaint_color_argb32_set_color_at;
    entity_class->get_color_at = gpaint_color_argb32_get_color_at;
    entity_class->compare      = gpaint_color_argb32_compare;
}

static void gpaint_color_argb32_init(GpaintColorARGB32 *self) {
    /* Instance init: default color, e.g. opaque black */
    self->r = self->g = self->b = 0;
    self->a = 255;
}

/* gpaint_color_a8.c */

/* #include "gpaint_color_a8.h" */

#define GPAINT_TYPE_COLOR_A8 (gpaint_color_a8_get_type())
G_DEFINE_TYPE(GpaintColorA8, gpaint_color_a8, GPAINT_TYPE_COLOR)

/* set_color_at: write the alpha value into the A8 surface */
static void gpaint_color_a8_set_color_at(GpaintColor *self,
                                         cairo_surface_t *surface,
                                         int x, int y) {
    GpaintColorA8 *obj = GPAINT_COLOR_A8(self);
    if (cairo_surface_get_type(surface) != CAIRO_SURFACE_TYPE_IMAGE ||
        cairo_image_surface_get_format(surface) != CAIRO_FORMAT_A8) {
        g_warning("Surface is not CAIRO_FORMAT_A8");
        return;
    }
    guint8 *data = cairo_image_surface_get_data(surface);
    int stride = cairo_image_surface_get_stride(surface);
    data[y * stride + x] = obj->alpha;
    cairo_surface_mark_dirty(surface);
}

/* get_color_at: read the alpha value from the A8 surface */
static void gpaint_color_a8_get_color_at(GpaintColor *self,
                                         cairo_surface_t *surface,
                                         int x, int y) {
    GpaintColorA8 *obj = GPAINT_COLOR_A8(self);
    if (cairo_surface_get_type(surface) != CAIRO_SURFACE_TYPE_IMAGE ||
        cairo_image_surface_get_format(surface) != CAIRO_FORMAT_A8) {
        g_warning("Surface is not CAIRO_FORMAT_A8");
        return;
    }
    guint8 *data = cairo_image_surface_get_data(surface);
    int stride = cairo_image_surface_get_stride(surface);
    obj->alpha = data[y * stride + x];
}

/* compare: two A8 colors are equal if alpha matches */
static gboolean gpaint_color_a8_compare(GpaintColor *self, GpaintColor *other) {
    if (!GPAINT_IS_COLOR_A8(other))
        return FALSE;
    GpaintColorA8 *c1 = GPAINT_COLOR_A8(self);
    GpaintColorA8 *c2 = GPAINT_COLOR_A8(other);
    return (c1->alpha == c2->alpha);
}

static void gpaint_color_a8_class_init(GpaintColorA8Class *klass) {
    GpaintColorClass *entity_class = GPAINT_COLOR_CLASS(klass);
    entity_class->set_color_at = gpaint_color_a8_set_color_at;
    entity_class->get_color_at = gpaint_color_a8_get_color_at;
    entity_class->compare      = gpaint_color_a8_compare;
}

static void gpaint_color_a8_init(GpaintColorA8 *self) {
    /* Default alpha = fully opaque */
    self->alpha = 255;
}
