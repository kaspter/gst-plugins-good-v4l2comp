/* Generic video aggregator plugin
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

#ifndef __GST_V4L2_VIDEO_AGGREGATOR_H__
#define __GST_V4L2_VIDEO_AGGREGATOR_H__

#ifndef GST_USE_UNSTABLE_API
#warning "The Video library from gst-plugins-bad is unstable API and may change in future."
#warning "You can define GST_USE_UNSTABLE_API to avoid this warning."
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gstv4l2aggregator.h>

G_BEGIN_DECLS

#define GST_TYPE_V4L2_VIDEO_AGGREGATOR (gst_v4l2videoaggregator_get_type())
#define GST_V4L2_VIDEO_AGGREGATOR(obj) \
        (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_V4L2_VIDEO_AGGREGATOR, GstV4l2VideoAggregator))
#define GST_V4L2_VIDEO_AGGREGATOR_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_V4L2_VIDEO_AGGREGATOR, GstV4l2VideoAggregatorClass))
#define GST_IS_VIDEO_AGGREGATOR(obj) \
        (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_V4L2_VIDEO_AGGREGATOR))
#define GST_IS_VIDEO_AGGREGATOR_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_V4L2_VIDEO_AGGREGATOR))
#define GST_V4L2_VIDEO_AGGREGATOR_GET_CLASS(obj) \
        (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_V4L2_VIDEO_AGGREGATOR,GstV4l2VideoAggregatorClass))

typedef struct _GstV4l2VideoAggregator GstV4l2VideoAggregator;
typedef struct _GstV4l2VideoAggregatorClass GstV4l2VideoAggregatorClass;
typedef struct _GstV4l2VideoAggregatorPrivate GstV4l2VideoAggregatorPrivate;

#include "gstv4l2videoaggregatorpad.h"

/**
 * GstV4l2VideoAggregator:
 * @info: The #GstVideoInfo representing the currently set
 * srcpad caps.
 */
struct _GstV4l2VideoAggregator
{
  GstV4l2Aggregator aggregator;

  /*< public >*/
  /* Output caps */
  GstVideoInfo info;

  /* < private > */
  GstV4l2VideoAggregatorPrivate *priv;
  gpointer          _gst_reserved[GST_PADDING_LARGE];
};

/**
 * GstV4l2VideoAggregatorClass:
 * @update_caps:              Optional.
 *                            Lets subclasses update the #GstCaps representing
 *                            the src pad caps before usage.  Return %NULL to indicate failure.
 * @aggregate_frames:         Lets subclasses aggregate frames that are ready. Subclasses
 *                            should iterate the GstElement.sinkpads and use the already
 *                            mapped #GstVideoFrame from GstV4l2VideoAggregatorPad.aggregated_frame
 *                            or directly use the #GstBuffer from GstV4l2VideoAggregatorPad.buffer
 *                            if it needs to map the buffer in a special way. The result of the
 *                            aggregation should land in @outbuffer.
 * @get_output_buffer:        Optional.
 *                            Lets subclasses provide a #GstBuffer to be used as @outbuffer of
 *                            the #aggregate_frames vmethod.
 * @negotiated_caps:          Optional.
 *                            Notifies subclasses what caps format has been negotiated
 * @find_best_format:         Optional.
 *                            Lets subclasses decide of the best common format to use.
 * @preserve_update_caps_result: Sub-classes should set this to true if the return result
 *                               of the update_caps() method should not be further modified
 *                               by GstV4l2VideoAggregator by removing fields.
 **/
struct _GstV4l2VideoAggregatorClass
{
  /*< private >*/
  GstV4l2AggregatorClass parent_class;

  /*< public >*/
  GstCaps *          (*update_caps)               (GstV4l2VideoAggregator *  videoaggregator,
                                                   GstCaps            *  caps);
  GstFlowReturn      (*aggregate_frames)          (GstV4l2VideoAggregator *  videoaggregator,
                                                   GstBuffer          *  outbuffer);
  GstFlowReturn      (*get_output_buffer)         (GstV4l2VideoAggregator *  videoaggregator,
                                                   GstBuffer          ** outbuffer);
  gboolean           (*negotiated_caps)           (GstV4l2VideoAggregator *  videoaggregator,
                                                   GstCaps            *  caps);
  void               (*find_best_format)          (GstV4l2VideoAggregator *  vagg,
                                                   GstCaps            *  downstream_caps,
                                                   GstVideoInfo       *  best_info,
                                                   gboolean           *  at_least_one_alpha);

  gboolean           preserve_update_caps_result;

  GstCaps           *sink_non_alpha_caps;

  /* < private > */
  gpointer            _gst_reserved[GST_PADDING_LARGE];
};

GType gst_v4l2videoaggregator_get_type       (void);

G_END_DECLS
#endif /* __GST_V4L2_VIDEO_AGGREGATOR_H__ */
