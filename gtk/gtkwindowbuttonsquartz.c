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

@interface NSWindow()
/* Expose the private titlebarHeight property, so we can set
 * the titlebar height to match the height of a GTK header bar.
 */
@property CGFloat titlebarHeight;

@end

enum
{
  PROP_0,
  PROP_DECORATION_LAYOUT,
  NUM_PROPERTIES
};

static GParamSpec *props[NUM_PROPERTIES] = { NULL, };

/**
 * GtkWindowButtonsQuartz:
 *
 * This class provides macOS native window buttons for close/minimize/maximize.
 * 
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

  char *decoration_layout;
};

struct _GtkWindowButtonsQuartzClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (GtkWindowButtonsQuartz, gtk_window_buttons_quartz, GTK_TYPE_WIDGET)


static gboolean
show_window_controls (NSWindow *window,
                      gboolean  show)
{
  g_return_val_if_fail (window != NULL, FALSE);

  /* By assigning a toolbar, the window controls are moved a bit more inwards,
   * In line with how toolbars look in macOS apps.
   */
  if (show)
    {
      NSToolbar *toolbar = [[NSToolbar alloc] init];
      [window setToolbar:toolbar];
      [toolbar release];
    }
  else
    [window setToolbar:nil];

  [[window standardWindowButton:NSWindowCloseButton] setHidden:!show];
  [[window standardWindowButton:NSWindowMiniaturizeButton] setHidden:!show];
  [[window standardWindowButton:NSWindowZoomButton] setHidden:!show];

  return TRUE;
}

static void
enable_window_controls (NSWindow *window,
                        gboolean  close,
                        gboolean  minimize,
                        gboolean  maximize)
{
  g_return_if_fail (window != NULL);

  [[window standardWindowButton:NSWindowCloseButton] setEnabled:close];
  [[window standardWindowButton:NSWindowMiniaturizeButton] setEnabled:minimize];
  [[window standardWindowButton:NSWindowZoomButton] setEnabled:maximize];
}

static void
set_window_controls_height (NSWindow *window,
                            int       height)
{
  g_return_if_fail (window != NULL);

  [window setTitlebarHeight:height];
  [[window contentView] setNeedsLayout:YES];
}

static void
window_controls_bounds (NSWindow *window, NSRect *out_bounds)
{
  NSRect bounds = NSZeroRect;
  NSButton* button;

  button = [window standardWindowButton:NSWindowCloseButton];
  bounds = NSUnionRect(bounds, [button frame]);

  button = [window standardWindowButton:NSWindowZoomButton];
  bounds = NSUnionRect(bounds, [button frame]);

  *out_bounds = bounds;
}

static NSWindow*
native_window (GtkWidget *widget)
{
  GtkNative *native = GTK_NATIVE (gtk_widget_get_root (widget));
  GdkSurface *surface = gtk_native_get_surface (native);

  if (GDK_IS_MACOS_SURFACE (surface))
    return (NSWindow*) gdk_macos_surface_get_native_window (GDK_MACOS_SURFACE (surface));

  return NULL;
}

static void
update_window_controls_from_decoration_layout (GtkWindowButtonsQuartz *self)
{
  char **tokens;

  if (self->decoration_layout)
    tokens = g_strsplit_set (self->decoration_layout, ",:", -1);
  else
    {
      char *layout_desc;
      g_object_get (gtk_widget_get_settings (GTK_WIDGET (self)),
                    "gtk-decoration-layout", &layout_desc,
                    NULL);
      tokens = g_strsplit_set (layout_desc, ",:", -1);

      g_free (layout_desc);
    }

  self->close = g_strv_contains ((const char * const *) tokens, "close");
  self->minimize = g_strv_contains ((const char * const *) tokens, "minimize");
  self->maximize = g_strv_contains ((const char * const *) tokens, "maximize");

  g_strfreev (tokens);

  enable_window_controls (native_window (GTK_WIDGET (self)),
                          self->close,
                          self->minimize,
                          self->maximize);
}

static void
gtk_window_buttons_quartz_finalize (GObject *object)
{
  GtkWindowButtonsQuartz *self = GTK_WINDOW_BUTTONS_QUARTZ (object);

  g_clear_pointer (&self->decoration_layout, g_free);

  G_OBJECT_CLASS (gtk_window_buttons_quartz_parent_class)->finalize (object);
}

static void
gtk_window_buttons_quartz_get_property (GObject     *object,
                                        guint        prop_id,
                                        GValue      *value,
                                        GParamSpec  *pspec)
{
  GtkWindowButtonsQuartz *self = GTK_WINDOW_BUTTONS_QUARTZ (object);

  switch (prop_id)
    {
    case PROP_DECORATION_LAYOUT:
      g_value_set_string (value, self->decoration_layout);
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
    case PROP_DECORATION_LAYOUT:
      g_free (self->decoration_layout);
      self->decoration_layout = g_strdup (g_value_get_string (value));

      update_window_controls_from_decoration_layout (self);
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
  NSWindow *window;

  GTK_WIDGET_CLASS (gtk_window_buttons_quartz_parent_class)->realize (widget);

  native = GTK_NATIVE (gtk_widget_get_root (widget));
  surface = gtk_native_get_surface (native);

  if (!GDK_IS_MACOS_SURFACE (surface))
    {
      g_critical ("Cannot show GtkWindowButtonsQuartz on a non-macos surface");
      return;
    }

  window = (NSWindow*) gdk_macos_surface_get_native_window (GDK_MACOS_SURFACE (surface));

  if (show_window_controls (window, TRUE))
    {
      NSRect bounds;

      enable_window_controls (window,
                              self->close,
                              self->minimize,
                              self->maximize);

      window_controls_bounds (window, &bounds);
      gtk_widget_set_size_request (widget, bounds.origin.x + bounds.size.width, bounds.size.height);
    }
}

static void
gtk_window_buttons_quartz_unrealize (GtkWidget *widget)
{
  GtkNative *native = GTK_NATIVE (gtk_widget_get_root (widget));
  GdkSurface *surface = gtk_native_get_surface (native);

  if (GDK_IS_MACOS_SURFACE (surface))
    {
      NSWindow *window = (NSWindow*) gdk_macos_surface_get_native_window (GDK_MACOS_SURFACE (surface));
      show_window_controls (window, FALSE);
    }

  GTK_WIDGET_CLASS (gtk_window_buttons_quartz_parent_class)->unrealize (widget);
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

  window_controls_bounds (window, &bounds);

  if (orientation == GTK_ORIENTATION_VERTICAL)
    *minimum = *natural = ceil(bounds.size.height);
  else if (orientation == GTK_ORIENTATION_HORIZONTAL)
    *minimum = *natural = ceil(bounds.origin.x + bounds.size.width);
}

static void
gtk_window_buttons_quartz_size_allocate (GtkWidget *widget,
                                         int        width,
                                         int        height,
                                         int        baseline)
{
  GtkNative *native = GTK_NATIVE (gtk_widget_get_root (widget));
  GdkSurface *surface = gtk_native_get_surface (native);
  NSWindow *window = (NSWindow*) gdk_macos_surface_get_native_window (GDK_MACOS_SURFACE (surface));

  GTK_WIDGET_CLASS (gtk_window_buttons_quartz_parent_class)->size_allocate (widget, width, height, baseline);

  set_window_controls_height (window, height);
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
      NSWindow *window = (NSWindow*) gdk_macos_surface_get_native_window (GDK_MACOS_SURFACE (surface));

      if (window == NULL)
        return;

      if (gtk_widget_get_state_flags (widget) & GTK_STATE_FLAG_INSENSITIVE)
        enable_window_controls (window, FALSE, FALSE, FALSE);
      else
        enable_window_controls (window, self->close, self->minimize, self->maximize);
    }

  GTK_WIDGET_CLASS (gtk_window_buttons_quartz_parent_class)->state_flags_changed (widget, previous_state_flags);
}

static void
gtk_window_buttons_quartz_class_init (GtkWindowButtonsQuartzClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  gobject_class->finalize = gtk_window_buttons_quartz_finalize;
  gobject_class->get_property = gtk_window_buttons_quartz_get_property;
  gobject_class->set_property = gtk_window_buttons_quartz_set_property;

  widget_class->measure = gtk_window_buttons_quartz_measure;
  widget_class->size_allocate = gtk_window_buttons_quartz_size_allocate;
  widget_class->realize = gtk_window_buttons_quartz_realize;
  widget_class->unrealize = gtk_window_buttons_quartz_unrealize;
  widget_class->state_flags_changed = gtk_window_buttons_quartz_state_flags_changed;

  /**
   * GtkWindowButtonsQuartz:decoration-layout:
   *
   * The decoration layout for window buttons.
   *
   * If this property is not set, the
   * [property@Gtk.Settings:gtk-decoration-layout] setting is used.
   */
  props[PROP_DECORATION_LAYOUT] =
      g_param_spec_string ("decoration-layout", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, props);

  gtk_widget_class_set_css_name (widget_class, I_("windowbuttonsquartz"));

  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_IMG);
}

static void
gtk_window_buttons_quartz_init (GtkWindowButtonsQuartz *self)
{
  self->close = TRUE;
  self->minimize = TRUE;
  self->maximize = TRUE;
  self->decoration_layout = NULL;
}
