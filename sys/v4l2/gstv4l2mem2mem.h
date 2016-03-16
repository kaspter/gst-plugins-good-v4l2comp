/* GStreamer
 *
 * Copyright (C) 2016 Frédéric Sureau <frederic.sureau@veo-labs.com>
 *
 * gstv4l2mem2mem.h: base class for V4L2 objects couples
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

#ifndef __GST_V4L2_MEM2MEM_H__
#define __GST_V4L2_MEM2MEM_H__

#include <gst/gst.h>
#include <gstv4l2object.h>
#include <gstv4l2allocator.h>

typedef struct _GstV4l2Mem2Mem GstV4l2Mem2Mem;

#define GST_V4L2_MEM2MEM(obj) (GstV4l2Mem2Mem *)(obj)

struct _GstV4l2Mem2Mem {
  GstObject * gstobject;

  GstV4l2Object * output_object;
  GstV4l2Object * capture_object;
  GstV4l2Allocator * output_allocator;
  GstV4l2Allocator * capture_allocator;
};

GType gst_v4l2_mem2mem_get_type (void);

/* create/destroy */
GstV4l2Mem2Mem*  gst_v4l2_mem2mem_new       (GstElement * element,
											 const char * default_device,
											 GstV4l2UpdateFpsFunction update_fps_func);

void         gst_v4l2_mem2mem_destroy   (GstV4l2Mem2Mem * mem2mem);

/* properties */

gboolean     gst_v4l2_mem2mem_set_property_helper       (GstV4l2Mem2Mem * mem2mem,
                                                        guint prop_id,
                                                        const GValue * value,
                                                        GParamSpec * pspec);
gboolean     gst_v4l2_mem2mem_get_property_helper       (GstV4l2Mem2Mem * mem2mem,
                                                        guint prop_id, GValue * value,
                                                        GParamSpec * pspec);
/* open/close */
gboolean     gst_v4l2_mem2mem_open           (GstV4l2Mem2Mem * mem2mem);
void         gst_v4l2_mem2mem_close          (GstV4l2Mem2Mem * mem2mem);

void         gst_v4l2_mem2mem_unlock         (GstV4l2Mem2Mem * mem2mem);
void         gst_v4l2_mem2mem_unlock_stop    (GstV4l2Mem2Mem * mem2mem);

void         gst_v4l2_mem2mem_stop           (GstV4l2Mem2Mem * mem2mem);


/* specific */

gboolean     gst_v4l2_mem2mem_setup_allocator (GstV4l2Mem2Mem * mem2mem, GstCaps * caps, int output_nbufs, int capture_nbufs);

GstMemory *  gst_v4l2_mem2mem_alloc (GstV4l2Mem2Mem * mem2mem, gboolean capture_buf);

gboolean     gst_v4l2_mem2mem_process (GstV4l2Mem2Mem * mem2mem, GstMemory * mdst, GstMemory * msrc);

gboolean     gst_v4l2_mem2mem_copy(GstV4l2Mem2Mem * mem2mem, GstMemory * mdst, GstMemory * msrc);

gboolean     gst_v4l2_mem2mem_set_selection (GstV4l2Mem2Mem * mem2mem, struct v4l2_rect * drect, struct v4l2_rect * srect);

#endif /* __GST_V4L2_MEM2MEM_H__ */
