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

#include <string.h>
#include "gtkalignment.h"
#include "gtkbutton.h"
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtksignal.h"
#include "gtkimage.h"
#include "gtkhbox.h"
#include "gtkstock.h"
#include "gtkiconfactory.h"
#include "gtkintl.h"

#define CHILD_SPACING     1

static GtkBorder default_default_border = { 1, 1, 1, 1 };
static GtkBorder default_default_outside_border = { 0, 0, 0, 0 };

/* Time out before giving up on getting a key release when animatng
 * the close button.
 */
#define ACTIVATE_TIMEOUT 250

enum {
  PRESSED,
  RELEASED,
  CLICKED,
  ENTER,
  LEAVE,
  ACTIVATE,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_LABEL,
  PROP_RELIEF,
  PROP_USE_UNDERLINE,
  PROP_USE_STOCK
};

static void gtk_button_class_init     (GtkButtonClass   *klass);
static void gtk_button_init           (GtkButton        *button);
static void gtk_button_set_property   (GObject         *object,
                                       guint            prop_id,
                                       const GValue    *value,
                                       GParamSpec      *pspec);
static void gtk_button_get_property   (GObject         *object,
                                       guint            prop_id,
                                       GValue          *value,
                                       GParamSpec      *pspec);
static void gtk_button_realize        (GtkWidget        *widget);
static void gtk_button_unrealize      (GtkWidget        *widget);
static void gtk_button_size_request   (GtkWidget        *widget,
				       GtkRequisition   *requisition);
static void gtk_button_size_allocate  (GtkWidget        *widget,
				       GtkAllocation    *allocation);
static void gtk_button_paint          (GtkWidget        *widget,
				       GdkRectangle     *area);
static gint gtk_button_expose         (GtkWidget        *widget,
				       GdkEventExpose   *event);
static gint gtk_button_button_press   (GtkWidget        *widget,
				       GdkEventButton   *event);
static gint gtk_button_button_release (GtkWidget        *widget,
				       GdkEventButton   *event);
static gint gtk_button_key_release    (GtkWidget        *widget,
				       GdkEventKey      *event);
static gint gtk_button_enter_notify   (GtkWidget        *widget,
				       GdkEventCrossing *event);
static gint gtk_button_leave_notify   (GtkWidget        *widget,
				       GdkEventCrossing *event);
static void gtk_real_button_pressed   (GtkButton        *button);
static void gtk_real_button_released  (GtkButton        *button);
static void gtk_real_button_activate (GtkButton         *button);
static void gtk_button_update_state   (GtkButton        *button);
static GtkType gtk_button_child_type  (GtkContainer     *container);
static void gtk_button_finish_activate (GtkButton *button,
					gboolean   do_it);

static GObject*	gtk_button_constructor     (GType                  type,
					    guint                  n_construct_properties,
					    GObjectConstructParam *construct_params);
static void     gtk_button_construct_child (GtkButton             *button);


static GtkBinClass *parent_class = NULL;
static guint button_signals[LAST_SIGNAL] = { 0 };


GtkType
gtk_button_get_type (void)
{
  static GtkType button_type = 0;

  if (!button_type)
    {
      static const GTypeInfo button_info =
      {
	sizeof (GtkButtonClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_button_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkButton),
	16,		/* n_preallocs */
	(GInstanceInitFunc) gtk_button_init,
      };

      button_type = g_type_register_static (GTK_TYPE_BIN, "GtkButton", &button_info, 0);
    }

  return button_type;
}

static void
gtk_button_class_init (GtkButtonClass *klass)
{
  GObjectClass *g_object_class;
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  g_object_class = G_OBJECT_CLASS (klass);
  object_class = (GtkObjectClass*) klass;
  widget_class = (GtkWidgetClass*) klass;
  container_class = (GtkContainerClass*) klass;
  
  parent_class = g_type_class_peek_parent (klass);

  g_object_class->constructor = gtk_button_constructor;
  g_object_class->set_property = gtk_button_set_property;
  g_object_class->get_property = gtk_button_get_property;

  widget_class->realize = gtk_button_realize;
  widget_class->unrealize = gtk_button_unrealize;
  widget_class->size_request = gtk_button_size_request;
  widget_class->size_allocate = gtk_button_size_allocate;
  widget_class->expose_event = gtk_button_expose;
  widget_class->button_press_event = gtk_button_button_press;
  widget_class->button_release_event = gtk_button_button_release;
  widget_class->key_release_event = gtk_button_key_release;
  widget_class->enter_notify_event = gtk_button_enter_notify;
  widget_class->leave_notify_event = gtk_button_leave_notify;

  container_class->child_type = gtk_button_child_type;

  klass->pressed = gtk_real_button_pressed;
  klass->released = gtk_real_button_released;
  klass->clicked = NULL;
  klass->enter = gtk_button_update_state;
  klass->leave = gtk_button_update_state;
  klass->activate = gtk_real_button_activate;

  g_object_class_install_property (G_OBJECT_CLASS(object_class),
                                   PROP_LABEL,
                                   g_param_spec_string ("label",
                                                        _("Label"),
                                                        _("Text of the label widget inside the button, if the button contains a label widget."),
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  
  g_object_class_install_property (G_OBJECT_CLASS(object_class),
                                   PROP_USE_UNDERLINE,
                                   g_param_spec_boolean ("use_underline",
							 _("Use underline"),
							 _("If set, an underline in the text indicates the next character should be used for the mnemonic accelerator key"),
                                                        FALSE,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  
  g_object_class_install_property (G_OBJECT_CLASS(object_class),
                                   PROP_USE_STOCK,
                                   g_param_spec_boolean ("use_stock",
							 _("Use stock"),
							 _("If set, the label is used to pick a stock item instead of being displayed"),
                                                        FALSE,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  
  g_object_class_install_property (G_OBJECT_CLASS(object_class),
                                   PROP_RELIEF,
                                   g_param_spec_enum ("relief",
                                                      _("Border relief"),
                                                      _("The border relief style."),
                                                      GTK_TYPE_RELIEF_STYLE,
                                                      GTK_RELIEF_NORMAL,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE));

  button_signals[PRESSED] =
    gtk_signal_new ("pressed",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkButtonClass, pressed),
                    gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
  button_signals[RELEASED] =
    gtk_signal_new ("released",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkButtonClass, released),
                    gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
  button_signals[CLICKED] =
    gtk_signal_new ("clicked",
                    GTK_RUN_FIRST | GTK_RUN_ACTION,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkButtonClass, clicked),
                    gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
  button_signals[ENTER] =
    gtk_signal_new ("enter",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkButtonClass, enter),
                    gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
  button_signals[LEAVE] =
    gtk_signal_new ("leave",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkButtonClass, leave),
                    gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
  button_signals[ACTIVATE] =
    gtk_signal_new ("activate",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkButtonClass, activate),
                    gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
  widget_class->activate_signal = button_signals[ACTIVATE];

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boxed ("default_border",
							       _("Default Spacing"),
							       _("Extra space to add for CAN_DEFAULT buttons"),
							       GTK_TYPE_BORDER,
							       G_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boxed ("default_outside_border",
							       _("Default Outside Spacing"),
							       _("Extra space to add for CAN_DEFAULT buttons that is always drawn outside the border"),
							       GTK_TYPE_BORDER,
							       G_PARAM_READABLE));
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("child_displacement_x",
							     _("Child X Displacement"),
							     _("How far in the x direction to move the child when the button is depressed"),
							     G_MININT,
							     G_MAXINT,
							     0,
							     G_PARAM_READABLE));
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("child_displacement_y",
							     _("Child Y Displacement"),
							     _("How far in the y direction to move the child when the button is depressed"),
							     G_MININT,
							     G_MAXINT,
							     0,
							     G_PARAM_READABLE));
}

static void
gtk_button_init (GtkButton *button)
{
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_FOCUS | GTK_RECEIVES_DEFAULT);
  GTK_WIDGET_UNSET_FLAGS (button, GTK_NO_WINDOW);

  button->label_text = NULL;
  
  button->constructed = FALSE;
  button->in_button = FALSE;
  button->button_down = FALSE;
  button->relief = GTK_RELIEF_NORMAL;
  button->use_stock = FALSE;
  button->use_underline = FALSE;
  button->depressed = FALSE;
}

static GObject*
gtk_button_constructor (GType                  type,
			guint                  n_construct_properties,
			GObjectConstructParam *construct_params)
{
  GObject *object;
  GtkButton *button;

  object = (* G_OBJECT_CLASS (parent_class)->constructor) (type,
							   n_construct_properties,
							   construct_params);

  button = GTK_BUTTON (object);
  button->constructed = TRUE;

  if (button->label_text != NULL)
    gtk_button_construct_child (button);
  
  return object;
}


static GtkType
gtk_button_child_type  (GtkContainer     *container)
{
  if (!GTK_BIN (container)->child)
    return GTK_TYPE_WIDGET;
  else
    return GTK_TYPE_NONE;
}

static void
gtk_button_set_property (GObject         *object,
                         guint            prop_id,
                         const GValue    *value,
                         GParamSpec      *pspec)
{
  GtkButton *button;

  button = GTK_BUTTON (object);

  switch (prop_id)
    {
    case PROP_LABEL:
      gtk_button_set_label (button, g_value_get_string (value));
      break;
    case PROP_RELIEF:
      gtk_button_set_relief (button, g_value_get_enum (value));
      break;
    case PROP_USE_UNDERLINE:
      gtk_button_set_use_underline (button, g_value_get_boolean (value));
      break;
    case PROP_USE_STOCK:
      gtk_button_set_use_stock (button, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_button_get_property (GObject         *object,
                         guint            prop_id,
                         GValue          *value,
                         GParamSpec      *pspec)
{
  GtkButton *button;

  button = GTK_BUTTON (object);

  switch (prop_id)
    {
    case PROP_LABEL:
      g_value_set_string (value, button->label_text);
      break;
    case PROP_RELIEF:
      g_value_set_enum (value, gtk_button_get_relief (button));
      break;
    case PROP_USE_UNDERLINE:
      g_value_set_boolean (value, button->use_underline);
      break;
    case PROP_USE_STOCK:
      g_value_set_boolean (value, button->use_stock);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

GtkWidget*
gtk_button_new (void)
{
  return GTK_WIDGET (gtk_type_new (gtk_button_get_type ()));
}

static void
gtk_button_construct_child (GtkButton *button)
{
  GtkStockItem item;
  GtkWidget *label;
  GtkWidget *image;
  GtkWidget *hbox;
  GtkWidget *align;

  if (!button->constructed)
    return;
  
  if (button->label_text == NULL)
    return;

  if (GTK_BIN (button)->child)
    gtk_container_remove (GTK_CONTAINER (button),
			  GTK_BIN (button)->child);

  
  if (button->use_stock &&
      gtk_stock_lookup (button->label_text, &item))
    {
      label = gtk_label_new_with_mnemonic (item.label);

      gtk_label_set_mnemonic_widget (GTK_LABEL (label), GTK_WIDGET (button));
      
      image = gtk_image_new_from_stock (button->label_text, GTK_ICON_SIZE_BUTTON);
      hbox = gtk_hbox_new (FALSE, 2);

      align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
      
      gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
      gtk_box_pack_end (GTK_BOX (hbox), label, FALSE, FALSE, 0);
      
      gtk_container_add (GTK_CONTAINER (button), align);
      gtk_container_add (GTK_CONTAINER (align), hbox);
      gtk_widget_show_all (align);

      return;
    }

  if (button->use_underline)
    {
      label = gtk_label_new_with_mnemonic (button->label_text);
      gtk_label_set_mnemonic_widget (GTK_LABEL (label), GTK_WIDGET (button));
    }
  else
    label = gtk_label_new (button->label_text);
  
  gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);

  gtk_widget_show (label);
  gtk_container_add (GTK_CONTAINER (button), label);
}


GtkWidget*
gtk_button_new_with_label (const gchar *label)
{
  return g_object_new (GTK_TYPE_BUTTON, "label", label, NULL);
}

/**
 * gtk_button_new_from_stock:
 * @stock_id: the name of the stock item 
 *
 * Creates a new #GtkButton containing the image and text from a stock item.
 * Some stock ids have preprocessor macros like #GTK_STOCK_OK and
 * #GTK_STOCK_APPLY.
 *
 * If @stock_id is unknown, then it will be treated as a mnemonic
 * label (as for gtk_button_new_with_mnemonic()).
 *
 * Returns: a new #GtkButton
 **/
GtkWidget*
gtk_button_new_from_stock (const gchar *stock_id)
{
  return g_object_new (GTK_TYPE_BUTTON,
                       "label", stock_id,
                       "use_stock", TRUE,
                       "use_underline", TRUE,
                       NULL);
}

/**
 * gtk_button_new_with_mnemonic:
 * @label: The text of the button, with an underscore in front of the
 *         mnemonic character
 * @returns: a new #GtkButton
 *
 * Creates a new #GtkButton containing a label.
 * If characters in @label are preceded by an underscore, they are underlined
 * indicating that they represent a keyboard accelerator called a mnemonic.
 * Pressing Alt and that key activates the button.
 **/
GtkWidget*
gtk_button_new_with_mnemonic (const gchar *label)
{
  return g_object_new (GTK_TYPE_BUTTON, "label", label, "use_underline", TRUE,  NULL);
}

void
gtk_button_pressed (GtkButton *button)
{
  g_return_if_fail (GTK_IS_BUTTON (button));

  gtk_signal_emit (GTK_OBJECT (button), button_signals[PRESSED]);
}

void
gtk_button_released (GtkButton *button)
{
  g_return_if_fail (GTK_IS_BUTTON (button));

  gtk_signal_emit (GTK_OBJECT (button), button_signals[RELEASED]);
}

void
gtk_button_clicked (GtkButton *button)
{
  g_return_if_fail (GTK_IS_BUTTON (button));

  gtk_signal_emit (GTK_OBJECT (button), button_signals[CLICKED]);
}

void
gtk_button_enter (GtkButton *button)
{
  g_return_if_fail (GTK_IS_BUTTON (button));

  gtk_signal_emit (GTK_OBJECT (button), button_signals[ENTER]);
}

void
gtk_button_leave (GtkButton *button)
{
  g_return_if_fail (GTK_IS_BUTTON (button));

  gtk_signal_emit (GTK_OBJECT (button), button_signals[LEAVE]);
}

void
gtk_button_set_relief (GtkButton *button,
		       GtkReliefStyle newrelief)
{
  g_return_if_fail (GTK_IS_BUTTON (button));

  button->relief = newrelief;
  g_object_notify(G_OBJECT(button), "relief");
  gtk_widget_queue_draw (GTK_WIDGET (button));
}

GtkReliefStyle
gtk_button_get_relief (GtkButton *button)
{
  g_return_val_if_fail (button != NULL, GTK_RELIEF_NORMAL);
  g_return_val_if_fail (GTK_IS_BUTTON (button), GTK_RELIEF_NORMAL);

  return button->relief;
}

static void
gtk_button_realize (GtkWidget *widget)
{
  GtkButton *button;
  GdkWindowAttr attributes;
  gint attributes_mask;
  gint border_width;

  g_return_if_fail (GTK_IS_BUTTON (widget));

  button = GTK_BUTTON (widget);
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  border_width = GTK_CONTAINER (widget)->border_width;

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x + border_width;
  attributes.y = widget->allocation.y + border_width;
  attributes.width = widget->allocation.width - border_width * 2;
  attributes.height = widget->allocation.height - border_width * 2;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_EXPOSURE_MASK |
			    GDK_BUTTON_PRESS_MASK |
			    GDK_BUTTON_RELEASE_MASK |
			    GDK_ENTER_NOTIFY_MASK |
			    GDK_LEAVE_NOTIFY_MASK);

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, button);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}

static void
gtk_button_unrealize (GtkWidget *widget)
{
  GtkButton *button = GTK_BUTTON (widget);

  if (button->activate_timeout)
    gtk_button_finish_activate (button, FALSE);
    
  GTK_WIDGET_CLASS (parent_class)->unrealize (widget);
}

static void
gtk_button_get_props (GtkButton *button,
		      GtkBorder *default_border,
		      GtkBorder *default_outside_border,
		      gboolean  *interior_focus)
{
  GtkWidget *widget =  GTK_WIDGET (button);
  GtkBorder *tmp_border;

  if (default_border)
    {
      gtk_widget_style_get (widget, "default_border", &tmp_border, NULL);

      if (tmp_border)
	{
	  *default_border = *tmp_border;
	  g_free (tmp_border);
	}
      else
	*default_border = default_default_border;
    }

  if (default_outside_border)
    {
      gtk_widget_style_get (widget, "default_outside_border", &tmp_border, NULL);

      if (tmp_border)
	{
	  *default_outside_border = *tmp_border;
	  g_free (tmp_border);
	}
      else
	*default_outside_border = default_default_outside_border;
    }

  if (interior_focus)
    gtk_widget_style_get (widget, "interior_focus", interior_focus, NULL);
}
	
static void
gtk_button_size_request (GtkWidget      *widget,
			 GtkRequisition *requisition)
{
  GtkButton *button = GTK_BUTTON (widget);
  GtkBorder default_border;
  gboolean interior_focus;

  gtk_button_get_props (button, &default_border, NULL, &interior_focus);
  
  requisition->width = (GTK_CONTAINER (widget)->border_width + CHILD_SPACING +
			GTK_WIDGET (widget)->style->xthickness) * 2;
  requisition->height = (GTK_CONTAINER (widget)->border_width + CHILD_SPACING +
			 GTK_WIDGET (widget)->style->ythickness) * 2;

  if (GTK_WIDGET_CAN_DEFAULT (widget))
    {
      requisition->width += default_border.left + default_border.right;
      requisition->height += default_border.top + default_border.bottom;
    }

  if (GTK_BIN (button)->child && GTK_WIDGET_VISIBLE (GTK_BIN (button)->child))
    {
      GtkRequisition child_requisition;

      gtk_widget_size_request (GTK_BIN (button)->child, &child_requisition);

      requisition->width += child_requisition.width;
      requisition->height += child_requisition.height;
    }

  if (interior_focus)
    {
      requisition->width += 2;
      requisition->height += 2;
    }
}

static void
gtk_button_size_allocate (GtkWidget     *widget,
			  GtkAllocation *allocation)
{
  GtkButton *button = GTK_BUTTON (widget);
  GtkAllocation child_allocation;

  gint border_width = GTK_CONTAINER (widget)->border_width;
  gint xthickness = GTK_WIDGET (widget)->style->xthickness;
  gint ythickness = GTK_WIDGET (widget)->style->ythickness;
  GtkBorder default_border;

  gtk_button_get_props (button, &default_border, NULL, NULL);
  
  widget->allocation = *allocation;

  if (GTK_WIDGET_REALIZED (widget))
    gdk_window_move_resize (widget->window,
			    widget->allocation.x + border_width,
			    widget->allocation.y + border_width,
			    widget->allocation.width - border_width * 2,
			    widget->allocation.height - border_width * 2);

  if (GTK_BIN (button)->child && GTK_WIDGET_VISIBLE (GTK_BIN (button)->child))
    {
      child_allocation.x = (CHILD_SPACING + xthickness);
      child_allocation.y = (CHILD_SPACING + ythickness);

      child_allocation.width = MAX (1, (gint)widget->allocation.width - child_allocation.x * 2 -
	                         border_width * 2);
      child_allocation.height = MAX (1, (gint)widget->allocation.height - child_allocation.y * 2 -
	                          border_width * 2);

      if (GTK_WIDGET_CAN_DEFAULT (button))
	{
	  child_allocation.x += default_border.left;
	  child_allocation.y += default_border.top;
	  child_allocation.width =  MAX (1, child_allocation.width - default_border.left - default_border.right);
	  child_allocation.height = MAX (1, child_allocation.height - default_border.top - default_border.bottom);
	}

      if (button->depressed)
	{
	  gint child_displacement_x;
	  gint child_displacement_y;
	  
	  gtk_widget_style_get (widget,
				"child_displacement_x", &child_displacement_x, 
				"child_displacement_y", &child_displacement_y,
				NULL);
	  child_allocation.x += child_displacement_x;
	  child_allocation.y += child_displacement_y;
	}

      gtk_widget_size_allocate (GTK_BIN (button)->child, &child_allocation);
    }
}

/*
 * +------------------------------------------------+
 * |                   BORDER                       |
 * |  +------------------------------------------+  |
 * |  |\\\\\\\\\\\\\\\\DEFAULT\\\\\\\\\\\\\\\\\  |  |
 * |  |\\+------------------------------------+  |  |
 * |  |\\| |           SPACING       3      | |  |  |
 * |  |\\| +--------------------------------+ |  |  |
 * |  |\\| |########## FOCUS ###############| |  |  |
 * |  |\\| |#+----------------------------+#| |  |  |
 * |  |\\| |#|         RELIEF            \|#| |  |  |
 * |  |\\| |#|  +-----------------------+\|#| |  |  |
 * |  |\\|1|#|  +     THE TEXT          +\|#|2|  |  |
 * |  |\\| |#|  +-----------------------+\|#| |  |  |
 * |  |\\| |#| \\\\\ ythickness \\\\\\\\\\|#| |  |  |
 * |  |\\| |#+----------------------------+#| |  |  |
 * |  |\\| |########### 1 ##################| |  |  |
 * |  |\\| +--------------------------------+ |  |  |
 * |  |\\| |        default spacing   4     | |  |  |
 * |  |\\+------------------------------------+  |  |
 * |  |\            ythickness                   |  |
 * |  +------------------------------------------+  |
 * |                border_width                    |
 * +------------------------------------------------+
 */

static void
gtk_button_paint (GtkWidget    *widget,
		  GdkRectangle *area)
{
  GtkButton *button;
  GtkShadowType shadow_type;
  gint width, height;
  gint x, y;
  GtkBorder default_border;
  GtkBorder default_outside_border;
  gboolean interior_focus;
   
  if (GTK_WIDGET_DRAWABLE (widget))
    {
      button = GTK_BUTTON (widget);

      gtk_button_get_props (button, &default_border, &default_outside_border, &interior_focus);
	
      x = 0;
      y = 0;
      width = widget->allocation.width - GTK_CONTAINER (widget)->border_width * 2;
      height = widget->allocation.height - GTK_CONTAINER (widget)->border_width * 2;

      gdk_window_set_back_pixmap (widget->window, NULL, TRUE);
      gdk_window_clear_area (widget->window, area->x, area->y, area->width, area->height);

      if (GTK_WIDGET_HAS_DEFAULT (widget) &&
	  GTK_BUTTON (widget)->relief == GTK_RELIEF_NORMAL)
	{
	  gtk_paint_box (widget->style, widget->window,
			 GTK_STATE_NORMAL, GTK_SHADOW_IN,
			 area, widget, "buttondefault",
			 x, y, width, height);

	  x += default_border.left;
	  y += default_border.top;
	  width -= default_border.left + default_border.right;
	  height -= default_border.top + default_border.bottom;
	}
      else if (GTK_WIDGET_CAN_DEFAULT (widget))
	{
	  x += default_outside_border.left;
	  y += default_outside_border.top;
	  width -= default_outside_border.left + default_outside_border.right;
	  height -= default_outside_border.top + default_outside_border.bottom;
	}
       
      if (!interior_focus && GTK_WIDGET_HAS_FOCUS (widget))
	{
	  x += 1;
	  y += 1;
	  width -= 2;
	  height -= 2;
	}

      shadow_type = button->depressed ? GTK_SHADOW_IN : GTK_SHADOW_OUT;

      if ((button->relief != GTK_RELIEF_NONE) ||
	  ((GTK_WIDGET_STATE(widget) != GTK_STATE_NORMAL) &&
	   (GTK_WIDGET_STATE(widget) != GTK_STATE_INSENSITIVE)))
	gtk_paint_box (widget->style, widget->window,
		       GTK_WIDGET_STATE (widget),
		       shadow_type, area, widget, "button",
		       x, y, width, height);
       
      if (GTK_WIDGET_HAS_FOCUS (widget))
	{
	  if (interior_focus)
	    {
	      x += widget->style->xthickness + 1;
	      y += widget->style->ythickness + 1;
	      width -= 2 * (widget->style->xthickness + 1);
	      height -=  2 * (widget->style->xthickness + 1);
	    }
	  else
	    {
	      x -= 1;
	      y -= 1;
	      width += 2;
	      height += 2;
	    }

	  gtk_paint_focus (widget->style, widget->window,
			   area, widget, "button",
			   x, y, width - 1, height - 1);
	}
    }
}

static gboolean
gtk_button_expose (GtkWidget      *widget,
		   GdkEventExpose *event)
{
  GtkBin *bin;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_BUTTON (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      bin = GTK_BIN (widget);
      
      gtk_button_paint (widget, &event->area);
      
      (* GTK_WIDGET_CLASS (parent_class)->expose_event) (widget, event);
    }
  
  return FALSE;
}

static gboolean
gtk_button_button_press (GtkWidget      *widget,
			 GdkEventButton *event)
{
  GtkButton *button;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_BUTTON (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (event->type == GDK_BUTTON_PRESS)
    {
      button = GTK_BUTTON (widget);

      if (!GTK_WIDGET_HAS_FOCUS (widget))
	gtk_widget_grab_focus (widget);

      if (event->button == 1)
	gtk_button_pressed (button);
    }

  return TRUE;
}

static gboolean
gtk_button_button_release (GtkWidget      *widget,
			   GdkEventButton *event)
{
  GtkButton *button;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_BUTTON (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (event->button == 1)
    {
      button = GTK_BUTTON (widget);
      gtk_button_released (button);
    }

  return TRUE;
}

static gboolean
gtk_button_key_release (GtkWidget   *widget,
			GdkEventKey *event)
{
  GtkButton *button = GTK_BUTTON (widget);

  if (button->activate_timeout)
    {
      gtk_button_finish_activate (button, TRUE);
      return TRUE;
    }
  else if (GTK_WIDGET_CLASS (parent_class)->key_release_event)
    return GTK_WIDGET_CLASS (parent_class)->key_release_event (widget, event);
  else
    return FALSE;
}

static gboolean
gtk_button_enter_notify (GtkWidget        *widget,
			 GdkEventCrossing *event)
{
  GtkButton *button;
  GtkWidget *event_widget;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_BUTTON (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  button = GTK_BUTTON (widget);
  event_widget = gtk_get_event_widget ((GdkEvent*) event);

  if ((event_widget == widget) &&
      (event->detail != GDK_NOTIFY_INFERIOR))
    {
      button->in_button = TRUE;
      gtk_button_enter (button);
    }

  return FALSE;
}

static gboolean
gtk_button_leave_notify (GtkWidget        *widget,
			 GdkEventCrossing *event)
{
  GtkButton *button;
  GtkWidget *event_widget;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_BUTTON (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  button = GTK_BUTTON (widget);
  event_widget = gtk_get_event_widget ((GdkEvent*) event);

  if ((event_widget == widget) &&
      (event->detail != GDK_NOTIFY_INFERIOR))
    {
      button->in_button = FALSE;
      gtk_button_leave (button);
    }

  return FALSE;
}

static void
gtk_real_button_pressed (GtkButton *button)
{
  if (button->activate_timeout)
    return;
  
  button->button_down = TRUE;
  gtk_button_update_state (button);
}

static void
gtk_real_button_released (GtkButton *button)
{
  if (button->button_down)
    {
      button->button_down = FALSE;

      if (button->activate_timeout)
	return;
      
      if (button->in_button)
	gtk_button_clicked (button);

      gtk_button_update_state (button);
    }
}

static gboolean
button_activate_timeout (gpointer data)
{
  GDK_THREADS_ENTER ();
  
  gtk_button_finish_activate (data, TRUE);

  GDK_THREADS_LEAVE ();

  return FALSE;
}

static void
gtk_real_button_activate (GtkButton *button)
{
  GtkWidget *widget = GTK_WIDGET (button);
  
  g_return_if_fail (GTK_IS_BUTTON (button));

  if (GTK_WIDGET_REALIZED (button) && !button->activate_timeout)
    {
      if (gdk_keyboard_grab (widget->window, TRUE,
			     gtk_get_current_event_time ()) == 0)
	{
	  gtk_grab_add (widget);
	  
	  button->activate_timeout = g_timeout_add (ACTIVATE_TIMEOUT,
						    button_activate_timeout,
						    button);
	  button->button_down = TRUE;
	  gtk_button_update_state (button);
	}
    }
}

static void
gtk_button_finish_activate (GtkButton *button,
			    gboolean   do_it)
{
  GtkWidget *widget = GTK_WIDGET (button);
  
  g_source_remove (button->activate_timeout);
  button->activate_timeout = 0;

  gdk_keyboard_ungrab (gtk_get_current_event_time ());
  gtk_grab_remove (widget);

  button->button_down = FALSE;

  gtk_button_update_state (button);

  if (do_it)
    gtk_button_clicked (button);
}

/**
 * gtk_button_set_label:
 * @button: a #GtkButton
 * @label: a string
 *
 * Sets the text of the label of the button to @str. This text is
 * also used to select the stock item if gtk_button_set_use_stock()
 * is used.
 *
 * This will also clear any previously set labels.
 **/
void
gtk_button_set_label (GtkButton   *button,
		      const gchar *label)
{
  g_return_if_fail (GTK_IS_BUTTON (button));
  
  g_free (button->label_text);
  button->label_text = g_strdup (label);
  
  gtk_button_construct_child (button);
  
  g_object_notify (G_OBJECT (button), "label");
}

/**
 * gtk_button_get_label:
 * @button: a #GtkButton
 *
 * Fetches the text from the label of the button, as set by
 * gtk_button_set_label().
 *
 * Return value: the text of the label widget. This string is
 *   owned by the widget and must not be modified or freed.
 *   If the label text has not been set the return value
 *   will be NULL. This will be the case if you create an
 *   empty button with gtk_button_new() to use as a container.
 **/
G_CONST_RETURN gchar *
gtk_button_get_label (GtkButton *button)
{
  g_return_val_if_fail (GTK_IS_BUTTON (button), NULL);
  
  return button->label_text;
}

/**
 * gtk_button_set_use_underline:
 * @button: a #GtkButton
 * @use_underline: %TRUE if underlines in the text indicate mnemonics
 *
 * If true, an underline in the text of the button label indicates
 * the next character should be used for the mnemonic accelerator key.
 */
void
gtk_button_set_use_underline (GtkButton *button,
			      gboolean   use_underline)
{
  g_return_if_fail (GTK_IS_BUTTON (button));

  use_underline = use_underline != FALSE;

  if (use_underline != button->use_underline)
    {
      button->use_underline = use_underline;
  
      gtk_button_construct_child (button);
      
      g_object_notify (G_OBJECT (button), "use_underline");
    }
}

/**
 * gtk_button_get_use_underline:
 * @label: a #GtkButton
 *
 * Returns whether an embedded underline in the button label indicates a
 * mnemonic. See gtk_button_set_use_underline ().
 *
 * Return value: %TRUE if an embedded underline in the button label
 *               indicates the mnemonic accelerator keys.
 **/
gboolean
gtk_button_get_use_underline (GtkButton *button)
{
  g_return_val_if_fail (GTK_IS_BUTTON (button), FALSE);
  
  return button->use_underline;
}

/**
 * gtk_button_set_use_stock:
 * @button: a #GtkButton
 * @use_stock: %TRUE if the button should use a stock item
 *
 * If true, the label set on the button is used as a
 * stock id to select the stock item for the button.
 */
void
gtk_button_set_use_stock (GtkButton *button,
			  gboolean   use_stock)
{
  g_return_if_fail (GTK_IS_BUTTON (button));

  use_stock = use_stock != FALSE;

  if (use_stock != button->use_stock)
    {
      button->use_stock = use_stock;
  
      gtk_button_construct_child (button);
      
      g_object_notify (G_OBJECT (button), "use_stock");
    }
}

/**
 * gtk_button_get_use_stock:
 * @button: a #GtkButton
 *
 * Returns whether the button label is a stock item.
 *
 * Return value: %TRUE if the button label is used to
 *               select a stock item instead of being
 *               used directly as the label text.
 */
gboolean
gtk_button_get_use_stock (GtkButton *button)
{
  g_return_val_if_fail (GTK_IS_BUTTON (button), FALSE);
  
  return button->use_stock;
}

/**
 * _gtk_button_set_depressed:
 * @button: a #GtkButton
 * @depressed: %TRUE if the button should be drawn with a recessed shadow.
 * 
 * Sets whether the button is currently drawn as down or not. This is 
 * purely a visual setting, and is meant only for use by derived widgets
 * such as #GtkToggleButton.
 **/
void
_gtk_button_set_depressed (GtkButton *button,
			   gboolean   depressed)
{
  GtkWidget *widget = GTK_WIDGET (button);

  depressed = depressed != FALSE;

  if (depressed != button->depressed)
    {
      button->depressed = depressed;
      gtk_widget_queue_resize (widget);
    }
}

static void
gtk_button_update_state (GtkButton *button)
{
  gboolean depressed;
  GtkStateType new_state;

  depressed = button->button_down && (button->in_button || button->activate_timeout);

  if (!button->button_down && button->in_button)
    new_state = GTK_STATE_PRELIGHT;
  else
    new_state = depressed ? GTK_STATE_ACTIVE: GTK_STATE_NORMAL;

  _gtk_button_set_depressed (button, depressed); 
  gtk_widget_set_state (GTK_WIDGET (button), new_state);
}
