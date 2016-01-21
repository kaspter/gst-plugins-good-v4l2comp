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

#include "gstv4l2m2m.h"
#include "gstv4l2object.h"
#include "v4l2_calls.h"

GST_DEBUG_CATEGORY_EXTERN (v4l2_debug);
#define GST_CAT_DEFAULT v4l2_debug

enum
{
  PROP_0,
  V4L2_STD_OBJECT_PROPS,
};

GstV4l2M2m *
gst_v4l2_m2m_new (GstElement * element,
    const char *default_device,
    GstV4l2UpdateFpsFunction update_fps_func)
{
  GstV4l2M2m * m2m;

  m2m = g_new0 (GstV4l2M2m, 1);

  m2m->v4l2output = gst_v4l2_object_new(element, V4L2_BUF_TYPE_VIDEO_OUTPUT,
      default_device, gst_v4l2_get_output, gst_v4l2_set_output, update_fps_func);

  m2m->v4l2capture = gst_v4l2_object_new(element, V4L2_BUF_TYPE_VIDEO_CAPTURE,
      default_device, gst_v4l2_get_input, gst_v4l2_set_input, update_fps_func);

  return m2m;
}

void
gst_v4l2_m2m_destroy (GstV4l2M2m * m2m)
{
  g_return_if_fail (m2m != NULL);

  gst_v4l2_object_destroy (m2m->v4l2capture);
  gst_v4l2_object_destroy (m2m->v4l2output);

  g_free (m2m);
}

gboolean
gst_v4l2_m2m_set_property_helper (GstV4l2M2m * m2m,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    case PROP_OUTPUT_IO_MODE:
      gst_v4l2_object_set_property_helper (m2m->v4l2output, PROP_IO_MODE, value,
          pspec);
      break;
    case PROP_CAPTURE_IO_MODE:
      gst_v4l2_object_set_property_helper (m2m->v4l2capture, PROP_IO_MODE, value,
          pspec);
      break;
    default:
      /* By default, only set on output */
      return gst_v4l2_object_set_property_helper (m2m->v4l2output, prop_id,
          value, pspec);
  }

  return TRUE;
}

gboolean
gst_v4l2_m2m_get_property_helper (GstV4l2M2m * m2m,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    case PROP_OUTPUT_IO_MODE:
      gst_v4l2_object_get_property_helper (m2m->v4l2output, PROP_IO_MODE, value,
          pspec);
      break;
    case PROP_CAPTURE_IO_MODE:
      gst_v4l2_object_get_property_helper (m2m->v4l2capture, PROP_IO_MODE, value,
          pspec);
      break;
    default:
      /* By default read from output */
      return gst_v4l2_object_get_property_helper (m2m->v4l2output, prop_id,
          value, pspec);
  }

  return TRUE;
}

gboolean
gst_v4l2_m2m_open (GstV4l2M2m * m2m)
{
  if (!gst_v4l2_object_open (m2m->v4l2output))
    return FALSE;

  if (!gst_v4l2_object_open_shared (m2m->v4l2capture, m2m->v4l2output)) {
    gst_v4l2_object_close (m2m->v4l2output);
    return FALSE;
  }

  return TRUE;
}

void
gst_v4l2_m2m_close (GstV4l2M2m * m2m)
{
  gst_v4l2_object_close (m2m->v4l2output);
  gst_v4l2_object_close (m2m->v4l2capture);
}

void
gst_v4l2_m2m_unlock (GstV4l2M2m * m2m)
{
  gst_v4l2_object_unlock (m2m->v4l2output);
  gst_v4l2_object_unlock (m2m->v4l2capture);
}

void
gst_v4l2_m2m_unlock_stop (GstV4l2M2m * m2m)
{
  gst_v4l2_object_unlock_stop (m2m->v4l2output);
  gst_v4l2_object_unlock_stop (m2m->v4l2capture);
}

void
gst_v4l2_m2m_stop (GstV4l2M2m * m2m)
{
  gst_v4l2_object_stop (m2m->v4l2output);
  gst_v4l2_object_stop (m2m->v4l2capture);
}

