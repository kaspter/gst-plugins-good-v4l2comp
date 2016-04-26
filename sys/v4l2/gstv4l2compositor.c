/* Video compositor plugin using V4L2 abilities
 * Copyright (C) 2016 Frédéric Sureau <frederic.sureau@veo-labs.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "gstv4l2compositor.h"
#include "gstv4l2compositorpad.h"
#include "v4l2_calls.h"

GST_DEBUG_CATEGORY_STATIC (gst_v4l2compositor_debug);
#define GST_CAT_DEFAULT gst_v4l2compositor_debug

#define FORMATS " { AYUV, BGRA, ARGB, RGBA, ABGR, Y444, Y42B, YUY2, UYVY, "\
                "   YVYU, I420, YV12, NV12, NV21, Y41B, RGB, BGR, xRGB, xBGR, "\
                "   RGBx, BGRx } "

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (FORMATS))
    );

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (FORMATS))
    );




#define DEFAULT_PAD_XPOS   0
#define DEFAULT_PAD_YPOS   0
#define DEFAULT_PAD_WIDTH  -1
#define DEFAULT_PAD_HEIGHT -1
#define DEFAULT_PAD_DEVICE NULL
enum
{
  PROP_PAD_0,
  PROP_PAD_XPOS,
  PROP_PAD_YPOS,
  PROP_PAD_WIDTH,
  PROP_PAD_HEIGHT,
  PROP_PAD_DEVICE,
};

G_DEFINE_TYPE (GstV4l2CompositorPad, gst_v4l2_compositor_pad,
    GST_TYPE_V4L2_VIDEO_AGGREGATOR_PAD);

static void
gst_v4l2_compositor_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstV4l2CompositorPad *pad = GST_V4L2_COMPOSITOR_PAD (object);

  switch (prop_id) {
    case PROP_PAD_XPOS:
      g_value_set_int (value, pad->xpos);
      break;
    case PROP_PAD_YPOS:
      g_value_set_int (value, pad->ypos);
      break;
    case PROP_PAD_WIDTH:
      g_value_set_int (value, pad->width);
      break;
    case PROP_PAD_HEIGHT:
      g_value_set_int (value, pad->height);
      break;
    case PROP_PAD_DEVICE:
      g_value_set_string (value, pad->videodev);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_v4l2_compositor_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstV4l2CompositorPad *pad = GST_V4L2_COMPOSITOR_PAD (object);

  switch (prop_id) {
    case PROP_PAD_XPOS:
      pad->xpos = g_value_get_int (value);
      break;
    case PROP_PAD_YPOS:
      pad->ypos = g_value_get_int (value);
      break;
    case PROP_PAD_WIDTH:
      pad->width = g_value_get_int (value);
      break;
    case PROP_PAD_HEIGHT:
      pad->height = g_value_get_int (value);
      break;
    case PROP_PAD_DEVICE:
      if (pad->videodev != NULL)
        g_free ((gpointer)pad->videodev);
      pad->videodev = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
_mixer_pad_get_output_size (GstV4l2Compositor * comp,
    GstV4l2CompositorPad * comp_pad, gint * width, gint * height)
{
  GstV4l2VideoAggregator *vagg = GST_V4L2_VIDEO_AGGREGATOR (comp);
  GstV4l2VideoAggregatorPad *vagg_pad =
      GST_V4L2_VIDEO_AGGREGATOR_PAD (comp_pad);
  gint pad_width, pad_height;
  guint dar_n, dar_d;

  /* FIXME: Anything better we can do here? */
  if (!vagg_pad->info.finfo
      || vagg_pad->info.finfo->format == GST_VIDEO_FORMAT_UNKNOWN) {
    GST_DEBUG_OBJECT (comp_pad, "Have no caps yet");
    *width = 0;
    *height = 0;
    return;
  }

  pad_width =
      comp_pad->width <=
      0 ? GST_VIDEO_INFO_WIDTH (&vagg_pad->info) : comp_pad->width;
  pad_height =
      comp_pad->height <=
      0 ? GST_VIDEO_INFO_HEIGHT (&vagg_pad->info) : comp_pad->height;

  gst_video_calculate_display_ratio (&dar_n, &dar_d, pad_width, pad_height,
      GST_VIDEO_INFO_PAR_N (&vagg_pad->info),
      GST_VIDEO_INFO_PAR_D (&vagg_pad->info),
      GST_VIDEO_INFO_PAR_N (&vagg->info), GST_VIDEO_INFO_PAR_D (&vagg->info)
      );
  GST_LOG_OBJECT (comp_pad, "scaling %ux%u by %u/%u (%u/%u / %u/%u)", pad_width,
      pad_height, dar_n, dar_d, GST_VIDEO_INFO_PAR_N (&vagg_pad->info),
      GST_VIDEO_INFO_PAR_D (&vagg_pad->info),
      GST_VIDEO_INFO_PAR_N (&vagg->info), GST_VIDEO_INFO_PAR_D (&vagg->info));

  if (pad_height % dar_n == 0) {
    pad_width = gst_util_uint64_scale_int (pad_height, dar_n, dar_d);
  } else if (pad_width % dar_d == 0) {
    pad_height = gst_util_uint64_scale_int (pad_width, dar_d, dar_n);
  } else {
    pad_width = gst_util_uint64_scale_int (pad_height, dar_n, dar_d);
  }

  if (width)
    *width = pad_width;
  if (height)
    *height = pad_height;
}

static gboolean
gst_v4l2_compositor_pad_set_info (GstV4l2VideoAggregatorPad * pad,
    GstV4l2VideoAggregator * vagg G_GNUC_UNUSED,
    GstVideoInfo * current_info, GstVideoInfo * wanted_info)
{
  return TRUE;
}

static gboolean
gst_v4l2_compositor_pad_prepare_frame (GstV4l2VideoAggregatorPad * pad,
    GstV4l2VideoAggregator * vagg)
{
  return TRUE;
}

static void
gst_v4l2_compositor_pad_clean_frame (GstV4l2VideoAggregatorPad * pad,
    GstV4l2VideoAggregator * vagg)
{
}

static void
gst_v4l2_compositor_pad_finalize (GObject * object)
{
  GstV4l2CompositorPad *pad = GST_V4L2_COMPOSITOR_PAD (object);

  if (pad->convert)
    gst_video_converter_free (pad->convert);
  pad->convert = NULL;

  G_OBJECT_CLASS (gst_v4l2_compositor_pad_parent_class)->finalize (object);
}

static void
gst_v4l2_compositor_pad_init (GstV4l2CompositorPad * cpad)
{
  cpad->xpos = DEFAULT_PAD_XPOS;
  cpad->ypos = DEFAULT_PAD_YPOS;
  cpad->width = DEFAULT_PAD_WIDTH;
  cpad->height = DEFAULT_PAD_HEIGHT;
  cpad->m2m = gst_v4l2_m2m_new (GST_ELEMENT (cpad));
}

static void
gst_v4l2_compositor_pad_class_init (GstV4l2CompositorPadClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstV4l2VideoAggregatorPadClass *vaggpadclass =
      (GstV4l2VideoAggregatorPadClass *) klass;

  gobject_class->set_property = gst_v4l2_compositor_pad_set_property;
  gobject_class->get_property = gst_v4l2_compositor_pad_get_property;
  gobject_class->finalize = gst_v4l2_compositor_pad_finalize;

  g_object_class_install_property (gobject_class, PROP_PAD_XPOS,
      g_param_spec_int ("xpos", "X Position", "X Position of the picture",
          G_MININT, G_MAXINT, DEFAULT_PAD_XPOS,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PAD_YPOS,
      g_param_spec_int ("ypos", "Y Position", "Y Position of the picture",
          G_MININT, G_MAXINT, DEFAULT_PAD_YPOS,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PAD_WIDTH,
      g_param_spec_int ("width", "Width", "Width of the picture",
          G_MININT, G_MAXINT, DEFAULT_PAD_WIDTH,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PAD_HEIGHT,
      g_param_spec_int ("height", "Height", "Height of the picture",
          G_MININT, G_MAXINT, DEFAULT_PAD_HEIGHT,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PAD_DEVICE,
      g_param_spec_string ("device", "Device", "Device location",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  vaggpadclass->set_info = GST_DEBUG_FUNCPTR (gst_v4l2_compositor_pad_set_info);
  vaggpadclass->prepare_frame =
      GST_DEBUG_FUNCPTR (gst_v4l2_compositor_pad_prepare_frame);
  vaggpadclass->clean_frame =
      GST_DEBUG_FUNCPTR (gst_v4l2_compositor_pad_clean_frame);
}


/* GstV4l2Compositor */

#define DEFAULT_PROP_DEVICE   "/dev/video0"
#define DEFAULT_PROP_IO_MODE  GST_V4L2_IO_AUTO
enum
{
  PROP_0,
  PROP_DEVICE,
  PROP_OUTPUT_IO_MODE,
  PROP_CAPTURE_IO_MODE
};


static void
gst_v4l2_compositor_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstV4l2Compositor *self = GST_V4L2_COMPOSITOR (object);

  switch (prop_id) {
    case PROP_DEVICE:
      g_value_set_string (value, self->videodev);
      break;
    case PROP_OUTPUT_IO_MODE:
      g_value_set_enum (value, self->output_io_mode);
      break;
    case PROP_CAPTURE_IO_MODE:
      g_value_set_enum (value, self->capture_io_mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}




static void
gst_v4l2_compositor_propagate_video_device (GstV4l2Compositor * self)
{
  GList *l;
  GstV4l2VideoAggregatorPad *pad;
  GstV4l2CompositorPad *cpad;

  for (l = GST_ELEMENT (self)->sinkpads; l; l = l->next) {
    pad = l->data;
    cpad = GST_V4L2_COMPOSITOR_PAD (pad);
    if (cpad->videodev != NULL)
      gst_v4l2_m2m_set_video_device (cpad->m2m, cpad->videodev);
    else
      gst_v4l2_m2m_set_video_device (cpad->m2m, self->videodev);
  }
}


static void
gst_v4l2_compositor_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstV4l2Compositor *self = GST_V4L2_COMPOSITOR (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      g_free (self->videodev);
      self->videodev = g_value_dup_string (value);
      break;
    case PROP_OUTPUT_IO_MODE:
      self->output_io_mode = g_value_get_enum (value);
      break;
    case PROP_CAPTURE_IO_MODE:
      self->capture_io_mode = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

#define gst_v4l2_compositor_parent_class parent_class
G_DEFINE_TYPE (GstV4l2Compositor, gst_v4l2_compositor,
    GST_TYPE_V4L2_VIDEO_AGGREGATOR);

static GstCaps *
gst_v4l2_compositor_update_caps (GstV4l2VideoAggregator * vagg, GstCaps * caps)
{
  GList *l;
  gint best_width = -1, best_height = -1;
  GstVideoInfo info;
  GstCaps *ret = NULL;

  gst_video_info_from_caps (&info, caps);

  /* FIXME: this doesn't work for non 1/1 output par's as we don't have that
   * information available at this time */

  GST_OBJECT_LOCK (vagg);
  for (l = GST_ELEMENT (vagg)->sinkpads; l; l = l->next) {
    GstV4l2VideoAggregatorPad *vaggpad = l->data;
    GstV4l2CompositorPad *compositor_pad = GST_V4L2_COMPOSITOR_PAD (vaggpad);
    gint this_width, this_height;
    gint width, height;

    _mixer_pad_get_output_size (GST_V4L2_COMPOSITOR (vagg), compositor_pad,
        &width, &height);

    if (width == 0 || height == 0)
      continue;

    this_width = width + MAX (compositor_pad->xpos, 0);
    this_height = height + MAX (compositor_pad->ypos, 0);

    if (best_width < this_width)
      best_width = this_width;
    if (best_height < this_height)
      best_height = this_height;
  }
  GST_OBJECT_UNLOCK (vagg);

  if (best_width > 0 && best_height > 0) {
    info.width = best_width;
    info.height = best_height;

    ret = gst_video_info_to_caps (&info);
    gst_caps_set_simple (ret, "pixel-aspect-ratio", GST_TYPE_FRACTION_RANGE,
        1, G_MAXINT, G_MAXINT, 1, NULL);
  }

  return ret;
}


static GstV4l2CompositorPad *
gst_v4l2_compositor_get_first_pad (GstV4l2Compositor * self)
{
  GList *l;
  GstV4l2VideoAggregatorPad *pad;
  GstV4l2CompositorPad *cpad;

  for (l = GST_ELEMENT (self)->sinkpads; l; l = l->next) {
    pad = l->data;
    cpad = GST_V4L2_COMPOSITOR_PAD (pad);
    return cpad;
  }
  return NULL;
}



static void
gst_v4l2_compositor_get_compose_bounds (GstV4l2Compositor * self, GstV4l2CompositorPad * pad,
    struct v4l2_rect *bounds)
{
  GstVideoInfo *info;

  bounds->left = pad->xpos;
  bounds->top = pad->ypos;

  info = gst_v4l2_m2m_get_video_info (pad->m2m, GST_V4L2_M2M_BUFTYPE_SOURCE);
  if (pad->width == DEFAULT_PAD_WIDTH)
    bounds->width = info->width;
  else
    bounds->width = pad->width;

  if (pad->height == DEFAULT_PAD_HEIGHT)
    bounds->height = info->height;
  else
    bounds->height = pad->height;
}

static void
gst_v4l2_compositor_get_crop_bounds (GstV4l2Compositor * self, GstV4l2CompositorPad * pad,
                                     struct v4l2_rect *bounds)
{
  GstVideoInfo *info;

  info = gst_v4l2_m2m_get_video_info (pad->m2m, GST_V4L2_M2M_BUFTYPE_SINK);
  bounds->left = 0;
  bounds->top = 0;
  bounds->width = info->width;
  bounds->height = info->height;
}



static GstFlowReturn
gst_v4l2_compositor_get_output_buffer (GstV4l2VideoAggregator * vagg,
    GstBuffer ** outbuf)
{
  GList *l;
  GstV4l2Compositor *self = GST_V4L2_COMPOSITOR (vagg);
  GstBuffer *external_sink_buf;
  GstBuffer *sink_buf;
  GstBuffer *returned_source_buf;
  GstBuffer *source_buf;
  gboolean ok;
  GstV4l2VideoAggregatorPad *pad;
  GstV4l2CompositorPad *cpad;
  GstV4l2CompositorPad *first_cpad;

  sink_buf = NULL;
  source_buf = NULL;
  returned_source_buf = NULL;
  (*outbuf) = NULL;

  GST_OBJECT_LOCK (vagg);

  for (l = GST_ELEMENT (self)->sinkpads; l; l = l->next) {
    pad = l->data;
    cpad = GST_V4L2_COMPOSITOR_PAD (pad);

    if (gst_v4l2_aggregator_pad_is_eos (GST_V4L2_AGGREGATOR_PAD (pad))) {
      GST_DEBUG_OBJECT (self, "EOS called, do not process buffer");
      goto not_ready;
    }

    if (!pad->buffer)
      goto not_ready;
  }


  first_cpad = gst_v4l2_compositor_get_first_pad(self);

  returned_source_buf =
      gst_v4l2_m2m_alloc_buffer (first_cpad->m2m, GST_V4L2_M2M_BUFTYPE_SOURCE);
  if (!returned_source_buf) {
    GST_ERROR_OBJECT (self,
        "gst_v4l2_m2m_alloc_buffer() for returned_source_buf failed");
    goto failed;
  }

  for (l = GST_ELEMENT (self)->sinkpads; l; l = l->next) {
    pad = l->data;
    cpad = GST_V4L2_COMPOSITOR_PAD (pad);
    external_sink_buf = pad->buffer;

    /* import external sink buffer */
    sink_buf = gst_v4l2_m2m_alloc_buffer (cpad->m2m, GST_V4L2_M2M_BUFTYPE_SINK);
    if (!sink_buf) {
      GST_ERROR_OBJECT (self,
          "gst_v4l2_m2m_alloc_buffer() for sink_buf failed");
      goto failed;
    }

    ok = gst_v4l2_m2m_import_buffer (cpad->m2m, sink_buf,
                                     external_sink_buf);
    if (!ok) {
      GST_ERROR_OBJECT (self,
          "gst_v4l2_m2m_import_buffer() failed");
      goto failed;
    }

    /* import returned source buffer if needed */
    if (cpad == first_cpad) {
      source_buf = returned_source_buf;
    }

    else {
      source_buf = gst_v4l2_m2m_alloc_buffer (cpad->m2m, GST_V4L2_M2M_BUFTYPE_SOURCE);
      if (!source_buf) {
        GST_ERROR_OBJECT (self,
                          "gst_v4l2_m2m_alloc_buffer() for source_buf failed");
        goto failed;
      }

      ok = gst_v4l2_m2m_import_buffer (cpad->m2m, source_buf,
                                       returned_source_buf);
      if (!ok) {
        GST_ERROR_OBJECT (self,
                          "gst_v4l2_m2m_import_buffer() failed");
        goto failed;
      }
    }

    GST_DEBUG_OBJECT (self, "process output buffer");

    ok = gst_v4l2_m2m_process_frame (cpad->m2m, source_buf, sink_buf);

    if (!ok) {
      GST_ERROR_OBJECT (self, "gst_v4l2_m2m_process() failed");
      goto failed;
    }

    gst_buffer_unref (sink_buf);
    sink_buf = NULL;

    if (cpad != first_cpad)
      gst_buffer_unref (source_buf);
    source_buf = NULL;
  }

  (*outbuf) = returned_source_buf;
  GST_OBJECT_UNLOCK (vagg);
  return GST_FLOW_OK;

not_ready:
  GST_OBJECT_UNLOCK (vagg);
  return GST_FLOW_OK;

failed:
  GST_OBJECT_UNLOCK (vagg);
  if (sink_buf)
    gst_buffer_unref (sink_buf);
  if (source_buf && (source_buf != returned_source_buf))
    gst_buffer_unref (source_buf);
  if (returned_source_buf)
    gst_buffer_unref (returned_source_buf);
  return GST_FLOW_ERROR;
}

static GstFlowReturn
gst_v4l2_compositor_aggregate_frames (GstV4l2VideoAggregator * vagg,
    GstBuffer * outbuf)
{
  GST_DEBUG_OBJECT (vagg, "Aggregate frames");

  /* All has already been done in get_output_buffer */
  return GST_FLOW_OK;
}

static gboolean
gst_v4l2_compositor_negotiated_caps (GstV4l2VideoAggregator * vagg,
    GstCaps * srccaps)
{
  GList *l;
  GstV4l2Compositor *self = GST_V4L2_COMPOSITOR (vagg);
  gboolean result = TRUE;
  int padn;
  GstCaps *sinkcaps;
  GstPad *pad;
  GstV4l2CompositorPad *cpad;
  struct v4l2_rect crop_bounds;
  struct v4l2_rect compose_bounds;
  gboolean ok;

  if (self->already_negotiated)
    return TRUE;

  GST_DEBUG_OBJECT (self, "Use negotiated caps");

  gst_caps_replace (&self->srccaps, srccaps);

  if (!gst_caps_is_fixed (srccaps))
    goto srccaps_not_fixed;

  /** Set format **/
  GST_OBJECT_LOCK (vagg);
  padn = 0;
  for (l = GST_ELEMENT (self)->sinkpads; l; l = l->next) {
    pad = l->data;
    cpad = GST_V4L2_COMPOSITOR_PAD (pad);

    sinkcaps = gst_pad_get_current_caps (pad);
    if (!gst_caps_is_fixed (sinkcaps))
      goto sinkcaps_not_fixed;

    printf("sinkpad #%d:\n", padn);
    printf("------------\n");
    printf("M2M iomodes: %d %d\n", gst_v4l2_m2m_get_sink_iomode(cpad->m2m), gst_v4l2_m2m_get_source_iomode(cpad->m2m));
    printf("CAPS: %s\n\n", gst_caps_to_string(sinkcaps));

    if (!gst_v4l2_m2m_setup (cpad->m2m, self->srccaps, 2, sinkcaps, 2))
      goto setup_failed;

    gst_v4l2_compositor_get_crop_bounds (self, cpad, &crop_bounds);
    gst_v4l2_compositor_get_compose_bounds (self, cpad, &compose_bounds);
    ok = gst_v4l2_m2m_set_selection (cpad->m2m, &crop_bounds, &compose_bounds);
    if (!ok) {
      GST_ERROR_OBJECT (self, "gst_v4l2_m2m_set_selection() failed");
      goto failed;
    }

    padn++;
  }
  GST_OBJECT_UNLOCK (vagg);

  printf("source CAPS:\n", padn);
  printf("------------\n");
  printf("%s\n\n", gst_caps_to_string(self->srccaps));

  self->already_negotiated = TRUE;
  goto done;

setup_failed:
  GST_ERROR_OBJECT (self, "could not setup m2m");
  goto failed;

srccaps_not_fixed:
  GST_ERROR_OBJECT (self, "source caps not fixed: %" GST_PTR_FORMAT,
      self->srccaps);
  goto failed;

sinkcaps_not_fixed:
  GST_ERROR_OBJECT (self,
      "sink caps from source pad#%d not fixed: %" GST_PTR_FORMAT, padn,
      sinkcaps);
  goto failed;

failed:
  result = FALSE;
  goto done;

done:
  return result;
}

static void gst_v4l2_compositor_close (GstV4l2Compositor * self);

static gboolean
gst_v4l2_compositor_open (GstV4l2Compositor * self)
{
  GList *l;
  GstV4l2VideoAggregator *vagg = GST_V4L2_VIDEO_AGGREGATOR (self);
  GstPad *pad;
  GstV4l2CompositorPad *cpad;
  GstV4l2CompositorPad *first_cpad;


  GST_DEBUG_OBJECT (self, "Opening");

  gst_v4l2_compositor_propagate_video_device (self);

  first_cpad = gst_v4l2_compositor_get_first_pad(self);

  GST_OBJECT_LOCK (vagg);

  for (l = GST_ELEMENT (self)->sinkpads; l; l = l->next) {
    pad = l->data;
    cpad = GST_V4L2_COMPOSITOR_PAD (pad);

    gst_v4l2_m2m_set_sink_iomode (cpad->m2m, GST_V4L2_IO_DMABUF_IMPORT);

    if (cpad == first_cpad) {
      gst_v4l2_m2m_set_source_iomode (cpad->m2m, GST_V4L2_IO_DMABUF);
    }
    else {
      gst_v4l2_m2m_set_source_iomode (cpad->m2m, GST_V4L2_IO_DMABUF_IMPORT);
    }

    if (!gst_v4l2_m2m_open (cpad->m2m))
      goto failure;

  }
  GST_OBJECT_UNLOCK (vagg);

  self->already_negotiated = FALSE;
  return TRUE;

failure:
  gst_v4l2_compositor_close (self);
  return FALSE;
}

static void
gst_v4l2_compositor_unlock (GstV4l2Compositor * self)
{
  GList *l;
  GstPad *pad;
  GstV4l2CompositorPad *cpad;

  GST_DEBUG_OBJECT (self, "Unlock");

  GST_OBJECT_LOCK (self);
  for (l = GST_ELEMENT (self)->sinkpads; l; l = l->next) {
    pad = l->data;
    cpad = GST_V4L2_COMPOSITOR_PAD (pad);
    gst_v4l2_m2m_unlock (cpad->m2m);
  }
  GST_OBJECT_UNLOCK (self);
}


static void
gst_v4l2_compositor_unlock_stop (GstV4l2Compositor * self)
{
  GList *l;
  GstPad *pad;
  GstV4l2CompositorPad *cpad;

  GST_DEBUG_OBJECT (self, "Unlock Stop");

  GST_OBJECT_LOCK (self);
  for (l = GST_ELEMENT (self)->sinkpads; l; l = l->next) {
    pad = l->data;
    cpad = GST_V4L2_COMPOSITOR_PAD (pad);
    gst_v4l2_m2m_unlock_stop (cpad->m2m);
  }
  GST_OBJECT_UNLOCK (self);
}




static void
gst_v4l2_compositor_close (GstV4l2Compositor * self)
{
  GList *l;
  GstPad *pad;
  GstV4l2CompositorPad *cpad;

  GST_DEBUG_OBJECT (self, "Closing");

  GST_OBJECT_LOCK (self);
  for (l = GST_ELEMENT (self)->sinkpads; l; l = l->next) {
    pad = l->data;
    cpad = GST_V4L2_COMPOSITOR_PAD (pad);
    gst_v4l2_m2m_close (cpad->m2m);
  }
  GST_OBJECT_UNLOCK (self);

  gst_caps_replace (&self->srccaps, NULL);
}

static gboolean
gst_v4l2_compositor_stop (GstV4l2Aggregator * agg)
{
  GList *l;
  GstPad *pad;
  GstV4l2CompositorPad *cpad;
  GstV4l2Compositor *self = GST_V4L2_COMPOSITOR (agg);

  GST_DEBUG_OBJECT (self, "Stop");

  GST_OBJECT_LOCK (self);
  for (l = GST_ELEMENT (self)->sinkpads; l; l = l->next) {
    pad = l->data;
    cpad = GST_V4L2_COMPOSITOR_PAD (pad);
    gst_v4l2_m2m_stop (cpad->m2m);
  }
  GST_OBJECT_UNLOCK (self);

  gst_caps_replace (&self->srccaps, NULL);

  return TRUE;
}





static gboolean
gst_v4l2_compositor_sink_query (GstV4l2Aggregator * agg,
    GstV4l2AggregatorPad * bpad, GstQuery * query)
{
  switch (GST_QUERY_TYPE (query)) {
    default:
      return GST_V4L2_AGGREGATOR_CLASS (parent_class)->sink_query (agg, bpad,
          query);
  }
}

static gboolean
gst_v4l2_compositor_sink_event (GstV4l2Aggregator * agg,
    GstV4l2AggregatorPad * bpad, GstEvent * event)
{
  gboolean ret;
  GstV4l2Compositor *self = GST_V4L2_COMPOSITOR (agg);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_START:
      GST_DEBUG_OBJECT (self, "flush start");
      gst_v4l2_compositor_unlock (self);
      break;
    default:
      break;
  }

  ret = GST_V4L2_AGGREGATOR_CLASS (parent_class)->sink_event (agg, bpad, event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_STOP:
      /* Buffer should be back now */
      GST_DEBUG_OBJECT (self, "flush stop");
      gst_v4l2_compositor_unlock_stop (self);
      break;

    default:
      break;
  }

  return ret;
}

static GstFlowReturn
gst_v4l2_compositor_flush (GstV4l2Aggregator * agg)
{
  return GST_V4L2_AGGREGATOR_CLASS (parent_class)->flush (agg);
}

static GstV4l2AggregatorPad *
gst_v4l2_compositor_create_new_pad (GstV4l2Aggregator * agg,
    GstPadTemplate * templ, const gchar * req_name, const GstCaps * caps)
{
  GstV4l2AggregatorPad *pad;

  pad = GST_V4L2_AGGREGATOR_CLASS (parent_class)->create_new_pad (agg, templ,
      req_name, caps);

  return pad;
}

static GstStateChangeReturn
gst_v4l2_compositor_change_state (GstElement * element,
    GstStateChange transition)
{
  GstV4l2Compositor *self = GST_V4L2_COMPOSITOR (element);
  GstStateChangeReturn ret;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!gst_v4l2_compositor_open (self))
        return GST_STATE_CHANGE_FAILURE;
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_v4l2_compositor_unlock (self);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_v4l2_compositor_close (self);
      break;
    default:
      break;
  }

  return ret;
}

static void
gst_v4l2_compositor_dispose (GObject * object)
{
  GstV4l2Compositor *self = GST_V4L2_COMPOSITOR (object);

  GST_DEBUG_OBJECT (self, "called");
  gst_caps_replace (&self->srccaps, NULL);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_v4l2_compositor_finalize (GObject * object)
{
  GstV4l2Compositor *self = GST_V4L2_COMPOSITOR (object);
  GstPad *pad;
  GstV4l2CompositorPad *cpad;
  GList *l;

  GST_DEBUG_OBJECT (self, "called");

  for (l = GST_ELEMENT (self)->sinkpads; l; l = l->next) {
    pad = l->data;
    cpad = GST_V4L2_COMPOSITOR_PAD (pad);
    gst_v4l2_m2m_destroy (cpad->m2m);
  }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_v4l2_compositor_init (GstV4l2Compositor * self)
{
  self->output_io_mode = DEFAULT_PROP_IO_MODE;
  self->capture_io_mode = DEFAULT_PROP_IO_MODE;
  self->videodev = g_strdup (DEFAULT_PROP_DEVICE);
}

/* GObject boilerplate */
static void
gst_v4l2_compositor_class_init (GstV4l2CompositorClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstV4l2VideoAggregatorClass *videoaggregator_class =
      (GstV4l2VideoAggregatorClass *) klass;
  GstV4l2AggregatorClass *agg_class = (GstV4l2AggregatorClass *) klass;

  GST_DEBUG_CATEGORY_INIT (gst_v4l2compositor_debug, "v4l2compositor", 0,
      "video compositor");

  gobject_class->dispose = GST_DEBUG_FUNCPTR (gst_v4l2_compositor_dispose);
  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_v4l2_compositor_finalize);
  gobject_class->get_property =
      GST_DEBUG_FUNCPTR (gst_v4l2_compositor_get_property);
  gobject_class->set_property =
      GST_DEBUG_FUNCPTR (gst_v4l2_compositor_set_property);

  agg_class->sinkpads_type = GST_TYPE_V4L2_COMPOSITOR_PAD;
  agg_class->stop = GST_DEBUG_FUNCPTR (gst_v4l2_compositor_stop);
  agg_class->sink_event = GST_DEBUG_FUNCPTR (gst_v4l2_compositor_sink_event);
  agg_class->sink_query = GST_DEBUG_FUNCPTR (gst_v4l2_compositor_sink_query);
  agg_class->flush = GST_DEBUG_FUNCPTR (gst_v4l2_compositor_flush);
  agg_class->create_new_pad =
      GST_DEBUG_FUNCPTR (gst_v4l2_compositor_create_new_pad);
  videoaggregator_class->negotiated_caps =
      GST_DEBUG_FUNCPTR (gst_v4l2_compositor_negotiated_caps);
  videoaggregator_class->update_caps =
      GST_DEBUG_FUNCPTR (gst_v4l2_compositor_update_caps);
  videoaggregator_class->get_output_buffer =
      GST_DEBUG_FUNCPTR (gst_v4l2_compositor_get_output_buffer);
  videoaggregator_class->aggregate_frames =
      GST_DEBUG_FUNCPTR (gst_v4l2_compositor_aggregate_frames);

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_v4l2_compositor_change_state);

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_factory));

  gst_element_class_set_static_metadata (gstelement_class, "V4l2Compositor",
      "Filter/Editor/Video/Compositor",
      "Composite multiple video streams using V4L2 API",
      "Frédéric Sureau <frederic.sureau@veo-labs.com>");

  gst_v4l2_compositor_install_properties_helper (gobject_class);
}



void
gst_v4l2_compositor_install_properties_helper (GObjectClass * gobject_class)
{
  g_object_class_install_property (gobject_class, PROP_DEVICE,
      g_param_spec_string ("device", "Device", "Device location",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_OUTPUT_IO_MODE,
      g_param_spec_enum ("output-io-mode", "Output IO mode",
          "Output side I/O mode (matches sink pad)",
          GST_TYPE_V4L2_IO_MODE, DEFAULT_PROP_IO_MODE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CAPTURE_IO_MODE,
      g_param_spec_enum ("capture-io-mode", "Capture IO mode",
          "Capture I/O mode (matches src pad)",
          GST_TYPE_V4L2_IO_MODE, DEFAULT_PROP_IO_MODE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}
