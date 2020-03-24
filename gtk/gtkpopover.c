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
 * attached to a widget, set with gtk_widget_set_parent(). By
 * default they will point to the whole widget area, although this
 * behavior can be changed through gtk_popover_set_pointing_to().
 *
 * The position of a popover relative to the widget it is attached to
 * can also be changed through gtk_popover_set_position().
 *
 * By default, #GtkPopover performs a grab, in order to ensure input events
 * get redirected to it while it is shown, and also so the popover is dismissed
 * in the expected situations (clicks outside the popover, or the Escape key
 * being pressed). If no such modal behavior is desired on a popover,
 * gtk_popover_set_autohide() may be called on it to tweak its behavior.
 *
 * ## GtkPopover as menu replacement
 *
 * GtkPopover is often used to replace menus. The best was to do this
 * is to use the #GtkPopoverMenu subclass which supports being populated
 * from a #GMenuModel with gtk_popover_menu_new_from_model().
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
 * popover[.menu]
 * ├── arrow
 * ╰── contents.background
 *     ╰── <child>
 * ]|
 *
 * The contents child node always gets the .background style class and
 * the popover itself gets the .menu style class if the popover is
 * menu-like (i.e. #GtkPopoverMenu).
 *
 * Particular uses of GtkPopover, such as touch selection popups or magnifiers
 * in #GtkEntry or #GtkTextView get style classes like .touch-selection or .magnifier
 * to differentiate from plain popovers.
 *
 * When styling a popover directly, the popover node should usually
 * not have any background.
 *
 * Note that, in order to accomplish appropriate arrow visuals, #GtkPopover uses
 * custom drawing for the arrow node. This makes it possible for the arrow to
 * change its shape dynamically, but it also limits the possibilities of styling
 * it using CSS. In particular, the arrow gets drawn over the content node's
 * border so they look like one shape, which means that the border-width of
 * the content node and the arrow node should be the same. The arrow also does
 * not support any border shape other than solid, no border-radius, only one
 * border width (border-bottom-width is used) and no box-shadow.
 */

#include "config.h"

#include "gtkpopoverprivate.h"
#include "gtkpopovermenuprivate.h"
#include "gtknative.h"
#include "gtkwidgetprivate.h"
#include "gtkeventcontrollerkey.h"
#include "gtkcssnodeprivate.h"
#include "gtkbinlayout.h"
#include "gtkenums.h"
#include "gtktypebuiltins.h"
#include "gtkgizmoprivate.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkmain.h"
#include "gtkstack.h"
#include "gtkmenusectionboxprivate.h"
#include "gdk/gdkeventsprivate.h"
#include "gtkpointerfocusprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkcsscolorvalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtksnapshot.h"
#include "gtkshortcutmanager.h"

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

typedef struct {
  GdkSurface *surface;
  GskRenderer *renderer;
  GtkWidget *default_widget;

  GdkRectangle pointing_to;
  gboolean has_pointing_to;
  guint surface_transform_changed_cb;
  GtkPositionType position;
  gboolean autohide;
  gboolean has_arrow;
  gboolean mnemonics_visible;

  GtkWidget *contents_widget;
  GtkCssNode *arrow_node;
  GskRenderNode *arrow_render_node;

  GdkPopupLayout *layout;
  GdkRectangle final_rect;
  GtkPositionType final_position;
} GtkPopoverPrivate;

enum {
  CLOSED,
  ACTIVATE_DEFAULT,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

enum {
  PROP_POINTING_TO = 1,
  PROP_POSITION,
  PROP_AUTOHIDE,
  PROP_DEFAULT_WIDGET,
  PROP_HAS_ARROW,
  PROP_MNEMONICS_VISIBLE,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL };

static void gtk_popover_shortcut_manager_interface_init (GtkShortcutManagerInterface *iface);
static void gtk_popover_native_interface_init (GtkNativeInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkPopover, gtk_popover, GTK_TYPE_BIN,
                         G_ADD_PRIVATE (GtkPopover)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SHORTCUT_MANAGER,
                                                gtk_popover_shortcut_manager_interface_init)
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
  GtkCssStyle *style;

  style = gtk_css_node_get_style (gtk_widget_get_css_node (GTK_WIDGET (native)));
  *x  = _gtk_css_number_value_get (style->size->margin_left, 100) +
        _gtk_css_number_value_get (style->border->border_left_width, 100) +
        _gtk_css_number_value_get (style->size->padding_left, 100);
  *y  = _gtk_css_number_value_get (style->size->margin_top, 100) +
        _gtk_css_number_value_get (style->border->border_top_width, 100) +
        _gtk_css_number_value_get (style->size->padding_top, 100);
}

static gboolean
is_gravity_facing_north (GdkGravity gravity)
{
  switch (gravity)
    {
    case GDK_GRAVITY_NORTH_EAST:
    case GDK_GRAVITY_NORTH:
    case GDK_GRAVITY_NORTH_WEST:
    case GDK_GRAVITY_STATIC:
      return TRUE;
    case GDK_GRAVITY_SOUTH_WEST:
    case GDK_GRAVITY_WEST:
    case GDK_GRAVITY_SOUTH_EAST:
    case GDK_GRAVITY_EAST:
    case GDK_GRAVITY_CENTER:
    case GDK_GRAVITY_SOUTH:
      return FALSE;
    default:
      g_assert_not_reached ();
    }
}

static gboolean
is_gravity_facing_south (GdkGravity gravity)
{
  switch (gravity)
    {
    case GDK_GRAVITY_SOUTH_WEST:
    case GDK_GRAVITY_SOUTH_EAST:
    case GDK_GRAVITY_SOUTH:
      return TRUE;
    case GDK_GRAVITY_NORTH_EAST:
    case GDK_GRAVITY_NORTH:
    case GDK_GRAVITY_NORTH_WEST:
    case GDK_GRAVITY_STATIC:
    case GDK_GRAVITY_WEST:
    case GDK_GRAVITY_EAST:
    case GDK_GRAVITY_CENTER:
      return FALSE;
    default:
      g_assert_not_reached ();
    }
}

static gboolean
is_gravity_facing_west (GdkGravity gravity)
{
  switch (gravity)
    {
    case GDK_GRAVITY_NORTH_WEST:
    case GDK_GRAVITY_STATIC:
    case GDK_GRAVITY_SOUTH_WEST:
    case GDK_GRAVITY_WEST:
      return TRUE;
    case GDK_GRAVITY_NORTH_EAST:
    case GDK_GRAVITY_SOUTH_EAST:
    case GDK_GRAVITY_EAST:
    case GDK_GRAVITY_NORTH:
    case GDK_GRAVITY_CENTER:
    case GDK_GRAVITY_SOUTH:
      return FALSE;
    default:
      g_assert_not_reached ();
    }
}

static gboolean
is_gravity_facing_east (GdkGravity gravity)
{
  switch (gravity)
    {
    case GDK_GRAVITY_NORTH_EAST:
    case GDK_GRAVITY_SOUTH_EAST:
    case GDK_GRAVITY_EAST:
      return TRUE;
    case GDK_GRAVITY_NORTH_WEST:
    case GDK_GRAVITY_STATIC:
    case GDK_GRAVITY_SOUTH_WEST:
    case GDK_GRAVITY_WEST:
    case GDK_GRAVITY_NORTH:
    case GDK_GRAVITY_CENTER:
    case GDK_GRAVITY_SOUTH:
      return FALSE;
    default:
      g_assert_not_reached ();
    }
}

static gboolean
did_flip_horizontally (GdkGravity original_gravity,
                       GdkGravity final_gravity)
{
  g_return_val_if_fail (original_gravity, FALSE);
  g_return_val_if_fail (final_gravity, FALSE);

  if (is_gravity_facing_east (original_gravity) &&
      is_gravity_facing_west (final_gravity))
    return TRUE;

  if (is_gravity_facing_west (original_gravity) &&
      is_gravity_facing_east (final_gravity))
    return TRUE;

  return FALSE;
}

static gboolean
did_flip_vertically (GdkGravity original_gravity,
                     GdkGravity final_gravity)
{
  g_return_val_if_fail (original_gravity, FALSE);
  g_return_val_if_fail (final_gravity, FALSE);

  if (is_gravity_facing_north (original_gravity) &&
      is_gravity_facing_south (final_gravity))
    return TRUE;

  if (is_gravity_facing_south (original_gravity) &&
      is_gravity_facing_north (final_gravity))
    return TRUE;

  return FALSE;
}

static void
update_popover_layout (GtkPopover     *popover,
                       GdkPopupLayout *layout)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  GdkRectangle final_rect;
  gboolean flipped_x;
  gboolean flipped_y;
  GdkPopup *popup = GDK_POPUP (priv->surface);

  g_clear_pointer (&priv->layout, gdk_popup_layout_unref);
  priv->layout = layout;

  final_rect = (GdkRectangle) {
    .x = gdk_popup_get_position_x (GDK_POPUP (priv->surface)),
    .y = gdk_popup_get_position_y (GDK_POPUP (priv->surface)),
    .width = gdk_surface_get_width (priv->surface),
    .height = gdk_surface_get_height (priv->surface),
  };

  flipped_x =
    did_flip_horizontally (gdk_popup_layout_get_rect_anchor (layout),
                           gdk_popup_get_rect_anchor (popup)) &&
    did_flip_horizontally (gdk_popup_layout_get_surface_anchor (layout),
                           gdk_popup_get_surface_anchor (popup));
  flipped_y =
    did_flip_vertically (gdk_popup_layout_get_rect_anchor (layout),
                         gdk_popup_get_rect_anchor (popup)) &&
    did_flip_vertically (gdk_popup_layout_get_surface_anchor (layout),
                         gdk_popup_get_surface_anchor (popup));

  gtk_widget_allocate (GTK_WIDGET (popover),
                       gdk_surface_get_width (priv->surface),
                       gdk_surface_get_height (priv->surface),
                       -1, NULL);

  priv->final_rect = final_rect;

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

  g_clear_pointer (&priv->arrow_render_node, gsk_render_node_unref);
  gtk_widget_queue_draw (GTK_WIDGET (popover));
}

static GdkPopupLayout *
create_popup_layout (GtkPopover *popover)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  GdkRectangle rect;
  GdkGravity parent_anchor;
  GdkGravity surface_anchor;
  GdkAnchorHints anchor_hints;
  GdkPopupLayout *layout;
  GtkWidget *parent;

  parent = gtk_widget_get_parent (GTK_WIDGET (popover));
  gtk_widget_get_surface_allocation (parent, &rect);
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
      switch (gtk_widget_get_valign (GTK_WIDGET (popover)))
        {
        case GTK_ALIGN_START:
          parent_anchor = GDK_GRAVITY_NORTH_WEST;
          surface_anchor = GDK_GRAVITY_NORTH_EAST;
          break;

        case GTK_ALIGN_END:
          parent_anchor = GDK_GRAVITY_SOUTH_WEST;
          surface_anchor = GDK_GRAVITY_SOUTH_EAST;
          break;

        case GTK_ALIGN_FILL:
        case GTK_ALIGN_CENTER:
        case GTK_ALIGN_BASELINE:
        default:
          parent_anchor = GDK_GRAVITY_WEST;
          surface_anchor = GDK_GRAVITY_EAST;
          break;
        }
      anchor_hints = GDK_ANCHOR_FLIP_X | GDK_ANCHOR_SLIDE_Y;
      break;

    case GTK_POS_RIGHT:
      switch (gtk_widget_get_valign (GTK_WIDGET (popover)))
        {
        case GTK_ALIGN_START:
          parent_anchor = GDK_GRAVITY_NORTH_EAST;
          surface_anchor = GDK_GRAVITY_NORTH_WEST;
          break;

        case GTK_ALIGN_END:
          parent_anchor = GDK_GRAVITY_SOUTH_EAST;
          surface_anchor = GDK_GRAVITY_SOUTH_WEST;
          break;

        case GTK_ALIGN_FILL:
        case GTK_ALIGN_CENTER:
        case GTK_ALIGN_BASELINE:
        default:
          parent_anchor = GDK_GRAVITY_EAST;
          surface_anchor = GDK_GRAVITY_WEST;
          break;
        }
      anchor_hints = GDK_ANCHOR_FLIP_X | GDK_ANCHOR_SLIDE_Y;
      break;

    case GTK_POS_TOP:
      switch (gtk_widget_get_halign (GTK_WIDGET (popover)))
        {
        case GTK_ALIGN_START:
          parent_anchor = GDK_GRAVITY_NORTH_WEST;
          surface_anchor = GDK_GRAVITY_SOUTH_WEST;
          break;

        case GTK_ALIGN_END:
          parent_anchor = GDK_GRAVITY_NORTH_EAST;
          surface_anchor = GDK_GRAVITY_SOUTH_EAST;
          break;

        case GTK_ALIGN_FILL:
        case GTK_ALIGN_CENTER:
        case GTK_ALIGN_BASELINE:
        default:
          parent_anchor = GDK_GRAVITY_NORTH;
          surface_anchor = GDK_GRAVITY_SOUTH;
          break;
        }
      anchor_hints = GDK_ANCHOR_FLIP_Y | GDK_ANCHOR_SLIDE_X;
      break;

    case GTK_POS_BOTTOM:
      switch (gtk_widget_get_halign (GTK_WIDGET (popover)))
        {
        case GTK_ALIGN_START:
          parent_anchor = GDK_GRAVITY_SOUTH_WEST;
          surface_anchor = GDK_GRAVITY_NORTH_WEST;
          break;

        case GTK_ALIGN_END:
          parent_anchor = GDK_GRAVITY_SOUTH_EAST;
          surface_anchor = GDK_GRAVITY_NORTH_EAST;
          break;

        case GTK_ALIGN_FILL:
        case GTK_ALIGN_CENTER:
        case GTK_ALIGN_BASELINE:
        default:
          parent_anchor = GDK_GRAVITY_SOUTH;
          surface_anchor = GDK_GRAVITY_NORTH;
          break;
        }
      anchor_hints = GDK_ANCHOR_FLIP_Y | GDK_ANCHOR_SLIDE_X;
      break;

    default:
      g_assert_not_reached ();
    }

  layout = gdk_popup_layout_new (&rect,
                                 parent_anchor,
                                 surface_anchor);
  gdk_popup_layout_set_anchor_hints (layout, anchor_hints);

  return layout;
}

static void
present_popup (GtkPopover *popover)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  GtkRequisition req;
  GdkPopupLayout *layout;

  layout = create_popup_layout (popover);
  gtk_widget_get_preferred_size (GTK_WIDGET (popover), NULL, &req);
  if (gdk_popup_present (GDK_POPUP (priv->surface),
                                 MAX (req.width, 1),
                                 MAX (req.height, 1),
                                 layout))
    update_popover_layout (popover, layout);
}

static void
gtk_popover_native_check_resize (GtkNative *native)
{
  GtkPopover *popover = GTK_POPOVER (native);
  GtkWidget *widget = GTK_WIDGET (popover);

  if (!_gtk_widget_get_alloc_needed (widget))
    gtk_widget_ensure_allocate (widget);
  else if (gtk_widget_get_visible (widget))
    present_popup (popover);
}

static void
close_menu (GtkPopover *popover)
{
  while (popover)
    {
      gtk_popover_popdown (popover);
      if (GTK_IS_POPOVER_MENU (popover))
        popover = (GtkPopover *)gtk_popover_menu_get_parent_menu (GTK_POPOVER_MENU (popover));
      else
        popover = NULL;
    }
}

static gboolean
gtk_popover_key_pressed (GtkWidget       *widget,
                         guint            keyval,
                         guint            keycode,
                         GdkModifierType  state)
{
  if (keyval == GDK_KEY_Escape)
    {
      close_menu (GTK_POPOVER (widget));
      return TRUE;
    }

  return FALSE;
}

static void
surface_mapped_changed (GtkWidget *widget)
{
  GtkPopover *popover = GTK_POPOVER (widget);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  gtk_widget_set_visible (widget, gdk_surface_get_mapped (priv->surface));
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
popup_layout_changed (GdkSurface *surface,
                      GtkWidget  *widget)
{
  GtkPopover *popover = GTK_POPOVER (widget);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  update_popover_layout (popover, gdk_popup_layout_ref (priv->layout));
}

static void
gtk_popover_activate_default (GtkPopover *popover)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  GtkWidget *focus_widget;

  focus_widget = gtk_window_get_focus (GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (popover))));
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
activate_default_cb (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       data)
{
  gtk_popover_activate_default (GTK_POPOVER (data));
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
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (GTK_POPOVER (widget));
  g_clear_pointer (&priv->arrow_render_node, gsk_render_node_unref);

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

  priv->position = GTK_POS_BOTTOM;
  priv->final_position = GTK_POS_BOTTOM;
  priv->autohide = TRUE;
  priv->has_arrow = TRUE;

  controller = gtk_event_controller_key_new ();
  g_signal_connect_swapped (controller, "key-pressed", G_CALLBACK (gtk_popover_key_pressed), popover);
  gtk_widget_add_controller (GTK_WIDGET (popover), controller);

  priv->arrow_node = gtk_css_node_new ();
  gtk_css_node_set_name (priv->arrow_node, g_quark_from_static_string ("arrow"));
  gtk_css_node_set_parent (priv->arrow_node, gtk_widget_get_css_node (widget));
  gtk_css_node_set_state (priv->arrow_node,
                          gtk_css_node_get_state (gtk_widget_get_css_node (widget)));
  g_signal_connect_object (priv->arrow_node, "style-changed",
                           G_CALLBACK (node_style_changed_cb), popover, 0);
  g_object_unref (priv->arrow_node);

  priv->contents_widget = gtk_gizmo_new ("contents", NULL, NULL, NULL, NULL);
  gtk_widget_set_layout_manager (priv->contents_widget, gtk_bin_layout_new ());
  gtk_widget_set_parent (priv->contents_widget, GTK_WIDGET (popover));

  gtk_css_node_add_class (gtk_widget_get_css_node (GTK_WIDGET (popover)),
                          g_quark_from_static_string (GTK_STYLE_CLASS_BACKGROUND));

  add_actions (popover);
}

static void
gtk_popover_realize (GtkWidget *widget)
{
  GtkPopover *popover = GTK_POPOVER (widget);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  GdkSurface *parent_surface;
  GtkWidget *parent;

  parent = gtk_widget_get_parent (widget);
  parent_surface = gtk_native_get_surface (gtk_widget_get_native (parent));
  priv->surface = gdk_surface_new_popup (parent_surface, priv->autohide);

  gdk_surface_set_widget (priv->surface, widget);

  g_signal_connect_swapped (priv->surface, "notify::mapped", G_CALLBACK (surface_mapped_changed), widget);
  g_signal_connect_swapped (priv->surface, "size-changed", G_CALLBACK (surface_size_changed), widget);
  g_signal_connect (priv->surface, "render", G_CALLBACK (surface_render), widget);
  g_signal_connect (priv->surface, "event", G_CALLBACK (surface_event), widget);
  g_signal_connect (priv->surface, "popup-layout-changed", G_CALLBACK (popup_layout_changed), widget);

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

  g_signal_handlers_disconnect_by_func (priv->surface, surface_mapped_changed, widget);
  g_signal_handlers_disconnect_by_func (priv->surface, surface_size_changed, widget);
  g_signal_handlers_disconnect_by_func (priv->surface, surface_render, widget);
  g_signal_handlers_disconnect_by_func (priv->surface, surface_event, widget);
  g_signal_handlers_disconnect_by_func (priv->surface, popup_layout_changed, widget);
  gdk_surface_set_widget (priv->surface, NULL);
  gdk_surface_destroy (priv->surface);
  g_clear_object (&priv->surface);
}

static void
gtk_popover_show (GtkWidget *widget)
{
  GtkPopover *popover = GTK_POPOVER (widget);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  _gtk_widget_set_visible_flag (widget, TRUE);
  gtk_widget_realize (widget);
  present_popup (popover);
  gtk_widget_map (widget);

  if (priv->autohide)
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
  GtkPopover *popover = user_data;
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  if (priv->surface && gdk_surface_get_mapped (priv->surface))
    present_popup (popover);

  return G_SOURCE_CONTINUE;
}

static void
gtk_popover_map (GtkWidget *widget)
{
  GtkPopover *popover = GTK_POPOVER (widget);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  GtkWidget *child;
  GtkWidget *parent;

  present_popup (popover);

  parent = gtk_widget_get_parent (widget);
  priv->surface_transform_changed_cb =
    gtk_widget_add_surface_transform_changed_callback (parent,
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
  GtkWidget *parent;

  parent = gtk_widget_get_parent (widget);
  gtk_widget_remove_surface_transform_changed_callback (parent,
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
  GtkWidget *child;

  child = gtk_bin_get_child (GTK_BIN (popover));

  if (child)
    {
      gtk_widget_unparent (child);
      _gtk_bin_set_child (GTK_BIN (popover), NULL);
    }

  g_clear_pointer (&priv->contents_widget, gtk_widget_unparent);
  g_clear_pointer (&priv->arrow_render_node, gsk_render_node_unref);

  G_OBJECT_CLASS (gtk_popover_parent_class)->dispose (object);
}

static void
gtk_popover_finalize (GObject *object)
{
  GtkPopover *popover = GTK_POPOVER (object);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  g_clear_pointer (&priv->layout, gdk_popup_layout_unref);

  G_OBJECT_CLASS (gtk_popover_parent_class)->finalize (object);
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
  int border_top, border_right, border_bottom;
  int border_radius;
  int popover_width, popover_height;
  GtkCssStyle *style;
  GtkWidget *parent;

  popover_width = gtk_widget_get_allocated_width (widget);
  popover_height = gtk_widget_get_allocated_height (widget);
  parent = gtk_widget_get_parent (widget);

  gtk_widget_get_surface_allocation (parent, &rect);
  if (priv->has_pointing_to)
    {
      rect.x += priv->pointing_to.x;
      rect.y += priv->pointing_to.y;
      rect.width = priv->pointing_to.width;
      rect.height = priv->pointing_to.height;
    }

  rect.x -= priv->final_rect.x;
  rect.y -= priv->final_rect.y;

  pos = priv->final_position;

  style = gtk_css_node_get_style (gtk_widget_get_css_node (priv->contents_widget));
  border_radius = _gtk_css_number_value_get (style->border->border_top_left_radius, 100);
  border_top = _gtk_css_number_value_get (style->border->border_top_width, 100);
  border_right = _gtk_css_number_value_get (style->border->border_right_width, 100);
  border_bottom = _gtk_css_number_value_get (style->border->border_bottom_width, 100);

  if (pos == GTK_POS_BOTTOM || pos == GTK_POS_RIGHT)
    {
      tip = 0;
      base = TAIL_HEIGHT + border_top;
    }
  else if (pos == GTK_POS_TOP)
    {
      base = popover_height - border_bottom - TAIL_HEIGHT;
      tip = popover_height;
    }
  else if (pos == GTK_POS_LEFT)
    {
      base = popover_width - border_right - TAIL_HEIGHT;
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
  GtkCssStyle *style;

  style = gtk_css_node_get_style (gtk_widget_get_css_node (widget));

  border->top = _gtk_css_number_value_get (style->size->margin_top, 100);
  border->right = _gtk_css_number_value_get (style->size->margin_right, 100);
  border->bottom = _gtk_css_number_value_get (style->size->margin_bottom, 100);
  border->left = _gtk_css_number_value_get (style->size->margin_left, 100);
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
get_border (GtkCssNode *node,
            GtkBorder *border)
{
  GtkCssStyle *style;

  style = gtk_css_node_get_style (node);

  border->top = _gtk_css_number_value_get (style->border->border_top_width, 100);
  border->right = _gtk_css_number_value_get (style->border->border_right_width, 100);
  border->bottom = _gtk_css_number_value_get (style->border->border_bottom_width, 100);
  border->left = _gtk_css_number_value_get (style->border->border_left_width, 100);
}

static void
gtk_popover_apply_tail_path (GtkPopover *popover,
                             cairo_t    *cr)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  gint initial_x, initial_y;
  gint tip_x, tip_y;
  gint final_x, final_y;
  GtkBorder border;
  GtkWidget *parent;

  parent = gtk_widget_get_parent (GTK_WIDGET (popover));

  if (!parent)
    return;

  get_border (priv->arrow_node, &border);

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
  int x, y, w, h;
  GskRoundedRect box;

  gtk_widget_get_allocation (widget, &allocation);

  cairo_set_source_rgba (cr, 0, 0, 0, 1);

  gtk_popover_apply_tail_path (popover, cr);
  cairo_close_path (cr);
  cairo_fill (cr);

  gtk_popover_get_rect_coords (popover, &x, &y, &w, &h);

  gtk_rounded_boxes_init_for_style (&box,
                                    NULL, NULL,
                                    gtk_css_node_get_style (gtk_widget_get_css_node (widget)),
                                    x, y, w, h);
  gsk_rounded_rect_path (&box, cr);
  cairo_fill (cr);
}

static void
gtk_popover_update_shape (GtkPopover *popover)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  if (priv->has_arrow)
    {
      cairo_surface_t *cairo_surface;
      cairo_region_t *region;
      cairo_t *cr;

      cairo_surface =
        gdk_surface_create_similar_surface (priv->surface,
                                           CAIRO_CONTENT_COLOR_ALPHA,
                                           gdk_surface_get_width (priv->surface),
                                           gdk_surface_get_height (priv->surface));

      cr = cairo_create (cairo_surface);
      gtk_popover_fill_border_path (popover, cr);
      cairo_destroy (cr);

      region = gdk_cairo_region_create_from_surface (cairo_surface);
      cairo_surface_destroy (cairo_surface);

      gdk_surface_set_input_region (priv->surface, region);
      cairo_region_destroy (region);
    }
  else
    gdk_surface_set_input_region (priv->surface, NULL);
}

static gint
get_border_radius (GtkWidget *widget)
{
  GtkCssStyle *style;

  style = gtk_css_node_get_style (gtk_widget_get_css_node (widget));
  return round (_gtk_css_number_value_get (style->border->border_top_left_radius, 100));
}

static gint
get_minimal_size (GtkPopover     *popover,
                  GtkOrientation  orientation)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  GtkPositionType pos;
  gint minimal_size;
  int tail_gap_width = priv->has_arrow ? TAIL_GAP_WIDTH : 0;

  minimal_size = 2 * get_border_radius (GTK_WIDGET (popover));
  pos = priv->position;

  if ((orientation == GTK_ORIENTATION_HORIZONTAL && POS_IS_VERTICAL (pos)) ||
      (orientation == GTK_ORIENTATION_VERTICAL && !POS_IS_VERTICAL (pos)))
    minimal_size += tail_gap_width;

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
  int tail_height = priv->has_arrow ? TAIL_HEIGHT : 0;

  if (for_size >= 0)
    for_size -= tail_height;

  gtk_widget_measure (priv->contents_widget,
                      orientation, for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);

  minimal_size = get_minimal_size (popover, orientation);
  *minimum = MAX (*minimum, minimal_size);
  *natural = MAX (*natural, minimal_size);

  *minimum += tail_height;
  *natural += tail_height;
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
  int tail_height = priv->has_arrow ? TAIL_HEIGHT : 0;

  switch (priv->final_position)
    {
    case GTK_POS_TOP:
      child_alloc.x = tail_height / 2;
      child_alloc.y = 0;
      break;
    case GTK_POS_BOTTOM:
      child_alloc.x = tail_height / 2;
      child_alloc.y = tail_height;
      break;
    case GTK_POS_LEFT:
      child_alloc.x = 0;
      child_alloc.y = tail_height / 2;
      break;
    case GTK_POS_RIGHT:
      child_alloc.x = tail_height;
      child_alloc.y = tail_height / 2;
      break;
    default:
      break;
    }

  child_alloc.width = width - tail_height;
  child_alloc.height = height - tail_height;

  gtk_widget_size_allocate (priv->contents_widget, &child_alloc, baseline);

  if (priv->surface)
    {
      gtk_popover_update_shape (popover);
      g_clear_pointer (&priv->arrow_render_node, gsk_render_node_unref);
    }
}

static void
create_arrow_render_node (GtkPopover *popover)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  GtkWidget *widget = GTK_WIDGET (popover);
  GtkStyleContext *context;
  GtkBorder border;
  cairo_t *cr;
  GtkSnapshot *snapshot;

  snapshot = gtk_snapshot_new ();

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

  get_border (priv->arrow_node, &border);

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_save_to_node (context, priv->arrow_node);

  /* Render the arrow background */
  gtk_render_background (context, cr,
                         0, 0,
                         gtk_widget_get_width (widget),
                         gtk_widget_get_height (widget));

  /* Render the border of the arrow tip */
  if (border.bottom > 0)
    {
      GtkCssStyle *style;
      const GdkRGBA *border_color;

      style = gtk_css_node_get_style (priv->arrow_node);
      border_color = gtk_css_color_value_get_rgba (style->border->border_left_color ? style->border->border_left_color : style->core->color);

      gtk_popover_apply_tail_path (popover, cr);
      gdk_cairo_set_source_rgba (cr, border_color);

      cairo_set_line_width (cr, border.bottom + 1);
      cairo_stroke (cr);
    }

  cairo_restore (cr);
  cairo_destroy (cr);

  gtk_style_context_restore (context);

  priv->arrow_render_node = gtk_snapshot_free_to_node (snapshot);
}

static void
gtk_popover_snapshot (GtkWidget   *widget,
                      GtkSnapshot *snapshot)
{
  GtkPopover *popover = GTK_POPOVER (widget);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  gtk_widget_snapshot_child (widget, priv->contents_widget, snapshot);

  if (priv->has_arrow)
    {
      if (!priv->arrow_render_node)
        create_arrow_render_node (popover);

      gtk_snapshot_append_node (snapshot, priv->arrow_render_node);
    }
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
    case PROP_POINTING_TO:
      gtk_popover_set_pointing_to (popover, g_value_get_boxed (value));
      break;

    case PROP_POSITION:
      gtk_popover_set_position (popover, g_value_get_enum (value));
      break;

    case PROP_AUTOHIDE:
      gtk_popover_set_autohide (popover, g_value_get_boolean (value));
      break;

    case PROP_DEFAULT_WIDGET:
      gtk_popover_set_default_widget (popover, g_value_get_object (value));
      break;

    case PROP_HAS_ARROW:
      gtk_popover_set_has_arrow (popover, g_value_get_boolean (value));
      break;

    case PROP_MNEMONICS_VISIBLE:
      gtk_popover_set_mnemonics_visible (popover, g_value_get_boolean (value));
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
    case PROP_POINTING_TO:
      g_value_set_boxed (value, &priv->pointing_to);
      break;

    case PROP_POSITION:
      g_value_set_enum (value, priv->position);
      break;

    case PROP_AUTOHIDE:
      g_value_set_boolean (value, priv->autohide);
      break;

    case PROP_DEFAULT_WIDGET:
      g_value_set_object (value, priv->default_widget);
      break;

    case PROP_HAS_ARROW:
      g_value_set_boolean (value, priv->has_arrow);
      break;

    case PROP_MNEMONICS_VISIBLE:
      g_value_set_boolean (value, priv->mnemonics_visible);
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
add_tab_bindings (GtkWidgetClass   *widget_class,
                  GdkModifierType   modifiers,
                  GtkDirectionType  direction)
{
  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_Tab, modifiers,
                                       "move-focus",
                                       "(i)", direction);
  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_KP_Tab, modifiers,
                                       "move-focus",
                                       "(i)", direction);
}

static void
add_arrow_bindings (GtkWidgetClass   *widget_class,
                    guint             keysym,
                    GtkDirectionType  direction)
{
  guint keypad_keysym = keysym - GDK_KEY_Left + GDK_KEY_KP_Left;

  gtk_widget_class_add_binding_signal (widget_class, keysym, 0,
                                       "move-focus",
                                       "(i)",
                                       direction);
  gtk_widget_class_add_binding_signal (widget_class, keysym, GDK_CONTROL_MASK,
                                       "move-focus",
                                       "(i)",
                                       direction);
  gtk_widget_class_add_binding_signal (widget_class, keypad_keysym, 0,
                                       "move-focus",
                                       "(i)",
                                       direction);
  gtk_widget_class_add_binding_signal (widget_class, keypad_keysym, GDK_CONTROL_MASK,
                                       "move-focus",
                                       "(i)",
                                       direction);
}

static void
gtk_popover_class_init (GtkPopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

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

  container_class->add = gtk_popover_add;
  container_class->remove = gtk_popover_remove;

  klass->activate_default = gtk_popover_activate_default;

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
                         GTK_TYPE_POSITION_TYPE, GTK_POS_BOTTOM,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_AUTOHIDE] =
      g_param_spec_boolean ("autohide",
                            P_("Autohide"),
                            P_("Whether to dismiss the popover on outside clicks"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_DEFAULT_WIDGET] =
      g_param_spec_object ("default-widget",
                           P_("Default widget"),
                           P_("The default widget"),
                           GTK_TYPE_WIDGET,
                           GTK_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_HAS_ARROW] =
      g_param_spec_boolean ("has-arrow",
                            P_("Has Arrow"),
                            P_("Whether to draw an arrow"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_MNEMONICS_VISIBLE] =
      g_param_spec_boolean ("mnemonics-visible",
                            P_("Mnemonics visible"),
                            P_("Whether mnemonics are currently visible in this popover"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

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

  signals[ACTIVATE_DEFAULT] =
    g_signal_new (I_("activate-default"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkPopoverClass, activate_default),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);

  add_arrow_bindings (widget_class, GDK_KEY_Up, GTK_DIR_UP);
  add_arrow_bindings (widget_class, GDK_KEY_Down, GTK_DIR_DOWN);
  add_arrow_bindings (widget_class, GDK_KEY_Left, GTK_DIR_LEFT);
  add_arrow_bindings (widget_class, GDK_KEY_Right, GTK_DIR_RIGHT);

  add_tab_bindings (widget_class, 0, GTK_DIR_TAB_FORWARD);
  add_tab_bindings (widget_class, GDK_CONTROL_MASK, GTK_DIR_TAB_FORWARD);
  add_tab_bindings (widget_class, GDK_SHIFT_MASK, GTK_DIR_TAB_BACKWARD);
  add_tab_bindings (widget_class, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_DIR_TAB_BACKWARD);

  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_Return, 0,
                                       "activate-default", NULL);
  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_ISO_Enter, 0,
                                       "activate-default", NULL);
  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_KP_Enter, 0,
                                       "activate-default", NULL);

  gtk_widget_class_set_css_name (widget_class, "popover");
}

GtkWidget *
gtk_popover_new (void)
{
  return g_object_new (GTK_TYPE_POPOVER, NULL);
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
gtk_popover_shortcut_manager_interface_init (GtkShortcutManagerInterface *iface)
{
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
 * gtk_popover_set_pointing_to:
 * @popover: a #GtkPopover
 * @rect: rectangle to point to
 *
 * Sets the rectangle that @popover will point to, in the
 * coordinate space of the @popover parent.
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
      priv->pointing_to.width = MAX (priv->pointing_to.width, 1);
      priv->pointing_to.height = MAX (priv->pointing_to.height, 1);
    }
  else
    priv->has_pointing_to = FALSE;

  g_object_notify_by_pspec (G_OBJECT (popover), properties[PROP_POINTING_TO]);

  if (gtk_widget_is_visible (GTK_WIDGET (popover)))
    present_popup (popover);
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
  else
    {
      graphene_rect_t r;
      GtkWidget *parent = gtk_widget_get_parent (GTK_WIDGET (popover));

      if (!gtk_widget_compute_bounds (parent, parent, &r))
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

  if (gtk_widget_is_visible (GTK_WIDGET (popover)))
    present_popup (popover);
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
 * gtk_popover_set_autohide:
 * @popover: a #GtkPopover
 * @autohide: #TRUE to dismiss the popover on outside clicks
 *
 * Sets whether @popover is modal.
 *
 * A modal popover will grab the keyboard focus on it when being
 * displayed. Clicking outside the popover area or pressing Esc will
 * dismiss the popover.
 *
 * Called this function on an already showing popup with a new autohide value
 * different from the current one, will cause the popup to be hidden.
 **/
void
gtk_popover_set_autohide (GtkPopover *popover,
                          gboolean    autohide)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  g_return_if_fail (GTK_IS_POPOVER (popover));

  autohide = autohide != FALSE;

  if (priv->autohide == autohide)
    return;

  priv->autohide = autohide;

  gtk_widget_unrealize (GTK_WIDGET (popover));

  g_object_notify_by_pspec (G_OBJECT (popover), properties[PROP_AUTOHIDE]);
}

/**
 * gtk_popover_get_autohide:
 * @popover: a #GtkPopover
 *
 * Returns whether the popover is modal.
 *
 * See gtk_popover_set_autohide() for the
 * implications of this.
 *
 * Returns: #TRUE if @popover is modal
 **/
gboolean
gtk_popover_get_autohide (GtkPopover *popover)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  g_return_val_if_fail (GTK_IS_POPOVER (popover), FALSE);

  return priv->autohide;
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

GtkWidget *
gtk_popover_get_contents_widget (GtkPopover *popover)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  return priv->contents_widget;
}

/**
 * gtk_popover_set_has_arrow:
 * @popover: a #GtkPopover
 * @has_arrow: %TRUE to draw an arrow
 *
 * Sets whether this popover should draw an arrow
 * pointing at the widget it is relative to.
 */
void
gtk_popover_set_has_arrow (GtkPopover *popover,
                           gboolean    has_arrow)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  g_return_if_fail (GTK_IS_POPOVER (popover));

  if (priv->has_arrow == has_arrow)
    return;

  priv->has_arrow = has_arrow;

  g_object_notify_by_pspec (G_OBJECT (popover), properties[PROP_HAS_ARROW]);
  gtk_widget_queue_resize (GTK_WIDGET (popover));
}

/**
 * gtk_popover_get_has_arrow:
 * @popover: a #GtkPopover
 *
 * Gets whether this popover is showing an arrow
 * pointing at the widget that it is relative to.
 *
 * Returns: whether the popover has an arrow
 */
gboolean
gtk_popover_get_has_arrow (GtkPopover *popover)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  g_return_val_if_fail (GTK_IS_POPOVER (popover), TRUE);

  return priv->has_arrow;
}

/**
 * gtk_popover_set_mnemonics_visible:
 * @popover: a #GtkPopover
 * @mnemonics_visible: the new value
 *
 * Sets the #GtkPopover:mnemonics-visible property.
 */
void
gtk_popover_set_mnemonics_visible (GtkPopover *popover,
                                   gboolean    mnemonics_visible)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  g_return_if_fail (GTK_IS_POPOVER (popover));

  if (priv->mnemonics_visible == mnemonics_visible)
    return;

  priv->mnemonics_visible = mnemonics_visible;

  g_object_notify_by_pspec (G_OBJECT (popover), properties[PROP_MNEMONICS_VISIBLE]);
  gtk_widget_queue_resize (GTK_WIDGET (popover));
}

/**
 * gtk_popover_get_mnemonics_visible:
 * @popover: a #GtkPopover
 *
 * Gets the value of the #GtkPopover:mnemonics-visible property.
 *
 * Returns: %TRUE if mnemonics are supposed to be visible in this popover 
 */
gboolean
gtk_popover_get_mnemonics_visible (GtkPopover *popover)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  g_return_val_if_fail (GTK_IS_POPOVER (popover), FALSE);

  return priv->mnemonics_visible;
}
