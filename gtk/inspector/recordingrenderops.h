/*
 * Copyright © 2014 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#ifndef __GTK_RECORDING_RENDER_OPS_PRIVATE_H__
#define __GTK_RECORDING_RENDER_OPS_PRIVATE_H__

#include <cairo.h>

#include "gtk/gtkrenderopsprivate.h"
#include "gtkrenderoperation.h"

G_BEGIN_DECLS

#define GTK_TYPE_RECORDING_RENDER_OPS           (gtk_recording_render_ops_get_type ())
#define GTK_RECORDING_RENDER_OPS(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_RECORDING_RENDER_OPS, GtkRecordingRenderOps))
#define GTK_RECORDING_RENDER_OPS_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_RECORDING_RENDER_OPS, GtkRecordingRenderOpsClass))
#define GTK_IS_RECORDING_RENDER_OPS(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_RECORDING_RENDER_OPS))
#define GTK_IS_RECORDING_RENDER_OPS_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_RECORDING_RENDER_OPS))
#define GTK_RECORDING_RENDER_OPS_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_RECORDING_RENDER_OPS, GtkRecordingRenderOpsClass))

typedef struct _GtkRecordingRenderOps           GtkRecordingRenderOps;
typedef struct _GtkRecordingRenderOpsClass      GtkRecordingRenderOpsClass;

struct _GtkRecordingRenderOps
{
  GtkRenderOps parent;

  GList *widgets;
};

struct _GtkRecordingRenderOpsClass
{
  GtkRenderOpsClass parent_class;
};

GType                   gtk_recording_render_ops_get_type       (void) G_GNUC_CONST;

GtkRenderOps *          gtk_recording_render_ops_new            (void);

GtkRenderOperation *    gtk_recording_render_ops_run_for_widget (GtkRecordingRenderOps  *ops,
                                                                 GtkWidget              *widget);


G_END_DECLS

#endif /* __GTK_RECORDING_RENDER_OPS_PRIVATE_H__ */
