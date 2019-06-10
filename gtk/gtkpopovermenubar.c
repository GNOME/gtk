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
 * SECTION:gtkpopovermenubar
 * @Title: GtkPopoverMenuBar
 * @Short_description: A menu bar with popovers
 * @See_also: #GtkPopover, #GtkPopoverMenu, #GMenuModel
 *
 * The #GtkPopoverBar presents a horizontal bar of items that pop
 * up popover menus when clicked. The only way to create instances
 * of GtkPopoverBar is from a #GMenuModel.
 *
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * menubar
 * ├── item[.active]
 * ┊   ╰── popover
 * ╰── item
 *     ╰── popover
 * ]|
 *
 * GtkMenuBar has a single CSS node with name menubar, below which
 * each item has its CSS node, and below that the corresponding
 * popover.
 *
 * The item whose popover is currently open gets the .active
 * style class.
 */


#include "config.h"

#include "gtkpopovermenubar.h"
#include "gtkpopovermenu.h"

#include "gtkboxlayout.h"
#include "gtklabel.h"
#include "gtkmenubutton.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkmarshalers.h"
#include "gtkstylecontext.h"
#include "gtkgestureclick.h"
#include "gtkeventcontrollerkey.h"
#include "gtkeventcontrollermotion.h"
#include "gtkactionmuxerprivate.h"
#include "gtkmenutracker.h"
#include "gtkwidgetprivate.h"
#include "gtkmain.h"
#include "gtknative.h"

#define GTK_TYPE_POPOVER_MENU_BAR_ITEM    (gtk_popover_menu_bar_item_get_type ())
#define GTK_POPOVER_MENU_BAR_ITEM(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_POPOVER_MENU_BAR_ITEM, GtkPopoverMenuBarItem))
#define GTK_IS_POPOVER_MENU_BAR_ITEM(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_POPOVER_MENU_BAR_ITEM))

GType gtk_popover_menu_bar_item_get_type (void) G_GNUC_CONST;

typedef struct _GtkPopoverMenuBarItem GtkPopoverMenuBarItem;

struct _GtkPopoverMenuBar
{
  GtkWidget parent;

  GMenuModel *model;
  GtkMenuTracker *tracker;

  GtkPopoverMenuBarItem *active_item;
};

typedef struct _GtkPopoverMenuBarClass GtkPopoverMenuBarClass;
struct _GtkPopoverMenuBarClass
{
  GtkWidgetClass parent_class;
};

struct _GtkPopoverMenuBarItem
{
  GtkWidget parent;

  GtkWidget *label;
  GtkPopover *popover;
};

typedef struct _GtkPopoverMenuBarItemClass GtkPopoverMenuBarItemClass;
struct _GtkPopoverMenuBarItemClass
{
  GtkWidgetClass parent_class;

  void (* activate) (GtkPopoverMenuBarItem *item);
};

G_DEFINE_TYPE (GtkPopoverMenuBarItem, gtk_popover_menu_bar_item, GTK_TYPE_WIDGET)

static void
set_active_item (GtkPopoverMenuBar     *bar,
                 GtkPopoverMenuBarItem *item,
                 gboolean               popup)
{
  gboolean changed;
  gboolean was_popup;

  changed = item != bar->active_item;

  if (bar->active_item)
    was_popup = gtk_widget_get_mapped (GTK_WIDGET (bar->active_item->popover));
  else
    was_popup = FALSE;

  if (was_popup && changed)
    gtk_popover_popdown (bar->active_item->popover);

  if (changed)
    {
      GtkStyleContext *context;

      if (bar->active_item)
        {
          context = gtk_widget_get_style_context (GTK_WIDGET (bar->active_item));
          gtk_style_context_remove_class (context, "active");
        }

      bar->active_item = item;

      if (bar->active_item)
        {
          context = gtk_widget_get_style_context (GTK_WIDGET (bar->active_item));
          gtk_style_context_add_class (context, "active");
        }
    }

  if (bar->active_item)
    {
      if (popup || (was_popup && changed))
        gtk_popover_popup (bar->active_item->popover);
      else if (changed)
        gtk_widget_grab_focus (GTK_WIDGET (bar->active_item));
    }
}

static void
clicked_cb (GtkGesture *gesture,
            int         n,
            double      x,
            double      y,
            gpointer    data)
{
  GtkWidget *target;
  GtkPopoverMenuBar *bar;

  target = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  bar = GTK_POPOVER_MENU_BAR (gtk_widget_get_ancestor (target, GTK_TYPE_POPOVER_MENU_BAR));

  set_active_item (bar, GTK_POPOVER_MENU_BAR_ITEM (target), TRUE);
}

static void
enter_cb (GtkEventController *controller,
          double              x,
          double              y,
          GdkCrossingMode     mode,
          GdkNotifyType       type,
          gpointer            data)
{
  GtkWidget *target;
  GtkPopoverMenuBar *bar;

  target = gtk_event_controller_get_widget (controller);

  bar = GTK_POPOVER_MENU_BAR (gtk_widget_get_ancestor (target, GTK_TYPE_POPOVER_MENU_BAR));

  set_active_item (bar, GTK_POPOVER_MENU_BAR_ITEM (target), FALSE);
}

static gboolean
gtk_popover_menu_bar_focus (GtkWidget        *widget,
                            GtkDirectionType  direction)
{
  GtkPopoverMenuBar *bar = GTK_POPOVER_MENU_BAR (widget);
  GtkWidget *next;
  if (bar->active_item &&
      gtk_widget_get_mapped (GTK_WIDGET (bar->active_item->popover)))
    {
      if (gtk_widget_child_focus (GTK_WIDGET (bar->active_item->popover), direction))
        return TRUE;
    }

  if (direction == GTK_DIR_LEFT)
    {
      if (bar->active_item)
        next = gtk_widget_get_prev_sibling (GTK_WIDGET (bar->active_item));
      else
        next = NULL;

      if (next == NULL)
        next = gtk_widget_get_last_child (GTK_WIDGET (bar));
    }
  else if (direction == GTK_DIR_RIGHT)
    {
      if (bar->active_item)
        next = gtk_widget_get_next_sibling (GTK_WIDGET (bar->active_item));
      else
        next = NULL;

      if (next == NULL)
        next = gtk_widget_get_first_child (GTK_WIDGET (bar));
    }
  else
    return FALSE;

  set_active_item (bar, GTK_POPOVER_MENU_BAR_ITEM (next), FALSE);

  return TRUE;
}

static void
gtk_popover_menu_bar_item_init (GtkPopoverMenuBarItem *item)
{
  GtkEventController *controller;

  gtk_widget_set_can_focus (GTK_WIDGET (item), TRUE);

  item->label = g_object_new (GTK_TYPE_LABEL,
                              "use-underline", TRUE,
                              NULL);
  gtk_widget_set_parent (item->label, GTK_WIDGET (item));

  controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
  g_signal_connect (controller, "pressed", G_CALLBACK (clicked_cb), NULL);
  gtk_widget_add_controller (GTK_WIDGET (item), controller);

  controller = gtk_event_controller_motion_new ();
  gtk_event_controller_set_propagation_limit (controller, GTK_LIMIT_NONE);
  g_signal_connect (controller, "enter", G_CALLBACK (enter_cb), NULL);
  gtk_widget_add_controller (GTK_WIDGET (item), controller);
}

static void
gtk_popover_menu_bar_item_dispose (GObject *object)
{
  GtkPopoverMenuBarItem *item = GTK_POPOVER_MENU_BAR_ITEM (object);

  g_clear_pointer (&item->label, gtk_widget_unparent);
  g_clear_pointer ((GtkWidget **)&item->popover, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_popover_menu_bar_item_parent_class)->dispose (object);
}

static void
gtk_popover_menu_bar_item_finalize (GObject *object)
{
  G_OBJECT_CLASS (gtk_popover_menu_bar_item_parent_class)->finalize (object);
}

static void
gtk_popover_menu_bar_item_measure (GtkWidget      *widget,
                                   GtkOrientation  orientation,
                                   int             for_size,
                                   int            *minimum,
                                   int            *natural,
                                   int            *minimum_baseline,
                                   int            *natural_baseline)
{
  GtkPopoverMenuBarItem *item = GTK_POPOVER_MENU_BAR_ITEM (widget);

  gtk_widget_measure (item->label, orientation, for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);
}

static void
gtk_popover_menu_bar_item_size_allocate (GtkWidget *widget,
                                         int        width,
                                         int        height,
                                         int        baseline)
{
  GtkPopoverMenuBarItem *item = GTK_POPOVER_MENU_BAR_ITEM (widget);

  gtk_widget_size_allocate (item->label,
                            &(GtkAllocation) { 0, 0, width, height },
                            baseline);

  gtk_native_check_resize (GTK_NATIVE (item->popover));
}

static void
gtk_popover_menu_bar_item_activate (GtkPopoverMenuBarItem *item)
{
  gtk_popover_popup (GTK_POPOVER (item->popover));
}

static void
gtk_popover_menu_bar_item_class_init (GtkPopoverMenuBarItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gtk_popover_menu_bar_item_dispose;
  object_class->finalize = gtk_popover_menu_bar_item_finalize;

  widget_class->measure = gtk_popover_menu_bar_item_measure;
  widget_class->size_allocate = gtk_popover_menu_bar_item_size_allocate;

  klass->activate = gtk_popover_menu_bar_item_activate;

  widget_class->activate_signal =
    g_signal_new (I_("activate"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkPopoverMenuBarItemClass, activate),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  gtk_widget_class_set_css_name (widget_class, I_("item"));
}
enum
{
  PROP_0,
  PROP_MENU_MODEL,
  LAST_PROP
};

static GParamSpec * bar_props[LAST_PROP];

G_DEFINE_TYPE (GtkPopoverMenuBar, gtk_popover_menu_bar, GTK_TYPE_WIDGET)

static void
tracker_remove (gint     position,
                gpointer user_data)
{
  GtkWidget *bar = user_data;
  GtkWidget *child;
  int i;

  for (child = gtk_widget_get_first_child (bar), i = 0;
       child;
       child = gtk_widget_get_next_sibling (child), i++)
    {
      if (i == position)
        {
          gtk_widget_destroy (child);
          break;
        }
    }
}

static void
popover_unmap (GtkPopover        *popover,
               GtkPopoverMenuBar *bar)
{
  if (bar->active_item && bar->active_item->popover == popover)
    set_active_item (bar, NULL, FALSE);
}

static void
tracker_insert (GtkMenuTrackerItem *item,
                gint                position,
                gpointer            user_data)
{
  GtkPopoverMenuBar *bar = user_data;

  if (gtk_menu_tracker_item_get_has_link (item, G_MENU_LINK_SUBMENU))
    {
      GtkPopoverMenuBarItem *widget;
      GMenuModel *model;
      GtkWidget *sibling;
      GtkWidget *child;
      GtkPopover *popover;
      int i;

      widget = g_object_new (GTK_TYPE_POPOVER_MENU_BAR_ITEM, NULL);
      g_object_bind_property (item, "label",
                              widget->label, "label",
                              G_BINDING_SYNC_CREATE);

      model = _gtk_menu_tracker_item_get_link (item, G_MENU_LINK_SUBMENU);
      popover = GTK_POPOVER (gtk_popover_menu_new_from_model (GTK_WIDGET (widget), model));
      gtk_popover_set_position (popover, GTK_POS_BOTTOM);
      gtk_popover_set_has_arrow (popover, FALSE);
      gtk_widget_set_halign (GTK_WIDGET (popover), GTK_ALIGN_START);

      g_signal_connect (popover, "unmap", G_CALLBACK (popover_unmap), bar);

      widget->popover = popover;

      sibling = NULL;
      for (child = gtk_widget_get_first_child (GTK_WIDGET (bar)), i = 1;
           child;
           child = gtk_widget_get_next_sibling (child), i++)
        {
          if (i == position)
            {
              sibling = child;
              break;
            }
        }
      gtk_widget_insert_after (GTK_WIDGET (widget), GTK_WIDGET (bar), sibling);
    }
  else
    g_warning ("Don't know how to handle this item");
}

static void
gtk_popover_menu_bar_set_property (GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GtkPopoverMenuBar *bar = GTK_POPOVER_MENU_BAR (object);

  switch (property_id)
    {
      case PROP_MENU_MODEL:
        gtk_popover_menu_bar_set_menu_model (bar, g_value_get_object (value));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gtk_popover_menu_bar_get_property (GObject    *object,
                                   guint       property_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GtkPopoverMenuBar *bar = GTK_POPOVER_MENU_BAR (object);

  switch (property_id)
    {
      case PROP_MENU_MODEL:
        g_value_set_object (value, bar->model);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}


static void
gtk_popover_menu_bar_dispose (GObject *object)
{
  GtkPopoverMenuBar *bar = GTK_POPOVER_MENU_BAR (object);
  GtkWidget *child;

  g_clear_pointer (&bar->tracker, gtk_menu_tracker_free);
  g_clear_object (&bar->model);

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (bar))))
    gtk_widget_destroy (child);

  G_OBJECT_CLASS (gtk_popover_menu_bar_parent_class)->dispose (object);
}

static void
gtk_popover_menu_bar_class_init (GtkPopoverMenuBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gtk_popover_menu_bar_dispose;
  object_class->set_property = gtk_popover_menu_bar_set_property;
  object_class->get_property = gtk_popover_menu_bar_get_property;

  widget_class->focus = gtk_popover_menu_bar_focus;

  /**
   * GtkPopoverMenuBar:menu-model:
   *
   * The #GMenuModel from which the menu bar is created.
   *
   * The model should only contain submenus as toplevel
   * items.
   */
  bar_props[PROP_MENU_MODEL] =
      g_param_spec_object ("menu-model",
                           P_("Menu model"),
                           P_("The model from which the bar is made."),
                           G_TYPE_MENU_MODEL,
                           GTK_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, bar_props);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, I_("menubar"));
}

static void
gtk_popover_menu_bar_init (GtkPopoverMenuBar *bar)
{
}

/**
 * gtk_popover_menu_bar_new_from_model:
 * @model: (allow-none): a #GMenuModel, or %NULL
 *
 * Creates a #GtkPopoverMenuBar from a #GMenuModel.
 *
 * Returns: a new #GtkPopoverMenuBar
 */
GtkWidget *
gtk_popover_menu_bar_new_from_model (GMenuModel *model)
{
  return g_object_new (GTK_TYPE_POPOVER_MENU_BAR,
                       "menu-model", model,
                       NULL);
}

/**
 * gtk_popover_menu_bar_set_menu_model:
 * @bar: a #GtkPopoverMenuBar
 * @model: (allow-none): a #GMenuModel, or %NULL
 *
 * Sets a menu model from which @bar should take
 * its contents.
 */
void
gtk_popover_menu_bar_set_menu_model (GtkPopoverMenuBar *bar,
                                     GMenuModel        *model)
{
  g_return_if_fail (GTK_IS_POPOVER_MENU_BAR (bar));
  g_return_if_fail (G_IS_MENU_MODEL (model));

  if (g_set_object (&bar->model, model))
    {
      GtkWidget *child;
      GtkActionMuxer *muxer;

      while ((child = gtk_widget_get_first_child (GTK_WIDGET (bar))))
        gtk_widget_destroy (child);

      g_clear_pointer (&bar->tracker, gtk_menu_tracker_free);

      if (model)
        {
          muxer = _gtk_widget_get_action_muxer (GTK_WIDGET (bar), TRUE);
          bar->tracker = gtk_menu_tracker_new (GTK_ACTION_OBSERVABLE (muxer),
                                               model,
                                               FALSE,
                                               TRUE,
                                               FALSE,
                                               NULL,
                                               tracker_insert,
                                               tracker_remove,
                                               bar);
        }

      g_object_notify_by_pspec (G_OBJECT (bar), bar_props[PROP_MENU_MODEL]);
    }
}

/**
 * gtk_popover_menu_bar_get_menu_model:
 * @bar: a #GtkPopoverMenuBar
 *
 * Returns the model from which the contents of @bar are taken.
 *
 * Returns: (transfer none): a #GMenuModel
 */
GMenuModel *
gtk_popover_menu_bar_get_menu_model (GtkPopoverMenuBar *bar)
{
  g_return_val_if_fail (GTK_IS_POPOVER_MENU_BAR (bar), NULL);

  return bar->model;
}
