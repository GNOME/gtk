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
#include <gdk/macos/GdkMacosWindow.h>
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

  GBinding *fullscreen_binding;
};

struct _GtkWindowButtonsQuartzClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (GtkWindowButtonsQuartz, gtk_window_buttons_quartz, GTK_TYPE_WIDGET)

static void
set_window_controls_height (GdkMacosWindow *window,
                            int             height)
{
  g_return_if_fail (GDK_IS_MACOS_WINDOW (window));

  if ([window respondsToSelector:@selector(setTitlebarHeight:)])
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

static GdkMacosWindow*
native_window (GtkWidget *widget)
{
  GtkNative *native = GTK_NATIVE (gtk_widget_get_root (widget));

  if (native != NULL)
    {
      GdkSurface *surface = gtk_native_get_surface (native);

      if (GDK_IS_MACOS_SURFACE (surface))
        return (GdkMacosWindow*) gdk_macos_surface_get_native_window (GDK_MACOS_SURFACE (surface));
    }
  return NULL;
}

static void
enable_window_controls (GtkWindowButtonsQuartz *self,
                        gboolean                enabled)
{
  gboolean is_sovereign_window;
  gboolean resizable;
  gboolean deletable;
  GtkRoot *root;
  GtkWindow *window;
  GdkMacosWindow *nswindow;

  root = gtk_widget_get_root (GTK_WIDGET (self));
  if (!root || !GTK_IS_WINDOW (root))
    return;

  nswindow = native_window (GTK_WIDGET (self));
  if (!GDK_IS_MACOS_WINDOW (nswindow))
    return;

  window = GTK_WINDOW (root);
  is_sovereign_window = !gtk_window_get_modal (window) &&
                         gtk_window_get_transient_for (window) == NULL;
  resizable = gtk_window_get_resizable (window);
  deletable = gtk_window_get_deletable (window);

  [[nswindow standardWindowButton:NSWindowCloseButton] setEnabled:enabled && self->close && deletable];
  [[nswindow standardWindowButton:NSWindowMiniaturizeButton] setEnabled:enabled && self->minimize && is_sovereign_window];
  [[nswindow standardWindowButton:NSWindowZoomButton] setEnabled:enabled && self->maximize && resizable && is_sovereign_window];
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

  enable_window_controls (self, TRUE);
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
gtk_window_buttons_quartz_root (GtkWidget *widget)
{
  GtkWindowButtonsQuartz *self = GTK_WINDOW_BUTTONS_QUARTZ (widget);

  GTK_WIDGET_CLASS (gtk_window_buttons_quartz_parent_class)->root (widget);

  if (self->fullscreen_binding)
    g_object_unref (self->fullscreen_binding);

  self->fullscreen_binding = g_object_bind_property (gtk_widget_get_root (widget),
                                                     "fullscreened",
                                                     widget,
                                                     "visible",
                                                     G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
}

static void
gtk_window_buttons_quartz_unroot (GtkWidget *widget)
{
  GtkWindowButtonsQuartz *self = GTK_WINDOW_BUTTONS_QUARTZ (widget);

  if (self->fullscreen_binding)
    {
      g_object_unref (self->fullscreen_binding);
      self->fullscreen_binding = NULL;
    }

  GTK_WIDGET_CLASS (gtk_window_buttons_quartz_parent_class)->unroot (widget);
}

static void
gtk_window_buttons_quartz_realize (GtkWidget *widget)
{
  GtkWindowButtonsQuartz *self = GTK_WINDOW_BUTTONS_QUARTZ (widget);
  GdkMacosWindow *window;
  NSRect bounds;

  GTK_WIDGET_CLASS (gtk_window_buttons_quartz_parent_class)->realize (widget);

  window = native_window (widget);

  if (!GDK_IS_MACOS_WINDOW (window))
    {
      g_critical ("Cannot show GtkWindowButtonsQuartz on a non-macos window");
      return;
    }

  [window setShowStandardWindowButtons:YES];

  enable_window_controls (self, TRUE);

  window_controls_bounds (window, &bounds);
  gtk_widget_set_size_request (widget, bounds.origin.x + bounds.size.width, bounds.size.height);
}

static void
gtk_window_buttons_quartz_unrealize (GtkWidget *widget)
{
  GdkMacosWindow *window = native_window (widget);

  if (GDK_IS_MACOS_WINDOW (window))
    [window setShowStandardWindowButtons:NO];

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
  GdkMacosWindow *window = native_window (widget);

  if (GDK_IS_MACOS_WINDOW (window))
    {
      NSRect bounds;

      window_controls_bounds (window, &bounds);

      if (orientation == GTK_ORIENTATION_VERTICAL)
        *minimum = *natural = ceil(bounds.size.height);
      else if (orientation == GTK_ORIENTATION_HORIZONTAL)
        *minimum = *natural = ceil(bounds.origin.x + bounds.size.width);
    }
}

static void
gtk_window_buttons_quartz_size_allocate (GtkWidget *widget,
                                         int        width,
                                         int        height,
                                         int        baseline)
{
  GdkMacosWindow *window = native_window (widget);
  graphene_rect_t bounds = { 0 };

  GTK_WIDGET_CLASS (gtk_window_buttons_quartz_parent_class)->size_allocate (widget, width, height, baseline);

  if (!gtk_widget_compute_bounds (widget, (GtkWidget *) gtk_widget_get_root (widget), &bounds))
    g_warning ("Could not calculate widget bounds");

  set_window_controls_height (window, bounds.origin.y * 2 + height);
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
  widget_class->root = gtk_window_buttons_quartz_root;
  widget_class->unroot = gtk_window_buttons_quartz_unroot;
  widget_class->realize = gtk_window_buttons_quartz_realize;
  widget_class->unrealize = gtk_window_buttons_quartz_unrealize;

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
  self->fullscreen_binding = NULL;
}
