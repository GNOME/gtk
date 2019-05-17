/* GTK - The GIMP Toolkit
 * Copyright (C) 2019 Red Hat, Inc.
 *
 * Authors:
 * - Matthias Clasen <mclasen@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:gtkpopover
 * @Short_description: Context dependent bubbles
 * @Title: GtkPopover
 *
 * GtkPopover is a bubble-like context window, primarily meant to
 * provide context-dependent information or options. Popovers are
 * attached to a widget, passed at construction time on gtk_popover_new(),
 * or updated afterwards through gtk_popover_set_relative_to(), by
 * default they will point to the whole widget area, although this
 * behavior can be changed through gtk_popover_set_pointing_to().
 *
 * The position of a popover relative to the widget it is attached to
 * can also be changed through gtk_popover_set_position().
 *
 * By default, #GtkPopover performs a GTK+ grab, in order to ensure
 * input events get redirected to it while it is shown, and also so
 * the popover is dismissed in the expected situations (clicks outside
 * the popover, or the Esc key being pressed). If no such modal behavior
 * is desired on a popover, gtk_popover_set_modal() may be called on it
 * to tweak its behavior.
 *
 * ## GtkPopover as menu replacement
 *
 * GtkPopover is often used to replace menus. To facilitate this, it
 * supports being populated from a #GMenuModel, using
 * gtk_popover_new_from_model(). In addition to all the regular menu
 * model features, this function supports rendering sections in the
 * model in a more compact form, as a row of icon buttons instead of
 * menu items.
 *
 * To use this rendering, set the ”display-hint” attribute of the
 * section to ”horizontal-buttons” and set the icons of your items
 * with the ”verb-icon” attribute.
 *
 * |[
 * <section>
 *   <attribute name="display-hint">horizontal-buttons</attribute>
 *   <item>
 *     <attribute name="label">Cut</attribute>
 *     <attribute name="action">app.cut</attribute>
 *     <attribute name="verb-icon">edit-cut-symbolic</attribute>
 *   </item>
 *   <item>
 *     <attribute name="label">Copy</attribute>
 *     <attribute name="action">app.copy</attribute>
 *     <attribute name="verb-icon">edit-copy-symbolic</attribute>
 *   </item>
 *   <item>
 *     <attribute name="label">Paste</attribute>
 *     <attribute name="action">app.paste</attribute>
 *     <attribute name="verb-icon">edit-paste-symbolic</attribute>
 *   </item>
 * </section>
 * ]|
 *
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * popover
 * ├── arrow
 * ╰── contents.background[.menu]
 *     ╰── <child>
 * ]|
 *
 * The contents child node always gets the .background style class and it
 * gets the .menu style class if the popover is menu-like (e.g. #GtkPopoverMenu
 * or created using gtk_popover_new_from_model()).
 *
 * Particular uses of GtkPopover, such as touch selection popups
 * or magnifiers in #GtkEntry or #GtkTextView get style classes
 * like .touch-selection or .magnifier to differentiate from
 * plain popovers.
 *
 * When styling a popover directly, the popover node should usually not have any
 * background.
 *
 * Note that, in order to accomplish appropriate arrow visuals, #GtkPopover uses
 * custom drawing for the arrow node. This makes it possible for the arrow to change
 * its shape dynamically, but it also limits the possibilities of styling it using CSS.
 * In particular, the arrow gets drawn over the content node's border so they look
 * like one shape, which means that the border-width of the content node and the arrow
 * node should be the same. The arrow also does not support any border shape other than
 * solid, no border-radius, only one border width (border-bottom-width is used) and no box-shadow.
 */

#include "config.h"

#include "gtkpopoverprivate.h"
#include "gtknative.h"
#include "gtkwidgetprivate.h"
#include "gtkeventcontrollerkey.h"
#include "gtkcssnodeprivate.h"
#include "gtkbindings.h"
#include "gtkenums.h"
#include "gtktypebuiltins.h"
#include "gtkmnemonichash.h"
#include "gtkgizmoprivate.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkmain.h"
#include "gtkstack.h"
#include "gtkmenusectionbox.h"
#include "gdk/gdkeventsprivate.h"
#include "gtkpointerfocusprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtksnapshot.h"

#include "gtkrender.h"
#include "gtkstylecontextprivate.h"
#include "gtkroundedboxprivate.h"
#include "gsk/gskroundedrectprivate.h"

#ifdef GDK_WINDOWING_WAYLAND
#include "wayland/gdkwayland.h"
#endif

#define TAIL_GAP_WIDTH  24
#define TAIL_HEIGHT     12

#define POS_IS_VERTICAL(p) ((p) == GTK_POS_TOP || (p) == GTK_POS_BOTTOM)

static GListStore *popover_list = NULL;

typedef struct {
  GdkSurface *surface;
  GskRenderer *renderer;
  GtkWidget *default_widget;

  GdkSurfaceState state;
  GtkWidget *relative_to;
  GdkRectangle pointing_to;
  gboolean has_pointing_to;
  guint surface_transform_changed_cb;
  GtkPositionType position;
  gboolean modal;

  GtkWidget *contents_widget;
  GtkCssNode *arrow_node;

  GdkRectangle final_rect;
  GtkPositionType final_position;
} GtkPopoverPrivate;

enum {
  CLOSED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

enum {
  PROP_RELATIVE_TO = 1,
  PROP_POINTING_TO,
  PROP_POSITION,
  PROP_MODAL,
  PROP_DEFAULT_WIDGET,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL };

static void gtk_popover_native_interface_init (GtkNativeInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkPopover, gtk_popover, GTK_TYPE_BIN,
                         G_ADD_PRIVATE (GtkPopover)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_NATIVE,
                                                gtk_popover_native_interface_init))


static GdkSurface *
gtk_popover_native_get_surface (GtkNative *native)
{
  GtkPopover *popover = GTK_POPOVER (native);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  return priv->surface;
}

static GskRenderer *
gtk_popover_native_get_renderer (GtkNative *native)
{
  GtkPopover *popover = GTK_POPOVER (native);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  return priv->renderer;
}

static void
gtk_popover_native_get_surface_transform (GtkNative *native,
                                          int       *x,
                                          int       *y)
{
  GtkStyleContext *context;
  GtkBorder margin, border, padding;

  context = gtk_widget_get_style_context (GTK_WIDGET (native));
  gtk_style_context_get_margin (context, &margin);
  gtk_style_context_get_border (context, &border);
  gtk_style_context_get_padding (context, &padding);

  *x = margin.left + border.left + padding.left;
  *y = margin.top + border.top + padding.top;
}

static void
move_to_rect (GtkPopover *popover)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  GdkRectangle rect;
  GdkGravity parent_anchor;
  GdkGravity surface_anchor;
  GdkAnchorHints anchor_hints;

  gtk_widget_get_surface_allocation (priv->relative_to, &rect);
  if (priv->has_pointing_to)
    {
      rect.x += priv->pointing_to.x;
      rect.y += priv->pointing_to.y;
      rect.width = priv->pointing_to.width;
      rect.height = priv->pointing_to.height;
    }

  switch (priv->position)
    {
    case GTK_POS_LEFT:
      parent_anchor = GDK_GRAVITY_WEST;
      surface_anchor = GDK_GRAVITY_EAST;
      anchor_hints = GDK_ANCHOR_FLIP_X | GDK_ANCHOR_SLIDE_Y;
      break;

    case GTK_POS_RIGHT:
      parent_anchor = GDK_GRAVITY_EAST;
      surface_anchor = GDK_GRAVITY_WEST;
      anchor_hints = GDK_ANCHOR_FLIP_X | GDK_ANCHOR_SLIDE_Y;
      break;

    case GTK_POS_TOP:
      parent_anchor = GDK_GRAVITY_NORTH;
      surface_anchor = GDK_GRAVITY_SOUTH;
      anchor_hints = GDK_ANCHOR_FLIP_Y | GDK_ANCHOR_SLIDE_X;
      break;

    case GTK_POS_BOTTOM:
      parent_anchor = GDK_GRAVITY_SOUTH;
      surface_anchor = GDK_GRAVITY_NORTH;
      anchor_hints = GDK_ANCHOR_FLIP_Y | GDK_ANCHOR_SLIDE_X;
      break;

    default:
      g_assert_not_reached ();
    }

  gdk_surface_move_to_rect (priv->surface,
                            &rect,
                            parent_anchor,
                            surface_anchor,
                            anchor_hints,
                            0, 0);
}

static void
gtk_popover_move_resize (GtkPopover *popover)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  GtkRequisition req;

  if (priv->surface)
    {
      gtk_widget_get_preferred_size (GTK_WIDGET (popover), NULL, &req);
      gdk_surface_resize (priv->surface, req.width, req.height);
      move_to_rect (popover);
    }
}

static void
gtk_popover_native_check_resize (GtkNative *native)
{
  GtkPopover *popover = GTK_POPOVER (native);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  GtkWidget *widget = GTK_WIDGET (popover);

  if (!_gtk_widget_get_alloc_needed (widget))
    gtk_widget_ensure_allocate (widget);
  else if (gtk_widget_get_visible (widget))
    {
      gtk_popover_move_resize (popover);
      if (priv->surface)
        gtk_widget_allocate (GTK_WIDGET (popover),
                             gdk_surface_get_width (priv->surface),
                             gdk_surface_get_height (priv->surface),
                             -1, NULL);
    }
}


static void
gtk_popover_focus_in (GtkWidget *widget)
{
}

static void
gtk_popover_focus_out (GtkWidget *widget)
{
}

static gboolean
gtk_popover_key_pressed (GtkWidget       *widget,
                         guint            keyval,
                         guint            keycode,
                         GdkModifierType  state)
{
  if (keyval == GDK_KEY_Escape)
    {
      gtk_popover_popdown (GTK_POPOVER (widget));
      return TRUE;
    }

  return FALSE;
}

static void
ensure_state_flag_backdrop (GtkWidget *widget)
{
  GtkPopover *popover = GTK_POPOVER (widget);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  if ((priv->state & GDK_SURFACE_STATE_FOCUSED) != 0)
    gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_BACKDROP);
  else
    gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_BACKDROP, FALSE);
}

static void
surface_state_changed (GtkWidget *widget)
{
  GtkPopover *popover = GTK_POPOVER (widget);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  GdkSurfaceState new_surface_state;
  GdkSurfaceState changed_mask;

  new_surface_state = gdk_surface_get_state (_gtk_widget_get_surface (widget));
  changed_mask = new_surface_state ^ priv->state;
  priv->state = new_surface_state;

  if (changed_mask & GDK_SURFACE_STATE_FOCUSED)
    ensure_state_flag_backdrop (widget);

  if (changed_mask & GDK_SURFACE_STATE_WITHDRAWN)
    {
      if (priv->state & GDK_SURFACE_STATE_WITHDRAWN)
        gtk_widget_hide (widget);
    }
}

static void
surface_size_changed (GtkWidget *widget,
                      guint      width,
                      guint      height)
{
}

static gboolean
surface_render (GdkSurface     *surface,
                cairo_region_t *region,
                GtkWidget      *widget)
{
  gtk_widget_render (widget, surface, region);
  return TRUE;
}

static gboolean
surface_event (GdkSurface *surface,
               GdkEvent   *event,
               GtkWidget  *widget)
{
  gtk_main_do_event (event);
  return TRUE;
}

static void
surface_moved_to_rect (GdkSurface   *surface,
                       GdkRectangle *flipped_rect,
                       GdkRectangle *final_rect,
                       gboolean      flipped_x,
                       gboolean      flipped_y,
                       GtkWidget    *widget)
{
  GtkPopover *popover = GTK_POPOVER (widget);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  priv->final_rect = *final_rect;

  switch (priv->position)
    {
    case GTK_POS_LEFT:
      priv->final_position = flipped_x ? GTK_POS_RIGHT : GTK_POS_LEFT;
      break;
    case GTK_POS_RIGHT:
      priv->final_position = flipped_x ? GTK_POS_LEFT : GTK_POS_RIGHT;
      break;
    case GTK_POS_TOP:
      priv->final_position = flipped_y ? GTK_POS_BOTTOM : GTK_POS_TOP;
      break;
    case GTK_POS_BOTTOM:
      priv->final_position = flipped_y ? GTK_POS_TOP : GTK_POS_BOTTOM;
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  if (priv->final_position != priv->position)
    gtk_widget_queue_allocate (widget);
}

static void
measure_contents (GtkGizmo       *gizmo,
                  GtkOrientation  orientation,
                  int             for_size,
                  int            *minimum,
                  int            *natural,
                  int            *minimum_baseline,
                  int            *natural_baseline)
{
  GtkPopover *popover = GTK_POPOVER (gtk_widget_get_parent (GTK_WIDGET (gizmo)));
  GtkWidget *child = gtk_bin_get_child (GTK_BIN (popover));

  if (child)
    gtk_widget_measure (child, orientation, for_size,
                        minimum, natural,
                        minimum_baseline, natural_baseline);
}

static void
allocate_contents (GtkGizmo *gizmo,
                   int       width,
                   int       height,
                   int       baseline)
{
  GtkPopover *popover = GTK_POPOVER (gtk_widget_get_parent (GTK_WIDGET (gizmo)));
  GtkWidget *child = gtk_bin_get_child (GTK_BIN (popover));

  if (child)
    gtk_widget_size_allocate (child,
                              &(GtkAllocation) { 0, 0, width, height
                              }, -1);
}

static void
activate_default_cb (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       data)
{
  GtkPopover *popover = data;
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  GtkWidget *focus_widget;

  focus_widget = gtk_window_get_focus (GTK_WINDOW (gtk_widget_get_root (priv->relative_to)));
  if (!gtk_widget_is_ancestor (focus_widget, GTK_WIDGET (popover)))
    focus_widget = NULL;

  if (priv->default_widget && gtk_widget_is_sensitive (priv->default_widget) &&
      (!focus_widget || !gtk_widget_get_receives_default (focus_widget)
))
    gtk_widget_activate (priv->default_widget);
  else if (focus_widget && gtk_widget_is_sensitive (focus_widget))
    gtk_widget_activate (focus_widget);
}

static void
add_actions (GtkPopover *popover)
{
  GActionEntry entries[] = {
    { "activate", activate_default_cb, NULL, NULL, NULL },
  };

  GActionGroup *actions;

  actions = G_ACTION_GROUP (g_simple_action_group_new ());
  g_action_map_add_action_entries (G_ACTION_MAP (actions),
                                   entries, G_N_ELEMENTS (entries),
                                   popover);
  gtk_widget_insert_action_group (GTK_WIDGET (popover), "default", actions);
  g_object_unref (actions);
}

static void
node_style_changed_cb (GtkCssNode        *node,
                       GtkCssStyleChange *change,
                       GtkWidget         *widget)
{
  if (gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_SIZE))
    gtk_widget_queue_resize (widget);
  else
    gtk_widget_queue_draw (widget);
}

static void
gtk_popover_init (GtkPopover *popover)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  GtkWidget *widget = GTK_WIDGET (popover);
  GtkEventController *controller;
  GtkStyleContext *context;

  priv->position = GTK_POS_TOP;
  priv->final_position = GTK_POS_TOP;
  priv->modal = TRUE;

  controller = gtk_event_controller_key_new ();
  g_signal_connect_swapped (controller, "focus-in", G_CALLBACK (gtk_popover_focus_in), popover);
  g_signal_connect_swapped (controller, "focus-out", G_CALLBACK (gtk_popover_focus_out), popover);
  g_signal_connect_swapped (controller, "key-pressed", G_CALLBACK (gtk_popover_key_pressed), popover);
  gtk_widget_add_controller (GTK_WIDGET (popover), controller);

  priv->arrow_node = gtk_css_node_new ();
  gtk_css_node_set_name (priv->arrow_node, I_("arrow"));
  gtk_css_node_set_parent (priv->arrow_node, gtk_widget_get_css_node (widget));
  gtk_css_node_set_state (priv->arrow_node,
                          gtk_css_node_get_state (gtk_widget_get_css_node (widget)));
  g_signal_connect_object (priv->arrow_node, "style-changed",
                           G_CALLBACK (node_style_changed_cb), popover, 0);
  g_object_unref (priv->arrow_node);

  priv->contents_widget = gtk_gizmo_new ("contents",
                                         measure_contents,
                                         allocate_contents,
                                         NULL,
                                         NULL);
  gtk_widget_set_parent (priv->contents_widget, GTK_WIDGET (popover));

  context = gtk_widget_get_style_context (GTK_WIDGET (popover));
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_BACKGROUND);

  add_actions (popover);
}

static void
gtk_popover_realize (GtkWidget *widget)
{
  GtkPopover *popover = GTK_POPOVER (widget);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  GdkRectangle parent_rect;
  GdkDisplay *display;

  gtk_widget_get_surface_allocation (priv->relative_to, &parent_rect);

  display = gtk_widget_get_display (priv->relative_to);

  priv->surface = gdk_surface_new_popup (display, gtk_widget_get_surface (priv->relative_to), priv->modal);

  gtk_widget_set_surface (widget, priv->surface);
  gdk_surface_set_widget (priv->surface, widget);

  g_signal_connect_swapped (priv->surface, "notify::state", G_CALLBACK (surface_state_changed), widget);
  g_signal_connect_swapped (priv->surface, "size-changed", G_CALLBACK (surface_size_changed), widget);
  g_signal_connect (priv->surface, "render", G_CALLBACK (surface_render), widget);
  g_signal_connect (priv->surface, "event", G_CALLBACK (surface_event), widget);
  g_signal_connect (priv->surface, "moved-to-rect", G_CALLBACK (surface_moved_to_rect), widget);

  GTK_WIDGET_CLASS (gtk_popover_parent_class)->realize (widget);

  priv->renderer = gsk_renderer_new_for_surface (priv->surface);
}

static void
gtk_popover_unrealize (GtkWidget *widget)
{
  GtkPopover *popover = GTK_POPOVER (widget);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  GTK_WIDGET_CLASS (gtk_popover_parent_class)->unrealize (widget);

  gsk_renderer_unrealize (priv->renderer);
  g_clear_object (&priv->renderer);

  g_signal_handlers_disconnect_by_func (priv->surface, surface_state_changed, widget);
  g_signal_handlers_disconnect_by_func (priv->surface, surface_size_changed, widget);
  g_signal_handlers_disconnect_by_func (priv->surface, surface_render, widget);
  g_signal_handlers_disconnect_by_func (priv->surface, surface_event, widget);
  g_signal_handlers_disconnect_by_func (priv->surface, surface_moved_to_rect, widget);
  gdk_surface_set_widget (priv->surface, NULL);
  gdk_surface_destroy (priv->surface);
  g_clear_object (&priv->surface);
}

static void
gtk_popover_move_focus (GtkWidget        *widget,
                        GtkDirectionType  dir)
{
  gtk_widget_child_focus (widget, dir);

  if (!gtk_widget_get_focus_child (widget))
    gtk_root_set_focus (GTK_ROOT (widget), NULL);
}

static void
gtk_popover_show (GtkWidget *widget)
{
  GtkPopover *popover = GTK_POPOVER (widget);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  _gtk_widget_set_visible_flag (widget, TRUE);
  gtk_css_node_validate (gtk_widget_get_css_node (widget));
  gtk_widget_realize (widget);
  gtk_popover_native_check_resize (GTK_NATIVE (widget));
  gtk_widget_map (widget);

  if (priv->modal)
    {
      if (!gtk_widget_get_focus_child (widget))
        gtk_widget_child_focus (widget, GTK_DIR_TAB_FORWARD);
    }
}

static void
gtk_popover_hide (GtkWidget *widget)
{
  _gtk_widget_set_visible_flag (widget, FALSE);
  gtk_widget_unmap (widget);
  g_signal_emit (widget, signals[CLOSED], 0);
}

static void
unset_surface_transform_changed_cb (gpointer data)
{
  GtkPopover *popover = data;
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  priv->surface_transform_changed_cb = 0;
}

static gboolean
surface_transform_changed_cb (GtkWidget               *widget,
                              const graphene_matrix_t *transform,
                              gpointer                 user_data)
{
  move_to_rect (GTK_POPOVER (user_data));

  return G_SOURCE_CONTINUE;
}

static void
gtk_popover_map (GtkWidget *widget)
{
  GtkPopover *popover = GTK_POPOVER (widget);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  GtkWidget *child;
  GdkRectangle parent_rect;

  gdk_surface_show (priv->surface);
  gtk_widget_get_surface_allocation (priv->relative_to, &parent_rect);
  move_to_rect (popover);

  priv->surface_transform_changed_cb =
    gtk_widget_add_surface_transform_changed_callback (priv->relative_to,
                                                       surface_transform_changed_cb,
                                                       popover,
                                                       unset_surface_transform_changed_cb);

  GTK_WIDGET_CLASS (gtk_popover_parent_class)->map (widget);

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child != NULL && gtk_widget_get_visible (child))
    gtk_widget_map (child);
}

static void
gtk_popover_unmap (GtkWidget *widget)
{
  GtkPopover *popover = GTK_POPOVER (widget);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  GtkWidget *child;

  gtk_widget_remove_surface_transform_changed_callback (priv->relative_to,
                                                        priv->surface_transform_changed_cb);
  priv->surface_transform_changed_cb = 0;

  GTK_WIDGET_CLASS (gtk_popover_parent_class)->unmap (widget);
  gdk_surface_hide (priv->surface);

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child != NULL)
    gtk_widget_unmap (child);
}

static void
gtk_popover_dispose (GObject *object)
{
  GtkPopover *popover = GTK_POPOVER (object);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  guint i;
  GtkWidget *child;

  for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (popover_list)); i++)
    {
      gpointer item = g_list_model_get_item (G_LIST_MODEL (popover_list), i);
      if (item == object)
        {
          g_list_store_remove (popover_list, i);
          break;
        }
      else
        g_object_unref (item);
    }

  child = gtk_bin_get_child (GTK_BIN (popover));

  if (child)
    {
      gtk_widget_unparent (child);
      _gtk_bin_set_child (GTK_BIN (popover), NULL);
    }

  g_clear_pointer (&priv->contents_widget, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_popover_parent_class)->dispose (object);
}

static void
gtk_popover_finalize (GObject *object)
{
  G_OBJECT_CLASS (gtk_popover_parent_class)->finalize (object);
}

static void
gtk_popover_constructed (GObject *object)
{
  g_list_store_append (popover_list, object);
  g_object_unref (object);
}

static void
gtk_popover_get_gap_coords (GtkPopover *popover,
                            gint       *initial_x_out,
                            gint       *initial_y_out,
                            gint       *tip_x_out,
                            gint       *tip_y_out,
                            gint       *final_x_out,
                            gint       *final_y_out)
{
  GtkWidget *widget = GTK_WIDGET (popover);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  GdkRectangle rect = { 0 };
  gint base, tip, tip_pos;
  gint initial_x, initial_y;
  gint tip_x, tip_y;
  gint final_x, final_y;
  GtkPositionType pos;
  gint border_radius;
  GtkStyleContext *context;
  GtkBorder border;
  int popover_width, popover_height;

  popover_width = gtk_widget_get_allocated_width (widget);
  popover_height = gtk_widget_get_allocated_height (widget);

  gtk_widget_get_surface_allocation (priv->relative_to, &rect);
  if (priv->has_pointing_to)
    {
      rect.x += priv->pointing_to.x;
      rect.y += priv->pointing_to.y;
      rect.width = priv->pointing_to.width;
      rect.height = priv->pointing_to.height;
    }

  rect.x -= priv->final_rect.x;
  rect.y -= priv->final_rect.y;

  context = gtk_widget_get_style_context (priv->contents_widget);

  pos = priv->final_position;

  gtk_style_context_get_border (context, &border);
  gtk_style_context_get (context,
                         GTK_STYLE_PROPERTY_BORDER_RADIUS, &border_radius,
                         NULL);

  if (pos == GTK_POS_BOTTOM || pos == GTK_POS_RIGHT)
    {
      tip = 0;
      base = TAIL_HEIGHT + border.top;
    }
  else if (pos == GTK_POS_TOP)
    {
      base = popover_height - border.bottom - TAIL_HEIGHT;
      tip = popover_height;
    }
  else if (pos == GTK_POS_LEFT)
    {
      base = popover_width - border.right - TAIL_HEIGHT;
      tip = popover_width;
    }
  else
    g_assert_not_reached ();

  if (POS_IS_VERTICAL (pos))
    {
      tip_pos = rect.x + (rect.width / 2);
      initial_x = CLAMP (tip_pos - TAIL_GAP_WIDTH / 2,
                         border_radius,
                         popover_width - TAIL_GAP_WIDTH - border_radius);
      initial_y = base;

      tip_x = CLAMP (tip_pos, 0, popover_width);
      tip_y = tip;

      final_x = CLAMP (tip_pos + TAIL_GAP_WIDTH / 2,
                       border_radius + TAIL_GAP_WIDTH,
                       popover_width - border_radius);
      final_y = base;
    }
  else
    {
      tip_pos = rect.y + (rect.height / 2);

      initial_x = base;
      initial_y = CLAMP (tip_pos - TAIL_GAP_WIDTH / 2,
                         border_radius,
                         popover_height - TAIL_GAP_WIDTH - border_radius);

      tip_x = tip;
      tip_y = CLAMP (tip_pos, 0, popover_height);

      final_x = base;
      final_y = CLAMP (tip_pos + TAIL_GAP_WIDTH / 2,
                       border_radius + TAIL_GAP_WIDTH,
                       popover_height - border_radius);
    }

  *initial_x_out = initial_x;
  *initial_y_out = initial_y;

  *tip_x_out = tip_x;
  *tip_y_out = tip_y;

  *final_x_out = final_x;
  *final_y_out = final_y;
}

static void
get_margin (GtkWidget *widget,
            GtkBorder *border)
{
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_get_margin (context, border);
}
static void
gtk_popover_get_rect_for_size (GtkPopover   *popover,
                               int           popover_width,
                               int           popover_height,
                               GdkRectangle *rect)
{
  GtkWidget *widget = GTK_WIDGET (popover);
  int x, y, w, h;
  GtkBorder margin;

  get_margin (widget, &margin);

  x = 0;
  y = 0;
  w = popover_width;
  h = popover_height;

  x += MAX (TAIL_HEIGHT, margin.left);
  y += MAX (TAIL_HEIGHT, margin.top);
  w -= x + MAX (TAIL_HEIGHT, margin.right);
  h -= y + MAX (TAIL_HEIGHT, margin.bottom);

  rect->x = x;
  rect->y = y;
  rect->width = w;
  rect->height = h;
}

static void
gtk_popover_get_rect_coords (GtkPopover *popover,
                             int        *x_out,
                             int        *y_out,
                             int        *w_out,
                             int        *h_out)
{
  GtkWidget *widget = GTK_WIDGET (popover);
  GdkRectangle rect;
  GtkAllocation allocation;

  gtk_widget_get_allocation (widget, &allocation);
  gtk_popover_get_rect_for_size (popover, allocation.width, allocation.height, &rect);

  *x_out = rect.x;
  *y_out = rect.y;
  *w_out = rect.width;
  *h_out = rect.height;
}

static void
gtk_popover_apply_tail_path (GtkPopover *popover,
                             cairo_t    *cr)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  gint initial_x, initial_y;
  gint tip_x, tip_y;
  gint final_x, final_y;
  GtkStyleContext *context;
  GtkBorder border;

  if (!priv->relative_to)
    return;

  context = gtk_widget_get_style_context (priv->contents_widget);
  gtk_style_context_get_border (context, &border);

  cairo_set_line_width (cr, 1);
  gtk_popover_get_gap_coords (popover,
                              &initial_x, &initial_y,
                              &tip_x, &tip_y,
                              &final_x, &final_y);

  cairo_move_to (cr, initial_x, initial_y);
  cairo_line_to (cr, tip_x, tip_y);
  cairo_line_to (cr, final_x, final_y);
}

static void
gtk_popover_fill_border_path (GtkPopover *popover,
                              cairo_t    *cr)
{
  GtkWidget *widget = GTK_WIDGET (popover);
  GtkAllocation allocation;
  GtkStyleContext *context;
  int x, y, w, h;
  GskRoundedRect box;

  context = gtk_widget_get_style_context (widget);
  gtk_widget_get_allocation (widget, &allocation);

  cairo_set_source_rgba (cr, 0, 0, 0, 1);

  gtk_popover_apply_tail_path (popover, cr);
  cairo_close_path (cr);
  cairo_fill (cr);

  gtk_popover_get_rect_coords (popover, &x, &y, &w, &h);

  gtk_rounded_boxes_init_for_style (&box,
                                    NULL, NULL,
                                    gtk_style_context_lookup_style (context),
                                    x, y, w, h);
  gsk_rounded_rect_path (&box, cr);
  cairo_fill (cr);
}

static void
gtk_popover_update_shape (GtkPopover *popover)
{
  GtkWidget *widget = GTK_WIDGET (popover);
  cairo_surface_t *cairo_surface;
  cairo_region_t *region;
  GdkSurface *surface;
  cairo_t *cr;

#ifdef GDK_WINDOWING_WAYLAND 
  if (GDK_IS_WAYLAND_DISPLAY (gtk_widget_get_display (widget)))
    return;
#endif

  surface = gtk_widget_get_surface (widget);
  cairo_surface =
    gdk_surface_create_similar_surface (surface,
                                       CAIRO_CONTENT_COLOR_ALPHA,
                                       gdk_surface_get_width (surface),
                                       gdk_surface_get_height (surface));

  cr = cairo_create (cairo_surface);
  gtk_popover_fill_border_path (popover, cr);
  cairo_destroy (cr);

  region = gdk_cairo_region_create_from_surface (cairo_surface);
  cairo_surface_destroy (cairo_surface);

  gtk_widget_input_shape_combine_region (widget, region);
  cairo_region_destroy (region);
}

static gint
get_border_radius (GtkWidget *widget)
{
  GtkStyleContext *context;
  gint border_radius;

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_get (context,
                         GTK_STYLE_PROPERTY_BORDER_RADIUS, &border_radius,
                         NULL);
  return border_radius;
}

static gint
get_minimal_size (GtkPopover     *popover,
                  GtkOrientation  orientation)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  GtkPositionType pos;
  gint minimal_size;

  minimal_size = 2 * get_border_radius (GTK_WIDGET (popover));
  pos = priv->position;

  if ((orientation == GTK_ORIENTATION_HORIZONTAL && POS_IS_VERTICAL (pos)) ||
      (orientation == GTK_ORIENTATION_VERTICAL && !POS_IS_VERTICAL (pos)))
    minimal_size += TAIL_GAP_WIDTH;

  return minimal_size;
}

static void
gtk_popover_measure (GtkWidget      *widget,
                     GtkOrientation  orientation,
                     int             for_size,
                     int            *minimum,
                     int            *natural,
                     int            *minimum_baseline,
                     int            *natural_baseline)
{
  GtkPopover *popover = GTK_POPOVER (widget);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  int minimal_size;

  if (for_size >= 0)
    for_size -= TAIL_HEIGHT;

  gtk_widget_measure (priv->contents_widget,
                      orientation, for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);

  minimal_size = get_minimal_size (popover, orientation);
  *minimum = MAX (*minimum, minimal_size);
  *natural = MAX (*natural, minimal_size);

  *minimum += TAIL_HEIGHT;
  *natural += TAIL_HEIGHT;
}

static void
gtk_popover_size_allocate (GtkWidget *widget,
                           int        width,
                           int        height,
                           int        baseline)
{
  GtkPopover *popover = GTK_POPOVER (widget);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  GtkAllocation child_alloc;

  if (priv->surface)
    gtk_popover_move_resize (popover);

  switch (priv->final_position)
    {
    case GTK_POS_TOP:
      child_alloc.x = TAIL_HEIGHT / 2;
      child_alloc.y = 0;
      break;
    case GTK_POS_BOTTOM:
      child_alloc.x = TAIL_HEIGHT / 2;
      child_alloc.y = TAIL_HEIGHT;
      break;
    case GTK_POS_LEFT:
      child_alloc.x = 0;
      child_alloc.y = TAIL_HEIGHT / 2;
      break;
    case GTK_POS_RIGHT:
      child_alloc.x = TAIL_HEIGHT;
      child_alloc.y = TAIL_HEIGHT / 2;
      break;
    default:
      break;
    }

  child_alloc.width = width - TAIL_HEIGHT;
  child_alloc.height = height - TAIL_HEIGHT;

  gtk_widget_size_allocate (priv->contents_widget, &child_alloc, baseline);

  if (priv->surface)
    gtk_popover_update_shape (popover);
}

static void
gtk_popover_snapshot (GtkWidget   *widget,
                      GtkSnapshot *snapshot)
{
  GtkPopover *popover = GTK_POPOVER (widget);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  GtkStyleContext *context;
  GtkBorder border;
  cairo_t *cr;

  gtk_widget_snapshot_child (widget, priv->contents_widget, snapshot);

  cr = gtk_snapshot_append_cairo (snapshot,
                                  &GRAPHENE_RECT_INIT (
                                    0, 0,
                                    gtk_widget_get_width (widget),
                                    gtk_widget_get_height (widget)
                                  ));

    /* Clip to the arrow shape */
  cairo_save (cr);

  gtk_popover_apply_tail_path (popover, cr);

  cairo_clip (cr);

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_save_to_node (context, priv->arrow_node);
  gtk_style_context_get_border (context, &border);

  /* Render the arrow background */
  gtk_render_background (context, cr,
                         0, 0,
                         gtk_widget_get_width (widget),
                         gtk_widget_get_height (widget));

  /* Render the border of the arrow tip */
  if (border.bottom > 0)
    {
      GdkRGBA *border_color;

      gtk_style_context_get (context, "border-color", &border_color, NULL);
      gtk_popover_apply_tail_path (popover, cr);
      gdk_cairo_set_source_rgba (cr, border_color);

      cairo_set_line_width (cr, border.bottom + 1);
      cairo_stroke (cr);
      gdk_rgba_free (border_color);
    }

  cairo_restore (cr);
  cairo_destroy (cr);
  gtk_style_context_restore (context);
}

static void
gtk_popover_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GtkPopover *popover = GTK_POPOVER (object);

  switch (prop_id)
    {
    case PROP_RELATIVE_TO:
      gtk_popover_set_relative_to (popover, g_value_get_object (value));
      break;

    case PROP_POINTING_TO:
      gtk_popover_set_pointing_to (popover, g_value_get_boxed (value));
      break;

    case PROP_POSITION:
      gtk_popover_set_position (popover, g_value_get_enum (value));
      break;

    case PROP_MODAL:
      gtk_popover_set_modal (popover, g_value_get_boolean (value));
      break;

    case PROP_DEFAULT_WIDGET:
      gtk_popover_set_default_widget (popover, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_popover_get_property (GObject      *object,
                          guint         prop_id,
                          GValue       *value,
                          GParamSpec   *pspec)
{
  GtkPopover *popover = GTK_POPOVER (object);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  switch (prop_id)
    {
    case PROP_RELATIVE_TO:
      g_value_set_object (value, priv->relative_to);
      break;

    case PROP_POINTING_TO:
      g_value_set_boxed (value, &priv->pointing_to);
      break;

    case PROP_POSITION:
      g_value_set_enum (value, priv->position);
      break;

    case PROP_MODAL:
      g_value_set_boolean (value, priv->modal);
      break;

    case PROP_DEFAULT_WIDGET:
      g_value_set_object (value, priv->default_widget);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_popover_add (GtkContainer *container,
                 GtkWidget    *child)
{
  GtkPopover *popover = GTK_POPOVER (container);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  _gtk_bin_set_child (GTK_BIN (popover), child);
  gtk_widget_set_parent (child, priv->contents_widget);
}
 
static void
gtk_popover_remove (GtkContainer *container,
                    GtkWidget    *child)
{
  GtkPopover *popover = GTK_POPOVER (container);

  _gtk_bin_set_child (GTK_BIN (popover), NULL);
  gtk_widget_unparent (child);
}

static void
gtk_popover_class_init (GtkPopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  if (popover_list == NULL)
    popover_list = g_list_store_new (GTK_TYPE_WIDGET);

  object_class->constructed = gtk_popover_constructed;
  object_class->dispose = gtk_popover_dispose;
  object_class->finalize = gtk_popover_finalize;
  object_class->set_property = gtk_popover_set_property;
  object_class->get_property = gtk_popover_get_property;

  widget_class->realize = gtk_popover_realize;
  widget_class->unrealize = gtk_popover_unrealize;
  widget_class->map = gtk_popover_map;
  widget_class->unmap = gtk_popover_unmap;
  widget_class->show = gtk_popover_show;
  widget_class->hide = gtk_popover_hide;
  widget_class->measure = gtk_popover_measure;
  widget_class->size_allocate = gtk_popover_size_allocate;
  widget_class->snapshot = gtk_popover_snapshot;
  widget_class->move_focus = gtk_popover_move_focus;

  container_class->add = gtk_popover_add;
  container_class->remove = gtk_popover_remove;

  properties[PROP_RELATIVE_TO] =
      g_param_spec_object ("relative-to",
                           P_("Relative to"),
                           P_("Widget the bubble window points to"),
                           GTK_TYPE_WIDGET,
                           GTK_PARAM_READWRITE);

  properties[PROP_POINTING_TO] =
      g_param_spec_boxed ("pointing-to",
                          P_("Pointing to"),
                          P_("Rectangle the bubble window points to"),
                          GDK_TYPE_RECTANGLE,
                          GTK_PARAM_READWRITE);

  properties[PROP_POSITION] =
      g_param_spec_enum ("position",
                         P_("Position"),
                         P_("Position to place the bubble window"),
                         GTK_TYPE_POSITION_TYPE, GTK_POS_TOP,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_MODAL] =
      g_param_spec_boolean ("modal",
                            P_("Modal"),
                            P_("Whether the popover is modal"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_DEFAULT_WIDGET] =
      g_param_spec_object ("default-widget",
                           P_("Default widget"),
                           P_("The default widget"),
                           GTK_TYPE_WIDGET,
                           GTK_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  signals[CLOSED] =
    g_signal_new (I_("closed"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkPopoverClass, closed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);

  gtk_widget_class_set_css_name (widget_class, "popover");
}

GtkWidget *
gtk_popover_new (GtkWidget *relative_to)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_POPOVER,
                                   "relative-to", relative_to,
                                   NULL));
}

static void
size_changed (GtkWidget   *widget,
              int          width,
              int          height,
              int          baseline,
              GtkPopover  *popover)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  if (priv->surface)
    gtk_popover_move_resize (popover);
}

GListModel *
gtk_popover_get_popovers (void)
{
  if (popover_list == NULL)
    popover_list = g_list_store_new (GTK_TYPE_WIDGET);

  return G_LIST_MODEL (popover_list);
}

void
gtk_popover_set_default_widget (GtkPopover *popover,
                                GtkWidget  *widget)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  g_return_if_fail (GTK_IS_POPOVER (popover));

  if (priv->default_widget == widget)
    return;

  if (priv->default_widget)
    {
      _gtk_widget_set_has_default (priv->default_widget, FALSE);
      gtk_widget_queue_draw (priv->default_widget);
      g_object_notify (G_OBJECT (priv->default_widget), "has-default");
    }

  g_set_object (&priv->default_widget, widget);

  if (priv->default_widget)
    {
      _gtk_widget_set_has_default (priv->default_widget, TRUE);
      gtk_widget_queue_draw (priv->default_widget);
      g_object_notify (G_OBJECT (priv->default_widget), "has-default");
    }

  g_object_notify_by_pspec (G_OBJECT (popover), properties[PROP_DEFAULT_WIDGET]);
}

static void
gtk_popover_native_interface_init (GtkNativeInterface *iface)
{
  iface->get_surface = gtk_popover_native_get_surface;
  iface->get_renderer = gtk_popover_native_get_renderer;
  iface->get_surface_transform = gtk_popover_native_get_surface_transform;
  iface->check_resize = gtk_popover_native_check_resize;
}

/**
 * gtk_popover_set_relative_to:
 * @popover: a #GtkPopover
 * @relative_to: (allow-none): a #GtkWidget
 *
 * Sets a new widget to be attached to @popover. If @popover is
 * visible, the position will be updated.
 *
 * Note: the ownership of popovers is always given to their @relative_to
 * widget, so if @relative_to is set to %NULL on an attached @popover, it
 * will be detached from its previous widget, and consequently destroyed
 * unless extra references are kept.
 **/
void
gtk_popover_set_relative_to (GtkPopover *popover,
                             GtkWidget  *relative_to)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  
  g_return_if_fail (GTK_IS_POPOVER (popover));

  g_object_ref (popover);

  if (priv->relative_to)
    {
      g_signal_handlers_disconnect_by_func (priv->relative_to, size_changed, popover);
      gtk_widget_unparent (GTK_WIDGET (popover));
    }

  priv->relative_to = relative_to;

  if (priv->relative_to)
    {
      g_signal_connect (priv->relative_to, "size-allocate", G_CALLBACK (size_changed), popover);
      gtk_css_node_set_parent (gtk_widget_get_css_node (GTK_WIDGET (popover)),
                               gtk_widget_get_css_node (relative_to));
      gtk_widget_set_parent (GTK_WIDGET (popover), relative_to);
    }

  g_object_notify_by_pspec (G_OBJECT (popover), properties[PROP_RELATIVE_TO]);

  g_object_unref (popover);
}

/**
 * gtk_popover_get_relative_to:
 * @popover: a #GtkPopover
 *
 * Returns the widget @popover is currently attached to
 *
 * Returns: (transfer none): a #GtkWidget
 **/
GtkWidget *
gtk_popover_get_relative_to (GtkPopover *popover)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  g_return_val_if_fail (GTK_IS_POPOVER (popover), NULL);

  return priv->relative_to;
}

/**
 * gtk_popover_set_pointing_to:
 * @popover: a #GtkPopover
 * @rect: rectangle to point to
 *
 * Sets the rectangle that @popover will point to, in the
 * coordinate space of the widget @popover is attached to,
 * see gtk_popover_set_relative_to().
 **/
void
gtk_popover_set_pointing_to (GtkPopover         *popover,
                             const GdkRectangle *rect)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  g_return_if_fail (GTK_IS_POPOVER (popover));

  if (rect)
    {
      priv->pointing_to = *rect;
      priv->has_pointing_to = TRUE;
    }
  else
    priv->has_pointing_to = FALSE;

  g_object_notify_by_pspec (G_OBJECT (popover), properties[PROP_POINTING_TO]);
}

/**
 * gtk_popover_get_pointing_to:
 * @popover: a #GtkPopover
 * @rect: (out): location to store the rectangle
 *
 * If a rectangle to point to has been set, this function will
 * return %TRUE and fill in @rect with such rectangle, otherwise
 * it will return %FALSE and fill in @rect with the attached
 * widget coordinates.
 *
 * Returns: %TRUE if a rectangle to point to was set.
 **/
gboolean
gtk_popover_get_pointing_to (GtkPopover   *popover,
                             GdkRectangle *rect)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  g_return_val_if_fail (GTK_IS_POPOVER (popover), FALSE);
  g_return_val_if_fail (rect != NULL, FALSE);

  if (priv->has_pointing_to)
    *rect = priv->pointing_to;
  else if (priv->relative_to)
    {
      graphene_rect_t r;

      if (!gtk_widget_compute_bounds (priv->relative_to, priv->relative_to, &r))
        return FALSE;

      rect->x = floorf (r.origin.x);
      rect->y = floorf (r.origin.y);
      rect->width = ceilf (r.size.width);
      rect->height = ceilf (r.size.height);
    }

  return priv->has_pointing_to;
}

/**
 * gtk_popover_set_position:
 * @popover: a #GtkPopover
 * @position: preferred popover position
 *
 * Sets the preferred position for @popover to appear. If the @popover
 * is currently visible, it will be immediately updated.
 *
 * This preference will be respected where possible, although
 * on lack of space (eg. if close to the window edges), the
 * #GtkPopover may choose to appear on the opposite side
 **/
void
gtk_popover_set_position (GtkPopover      *popover,
                          GtkPositionType  position)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  g_return_if_fail (GTK_IS_POPOVER (popover));

  if (priv->position == position)
    return;

  priv->position = position;
  priv->final_position = position;

  g_object_notify_by_pspec (G_OBJECT (popover), properties[PROP_POSITION]);
}

/**
 * gtk_popover_get_position:
 * @popover: a #GtkPopover
 *
 * Returns the preferred position of @popover.
 *
 * Returns: The preferred position.
 **/
GtkPositionType
gtk_popover_get_position (GtkPopover *popover)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  g_return_val_if_fail (GTK_IS_POPOVER (popover), GTK_POS_TOP);

  return priv->position;
}

/**
 * gtk_popover_set_modal:
 * @popover: a #GtkPopover
 * @modal: #TRUE to make popover claim all input within the toplevel
 *
 * Sets whether @popover is modal, a modal popover will grab all input
 * within the toplevel and grab the keyboard focus on it when being
 * displayed. Clicking outside the popover area or pressing Esc will
 * dismiss the popover and ungrab input.
 **/
void
gtk_popover_set_modal (GtkPopover *popover,
                       gboolean    modal)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  g_return_if_fail (GTK_IS_POPOVER (popover));

  modal = modal != FALSE;

  if (priv->modal == modal)
    return;

  priv->modal = modal;

  g_object_notify_by_pspec (G_OBJECT (popover), properties[PROP_MODAL]);
}

/**
 * gtk_popover_get_modal:
 * @popover: a #GtkPopover
 *
 * Returns whether the popover is modal, see gtk_popover_set_modal to
 * see the implications of this.
 *
 * Returns: #TRUE if @popover is modal
 **/
gboolean
gtk_popover_get_modal (GtkPopover *popover)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  g_return_val_if_fail (GTK_IS_POPOVER (popover), FALSE);

  return priv->modal;
}

/**
 * gtk_popover_popup:
 * @popover: a #GtkPopover
 *
 * Pops @popover up. This is different than a gtk_widget_show() call
 * in that it shows the popover with a transition. If you want to show
 * the popover without a transition, use gtk_widget_show().
 */
void
gtk_popover_popup (GtkPopover *popover)
{
  g_return_if_fail (GTK_IS_POPOVER (popover));

  gtk_widget_show (GTK_WIDGET (popover));
}

/**
 * gtk_popover_popdown:
 * @popover: a #GtkPopover
 *
 * Pops @popover down.This is different than a gtk_widget_hide() call
 * in that it shows the popover with a transition. If you want to hide
 * the popover without a transition, use gtk_widget_hide().
 */
void
gtk_popover_popdown (GtkPopover *popover)
{
  g_return_if_fail (GTK_IS_POPOVER (popover));

  gtk_widget_hide (GTK_WIDGET (popover));
}

static void
back_to_main (GtkWidget *popover)
{
  GtkWidget *stack;

  stack = gtk_bin_get_child (GTK_BIN (popover));
  gtk_stack_set_visible_child_name (GTK_STACK (stack), "main");
}

/**
 * gtk_popover_bind_model:
 * @popover: a #GtkPopover
 * @model: (allow-none): the #GMenuModel to bind to or %NULL to remove
 *   binding
 * @action_namespace: (allow-none): the namespace for actions in @model
 *
 * Establishes a binding between a #GtkPopover and a #GMenuModel.
 *
 * The contents of @popover are removed and then refilled with menu items
 * according to @model.  When @model changes, @popover is updated.
 * Calling this function twice on @popover with different @model will
 * cause the first binding to be replaced with a binding to the new
 * model. If @model is %NULL then any previous binding is undone and
 * all children are removed.
 *
 * If @action_namespace is non-%NULL then the effect is as if all
 * actions mentioned in the @model have their names prefixed with the
 * namespace, plus a dot.  For example, if the action “quit” is
 * mentioned and @action_namespace is “app” then the effective action
 * name is “app.quit”.
 *
 * This function uses #GtkActionable to define the action name and
 * target values on the created menu items.  If you want to use an
 * action group other than “app” and “win”, or if you want to use a
 * #GtkMenuShell outside of a #GtkApplicationWindow, then you will need
 * to attach your own action group to the widget hierarchy using
 * gtk_widget_insert_action_group().  As an example, if you created a
 * group with a “quit” action and inserted it with the name “mygroup”
 * then you would use the action name “mygroup.quit” in your
 * #GMenuModel.
 */
void
gtk_popover_bind_model (GtkPopover  *popover,
                        GMenuModel  *model,
                        const gchar *action_namespace)
{
  GtkWidget *child;
  GtkWidget *stack;
  GtkStyleContext *style_context;

  g_return_if_fail (GTK_IS_POPOVER (popover));
  g_return_if_fail (model == NULL || G_IS_MENU_MODEL (model));

  child = gtk_bin_get_child (GTK_BIN (popover));
  if (child)
    gtk_widget_destroy (child);

  style_context = gtk_widget_get_style_context (GTK_WIDGET (popover));

  if (model)
    {
      stack = gtk_stack_new ();
      gtk_stack_set_vhomogeneous (GTK_STACK (stack), FALSE);
      gtk_stack_set_transition_type (GTK_STACK (stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
      gtk_stack_set_interpolate_size (GTK_STACK (stack), TRUE);
      gtk_container_add (GTK_CONTAINER (popover), stack);

      gtk_menu_section_box_new_toplevel (GTK_STACK (stack),
                                         model,
                                         action_namespace,
                                         popover);
      gtk_stack_set_visible_child_name (GTK_STACK (stack), "main");

      g_signal_connect (popover, "unmap", G_CALLBACK (back_to_main), NULL);
      g_signal_connect (popover, "map", G_CALLBACK (back_to_main), NULL);

      gtk_style_context_add_class (style_context, GTK_STYLE_CLASS_MENU);
    }
  else
    {
      gtk_style_context_remove_class (style_context, GTK_STYLE_CLASS_MENU);
    }
}

/**
 * gtk_popover_new_from_model:
 * @relative_to: (allow-none): #GtkWidget the popover is related to
 * @model: a #GMenuModel
 *
 * Creates a #GtkPopover and populates it according to
 * @model. The popover is pointed to the @relative_to widget.
 *
 * The created buttons are connected to actions found in the
 * #GtkApplicationWindow to which the popover belongs - typically
 * by means of being attached to a widget that is contained within
 * the #GtkApplicationWindows widget hierarchy.
 *
 * Actions can also be added using gtk_widget_insert_action_group()
 * on the menus attach widget or on any of its parent widgets.
 *
 * Returns: the new #GtkPopover
 */
GtkWidget *
gtk_popover_new_from_model (GtkWidget  *relative_to,
                            GMenuModel *model)
{
  GtkWidget *popover;

  g_return_val_if_fail (relative_to == NULL || GTK_IS_WIDGET (relative_to), NULL);
  g_return_val_if_fail (G_IS_MENU_MODEL (model), NULL);

  popover = gtk_popover_new (relative_to);
  gtk_popover_bind_model (GTK_POPOVER (popover), model, NULL);

  return popover;
}


GtkWidget *
gtk_popover_get_contents_widget (GtkPopover *popover)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  return priv->contents_widget;
}
