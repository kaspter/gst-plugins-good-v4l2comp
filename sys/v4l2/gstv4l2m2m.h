/* GStreamer
 *
 * Copyright (C) 2016 Frédéric Sureau <frederic.sureau@veo-labs.com>
 *
 * gstv4l2m2m.h: base class for V4L2 objects couples
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

#ifndef __GST_V4L2_M2M_H__
#define __GST_V4L2_M2M_H__

#include <gst/gst.h>
#include <gstv4l2object.h>
#include <gstv4l2allocator.h>

typedef struct _GstV4l2M2m GstV4l2M2m;

#define GST_V4L2_M2M(obj) (GstV4l2M2m *)(obj)

#define GST_TYPE_V4L2_M2M      (gst_v4l2_m2m_get_type())

struct _GstV4l2M2m
{
  GstElement *parent;
  GstV4l2Object *source_obj;
  GstV4l2Object *sink_obj;
  GstV4l2Allocator *source_allocator;
  GstV4l2Allocator *sink_allocator;
  GstAllocator *dmabuf_allocator;
  GstV4l2IOMode source_iomode;
  GstV4l2IOMode sink_iomode;
};

enum GstV4l2M2mBufferType
{
  GST_V4L2_M2M_BUFTYPE_SINK,
  GST_V4L2_M2M_BUFTYPE_SOURCE,
  GST_V4L2_M2M_BUFTYPE_ANY,
};

/* create/destroy */
GstV4l2M2m *gst_v4l2_m2m_new (GstElement * parent);

void gst_v4l2_m2m_destroy (GstV4l2M2m * m2m);

/* properties */
void gst_v4l2_m2m_set_source_iomode (GstV4l2M2m * m2m, GstV4l2IOMode mode);
void gst_v4l2_m2m_set_sink_iomode (GstV4l2M2m * m2m, GstV4l2IOMode mode);
void gst_v4l2_m2m_set_video_device (GstV4l2M2m * m2m, char *videodev);

GstV4l2IOMode gst_v4l2_m2m_get_sink_iomode (GstV4l2M2m * m2m);
GstV4l2IOMode gst_v4l2_m2m_get_source_iomode (GstV4l2M2m * m2m);

/* open/close */
gboolean gst_v4l2_m2m_open (GstV4l2M2m * m2m);
void gst_v4l2_m2m_close (GstV4l2M2m * m2m);
void gst_v4l2_m2m_unlock (GstV4l2M2m * m2m);
void gst_v4l2_m2m_unlock_stop (GstV4l2M2m * m2m);
void gst_v4l2_m2m_stop (GstV4l2M2m * m2m);


/* specific operations */

gboolean gst_v4l2_m2m_setup (GstV4l2M2m * m2m, GstCaps * src_caps,
    GstCaps * sink_caps);

GstVideoInfo *gst_v4l2_m2m_get_video_info (GstV4l2M2m * m2m,
    enum GstV4l2M2mBufferType buf_type);

GstBuffer *gst_v4l2_m2m_alloc_buffer (GstV4l2M2m * m2m,
    enum GstV4l2M2mBufferType buf_type);

gboolean gst_v4l2_m2m_process (GstV4l2M2m * m2m, GstBuffer * source_buf,
    GstBuffer * sink_buf);

gboolean gst_v4l2_m2m_wait (GstV4l2M2m * m2m);

gboolean gst_v4l2_m2m_import_buffer (GstV4l2M2m * m2m, GstBuffer * our_buf,
    GstBuffer * external_buf);

gboolean gst_v4l2_m2m_set_selection (GstV4l2M2m * m2m,
    struct v4l2_rect *source_rect, struct v4l2_rect *sink_rect);

#endif /* __GST_V4L2_M2M_H__ */
