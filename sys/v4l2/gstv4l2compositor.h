/* Generic video compositor plugin
 * Copyright (C) 2008 Wim Taymans <wim@fluendo.com>
 * Copyright (C) 2010 Sebastian Dröge <sebastian.droege@collabora.co.uk>
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

#ifndef __GST_V4L2_COMPOSITOR_H__
#define __GST_V4L2_COMPOSITOR_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include "gstv4l2m2m.h"
#include "gstv4l2videoaggregator.h"


G_BEGIN_DECLS
#define GST_TYPE_V4L2_COMPOSITOR (gst_v4l2_compositor_get_type())
#define GST_V4L2_COMPOSITOR(obj) \
        (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_V4L2_COMPOSITOR, GstV4l2Compositor))
#define GST_V4L2_COMPOSITOR_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_V4L2_COMPOSITOR, GstV4l2CompositorClass))
#define GST_IS_V4L2_COMPOSITOR(obj) \
        (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_V4L2_COMPOSITOR))
#define GST_IS_V4L2_COMPOSITOR_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_V4L2_COMPOSITOR))
typedef struct _GstV4l2Compositor GstV4l2Compositor;
typedef struct _GstV4l2CompositorClass GstV4l2CompositorClass;

/**
 * GstV4l2Compositor:
 *
 * The opaque #GstV4l2Compositor structure.
 */
struct _GstV4l2Compositor
{
  GstV4l2VideoAggregator videoaggregator;

  GstV4l2IOMode output_io_mode;
  GstV4l2IOMode capture_io_mode;
  gchar *videodev;
  GstCaps *srccaps;
  gboolean already_negotiated;
};

struct _GstV4l2CompositorClass
{
  GstV4l2VideoAggregatorClass parent_class;
};

GType gst_v4l2_compositor_get_type (void);

void gst_v4l2_compositor_install_properties_helper (GObjectClass *
    gobject_class);


G_END_DECLS
#endif /* __GST_V4L2_COMPOSITOR_H__ */
