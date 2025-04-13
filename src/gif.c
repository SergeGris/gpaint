#include <cairo.h>
#include <gif_lib.h>
#include <gtk/gtk.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Построение палитры: web-safe (6*6*6=216 цветов)
static ColorMapObject *
build_websafe_palette (void)
{
  int numColors = 216;
  ColorMapObject *map = GifMakeMapObject (numColors, NULL);
  if (!map)
    return NULL;
  int idx = 0;
  for (int r = 0; r < 6; r++)
    {
      for (int g = 0; g < 6; g++)
        {
          for (int b = 0; b < 6; b++)
            {
              map->Colors[idx].Red = r * 51;
              map->Colors[idx].Green = g * 51;
              map->Colors[idx].Blue = b * 51;
              idx++;
            }
        }
    }
  return map;
}

// Преобразование пикселя ARGB32 в индекс из палитры web-safe
static int
pixel_to_index (uint32_t pixel)
{
  int r = (pixel >> 16) & 0xff;
  int g = (pixel >> 8) & 0xff;
  int b = pixel & 0xff;
  int ri = r / 51;
  int gi = g / 51;
  int bi = b / 51;
  return ri * 36 + gi * 6 + bi;
}

// Запись анимированного GIF с множеством функций.
// frames - массив cairo_surface_t* (каждый кадр в CAIRO_FORMAT_ARGB32)
// nframes - число кадров, delay - задержка в сотых долях секунды (например, 50 => 0.5 с)
int
gif_save_anim (const char *filename, cairo_surface_t **frames, int nframes, int delay)
{
  if (nframes <= 0)
    return -1;
  int error;
  GifFileType *gif = EGifOpenFileName (filename, false, &error);
  if (!gif)
    {
      fprintf (stderr, "Ошибка открытия файла для записи: %s\n", GifErrorString (error));
      return -1;
    }
  int width = cairo_image_surface_get_width (frames[0]);
  int height = cairo_image_surface_get_height (frames[0]);

  ColorMapObject *globalMap = build_websafe_palette ();
  if (!globalMap)
    {
      fprintf (stderr, "Ошибка создания палитры.\n");
      EGifCloseFile (gif, &error);
      return -1;
    }
  // Запись дескриптора экрана (глобальная палитра)
  if (EGifPutScreenDesc (gif, width, height, 8, 0, globalMap) == GIF_ERROR)
    {
      fprintf (stderr, "Ошибка записи дескриптора экрана: %s\n", GifErrorString (gif->Error));
      EGifCloseFile (gif, &error);
      GifFreeMapObject (globalMap);
      return -1;
    }

  // Для каждого кадра: запишем Graphics Control Extension и сам кадр
  for (int i = 0; i < nframes; i++)
    {
      // Graphics Control Extension (указывает задержку, метод удаления и прозрачность)

      unsigned char gce[4];
      // Установка метода удаления: 2 (восстановить фон), без прозрачности
      gce[0] = 0x04 | (2 << 2); // 0x04 бит прозрачности unset, disposal = 2
      // Задержка кадра
      gce[1] = delay & 0xFF;
      gce[2] = (delay >> 8) & 0xFF;
      gce[3] = 0;
      if (EGifPutExtensionLeader (gif, 0x21) == GIF_ERROR)
        {
          fprintf (stderr, "Ошибка записи лидера расширения: %s", GifErrorString (gif->Error));
          EGifCloseFile (gif, &error);
          GifFreeMapObject (globalMap);
          return -1;
        }
      if (EGifPutExtensionBlock (gif, 4, gce) == GIF_ERROR)
        {
          fprintf (stderr, "Ошибка записи блока расширения: %s", GifErrorString (gif->Error));
          EGifCloseFile (gif, &error);
          GifFreeMapObject (globalMap);
          return -1;
        }
      if (EGifPutExtensionTrailer (gif) == GIF_ERROR)
        {
          fprintf (stderr, "Ошибка завершения расширения: %s", GifErrorString (gif->Error));
          EGifCloseFile (gif, &error);
          GifFreeMapObject (globalMap);
          return -1;
        }

      // Запись дескриптора изображения
      if (EGifPutImageDesc (gif, 0, 0, width, height, false, NULL) == GIF_ERROR)
        {
          fprintf (stderr, "Ошибка записи дескриптора изображения: %s", GifErrorString (gif->Error));
          EGifCloseFile (gif, &error);
          GifFreeMapObject (globalMap);
          return -1;
        }

      // Преобразование каждого пикселя кадра в индекс палитры (байты)
      unsigned char *indices = malloc (width * height);
      if (!indices)
        {
          fprintf (stderr, "Ошибка выделения памяти для индексов.");
          EGifCloseFile (gif, &error);
          GifFreeMapObject (globalMap);
          return -1;
        }
      cairo_surface_t *surf = frames[i];
      int stride = cairo_image_surface_get_stride (surf);
      unsigned char *data = cairo_image_surface_get_data (surf);
      for (int y = 0; y < height; y++)
        {
          uint32_t *row = (uint32_t *) (data + y * stride);
          for (int x = 0; x < width; x++)
            {
              indices[y * width + x] = (unsigned char) pixel_to_index (row[x]);
            }
        }
      // Запись пикселов по строкам
      for (int y = 0; y < height; y++)
        {
          for (int x = 0; x < width; x++)
            {
              if (EGifPutPixel (gif, indices[y * width + x]) == GIF_ERROR)
                {
                  fprintf (stderr, "Ошибка записи пикселя: %s", GifErrorString (gif->Error));
                  free (indices);
                  EGifCloseFile (gif, &error);
                  GifFreeMapObject (globalMap);
                  return -1;
                }
            }
        }
      free (indices);
    }

  if (EGifCloseFile (gif, &error) == GIF_ERROR)
    {
      fprintf (stderr, "Ошибка закрытия GIF: %s", GifErrorString (error));
      GifFreeMapObject (globalMap);
      return -1;
    }
  GifFreeMapObject (globalMap);
  return 0;
}

// Пример использования: создаёт окно GTK, сохраняет анимированный GIF и выводит его на канвасе
int
main (void)
{
  int nframes = 3;
  cairo_surface_t **frames = malloc (nframes * sizeof (cairo_surface_t *));
  if (!frames)
    return EXIT_FAILURE;

  for (int i = 0; i < nframes; i++)
    {
      frames[i] = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 200, 200);
      cairo_t *cr = cairo_create (frames[i]);
      double r = (double) i / nframes;
      cairo_set_source_rgb (cr, r, 0.5, 1.0 - r);
      cairo_paint (cr);
      cairo_destroy (cr);
    }

  if (gif_save_anim ("output.gif", frames, nframes, 50) == 0)
    printf ("Анимированный GIF успешно сохранён.");
  else
    printf ("Не удалось сохранить анимированный GIF.");

  for (int i = 0; i < nframes; i++)
    {
      cairo_surface_destroy (frames[i]);
    }
  free (frames);

  GtkWidget *window = gtk_window_new ();
  gtk_window_set_title (GTK_WINDOW (window), "Canvas with Animated GIF");
  gtk_window_set_default_size (GTK_WINDOW (window), 220, 220);

  GtkWidget *image = gtk_image_new_from_file ("output.gif");
  gtk_window_set_child (GTK_WINDOW (window), image);

  gtk_window_present (GTK_WINDOW (window));

  return 0;
}

/* ``` */

/* --- */

/* Этот код реализует GIF‑кодек с поддержкой анимации (несколько кадров, GCE‑блок с задержкой) через giflib. Он безопасно проверяет ошибки, использует фиксированную палитру (web‑safe с 216 цветами) и выводит результат в GTK‑окне с помощью GtkImage. Код можно расширять дополнительными параметрами (например, прозрачностью, диспозалом, локальной цветовой картой) при необходимости. */
