/* // cursor_loader.c */

/* #include <gdk-pixbuf/gdk-pixbuf.h> */
/* #include <gdk/gdk.h> */

/* #if HAVE_XCURSOR */
/* /\* X11: use libXcursor *\/ */
/* #include <X11/Xcursor/Xcursor.h> /\* for XcursorLibraryLoadCursor() *\/ */
/* #include <gdk/x11/gdkx.h>        /\* for gdk_x11_display_get_xdisplay() *\/ */
/* #endif */

/* #if HAVE_WAYLAND_CURSOR */
/* /\* Wayland: use libwayland-cursor *\/ */
/* #include <gdk/wayland/gdkwayland.h> /\* for gdk_wayland_display_get_wl_display() *\/ */
/* #include <wayland-cursor.h> */
/* #endif */

/* #ifdef _WIN32 */
/* /\* Windows: load system cursor via Win32 API *\/ */
/* #include <windows.h> */
/* #include <winuser.h> */
/* #endif */

/* /\* Prototype to avoid missing‐declaration warning *\/ */
/* GdkTexture *cursor_loader_get_texture (GdkDisplay *display, */
/*                                        const char *cursor_name); */

/* /\** */
/*  * cursor_loader_get_texture: */
/*  * @display: a #GdkDisplay */
/*  * @cursor_name: e.g. "pointer", "text" */
/*  * */
/*  * Attempts X11, then Wayland, then PNG fallback. */
/*  *\/ */
/* GdkTexture * */
/* cursor_loader_get_texture (GdkDisplay *display, */
/*                            const char *cursor_name) */
/* { */
/*   GdkTexture *tex = NULL; */

/* #if HAVE_XCURSOR */
/*   if (GDK_IS_X11_DISPLAY (display)) */
/*     { */
/*       Display *xdpy = gdk_x11_display_get_xdisplay (display); */
/*       XcursorLibraryLoadCursors (xdpy, NULL); */
/*       XcursorImage *xcimg = NULL; // TODO XcursorLibraryLoadCursor (xdpy, cursor_name); */
/*       if (xcimg) */
/*         { */
/*           GdkPixbuf *pix = gdk_pixbuf_new_from_data ( */
/*               (const guint8 *) xcimg->pixels, */
/*               GDK_COLORSPACE_RGB, TRUE, 8, */
/*               xcimg->width, xcimg->height, */
/*               xcimg->width * 4, */
/*               NULL, NULL); */
/*           if (pix) */
/*             { */
/*               tex = gdk_texture_new_for_pixbuf (pix); */
/*               g_object_unref (pix); */
/*             } */
/*           XcursorImageDestroy (xcimg); */
/*           if (tex) */
/*             return tex; */
/*         } */
/*     } */
/* #endif */

/* #if HAVE_WAYLAND_CURSOR */
/*   if (GDK_IS_WAYLAND_DISPLAY (display)) */
/*     { */
/*       struct wl_display *wl_dpy = */
/*           gdk_wayland_display_get_wl_display (display); */
/*       struct wl_cursor_theme *theme = */
/*           wl_cursor_theme_load (NULL, 24, NULL); */
/*       if (theme) */
/*         { */
/*           struct wl_cursor *wc = */
/*               wl_cursor_theme_get_cursor (theme, cursor_name); */
/*           if (wc && wc->images && wc->images[0]) */
/*             { */
/*               struct wl_cursor_image *img = wc->images[0]; */
/*               void *data = wl_cursor_image_get_buffer (img); */
/*               GdkPixbuf *pix = gdk_pixbuf_new_from_data ( */
/*                   (const guint8 *) data, */
/*                   GDK_COLORSPACE_RGB, TRUE, 8, */
/*                   img->width, img->height, */
/*                   img->width * 4, */
/*                   NULL, NULL); */
/*               if (pix) */
/*                 { */
/*                   tex = gdk_texture_new_for_pixbuf (pix); */
/*                   g_object_unref (pix); */
/*                 } */
/*             } */
/*           wl_cursor_theme_destroy (theme); */
/*           if (tex) */
/*             return tex; */
/*         } */
/*     } */
/* #endif */

/* #ifdef _WIN32 */
/*   /\* map common names *\/ */
/*   LPCSTR idc = IDC_ARROW; */
/*   if (strcmp (cursor_name, "text") == 0) */
/*     idc = IDC_IBEAM; */
/*   else if (strcmp (cursor_name, "wait") == 0) */
/*     idc = IDC_WAIT; */
/*   HCURSOR hcur = LoadCursorA (NULL, idc); */
/*   if (hcur) */
/*     { */
/*       ICONINFO info; */
/*       if (GetIconInfo (hcur, &info)) */
/*         { */
/*           BITMAP bmp; */
/*           GetObject (info.hbmColor ? info.hbmColor : info.hbmMask, */
/*                      sizeof (bmp), &bmp); */
/*           HDC dc = GetDC (NULL); */
/*           BITMAPINFO bi = { */
/*             .bmiHeader = { */
/*                           .biSize = sizeof (bi.bmiHeader), */
/*                           .biWidth = bmp.bmWidth, */
/*                           .biHeight = -bmp.bmHeight, /\* top-down *\/ */
/*                 .biPlanes = 1, */
/*                           .biBitCount = 32, */
/*                           .biCompression = BI_RGB } */
/*           }; */
/*           int bytes = bmp.bmWidth * bmp.bmHeight * 4; */
/*           void *buffer = malloc (bytes); */
/*           if (buffer && GetDIBits (dc, info.hbmColor, 0, bmp.bmHeight, */
/*                                    buffer, &bi, DIB_RGB_COLORS)) */
/*             { */
/*               GdkPixbuf *pix = gdk_pixbuf_new_from_data ( */
/*                   (const guint8 *) buffer, */
/*                   GDK_COLORSPACE_RGB, TRUE, 8, */
/*                   bmp.bmWidth, bmp.bmHeight, */
/*                   bmp.bmWidth * 4, */
/*                   free, NULL); */
/*               if (pix) */
/*                 { */
/*                   tex = gdk_texture_new_for_pixbuf (pix); */
/*                   g_object_unref (pix); */
/*                 } */
/*             } */
/*           ReleaseDC (NULL, dc); */
/*           DeleteObject (info.hbmColor); */
/*           DeleteObject (info.hbmMask); */
/*           if (tex) */
/*             return tex; */
/*         } */
/*     } */
/* #endif */

/*   /\* Fallback: load a PNG file from CURSOR_FALLBACK_DIR *\/ */
/*   { */
/*     const char *dir = getenv ("CURSOR_FALLBACK_DIR"); */
/*     if (dir && cursor_name) */
/*       { */
/*         char path[1024]; */
/*         snprintf (path, sizeof (path), "%s/%s.png", dir, cursor_name); */
/*         GError *err = NULL; */
/*         GdkPixbuf *pix = gdk_pixbuf_new_from_file (path, &err); */
/*         if (!err && pix) */
/*           { */
/*             tex = gdk_texture_new_for_pixbuf (pix); */
/*             g_object_unref (pix); */
/*             return tex; */
/*           } */
/*         g_clear_error (&err); */
/*       } */
/*   } */

/*   return NULL; */
/* } */
