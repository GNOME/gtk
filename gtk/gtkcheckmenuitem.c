/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2001.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "gtkcheckmenuitem.h"
#include "gtkaccellabel.h"
#include "gtksignal.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"

#define CHECK_TOGGLE_SIZE 12

enum {
  TOGGLED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_ACTIVE,
  PROP_INCONSISTENT
};

static void gtk_check_menu_item_class_init           (GtkCheckMenuItemClass *klass);
static void gtk_check_menu_item_init                 (GtkCheckMenuItem      *check_menu_item);
static gint gtk_check_menu_item_expose               (GtkWidget             *widget,
						      GdkEventExpose        *event);
static void gtk_check_menu_item_activate             (GtkMenuItem           *menu_item);
static void gtk_check_menu_item_toggle_size_request  (GtkMenuItem           *menu_item,
						      gint                  *requisition);
static void gtk_check_menu_item_draw_indicator       (GtkCheckMenuItem      *check_menu_item,
						      GdkRectangle          *area);
static void gtk_real_check_menu_item_draw_indicator  (GtkCheckMenuItem      *check_menu_item,
						      GdkRectangle          *area);
static void gtk_check_menu_item_set_property (GObject         *object,
					      guint            prop_id,
					      const GValue    *value,
					      GParamSpec      *pspec);
static void gtk_check_menu_item_get_property (GObject         *object,
					      guint            prop_id,
					      GValue          *value,
					      GParamSpec      *pspec);


static GtkMenuItemClass *parent_class = NULL;
static guint check_menu_item_signals[LAST_SIGNAL] = { 0 };


GtkType
gtk_check_menu_item_get_type (void)
{
  static GtkType check_menu_item_type = 0;

  if (!check_menu_item_type)
    {
      static const GtkTypeInfo check_menu_item_info =
      {
        "GtkCheckMenuItem",
        sizeof (GtkCheckMenuItem),
        sizeof (GtkCheckMenuItemClass),
        (GtkClassInitFunc) gtk_check_menu_item_class_init,
        (GtkObjectInitFunc) gtk_check_menu_item_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      check_menu_item_type = gtk_type_unique (GTK_TYPE_MENU_ITEM, &check_menu_item_info);
    }

  return check_menu_item_type;
}

static void
gtk_check_menu_item_class_init (GtkCheckMenuItemClass *klass)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkMenuItemClass *menu_item_class;
  
  object_class = (GtkObjectClass*) klass;
  widget_class = (GtkWidgetClass*) klass;
  menu_item_class = (GtkMenuItemClass*) klass;
  
  parent_class = gtk_type_class (GTK_TYPE_MENU_ITEM);

  G_OBJECT_CLASS(object_class)->set_property = gtk_check_menu_item_set_property;
  G_OBJECT_CLASS(object_class)->get_property = gtk_check_menu_item_get_property;

  g_object_class_install_property (G_OBJECT_CLASS (object_class),
                                   PROP_ACTIVE,
                                   g_param_spec_boolean ("active",
                                                         _("Active"),
                                                         _("Whether the menu item is checked."),
                                                         FALSE,
                                                         G_PARAM_READWRITE));
  
  g_object_class_install_property (G_OBJECT_CLASS (object_class),
                                   PROP_INCONSISTENT,
                                   g_param_spec_boolean ("inconsistent",
                                                         _("Inconsistent"),
                                                         _("Whether to display an \"inconsistent\" state."),
                                                         FALSE,
                                                         G_PARAM_READWRITE));
  
  widget_class->expose_event = gtk_check_menu_item_expose;
  
  menu_item_class->activate = gtk_check_menu_item_activate;
  menu_item_class->hide_on_activate = FALSE;
  menu_item_class->toggle_size_request = gtk_check_menu_item_toggle_size_request;
  
  klass->toggled = NULL;
  klass->draw_indicator = gtk_real_check_menu_item_draw_indicator;

  check_menu_item_signals[TOGGLED] =
    gtk_signal_new ("toggled",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkCheckMenuItemClass, toggled),
                    _gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
}

GtkWidget*
gtk_check_menu_item_new (void)
{
  return GTK_WIDGET (gtk_type_new (GTK_TYPE_CHECK_MENU_ITEM));
}

GtkWidget*
gtk_check_menu_item_new_with_label (const gchar *label)
{
  GtkWidget *check_menu_item;
  GtkWidget *accel_label;

  check_menu_item = gtk_check_menu_item_new ();
  accel_label = gtk_accel_label_new (label);
  gtk_misc_set_alignment (GTK_MISC (accel_label), 0.0, 0.5);

  gtk_container_add (GTK_CONTAINER (check_menu_item), accel_label);
  gtk_accel_label_set_accel_widget (GTK_ACCEL_LABEL (accel_label), check_menu_item);
  gtk_widget_show (accel_label);

  return check_menu_item;
}


/**
 * gtk_check_menu_item_new_with_mnemonic:
 * @label: The text of the button, with an underscore in front of the
 *         mnemonic character
 * @returns: a new #GtkCheckMenuItem
 *
 * Creates a new #GtkCheckMenuItem containing a label. The label
 * will be created using gtk_label_new_with_mnemonic(), so underscores
 * in @label indicate the mnemonic for the menu item.
 **/
GtkWidget*
gtk_check_menu_item_new_with_mnemonic (const gchar *label)
{
  GtkWidget *check_menu_item;
  GtkWidget *accel_label;

  check_menu_item = gtk_check_menu_item_new ();
  accel_label = gtk_type_new (GTK_TYPE_ACCEL_LABEL);
  gtk_label_set_text_with_mnemonic (GTK_LABEL (accel_label), label);
  gtk_misc_set_alignment (GTK_MISC (accel_label), 0.0, 0.5);

  gtk_container_add (GTK_CONTAINER (check_menu_item), accel_label);
  gtk_accel_label_set_accel_widget (GTK_ACCEL_LABEL (accel_label), check_menu_item);
  gtk_widget_show (accel_label);

  return check_menu_item;
}

void
gtk_check_menu_item_set_active (GtkCheckMenuItem *check_menu_item,
				gboolean          is_active)
{
  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (check_menu_item));

  is_active = is_active != 0;

  if (check_menu_item->active != is_active)
    gtk_menu_item_activate (GTK_MENU_ITEM (check_menu_item));
}

/**
 * gtk_check_menu_item_get_active:
 * @check_menu_item: a #GtkCheckMenuItem
 * 
 * Returns whether the check menu item is active. See
 * gtk_check_menu_item_set_active ().
 * 
 * Return value: %TRUE if the menu item is checked.
 */
gboolean
gtk_check_menu_item_get_active (GtkCheckMenuItem *check_menu_item)
{
  g_return_val_if_fail (GTK_IS_CHECK_MENU_ITEM (check_menu_item), FALSE);

  return check_menu_item->active;
}

static void
gtk_check_menu_item_toggle_size_request (GtkMenuItem *menu_item,
					 gint        *requisition)
{
  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (menu_item));

  *requisition = CHECK_TOGGLE_SIZE;
}

void
gtk_check_menu_item_set_show_toggle (GtkCheckMenuItem *menu_item,
				     gboolean          always)
{
  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (menu_item));

#if 0
  menu_item->always_show_toggle = always != FALSE;
#endif  
}

void
gtk_check_menu_item_toggled (GtkCheckMenuItem *check_menu_item)
{
  gtk_signal_emit (GTK_OBJECT (check_menu_item), check_menu_item_signals[TOGGLED]);
}

/**
 * gtk_check_menu_item_set_inconsistent:
 * @check_menu_item: a #GtkCheckMenuItem
 * @setting: %TRUE to display an "inconsistent" third state check
 *
 * If the user has selected a range of elements (such as some text or
 * spreadsheet cells) that are affected by a boolean setting, and the
 * current values in that range are inconsistent, you may want to
 * display the check in an "in between" state. This function turns on
 * "in between" display.  Normally you would turn off the inconsistent
 * state again if the user explicitly selects a setting. This has to be
 * done manually, gtk_check_menu_item_set_inconsistent() only affects
 * visual appearance, it doesn't affect the semantics of the widget.
 * 
 **/
void
gtk_check_menu_item_set_inconsistent (GtkCheckMenuItem *check_menu_item,
                                      gboolean          setting)
{
  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (check_menu_item));
  
  setting = setting != FALSE;

  if (setting != check_menu_item->inconsistent)
    {
      check_menu_item->inconsistent = setting;
      gtk_widget_queue_draw (GTK_WIDGET (check_menu_item));
      g_object_notify (G_OBJECT (check_menu_item), "inconsistent");
    }
}

/**
 * gtk_check_menu_item_get_inconsistent:
 * @check_menu_item: a #GtkCheckMenuItem
 * 
 * Retrieves the value set by gtk_check_menu_item_set_inconsistent().
 * 
 * Return value: %TRUE if inconsistent
 **/
gboolean
gtk_check_menu_item_get_inconsistent (GtkCheckMenuItem *check_menu_item)
{
  g_return_val_if_fail (GTK_IS_CHECK_MENU_ITEM (check_menu_item), FALSE);

  return check_menu_item->inconsistent;
}

static void
gtk_check_menu_item_init (GtkCheckMenuItem *check_menu_item)
{
  check_menu_item->active = FALSE;
  check_menu_item->always_show_toggle = TRUE;
}

static gint
gtk_check_menu_item_expose (GtkWidget      *widget,
			    GdkEventExpose *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CHECK_MENU_ITEM (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_CLASS (parent_class)->expose_event)
    (* GTK_WIDGET_CLASS (parent_class)->expose_event) (widget, event);

  gtk_check_menu_item_draw_indicator (GTK_CHECK_MENU_ITEM (widget), &event->area);

  return FALSE;
}

static void
gtk_check_menu_item_activate (GtkMenuItem *menu_item)
{
  GtkCheckMenuItem *check_menu_item;

  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (menu_item));

  check_menu_item = GTK_CHECK_MENU_ITEM (menu_item);
  check_menu_item->active = !check_menu_item->active;

  gtk_check_menu_item_toggled (check_menu_item);
  gtk_widget_queue_draw (GTK_WIDGET (check_menu_item));

  g_object_notify (G_OBJECT (check_menu_item), "active");
}

static void
gtk_check_menu_item_draw_indicator (GtkCheckMenuItem *check_menu_item,
				    GdkRectangle     *area)
{
  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (check_menu_item));
  g_return_if_fail (GTK_CHECK_MENU_ITEM_GET_CLASS (check_menu_item) != NULL);

  if (GTK_CHECK_MENU_ITEM_GET_CLASS (check_menu_item)->draw_indicator)
    (* GTK_CHECK_MENU_ITEM_GET_CLASS (check_menu_item)->draw_indicator) (check_menu_item, area);
}

static void
gtk_real_check_menu_item_draw_indicator (GtkCheckMenuItem *check_menu_item,
					 GdkRectangle     *area)
{
  GtkWidget *widget;
  GtkStateType state_type;
  GtkShadowType shadow_type;
  gint width, height;
  gint x, y;

  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (check_menu_item));

  if (GTK_WIDGET_DRAWABLE (check_menu_item))
    {
      widget = GTK_WIDGET (check_menu_item);

      width = 8;
      height = 8;
      x = widget->allocation.x + (GTK_CONTAINER (check_menu_item)->border_width +
				  widget->style->xthickness + 2);
      y = widget->allocation.y + (widget->allocation.height - height) / 2;

      if (check_menu_item->active ||
	  check_menu_item->always_show_toggle ||
	  (GTK_WIDGET_STATE (check_menu_item) == GTK_STATE_PRELIGHT))
	{
	  state_type = GTK_WIDGET_STATE (widget);
	  
	  if (check_menu_item->always_show_toggle)
	    {
	      shadow_type = GTK_SHADOW_OUT;
	      if (check_menu_item->active)
		shadow_type = GTK_SHADOW_IN;
	    }
	  else
	    {
	      shadow_type = GTK_SHADOW_IN;
	      if (check_menu_item->active &&
		  (state_type == GTK_STATE_PRELIGHT))
		shadow_type = GTK_SHADOW_OUT;
	    }

          if (check_menu_item->inconsistent)
            {
              shadow_type = GTK_SHADOW_ETCHED_IN;
              if (state_type == GTK_STATE_ACTIVE)
                state_type = GTK_STATE_NORMAL;
            }
              
	  gtk_paint_check (widget->style, widget->window,
			   state_type, shadow_type,
			   area, widget, "check",
			   x, y, width, height);
	}
    }
}


static void
gtk_check_menu_item_get_property (GObject     *object,
				  guint        prop_id,
				  GValue      *value,
				  GParamSpec  *pspec)
{
  GtkCheckMenuItem *checkitem = GTK_CHECK_MENU_ITEM (object);
  
  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, checkitem->active);
      break;
    case PROP_INCONSISTENT:
      g_value_set_boolean (value, checkitem->inconsistent);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
gtk_check_menu_item_set_property (GObject      *object,
				  guint         prop_id,
				  const GValue *value,
				  GParamSpec   *pspec)
{
  GtkCheckMenuItem *checkitem = GTK_CHECK_MENU_ITEM (object);
  
  switch (prop_id)
    {
    case PROP_ACTIVE:
      gtk_check_menu_item_set_active (checkitem, g_value_get_boolean (value));
      break;
    case PROP_INCONSISTENT:
      gtk_check_menu_item_set_inconsistent (checkitem, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}
