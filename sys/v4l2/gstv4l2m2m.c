/* GStreamer
 *
 * Copyright (C) 2016 Frédéric Sureau <frederic.sureau@veo-labs.com>
 *
 * gstv4l2m2m.h: base class for V4L2 objects couples
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Library General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. This library is distributed in the hope
 * that it will be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU Library General Public License for more details.
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301,
 * USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "gstv4l2m2m.h"
#include "gstv4l2object.h"
#include "v4l2_calls.h"
#include "gst/allocators/gstdmabuf.h"

GST_DEBUG_CATEGORY_EXTERN (v4l2_debug);
#define GST_CAT_DEFAULT v4l2_debug

GstV4l2M2m *
gst_v4l2_m2m_new (GstElement * parent)
{
  GstV4l2M2m *m2m;
  GstV4l2UpdateFpsFunction update_fps_func = NULL;
  const char *default_device = NULL;

  m2m = (GstV4l2M2m *) g_new0 (GstV4l2M2m, 1);

  m2m->parent = parent;

  m2m->sink_iomode = GST_V4L2_IO_AUTO;
  m2m->source_iomode = GST_V4L2_IO_AUTO;

  m2m->sink_obj = gst_v4l2_object_new (parent, V4L2_BUF_TYPE_VIDEO_OUTPUT,
      default_device, gst_v4l2_get_output, gst_v4l2_set_output,
      update_fps_func);

  m2m->source_obj = gst_v4l2_object_new (parent, V4L2_BUF_TYPE_VIDEO_CAPTURE,
      default_device, gst_v4l2_get_input, gst_v4l2_set_input, update_fps_func);

  m2m->sink_allocator = NULL;
  m2m->source_allocator = NULL;
  m2m->dmabuf_allocator = NULL;

  m2m->sink_obj->use_pool = FALSE;
  m2m->sink_obj->no_initial_format = TRUE;
  m2m->sink_obj->keep_aspect = FALSE;

  m2m->source_obj->use_pool = FALSE;
  m2m->source_obj->no_initial_format = TRUE;
  m2m->source_obj->keep_aspect = FALSE;

  return m2m;
}

void
gst_v4l2_m2m_destroy (GstV4l2M2m * m2m)
{
  g_return_if_fail (m2m != NULL);

  if (m2m->sink_allocator)
    gst_object_unref (m2m->sink_allocator);
  m2m->sink_allocator = NULL;

  if (m2m->source_allocator)
    gst_object_unref (m2m->source_allocator);
  m2m->source_allocator = NULL;

  gst_v4l2_object_destroy (m2m->source_obj);
  gst_v4l2_object_destroy (m2m->sink_obj);

  g_free (m2m);
}

static GstV4l2IOMode
get_io_mode (GstV4l2M2m * m2m, enum GstV4l2M2mBufferType buf_type)
{
  GstV4l2IOMode mode;

  if (buf_type == GST_V4L2_M2M_BUFTYPE_SOURCE)
    mode = m2m->source_iomode;
  else
    mode = m2m->sink_iomode;

  return mode;
}

static gboolean
get_v4l2_memory (GstV4l2M2m * m2m, enum GstV4l2M2mBufferType buf_type,
    enum v4l2_memory *memory)
{
  GstV4l2IOMode mode;

  mode = get_io_mode (m2m, buf_type);

  switch (mode) {
    case GST_V4L2_IO_AUTO:
    case GST_V4L2_IO_RW:
    case GST_V4L2_IO_USERPTR:
    case GST_V4L2_IO_MMAP:
    default:
      GST_DEBUG_OBJECT (m2m->parent, "Unsupported iomode type %d", mode);
      return FALSE;

    case GST_V4L2_IO_DMABUF:
      if (buf_type == GST_V4L2_M2M_BUFTYPE_SINK)
        return FALSE;
      (*memory) = V4L2_MEMORY_MMAP;
      return TRUE;

    case GST_V4L2_IO_DMABUF_IMPORT:
      (*memory) = V4L2_MEMORY_DMABUF;
      return TRUE;
  }
}

static GstV4l2Memory *
get_memory_object_from_buffer (GstV4l2M2m * m2m, GstBuffer * our_buf,
    enum GstV4l2M2mBufferType buf_type)
{
  GstV4l2Memory *our_mem;

  our_mem = (GstV4l2Memory *) gst_buffer_peek_memory (our_buf, 0);

  if (our_mem->mem.allocator == (GstAllocator *) m2m->source_allocator) {
    goto our_buffer;
  }

  else if (our_mem->mem.allocator == (GstAllocator *) m2m->sink_allocator) {
    goto our_buffer;
  }

  else if (our_mem->mem.allocator == m2m->dmabuf_allocator) {
    our_mem = (GstV4l2Memory *)
        gst_mini_object_get_qdata (GST_MINI_OBJECT (our_mem),
        GST_V4L2_MEMORY_QUARK);

    if (our_mem->mem.allocator == (GstAllocator *) m2m->source_allocator) {
      goto our_buffer;
    }

    else if (our_mem->mem.allocator != (GstAllocator *) m2m->sink_allocator) {
      goto our_buffer;
    }

    else {
      goto not_our_buffer;
    }
  }

  else {
    goto not_our_buffer;
  }

our_buffer:
  switch (buf_type) {
    case GST_V4L2_M2M_BUFTYPE_SOURCE:
      if (our_mem->mem.allocator != (GstAllocator *) m2m->source_allocator)
        goto bad_requested_buffer_type;
      else
        return our_mem;

    case GST_V4L2_M2M_BUFTYPE_SINK:
      if (our_mem->mem.allocator != (GstAllocator *) m2m->sink_allocator)
        goto bad_requested_buffer_type;
      else
        return our_mem;

    case GST_V4L2_M2M_BUFTYPE_ANY:
      return our_mem;

    default:
      goto bad_requested_buffer_type;
  }


bad_requested_buffer_type:
  return NULL;

not_our_buffer:
  return NULL;
}

static GstV4l2Allocator *
get_allocator_from_buffer (GstV4l2M2m * m2m, GstBuffer * our_buf,
    GstV4l2IOMode * io_mode)
{
  GstV4l2Memory *our_mem;
  GstV4l2Allocator *allocator;

  our_mem =
      get_memory_object_from_buffer (m2m, our_buf, GST_V4L2_M2M_BUFTYPE_ANY);
  if (our_buf == NULL)
    return NULL;

  allocator = (GstV4l2Allocator *) our_mem->mem.allocator;
  if (allocator == m2m->source_allocator)
    (*io_mode) = m2m->source_iomode;
  else
    (*io_mode) = m2m->sink_iomode;

  return allocator;
}

static GstV4l2Allocator *
get_allocator_from_buftype (GstV4l2M2m * m2m,
    enum GstV4l2M2mBufferType buf_type)
{
  switch (buf_type) {
    case GST_V4L2_M2M_BUFTYPE_SOURCE:
      return m2m->source_allocator;
    case GST_V4L2_M2M_BUFTYPE_SINK:
      return m2m->sink_allocator;
    default:
      return NULL;
  }
}

GstV4l2IOMode
gst_v4l2_m2m_get_sink_iomode (GstV4l2M2m * m2m)
{
  GstV4l2IOMode mode;
  mode = get_io_mode (m2m, GST_V4L2_M2M_BUFTYPE_SINK);
  return mode;
}

GstV4l2IOMode
gst_v4l2_m2m_get_source_iomode (GstV4l2M2m * m2m)
{
  GstV4l2IOMode mode;
  mode = get_io_mode (m2m, GST_V4L2_M2M_BUFTYPE_SOURCE);
  return mode;
}

gboolean
gst_v4l2_m2m_setup (GstV4l2M2m * m2m, GstCaps * source_caps,
    GstCaps * sink_caps)
{
  gboolean ok;
  int ret;
  enum v4l2_memory memory;
  struct v4l2_control control;
  int sink_nbufs, source_nbufs;

  control.id = V4L2_CID_MIN_BUFFERS_FOR_OUTPUT;
  ret = v4l2_ioctl (m2m->sink_obj->video_fd, VIDIOC_G_CTRL, &control);
  if (ret < 0)
    return FALSE;
  sink_nbufs = control.value;

  ok = gst_v4l2_object_set_format (m2m->sink_obj, sink_caps);
  if (!ok)
    return FALSE;

  ok = gst_v4l2_object_set_format (m2m->source_obj, source_caps);
  if (!ok)
    return FALSE;

  ok = get_v4l2_memory (m2m, GST_V4L2_M2M_BUFTYPE_SINK, &memory);
  if (!ok)
    return FALSE;

  m2m->sink_allocator =
      gst_v4l2_allocator_new (GST_OBJECT (m2m->parent), m2m->sink_obj->video_fd,
      &m2m->sink_obj->format);
  ret = gst_v4l2_allocator_start (m2m->sink_allocator, sink_nbufs, memory);
  if (ret < sink_nbufs)
    return FALSE;

  control.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE;
  ret = v4l2_ioctl (m2m->source_obj->video_fd, VIDIOC_G_CTRL, &control);
  if (ret < 0)
    return FALSE;
  source_nbufs = control.value;

  ok = get_v4l2_memory (m2m, GST_V4L2_M2M_BUFTYPE_SOURCE, &memory);
  if (!ok)
    return FALSE;

  m2m->source_allocator =
      gst_v4l2_allocator_new (GST_OBJECT (m2m->parent),
      m2m->source_obj->video_fd, &m2m->source_obj->format);
  ret = gst_v4l2_allocator_start (m2m->source_allocator, source_nbufs, memory);
  if (ret < source_nbufs)
    return FALSE;

  m2m->dmabuf_allocator = gst_dmabuf_allocator_new ();
  if (!m2m->dmabuf_allocator)
    return FALSE;

  ret =
      v4l2_ioctl (m2m->sink_obj->video_fd, VIDIOC_STREAMON,
      &m2m->sink_obj->type);
  if (ret < 0)
    return FALSE;

  ret =
      v4l2_ioctl (m2m->source_obj->video_fd, VIDIOC_STREAMON,
      &m2m->source_obj->type);
  if (ret < 0)
    return FALSE;

  return TRUE;
}

GstVideoInfo *
gst_v4l2_m2m_get_video_info (GstV4l2M2m * m2m,
    enum GstV4l2M2mBufferType buf_type)
{
  if (buf_type == GST_V4L2_M2M_BUFTYPE_SOURCE)
    return &m2m->source_obj->info;
  else
    return &m2m->sink_obj->info;
}

static void
on_buffer_finalization (gpointer m2m_p, GstMiniObject * buf_p)
{
  GstV4l2M2m *m2m = (GstV4l2M2m *) m2m_p;
  GstBuffer *buf = (GstBuffer *) buf_p;
  GstMemory *mem;
  GstV4l2IOMode mode;
  GstV4l2Allocator *allocator;
  GstV4l2MemoryGroup *group;

  allocator = get_allocator_from_buffer (m2m, buf, &mode);
  if (!allocator)
    return;

  if (mode != GST_V4L2_IO_DMABUF_IMPORT)
    return;

  mem = gst_buffer_peek_memory (buf, 0);
  group = ((GstV4l2Memory *) mem)->group;
  gst_v4l2_allocator_reset_group (allocator, group);
}

gboolean
gst_v4l2_m2m_set_background (GstV4l2M2m * m2m, unsigned int background)
{
  struct v4l2_control control = { 0, };

  control.id = V4L2_CID_BG_COLOR;
  control.value = background;
  if (v4l2_ioctl (m2m->source_obj->video_fd, VIDIOC_S_CTRL, &control) < 0)
    return FALSE;

  return TRUE;
}

gboolean
gst_v4l2_m2m_set_selection (GstV4l2M2m * m2m, struct v4l2_rect * crop_bounds,
    struct v4l2_rect * compose_bounds)
{
  struct v4l2_selection sel;
  gboolean ret;

  sel.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  sel.target = V4L2_SEL_TGT_CROP;
  sel.flags = 0;
  sel.r = (*crop_bounds);

  ret = v4l2_ioctl (m2m->sink_obj->video_fd, VIDIOC_S_SELECTION, &sel);
  if (ret < 0)
    return FALSE;

  sel.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  sel.target = V4L2_SEL_TGT_COMPOSE;
  sel.flags = 0;
  sel.r = (*compose_bounds);

  ret = v4l2_ioctl (m2m->source_obj->video_fd, VIDIOC_S_SELECTION, &sel);
  if (ret < 0)
    return FALSE;

  return TRUE;
}

GstBuffer *
gst_v4l2_m2m_alloc_buffer (GstV4l2M2m * m2m, enum GstV4l2M2mBufferType buf_type)
{
  GstV4l2MemoryGroup *group;
  GstV4l2Allocator *allocator;
  GstV4l2IOMode mode;
  GstBuffer *buf;
  GstMemory *mem;

  buf = gst_buffer_new ();
  if (!buf)
    return NULL;

  allocator = get_allocator_from_buftype (m2m, buf_type);
  if (!allocator)
    return NULL;

  mode = get_io_mode (m2m, buf_type);

  switch (mode) {
    case GST_V4L2_IO_DMABUF_IMPORT:
      group = gst_v4l2_allocator_alloc_dmabufin (allocator);
      break;
    case GST_V4L2_IO_DMABUF:
      group =
          gst_v4l2_allocator_alloc_dmabuf (allocator, m2m->dmabuf_allocator);
      break;
    default:
      return NULL;
  }

  if (!(group && (group->n_mem == 1)))
    return NULL;

  mem = group->mem[0];
  gst_buffer_append_memory (buf, mem);

  gst_mini_object_weak_ref ((GstMiniObject *) buf, on_buffer_finalization,
      (gpointer) m2m);

  return buf;
}

gboolean
gst_v4l2_m2m_import_buffer (GstV4l2M2m * m2m, GstBuffer * our_buf,
    GstBuffer * external_buf)
{
  gboolean ok;
  GstV4l2MemoryGroup *group;
  GstMemory *external_mem;
  GstV4l2Memory *our_mem;
  GstV4l2IOMode mode;
  GstV4l2Allocator *allocator;

  allocator = get_allocator_from_buffer (m2m, our_buf, &mode);
  if (!allocator)
    return FALSE;

  if (mode != GST_V4L2_IO_DMABUF_IMPORT)
    return FALSE;

  external_mem = gst_buffer_peek_memory (external_buf, 0);

  our_mem = (GstV4l2Memory *) gst_buffer_peek_memory (our_buf, 0);
  group = our_mem->group;

  ok = gst_v4l2_allocator_import_dmabuf (allocator, group, 1, &external_mem);
  if (!ok)
    return FALSE;

  return TRUE;
}

void
gst_v4l2_m2m_set_sink_iomode (GstV4l2M2m * m2m, GstV4l2IOMode mode)
{
  m2m->sink_iomode = mode;
  m2m->sink_obj->req_mode = mode;
}

void
gst_v4l2_m2m_set_source_iomode (GstV4l2M2m * m2m, GstV4l2IOMode mode)
{
  m2m->source_iomode = mode;
  m2m->source_obj->req_mode = mode;
}

void
gst_v4l2_m2m_set_video_device (GstV4l2M2m * m2m, char *videodev)
{
  m2m->source_obj->videodev = g_strdup (videodev);
  m2m->sink_obj->videodev = g_strdup (videodev);
}

gboolean
gst_v4l2_m2m_process (GstV4l2M2m * m2m, GstBuffer * srcbuf, GstBuffer * sinkbuf)
{
  gboolean ok;
  GstV4l2Memory *srcmem;
  GstV4l2Memory *sinkmem;

  srcmem =
      get_memory_object_from_buffer (m2m, srcbuf, GST_V4L2_M2M_BUFTYPE_SOURCE);
  if (!srcmem)
    return FALSE;

  sinkmem =
      get_memory_object_from_buffer (m2m, sinkbuf, GST_V4L2_M2M_BUFTYPE_SINK);
  if (!sinkmem)
    return FALSE;

  ok = gst_v4l2_allocator_qbuf (m2m->sink_allocator, sinkmem->group);
  if (!ok)
    return FALSE;

  ok = gst_v4l2_allocator_qbuf (m2m->source_allocator, srcmem->group);
  if (!ok)
    return FALSE;

  return TRUE;
}

gboolean
gst_v4l2_m2m_wait (GstV4l2M2m * m2m)
{
  GstFlowReturn flow;
  GstV4l2MemoryGroup *srcgroup;
  GstV4l2MemoryGroup *sinkgroup;

  flow = gst_v4l2_allocator_dqbuf (m2m->sink_allocator, &sinkgroup);
  if (flow != GST_FLOW_OK)
    return FALSE;

  flow = gst_v4l2_allocator_dqbuf (m2m->source_allocator, &srcgroup);
  if (flow != GST_FLOW_OK)
    return FALSE;

  if (sinkgroup->n_mem != 1)
    return FALSE;

  if (srcgroup->n_mem != 1)
    return FALSE;

  return TRUE;
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

void
gst_v4l2_m2m_close (GstV4l2M2m * m2m)
{
  if (m2m->dmabuf_allocator)
    gst_object_unref (m2m->dmabuf_allocator);
  m2m->dmabuf_allocator = NULL;

  gst_v4l2_object_close (m2m->sink_obj);
  gst_v4l2_object_close (m2m->source_obj);
}

void
gst_v4l2_m2m_unlock (GstV4l2M2m * m2m)
{
  gst_v4l2_object_unlock (m2m->sink_obj);
  gst_v4l2_object_unlock (m2m->source_obj);
}

void
gst_v4l2_m2m_unlock_stop (GstV4l2M2m * m2m)
{
  gst_v4l2_object_unlock_stop (m2m->sink_obj);
  gst_v4l2_object_unlock_stop (m2m->source_obj);
}

void
gst_v4l2_m2m_stop (GstV4l2M2m * m2m)
{
  gst_v4l2_object_stop (m2m->sink_obj);
  v4l2_ioctl (m2m->sink_obj->video_fd, VIDIOC_STREAMOFF, &m2m->sink_obj->type);
  if (m2m->sink_allocator) {
    gst_v4l2_allocator_flush (m2m->sink_allocator);
    gst_v4l2_allocator_stop (m2m->sink_allocator);
  }

  gst_v4l2_object_stop (m2m->source_obj);
  v4l2_ioctl (m2m->source_obj->video_fd, VIDIOC_STREAMOFF,
      &m2m->source_obj->type);
  if (m2m->source_allocator) {
    gst_v4l2_allocator_flush (m2m->source_allocator);
    gst_v4l2_allocator_stop (m2m->source_allocator);
  }
}
