/* Implementation for saving/loading Cairo surfaces via FFmpeg */
#include "formats.h"
#include <cairo.h>
#include <glib.h>

#if !HAVE_FFMPEG
typedef int dummy;
#else
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>

GQuark
gpaint_format_error_quark (void)
{
  return g_quark_from_static_string ("gpaint-format-error-quark");
}

/**
 * save_surfaces_with_ffmpeg:
 * @filename: output filename
 * @surfaces: list of cairo_surface_t*
 * @codec_id: desired codec
 * @fps: frames per second (for video)
 * @error: return location for GError
 *
 * Saves one or more Cairo surfaces to an image or video file using FFmpeg.
 * Returns TRUE on success, FALSE and sets @error on failure.
 **/
gboolean
save_surfaces_with_ffmpeg (const char *filename,
                           GList *surfaces,
                           enum AVCodecID codec_id,
                           int fps,
                           const char *options_string,
                           GError **error)
{
  for (size_t i = 0; i < countof (gpaint_formats); i++)
    if (codec_id == gpaint_formats[i].codec_id)
      if (!gpaint_formats[i].supports_animation && g_list_length (surfaces) > 1)
        return FALSE; // TODO

  AVFormatContext *fmt_ctx = NULL;
  int ret = avformat_alloc_output_context2 (&fmt_ctx, NULL, NULL, filename);
  if (!fmt_ctx)
    {
      g_set_error (error, GPAINT_FORMAT_ERROR, GPAINT_FORMAT_ERROR_INSUFFICIENT_MEMORY,
                   "Failed to allocate format context");
      return FALSE;
    }

  // Find encoder
  const AVCodec *codec = avcodec_find_encoder (codec_id);
  if (!codec)
    {
      g_set_error (error, GPAINT_FORMAT_ERROR, GPAINT_FORMAT_ERROR_UNSUPPORTED_CODEC,
                   "Codec '%s' not found", avcodec_get_name (codec_id)); // TODO FIX
      avformat_free_context (fmt_ctx);
      return FALSE;
    }

  // Take first surface to get width/height
  cairo_surface_t *cs = (cairo_surface_t *) surfaces->data;
  int width = cairo_image_surface_get_width (cs);
  int height = cairo_image_surface_get_height (cs);

  // Create new video stream
  AVStream *stream = avformat_new_stream (fmt_ctx, codec);
  if (!stream)
    {
      g_set_error (error, GPAINT_FORMAT_ERROR, GPAINT_FORMAT_ERROR_FFMPEG_INTERNAL_ERROR,
                   "Failed to create new stream");
      avformat_free_context (fmt_ctx);
      return FALSE;
    }

  AVCodecContext *cctx = avcodec_alloc_context3 (codec);
  if (!cctx)
    {
      g_set_error (error, GPAINT_FORMAT_ERROR, GPAINT_FORMAT_ERROR_INSUFFICIENT_MEMORY,
                   "Could not allocate codec context");
      avformat_free_context (fmt_ctx);
      return FALSE;
    }

  // Set codec parameters
  cctx->codec_id = codec_id;
  cctx->width = width;
  cctx->height = height;
  cctx->time_base = (AVRational) { 1, fps };
  cctx->framerate = (AVRational) { fps, 1 };
  cctx->gop_size = 12; // intra frame interval

  /* Determine appropriate pixel format from our gpaint_formats array */
  enum AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;
  for (size_t i = 0; i < countof (gpaint_formats); i++)
    {
      if (codec_id == gpaint_formats[i].codec_id)
        {
          pix_fmt = gpaint_formats[i].pix_fmt;
          break;
        }
    }

  if (pix_fmt == AV_PIX_FMT_NONE)
    {
      g_set_error (error, GPAINT_FORMAT_ERROR, GPAINT_FORMAT_ERROR_FAILED,
                   "Unsupported pixel format for codec");
      avcodec_free_context (&cctx);
      avformat_free_context (fmt_ctx);
      return FALSE;
    }

  cctx->pix_fmt = pix_fmt;
  if (fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
    cctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  AVDictionary *codec_opts = NULL;
  // TODO
  /* if (options_string) */
  /*   { */
  /*     // TODO */
  /*     if (av_dict_parse_string(&codec_opts, */
  /*                              options_string,   // e.g. "crf=23:b=1M" */
  /*                              ":",              // key/value sep */
  /*                              "=",              // pair sep */
  /*                              0) < 0) { */
  /*       /\* fprintf(stderr, "Could not parse options: %s\n", options_string); *\/ */
  /*       /\* exit(1); *\/ */
  /*       // TODO */
  /*       g_warning("bad opts: %s", options_string); */
  /*     } */
  /*     //TODO */
  /*     /\* av_dict_set(&codec_opts, "crf", "23", 0); *\/ */
  /*     /\* av_dict_set(&codec_opts, "preset", "medium", 0); *\/ */
  /*   } */

  // Open codec
  ret = avcodec_open2 (cctx, codec, &codec_opts);
  if (ret < 0)
    {
      g_set_error (error, GPAINT_FORMAT_ERROR, GPAINT_FORMAT_ERROR_FFMPEG_INTERNAL_ERROR,
                   "Failed to open codec: %s", av_err2str (ret));
      avcodec_free_context (&cctx);
      avformat_free_context (fmt_ctx);
      return FALSE;
    }

  ret = avcodec_parameters_from_context (stream->codecpar, cctx);
  if (ret < 0)
    {
      g_set_error (error, GPAINT_FORMAT_ERROR, GPAINT_FORMAT_ERROR_FFMPEG_INTERNAL_ERROR,
                   "Failed to copy codec parameters: %s", av_err2str (ret));
      avcodec_free_context (&cctx);
      avformat_free_context (fmt_ctx);
      return FALSE;
    }

  // Open output file
  if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE))
    {
      ret = avio_open (&fmt_ctx->pb, filename, AVIO_FLAG_WRITE);
      if (ret < 0)
        {
          g_set_error (error, GPAINT_FORMAT_ERROR, GPAINT_FORMAT_ERROR_FFMPEG_INTERNAL_ERROR,
                       "Could not open '%s': %s", filename, av_err2str (ret));
          avcodec_free_context (&cctx);
          avformat_free_context (fmt_ctx);
          return FALSE;
        }
    }

  // TODO
  /* If writing a single image (only one surface),
     set the "update" option so the muxer will update the same file */
  AVDictionary *opts = NULL;

  if (g_list_length (surfaces) == 1)
    av_dict_set (&opts, "update", "1", 0);

  /* Write the header once – do not call this twice */
  if (avformat_write_header (fmt_ctx, &opts) < 0)
    {
      g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Failed to write header");
      //TODO goto cleanup;
    }

  av_dict_free (&opts);
  // TODO

  /* // Write header */
  /* ret = avformat_write_header (fmt_ctx, NULL); */
  /* if (ret < 0) */
  /*   { */
  /*     g_set_error (error, GPAINT_FORMAT_ERROR, GPAINT_FORMAT_ERROR_FFMPEG_INTERNAL_ERROR, */
  /*                  "Error writing header: %s", av_err2str (ret)); */
  /*     avio_closep (&fmt_ctx->pb); */
  /*     avcodec_free_context (&cctx); */
  /*     avformat_free_context (fmt_ctx); */
  /*     return FALSE; */
  /*   } */

  // Prepare frame and converter
  struct SwsContext *sws_ctx = sws_getContext (width, height, AV_PIX_FMT_BGRA,
                                               width, height, cctx->pix_fmt,
                                               SWS_BICUBIC, NULL, NULL, NULL);
  AVFrame *frame = av_frame_alloc ();
  frame->format = cctx->pix_fmt;
  frame->width = width;
  frame->height = height;
  av_image_alloc (frame->data, frame->linesize, width, height, cctx->pix_fmt, 1);

  // TODO
  AVPacket *pkt = av_packet_alloc ();

  if (!pkt)
    {
      g_set_error (error, GPAINT_FORMAT_ERROR, GPAINT_FORMAT_ERROR_INSUFFICIENT_MEMORY,
                   "failed to create packet");
      return FALSE;
    }

  int64_t pts = 0;
  for (GList *l = surfaces; l; l = l->next)
    {
      cairo_surface_t *surf = (cairo_surface_t *) l->data;
      unsigned char *src_data = cairo_image_surface_get_data (surf);
      int src_stride = cairo_image_surface_get_stride (surf);

      // Feed each frame
      uint8_t *in_data[4] = { src_data, NULL, NULL, NULL };
      int in_linesize[4] = { src_stride, 0, 0, 0 };
      sws_scale (sws_ctx, (const uint8_t *const *) in_data, in_linesize,
                 0, height, frame->data, frame->linesize);

      frame->pts = pts++;

      ret = avcodec_send_frame (cctx, frame);
      if (ret < 0)
        {
          g_set_error (error, GPAINT_FORMAT_ERROR, GPAINT_FORMAT_ERROR_FFMPEG_INTERNAL_ERROR,
                       "Error sending frame to encoder: %s", av_err2str (ret));
          break;
        }
      while (ret >= 0)
        {
          /* AVPacket pkt = { .data = NULL, .size = 0 }; */

          /* av_init_packet (&pkt); */
          av_packet_unref (pkt);
          ret = avcodec_receive_packet (cctx, pkt);

          if (ret == AVERROR (EAGAIN) || ret == AVERROR_EOF)
            {
              av_packet_unref (pkt);
              break;
            }
          else if (ret < 0)
            {
              g_set_error (error, GPAINT_FORMAT_ERROR, GPAINT_FORMAT_ERROR_FFMPEG_INTERNAL_ERROR,
                           "Error encoding frame: %s", av_err2str (ret));
              av_packet_unref (pkt);
              break;
            }

          pkt->stream_index = stream->index;
          av_interleaved_write_frame (fmt_ctx, pkt);
          av_packet_unref (pkt);
        }
    }

  // Flush encoder
  avcodec_send_frame (cctx, NULL);
  while (avcodec_receive_packet (cctx, &(AVPacket) { 0 }) >= 0)
    {
      av_packet_unref (pkt);
      /* AVPacket pkt = { .data = NULL, .size = 0 }; */
      av_interleaved_write_frame (fmt_ctx, pkt);
    }

  av_packet_free (&pkt);

  av_write_trailer (fmt_ctx);
  if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE))
    avio_closep (&fmt_ctx->pb);
  avcodec_free_context (&cctx);
  av_frame_free (&frame);
  sws_freeContext (sws_ctx);
  avformat_free_context (fmt_ctx);
  return TRUE;
}

/**
 * load_image_to_cairo_surface:
 * @filename: input image file
 *
 * Loads the first frame of an image or video file into a Cairo image surface.
 * Returns newly allocated cairo_surface_t* or NULL on failure.
 **/
cairo_surface_t *
load_image_to_cairo_surface (const char *filename)
{
  // TODO av_register_all();

  AVFormatContext *fmt_ctx = NULL;
  if (avformat_open_input (&fmt_ctx, filename, NULL, NULL) < 0)
    return NULL;
  if (avformat_find_stream_info (fmt_ctx, NULL) < 0)
    {
      avformat_close_input (&fmt_ctx);
      return NULL;
    }

  // Find first video stream
  int stream_index = av_find_best_stream (fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  if (stream_index < 0)
    {
      avformat_close_input (&fmt_ctx);
      return NULL;
    }

  AVStream *stream = fmt_ctx->streams[stream_index];
  const AVCodec *codec = avcodec_find_decoder (stream->codecpar->codec_id);
  if (!codec)
    {
      avformat_close_input (&fmt_ctx);
      return NULL;
    }

  AVCodecContext *cctx = avcodec_alloc_context3 (codec);
  avcodec_parameters_to_context (cctx, stream->codecpar);
  if (avcodec_open2 (cctx, codec, NULL) < 0)
    {
      avcodec_free_context (&cctx);
      avformat_close_input (&fmt_ctx);
      return NULL;
    }

  AVPacket packet;
  AVFrame *frame = av_frame_alloc ();
  struct SwsContext *sws_ctx = NULL;
  gboolean got_frame = FALSE;
  cairo_surface_t *surface = NULL;

  // Read frames
  while (av_read_frame (fmt_ctx, &packet) >= 0)
    {
      if (packet.stream_index == stream_index
          && avcodec_send_packet (cctx, &packet) == 0
          && avcodec_receive_frame (cctx, frame) == 0)
        {
          got_frame = TRUE;
          av_packet_unref (&packet);
          break;
        }

      av_packet_unref (&packet);
    }

  if (!got_frame)
    goto cleanup;

  // Convert to ARGB
  int width = frame->width;
  int height = frame->height;
  sws_ctx = sws_getContext (width, height, cctx->pix_fmt,
                            width, height, AV_PIX_FMT_BGRA,
                            SWS_BILINEAR, NULL, NULL, NULL);
  int out_linesize = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, width);
  uint8_t *data = g_malloc0 (height * out_linesize);

  uint8_t *dst_data[4] = { data, NULL, NULL, NULL };
  int dst_linesize[4] = { out_linesize, 0, 0, 0 };
  sws_scale (sws_ctx, (const uint8_t *const *) frame->data, frame->linesize,
             0, height, dst_data, dst_linesize);

  surface = cairo_image_surface_create_for_data (data,
                                                 CAIRO_FORMAT_ARGB32,
                                                 width, height,
                                                 out_linesize);
  // Attach data pointer to surface user data to free later
  cairo_surface_set_user_data (surface, NULL, data, g_free);

cleanup:
  if (sws_ctx)
    sws_freeContext (sws_ctx);
  av_frame_free (&frame);
  avcodec_free_context (&cctx);
  avformat_close_input (&fmt_ctx);
  return surface;
}
#endif
