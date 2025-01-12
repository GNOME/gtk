/* GTK - The GIMP Toolkit
 * Copyright (c) 2024 Arjan Molenaar <amolenaar@gnome.org>
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
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#import <AppKit/AppKit.h>
#include <math.h>
#include <gdk/macos/gdkmacossurface.h>
#include "gtkprivate.h"
#include "gtknative.h"
#include "gtkwindowbuttonsquartzprivate.h"

enum
{
  PROP_0,
  PROP_CLOSE,
  PROP_MINIMIZE,
  PROP_MAXIMIZE,
  NUM_PROPERTIES
};

static GParamSpec *props[NUM_PROPERTIES] = { NULL, };

/**
 * GtkWindowButtonsQuartz:
 *
 * This class provides macOS native window buttons for close/minimize/maximize.
 * The buttons can be set by adding "native" to the `decoration-layout` of
 * GtkWindowControls or GtkHeader.
 *
 * ## Accessibility
 *
 * `GtkWindowButtonsQuartz` uses the `GTK_ACCESSIBLE_ROLE_IMG` role.
 */

typedef struct _GtkWindowButtonsQuartzClass GtkWindowButtonsQuartzClass;

struct _GtkWindowButtonsQuartz
{
  GtkWidget parent_instance;

  gboolean close;
  gboolean minimize;
  gboolean maximize;
};

struct _GtkWindowButtonsQuartzClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (GtkWindowButtonsQuartz, gtk_window_buttons_quartz, GTK_TYPE_WIDGET)

static void
gtk_window_buttons_quartz_get_property (GObject     *object,
			                                  guint        prop_id,
			                                  GValue      *value,
			                                  GParamSpec  *pspec)
{
  GtkWindowButtonsQuartz *self = GTK_WINDOW_BUTTONS_QUARTZ (object);

  switch (prop_id)
    {
    case PROP_CLOSE:
      g_value_set_boolean (value, self->close);
      break;
    case PROP_MINIMIZE:
      g_value_set_boolean (value, self->minimize);
      break;
    case PROP_MAXIMIZE:
      g_value_set_boolean (value, self->maximize);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_window_buttons_quartz_set_property (GObject      *object,
			                                  guint         prop_id,
			                                  const GValue *value,
			                                  GParamSpec   *pspec)
{
  GtkWindowButtonsQuartz *self = GTK_WINDOW_BUTTONS_QUARTZ (object);

  switch (prop_id)
    {
    case PROP_CLOSE:
      self->close = g_value_get_boolean (value);
      break;
    case PROP_MINIMIZE:
      self->minimize = g_value_get_boolean (value);
      break;
    case PROP_MAXIMIZE:
      self->maximize = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_window_buttons_quartz_realize (GtkWidget *widget)
{
  GtkWindowButtonsQuartz *self = GTK_WINDOW_BUTTONS_QUARTZ (widget);
  GtkNative *native;
  GdkSurface *surface;

  GTK_WIDGET_CLASS (gtk_window_buttons_quartz_parent_class)->realize (widget);

  native = GTK_NATIVE (gtk_widget_get_root (widget));
  surface = gtk_native_get_surface (native);

  if (!GDK_IS_MACOS_SURFACE (surface))
    {
      g_critical ("Cannot show GtkWindowButtonsQuartz on a non-macos surface");
      return;
    }

  if (gdk_macos_surface_show_window_controls (GDK_MACOS_SURFACE (surface), TRUE))
    {
      gdk_macos_surface_enable_window_controls (GDK_MACOS_SURFACE (surface),
                                                self->close,
                                                self->minimize,
                                                self->maximize);
    }
}

static void
gtk_window_buttons_quartz_unrealize (GtkWidget *widget)
{
  GtkNative *native = GTK_NATIVE (gtk_widget_get_root (widget));
  GdkSurface *surface = gtk_native_get_surface (native);

  if (GDK_IS_MACOS_SURFACE (surface))
    gdk_macos_surface_show_window_controls (GDK_MACOS_SURFACE (surface), FALSE);

  GTK_WIDGET_CLASS (gtk_window_buttons_quartz_parent_class)->unrealize (widget);
}

/* initial_window_buttons_bounds:
 *
 * Record the bounding box of the window buttons once.
 * This way we always know the original bounding box
 * of the window decorations.
 */
static void
initial_window_buttons_bounds (NSWindow *window, NSRect *out_bounds)
{
  static NSRect bounds = { 0 };

  if (NSIsEmptyRect(bounds))
    {
      NSButton* button;

      button = [window standardWindowButton:NSWindowCloseButton];
      bounds = NSUnionRect(bounds, [button frame]);

      button = [window standardWindowButton:NSWindowMiniaturizeButton];
      bounds = NSUnionRect(bounds, [button frame]);

      button = [window standardWindowButton:NSWindowZoomButton];
      bounds = NSUnionRect(bounds, [button frame]);
    }
    *out_bounds = bounds;
}

static void
gtk_window_buttons_quartz_measure (GtkWidget      *widget,
                                   GtkOrientation  orientation,
                                   int             for_size,
                                   int            *minimum,
                                   int            *natural,
                                   int            *minimum_baseline,
                                   int            *natural_baseline)
{
  GtkNative *native = GTK_NATIVE (gtk_widget_get_root (widget));
  GdkSurface *surface = gtk_native_get_surface (native);
  NSWindow *window = (NSWindow*) gdk_macos_surface_get_native_window (GDK_MACOS_SURFACE (surface));
  NSRect bounds;

  if (window == NULL)
    return;

  initial_window_buttons_bounds (window, &bounds);

  if (orientation == GTK_ORIENTATION_VERTICAL)
    *minimum = *natural = ceil(bounds.size.height);
  else if (orientation == GTK_ORIENTATION_HORIZONTAL)
    *minimum = *natural = ceil(bounds.size.width);
}

static void
gtk_window_buttons_quartz_size_allocate (GtkWidget *widget,
                                         int        width,
                                         int        height,
                                         int        baseline)
{
  GtkNative *native = GTK_NATIVE (gtk_widget_get_root (widget));
  GdkSurface *surface = gtk_native_get_surface (native);

  GTK_WIDGET_CLASS (gtk_window_buttons_quartz_parent_class)->size_allocate (widget, width, height, baseline);

  gdk_macos_surface_set_window_controls_height (GDK_MACOS_SURFACE (surface), height);
}

static void
gtk_window_buttons_quartz_state_flags_changed (GtkWidget* widget,
                                               GtkStateFlags previous_state_flags)
{
  GtkWindowButtonsQuartz *self = GTK_WINDOW_BUTTONS_QUARTZ (widget);
  GtkNative *native = GTK_NATIVE (gtk_widget_get_root (widget));
  GdkSurface *surface;

  if (native != NULL && (surface = gtk_native_get_surface (native)) != NULL)
    {
      if (gtk_widget_get_state_flags (widget) & GTK_STATE_FLAG_INSENSITIVE)
        gdk_macos_surface_enable_window_controls (GDK_MACOS_SURFACE (surface),
                                                  FALSE,
                                                  FALSE,
                                                  FALSE);
      else
        gdk_macos_surface_enable_window_controls (GDK_MACOS_SURFACE (surface),
                                                  self->close,
                                                  self->minimize,
                                                  self->maximize);
    }

  GTK_WIDGET_CLASS (gtk_window_buttons_quartz_parent_class)->state_flags_changed (widget, previous_state_flags);
}

static void
gtk_window_buttons_quartz_class_init (GtkWindowButtonsQuartzClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  gobject_class->get_property = gtk_window_buttons_quartz_get_property;
  gobject_class->set_property = gtk_window_buttons_quartz_set_property;

  widget_class->measure = gtk_window_buttons_quartz_measure;
  widget_class->size_allocate = gtk_window_buttons_quartz_size_allocate;
  widget_class->realize = gtk_window_buttons_quartz_realize;
  widget_class->unrealize = gtk_window_buttons_quartz_unrealize;
  widget_class->state_flags_changed = gtk_window_buttons_quartz_state_flags_changed;

  props[PROP_CLOSE] =
      g_param_spec_boolean ("close", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY);
  props[PROP_MINIMIZE] =
      g_param_spec_boolean ("minimize", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY);
  props[PROP_MAXIMIZE] =
      g_param_spec_boolean ("maximize", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, props);

  gtk_widget_class_set_css_name (widget_class, I_("windowbuttonsquartz"));

  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_IMG);
}

static void
gtk_window_buttons_quartz_init (GtkWindowButtonsQuartz *buttons)
{
}
