/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998 Elliot Lee
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
/* NOTES about this widget:
   It would be nice if we kept track of the last menu item that
   people selected, so that we could allow choosing text by using
   up/down arrows. 
   -- Elliot
*/
#include <ctype.h>
#include <string.h>
#include "gdk/gdkx.h"
#include "gdk/gdkkeysyms.h"
#include "gdk/gdki18n.h"
#include "gtkcombobox.h"
#include "gtkmain.h"
#include "gtksignal.h"
#include "gtkmenu.h"
#include "gtkmenuitem.h"
#include "gtklabel.h"
#include "gtkbin.h"

#define ARROW_SIZE_X 20
#define ARROW_SIZE_Y 20
#define ARROW_PADDING 4

static void gtk_combo_box_class_init   (GtkComboBoxClass *class);
static void gtk_combo_box_init         (GtkComboBox *combobox);
static void gtk_combo_box_size_request (GtkWidget      *widget,
					GtkRequisition *requisition);
static void gtk_combo_box_size_allocate(GtkWidget     *widget,
					GtkAllocation *allocation);
static void gtk_combo_box_draw         (GtkWidget    *widget,
					GdkRectangle *area);
static void gtk_combo_box_draw_arrow   (GtkWidget *widget);
static gint gtk_combo_box_expose       (GtkWidget *object,
					GdkEventExpose *event);
static void gtk_combo_box_destroy      (GtkObject *object);
#if 0
static gint gtk_combo_box_key_press    (GtkWidget   *widget,
					GdkEventKey *event);
#endif
static gint gtk_combo_box_button_press (GtkWidget      *widget,
					GdkEventButton *event);
static void gtk_combo_box_realize      (GtkWidget      *widget);
static void gtk_combo_box_item_activate(GtkWidget      *widget,
					GtkComboBox    *cb);
static void gtk_combo_box_set_down     (GtkWidget      *widget,
					GtkComboBox    *cb);
static void gtk_combo_box_set_up       (GtkWidget      *widget,
					GtkComboBox    *cb);

static GtkEntryClass *parent_class = NULL;

guint
gtk_combo_box_get_type ()
{
  static guint combo_box_type = 0;

  if (!combo_box_type)
    {
      GtkTypeInfo combo_box_info =
      {
	"GtkComboBox",
	sizeof (GtkComboBox),
	sizeof (GtkComboBoxClass),
	(GtkClassInitFunc) gtk_combo_box_class_init,
	(GtkObjectInitFunc) gtk_combo_box_init,
	(GtkArgFunc) NULL,
      };

      combo_box_type = gtk_type_unique (gtk_entry_get_type (), &combo_box_info);
    }

  return combo_box_type;
}

static void
gtk_combo_box_class_init (GtkComboBoxClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GtkObjectClass *) class;
  widget_class = (GtkWidgetClass *) class;

  parent_class = gtk_type_class (gtk_entry_get_type ());

  object_class->destroy = gtk_combo_box_destroy;

  widget_class->button_press_event = gtk_combo_box_button_press;
#if 0
  widget_class->key_press_event = gtk_combo_box_key_press;
#endif
  widget_class->size_request = gtk_combo_box_size_request;
  widget_class->size_allocate = gtk_combo_box_size_allocate;
  widget_class->draw = gtk_combo_box_draw;
  widget_class->expose_event = gtk_combo_box_expose;
  widget_class->realize = gtk_combo_box_realize;

}

static void
gtk_combo_box_init (GtkComboBox *combobox)
{
  GTK_WIDGET_SET_FLAGS (combobox, GTK_CAN_FOCUS);

  combobox->popdown = NULL;
  combobox->menu_is_down = FALSE;
}

GtkWidget*
gtk_combo_box_new (GList *popdown_strings)
{
  GtkWidget *retval = GTK_WIDGET (gtk_type_new (gtk_combo_box_get_type ()));
  if(popdown_strings)
    gtk_combo_box_set_popdown_strings(GTK_COMBO_BOX(retval), popdown_strings);
  return retval;
}

GtkWidget*
gtk_combo_box_new_with_max_length (GList *popdown_strings,
				   guint16 max)
{
  GtkWidget *retval;
  retval = GTK_WIDGET(gtk_type_new (gtk_combo_box_get_type ()));
  GTK_ENTRY(retval)->text_max_length = max;
  if(popdown_strings)
    gtk_combo_box_set_popdown_strings(GTK_COMBO_BOX(retval), popdown_strings);
  return retval;
}

void
gtk_combo_box_set_popdown_strings(GtkComboBox *combobox,
				  GList *popdown_strings)
{
  GList *t;

  g_return_if_fail(combobox != NULL);
  g_return_if_fail(popdown_strings != NULL);
  if(combobox->popdown)
    gtk_widget_destroy(combobox->popdown);

  combobox->popdown = gtk_menu_new();
  gtk_signal_connect(GTK_OBJECT(combobox->popdown),
		     "show",
		     GTK_SIGNAL_FUNC(gtk_combo_box_set_down),
		     combobox);
  gtk_signal_connect(GTK_OBJECT(combobox->popdown),
		     "hide",
		     GTK_SIGNAL_FUNC(gtk_combo_box_set_up),
		     combobox);

  for(t = popdown_strings; t; t = t->next)
    {
      GtkWidget *mitem;
      mitem = gtk_menu_item_new_with_label(t->data);
      gtk_widget_show(mitem);
      gtk_menu_append(GTK_MENU(combobox->popdown), mitem);
      gtk_signal_connect(GTK_OBJECT(mitem), "activate",
			 GTK_SIGNAL_FUNC(gtk_combo_box_item_activate),
			 combobox);
    }

  if(GTK_WIDGET_REALIZED(combobox))
    gtk_widget_set_usize(combobox->popdown,
			 GTK_WIDGET(combobox)->requisition.width,
			 -1);
}

static void
gtk_combo_box_realize(GtkWidget *widget)
{
  GtkComboBox *cb;

  g_return_if_fail(widget != NULL);
  g_return_if_fail(GTK_IS_COMBO_BOX(widget));

  cb = GTK_COMBO_BOX(widget);
  GTK_WIDGET_CLASS(parent_class)->realize(widget);
  gdk_window_set_background (GTK_WIDGET(widget)->window,
			     &widget->style->bg[GTK_WIDGET_STATE(widget)]);

  if(cb->popdown)
    {
      gtk_widget_set_usize(cb->popdown,
			   widget->requisition.width,
			   -1);
    }
}

static void
gtk_combo_box_size_request (GtkWidget      *widget,
			    GtkRequisition *requisition)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_COMBO_BOX (widget));
  g_return_if_fail (requisition != NULL);

  GTK_WIDGET_CLASS(parent_class)->size_request(widget, requisition);

  requisition->width += ARROW_SIZE_X;
  if(requisition->height < ARROW_SIZE_Y)
    requisition->height = ARROW_SIZE_Y;
}

static void
gtk_combo_box_size_allocate (GtkWidget     *widget,
			     GtkAllocation *allocation)
{
  GtkComboBox *cb;
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_COMBO_BOX (widget));
  g_return_if_fail (allocation != NULL);

  cb = GTK_COMBO_BOX(widget);

  widget->allocation = *allocation;

  GTK_WIDGET_CLASS(parent_class)->size_allocate(widget, allocation);
#define INNER_BORDER 2
  gdk_window_resize(GTK_ENTRY(widget)->text_area,
		    allocation->width - (widget->style->klass->xthickness + INNER_BORDER) * 2 - ARROW_SIZE_X,
		    widget->requisition.height - (widget->style->klass->ythickness + INNER_BORDER) * 2);
}

static void
gtk_combo_box_draw_arrow   (GtkWidget *widget)
{
  gint sx, sy;

  sx = widget->allocation.width - ARROW_SIZE_X;
  sy = ARROW_PADDING;
  widget->style->klass->draw_arrow(widget->style,
				   widget->window,
				   GTK_WIDGET_STATE(widget),
				   GTK_SHADOW_OUT,
				   GTK_ARROW_DOWN, TRUE,
				   sx, sy,
				   ARROW_SIZE_X - ARROW_PADDING,
				   ARROW_SIZE_Y - ARROW_PADDING - 2);
}

static void
gtk_combo_box_draw (GtkWidget    *widget,
		    GdkRectangle *area)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_COMBO_BOX (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gtk_combo_box_draw_arrow(widget);
      GTK_WIDGET_CLASS(parent_class)->draw(widget, area);
    }
}

static gint
gtk_combo_box_expose (GtkWidget      *widget,
		      GdkEventExpose *event)
{
  gint retval;
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_ENTRY (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  gtk_combo_box_draw_arrow(widget);
  retval = GTK_WIDGET_CLASS(parent_class)->expose_event(widget, event);
  return retval;
}

static void
gtk_combo_box_position_menu(GtkMenu  *menu,
			    gint     *x,
			    gint     *y,
			    gpointer  user_data)
{
  GtkComboBox *cb;
  GtkWidget *widget;
  gint rootw, rooth, wy;

  g_return_if_fail(user_data != NULL);
  g_return_if_fail(GTK_IS_COMBO_BOX(user_data));

  cb = GTK_COMBO_BOX(user_data);
  widget = GTK_WIDGET(user_data);

  gdk_window_get_origin(widget->window, x, &wy);
  *y = wy + widget->allocation.height;

  gdk_window_get_size(GDK_ROOT_PARENT(), &rootw, &rooth);

  /* This check isn't perfect - what if the menu is too big to fit on
     the screen going either up or down? *sigh* not a perfect
     world, I guess */
  if((*y + GTK_WIDGET(menu)->requisition.height) > rooth)
    *y = wy - GTK_WIDGET(menu)->requisition.height;
}

static void
gtk_combo_box_set_down (GtkWidget *widget,
			GtkComboBox *cb)
{
  g_return_if_fail(GTK_IS_COMBO_BOX(cb));

  cb->menu_is_down = TRUE;
}

static void
gtk_combo_box_set_up (GtkWidget *widget,
		      GtkComboBox *cb)
{
  g_return_if_fail(GTK_IS_COMBO_BOX(cb));

  cb->menu_is_down = FALSE;
}

static gint
gtk_combo_box_button_press (GtkWidget      *widget,
			    GdkEventButton *event)
{
  GtkComboBox *cb;
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_ENTRY (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  cb = GTK_COMBO_BOX(widget);

  if(event->window == widget->window
     && event->x > (widget->allocation.width - ARROW_SIZE_X)
     && event->x < widget->allocation.width
     && event->button == 1)
    {
      gtk_menu_popup(GTK_MENU(cb->popdown),
		     NULL, NULL,
		     gtk_combo_box_position_menu,
		     cb, event->button,
		     event->time);
      return TRUE;
    }
  else
    return GTK_WIDGET_CLASS(parent_class)->button_press_event(widget, event);
}

#if 0
static gint
gtk_combo_box_key_press (GtkWidget   *widget,
			 GdkEventKey *event)
{

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_ENTRY (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  return GTK_WIDGET_CLASS(parent_class)->key_press_event(widget, event);
}
#endif

static void
gtk_combo_box_destroy(GtkObject *object)
{
  GtkComboBox *cb;
  g_return_if_fail(GTK_IS_COMBO_BOX(object));

  cb = GTK_COMBO_BOX(object);
  if(cb->popdown)
    gtk_widget_destroy(cb->popdown);

  GTK_OBJECT_CLASS(parent_class)->destroy(object);
}

static void gtk_combo_box_item_activate(GtkWidget *widget,
					GtkComboBox *cb)
{
  char *newtext = GTK_LABEL(GTK_BIN(widget)->child)->label;

  gtk_entry_set_text(GTK_ENTRY(cb), newtext);
}
