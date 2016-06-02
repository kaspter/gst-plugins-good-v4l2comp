/* GStreamer aggregator base class
 * Copyright (C) 2014 Mathieu Duponchelle <mathieu.duponchelle@oencreed.com>
 * Copyright (C) 2014 Thibault Saunier <tsaunier@gnome.org>
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

#ifndef __GST_V4L2_AGGREGATOR_H__
#define __GST_V4L2_AGGREGATOR_H__

#ifndef GST_USE_UNSTABLE_API
#warning "The Base library from gst-plugins-bad is unstable API and may change in future."
#warning "You can define GST_USE_UNSTABLE_API to avoid this warning."
#endif

#include <gst/gst.h>

G_BEGIN_DECLS

/**************************
 * GstV4l2Aggregator Structs  *
 *************************/

typedef struct _GstV4l2Aggregator GstV4l2Aggregator;
typedef struct _GstV4l2AggregatorPrivate GstV4l2AggregatorPrivate;
typedef struct _GstV4l2AggregatorClass GstV4l2AggregatorClass;

/************************
 * GstV4l2AggregatorPad API *
 ***********************/

#define GST_TYPE_V4L2_AGGREGATOR_PAD            (gst_v4l2_aggregator_pad_get_type())
#define GST_V4L2_AGGREGATOR_PAD(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_V4L2_AGGREGATOR_PAD, GstV4l2AggregatorPad))
#define GST_V4L2_AGGREGATOR_PAD_CAST(obj)       ((GstV4l2AggregatorPad *)(obj))
#define GST_V4L2_AGGREGATOR_PAD_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_V4L2_AGGREGATOR_PAD, GstV4l2AggregatorPadClass))
#define GST_V4L2_AGGREGATOR_PAD_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),GST_TYPE_V4L2_AGGREGATOR_PAD, GstV4l2AggregatorPadClass))
#define GST_IS_V4L2_AGGREGATOR_PAD(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_V4L2_AGGREGATOR_PAD))
#define GST_IS_V4L2_AGGREGATOR_PAD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_V4L2_AGGREGATOR_PAD))

/****************************
 * GstV4l2AggregatorPad Structs *
 ***************************/

typedef struct _GstV4l2AggregatorPad GstV4l2AggregatorPad;
typedef struct _GstV4l2AggregatorPadClass GstV4l2AggregatorPadClass;
typedef struct _GstV4l2AggregatorPadPrivate GstV4l2AggregatorPadPrivate;

/**
 * GstV4l2AggregatorPad:
 * @buffer: currently queued buffer.
 * @segment: last segment received.
 *
 * The implementation the GstPad to use with #GstV4l2Aggregator
 */
struct _GstV4l2AggregatorPad
{
  GstPad                       parent;

  /* Protected by the OBJECT_LOCK */
  GstSegment segment;
  /* Segment to use in the clip function, before the queue */
  GstSegment clip_segment;

  /* < Private > */
  GstV4l2AggregatorPadPrivate   *  priv;

  gpointer _gst_reserved[GST_PADDING];
};

/**
 * GstV4l2AggregatorPadClass:
 * @flush:    Optional
 *            Called when the pad has received a flush stop, this is the place
 *            to flush any information specific to the pad, it allows for individual
 *            pads to be flushed while others might not be.
 *
 */
struct _GstV4l2AggregatorPadClass
{
  GstPadClass   parent_class;

  GstFlowReturn (*flush)     (GstV4l2AggregatorPad * aggpad, GstV4l2Aggregator * aggregator);

  /*< private >*/
  gpointer      _gst_reserved[GST_PADDING_LARGE];
};

GType gst_v4l2_aggregator_pad_get_type           (void);

/****************************
 * GstV4l2AggregatorPad methods *
 ***************************/

GstBuffer * gst_v4l2_aggregator_pad_steal_buffer (GstV4l2AggregatorPad *  pad);
GstBuffer * gst_v4l2_aggregator_pad_get_buffer   (GstV4l2AggregatorPad *  pad);
gboolean    gst_v4l2_aggregator_pad_drop_buffer  (GstV4l2AggregatorPad *  pad);
gboolean    gst_v4l2_aggregator_pad_is_eos       (GstV4l2AggregatorPad *  pad);

/*********************
 * GstV4l2Aggregator API *
 ********************/

#define GST_TYPE_V4L2_AGGREGATOR            (gst_v4l2_aggregator_get_type())
#define GST_V4L2_AGGREGATOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_V4L2_AGGREGATOR,GstV4l2Aggregator))
#define GST_V4L2_AGGREGATOR_CAST(obj)       ((GstV4l2Aggregator *)(obj))
#define GST_V4L2_AGGREGATOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_V4L2_AGGREGATOR,GstV4l2AggregatorClass))
#define GST_V4L2_AGGREGATOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),GST_TYPE_V4L2_AGGREGATOR,GstV4l2AggregatorClass))
#define GST_IS_V4L2_AGGREGATOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_V4L2_AGGREGATOR))
#define GST_IS_V4L2_AGGREGATOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_V4L2_AGGREGATOR))

#define GST_FLOW_NOT_HANDLED           GST_FLOW_CUSTOM_SUCCESS

/**
 * GstV4l2Aggregator:
 * @srcpad: the aggregator's source pad
 * @segment: the output segment
 *
 * Aggregator base class object structure.
 */
struct _GstV4l2Aggregator
{
  GstElement               parent;

  GstPad                *  srcpad;

  /* Only access with the object lock held */
  GstSegment               segment;

  /*< private >*/
  GstV4l2AggregatorPrivate  *  priv;

  gpointer                 _gst_reserved[GST_PADDING_LARGE];
};

/**
 * GstV4l2AggregatorClass:
 * @sinkpads_type:  Optional.
 *                  The type of the pads that should be created when
 *                  GstElement.request_new_pad is called.
 * @flush:          Optional.
 *                  Called after a succesful flushing seek, once all the flush
 *                  stops have been received. Flush pad-specific data in
 *                  #GstV4l2AggregatorPad->flush.
 * @clip:           Optional.
 *                  Called when a buffer is received on a sink pad, the task
 *                  of clipping it and translating it to the current segment
 *                  falls on the subclass.
 * @sink_event:     Optional.
 *                  Called when an event is received on a sink pad, the subclass
 *                  should always chain up.
 * @sink_query:     Optional.
 *                  Called when a query is received on a sink pad, the subclass
 *                  should always chain up.
 * @src_event:      Optional.
 *                  Called when an event is received on the src pad, the subclass
 *                  should always chain up.
 * @src_query:      Optional.
 *                  Called when a query is received on the src pad, the subclass
 *                  should always chain up.
 * @src_activate:   Optional.
 *                  Called when the src pad is activated, it will start/stop its
 *                  pad task right after that call.
 * @aggregate:      Mandatory.
 *                  Called when buffers are queued on all sinkpads. Classes
 *                  should iterate the GstElement->sinkpads and peek or steal
 *                  buffers from the #GstV4l2AggregatorPads. If the subclass returns
 *                  GST_FLOW_EOS, sending of the eos event will be taken care
 *                  of. Once / if a buffer has been constructed from the
 *                  aggregated buffers, the subclass should call _finish_buffer.
 * @stop:           Optional.
 *                  Called when the element goes from PAUSED to READY.
 *                  The subclass should free all resources and reset its state.
 * @start:          Optional.
 *                  Called when the element goes from READY to PAUSED.
 *                  The subclass should get ready to process
 *                  aggregated buffers.
 * @get_next_time:  Optional.
 *                  Called when the element needs to know the running time of the next
 *                  rendered buffer for live pipelines. This causes deadline
 *                  based aggregation to occur. Defaults to returning
 *                  GST_CLOCK_TIME_NONE causing the element to wait for buffers
 *                  on all sink pads before aggregating.
 *
 * The aggregator base class will handle in a thread-safe way all manners of
 * concurrent flushes, seeks, pad additions and removals, leaving to the
 * subclass the responsibility of clipping buffers, and aggregating buffers in
 * the way the implementor sees fit.
 *
 * It will also take care of event ordering (stream-start, segment, eos).
 *
 * Basically, a basic implementation will override @aggregate, and call
 * _finish_buffer from inside that function.
 */
struct _GstV4l2AggregatorClass {
  GstElementClass   parent_class;

  GType             sinkpads_type;

  GstFlowReturn     (*flush)          (GstV4l2Aggregator    *  aggregator);

  GstFlowReturn     (*clip)           (GstV4l2Aggregator    *  aggregator,
                                       GstV4l2AggregatorPad *  aggregator_pad,
                                       GstBuffer        *  buf,
                                       GstBuffer        ** outbuf);

  /* sinkpads virtual methods */
  gboolean          (*sink_event)     (GstV4l2Aggregator    *  aggregator,
                                       GstV4l2AggregatorPad *  aggregator_pad,
                                       GstEvent         *  event);

  gboolean          (*sink_query)     (GstV4l2Aggregator    *  aggregator,
                                       GstV4l2AggregatorPad *  aggregator_pad,
                                       GstQuery         *  query);

  /* srcpad virtual methods */
  gboolean          (*src_event)      (GstV4l2Aggregator    *  aggregator,
                                       GstEvent         *  event);

  gboolean          (*src_query)      (GstV4l2Aggregator    *  aggregator,
                                       GstQuery         *  query);

  gboolean          (*src_activate)   (GstV4l2Aggregator    *  aggregator,
                                       GstPadMode          mode,
                                       gboolean            active);

  GstFlowReturn     (*aggregate)      (GstV4l2Aggregator    *  aggregator,
                                       gboolean            timeout);

  gboolean          (*stop)           (GstV4l2Aggregator    *  aggregator);

  gboolean          (*start)          (GstV4l2Aggregator    *  aggregator);

  GstClockTime      (*get_next_time)  (GstV4l2Aggregator    *  aggregator);

  GstV4l2AggregatorPad * (*create_new_pad) (GstV4l2Aggregator  * self,
                                        GstPadTemplate * templ,
                                        const gchar    * req_name,
                                        const GstCaps  * caps);

  /*< private >*/
  gpointer          _gst_reserved[GST_PADDING_LARGE];
};

/************************************
 * GstV4l2Aggregator convenience macros *
 ***********************************/

/**
 * GST_V4L2_AGGREGATOR_SRC_PAD:
 * @agg: a #GstV4l2Aggregator
 *
 * Convenience macro to access the source pad of #GstV4l2Aggregator
 *
 * Since: 1.6
 */
#define GST_V4L2_AGGREGATOR_SRC_PAD(agg) (((GstV4l2Aggregator *)(agg))->srcpad)

/*************************
 * GstV4l2Aggregator methods *
 ************************/

GstFlowReturn  gst_v4l2_aggregator_finish_buffer         (GstV4l2Aggregator                *  agg,
                                                     GstBuffer                    *  buffer);
void           gst_v4l2_aggregator_set_src_caps          (GstV4l2Aggregator                *  agg,
                                                     GstCaps                      *  caps);

void           gst_v4l2_aggregator_set_latency           (GstV4l2Aggregator                *  self,
                                                     GstClockTime                    min_latency,
                                                     GstClockTime                    max_latency);

GType gst_v4l2_aggregator_get_type(void);

/* API that should eventually land in GstElement itself (FIXME) */
typedef gboolean (*GstV4l2AggregatorPadForeachFunc)    (GstV4l2Aggregator                 *  aggregator,
                                                    GstV4l2AggregatorPad              *  aggregator_pad,
                                                    gpointer                         user_data);

gboolean gst_v4l2_aggregator_iterate_sinkpads           (GstV4l2Aggregator                 *  self,
                                                    GstV4l2AggregatorPadForeachFunc      func,
                                                    gpointer                         user_data);

GstClockTime  gst_v4l2_aggregator_get_latency           (GstV4l2Aggregator                 *  self);

G_END_DECLS

#endif /* __GST_V4L2_AGGREGATOR_H__ */
