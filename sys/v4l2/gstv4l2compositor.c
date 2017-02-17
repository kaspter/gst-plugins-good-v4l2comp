/* Video compositor plugin using V4L2 abilities
 * Copyright (C) 2016 Sebastien MATZ <sebastien.matz@veo-labs.com>
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
        g_free ((gpointer) pad->videodev);
      pad->videodev = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
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
  G_OBJECT_CLASS (gst_v4l2_compositor_pad_parent_class)->finalize (object);
}

static void
gst_v4l2_compositor_pad_init (GstV4l2CompositorPad * cpad)
{
  cpad->xpos = DEFAULT_PAD_XPOS;
  cpad->ypos = DEFAULT_PAD_YPOS;
  cpad->width = DEFAULT_PAD_WIDTH;
  cpad->height = DEFAULT_PAD_HEIGHT;
  cpad->m2m = NULL;
  cpad->jobs = NULL;
  cpad->prepared_jobs = NULL;
  cpad->queued_jobs = NULL;
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
#define DEFAULT_PROP_NUMJOBS  0
enum
{
  PROP_0,
  PROP_DEVICE,
  PROP_NUMJOBS,
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
    case PROP_NUMJOBS:
      g_value_set_int (value, self->prop_number_of_jobs);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_v4l2_compositor_propagate_video_device (GstV4l2Compositor * self)
{
  GList *it;
  GstV4l2VideoAggregatorPad *pad;
  GstV4l2CompositorPad *cpad;

  for (it = GST_ELEMENT (self)->sinkpads; it; it = it->next) {
    pad = it->data;
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

  switch (prop_id) {
    case PROP_DEVICE:
      g_free (self->videodev);
      self->videodev = g_value_dup_string (value);
      break;
    case PROP_NUMJOBS:
      self->prop_number_of_jobs = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

#define gst_v4l2_compositor_parent_class parent_class
G_DEFINE_TYPE (GstV4l2Compositor, gst_v4l2_compositor,
    GST_TYPE_V4L2_VIDEO_AGGREGATOR);

static void
gst_v4l2_compositor_get_compose_bounds (GstV4l2Compositor * self,
    GstV4l2CompositorPad * pad, struct v4l2_rect *bounds)
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
gst_v4l2_compositor_get_crop_bounds (GstV4l2Compositor * self,
    GstV4l2CompositorPad * pad, struct v4l2_rect *bounds)
{
  GstVideoInfo *info;

  info = gst_v4l2_m2m_get_video_info (pad->m2m, GST_V4L2_M2M_BUFTYPE_SINK);
  bounds->left = 0;
  bounds->top = 0;
  bounds->width = info->width;
  bounds->height = info->height;
}

static GstV4l2CompositorPad *
gst_v4l2_compositor_get_master_pad (GstV4l2Compositor * self)
{
  GList *it;
  GstV4l2VideoAggregatorPad *pad;
  GstV4l2CompositorPad *cpad;

  if (self->master_cpad != NULL)
    return self->master_cpad;

  for (it = GST_ELEMENT (self)->sinkpads; it; it = it->next) {
    pad = it->data;
    cpad = GST_V4L2_COMPOSITOR_PAD (pad);
    self->master_cpad = cpad;
    return cpad;
  }

  return NULL;
}

static gboolean
gst_v4l2_get_number_of_jobs (GstV4l2Compositor * self)
{
  GList *it;
  GstPad *pad;
  GstV4l2CompositorPad *cpad;
  int njobs, returned_njobs;

  if (self->number_of_jobs > 0)
    return self->number_of_jobs;

  if (self->prop_number_of_jobs > 0)
    returned_njobs = self->prop_number_of_jobs;

  else {
    returned_njobs = 0;
    for (it = GST_ELEMENT (self)->sinkpads; it; it = it->next) {
      pad = it->data;
      cpad = GST_V4L2_COMPOSITOR_PAD (pad);
      njobs = gst_v4l2_m2m_get_min_sink_buffers (cpad->m2m);
      returned_njobs = MAX (njobs, returned_njobs);
      njobs = gst_v4l2_m2m_get_min_source_buffers (cpad->m2m);
      returned_njobs = MAX (njobs, returned_njobs);
    }
    returned_njobs = returned_njobs + 2;
  }

  self->number_of_jobs = returned_njobs;
  return returned_njobs;
}

static GstV4l2CompositorJob *
gst_v4l2_compositor_create_job (GstV4l2Compositor * self,
    GstV4l2CompositorPad * cpad)
{
  GstV4l2CompositorJob *job;
  GstBuffer *sink_buf;
  GstBuffer *source_buf;

  if (cpad == NULL)
    return NULL;

  job = g_new0 (GstV4l2CompositorJob, 1);
  if (job == NULL)
    return NULL;

  job->parent = self;
  job->sink_buf = NULL;
  job->source_buf = NULL;
  job->external_sink_buf = NULL;
  job->cpad = cpad;
  job->state = GST_V4L2_COMPOSITOR_JOB_READY;

  sink_buf = gst_v4l2_m2m_alloc_buffer (cpad->m2m, GST_V4L2_M2M_BUFTYPE_SINK);
  if (sink_buf == NULL)
    goto failed;
  job->sink_buf = sink_buf;

  source_buf =
      gst_v4l2_m2m_alloc_buffer (cpad->m2m, GST_V4L2_M2M_BUFTYPE_SOURCE);
  if (source_buf == NULL)
    goto failed;
  job->source_buf = source_buf;

  return job;

failed:
  if (job->source_buf)
    gst_buffer_unref (job->source_buf);

  if (job->sink_buf)
    gst_buffer_unref (job->sink_buf);

  g_free (job);
  return NULL;
}

static GstV4l2CompositorJob *
gst_v4l2_compositor_lookup_job (GstV4l2Compositor * self,
    GstV4l2CompositorPad * cpad)
{
  GList *it;
  GstV4l2CompositorJob *job;

  for (it = cpad->jobs; it; it = it->next) {
    job = it->data;
    if (job->state != GST_V4L2_COMPOSITOR_JOB_READY)
      continue;

    cpad->jobs = g_list_delete_link (cpad->jobs, it);
    cpad->jobs = g_list_append (cpad->jobs, job);
    return job;
  }

  return NULL;
}

static gboolean
gst_v4l2_compositor_ensure_jobs (GstV4l2Compositor * self)
{
  GList *it;
  GstV4l2CompositorJob *job;
  GstV4l2VideoAggregatorPad *pad;
  GstV4l2CompositorPad *cpad;
  int i, njobs;

  njobs = gst_v4l2_get_number_of_jobs (self);

  for (it = GST_ELEMENT (self)->sinkpads; it; it = it->next) {
    pad = it->data;
    cpad = GST_V4L2_COMPOSITOR_PAD (pad);
    if (cpad->jobs != NULL)
      return TRUE;

    for (i = 0; i < njobs; i++) {
      job = gst_v4l2_compositor_create_job (self, cpad);
      if (job == NULL) {
        GST_ERROR_OBJECT (self, "gst_v4l2_compositor_create_job() failed");
        return FALSE;
      }
      cpad->jobs = g_list_append (cpad->jobs, job);
    }
  }

  return TRUE;
}

static gboolean
gst_v4l2_compositor_prepare_jobs (GstV4l2Compositor * self)
{
  GList *it;
  GList *it2;
  GstV4l2CompositorJob *job;
  GstV4l2VideoAggregatorPad *pad;
  GstV4l2CompositorPad *cpad;
  GstBuffer *external_sink_buf;
  gboolean found;

  for (it = GST_ELEMENT (self)->sinkpads; it; it = it->next) {
    pad = it->data;
    cpad = GST_V4L2_COMPOSITOR_PAD (pad);
    external_sink_buf = pad->buffer;
    if (external_sink_buf == NULL)
      continue;

    found = FALSE;
    for (it2 = cpad->jobs; it2; it2 = it2->next) {
      job = it2->data;
      if ((job->state != GST_V4L2_COMPOSITOR_JOB_PREPARED)
          && (job->state != GST_V4L2_COMPOSITOR_JOB_QUEUED))
        continue;
      if (external_sink_buf == job->external_sink_buf) {
        found = TRUE;
        break;
      }
    }

    if (found)
      continue;

    job = gst_v4l2_compositor_lookup_job (self, cpad);
    if (job == NULL) {
      GST_WARNING_OBJECT (self, "gst_v4l2_compositor_lookup_job() failed");
      continue;
    }

    cpad->prepared_jobs = g_list_append (cpad->prepared_jobs, job);
    job->external_sink_buf = external_sink_buf;
    gst_buffer_ref (job->external_sink_buf);
    job->state = GST_V4L2_COMPOSITOR_JOB_PREPARED;
  }

  return TRUE;
}

static gboolean
gst_v4l2_compositor_queue_jobs (GstV4l2Compositor * self)
{
  GList *it;
  GstV4l2CompositorJob *master_job;
  GstV4l2CompositorJob *job;
  GstV4l2VideoAggregatorPad *pad;
  GstV4l2CompositorPad *cpad;
  GstV4l2CompositorPad *master_cpad;
  gboolean ok;
  int njobs, nbufs;

  master_cpad = gst_v4l2_compositor_get_master_pad (self);
  master_job = g_list_nth_data (master_cpad->prepared_jobs, 0);
  for (it = GST_ELEMENT (self)->sinkpads; it; it = it->next) {
    pad = it->data;
    cpad = GST_V4L2_COMPOSITOR_PAD (pad);
    job = g_list_nth_data (cpad->prepared_jobs, 0);
    if (job == NULL)
      return TRUE;
    njobs = g_list_length (cpad->queued_jobs);
    nbufs = gst_v4l2_m2m_get_min_sink_buffers (cpad->m2m);
    if (njobs > nbufs)
      return TRUE;
  }

  for (it = GST_ELEMENT (self)->sinkpads; it; it = it->next) {
    pad = it->data;
    cpad = GST_V4L2_COMPOSITOR_PAD (pad);

    job = g_list_nth_data (cpad->prepared_jobs, 0);
    if (job->external_sink_buf) {
      if (job->cpad != master_cpad) {
        ok = gst_v4l2_m2m_import_buffer (cpad->m2m, job->source_buf,
            master_job->source_buf);
        if (!ok) {
          GST_ERROR_OBJECT (cpad->m2m->parent,
              "gst_v4l2_m2m_import_buffer() failed");
          return FALSE;
        }
      }

      ok = gst_v4l2_m2m_import_buffer (cpad->m2m, job->sink_buf,
          job->external_sink_buf);
      if (!ok) {
        GST_ERROR_OBJECT (cpad->m2m->parent,
            "gst_v4l2_m2m_import_buffer() failed");
        return FALSE;
      }

      ok = gst_v4l2_m2m_qbuf (cpad->m2m, job->sink_buf);
      if (!ok) {
        GST_ERROR_OBJECT (self, "gst_v4l2_m2m_qbuf() failed");
        return FALSE;
      }

      ok = gst_v4l2_m2m_qbuf (cpad->m2m, job->source_buf);
      if (!ok) {
        GST_ERROR_OBJECT (self, "gst_v4l2_m2m_qbuf() failed");
        return FALSE;
      }

      ok = gst_v4l2_m2m_require_streamon (cpad->m2m);
      if (!ok) {
        GST_ERROR_OBJECT (self, "gst_v4l2_m2m_require_streamon() failed");
        return FALSE;
      }
    }

    job->master_job = master_job;
    job->state = GST_V4L2_COMPOSITOR_JOB_QUEUED;
    cpad->prepared_jobs = g_list_remove (cpad->prepared_jobs, job);
    cpad->queued_jobs = g_list_append (cpad->queued_jobs, job);
  }

  return TRUE;
}

static gboolean
gst_v4l2_compositor_dispose_output_buffer (GstBuffer * buf, gpointer user_data)
{
  GstV4l2CompositorJob *outjob = (GstV4l2CompositorJob *) user_data;
  GstV4l2Compositor *self = GST_V4L2_COMPOSITOR (outjob->parent);
  gboolean ok;
  GstV4l2M2mMeta *emeta;

  emeta = gst_v4l2_m2m_get_meta (buf);

  gst_buffer_ref (buf);

  ok = gst_v4l2_m2m_reset_buffer (emeta->instance, buf);
  if (!ok) {
    GST_ERROR_OBJECT (emeta->instance, "gst_v4l2_m2m_reset_buffer() failed");
    return TRUE;
  }

  if (self->cleanup_jobs) {
    /* ok = gst_v4l2_m2m_qbuf (buf->m2m_instance, buf); */
    /* if (!ok) */
    /*   GST_ERROR_OBJECT (self, "gst_v4l2_m2m_qbuf() failed when releasing buffer"); */

    g_atomic_int_add (&buf->mini_object.refcount, -1);

    return TRUE;
  }

  outjob->state = GST_V4L2_COMPOSITOR_JOB_READY;
  return FALSE;
}

static gboolean
gst_v4l2_compositor_dequeue_jobs (GstV4l2Compositor * self,
    GstV4l2CompositorJob ** outjob)
{
  GList *it;
  GstV4l2CompositorPad *master_cpad;
  GstV4l2CompositorJob *job;
  gboolean ok;
  GstV4l2VideoAggregatorPad *pad;
  GstV4l2CompositorPad *cpad;
  int njobs, nbufs;

  (*outjob) = NULL;
  for (it = GST_ELEMENT (self)->sinkpads; it; it = it->next) {
    pad = it->data;
    cpad = GST_V4L2_COMPOSITOR_PAD (pad);
    njobs = g_list_length (cpad->queued_jobs);
    nbufs = gst_v4l2_m2m_get_min_source_buffers (cpad->m2m);
    if (njobs <= nbufs)
      return TRUE;
  }

  master_cpad = gst_v4l2_compositor_get_master_pad (self);

  for (it = GST_ELEMENT (self)->sinkpads; it; it = it->next) {
    pad = it->data;
    cpad = GST_V4L2_COMPOSITOR_PAD (pad);

    job = g_list_nth_data (cpad->queued_jobs, 0);

    if (job->cpad == master_cpad)
      (*outjob) = job;

    if (job->external_sink_buf) {
      ok = gst_v4l2_m2m_dqbuf (job->cpad->m2m, job->sink_buf);
      if (!ok) {
        GST_ERROR_OBJECT (self, "gst_v4l2_m2m_dqbuf() failed");
        return FALSE;
      }

      ok = gst_v4l2_m2m_dqbuf (job->cpad->m2m, job->source_buf);
      if (!ok) {
        GST_ERROR_OBJECT (self, "gst_v4l2_m2m_dqbuf() failed");
        return FALSE;
      }

      if (job->cpad != master_cpad) {
        ok = gst_v4l2_m2m_reset_buffer (job->cpad->m2m, job->source_buf);
        if (!ok) {
          GST_ERROR_OBJECT (self, "gst_v4l2_m2m_reset_buffer() failed");
          return FALSE;
        }
      }

      ok = gst_v4l2_m2m_reset_buffer (job->cpad->m2m, job->sink_buf);
      if (!ok) {
        GST_ERROR_OBJECT (self, "gst_v4l2_m2m_reset_buffer() failed");
        return FALSE;
      }

      gst_buffer_unref (job->external_sink_buf);
      job->external_sink_buf = NULL;
    }

    job->state = GST_V4L2_COMPOSITOR_JOB_READY;
    job->master_job = NULL;
    cpad->queued_jobs = g_list_remove (cpad->queued_jobs, job);
  }

  return TRUE;
}

static void
gst_v4l2_compositor_cleanup_jobs (GstV4l2Compositor * self)
{
  GList *it;
  GList *it2;
  GstV4l2VideoAggregatorPad *pad;
  GstV4l2CompositorPad *cpad;
  GstV4l2CompositorJob *job;

  self->cleanup_jobs = TRUE;

  for (it = GST_ELEMENT (self)->sinkpads; it; it = it->next) {
    pad = it->data;
    cpad = GST_V4L2_COMPOSITOR_PAD (pad);
    g_list_free (cpad->prepared_jobs);
    cpad->prepared_jobs = NULL;
    g_list_free (cpad->queued_jobs);
    cpad->queued_jobs = NULL;

    for (it2 = cpad->jobs; it2; it2 = it2->next) {
      job = it2->data;
      if (job->state == GST_V4L2_COMPOSITOR_JOB_QUEUED) {
        gst_v4l2_m2m_dqbuf (cpad->m2m, job->source_buf);
        gst_v4l2_m2m_dqbuf (cpad->m2m, job->sink_buf);
        gst_buffer_unref (job->external_sink_buf);
        job->state = GST_V4L2_COMPOSITOR_JOB_READY;
      }
      if (job->source_buf && job->state != GST_V4L2_COMPOSITOR_JOB_GONE)
        gst_buffer_unref (job->source_buf);
      if (job->sink_buf)
        gst_buffer_unref (job->sink_buf);
      g_free (job);
    }
    g_list_free (cpad->jobs);
    cpad->jobs = NULL;
  }
}

static gboolean
gst_v4l2_compositor_is_eos (GstV4l2Compositor * self)
{
  GList *it;
  GstV4l2VideoAggregatorPad *pad;

  /* do not process any buffers if EOS is called on a sink pad */
  for (it = GST_ELEMENT (self)->sinkpads; it; it = it->next) {
    pad = it->data;

    if (gst_v4l2_aggregator_pad_is_eos (GST_V4L2_AGGREGATOR_PAD (pad))) {
      GST_DEBUG_OBJECT (self, "EOS called, do not process any buffers");
      return TRUE;
    }
  }

  return FALSE;
}

static GstFlowReturn
gst_v4l2_compositor_get_output_buffer (GstV4l2VideoAggregator * vagg,
    GstBuffer ** outbuf_p)
{
  gboolean ok, is_eos;
  GstV4l2Compositor *self = GST_V4L2_COMPOSITOR (vagg);
  GstV4l2CompositorJob *outjob;
  GstBuffer *outbuf;
  GstV4l2M2mMeta *emeta;

  GST_OBJECT_LOCK (vagg);

  (*outbuf_p) = NULL;

  is_eos = gst_v4l2_compositor_is_eos (self);
  if (is_eos)
    goto eos;

  ok = gst_v4l2_compositor_ensure_jobs (self);
  if (!ok) {
    GST_ERROR_OBJECT (self, "gst_v4l2_compositor_ensure_jobs() failed");
    goto failed;
  }

  ok = gst_v4l2_compositor_prepare_jobs (self);
  if (!ok) {
    GST_ERROR_OBJECT (self, "gst_v4l2_compositor_prepare_jobs() failed");
    goto failed;
  }

  ok = gst_v4l2_compositor_queue_jobs (self);
  if (!ok) {
    GST_ERROR_OBJECT (self, "gst_v4l2_compositor_queue_jobs() failed");
    goto failed;
  }

  ok = gst_v4l2_compositor_dequeue_jobs (self, &outjob);
  if (!ok) {
    GST_ERROR_OBJECT (self, "gst_v4l2_compositor_dequeue_jobs() failed");
    goto failed;
  }

  if (outjob) {
    outbuf = outjob->source_buf;
    emeta = gst_v4l2_m2m_get_meta (outbuf);
    emeta->user_data = (gpointer) outjob;
    emeta->dispose = gst_v4l2_compositor_dispose_output_buffer;
    outjob->state = GST_V4L2_COMPOSITOR_JOB_GONE;
    (*outbuf_p) = outbuf;
  }

  GST_OBJECT_UNLOCK (vagg);
  return GST_FLOW_OK;

eos:
  GST_OBJECT_UNLOCK (vagg);
  return GST_FLOW_EOS;

failed:
  GST_OBJECT_UNLOCK (vagg);
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
  GList *it;
  GstV4l2Compositor *self = GST_V4L2_COMPOSITOR (vagg);
  gboolean result = TRUE;
  GstCaps *sinkcaps;
  GstPad *pad;
  GstV4l2CompositorPad *cpad;
  struct v4l2_rect crop_bounds;
  struct v4l2_rect compose_bounds;
  gboolean ok;
  int njobs;

  if (self->already_negotiated)
    return TRUE;

  GST_DEBUG_OBJECT (self, "Use negotiated caps");

  gst_caps_replace (&self->srccaps, srccaps);

  if (!gst_caps_is_fixed (srccaps))
    goto srccaps_not_fixed;

  njobs = gst_v4l2_get_number_of_jobs (self);

  /** Set format **/
  for (it = GST_ELEMENT (self)->sinkpads; it; it = it->next) {
    pad = it->data;
    cpad = GST_V4L2_COMPOSITOR_PAD (pad);

    sinkcaps = gst_pad_get_current_caps (pad);
    if (sinkcaps == NULL)
      goto sinkcaps_not_ready;
  }

  for (it = GST_ELEMENT (self)->sinkpads; it; it = it->next) {
    pad = it->data;
    cpad = GST_V4L2_COMPOSITOR_PAD (pad);

    if (!gst_caps_is_fixed (sinkcaps))
      goto sinkcaps_not_fixed;

    if (!gst_v4l2_m2m_setup (cpad->m2m, self->srccaps, sinkcaps, njobs))
      goto setup_failed;

    gst_v4l2_compositor_get_crop_bounds (self, cpad, &crop_bounds);
    gst_v4l2_compositor_get_compose_bounds (self, cpad, &compose_bounds);
    ok = gst_v4l2_m2m_set_selection (cpad->m2m, &crop_bounds, &compose_bounds);
    if (!ok) {
      GST_ERROR_OBJECT (self, "gst_v4l2_m2m_set_selection() failed");
      goto failed;
    }
  }

  self->already_negotiated = TRUE;
  goto end;

sinkcaps_not_ready:
  GST_ERROR_OBJECT (self, "sink caps not ready: %" GST_PTR_FORMAT, sinkcaps);
  goto end;

setup_failed:
  GST_ERROR_OBJECT (self, "could not setup m2m");
  goto failed;

srccaps_not_fixed:
  GST_ERROR_OBJECT (self, "source caps not fixed: %" GST_PTR_FORMAT,
      self->srccaps);
  goto failed;

sinkcaps_not_fixed:
  GST_ERROR_OBJECT (self, "sink caps not fixed: %" GST_PTR_FORMAT, sinkcaps);
  goto end;

failed:
  result = FALSE;
  goto end;

end:
  return result;
}

static void gst_v4l2_compositor_close (GstV4l2Compositor * self);

static gboolean
gst_v4l2_compositor_open (GstV4l2Compositor * self)
{
  GList *it;
  GstV4l2VideoAggregator *vagg = GST_V4L2_VIDEO_AGGREGATOR (self);
  GstPad *pad;
  GstV4l2CompositorPad *cpad;
  GstV4l2CompositorPad *master_cpad;
  int num;

  GST_DEBUG_OBJECT (self, "Opening");

  GST_OBJECT_LOCK (vagg);

  num = 0;
  for (it = GST_ELEMENT (self)->sinkpads; it; it = it->next) {
    pad = it->data;
    cpad = GST_V4L2_COMPOSITOR_PAD (pad);
    cpad->m2m = gst_v4l2_m2m_new (GST_ELEMENT (self), num);
    num++;
  }

  gst_v4l2_compositor_propagate_video_device (self);

  master_cpad = gst_v4l2_compositor_get_master_pad (self);


  for (it = GST_ELEMENT (self)->sinkpads; it; it = it->next) {
    pad = it->data;
    cpad = GST_V4L2_COMPOSITOR_PAD (pad);

    gst_v4l2_m2m_set_sink_iomode (cpad->m2m, GST_V4L2_IO_DMABUF_IMPORT);

    if (cpad == master_cpad) {
      gst_v4l2_m2m_set_source_iomode (cpad->m2m, GST_V4L2_IO_DMABUF);
    } else {
      gst_v4l2_m2m_set_source_iomode (cpad->m2m, GST_V4L2_IO_DMABUF_IMPORT);
    }

    if (!gst_v4l2_m2m_open (cpad->m2m))
      goto failure;
  }

  if (!gst_v4l2_m2m_set_background (master_cpad->m2m, 0))
    GST_DEBUG_OBJECT (self, "could not set background color");

  self->already_negotiated = FALSE;
  self->master_cpad = NULL;

  GST_OBJECT_UNLOCK (vagg);
  return TRUE;

failure:
  GST_OBJECT_UNLOCK (vagg);
  gst_v4l2_compositor_close (self);
  return FALSE;
}

gboolean
gst_v4l2_m2m_open (GstV4l2M2m * m2m)
{
  if (!gst_v4l2_object_open (m2m->sink_obj))
    return FALSE;

  if (!gst_v4l2_object_open_shared (m2m->source_obj, m2m->sink_obj)) {
    gst_v4l2_object_close (m2m->sink_obj);
    return FALSE;
  }

  return TRUE;
}

static void
gst_v4l2_compositor_unlock (GstV4l2Compositor * self)
{
  GList *it;
  GstPad *pad;
  GstV4l2CompositorPad *cpad;

  GST_DEBUG_OBJECT (self, "Unlock");

  GST_OBJECT_LOCK (self);
  for (it = GST_ELEMENT (self)->sinkpads; it; it = it->next) {
    pad = it->data;
    cpad = GST_V4L2_COMPOSITOR_PAD (pad);
    gst_v4l2_m2m_unlock (cpad->m2m);
  }
  GST_OBJECT_UNLOCK (self);
}

static void
gst_v4l2_compositor_unlock_stop (GstV4l2Compositor * self)
{
  GList *it;
  GstPad *pad;
  GstV4l2CompositorPad *cpad;

  GST_DEBUG_OBJECT (self, "Unlock Stop");

  GST_OBJECT_LOCK (self);
  for (it = GST_ELEMENT (self)->sinkpads; it; it = it->next) {
    pad = it->data;
    cpad = GST_V4L2_COMPOSITOR_PAD (pad);
    gst_v4l2_m2m_unlock_stop (cpad->m2m);
  }
  GST_OBJECT_UNLOCK (self);
}

static void
gst_v4l2_compositor_close (GstV4l2Compositor * self)
{
  GList *it;
  GstPad *pad;
  GstV4l2CompositorPad *cpad;

  GST_DEBUG_OBJECT (self, "Closing");

  GST_OBJECT_LOCK (self);

  for (it = GST_ELEMENT (self)->sinkpads; it; it = it->next) {
    pad = it->data;
    cpad = GST_V4L2_COMPOSITOR_PAD (pad);
    gst_v4l2_m2m_close (cpad->m2m);
  }
  GST_OBJECT_UNLOCK (self);

  gst_caps_replace (&self->srccaps, NULL);
}

static gboolean
gst_v4l2_compositor_stop (GstV4l2Aggregator * agg)
{
  GList *it;
  GstPad *pad;
  GstV4l2CompositorPad *cpad;
  GstV4l2Compositor *self = GST_V4L2_COMPOSITOR (agg);

  GST_DEBUG_OBJECT (self, "Stop");

  gst_v4l2_compositor_cleanup_jobs (self);

  GST_OBJECT_LOCK (self);
  for (it = GST_ELEMENT (self)->sinkpads; it; it = it->next) {
    pad = it->data;
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
  gboolean update;
  GstBufferPool *pool = NULL;
  guint size, min, max = 0;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_ALLOCATION:
      if (gst_query_get_n_allocation_pools (query) > 0) {
        gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min,
            &max);
        update = TRUE;
      } else {
        pool = NULL;
        min = max = 0;
        size = 0;
        update = FALSE;
      }

      if (update)
        gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
      else
        gst_query_add_allocation_pool (query, pool, size, min, max);

      if (pool)
        gst_object_unref (pool);

      return TRUE;
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
  GstPad *pad;
  GstV4l2CompositorPad *cpad;
  GList *it;

  GST_DEBUG_OBJECT (self, "called");

  for (it = GST_ELEMENT (self)->sinkpads; it; it = it->next) {
    GST_DEBUG_OBJECT (self, "calling destroy");
    pad = it->data;
    cpad = GST_V4L2_COMPOSITOR_PAD (pad);
  }

  gst_caps_replace (&self->srccaps, NULL);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_v4l2_compositor_finalize (GObject * object)
{
  GstV4l2Compositor *self = GST_V4L2_COMPOSITOR (object);

  GST_DEBUG_OBJECT (self, "called");

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_v4l2_compositor_init (GstV4l2Compositor * self)
{
  self->videodev = g_strdup (DEFAULT_PROP_DEVICE);
  self->number_of_sinkpads = -1;
  self->number_of_jobs = 0;
  self->prop_number_of_jobs = DEFAULT_PROP_NUMJOBS;
  self->cleanup_jobs = FALSE;
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
      "Sebastien MATZ <sebastien.matz@veo-labs.com>");

  gst_v4l2_compositor_install_properties_helper (gobject_class);
}

void
gst_v4l2_compositor_install_properties_helper (GObjectClass * gobject_class)
{
  g_object_class_install_property (gobject_class, PROP_DEVICE,
      g_param_spec_string ("device", "Device", "Device location",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_NUMJOBS,
      g_param_spec_int ("num_jobs", "num_jobs", "num_jobs",
          0, G_MAXINT, DEFAULT_PROP_NUMJOBS, G_PARAM_READWRITE));
}
