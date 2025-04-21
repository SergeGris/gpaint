
#include <gtk/gtk.h>

int main(int argc, char **argv)
{
  /* g_autoptr (GdkPixbuf) pixbuf = gdk_pixbuf_new_from_data (tool->icon->data, */
  /*                                                          tool->icon->colorspace, */
  /*                                                          /\* has alpha *\/ TRUE, */
  /*                                                          tool->icon->bits_per_sample, */
  /*                                                          tool->icon->height, tool->icon->width, */
  /*                                                          tool->icon->rowstride, NULL, NULL); */

  for (int i = 1; i < argc; i++)
    {
    g_autoptr (GdkPixbuf) pixbuf = gdk_pixbuf_new_from_file (argv[i], NULL);

  char *buffer = NULL;
  gsize buffer_size = 0;
  GError *error = NULL;

  gdk_pixbuf_save_to_buffer (
                             pixbuf,
                             &buffer,
                             &buffer_size,
                             "png",
                             &error,
                             "compression", "9", NULL);

  printf(
         "static const struct raw_bitmap %s_data =\n"
         "  {\n"
         "\n"
         "    .data =\n"
         "      {\n",
argv[i]
         );

  for (int y = 0; y < buffer_size; y++)
    {
      printf ("0x%02X , ", (guint) (guint8) buffer[y]);
    }
  printf ("      }\n};\n");
    }
}
