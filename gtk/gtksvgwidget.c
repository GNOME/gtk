/*
 * Copyright © 2026 Red Hat, Inc
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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

#include "gtksvgwidget.h"
#include "gtkwidgetprivate.h"
#include "gtktooltip.h"
#include "gtkcssnodeprivate.h"
#include "gtkcssstyleprivate.h"
#include "gtkrendericonprivate.h"
#include "gtkmarshalers.h"
#include "svg/gtksvgprivate.h"
#include "svg/gtksvgrendererprivate.h"
#include "svg/gtksvgeventcontrollerprivate.h"
#include "svg/gtksvgelementprivate.h"
#include "svg/gtksvghrefprivate.h"


/**
 * GtkSvgWidget:
 *
 * A widget that renders SVG, with animations and event handling.
 *
 * `GtkSvgWidget` uses [class@Gtk.Svg] internally, and should read
 * its documentation to learn about the supported SVG features and
 * extensions.
 *
 * On top of the `GtkSvg` rendering, `GtkSvgWidget` adds event handling.
 * <kbd>Tab</kbd> and <kbd>Shift</kbd>+<kbd>Tab</kbd> keys can be used
 * to move the focus and <kbd>Enter</kbd> and clicks will activate links
 * by emitting the [signal@Gtk.SvgWidget::activate] signal.
 *
 * The `tabindex` and `autofocus` attributes can be used to influence
 * what elements act as focus locations, and where focus goes initially.
 *
 * The styling of the SVG content is following input-related pseudo
 * classes such as `:focus`, `:active`, `:hover` or `:visited`.
 *
 * If [property@Gtk.Widget:has-tooltip] is set, then the content
 * of \<title\> elements will be shown as tooltips.
 *
 * SVG animations and different \<view\>s can be triggered by input
 * events as well. The following events are supported: focus, blur,
 * mouseenter, mouseleave, click.
 * See the [SVG animation](https://svgwg.org/specs/animations/)
 * specification for details.
 *
 * Since: 4.24
 */

struct _GtkSvgWidget
{
  GtkWidget parent;
  GtkSvg *svg;
};

struct _GtkSvgWidgetClass
{
  GtkWidgetClass parent_class;
};

enum
{
  PROP_RESOURCE = 1,
  PROP_STATE,
  PROP_STYLESHEET,
  NUM_PROPERTIES,
};

static GParamSpec *properties[NUM_PROPERTIES];

static unsigned int activate_signal;
static unsigned int error_signal;

/* {{{ Focus handling */

static gboolean
direction_is_forward (GtkDirectionType direction)
{
  switch (direction)
    {
    case GTK_DIR_TAB_FORWARD:
    case GTK_DIR_RIGHT:
    case GTK_DIR_DOWN:
      return TRUE;
    case GTK_DIR_TAB_BACKWARD:
    case GTK_DIR_LEFT:
    case GTK_DIR_UP:
      return FALSE;
    default:
      g_assert_not_reached ();
    }
}

/* This is a 99% copy of gtk_widget_real_focus, replacing
 * gtk_widget_focus_move calls with gtk_svg_move_focus
 */
static gboolean
gtk_svg_widget_focus (GtkWidget        *widget,
                      GtkDirectionType  direction)
{
  GtkSvgWidget *self = GTK_SVG_WIDGET (widget);
  GtkWidget *focus;

  if (gtk_widget_is_focus (widget))
    {
      if (gtk_svg_move_focus (self->svg, direction))
        return TRUE;

      gtk_svg_lose_focus (self->svg);
      return FALSE;
    }

  focus = gtk_window_get_focus (GTK_WINDOW (gtk_widget_get_root (widget)));

  if (focus && gtk_widget_is_ancestor (focus, widget))
    {
      if (gtk_svg_move_focus (self->svg, direction))
        return gtk_widget_grab_focus (widget);

      if (direction_is_forward (direction))
        return FALSE;
      else
        {
          return gtk_svg_grab_focus (self->svg) &&
                 gtk_widget_grab_focus (widget);
        }
    }

  if (!direction_is_forward (direction))
    {
      if (gtk_svg_move_focus (self->svg, direction))
        return gtk_widget_grab_focus (widget);

      return gtk_svg_grab_focus (self->svg) &&
             gtk_widget_grab_focus (widget);
    }
  else
    {
      if (gtk_svg_grab_focus (self->svg))
        return gtk_widget_grab_focus (widget);

      return gtk_svg_move_focus (self->svg, direction) &&
             gtk_widget_grab_focus (widget);
    }
}

/* }}} */
/* {{{ GObject boilerplate */

G_DEFINE_TYPE (GtkSvgWidget, gtk_svg_widget, GTK_TYPE_WIDGET)

static void
error_cb (GtkSvg       *svg,
          const GError *error,
          GtkSvgWidget *self)
{
  g_signal_emit (self, error_signal, 0, error);
}

static void
activate_cb (SvgElement *element,
             gpointer    data)
{
  GtkSvgWidget *self = data;

  if (svg_element_get_type (element) == SVG_ELEMENT_LINK)
    {
      const char *id = svg_element_get_id (element);
      SvgValue *href = svg_element_get_current_value (element, SVG_PROPERTY_HREF);
      if (svg_href_get_kind (href) != HREF_NONE)
        g_signal_emit (self, activate_signal, 0, id, svg_href_get_ref (href));
    }
}

static void
hover_cb (SvgElement *element,
          gpointer    data)
{
  GtkSvgWidget *self = data;

  if (svg_element_or_ancestor_has_type (element, SVG_ELEMENT_LINK))
    gtk_widget_set_cursor_from_name (GTK_WIDGET (self), "pointer");
  else
    gtk_widget_set_cursor_from_name (GTK_WIDGET (self), NULL);
}

static void
gtk_svg_widget_init (GtkSvgWidget *self)
{
  gtk_widget_set_focusable (GTK_WIDGET (self), TRUE);

  self->svg = gtk_svg_new ();

  gtk_svg_play (self->svg);

  g_object_bind_property (self, "overflow",
                          self->svg, "overflow",
                          G_BINDING_SYNC_CREATE);

  g_signal_connect_swapped (self->svg, "invalidate-contents",
                            G_CALLBACK (gtk_widget_queue_draw), self);
  g_signal_connect_swapped (self->svg, "invalidate-size",
                            G_CALLBACK (gtk_widget_queue_resize), self);

  g_signal_connect (self->svg, "error", G_CALLBACK (error_cb), self);

  gtk_svg_set_activate_callback (self->svg, activate_cb, self);
  gtk_svg_set_hover_callback (self->svg, hover_cb, self);

  gtk_widget_add_controller (GTK_WIDGET (self),
                             gtk_svg_event_controller_new (self->svg));
}

static void
gtk_svg_widget_get_property (GObject      *object,
                             unsigned int  property_id,
                             GValue       *value,
                             GParamSpec   *pspec)
{
  GtkSvgWidget *self = GTK_SVG_WIDGET (object);

  switch (property_id)
    {
    case PROP_RESOURCE:
      g_value_set_string (value, self->svg->resource);
      break;

    case PROP_STATE:
      g_value_set_uint (value, gtk_svg_widget_get_state (self));
      break;

    case PROP_STYLESHEET:
      g_value_set_boxed (value, gtk_svg_widget_get_stylesheet (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_svg_widget_set_property (GObject      *object,
                             unsigned int  property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkSvgWidget *self = GTK_SVG_WIDGET (object);

  switch (property_id)
    {
    case PROP_RESOURCE:
      gtk_svg_load_from_resource (self->svg, g_value_get_string (value));
      break;

    case PROP_STATE:
      gtk_svg_widget_set_state (self, g_value_get_uint (value));
      break;

    case PROP_STYLESHEET:
      gtk_svg_widget_set_stylesheet (self, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_svg_widget_dispose (GObject *object)
{
  GtkSvgWidget *self = GTK_SVG_WIDGET (object);

  g_clear_object (&self->svg);

  G_OBJECT_CLASS (gtk_svg_widget_parent_class)->dispose (object);
}

static void
gtk_svg_widget_realize (GtkWidget *widget)
{
  GtkSvgWidget *self = GTK_SVG_WIDGET (widget);

  GTK_WIDGET_CLASS (gtk_svg_widget_parent_class)->realize (widget);

  gtk_svg_set_frame_clock (self->svg, gtk_widget_get_frame_clock (widget));
  gtk_svg_play (self->svg);
}

static void
gtk_svg_widget_measure (GtkWidget      *widget,
                        GtkOrientation  orientation,
                        int             for_size,
                        int            *minimum,
                        int            *natural,
                        int            *minimum_baseline,
                        int            *natural_baseline)
{
  GtkSvgWidget *self = GTK_SVG_WIDGET (widget);
  double default_width, default_height;
  double min_width, min_height, nat_width, nat_height;

  /* ? */
  default_width = 16;
  default_height = 16;
  min_width = 0;
  min_height = 0;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      gdk_paintable_compute_concrete_size (GDK_PAINTABLE (self->svg),
                                           0,
                                           for_size < 0 ? 0 : for_size,
                                           default_width, default_height,
                                           &nat_width, &nat_height);
      *minimum = ceil (min_width);
      *natural = ceil (nat_width);
    }
  else
    {
      gdk_paintable_compute_concrete_size (GDK_PAINTABLE (self->svg),
                                           for_size < 0 ? 0 : for_size,
                                           0,
                                           default_width, default_height,
                                           &nat_width, &nat_height);
      *minimum = ceil (min_height);
      *natural = ceil (nat_height);
    }
}

static void
gtk_svg_widget_snapshot (GtkWidget   *widget,
                         GtkSnapshot *snapshot)
{
  GtkSvgWidget *self = GTK_SVG_WIDGET (widget);
  int width, height;
  GtkCssStyle *style;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  style = gtk_css_node_get_style (gtk_widget_get_css_node (widget));

  gtk_css_style_snapshot_icon_paintable (style,
                                         snapshot,
                                         GDK_PAINTABLE (self->svg),
                                         width, height);
}

static gboolean
gtk_svg_widget_query_tooltip (GtkWidget  *widget,
                              int         x,
                              int         y,
                              gboolean    keyboard_mode,
                              GtkTooltip *tooltip)
{
  GtkSvgWidget *self = GTK_SVG_WIDGET (widget);
  SvgElement *target;

  target = gtk_svg_pick_element (self->svg, &GRAPHENE_POINT_INIT (x, y));
  while (target)
    {
      if (svg_element_get_title (target))
        {
          gtk_tooltip_set_text (tooltip, svg_element_get_title (target));
          return TRUE;
        }
      target = svg_element_get_parent (target);
    }

  return FALSE;
}

static void
gtk_svg_widget_class_init (GtkSvgWidgetClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = gtk_svg_widget_dispose;
  object_class->get_property = gtk_svg_widget_get_property;
  object_class->set_property = gtk_svg_widget_set_property;

  widget_class->realize = gtk_svg_widget_realize;
  widget_class->measure = gtk_svg_widget_measure;
  widget_class->snapshot = gtk_svg_widget_snapshot;
  widget_class->focus = gtk_svg_widget_focus;
  widget_class->query_tooltip = gtk_svg_widget_query_tooltip;

  /**
   * GtkSvgWidget:resource:
   *
   * Resource to load SVG data from.
   *
   * This property is meant for use in ui files.
   *
   * Since: 4.24
   */
  properties[PROP_RESOURCE] =
    g_param_spec_string ("resource", NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_NAME);

  /**
   * GtkSvgWidget:state:
   *
   * The current state of the renderer.
   *
   * This can be a number between 0 and 63.
   *
   * Since: 4.24
   */
  properties[PROP_STATE] =
    g_param_spec_uint ("state", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkSvgWidget:stylesheet:
   *
   * A CSS stylesheet to apply to the SVG.
   *
   * Since: 4.24
   */
  properties[PROP_STYLESHEET] =
    g_param_spec_boxed ("stylesheet", NULL, NULL,
                        G_TYPE_BYTES,
                        G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  /**
   * GtkSvgWidget::error:
   * @self: the widget
   * @error: the error
   *
   * Signals that an error occurred.
   *
   * Errors can occur both during parsing and during rendering.
   *
   * The expected error values are in the [error@Gtk.SvgError] enumeration,
   * context information about the location of parsing errors can
   * be obtained with the various `gtk_svg_error` functions.
   *
   * Parsing errors are never fatal, so the parsing will resume after
   * the error. Errors may however cause parts of the given data or
   * even all of it to not be parsed at all. So it is a useful idea
   * to check that the parsing succeeds by connecting to this signal.
   *
   * ::: note
   *     This signal is emitted in the middle of parsing or rendering,
   *     and if you handle it, you must be careful. Logging the errors
   *     you receive is fine, but modifying the widget hierarchy or
   *     changing the paintable state definitively isn't.
   *
   *     If in doubt, defer to an idle.
   *
   * Since: 4.24
   */
  error_signal =
    g_signal_new ("error",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__BOXED,
                  G_TYPE_NONE, 1, G_TYPE_ERROR);
  g_signal_set_va_marshaller (error_signal,
                              G_TYPE_FROM_CLASS (object_class),
                              g_cclosure_marshal_VOID__BOXEDv);

  /**
   * GtkSvgWidget::activate:
   * @self: the SVG widget
   * @id: (nullable): the ID of the element
   * @href: (nullable): the href, if the element is a link
   *
   * Emitted when a link or other element is activated.
   *
   * Activating elements can be achieved by clicking
   * or by hitting <kbd>Enter</kbd> while the element
   * has focus.
   *
   * Since: 4.24
   */
  activate_signal =
    g_signal_new ("activate",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  _gtk_marshal_VOID__STRING_STRING,
                  G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);
  g_signal_set_va_marshaller (activate_signal,
                              G_TYPE_FROM_CLASS (object_class),
                              _gtk_marshal_VOID__STRING_STRINGv);

  gtk_widget_class_set_css_name (widget_class, "svg");
}

/* }}} */
/* {{{ Public API */

/**
 * gtk_svg_widget_new:
 *
 * Creates a new, empty `GtkSvgWidget`.
 *
 * Returns: the new widget
 *
 * Since: 4.24
 */
GtkSvgWidget *
gtk_svg_widget_new (void)
{
  return g_object_new (GTK_TYPE_SVG_WIDGET, NULL);
}

/**
 * gtk_svg_widget_load_from_bytes:
 * @self: an SVG widget
 * @bytes: the data to data
 *
 * Loads SVG content into an existing SVG widget.
 *
 * To track errors while loading SVG content, connect
 * to the [signal@Gtk.SvgWidget::error] signal.
 *
 * This clears any previously loaded content.
 *
 * Since: 4.24
 */
void
gtk_svg_widget_load_from_bytes (GtkSvgWidget *self,
                                GBytes       *bytes)
{
  g_return_if_fail (GTK_IS_SVG_WIDGET (self));

  gtk_svg_load_from_bytes (self->svg, bytes);
}

/**
 * gtk_svg_widget_set_stylesheet:
 * @self: an SVG widget
 * @bytes: (nullable): CSS data
 *
 * Sets a CSS user stylesheet to use.
 *
 * Note that styles are applied at load time, so this
 * function must be called before loading SVG.
 *
 * Since: 4.24
 */
void
gtk_svg_widget_set_stylesheet (GtkSvgWidget *self,
                               GBytes       *bytes)
{
  g_return_if_fail (GTK_IS_SVG_WIDGET (self));

  gtk_svg_set_stylesheet (self->svg, bytes);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STYLESHEET]);
}

/**
 * gtk_svg_widget_get_stylesheet:
 * @self: an SVG widget
 *
 * Gets the CSS user stylesheet.
 *
 * Returns: (nullable): a `GBytes` with the CSS data
 *
 * Since: 4.24
 */
GBytes *
gtk_svg_widget_get_stylesheet (GtkSvgWidget *self)
{
  g_return_val_if_fail (GTK_IS_SVG_WIDGET (self), NULL);

  return gtk_svg_get_stylesheet (self->svg);
}

/**
 * gtk_svg_widget_set_state:
 * @self: an SVG widget
 * @state: the state to set, as a value between 0 and 63
 *
 * Sets the state of the widget.
 *
 * The state change will apply transitions that are defined
 * in the SVG. See [class@Gtk.Svg] for details about states
 * and transitions.
 *
 * Since: 4.24
 */
void
gtk_svg_widget_set_state (GtkSvgWidget *self,
                          unsigned int  state)
{
  g_return_if_fail (GTK_IS_SVG_WIDGET (self));

  if (state == gtk_svg_get_state (self->svg))
    return;

  gtk_svg_set_state (self->svg, state);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STATE]);
}

/**
 * gtk_svg_widget_get_state:
 * @self: an SVG widget
 *
 * Gets the current state of the widget.
 *
 * Returns: the state
 *
 * Since: 4.24
 */
unsigned int
gtk_svg_widget_get_state (GtkSvgWidget *self)
{
  g_return_val_if_fail (GTK_IS_SVG_WIDGET (self), 0);

  return gtk_svg_get_state (self->svg);
}

/* }}} */

/* vim:set foldmethod=marker: */
