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

#include "config.h"

#include "gtkpopoverbar.h"
#include "gtkpopovermenu.h"

#include "gtkbox.h"
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

#define GTK_TYPE_POPOVER_BAR_ITEM    (gtk_popover_bar_item_get_type ())
#define GTK_POPOVER_BAR_ITEM(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_POPOVER_BAR_ITEM, GtkPopoverBarItem))
#define GTK_IS_POPOVER_BAR_ITEM(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_POPOVER_BAR_ITEM))

GType gtk_popover_bar_item_get_type (void) G_GNUC_CONST;

struct _GtkPopoverBar
{
  GtkWidget parent;

  GtkMenuTracker *tracker;
  GtkWidget *box;
  GtkWidget *open_popover;
};

typedef struct _GtkPopoverBarClass GtkPopoverBarClass;
struct _GtkPopoverBarClass
{
  GtkWidgetClass parent_class;
};

typedef struct _GtkPopoverBarItem GtkPopoverBarItem;
struct _GtkPopoverBarItem
{
  GtkWidget parent;

  GtkWidget *label;
  GtkWidget *popover;
};

typedef struct _GtkPopoverBarItemClass GtkPopoverBarItemClass;
struct _GtkPopoverBarItemClass
{
  GtkWidgetClass parent_class;

  void (* activate) (GtkPopoverBarItem *item);
};

G_DEFINE_TYPE (GtkPopoverBarItem, gtk_popover_bar_item, GTK_TYPE_WIDGET)

static void
clicked_cb (GtkGesture *gesture,
            int         n,
            double      x,
            double      y,
            gpointer    data)
{
  GtkWidget *target;
  GtkPopoverBarItem *item;
  GtkPopoverBar *bar;

  target = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  bar = GTK_POPOVER_BAR (gtk_widget_get_ancestor (target, GTK_TYPE_POPOVER_BAR));
  item = GTK_POPOVER_BAR_ITEM (target);

  if (item->popover && item->popover != bar->open_popover)
    {
      if (bar->open_popover)
        gtk_popover_popdown (GTK_POPOVER (bar->open_popover));
      bar->open_popover = item->popover;
      gtk_popover_popup (GTK_POPOVER (bar->open_popover));
    }
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
  GtkPopoverBarItem *item;
  GtkPopoverBar *bar;

  target = gtk_event_controller_get_widget (controller);

  bar = GTK_POPOVER_BAR (gtk_widget_get_ancestor (target, GTK_TYPE_POPOVER_BAR));
  item = GTK_POPOVER_BAR_ITEM (target);

  if (item->popover && bar->open_popover &&
      item->popover != bar->open_popover)
    {
      gtk_popover_popdown (GTK_POPOVER (bar->open_popover));
      bar->open_popover = item->popover;
      gtk_popover_popup (GTK_POPOVER (bar->open_popover));
    }
}

static void
gtk_popover_bar_item_init (GtkPopoverBarItem *item)
{
  GtkEventController *controller;

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
gtk_popover_bar_item_dispose (GObject *object)
{
  GtkPopoverBarItem *item = GTK_POPOVER_BAR_ITEM (object);

  g_clear_pointer (&item->label, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_popover_bar_item_parent_class)->dispose (object);
}

static void
gtk_popover_bar_item_finalize (GObject *object)
{
  G_OBJECT_CLASS (gtk_popover_bar_item_parent_class)->finalize (object);
}

static void
gtk_popover_bar_item_measure (GtkWidget      *widget,
                              GtkOrientation  orientation,
                              int             for_size,
                              int            *minimum,
                              int            *natural,
                              int            *minimum_baseline,
                              int            *natural_baseline)
{
  GtkPopoverBarItem *item = GTK_POPOVER_BAR_ITEM (widget);

  gtk_widget_measure (item->label, orientation, for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);
}

static void
gtk_popover_bar_item_size_allocate (GtkWidget *widget,
                                    int        width,
                                    int        height,
                                    int        baseline)
{
  GtkPopoverBarItem *item = GTK_POPOVER_BAR_ITEM (widget);

  gtk_widget_size_allocate (item->label,
                            &(GtkAllocation) { 0, 0, width, height },
                            baseline);

  gtk_native_check_resize (GTK_NATIVE (item->popover));
}

static void
gtk_popover_bar_item_activate (GtkPopoverBarItem *item)
{
  gtk_popover_popup (GTK_POPOVER (item->popover));
}

static void
gtk_popover_bar_item_class_init (GtkPopoverBarItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gtk_popover_bar_item_dispose;
  object_class->finalize = gtk_popover_bar_item_finalize;

  widget_class->measure = gtk_popover_bar_item_measure;
  widget_class->size_allocate = gtk_popover_bar_item_size_allocate;

  klass->activate = gtk_popover_bar_item_activate;

  widget_class->activate_signal =
    g_signal_new (I_("activate"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkPopoverBarItemClass, activate),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  gtk_widget_class_set_css_name (widget_class, I_("menuitem"));
}


G_DEFINE_TYPE (GtkPopoverBar, gtk_popover_bar, GTK_TYPE_WIDGET)

static void
gtk_popover_bar_init (GtkPopoverBar *bar)
{
  bar->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_parent (bar->box, GTK_WIDGET (bar));
}

static void
gtk_popover_bar_dispose (GObject *object)
{
  GtkPopoverBar *bar = GTK_POPOVER_BAR (object);

  g_clear_pointer (&bar->tracker, gtk_menu_tracker_free);
  g_clear_pointer (&bar->box, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_popover_bar_parent_class)->dispose (object);
}

static void
gtk_popover_bar_finalize (GObject *object)
{
  G_OBJECT_CLASS (gtk_popover_bar_parent_class)->finalize (object);
}

static void
gtk_popover_bar_measure (GtkWidget      *widget,
                         GtkOrientation  orientation,
                         int             for_size,
                         int            *minimum,
                         int            *natural,
                         int            *minimum_baseline,
                         int            *natural_baseline)
{
  GtkPopoverBar *bar = GTK_POPOVER_BAR (widget);

  gtk_widget_measure (bar->box, orientation, for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);
}

static void
gtk_popover_bar_size_allocate (GtkWidget *widget,
                               int        width,
                               int        height,
                               int        baseline)
{
  GtkPopoverBar *bar = GTK_POPOVER_BAR (widget);

  gtk_widget_size_allocate (bar->box,
                            &(GtkAllocation) { 0, 0, width, height },
                            baseline);
}

static void
gtk_popover_bar_class_init (GtkPopoverBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gtk_popover_bar_dispose;
  object_class->finalize = gtk_popover_bar_finalize;

  widget_class->measure = gtk_popover_bar_measure;
  widget_class->size_allocate = gtk_popover_bar_size_allocate;

  gtk_widget_class_set_css_name (widget_class, I_("menubar"));
}

static void
tracker_remove (gint     position,
                gpointer user_data)
{
  GtkPopoverBar *bar = user_data;
  GtkWidget *child;
  int i;

  for (child = gtk_widget_get_first_child (bar->box), i = 0;
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
popover_unmap (GtkWidget     *popover,
               GtkPopoverBar *bar)
{
  if (popover == bar->open_popover)
    bar->open_popover = NULL;
}

static void
tracker_insert (GtkMenuTrackerItem *item,
                gint                position,
                gpointer            user_data)
{
  GtkPopoverBar *bar = user_data;

  if (gtk_menu_tracker_item_get_has_link (item, G_MENU_LINK_SUBMENU))
    {
      GtkPopoverBarItem *widget;
      GMenuModel *model;
      GtkWidget *sibling;
      GtkWidget *child;
      GtkWidget *popover;
      int i;

      widget = g_object_new (GTK_TYPE_POPOVER_BAR_ITEM, NULL);
      g_object_bind_property (item, "label",
                              widget->label, "label",
                              G_BINDING_SYNC_CREATE);

      model = _gtk_menu_tracker_item_get_link (item, G_MENU_LINK_SUBMENU);
      popover = gtk_popover_menu_new_from_model (GTK_WIDGET (widget), model);
      gtk_popover_set_position (GTK_POPOVER (popover), GTK_POS_BOTTOM);
      gtk_popover_set_has_arrow (GTK_POPOVER (popover), FALSE);
      gtk_widget_set_halign (popover, GTK_ALIGN_START);

      g_signal_connect (popover, "unmap", G_CALLBACK (popover_unmap), bar);

      widget->popover = popover;

      sibling = NULL;
      for (child = gtk_widget_get_first_child (bar->box), i = 1;
           child;
           child = gtk_widget_get_next_sibling (child), i++)
        {
          if (i == position)
            {
              sibling = child;
              break;
            }
        }
      gtk_box_insert_child_after (GTK_BOX (bar->box), GTK_WIDGET (widget), sibling);
    }
  else
    g_warning ("Don't know how to handle this item");
}

GtkWidget *
gtk_popover_bar_new_from_model (GMenuModel *model)
{
  GtkWidget *bar;
  GtkActionMuxer *muxer;

  bar = g_object_new (GTK_TYPE_POPOVER_BAR, NULL);

  muxer = _gtk_widget_get_action_muxer (bar, TRUE);

  GTK_POPOVER_BAR (bar)->tracker =
      gtk_menu_tracker_new (GTK_ACTION_OBSERVABLE (muxer),
                            model,
                            FALSE,
                            TRUE,
                            FALSE,
                            NULL,
                            tracker_insert,
                            tracker_remove,
                            bar);

  return bar;
}
