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
gst_v4l2_m2m_new (GstElement * element,
    const char *default_device, GstV4l2UpdateFpsFunction update_fps_func)
{
  GstV4l2M2m *m2m;

  m2m = (GstV4l2M2m *) g_new0 (GstV4l2M2m, 1);

  m2m->parent = element;

  m2m->sink_iomode = GST_V4L2_IO_AUTO;
  m2m->source_iomode = GST_V4L2_IO_AUTO;

  m2m->sink_obj = gst_v4l2_object_new (element, V4L2_BUF_TYPE_VIDEO_OUTPUT,
      default_device, gst_v4l2_get_output, gst_v4l2_set_output,
      update_fps_func);

  m2m->source_obj = gst_v4l2_object_new (element, V4L2_BUF_TYPE_VIDEO_CAPTURE,
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

  if (mode == GST_V4L2_IO_AUTO)
    return GST_V4L2_IO_MMAP;
  else
    return mode;
}

static GstV4l2Allocator *
get_allocator (GstV4l2M2m * m2m, enum GstV4l2M2mBufferType buf_type)
{
  if (buf_type == GST_V4L2_M2M_BUFTYPE_SOURCE) {
    return m2m->source_allocator;
  } else {
    return m2m->sink_allocator;
  }
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
    default:
      return FALSE;

    case GST_V4L2_IO_DMABUF:
      if (buf_type == GST_V4L2_M2M_BUFTYPE_SINK)
        return FALSE;
      (*memory) = V4L2_MEMORY_MMAP;
      return TRUE;

    case GST_V4L2_IO_DMABUF_IMPORT:
      if (buf_type == GST_V4L2_M2M_BUFTYPE_SOURCE)
        return FALSE;
      (*memory) = V4L2_MEMORY_DMABUF;
      return TRUE;

    case GST_V4L2_IO_MMAP:
      (*memory) = V4L2_MEMORY_MMAP;
      return TRUE;
  }
}


gboolean
gst_v4l2_m2m_setup (GstV4l2M2m * m2m, GstCaps * source_caps, int source_nbufs,
    GstCaps * sink_caps, int sink_nbufs)
{
  gboolean ok;
  int ret;
  enum v4l2_memory memory;

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


gboolean
gst_v4l2_m2m_set_selection (GstV4l2M2m * m2m, struct v4l2_rect * drect,
    struct v4l2_rect * srect)
{
  struct v4l2_selection sel;
  gboolean ret;

  sel.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  sel.target = V4L2_SEL_TGT_CROP;
  sel.flags = 0;
  sel.r = (*srect);

  ret = v4l2_ioctl (m2m->sink_obj->video_fd, VIDIOC_S_SELECTION, &sel);
  if (ret < 0)
    return FALSE;

  sel.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  sel.target = V4L2_SEL_TGT_COMPOSE;
  sel.flags = 0;
  sel.r = (*drect);

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

  allocator = get_allocator (m2m, buf_type);
  mode = get_io_mode (m2m, buf_type);

  switch (mode) {
    case GST_V4L2_IO_MMAP:
      group = gst_v4l2_allocator_alloc_mmap (allocator);
      break;
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

  return buf;
}




void
gst_v4l2_m2m_free_buffer (GstV4l2M2m * m2m, enum GstV4l2M2mBufferType buf_type,
    GstBuffer * buf)
{
  GstMemory *mem;
  GstV4l2IOMode mode;
  GstV4l2Allocator *allocator;
  GstV4l2MemoryGroup *group;

  mem = gst_buffer_peek_memory (buf, 0);

  allocator = get_allocator (m2m, buf_type);
  mode = get_io_mode (m2m, buf_type);

  switch (mode) {
    case GST_V4L2_IO_DMABUF_IMPORT:
      group = ((GstV4l2Memory *) mem)->group;
      gst_v4l2_allocator_reset_group (allocator, group);
      break;
    default:
      break;
  }

  gst_buffer_unref (buf);
}



static gboolean
copy (GstV4l2M2m * m2m, GstBuffer * dbuf, GstBuffer * sbuf)
{
  GstMapInfo idst;
  GstMapInfo isrc;
  gboolean ok;
  GstMemory *dmem;
  GstMemory *smem;

  smem = gst_buffer_peek_memory (sbuf, 0);
  dmem = gst_buffer_peek_memory (dbuf, 0);

  if (!gst_is_v4l2_memory (dmem))
    return FALSE;

  ok = gst_memory_map (dmem, &idst, GST_MAP_WRITE);
  if (!ok)
    goto map_dmem_failed;

  ok = gst_memory_map (smem, &isrc, GST_MAP_READ);
  if (!ok)
    goto map_smem_failed;

  if (idst.size != isrc.size)
    goto size_check_failed;

  memcpy ((guint8 *) idst.data, (guint8 *) isrc.data, idst.size);

  gst_memory_unmap (smem, &isrc);
  gst_memory_unmap (dmem, &idst);
  return TRUE;

size_check_failed:
  gst_memory_unmap (smem, &isrc);

map_smem_failed:
  gst_memory_unmap (dmem, &idst);

map_dmem_failed:
  return FALSE;
}


static gboolean
import (GstV4l2M2m * m2m, GstBuffer * dbuf, GstBuffer * sbuf)
{
  gboolean ok;
  GstV4l2MemoryGroup *group;
  GstMemory *smem;
  GstV4l2Memory *dmem;

  smem = gst_buffer_peek_memory (sbuf, 0);

  dmem = (GstV4l2Memory *) gst_buffer_peek_memory (dbuf, 0);
  group = dmem->group;

  ok = gst_v4l2_allocator_import_dmabuf (m2m->sink_allocator, group, 1, &smem);
  if (!ok)
    return FALSE;


  return TRUE;
}








gboolean
gst_v4l2_m2m_copy_or_import_sink_buffer (GstV4l2M2m * m2m, GstBuffer * dbuf,
    GstBuffer * sbuf)
{
  GstV4l2IOMode mode;

  mode = get_io_mode (m2m, GST_V4L2_M2M_BUFTYPE_SINK);

  switch (mode) {
    case GST_V4L2_IO_MMAP:
      return copy (m2m, dbuf, sbuf);
    case GST_V4L2_IO_DMABUF_IMPORT:
      return import (m2m, dbuf, sbuf);
    default:
      return FALSE;
  }
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
gst_v4l2_m2m_process (GstV4l2M2m * m2m, GstBuffer * dbuf, GstBuffer * sbuf)
{
  gboolean ok;
  GstMemory *dmem_b;
  GstMemory *smem_b;
  GstV4l2Memory *dmem;
  GstV4l2Memory *smem;
  GstFlowReturn flow;
  GstV4l2MemoryGroup *dmgroup;
  GstV4l2MemoryGroup *smgroup;

  smem_b = gst_buffer_peek_memory (sbuf, 0);
  if (!gst_is_v4l2_memory (smem_b))
    return FALSE;
  smem = (GstV4l2Memory *) smem_b;

  dmem_b = gst_buffer_peek_memory (dbuf, 0);
  if (dmem_b->allocator == m2m->dmabuf_allocator) {
    dmem =
        (GstV4l2Memory *) gst_mini_object_get_qdata (GST_MINI_OBJECT (dmem_b),
        GST_V4L2_MEMORY_QUARK);
    if (dmem->mem.allocator != (GstAllocator *) m2m->source_allocator)
      return FALSE;
  }

  else if (!gst_is_v4l2_memory (smem_b))
    return FALSE;

  else
    dmem = (GstV4l2Memory *) dmem_b;

  ok = gst_v4l2_allocator_qbuf (m2m->sink_allocator, smem->group);
  if (!ok)
    return FALSE;

  ok = gst_v4l2_allocator_qbuf (m2m->source_allocator, dmem->group);
  if (!ok)
    return FALSE;

  flow = gst_v4l2_allocator_dqbuf (m2m->sink_allocator, &smgroup);
  if (flow != GST_FLOW_OK)
    return FALSE;

  flow = gst_v4l2_allocator_dqbuf (m2m->source_allocator, &dmgroup);
  if (flow != GST_FLOW_OK)
    return FALSE;

  if (smgroup->n_mem != 1)
    return FALSE;

  if (smgroup->mem[0] != smem_b)
    return FALSE;

  if (dmgroup->n_mem != 1)
    return FALSE;

  if (dmgroup->mem[0] != dmem_b)
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
  gst_v4l2_object_stop (m2m->source_obj);
}
