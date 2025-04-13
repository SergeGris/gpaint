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

  switch (codec_id)
    {
    case AV_CODEC_ID_BMP:
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
  uint8_t *data = cairo_image_surface_get_data (surface);

  AVFrame *frame = av_frame_alloc ();
  frame->format = ctx->pix_fmt;
  frame->width = width;
  frame->height = height;

  if (av_frame_get_buffer (frame, 32) < 0)
    return NULL;

  enum AVPixelFormat src_fmt = CAIRO_FORMAT_ARGB32 ? AV_PIX_FMT_BGRA : AV_PIX_FMT_RGBA;

  struct SwsContext *sws_ctx = sws_getContext (width, height, src_fmt,
                                               width, height, ctx->pix_fmt,
                                               SWS_BICUBIC, NULL, NULL, NULL);

  const uint8_t *src_slice[] = { data };
  int src_stride[] = { stride };

  sws_scale (sws_ctx, src_slice, src_stride, 0, height, frame->data, frame->linesize);
  sws_freeContext (sws_ctx);

  return frame;
}

gboolean
save_surfaces_with_ffmpeg (const char         *filename,
                           GList              *surfaces,
                           enum AVCodecID      codec_id,
                           int                 fps,
                           GError            **error)
{
  g_return_val_if_fail (filename && surfaces, FALSE);

  // TODO gboolean is_video = (g_list_length (surfaces) > 1);
  AVFormatContext *fmt_ctx = NULL;
  AVCodecContext *codec_ctx = NULL;
  AVStream *stream = NULL;
  AVPacket pkt = { 0 };
  gboolean success = FALSE;

  const AVCodec *codec = avcodec_find_encoder (codec_id);

  if (!codec)
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_NOENT,
                   "Encoder not found for codec ID %d", codec_id);
      return FALSE;
    }

  codec_ctx = avcodec_alloc_context3 (codec);
  if (!codec_ctx)
    {
      g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Failed to allocate codec context");
      return FALSE;
    }

  cairo_surface_t *ref_surface = (cairo_surface_t *) surfaces->data;
  codec_ctx->width = cairo_image_surface_get_width (ref_surface);
  codec_ctx->height = cairo_image_surface_get_height (ref_surface);
  codec_ctx->pix_fmt = get_target_pix_fmt (codec_id, cairo_image_surface_get_format (surfaces->data));
  codec_ctx->time_base = (AVRational){ 1, fps };
  codec_ctx->framerate = (AVRational){ fps, 1 };
  // TODO
  /* codec_ctx->max_b_frames = 0; */
  /* codec_ctx->gop_size = 1; */

  if (avcodec_open2 (codec_ctx, codec, NULL) < 0)
    {
      g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Failed to open codec");
      goto cleanup;
    }

  if (avformat_alloc_output_context2 (&fmt_ctx, NULL, "image2", filename) < 0 || !fmt_ctx)
    {
      g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Failed to allocate format context");
      goto cleanup;
    }

  stream = avformat_new_stream (fmt_ctx, NULL);
  if (!stream)
    {
      g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Failed to create output stream");
      goto cleanup;
    }

  if (avcodec_parameters_from_context (stream->codecpar, codec_ctx) < 0)
    {
      g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Failed to copy codec parameters");
      goto cleanup;
    }
  stream->time_base = codec_ctx->time_base;

  if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE))
    {
      if (avio_open (&fmt_ctx->pb, filename, AVIO_FLAG_WRITE) < 0)
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                       "Failed to open file: %s", filename);
          goto cleanup;
        }
    }

  if (avformat_write_header (fmt_ctx, NULL) < 0)
    {
      g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Failed to write header");
      goto cleanup;
    }

  for (GList *l = surfaces; l != NULL; l = l->next)
    {
      cairo_surface_t *surface = (cairo_surface_t *) l->data;
      AVFrame *frame = convert_surface_to_frame (surface, codec_ctx);
      if (!frame)
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Failed to convert surface to frame");
          goto cleanup;
        }

      frame->pts = l->data ? g_list_position (surfaces, l) : 0;

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

/* // Save Cairo surface using FFmpeg */
/* int */
/* save_image_with_ffmpeg (const char *filename, cairo_surface_t *surface, enum AVCodecID codec_id, int fps) */
/* { */
/*   // avformat_network_init(); */

/*   AVFormatContext *fmt_ctx = NULL; */
/*   const AVCodec *codec = avcodec_find_encoder (codec_id); */
/*   if (!codec) */
/*     return -1; */

/*   AVCodecContext *codec_ctx = avcodec_alloc_context3 (codec); */
/*   codec_ctx->width = cairo_image_surface_get_width (surface); */
/*   codec_ctx->height = cairo_image_surface_get_height (surface); */
/*   codec_ctx->time_base = (AVRational) { 1, fps }; */
/*   codec_ctx->pix_fmt = get_target_pix_fmt (codec_id, cairo_image_surface_get_format (surface)); */
/*   codec_ctx->framerate = (AVRational) { fps, 1 }; */

/*   if (avcodec_open2 (codec_ctx, codec, NULL) < 0) */
/*     return -1; */

/*   avformat_alloc_output_context2 (&fmt_ctx, NULL, NULL, filename); */
/*   if (!fmt_ctx) */
/*     return -1; */

/*   AVStream *stream = avformat_new_stream (fmt_ctx, NULL); */
/*   avcodec_parameters_from_context (stream->codecpar, codec_ctx); */
/*   stream->time_base = codec_ctx->time_base; */

/*   if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) */
/*     { */
/*       if (avio_open (&fmt_ctx->pb, filename, AVIO_FLAG_WRITE) < 0) */
/*         return -1; */
/*     } */

/*   avformat_write_header (fmt_ctx, NULL); */

/*   AVFrame *frame = convert_surface_to_frame (surface, codec_ctx); */
/*   if (!frame) */
/*     return -1; */

/*   AVPacket pkt; */
/*   av_init_packet (&pkt); */
/*   pkt.data = NULL; */
/*   pkt.size = 0; */

/*   if (avcodec_send_frame (codec_ctx, frame) == 0) */
/*     { */
/*       if (avcodec_receive_packet (codec_ctx, &pkt) == 0) */
/*         { */
/*           pkt.stream_index = stream->index; */
/*           av_interleaved_write_frame (fmt_ctx, &pkt); */
/*           av_packet_unref (&pkt); */
/*         } */
/*     } */

/*   av_write_trailer (fmt_ctx); */

/*   av_frame_free (&frame); */
/*   avcodec_free_context (&codec_ctx); */
/*   if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) */
/*     avio_closep (&fmt_ctx->pb); */
/*   avformat_free_context (fmt_ctx); */

/*   return 0; */
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

              uint8_t *cairo_data = g_malloc (height * stride);

              av_image_fill_arrays (rgb_frame->data, rgb_frame->linesize, cairo_data,
                                    AV_PIX_FMT_BGRA, width, height, 1);

              sws_ctx = sws_getContext (width, height, codec_ctx->pix_fmt,
                                        width, height, AV_PIX_FMT_BGRA,
                                        SWS_BILINEAR, NULL, NULL, NULL);

              sws_scale (sws_ctx, (const uint8_t *const *) frame->data, frame->linesize,
                         0, height, rgb_frame->data, rgb_frame->linesize);

              surface = cairo_image_surface_create_for_data (cairo_data,
                                                             CAIRO_FORMAT_ARGB32,
                                                             width, height, stride);
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

/* #include <cairo/cairo.h> */
/* #include <libavformat/avformat.h> */
/* #include <libavcodec/avcodec.h> */
/* #include <libavutil/imgutils.h> */

/* #include <libavcodec/avcodec.h> */
/* #include <libavformat/avformat.h> */
/* #include <libswscale/swscale.h> */

/* int save_image_with_ffmpeg(const char *filename, cairo_surface_t *surface, enum AVCodecID codec_id) { */
/*   int width = cairo_image_surface_get_width(surface); */
/*   int height = cairo_image_surface_get_height(surface); */
/*   int stride = cairo_image_surface_get_stride(surface); */
/*   uint8_t *data = cairo_image_surface_get_data(surface); */

/*   AVCodec *codec = avcodec_find_encoder(codec_id); */
/*   if (!codec) return -1; */

/*   AVCodecContext *codec_ctx = avcodec_alloc_context3(codec); */
/*   codec_ctx->width = width; */
/*   codec_ctx->height = height; */
/*   codec_ctx->pix_fmt = AV_PIX_FMT_RGBA; */
/*   codec_ctx->time_base = (AVRational){1, 25}; */

/*   if (avcodec_open2(codec_ctx, codec, NULL) < 0) return -1; */

/*   AVFrame *frame = av_frame_alloc(); */
/*   frame->format = codec_ctx->pix_fmt; */
/*   frame->width = width; */
/*   frame->height = height; */
/*   av_frame_get_buffer(frame, 32); */

/*   struct SwsContext *sws_ctx = sws_getContext( */
/*     width, height, AV_PIX_FMT_RGB32, */
/*     width, height, AV_PIX_FMT_RGBA, */
/*     SWS_BILINEAR, NULL, NULL, NULL */
/*   ); */

/*   const uint8_t *src_slices[1] = { data }; */
/*   int src_stride[1] = { stride }; */
/*   sws_scale(sws_ctx, src_slices, src_stride, 0, height, frame->data, frame->linesize); */

/*   AVFormatContext *fmt_ctx = NULL; */
/*   avformat_alloc_output_context2(&fmt_ctx, NULL, NULL, filename); */
/*   AVStream *stream = avformat_new_stream(fmt_ctx, NULL); */
/*   stream->codecpar->codec_id = codec_id; */
/*   stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO; */
/*   stream->codecpar->width = width; */
/*   stream->codecpar->height = height; */
/*   stream->codecpar->format = codec_ctx->pix_fmt; */

/*   if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) { */
/*     if (avio_open(&fmt_ctx->pb, filename, AVIO_FLAG_WRITE) < 0) return -1; */
/*   } */

/*   avformat_write_header(fmt_ctx, NULL); */

/*   AVPacket pkt; */
/*   av_init_packet(&pkt); */
/*   pkt.data = NULL; */
/*   pkt.size = 0; */

/*   if (avcodec_send_frame(codec_ctx, frame) == 0) { */
/*     if (avcodec_receive_packet(codec_ctx, &pkt) == 0) { */
/*       av_interleaved_write_frame(fmt_ctx, &pkt); */
/*       av_packet_unref(&pkt); */
/*     } */
/*   } */

/*   av_write_trailer(fmt_ctx); */
/*   avcodec_free_context(&codec_ctx); */
/*   av_frame_free(&frame); */
/*   sws_freeContext(sws_ctx); */
/*   if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) { */
/*     avio_closep(&fmt_ctx->pb); */
/*   } */
/*   avformat_free_context(fmt_ctx); */

/*   return 0; */
/* } */

/* #include <cairo.h> */
/* #include <libavformat/avformat.h> */
/* #include <libavcodec/avcodec.h> */
/* #include <libswscale/swscale.h> */

/* cairo_surface_t* load_image_to_cairo_surface(const char *filename) { */
/*     AVFormatContext *fmt_ctx = NULL; */
/*     AVCodecContext *codec_ctx = NULL; */
/*     AVFrame *frame = NULL, *rgb_frame = NULL; */
/*     struct SwsContext *sws_ctx = NULL; */
/*     int video_stream_index; */
/*     cairo_surface_t *surface = NULL; */
/*     uint8_t *buffer = NULL; */

/*     // Initialize FFmpeg */
/*     // TODO av_register_all(); */

/*     // Open the input file */
/*     if (avformat_open_input(&fmt_ctx, filename, NULL, NULL) < 0) { */
/*         fprintf(stderr, "Could not open input file '%s'\n", filename); */
/*         return NULL; */
/*     } */

/*     // Retrieve stream information */
/*     if (avformat_find_stream_info(fmt_ctx, NULL) < 0) { */
/*         fprintf(stderr, "Could not find stream information\n"); */
/*         goto cleanup; */
/*     } */

/*     // Find the first video stream */
/*     video_stream_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0); */
/*     if (video_stream_index < 0) { */
/*         fprintf(stderr, "Could not find a video stream in the input file\n"); */
/*         goto cleanup; */
/*     } */

/*     // Get the codec context for the video stream */
/*     AVStream *video_stream = fmt_ctx->streams[video_stream_index]; */
/*     AVCodec *codec = avcodec_find_decoder(video_stream->codecpar->codec_id); */
/*     if (!codec) { */
/*         fprintf(stderr, "Unsupported codec\n"); */
/*         goto cleanup; */
/*     } */

/*     codec_ctx = avcodec_alloc_context3(codec); */
/*     if (!codec_ctx) { */
/*         fprintf(stderr, "Could not allocate codec context\n"); */
/*         goto cleanup; */
/*     } */

/*     if (avcodec_parameters_to_context(codec_ctx, video_stream->codecpar) < 0) { */
/*         fprintf(stderr, "Could not copy codec parameters\n"); */
/*         goto cleanup; */
/*     } */

/*     // Open codec */
/*     if (avcodec_open2(codec_ctx, codec, NULL) < 0) { */
/*         fprintf(stderr, "Could not open codec\n"); */
/*         goto cleanup; */
/*     } */

/*     // Allocate frames */
/*     frame = av_frame_alloc(); */
/*     rgb_frame = av_frame_alloc(); */
/*     if (!frame || !rgb_frame) { */
/*         fprintf(stderr, "Could not allocate frames\n"); */
/*         goto cleanup; */
/*     } */

/*     // Determine required buffer size and allocate buffer */
/*     int width = codec_ctx->width; */
/*     int height = codec_ctx->height; */
/*     enum AVPixelFormat pix_fmt = AV_PIX_FMT_RGB24; */
/*     int num_bytes = av_image_get_buffer_size(pix_fmt, width, height, 1); */
/*     buffer = (uint8_t *)av_malloc(num_bytes * sizeof(uint8_t)); */
/*     if (!buffer) { */
/*         fprintf(stderr, "Could not allocate buffer\n"); */
/*         goto cleanup; */
/*     } */

/*     // Assign appropriate parts of buffer to image planes in rgb_frame */
/*     av_image_fill_arrays(rgb_frame->data, rgb_frame->linesize, buffer, pix_fmt, width, height, 1); */

/*     // Initialize SWS context for software scaling */
/*     sws_ctx = sws_getContext(width, height, codec_ctx->pix_fmt, */
/*                              width, height, pix_fmt, */
/*                              SWS_BILINEAR, NULL, NULL, NULL); */
/*     if (!sws_ctx) { */
/*         fprintf(stderr, "Could not initialize the conversion context\n"); */
/*         goto cleanup; */
/*     } */

/*     // Read frames and decode */
/*     AVPacket packet; */
/*     av_init_packet(&packet); */
/*     packet.data = NULL; */
/*     packet.size = 0; */

/*     int got_frame = 0; */
/*     while (av_read_frame(fmt_ctx, &packet) >= 0) { */
/*         if (packet.stream_index == video_stream_index) { */
/*             if (avcodec_send_packet(codec_ctx, &packet) == 0) { */
/*                 if (avcodec_receive_frame(codec_ctx, frame) == 0) { */
/*                     // Convert the image from its native format to RGB */
/*                     sws_scale(sws_ctx, (uint8_t const * const *)frame->data, */
/*                               frame->linesize, 0, height, */
/*                               rgb_frame->data, rgb_frame->linesize); */
/*                     got_frame = 1; */
/*                     av_packet_unref(&packet); */
/*                     break; */
/*                 } */
/*             } */
/*         } */
/*         av_packet_unref(&packet); */
/*     } */

/*     if (!got_frame) { */
/*         fprintf(stderr, "Could not decode frame\n"); */
/*         goto cleanup; */
/*     } */

/*     // Create a Cairo image surface from the RGB data */
/*     surface = cairo_image_surface_create_for_data(rgb_frame->data[0], */
/*                                                   CAIRO_FORMAT_RGB24, */
/*                                                   width, height, */
/*                                                   rgb_frame->linesize[0]); */

/* cleanup: */
/*     if (frame) av_frame_free(&frame); */
/*     if (rgb_frame) av_frame_free(&rgb_frame); */
/*     if (codec_ctx) avcodec_free_context(&codec_ctx); */
/*     if (fmt_ctx) avformat_close_input(&fmt_ctx); */
/*     if (sws_ctx) sws_freeContext(sws_ctx); */
/*     if (buffer) av_free(buffer); */

/*     return surface; */
/* } */
