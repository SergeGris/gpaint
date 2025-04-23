#include <gtk/gtk.h>

void
dump_texture_as_c_code (const char *icon)
{
  g_autoptr (GdkTexture) texture = gdk_texture_new_from_filename (icon, NULL);

  if (!texture)
    {
      g_warning ("Failed to create GdkTexture from filename: %s", icon);
      return;
    }

  // Convert GdkTexture to a GdkPixbuf (available in recent GTK4 versions)
  g_autoptr (GdkPixbuf) pixbuf = gdk_pixbuf_get_from_texture (texture);
  if (!pixbuf)
    {
      g_printerr ("Failed to convert texture to pixbuf.\n");
      return;
    }
  int width = gdk_pixbuf_get_width (pixbuf);
  int height = gdk_pixbuf_get_height (pixbuf);
  int rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  int bits_per_sample = gdk_pixbuf_get_bits_per_sample (pixbuf);
  guchar *data = gdk_pixbuf_get_pixels (pixbuf);
  gchar *s = strdup(icon);
  gchar *dot = strchr(s, '.');
  if (dot)
    *dot = '\0';

 printf(
   "static const struct raw_bitmap %s_data =\n"
   "  {\n"
   "    .height = %d,\n"
   "    .width = %d,\n"
   "    .stride = %d,\n"
   "    .colorspace = GDK_COLORSPACE_RGB,\n"
   "    .bits_per_sample = %d,\n"
   "\n"
   "    .data =\n"
   "      {\n",
   s, height, width, 8, bits_per_sample
        );
  for (int y = 0; y < height; y++)
    {
      printf("        ");
      for (int x = 0; x < rowstride; x++)
        printf ("0x%02X,%s", data[y * rowstride + x], (x == rowstride / 2 - 1) ? "\n        " : " ");

      printf ("\n");
    }
  printf ("      }\n};\n");
}

int
main (int argc, char **argv)
{
  for (int i = 1; i < argc; i++)
    dump_texture_as_c_code (argv[i]);
}
