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
#define DEFAULT_PAD_WIDTH  0
#define DEFAULT_PAD_HEIGHT 0
enum
{
  PROP_PAD_0,
  PROP_PAD_XPOS,
  PROP_PAD_YPOS,
  PROP_PAD_WIDTH,
  PROP_PAD_HEIGHT
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
  GstV4l2VideoAggregatorPad *vagg_pad = GST_V4L2_VIDEO_AGGREGATOR_PAD (comp_pad);
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

  gst_v4l2_m2m_destroy (pad->m2m);
  
  G_OBJECT_CLASS (gst_v4l2_compositor_pad_parent_class)->finalize (object);
}

static void
gst_v4l2_compositor_pad_init (GstV4l2CompositorPad * cpad)
{
  cpad->xpos = DEFAULT_PAD_XPOS;
  cpad->ypos = DEFAULT_PAD_YPOS;
  cpad->width = DEFAULT_PAD_WIDTH;
  cpad->height = DEFAULT_PAD_HEIGHT;

  cpad->m2m = gst_v4l2_m2m_new (NULL, NULL, NULL);

  cpad->m2m->v4l2output->no_initial_format = TRUE;
  cpad->m2m->v4l2output->keep_aspect = FALSE;

  cpad->m2m->v4l2capture->no_initial_format = TRUE;
  cpad->m2m->v4l2capture->keep_aspect = FALSE;
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

  vaggpadclass->set_info = GST_DEBUG_FUNCPTR (gst_v4l2_compositor_pad_set_info);
  vaggpadclass->prepare_frame =
      GST_DEBUG_FUNCPTR (gst_v4l2_compositor_pad_prepare_frame);
  vaggpadclass->clean_frame =
      GST_DEBUG_FUNCPTR (gst_v4l2_compositor_pad_clean_frame);
}


/* GstV4l2Compositor */

enum
{
  PROP_0,
};

static void
gst_v4l2_compositor_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstV4l2Compositor *self = GST_V4L2_COMPOSITOR (object);

  switch (prop_id) {
    default:
      if (!gst_v4l2_m2m_get_property_helper (self->m2m, prop_id, value,
          pspec)) {
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      }
      break;
  }
}

static void
gst_v4l2_compositor_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstV4l2Compositor *self = GST_V4L2_COMPOSITOR (object);

  switch (prop_id) {
    default:
      if (!gst_v4l2_m2m_set_property_helper (self->m2m, prop_id, value,
          pspec)) {
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      }
      break;
  }
}

#define gst_v4l2_compositor_parent_class parent_class
G_DEFINE_TYPE (GstV4l2Compositor, gst_v4l2_compositor, GST_TYPE_V4L2_VIDEO_AGGREGATOR);

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

    _mixer_pad_get_output_size (GST_V4L2_COMPOSITOR (vagg), compositor_pad, &width,
        &height);

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

#define UNSET_QUEUED(buffer) \
    ((buffer).flags &= ~(V4L2_BUF_FLAG_QUEUED | V4L2_BUF_FLAG_DONE))

#define SET_QUEUED(buffer) ((buffer).flags |= V4L2_BUF_FLAG_QUEUED)

#define IS_QUEUED(buffer) \
    ((buffer).flags & (V4L2_BUF_FLAG_QUEUED | V4L2_BUF_FLAG_DONE))

#if 0
static GstFlowReturn
gst_v4l2_compositor_qbuf (GstV4l2Object * obj, GstBuffer * buf)
{
  GstV4l2MemoryGroup *group = NULL;
  GstClockTime timestamp;
  gint i;
  GstFlowReturn ret = GST_FLOW_OK;

  if (!gst_v4l2_is_buffer_valid (buf, &group)) {
    GST_LOG_OBJECT (obj, "unref copied/invalid buffer %p", buf);
    gst_buffer_unref (buf);
    return GST_FLOW_OK;
  }

  GST_LOG_OBJECT (obj, "queuing buffer");

  if (V4L2_TYPE_IS_OUTPUT (obj->type)) {
    enum v4l2_field field;

    /* Except when field is set to alternate, buffer field is the same as
     * the one defined in format */
    if (V4L2_TYPE_IS_MULTIPLANAR (obj->type))
      field = obj->format.fmt.pix_mp.field;
    else
      field = obj->format.fmt.pix.field;

    /* NB: At this moment, we can't have alternate mode because it not handled
     * yet */
    if (field == V4L2_FIELD_ALTERNATE) {
      if (GST_BUFFER_FLAG_IS_SET (buf, GST_VIDEO_FRAME_FLAG_TFF))
        field = V4L2_FIELD_TOP;
      else
        field = V4L2_FIELD_BOTTOM;
    }

    group->buffer.field = field;
  }

  if (GST_BUFFER_TIMESTAMP_IS_VALID (buf)) {
    timestamp = GST_BUFFER_TIMESTAMP (buf);
    GST_TIME_TO_TIMEVAL (timestamp, group->buffer.timestamp);
  }

  /* update sizes */
  if (V4L2_TYPE_IS_MULTIPLANAR (obj->type)) {
    for (i = 0; i < group->n_mem; i++)
      group->planes[i].bytesused =
          gst_memory_get_sizes (group->mem[i], NULL, NULL);
  } else {
    group->buffer.bytesused = gst_memory_get_sizes (group->mem[0], NULL, NULL);
  }
  
  GST_DEBUG_OBJECT (obj, "video_fd=%d", obj->video_fd);

  if (v4l2_ioctl (obj->video_fd, VIDIOC_QBUF, &group->buffer) < 0) {
    ret = GST_FLOW_ERROR;
    GST_ERROR_OBJECT (obj, "failed queing buffer %i: %s",
        group->buffer.index, g_strerror (errno));

    if (IS_QUEUED (group->buffer)) {
      GST_DEBUG_OBJECT (obj,
          "driver pretends buffer is queued even if queue failed");
      UNSET_QUEUED (group->buffer);
    }
    goto done;
  }

  GST_LOG_OBJECT (obj, "queued buffer %i (flags 0x%X)",
      group->buffer.index, group->buffer.flags);

  if (!IS_QUEUED (group->buffer)) {
    GST_DEBUG_OBJECT (obj,
        "driver pretends buffer is not queued even if queue succeeded");
    SET_QUEUED (group->buffer);
  }

  /* Ensure the memory will stay around and is RO */
  for (i = 0; i < group->n_mem; i++)
    gst_memory_ref (group->mem[i]);

done:
  return ret;
}
#endif


static GstFlowReturn
gst_v4l2_compositor_pad_prepare_output_buffer (GstV4l2Compositor * comp, GstV4l2CompositorPad * cpad,
    GstBuffer * inbuf, GstBuffer ** midbuf, GstBuffer ** outbuf)
{
  GstBufferPool *output_pool = GST_BUFFER_POOL (cpad->m2m->v4l2output->pool);
  GstBufferPool *capture_pool = GST_BUFFER_POOL (cpad->m2m->v4l2capture->pool);
  GstFlowReturn ret = GST_FLOW_OK;

  if (inbuf == NULL) {
    GST_WARNING_OBJECT (cpad, "Input buffer is NULL");
    return GST_FLOW_ERROR;
  }

  /* Ensure input internal pool is active */
  if (!gst_buffer_pool_is_active (output_pool)) {
    GstStructure *config = gst_buffer_pool_get_config (output_pool);
    gint min = cpad->m2m->v4l2output->min_buffers == 0 ? GST_V4L2_MIN_BUFFERS :
        cpad->m2m->v4l2output->min_buffers;

    GstCaps * incaps = gst_pad_get_current_caps (GST_PAD (cpad));
    gst_buffer_pool_config_set_params (config, incaps,
        cpad->m2m->v4l2output->info.size, min, min);
    gst_caps_unref (incaps);

    /* There is no reason to refuse this config */
    if (!gst_buffer_pool_set_config (output_pool, config))
      goto activate_failed;

    if (!gst_buffer_pool_set_active (output_pool, TRUE))
      goto activate_failed;
  }

  GST_DEBUG_OBJECT (cpad, "Queue input buffer");
  ret = gst_v4l2_buffer_pool_process (GST_V4L2_BUFFER_POOL (output_pool),
      &inbuf);
  if (G_UNLIKELY (ret != GST_FLOW_OK))
    goto beach;

  do {
    if (!gst_buffer_pool_set_active (cpad->peer_pool, TRUE))
      goto activate_failed;

    GST_DEBUG_OBJECT (cpad, "Dequeue capture buffer");
    if (*midbuf == NULL) {
      ret = gst_buffer_pool_acquire_buffer (cpad->peer_pool, midbuf, NULL);
      if (ret != GST_FLOW_OK)
        goto alloc_failed;
    }

    if (*outbuf != NULL) {
      gst_buffer_unref (*outbuf);
    }

    *outbuf = gst_buffer_ref (*midbuf);
    GST_DEBUG_OBJECT (cpad, "midbuf=%p, outbuf=%p", *midbuf, *outbuf);
    ret = gst_v4l2_buffer_pool_process (GST_V4L2_BUFFER_POOL (capture_pool), outbuf);

  } while (ret == GST_V4L2_FLOW_CORRUPTED_BUFFER);

beach:
  GST_DEBUG_OBJECT (cpad, "Returning outbuf=%p *outbuf=%p", outbuf, *outbuf);
  return ret;

activate_failed:
  GST_ERROR_OBJECT (cpad, "Failed to activate pool");
  return GST_FLOW_ERROR;
  
alloc_failed:
  GST_DEBUG_OBJECT (cpad, "could not allocate buffer from pool");
  return ret;
}

static GstFlowReturn
gst_v4l2_compositor_get_output_buffer (GstV4l2VideoAggregator * vagg, GstBuffer ** outbuf)
{
  GList *l;
  GstV4l2Compositor *self = GST_V4L2_COMPOSITOR (vagg);
  GstV4l2M2m * m2m = self->m2m;
  GstBufferPool *output_pool = GST_BUFFER_POOL (m2m->v4l2output->pool);
  GstBufferPool *capture_pool = GST_BUFFER_POOL (m2m->v4l2capture->pool);
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer * midbuf = NULL;
  GstBuffer * buf = NULL;

  GST_DEBUG_OBJECT (self, "Producing output buffer");

  GST_OBJECT_LOCK (vagg);
  for (l = GST_ELEMENT (vagg)->sinkpads; l; l = l->next) {
    GstV4l2VideoAggregatorPad *pad = l->data;
    GstV4l2CompositorPad *cpad = GST_V4L2_COMPOSITOR_PAD (pad);

    gst_v4l2_compositor_pad_prepare_output_buffer (self, cpad, pad->buffer, &buf, &midbuf);
    GST_DEBUG_OBJECT (self, "Pad returned buf=%p, midbuf=%p", buf, midbuf);
  }
  GST_OBJECT_UNLOCK (vagg);
  
  gst_buffer_unref (buf);

  if (midbuf == NULL) {
    GST_DEBUG_OBJECT (self, "Pads did not produced any buffer");
    goto beach;
  }

  /* Ensure input internal pool is active */
  if (!gst_buffer_pool_is_active (output_pool)) {
    GstStructure *config = gst_buffer_pool_get_config (output_pool);
    gint min = m2m->v4l2output->min_buffers == 0 ? GST_V4L2_MIN_BUFFERS :
        m2m->v4l2output->min_buffers;
    gst_buffer_pool_config_set_params (config, self->outcaps,
        m2m->v4l2output->info.size, min, min);

    /* There is no reason to refuse this config */
    if (!gst_buffer_pool_set_config (output_pool, config))
      goto activate_failed;

    if (!gst_buffer_pool_set_active (output_pool, TRUE))
      goto activate_failed;
  }

  GST_DEBUG_OBJECT (self, "Queue intermediate buffer");
  ret = gst_v4l2_buffer_pool_process (GST_V4L2_BUFFER_POOL (output_pool), &midbuf);
  if (G_UNLIKELY (ret != GST_FLOW_OK))
    goto beach;

  do {
    if (!gst_buffer_pool_set_active (capture_pool, TRUE))
      goto activate_failed;

    GST_DEBUG_OBJECT (self, "Dequeue output buffer");
    ret = gst_buffer_pool_acquire_buffer (capture_pool, outbuf, NULL);

    if (ret != GST_FLOW_OK)
      goto alloc_failed;

    ret = gst_v4l2_buffer_pool_process (GST_V4L2_BUFFER_POOL (capture_pool), outbuf);

  } while (ret == GST_V4L2_FLOW_CORRUPTED_BUFFER);

  gst_buffer_unref (midbuf);

  if (ret != GST_FLOW_OK) {
    gst_buffer_unref (*outbuf);
    *outbuf = NULL;
  }

  GST_OBJECT_UNLOCK (vagg);
  return ret;

beach:
  return ret;

activate_failed:
  GST_ELEMENT_ERROR (self, RESOURCE, SETTINGS,
      ("failed to activate bufferpool"), ("failed to activate bufferpool"));
  return GST_FLOW_ERROR;

alloc_failed:
  GST_DEBUG_OBJECT (self, "could not allocate buffer from pool");
  return ret;
}

static GstFlowReturn
gst_v4l2_compositor_aggregate_frames (GstV4l2VideoAggregator * vagg, GstBuffer * outbuf)
{
  GST_DEBUG_OBJECT (vagg, "Aggregate frames");

  /* All has already been done in get_output_buffer */
  return GST_FLOW_OK;
}

static gboolean
gst_v4l2_compositor_negotiated_caps (GstV4l2VideoAggregator * vagg,
    GstCaps * caps)
{
  GList * l;
  GstV4l2Aggregator * agg = GST_V4L2_AGGREGATOR (vagg);
  GstV4l2Compositor * self = GST_V4L2_COMPOSITOR (vagg);
  GstQuery *query;
  gboolean result = TRUE;

  static gint count = 0;
  if (count != 0)
    return TRUE;
  count = 1;

  GST_DEBUG_OBJECT (self, "Use negotiated caps");

  gst_caps_replace (&self->outcaps, caps);

  /** Set format **/

  GST_OBJECT_LOCK (vagg);
  for (l = GST_ELEMENT (vagg)->sinkpads; l; l = l->next) {
    GstCaps * pad_caps;
    GstPad *pad = l->data;
    GstV4l2CompositorPad *cpad = GST_V4L2_COMPOSITOR_PAD (pad);

    pad_caps = gst_pad_get_current_caps (pad);
    if (!gst_v4l2_object_set_format (cpad->m2m->v4l2output, pad_caps)) {
      goto padcaps_failed;
    }

    if (!gst_v4l2_object_set_format (cpad->m2m->v4l2capture, caps)) {
      goto padcaps_failed;
    }
  }
  GST_OBJECT_UNLOCK (vagg);

  if (!gst_v4l2_object_set_format (self->m2m->v4l2output, caps))
    goto outcaps_failed;

  if (!gst_v4l2_object_set_format (self->m2m->v4l2capture, caps))
    goto outcaps_failed;

  /* TODO set_selection */
  
  /** Do allocations **/

  /* find a pool for the negotiated caps now */
  GST_DEBUG_OBJECT (self, "doing allocation query");
  query = gst_query_new_allocation (caps, TRUE);
  if (!gst_pad_peer_query (agg->srcpad, query)) {
    /* not a problem, just debug a little */
    GST_DEBUG_OBJECT (self, "peer ALLOCATION query failed");
  }

  if (gst_v4l2_object_decide_allocation (self->m2m->v4l2capture, query)) {
    GstBufferPool *pool = GST_BUFFER_POOL (self->m2m->v4l2capture->pool);

    if (!gst_buffer_pool_set_active (pool, TRUE))
      goto activate_failed;
  }

  GST_DEBUG_OBJECT (self, "ALLOCATION (%d) params: %" GST_PTR_FORMAT, result,
      query);

  /* Negotiate allocation between internal m2m and first pad m2m */
  GST_OBJECT_LOCK (vagg);
  for (l = GST_ELEMENT (vagg)->sinkpads; l; l = l->next) {
    GstPad *pad = l->data;
    GstV4l2CompositorPad *cpad = GST_V4L2_COMPOSITOR_PAD (pad);

    query = gst_query_new_allocation (caps, TRUE);
    gst_v4l2_object_propose_allocation (self->m2m->v4l2output, query);

    if (gst_v4l2_object_decide_allocation (cpad->m2m->v4l2capture, query)) {
      GstBufferPool *peer_pool = NULL;

      if (gst_query_get_n_allocation_pools (query) > 0)
        gst_query_parse_nth_allocation_pool (query, 0, &peer_pool, NULL, NULL, NULL);

      cpad->peer_pool = peer_pool;

      if (!gst_buffer_pool_set_active (cpad->m2m->v4l2capture->pool, TRUE))
        goto pad_activate_failed;
    }
  }
  GST_OBJECT_UNLOCK (vagg);

  goto done;

  /* Errors */

padcaps_failed:
  GST_OBJECT_UNLOCK (vagg);
  GST_ERROR_OBJECT (self, "failed to set output caps for pad: %" GST_PTR_FORMAT, caps);
  goto failed;

pad_activate_failed:
  GST_OBJECT_UNLOCK (vagg);
  GST_WARNING_OBJECT (self, "Failed to activate bufferpool for pad");
  goto failed;

outcaps_failed:
  GST_ERROR_OBJECT (self, "failed to set output caps: %" GST_PTR_FORMAT, caps);
  goto failed;

activate_failed:
  GST_WARNING_OBJECT (self, "Failed to activate bufferpool");
  goto failed;

failed:
  result = FALSE;
  goto done;

done:
  gst_query_unref (query);
  return result;
}

static void
gst_v4l2_compositor_close (GstV4l2Compositor * self);

static gboolean
gst_v4l2_compositor_open (GstV4l2Compositor * self)
{
  GList * l;
  GstV4l2VideoAggregator * vagg = GST_V4L2_VIDEO_AGGREGATOR (self);

  GST_DEBUG_OBJECT (self, "Opening");

  if (!gst_v4l2_m2m_open (self->m2m))
    goto failure;

  GST_OBJECT_LOCK (vagg);
  for (l = GST_ELEMENT (vagg)->sinkpads; l; l = l->next) {
    GstV4l2CompositorPad *cpad = l->data;

    if (!gst_v4l2_m2m_open (cpad->m2m))
      goto pad_open_failed;
  }
  GST_OBJECT_UNLOCK (vagg);

  self->probed_sinkcaps = gst_v4l2_object_get_caps (self->m2m->v4l2output,
      gst_v4l2_object_get_raw_caps ());

  if (gst_caps_is_empty (self->probed_sinkcaps))
    goto no_input_format;

  self->probed_srccaps = gst_v4l2_object_get_caps (self->m2m->v4l2capture,
      gst_v4l2_object_get_raw_caps ());

  if (gst_caps_is_empty (self->probed_srccaps))
    goto no_output_format;

  return TRUE;

pad_open_failed:
  GST_OBJECT_UNLOCK (vagg);
  GST_ERROR_OBJECT (self, "failed to open pad m2m");
  goto failure;

no_input_format:
  GST_ELEMENT_ERROR (self, RESOURCE, SETTINGS,
      ("Converter on device %s has no supported input format",
          self->m2m->v4l2output->videodev), (NULL));
  goto failure;

no_output_format:
  GST_ELEMENT_ERROR (self, RESOURCE, SETTINGS,
      ("Converter on device %s has no supported output format",
          self->m2m->v4l2capture->videodev), (NULL));
  goto failure;

failure:
  gst_v4l2_compositor_close (self);
  return FALSE;
}

static void
gst_v4l2_compositor_unlock (GstV4l2Compositor * self)
{
  GList * l;
  GstV4l2VideoAggregator * vagg = GST_V4L2_VIDEO_AGGREGATOR (self);

  GST_DEBUG_OBJECT (self, "Unlock");

  GST_OBJECT_LOCK (vagg);
  for (l = GST_ELEMENT (vagg)->sinkpads; l; l = l->next) {
    GstV4l2CompositorPad *cpad = l->data;

    gst_v4l2_m2m_unlock (cpad->m2m);
  }
  GST_OBJECT_UNLOCK (vagg);

  gst_v4l2_m2m_unlock (self->m2m);
}

static void
gst_v4l2_compositor_close (GstV4l2Compositor * self)
{
  GList * l;
  GstV4l2VideoAggregator * vagg = GST_V4L2_VIDEO_AGGREGATOR (self);

  GST_DEBUG_OBJECT (self, "Closing");

  GST_OBJECT_LOCK (vagg);
  for (l = GST_ELEMENT (vagg)->sinkpads; l; l = l->next) {
    GstV4l2CompositorPad *cpad = l->data;

    gst_v4l2_m2m_close (cpad->m2m);
  }
  GST_OBJECT_UNLOCK (vagg);

  gst_v4l2_m2m_close (self->m2m);
  gst_caps_replace (&self->probed_sinkcaps, NULL);
  gst_caps_replace (&self->probed_srccaps, NULL);
}

static gboolean
gst_v4l2_compositor_stop (GstV4l2Aggregator * agg)
{
  GList * l;
  GstV4l2Compositor * self = GST_V4L2_COMPOSITOR (agg);
  GstV4l2VideoAggregator * vagg = GST_V4L2_VIDEO_AGGREGATOR (self);

  GST_DEBUG_OBJECT (self, "Stop");

  GST_OBJECT_LOCK (vagg);
  for (l = GST_ELEMENT (vagg)->sinkpads; l; l = l->next) {
    GstV4l2CompositorPad *cpad = l->data;

    gst_v4l2_m2m_stop (cpad->m2m);
  }
  GST_OBJECT_UNLOCK (vagg);

  gst_v4l2_m2m_stop (self->m2m);
  gst_caps_replace (&self->outcaps, NULL);

  return TRUE;
}

static gboolean
gst_v4l2_compositor_sink_query (GstV4l2Aggregator * agg,
    GstV4l2AggregatorPad * bpad, GstQuery * query)
{
  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_ALLOCATION:{
      GstV4l2CompositorPad * cpad = GST_V4L2_COMPOSITOR_PAD (bpad);

      GST_DEBUG_OBJECT (agg, "Allocation");

      return gst_v4l2_object_propose_allocation (cpad->m2m->v4l2output, query);
    }
    default:
      return GST_V4L2_AGGREGATOR_CLASS (parent_class)->sink_query (agg, bpad, query);
  }
}

static gboolean
gst_v4l2_compositor_sink_event (GstV4l2Aggregator * agg,
    GstV4l2AggregatorPad * bpad, GstEvent * event)
{
  GstV4l2CompositorPad *cpad = GST_V4L2_COMPOSITOR_PAD (bpad);
  gboolean ret;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_START:
      GST_DEBUG_OBJECT (cpad, "flush start");
      gst_v4l2_m2m_unlock (cpad->m2m);
      break;
    default:
      break;
  }

  ret = GST_V4L2_AGGREGATOR_CLASS (parent_class)->sink_event (agg, bpad, event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_STOP:
      /* Buffer should be back now */
      GST_DEBUG_OBJECT (cpad, "flush stop");
      gst_v4l2_m2m_unlock_stop (cpad->m2m);
      break;
    default:
      break;
  }

  return ret;
}

static GstFlowReturn
gst_v4l2_compositor_flush (GstV4l2Aggregator * agg)
{
  GstV4l2Compositor *self = GST_V4L2_COMPOSITOR (agg);

  GST_DEBUG_OBJECT (self, "flush stop");
  gst_v4l2_m2m_unlock_stop (self->m2m);

  return GST_V4L2_AGGREGATOR_CLASS (parent_class)->flush (agg);
}

static GstV4l2AggregatorPad *
gst_v4l2_compositor_create_new_pad (GstV4l2Aggregator * agg,
    GstPadTemplate * templ, const gchar * req_name, const GstCaps * caps)
{
  GstV4l2AggregatorPad * pad;
  GstV4l2CompositorPad * cpad;
  GstV4l2Compositor *self = GST_V4L2_COMPOSITOR (agg);

  pad = GST_V4L2_AGGREGATOR_CLASS (parent_class)->create_new_pad (agg, templ,
      req_name, caps);

  cpad = GST_V4L2_COMPOSITOR_PAD (pad);
//  cpad->m2m->v4l2output->videodev = g_strdup (self->m2m->v4l2output->videodev);
  cpad->m2m->v4l2output->videodev = g_strdup ("/dev/v4l/by-path/ipu1-scaler");
  cpad->m2m->v4l2output->req_mode = GST_V4L2_IO_DMABUF_IMPORT;
  cpad->m2m->v4l2capture->req_mode = GST_V4L2_IO_DMABUF_IMPORT;
  cpad->m2m->v4l2output->element = (GstElement *)self;
  cpad->m2m->v4l2capture->element = (GstElement *)self;

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

  gst_caps_replace (&self->probed_sinkcaps, NULL);
  gst_caps_replace (&self->probed_srccaps, NULL);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_v4l2_compositor_finalize (GObject * object)
{
  GstV4l2Compositor *self = GST_V4L2_COMPOSITOR (object);

  gst_v4l2_m2m_destroy (self->m2m);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_v4l2_compositor_init (GstV4l2Compositor * self)
{
  /* Create scaler instance for src buffers allocations */
  self->m2m = gst_v4l2_m2m_new (GST_ELEMENT (self), NULL, NULL);

  self->m2m->v4l2output->no_initial_format = TRUE;
  self->m2m->v4l2output->keep_aspect = FALSE;

  self->m2m->v4l2capture->no_initial_format = TRUE;
  self->m2m->v4l2capture->keep_aspect = FALSE;
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

  gst_v4l2_object_install_m2m_properties_helper (gobject_class);
}

