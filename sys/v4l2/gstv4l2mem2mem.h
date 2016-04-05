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

#define GST_TYPE_V4L2_MEM2MEM      (gst_v4l2_mem2mem_get_type())

struct _GstV4l2Mem2Mem {
  GstElement * parent;
  GstV4l2Object * output_object;
  GstV4l2Object * capture_object;
  GstV4l2Allocator * output_allocator;
  GstV4l2Allocator * capture_allocator;
  GstAllocator * dmabuf_allocator;
  GstV4l2IOMode output_io_mode;
  GstV4l2IOMode capture_io_mode;
};

/* create/destroy */
GstV4l2Mem2Mem*  gst_v4l2_mem2mem_new       (GstElement * element,
											 const char * default_device,
											 GstV4l2UpdateFpsFunction update_fps_func);

void         gst_v4l2_mem2mem_destroy   (GstV4l2Mem2Mem * mem2mem);

/* properties */

void         gst_v4l2_mem2mem_set_output_io_mode (GstV4l2Mem2Mem * mem2mem, GstV4l2IOMode mode);
void         gst_v4l2_mem2mem_set_capture_io_mode (GstV4l2Mem2Mem * mem2mem, GstV4l2IOMode mode);
void         gst_v4l2_mem2mem_set_video_device (GstV4l2Mem2Mem * mem2mem, char * videodev);

/* open/close */
gboolean     gst_v4l2_mem2mem_open           (GstV4l2Mem2Mem * mem2mem);
void         gst_v4l2_mem2mem_close          (GstV4l2Mem2Mem * mem2mem);

void         gst_v4l2_mem2mem_unlock         (GstV4l2Mem2Mem * mem2mem);
void         gst_v4l2_mem2mem_unlock_stop    (GstV4l2Mem2Mem * mem2mem);

void         gst_v4l2_mem2mem_stop           (GstV4l2Mem2Mem * mem2mem);


/* specific */

gboolean     gst_v4l2_mem2mem_setup_allocator (GstV4l2Mem2Mem * mem2mem, GstCaps * caps, int output_nbufs, int capture_nbufs);

GstBuffer *  gst_v4l2_mem2mem_alloc (GstV4l2Mem2Mem * mem2mem, gboolean capture_buf);

void         gst_v4l2_mem2mem_free (GstV4l2Mem2Mem * mem2mem, gboolean capture_buf, GstBuffer * sbuf);

gboolean     gst_v4l2_mem2mem_process (GstV4l2Mem2Mem * mem2mem, GstBuffer * dbuf, GstBuffer * sbuf);

gboolean     gst_v4l2_mem2mem_copy_or_import_source(GstV4l2Mem2Mem * mem2mem, GstBuffer * dbuf, GstBuffer * sbuf);

gboolean     gst_v4l2_mem2mem_set_selection (GstV4l2Mem2Mem * mem2mem, struct v4l2_rect * drect, struct v4l2_rect * srect);

#endif /* __GST_V4L2_MEM2MEM_H__ */
