/* GStreamer
 *
 * Copyright (C) 2016 Sebastien MATZ <sebastien.matz@veo-labs.com>
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

#include "gstv4l2m2m.h"
#include "gstv4l2object.h"
#include "v4l2_calls.h"
#include "gst/allocators/gstdmabuf.h"

GST_DEBUG_CATEGORY_EXTERN (v4l2_debug);
#define GST_CAT_DEFAULT v4l2_debug

GstV4l2M2m *
gst_v4l2_m2m_new (GstElement * parent, int index)
{
  GstV4l2M2m *m2m;
  GstV4l2UpdateFpsFunction update_fps_func = NULL;
  const char *default_device = NULL;

  m2m = (GstV4l2M2m *) g_new0 (GstV4l2M2m, 1);

  m2m->parent = parent;
  m2m->index = index;

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

  m2m->sink_obj->no_initial_format = TRUE;
  m2m->sink_obj->keep_aspect = FALSE;

  m2m->source_obj->no_initial_format = TRUE;
  m2m->source_obj->keep_aspect = FALSE;

  m2m->streaming = FALSE;

  m2m->sink_min_buffers = -1;
  m2m->source_min_buffers = -1;

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
get_memory_from_memory (GstV4l2M2m * m2m, GstMemory * mem,
    enum GstV4l2M2mBufferType buf_type)
{
  if (mem->allocator == (GstAllocator *) m2m->source_allocator) {
    goto our_buffer;
  }

  else if (mem->allocator == (GstAllocator *) m2m->sink_allocator) {
    goto our_buffer;
  }

  else if (mem->allocator == m2m->dmabuf_allocator) {
    mem = (GstMemory *)
        gst_mini_object_get_qdata (GST_MINI_OBJECT (mem),
        GST_V4L2_MEMORY_QUARK);

    if (mem->allocator == (GstAllocator *) m2m->source_allocator) {
      goto our_buffer;
    }

    else if (mem->allocator != (GstAllocator *) m2m->sink_allocator) {
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
      if (mem->allocator != (GstAllocator *) m2m->source_allocator)
        goto bad_requested_buffer_type;
      else
        return (GstV4l2Memory *) mem;

    case GST_V4L2_M2M_BUFTYPE_SINK:
      if (mem->allocator != (GstAllocator *) m2m->sink_allocator)
        goto bad_requested_buffer_type;
      else
        return (GstV4l2Memory *) mem;

    case GST_V4L2_M2M_BUFTYPE_ANY:
      return (GstV4l2Memory *) mem;

    default:
      goto bad_requested_buffer_type;
  }


bad_requested_buffer_type:
  return NULL;

not_our_buffer:
  return NULL;
}

static GstV4l2Memory *
get_memory_from_buffer (GstV4l2M2m * m2m, GstBuffer * buffer,
    enum GstV4l2M2mBufferType buf_type)
{
  GstMemory *mem;
  mem = gst_buffer_peek_memory (buffer, 0);
  return get_memory_from_memory (m2m, mem, buf_type);
}

static GstV4l2Allocator *
get_allocator_from_buffer (GstV4l2M2m * m2m, GstBuffer * our_buf)
{
  GstV4l2Memory *our_mem;
  GstV4l2Allocator *allocator;

  our_mem = get_memory_from_buffer (m2m, our_buf, GST_V4L2_M2M_BUFTYPE_ANY);
  if (our_mem == NULL)
    return NULL;

  allocator = (GstV4l2Allocator *) our_mem->mem.allocator;
  return allocator;
}

static enum GstV4l2M2mBufferType
get_buftype_from_allocator (GstV4l2M2m * m2m, GstV4l2Allocator * allocator)
{
  if (allocator == m2m->source_allocator)
    return GST_V4L2_M2M_BUFTYPE_SOURCE;
  else
    return GST_V4L2_M2M_BUFTYPE_SINK;
}


static GstV4l2IOMode
get_iomode_from_allocator (GstV4l2M2m * m2m, GstV4l2Allocator * allocator)
{
  if (allocator == m2m->source_allocator)
    return m2m->source_iomode;
  else
    return m2m->sink_iomode;
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


int
gst_v4l2_m2m_get_min_sink_buffers (GstV4l2M2m * m2m)
{
  int ret;
  struct v4l2_control control;
  int fd;

  if (m2m->sink_min_buffers >= 0)
    return m2m->sink_min_buffers;

  control.id = V4L2_CID_MIN_BUFFERS_FOR_OUTPUT;
  fd = m2m->sink_obj->video_fd;
  ret = v4l2_ioctl (fd, VIDIOC_G_CTRL, &control);
  if (ret < 0)
    return -1;
  return control.value;
}

int
gst_v4l2_m2m_get_min_source_buffers (GstV4l2M2m * m2m)
{
  int ret;
  struct v4l2_control control;
  int fd;

  if (m2m->source_min_buffers >= 0)
    return m2m->source_min_buffers;

  control.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE;
  fd = m2m->source_obj->video_fd;
  ret = v4l2_ioctl (fd, VIDIOC_G_CTRL, &control);
  if (ret < 0)
    return -1;
  return control.value;
}

gboolean
gst_v4l2_m2m_setup (GstV4l2M2m * m2m,
    GstCaps * source_caps, GstCaps * sink_caps, int nbufs)
{
  gboolean ok;
  int ret;
  enum v4l2_memory memory;
  GstV4l2Error error = GST_V4L2_ERROR_INIT;

  ok = get_v4l2_memory (m2m, GST_V4L2_M2M_BUFTYPE_SOURCE, &memory);
  if (!ok)
    return FALSE;

  ok = gst_v4l2_object_set_format (m2m->source_obj, source_caps, &error);
  if (!ok)
    return FALSE;

  m2m->source_allocator =
      gst_v4l2_allocator_new (GST_OBJECT (m2m->parent),
      m2m->source_obj->video_fd, &m2m->source_obj->format);

  ret = gst_v4l2_allocator_start (m2m->source_allocator, nbufs, memory);
  if (ret < nbufs)
    return FALSE;

  ok = gst_v4l2_object_set_format (m2m->sink_obj, sink_caps, &error);
  if (!ok)
    return FALSE;

  ok = get_v4l2_memory (m2m, GST_V4L2_M2M_BUFTYPE_SINK, &memory);
  if (!ok)
    return FALSE;

  m2m->sink_allocator =
      gst_v4l2_allocator_new (GST_OBJECT (m2m->parent), m2m->sink_obj->video_fd,
      &m2m->sink_obj->format);
  ret = gst_v4l2_allocator_start (m2m->sink_allocator, nbufs, memory);
  if (ret < nbufs)
    return FALSE;

  m2m->dmabuf_allocator = gst_dmabuf_allocator_new ();
  if (!m2m->dmabuf_allocator)
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
gst_v4l2_m2m_reset_buffer (GstV4l2M2m * m2m, GstBuffer * buf)
{
  GstMemory *mem;
  GstV4l2IOMode mode;
  GstV4l2Allocator *allocator;
  GstV4l2MemoryGroup *group;

  allocator = get_allocator_from_buffer (m2m, buf);
  if (!allocator)
    return FALSE;

  mode = get_iomode_from_allocator (m2m, allocator);

  if (mode != GST_V4L2_IO_DMABUF_IMPORT)
    return TRUE;

  mem = gst_buffer_peek_memory (buf, 0);
  group = ((GstV4l2Memory *) mem)->group;
  gst_v4l2_allocator_reset_group (allocator, group);

  return TRUE;
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


static gboolean
gst_v4l2_m2m_dispose_buffer (GstBuffer * buffer)
{
  gboolean do_free;
  GstV4l2M2mMeta *emeta;

  emeta = gst_v4l2_m2m_get_meta (buffer);

  if (emeta->dispose)
    do_free = emeta->dispose (buffer, emeta->user_data);
  else
    do_free = TRUE;
  return do_free;
}


GstBuffer *
gst_v4l2_m2m_alloc_buffer (GstV4l2M2m * m2m, enum GstV4l2M2mBufferType buf_type)
{
  GstV4l2MemoryGroup *group;
  GstV4l2Allocator *allocator;
  GstV4l2IOMode mode;
  GstBuffer *buf;
  GstMemory *mem;
  GstV4l2M2mMeta *emeta;

  buf = gst_buffer_new ();
  if (!buf)
    return NULL;

  emeta = gst_v4l2_m2m_meta_add (buf);
  if (emeta == NULL)
    return NULL;

  emeta->instance = m2m;
  buf->mini_object.dispose =
      (GstMiniObjectDisposeFunction) gst_v4l2_m2m_dispose_buffer;

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

  allocator = get_allocator_from_buffer (m2m, our_buf);
  if (!allocator)
    return FALSE;

  mode = get_iomode_from_allocator (m2m, allocator);
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
gst_v4l2_m2m_qbuf (GstV4l2M2m * m2m, GstBuffer * buf)
{
  gboolean ok;
  GstV4l2Memory *mem;
  GstV4l2Allocator *allocator;

  allocator = get_allocator_from_buffer (m2m, buf);
  if (!allocator)
    return FALSE;

  mem = get_memory_from_buffer (m2m, buf, GST_V4L2_M2M_BUFTYPE_ANY);
  if (!mem)
    return FALSE;

  ok = gst_v4l2_allocator_qbuf (allocator, mem->group);
  if (!ok)
    return FALSE;

  return TRUE;
}

gboolean
gst_v4l2_m2m_dqbuf (GstV4l2M2m * m2m, GstBuffer * buf)
{
  GstV4l2Memory *memp;
  GstV4l2Memory *mem;
  GstV4l2Allocator *allocator;
  GstFlowReturn flow;
  GstV4l2MemoryGroup *group;
  enum GstV4l2M2mBufferType buf_type;

  allocator = get_allocator_from_buffer (m2m, buf);
  if (!allocator)
    return FALSE;

  buf_type = get_buftype_from_allocator (m2m, allocator);

  flow = gst_v4l2_allocator_dqbuf (allocator, &group);
  if (flow != GST_FLOW_OK)
    return FALSE;

  mem = get_memory_from_memory (m2m, group->mem[0], GST_V4L2_M2M_BUFTYPE_ANY);
  memp = get_memory_from_buffer (m2m, buf, buf_type);
  if (mem != memp)
    return FALSE;

  if (group->n_mem != 1)
    return FALSE;

  return TRUE;
}


gboolean
gst_v4l2_m2m_require_streamon (GstV4l2M2m * m2m)
{
  int ret;

  if (m2m->streaming)
    return TRUE;

  ret = v4l2_ioctl (m2m->sink_obj->video_fd, VIDIOC_STREAMON,
      &m2m->sink_obj->type);
  if (ret < 0)
    return FALSE;

  ret = v4l2_ioctl (m2m->source_obj->video_fd, VIDIOC_STREAMON,
      &m2m->source_obj->type);
  if (ret < 0)
    return FALSE;

  m2m->streaming = TRUE;

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

GType
gst_v4l2_m2m_meta_api_get_type (void)
{
  static volatile GType type;
  static const gchar *tags[] = { NULL };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstV4l2M2mMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

static gboolean
gst_v4l2_m2m_meta_init (GstMeta * meta, gpointer params, GstBuffer * buffer)
{
  GstV4l2M2mMeta *emeta = (GstV4l2M2mMeta *) meta;

  emeta->instance = NULL;
  emeta->dispose = NULL;
  emeta->user_data = NULL;

  return TRUE;
}

static gboolean
gst_v4l2_m2m_meta_transform (GstBuffer * transbuf, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  /* M2M meta is not propagated on transform */

  return TRUE;
}

static void
gst_v4l2_m2m_meta_free (GstMeta * meta, GstBuffer * buffer)
{
  GstV4l2M2mMeta *emeta = (GstV4l2M2mMeta *) meta;

  emeta->instance = NULL;
  emeta->dispose = NULL;
  emeta->user_data = NULL;
}

const GstMetaInfo *
gst_v4l2_m2m_meta_get_info (void)
{
  static const GstMetaInfo *meta_info = NULL;

  if (g_once_init_enter (&meta_info)) {
    const GstMetaInfo *mi = gst_meta_register (GST_V4L2_M2M_META_API_TYPE,
        "GstV4l2M2mMeta",
        sizeof (GstV4l2M2mMeta),
        gst_v4l2_m2m_meta_init,
        gst_v4l2_m2m_meta_free,
        gst_v4l2_m2m_meta_transform);
    g_once_init_leave (&meta_info, mi);
  }
  return meta_info;
}

GstV4l2M2mMeta *
gst_v4l2_m2m_meta_add (GstBuffer * buffer)
{
  GstV4l2M2mMeta *emeta;

  g_return_val_if_fail (GST_IS_BUFFER (buffer), NULL);

  emeta = (GstV4l2M2mMeta *) gst_buffer_add_meta (buffer,
      GST_V4L2_M2M_META_INFO, NULL);

  emeta->instance = NULL;
  emeta->dispose = NULL;
  emeta->user_data = NULL;

  return emeta;
}
