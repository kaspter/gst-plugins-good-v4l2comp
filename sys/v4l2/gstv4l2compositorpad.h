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

#ifndef __GST_V4L2_COMPOSITOR_PAD_H__
#define __GST_V4L2_COMPOSITOR_PAD_H__

#include <gst/gst.h>
#include <gst/video/video.h>

G_BEGIN_DECLS
#define GST_TYPE_V4L2_COMPOSITOR_PAD (gst_v4l2_compositor_pad_get_type())
#define GST_V4L2_COMPOSITOR_PAD(obj) \
        (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_V4L2_COMPOSITOR_PAD, GstV4l2CompositorPad))
#define GST_V4L2_COMPOSITOR_PAD_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_V4L2_COMPOSITOR_PAD, GstV4l2CompositorPadClass))
#define GST_IS_V4L2_COMPOSITOR_PAD(obj) \
        (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_V4L2_COMPOSITOR_PAD))
#define GST_IS_V4L2_COMPOSITOR_PAD_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_V4L2_COMPOSITOR_PAD))
typedef struct _GstV4l2CompositorPad GstV4l2CompositorPad;
typedef struct _GstV4l2CompositorPadClass GstV4l2CompositorPadClass;
typedef struct _GstV4l2CompositorJob GstV4l2CompositorJob;

struct _GstV4l2Compositor;
typedef struct _GstV4l2Compositor GstV4l2Compositor;

enum _GstV4l2CompositorJobState
{
  GST_V4L2_COMPOSITOR_JOB_READY       =  0,
  GST_V4L2_COMPOSITOR_JOB_PREPARED    =  1,
  GST_V4L2_COMPOSITOR_JOB_QUEUED      =  2,
  GST_V4L2_COMPOSITOR_JOB_GONE        =  3,
  GST_V4L2_COMPOSITOR_JOB_BACK        =  4,
  GST_V4L2_COMPOSITOR_JOB_FLUSHED     =  5,
  GST_V4L2_COMPOSITOR_JOB_CLEANUP     =  6
};
typedef enum _GstV4l2CompositorJobState GstV4l2CompositorJobState;

struct _GstV4l2CompositorJob
{
  GstV4l2Compositor *parent;
  GstV4l2CompositorJob *master_job;
  GstV4l2CompositorPad *cpad;
  GstBuffer *sink_buf;
  GstBuffer *external_sink_buf;
  GstBuffer *source_buf;
  GstV4l2CompositorJobState state;
};

/**
 * GstV4l2CompositorPad:
 *
 * The opaque #GstV4l2CompositorPad structure.
 */
struct _GstV4l2CompositorPad
{
  GstV4l2VideoAggregatorPad parent;
  GstV4l2M2m *m2m;
  GList *jobs;
  GList *prepared_jobs;
  GList *queued_jobs;
  int index;

  /* properties */
  gint xpos, ypos;
  gint width, height;
  gchar *videodev;
};

struct _GstV4l2CompositorPadClass
{
  GstV4l2VideoAggregatorPadClass parent_class;
};

GType gst_v4l2_compositor_pad_get_type (void);

G_END_DECLS
#endif /* __GST_V4L2_COMPOSITOR_PAD_H__ */