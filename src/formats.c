#include <cairo/cairo.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <string.h>

#include "formats.h"

// Map cairo format to AVPixelFormat
static inline enum AVPixelFormat
get_target_pix_fmt (enum AVCodecID codec_id, cairo_format_t format)
{
  /* const enum AVPixelFormat *pix_fmts = codec->pix_fmts; */
  /* if (pix_fmts) { */
  /*     printf("Supported pixel formats:\n"); */
  /*     for (int i = 0; pix_fmts[i] != AV_PIX_FMT_NONE; i++) { */
  /*         printf("  %s\n", av_get_pix_fmt_name(pix_fmts[i])); */
  /*     } */
  /* } else { */
  /*     printf("No specific pixel formats supported.\n"); */
  /* } */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"

  switch (codec_id)
    {
    case AV_CODEC_ID_BMP:
      return AV_PIX_FMT_BGRA;

    case AV_CODEC_ID_H264:
      return AV_PIX_FMT_YUV444P;
    case AV_CODEC_ID_WEBP:
      return AV_PIX_FMT_BGRA;
    case AV_CODEC_ID_AV1:
      return AV_PIX_FMT_YUV444P;
      // TODO
    case AV_CODEC_ID_GIF:
      return AV_PIX_FMT_RGB8;
    case AV_CODEC_ID_JPEG2000:
    case AV_CODEC_ID_MJPEG:
    case AV_CODEC_ID_JPEGLS:
    case AV_CODEC_ID_JPEGXL:
      return AV_PIX_FMT_RGB24; // JPEG doesn't support alpha
    default:
      return AV_PIX_FMT_RGBA;
    }

#pragma GCC diagnostic pop
}

// TODO
static void
apply_default_background (cairo_surface_t *surface, double r, double g, double b)
{
  cairo_t *cr = cairo_create (surface);
  cairo_set_operator (cr, CAIRO_OPERATOR_DEST_OVER);
  cairo_set_source_rgb (cr, r, g, b);
  cairo_paint (cr);
  cairo_destroy (cr);
}

// Convert Cairo surface to AVFrame
static AVFrame *
convert_surface_to_frame (cairo_surface_t *surface, AVCodecContext *ctx)
{
  int width = cairo_image_surface_get_width (surface);
  int height = cairo_image_surface_get_height (surface);
  int stride = cairo_image_surface_get_stride (surface);
  cairo_format_t format = cairo_image_surface_get_format (surface);
  uint8_t *data = cairo_image_surface_get_data (surface);

  AVFrame *frame = av_frame_alloc ();
  frame->format = ctx->pix_fmt;
  frame->width = width;
  frame->height = height;

  if (av_frame_get_buffer (frame, 32) < 0)
    return NULL;

  // TODO
  enum AVPixelFormat src_fmt = format == CAIRO_FORMAT_ARGB32 ? AV_PIX_FMT_BGRA : AV_PIX_FMT_RGBA;

  struct SwsContext *sws_ctx = sws_getContext (width, height, src_fmt, width, height, ctx->pix_fmt, SWS_BICUBIC, NULL, NULL, NULL);

  const uint8_t *src_slice[] = { data };
  int src_stride[] = { stride };

  sws_scale (sws_ctx, src_slice, src_stride, 0, height, frame->data, frame->linesize);
  sws_freeContext (sws_ctx);

  return frame;
}

gboolean
save_surfaces_with_ffmpeg (const char *filename, GList *surfaces, enum AVCodecID codec_id, int fps, GError **error)
{
  g_return_val_if_fail (filename && surfaces, FALSE);

  AVFormatContext *fmt_ctx = NULL;
  AVCodecContext *codec_ctx = NULL;
  AVStream *stream = NULL;
  AVPacket pkt;
  av_init_packet (&pkt);
  gboolean success = FALSE;
  const AVCodec *codec = avcodec_find_encoder (codec_id);
  if (!codec)
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_NOENT, "Encoder not found for codec ID %u", codec_id);
      return FALSE;
    }

  codec_ctx = avcodec_alloc_context3 (codec);
  if (!codec_ctx)
    {
      g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Failed to allocate codec context");
      return FALSE;
    }

  /* Determine appropriate pixel format from our gpaint_formats array */
  enum AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;
  for (size_t i = 0; i < G_N_ELEMENTS (gpaint_formats); i++)
    {
      if (codec_id == gpaint_formats[i].codec_id)
        {
          pix_fmt = gpaint_formats[i].pix_fmt;
          break;
        }
    }
  if (pix_fmt == AV_PIX_FMT_NONE)
    {
      g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Unsupported pixel format for codec");
      goto cleanup;
    }

  /* Use the first surface for dimensions */
  cairo_surface_t *ref_surface = (cairo_surface_t *) surfaces->data;
  codec_ctx->width = cairo_image_surface_get_width (ref_surface);
  codec_ctx->height = cairo_image_surface_get_height (ref_surface);
  codec_ctx->pix_fmt = pix_fmt;
  codec_ctx->time_base = (AVRational) { 1, fps };
  codec_ctx->framerate = (AVRational) { fps, 1 };
  /* Optionally: set gop_size, max_b_frames etc. if desired */

  if (avcodec_open2 (codec_ctx, codec, NULL) < 0)
    {
      g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Failed to open codec");
      goto cleanup;
    }

  /* Allocate the output format context with the image2 muxer.
     (Make sure the output filename uses a suitable pattern if writing multiple
     images.) */
  if (avformat_alloc_output_context2 (&fmt_ctx, NULL, NULL, filename) < 0 || !fmt_ctx)
    {
      g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Failed to allocate format context");
      goto cleanup;
    }

  /* Create a new stream for the output */
  stream = avformat_new_stream (fmt_ctx, NULL);
  if (!stream)
    {
      g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Failed to create output stream");
      goto cleanup;
    }
  stream->time_base = codec_ctx->time_base;
  if (avcodec_parameters_from_context (stream->codecpar, codec_ctx) < 0)
    {
      g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Failed to copy codec parameters");
      goto cleanup;
    }

  /* Open the output file if the muxer requires it */
  if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE))
    {
      if (avio_open (&fmt_ctx->pb, filename, AVIO_FLAG_WRITE) < 0)
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Failed to open file: %s", filename);
          goto cleanup;
        }
    }

  /* If writing a single image (only one surface),
     set the "update" option so the muxer will update the same file */
  AVDictionary *opts = NULL;

  if (g_list_length (surfaces) == 1)
    av_dict_set (&opts, "update", "1", 0);

  /* Write the header once – do not call this twice */
  if (avformat_write_header (fmt_ctx, &opts) < 0)
    {
      g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Failed to write header");
      goto cleanup;
    }

  av_dict_free (&opts);

  /* Process each surface */
  for (GList *l = surfaces; l != NULL; l = l->next)
    {
      cairo_surface_t *surface = (cairo_surface_t *) l->data;
      AVFrame *frame = convert_surface_to_frame (surface, codec_ctx);
      if (!frame)
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Failed to convert surface to frame");
          goto cleanup;
        }
      frame->pts = g_list_index (surfaces, l); /* Using the list index as pts */
      if (avcodec_send_frame (codec_ctx, frame) < 0)
        {
          g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Failed to send frame");
          av_frame_free (&frame);
          goto cleanup;
        }
      while (avcodec_receive_packet (codec_ctx, &pkt) == 0)
        {
          pkt.stream_index = stream->index;
          av_interleaved_write_frame (fmt_ctx, &pkt);
          av_packet_unref (&pkt);
        }
      av_frame_free (&frame);
    }

  av_write_trailer (fmt_ctx);
  success = TRUE;

cleanup:
  if (codec_ctx)
    avcodec_free_context (&codec_ctx);
  if (fmt_ctx)
    {
      if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE))
        avio_closep (&fmt_ctx->pb);
      avformat_free_context (fmt_ctx);
    }
  return success;
}

/* gboolean */
/* save_surfaces_with_ffmpeg (const char *filename, */
/*                            GList *surfaces, */
/*                            enum AVCodecID codec_id, */
/*                            int fps, */
/*                            GError **error) */
/* { */
/*   g_return_val_if_fail (filename && surfaces, FALSE); */

/*   // TODO gboolean is_video = (g_list_length (surfaces) > 1); */
/*   AVFormatContext *fmt_ctx = NULL; */
/*   AVCodecContext *codec_ctx = NULL; */
/*   AVStream *stream = NULL; */
/*   AVPacket pkt = { 0 }; */
/*   gboolean success = FALSE; */

/*   const AVCodec *codec = avcodec_find_encoder (codec_id); */

/*   if (!codec) */
/*     { */
/*       g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_NOENT, */
/*                    "Encoder not found for codec ID %d", codec_id); */
/*       return FALSE; */
/*     } */

/*   codec_ctx = avcodec_alloc_context3 (codec); */
/*   if (!codec_ctx) */
/*     { */
/*       g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Failed
 * to allocate codec context"); */
/*       return FALSE; */
/*     } */

/*   enum AVPixelFormat pix_fmt = AV_PIX_FMT_NONE; */
/*   for (size_t i = 0; i < G_N_ELEMENTS (gpaint_formats); i++) */
/*     if (codec_id == gpaint_formats[i].codec_id) */
/*       { */
/*         pix_fmt = gpaint_formats[i].pix_fmt; */
/*         break; */
/*       } */

/*   cairo_surface_t *ref_surface = (cairo_surface_t *) surfaces->data; */
/*   codec_ctx->width = cairo_image_surface_get_width (ref_surface); */
/*   codec_ctx->height = cairo_image_surface_get_height (ref_surface); */
/*   codec_ctx->pix_fmt = pix_fmt; */
/*   codec_ctx->time_base = (AVRational) { 1, fps }; */
/*   codec_ctx->framerate = (AVRational) { fps, 1 }; */
/*   // TODO */
/*   /\* codec_ctx->max_b_frames = 0; *\/ */
/*   /\* codec_ctx->gop_size = 1; *\/ */

/*   if (avcodec_open2 (codec_ctx, codec, NULL) < 0) */
/*     { */
/*       g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Failed
 * to open codec"); */
/*       goto cleanup; */
/*     } */

/*   if (avformat_alloc_output_context2 (&fmt_ctx, NULL, "image2", filename) <
 * 0
 * || !fmt_ctx) */
/*     { */
/*       g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Failed
 * to allocate format context"); */
/*       goto cleanup; */
/*     } */

/*   stream = avformat_new_stream (fmt_ctx, NULL); */
/*   if (!stream) */
/*     { */
/*       g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Failed
 * to create output stream"); */
/*       goto cleanup; */
/*     } */

/*   // TODO */
/*   AVDictionary *opts = NULL; */
/*   // For a single-frame output, tell the muxer to update the file. */
/*   av_dict_set(&opts, "update", "1", 0); */
/*   if (avformat_write_header(fmt_ctx, &opts) < 0) { */
/*     g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Failed
 * to write header"); */
/*     goto cleanup; */
/*   } */
/*   av_dict_free(&opts); */

/*   if (avcodec_parameters_from_context (stream->codecpar, codec_ctx) < 0) */
/*     { */
/*       g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Failed
 * to copy codec parameters"); */
/*       goto cleanup; */
/*     } */

/*   stream->time_base = codec_ctx->time_base; */

/*   if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) */
/*     { */
/*       if (avio_open (&fmt_ctx->pb, filename, AVIO_FLAG_WRITE) < 0) */
/*         { */
/*           g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, */
/*                        "Failed to open file: %s", filename); */
/*           goto cleanup; */
/*         } */
/*     } */

/*   if (avformat_write_header (fmt_ctx, NULL) < 0) */
/*     { */
/*       g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Failed
 * to write header"); */
/*       goto cleanup; */
/*     } */

/*   for (GList *l = surfaces; l != NULL; l = l->next) */
/*     { */
/*       cairo_surface_t *surface = (cairo_surface_t *) l->data; */
/*       AVFrame *frame = convert_surface_to_frame (surface, codec_ctx); */

/*       if (!frame) */
/*         { */
/*           g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Failed to
 * convert surface to frame"); */
/*           goto cleanup; */
/*         } */

/*       frame->pts = l->data ? g_list_position (surfaces, l) : 0; */

/*       if (avcodec_send_frame (codec_ctx, frame) < 0) */
/*         { */
/*           g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
 * "Failed to send frame"); */
/*           av_frame_free (&frame); */
/*           goto cleanup; */
/*         } */

/*       while (avcodec_receive_packet (codec_ctx, &pkt) == 0) */
/*         { */
/*           pkt.stream_index = stream->index; */
/*           av_interleaved_write_frame (fmt_ctx, &pkt); */
/*           av_packet_unref (&pkt); */
/*         } */

/*       av_frame_free (&frame); */
/*     } */

/*   av_write_trailer (fmt_ctx); */
/*   success = TRUE; */

/* cleanup: */
/*   if (codec_ctx) */
/*     avcodec_free_context (&codec_ctx); */
/*   if (fmt_ctx) */
/*     { */
/*       if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) */
/*         avio_closep (&fmt_ctx->pb); */
/*       avformat_free_context (fmt_ctx); */
/*     } */
/*   return success; */
/* } */

// Load image or video frame to Cairo surface
cairo_surface_t *
load_image_to_cairo_surface (const char *filename)
{
  AVFormatContext *fmt_ctx = NULL;

  if (avformat_open_input (&fmt_ctx, filename, NULL, NULL) < 0)
    return NULL;

  if (avformat_find_stream_info (fmt_ctx, NULL) < 0)
    goto cleanup;

  int stream_index = av_find_best_stream (fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  if (stream_index < 0)
    goto cleanup;

  AVStream *stream = fmt_ctx->streams[stream_index];
  const AVCodec *codec = avcodec_find_decoder (stream->codecpar->codec_id);
  AVCodecContext *codec_ctx = avcodec_alloc_context3 (codec);
  avcodec_parameters_to_context (codec_ctx, stream->codecpar);
  avcodec_open2 (codec_ctx, codec, NULL);

  AVFrame *frame = av_frame_alloc ();
  AVFrame *rgb_frame = av_frame_alloc ();
  struct SwsContext *sws_ctx = NULL;
  cairo_surface_t *surface = NULL;

  AVPacket pkt;
  while (av_read_frame (fmt_ctx, &pkt) >= 0)
    {
      if (pkt.stream_index == stream_index)
        {
          if (avcodec_send_packet (codec_ctx, &pkt) == 0 && avcodec_receive_frame (codec_ctx, frame) == 0)
            {
              int width = codec_ctx->width;
              int height = codec_ctx->height;
              int stride = width * 4;

              // TODO
              g_assert (height > 0);
              g_assert (stride > 0);

              uint8_t *cairo_data = g_malloc (height * stride);

              av_image_fill_arrays (rgb_frame->data, rgb_frame->linesize, cairo_data, AV_PIX_FMT_BGRA, width, height, 1);

              sws_ctx = sws_getContext (width, height, codec_ctx->pix_fmt, width, height, AV_PIX_FMT_BGRA, SWS_BILINEAR, NULL, NULL, NULL);

              sws_scale (sws_ctx, (const uint8_t *const *) frame->data, frame->linesize, 0, height, rgb_frame->data, rgb_frame->linesize);

              surface = cairo_image_surface_create_for_data (
                  cairo_data, CAIRO_FORMAT_ARGB32, width, height, stride);
              g_free (cairo_data);
              break;
            }
        }

      av_packet_unref (&pkt);
    }

  av_frame_free (&frame);
  av_frame_free (&rgb_frame);
  avcodec_free_context (&codec_ctx);
  sws_freeContext (sws_ctx);
  avformat_close_input (&fmt_ctx);
  return surface;

cleanup:
  avformat_close_input (&fmt_ctx);
  return NULL;
}
