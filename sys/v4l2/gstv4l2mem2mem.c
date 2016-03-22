/* GStreamer
 *
 * Copyright (C) 2016 Frédéric Sureau <frederic.sureau@veo-labs.com>
 *
 * gstv4l2mem2mem.h: base class for V4L2 objects couples
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

#include "gstv4l2mem2mem.h"
#include "gstv4l2object.h"
#include "v4l2_calls.h"

GST_DEBUG_CATEGORY_EXTERN (v4l2_debug);
#define GST_CAT_DEFAULT v4l2_debug

GstV4l2Mem2Mem *
gst_v4l2_mem2mem_new (GstElement * element,
    const char *default_device,
    GstV4l2UpdateFpsFunction update_fps_func)
{
  GstV4l2Mem2Mem * mem2mem;

  mem2mem = (GstV4l2Mem2Mem *) g_new0 (GstV4l2Mem2Mem, 1);

  mem2mem->parent = element;

  mem2mem->output_io_mode = GST_V4L2_IO_AUTO;
  mem2mem->capture_io_mode = GST_V4L2_IO_AUTO;

  mem2mem->output_object = gst_v4l2_object_new(element, V4L2_BUF_TYPE_VIDEO_OUTPUT,
      default_device, gst_v4l2_get_output, gst_v4l2_set_output, update_fps_func);

  mem2mem->capture_object = gst_v4l2_object_new(element, V4L2_BUF_TYPE_VIDEO_CAPTURE,
      default_device, gst_v4l2_get_input, gst_v4l2_set_input, update_fps_func);

  mem2mem->output_allocator = NULL;
  mem2mem->capture_allocator = NULL;

  mem2mem->output_object->use_pool = FALSE;
  mem2mem->output_object->no_initial_format = TRUE;
  mem2mem->output_object->keep_aspect = FALSE;

  mem2mem->capture_object->use_pool = FALSE;
  mem2mem->capture_object->no_initial_format = TRUE;
  mem2mem->capture_object->keep_aspect = FALSE;

  return mem2mem;
}

void
gst_v4l2_mem2mem_destroy (GstV4l2Mem2Mem * mem2mem)
{
  g_return_if_fail (mem2mem != NULL);

  gst_v4l2_object_destroy (mem2mem->capture_object);
  gst_v4l2_object_destroy (mem2mem->output_object);

  g_free (mem2mem);
}


static GstV4l2IOMode
get_io_mode (GstV4l2Mem2Mem * mem2mem, gboolean capture)
{
  GstV4l2IOMode mode;

  if (capture)
    mode = mem2mem->capture_io_mode;
  else
    mode = mem2mem->output_io_mode;

  if (mode == GST_V4L2_IO_AUTO)
    return GST_V4L2_IO_MMAP;
  else
    return mode;
}


static gboolean
get_v4l2_memory(GstV4l2Mem2Mem * mem2mem, gboolean capture, enum v4l2_memory * memory)
{
  GstV4l2IOMode mode;

  mode = get_io_mode(mem2mem, capture);

  switch(mode)
    {
    case GST_V4L2_IO_AUTO:
    case GST_V4L2_IO_RW:
    case GST_V4L2_IO_USERPTR:
    default:
      return FALSE;

    case GST_V4L2_IO_DMABUF:
    case GST_V4L2_IO_DMABUF_IMPORT:
      (*memory) = V4L2_MEMORY_DMABUF;
      return TRUE;

    case GST_V4L2_IO_MMAP:
      (*memory) = V4L2_MEMORY_MMAP;
      return TRUE;
    }
}


gboolean
gst_v4l2_mem2mem_setup_allocator (GstV4l2Mem2Mem * mem2mem, GstCaps * caps, int output_nbufs, int capture_nbufs)
{
  gboolean ok;
  int ret;
  enum v4l2_memory memory;

  ok = gst_v4l2_object_set_format (mem2mem->output_object, caps);
  if (!ok)
	return FALSE;

  ok = gst_v4l2_object_set_format (mem2mem->capture_object, caps);
  if (!ok)
	return FALSE;

  ok = get_v4l2_memory (mem2mem, FALSE, &memory);
  if (!ok)
	return FALSE;

  mem2mem->output_allocator = gst_v4l2_allocator_new(GST_OBJECT(mem2mem->parent), mem2mem->output_object->video_fd, &mem2mem->output_object->format);
  ret = gst_v4l2_allocator_start (mem2mem->output_allocator, output_nbufs, memory);
  if (ret != output_nbufs)
	return FALSE;

  ok = get_v4l2_memory (mem2mem, TRUE, &memory);
  if (!ok)
	return FALSE;

  mem2mem->capture_allocator = gst_v4l2_allocator_new(GST_OBJECT (mem2mem->parent), mem2mem->capture_object->video_fd, &mem2mem->capture_object->format);
  ret = gst_v4l2_allocator_start (mem2mem->capture_allocator, capture_nbufs, memory);
  if (ret != capture_nbufs)
	return FALSE;

  ret = v4l2_ioctl (mem2mem->output_object->video_fd, VIDIOC_STREAMON, &mem2mem->output_object->type);
  if (ret < 0)
	return FALSE;

  ret = v4l2_ioctl (mem2mem->capture_object->video_fd, VIDIOC_STREAMON, &mem2mem->capture_object->type);
  if (ret < 0)
	return FALSE;

  return TRUE;
}

gboolean
gst_v4l2_mem2mem_set_selection (GstV4l2Mem2Mem * mem2mem, struct v4l2_rect * drect, struct v4l2_rect * srect)
{
  struct v4l2_selection sel;
  gboolean ret;

  sel.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  sel.target = V4L2_SEL_TGT_CROP;
  sel.flags = 0;
  sel.r = (*srect);

  ret = v4l2_ioctl (mem2mem->output_object->video_fd, VIDIOC_S_SELECTION, &sel);
  if (ret < 0)
	return FALSE;

  sel.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  sel.target = V4L2_SEL_TGT_COMPOSE;
  sel.flags = 0;
  sel.r = (*drect);

  ret = v4l2_ioctl (mem2mem->capture_object->video_fd, VIDIOC_S_SELECTION, &sel);
  if (ret < 0)
	return FALSE;

  return TRUE;
}


GstBuffer *
gst_v4l2_mem2mem_alloc (GstV4l2Mem2Mem * mem2mem, gboolean capture)
{
  GstV4l2MemoryGroup * group;
  GstV4l2Allocator * allocator;
  GstV4l2IOMode mode;
  GstBuffer * buf;
  GstMemory * mem;

  buf = gst_buffer_new();
  if (!buf)
    return NULL;

  if (capture) {
    allocator = mem2mem->capture_allocator;
  }
  else {
    allocator = mem2mem->output_allocator;
  }

  mode = get_io_mode(mem2mem, capture);

  switch (mode)
    {
    case GST_V4L2_IO_MMAP:
      group = gst_v4l2_allocator_alloc_mmap (allocator);
      break;
    case GST_V4L2_IO_DMABUF_IMPORT:
      group = gst_v4l2_allocator_alloc_dmabufin (allocator);
      break;
    default:
      return NULL;
    }

  if (!(group && (group->n_mem==1)))
	return NULL;

  mem = group->mem[0];

  gst_buffer_append_memory (buf, mem);

  return buf;
}



static gboolean
copy(GstV4l2Mem2Mem * mem2mem, GstBuffer * dbuf, GstBuffer * sbuf)
{
  GstMapInfo idst;
  GstMapInfo isrc;
  gboolean ok;
  GstMemory * dmem;
  GstMemory * smem;

  smem = gst_buffer_peek_memory (sbuf, 0);

  dmem = gst_buffer_peek_memory (dbuf, 0);

  if (! gst_is_v4l2_memory (dmem))
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
import(GstV4l2Mem2Mem * mem2mem, GstBuffer * dbuf, GstBuffer * sbuf)
{
  gboolean ok;
  GstV4l2MemoryGroup *group;
  GstMemory * smem;
  GstV4l2Memory * dmem;

  smem = gst_buffer_peek_memory (sbuf, 0);

  dmem = (GstV4l2Memory *)gst_buffer_peek_memory (dbuf, 0);
  group=dmem->group;

  ok = gst_v4l2_allocator_import_dmabuf (mem2mem->output_allocator, group, 1, &smem);
  if (! ok)
    return FALSE;


  return TRUE;
}








gboolean
gst_v4l2_mem2mem_copy_or_import_source (GstV4l2Mem2Mem * mem2mem, GstBuffer * dbuf, GstBuffer * sbuf)
{
  GstV4l2IOMode mode;

  mode = get_io_mode(mem2mem, FALSE);

  switch (mode)
    {
    case GST_V4L2_IO_MMAP:
      return copy(mem2mem, dbuf, sbuf);
    case GST_V4L2_IO_DMABUF_IMPORT:
      return import(mem2mem, dbuf, sbuf);
    default:
      return FALSE;
    }
}




void
gst_v4l2_mem2mem_set_output_io_mode (GstV4l2Mem2Mem * mem2mem, GstV4l2IOMode mode)
{
  mem2mem->output_io_mode = mode;
  mem2mem->output_object->req_mode = mode;
}

void
gst_v4l2_mem2mem_set_capture_io_mode (GstV4l2Mem2Mem * mem2mem, GstV4l2IOMode mode)
{
  mem2mem->capture_io_mode = mode;
  mem2mem->capture_object->req_mode = mode;
}

void
gst_v4l2_mem2mem_set_video_device (GstV4l2Mem2Mem * mem2mem, char * videodev)
{
  mem2mem->capture_object->videodev = g_strdup (videodev);
  mem2mem->output_object->videodev = g_strdup (videodev);
}


gboolean
gst_v4l2_mem2mem_process (GstV4l2Mem2Mem * mem2mem, GstBuffer * dbuf, GstBuffer * sbuf)
{
  gboolean ok;
  GstMemory * dmem_b;
  GstMemory * smem_b;
  GstV4l2Memory * dmem;
  GstV4l2Memory * smem;
  GstFlowReturn flow;
  GstV4l2MemoryGroup * dmgroup;
  GstV4l2MemoryGroup * smgroup;

  smem_b = gst_buffer_peek_memory (sbuf, 0);

  dmem_b = gst_buffer_peek_memory (dbuf, 0);

  if (! gst_is_v4l2_memory (dmem_b))
	return FALSE;

  if (! gst_is_v4l2_memory (smem_b))
	return FALSE;

  smem = (GstV4l2Memory *)smem_b;

  dmem = (GstV4l2Memory *)dmem_b;

  ok = gst_v4l2_allocator_qbuf (mem2mem->output_allocator, smem->group);
  if (!ok)
	return FALSE;

  ok = gst_v4l2_allocator_qbuf (mem2mem->capture_allocator, dmem->group);
  if (!ok)
	return FALSE;

  flow = gst_v4l2_allocator_dqbuf (mem2mem->output_allocator, &smgroup);
  if (flow != GST_FLOW_OK)
	return FALSE;

  flow = gst_v4l2_allocator_dqbuf (mem2mem->capture_allocator, &dmgroup);
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
gst_v4l2_mem2mem_open (GstV4l2Mem2Mem * mem2mem)
{
  if (!gst_v4l2_object_open (mem2mem->output_object))
    return FALSE;

  if (!gst_v4l2_object_open_shared (mem2mem->capture_object, mem2mem->output_object)) {
    gst_v4l2_object_close (mem2mem->output_object);
    return FALSE;
  }

  return TRUE;
}

void
gst_v4l2_mem2mem_close (GstV4l2Mem2Mem * mem2mem)
{
  gst_v4l2_object_close (mem2mem->output_object);
  gst_v4l2_object_close (mem2mem->capture_object);
}

void
gst_v4l2_mem2mem_unlock (GstV4l2Mem2Mem * mem2mem)
{
  gst_v4l2_object_unlock (mem2mem->output_object);
  gst_v4l2_object_unlock (mem2mem->capture_object);
}

void
gst_v4l2_mem2mem_unlock_stop (GstV4l2Mem2Mem * mem2mem)
{
  gst_v4l2_object_unlock_stop (mem2mem->output_object);
  gst_v4l2_object_unlock_stop (mem2mem->capture_object);
}

void
gst_v4l2_mem2mem_stop (GstV4l2Mem2Mem * mem2mem)
{
  gst_v4l2_object_stop (mem2mem->output_object);
  gst_v4l2_object_stop (mem2mem->capture_object);
}
