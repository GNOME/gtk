/*
 * Copyright © 2019 Red Hat, Inc.
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
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include <math.h>


#include "gtkcssboxesimplprivate.h"
#include "gtkcsscolorvalueprivate.h"
#include "gtkcsscornervalueprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkcssshadowvalueprivate.h"
#include "gtknativeprivate.h"
#include "gtkwidgetprivate.h"

#include "gdk/gdksurfaceprivate.h"

typedef struct _GtkNativePrivate
{
  gulong update_handler_id;
  gulong layout_handler_id;
  gulong scale_changed_handler_id;
} GtkNativePrivate;

static GQuark quark_gtk_native_private;

/**
 * GtkNative:
 *
 * `GtkNative` is the interface implemented by all widgets that have
 * their own `GdkSurface`.
 *
 * The obvious example of a `GtkNative` is `GtkWindow`.
 *
 * Every widget that is not itself a `GtkNative` is contained in one,
 * and you can get it with [method@Gtk.Widget.get_native].
 *
 * To get the surface of a `GtkNative`, use [method@Gtk.Native.get_surface].
 * It is also possible to find the `GtkNative` to which a surface
 * belongs, with [func@Gtk.Native.get_for_surface].
 *
 * In addition to a [class@Gdk.Surface], a `GtkNative` also provides
 * a [class@Gsk.Renderer] for rendering on that surface. To get the
 * renderer, use [method@Gtk.Native.get_renderer].
 */

G_DEFINE_INTERFACE (GtkNative, gtk_native, GTK_TYPE_WIDGET)

static GskRenderer *
gtk_native_default_get_renderer (GtkNative *self)
{
  return NULL;
}

static void
gtk_native_default_get_surface_transform (GtkNative *self,
                                          double    *x,
                                          double    *y)
{
  *x = 0;
  *y = 0;
}

static void
gtk_native_default_layout (GtkNative *self,
                           int        width,
                           int        height)
{
}

static void
gtk_native_default_init (GtkNativeInterface *iface)
{
  iface->get_renderer = gtk_native_default_get_renderer;
  iface->get_surface_transform = gtk_native_default_get_surface_transform;
  iface->layout = gtk_native_default_layout;

  quark_gtk_native_private = g_quark_from_static_string ("gtk-native-private");
}

static void
frame_clock_update_cb (GdkFrameClock *clock,
                       GtkNative     *native)
{
  if (GTK_IS_ROOT (native))
    gtk_css_node_validate (gtk_widget_get_css_node (GTK_WIDGET (native)));
}

static void
gtk_native_layout (GtkNative *self,
                   int        width,
                   int        height)
{
  GTK_NATIVE_GET_IFACE (self)->layout (self, width, height);
}

static void
surface_layout_cb (GdkSurface *surface,
                   int         width,
                   int         height,
                   GtkNative  *native)
{
  gtk_native_layout (native, width, height);

  if (gtk_widget_needs_allocate (GTK_WIDGET (native)))
    gtk_native_queue_relayout (native);
}

static void
scale_changed_cb (GdkSurface *surface,
                  GParamSpec *pspec,
                  GtkNative  *native)
{
  _gtk_widget_scale_changed (GTK_WIDGET (native));
}

static void
verify_priv_unrealized (gpointer user_data)
{
  GtkNativePrivate *priv = user_data;

  g_warn_if_fail (priv->update_handler_id == 0);
  g_warn_if_fail (priv->layout_handler_id == 0);
  g_warn_if_fail (priv->scale_changed_handler_id == 0);

  g_free (priv);
}

/**
 * gtk_native_realize:
 * @self: a `GtkNative`
 *
 * Realizes a `GtkNative`.
 *
 * This should only be used by subclasses.
 */
void
gtk_native_realize (GtkNative *self)
{
  GdkSurface *surface;
  GdkFrameClock *clock;
  GtkNativePrivate *priv;

  g_return_if_fail (g_object_get_qdata (G_OBJECT (self),
                                        quark_gtk_native_private) == NULL);

  surface = gtk_native_get_surface (self);
  clock = gdk_surface_get_frame_clock (surface);
  g_return_if_fail (clock != NULL);

  priv = g_new0 (GtkNativePrivate, 1);
  priv->update_handler_id = g_signal_connect_after (clock, "update",
                                              G_CALLBACK (frame_clock_update_cb),
                                              self);
  priv->layout_handler_id = g_signal_connect (surface, "layout",
                                              G_CALLBACK (surface_layout_cb),
                                              self);

  priv->scale_changed_handler_id = g_signal_connect (surface, "notify::scale-factor",
                                                     G_CALLBACK (scale_changed_cb),
                                                     self);

  g_object_set_qdata_full (G_OBJECT (self),
                           quark_gtk_native_private,
                           priv,
                           verify_priv_unrealized);
}

/**
 * gtk_native_unrealize:
 * @self: a `GtkNative`
 *
 * Unrealizes a `GtkNative`.
 *
 * This should only be used by subclasses.
 */
void
gtk_native_unrealize (GtkNative *self)
{
  GtkNativePrivate *priv;
  GdkSurface *surface;
  GdkFrameClock *clock;

  priv = g_object_get_qdata (G_OBJECT (self), quark_gtk_native_private);
  g_return_if_fail (priv != NULL);

  surface = gtk_native_get_surface (self);
  clock = gdk_surface_get_frame_clock (surface);
  g_return_if_fail (clock != NULL);

  g_clear_signal_handler (&priv->update_handler_id, clock);
  g_clear_signal_handler (&priv->layout_handler_id, surface);
  g_clear_signal_handler (&priv->scale_changed_handler_id, surface);

  g_object_set_qdata (G_OBJECT (self), quark_gtk_native_private, NULL);
}

/**
 * gtk_native_get_surface:
 * @self: a `GtkNative`
 *
 * Returns the surface of this `GtkNative`.
 *
 * Returns: (transfer none) (nullable): the surface of @self
 */
GdkSurface *
gtk_native_get_surface (GtkNative *self)
{
  g_return_val_if_fail (GTK_IS_NATIVE (self), NULL);

  return GTK_NATIVE_GET_IFACE (self)->get_surface (self);
}

/**
 * gtk_native_get_renderer:
 * @self: a `GtkNative`
 *
 * Returns the renderer that is used for this `GtkNative`.
 *
 * Returns: (transfer none) (nullable): the renderer for @self
 */
GskRenderer *
gtk_native_get_renderer (GtkNative *self)
{
  g_return_val_if_fail (GTK_IS_NATIVE (self), NULL);

  return GTK_NATIVE_GET_IFACE (self)->get_renderer (self);
}

/**
 * gtk_native_get_surface_transform:
 * @self: a `GtkNative`
 * @x: (out): return location for the x coordinate
 * @y: (out): return location for the y coordinate
 *
 * Retrieves the surface transform of @self.
 *
 * This is the translation from @self's surface coordinates into
 * @self's widget coordinates.
 */
void
gtk_native_get_surface_transform (GtkNative *self,
                                  double    *x,
                                  double    *y)
{
  g_return_if_fail (GTK_IS_NATIVE (self));
  g_return_if_fail (x != NULL);
  g_return_if_fail (y != NULL);

  GTK_NATIVE_GET_IFACE (self)->get_surface_transform (self, x, y);
}

/**
 * gtk_native_get_for_surface:
 * @surface: a `GdkSurface`
 *
 * Finds the `GtkNative` associated with the surface.
 *
 * Returns: (transfer none) (nullable): the `GtkNative` that is associated with @surface
 */
GtkNative *
gtk_native_get_for_surface (GdkSurface *surface)
{
  GtkWidget *widget;

  widget = (GtkWidget *)gdk_surface_get_widget (surface);

  if (widget && GTK_IS_NATIVE (widget))
    return GTK_NATIVE (widget);

  return NULL;
}

void
gtk_native_queue_relayout (GtkNative *self)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GdkSurface *surface;
  GdkFrameClock *clock;

  surface = gtk_widget_get_surface (widget);
  clock = gtk_widget_get_frame_clock (widget);
  if (clock == NULL)
    return;

  gdk_frame_clock_request_phase (clock, GDK_FRAME_CLOCK_PHASE_UPDATE);
  gdk_surface_request_layout (surface);
}

