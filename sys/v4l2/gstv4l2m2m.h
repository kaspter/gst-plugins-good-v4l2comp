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

typedef struct _GstV4l2M2m GstV4l2M2m;

#define GST_V4L2_M2M(obj) (GstV4l2M2m *)(obj)

struct _GstV4l2M2m {
  GstObject * gstobject;
  
  GstV4l2Object * v4l2output;
  GstV4l2Object * v4l2capture;
};

GType gst_v4l2_m2m_get_type (void);

/* create/destroy */
GstV4l2M2m*  gst_v4l2_m2m_new       (GstElement * element,
                                     const char * default_device,
                                     GstV4l2UpdateFpsFunction update_fps_func);

void         gst_v4l2_m2m_destroy   (GstV4l2M2m * m2m);

/* properties */

gboolean     gst_v4l2_m2m_set_property_helper       (GstV4l2M2m * m2m,
                                                        guint prop_id,
                                                        const GValue * value,
                                                        GParamSpec * pspec);
gboolean     gst_v4l2_m2m_get_property_helper       (GstV4l2M2m * m2m,
                                                        guint prop_id, GValue * value,
                                                        GParamSpec * pspec);
/* open/close */
gboolean     gst_v4l2_m2m_open           (GstV4l2M2m * m2m);
void         gst_v4l2_m2m_close          (GstV4l2M2m * m2m);

void         gst_v4l2_m2m_unlock         (GstV4l2M2m * m2m);
void         gst_v4l2_m2m_unlock_stop    (GstV4l2M2m * m2m);

void         gst_v4l2_m2m_stop           (GstV4l2M2m * m2m);

#endif /* __GST_V4L2_M2M_H__ */
