/* gtkcombo - combo widget for gtk+
 * Copyright 1997 Paolo Molaro
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

#include <string.h>

#include "gtkarrow.h"
#include "gtklabel.h"
#include "gtklist.h"
#include "gtkentry.h"
#include "gtkeventbox.h"
#include "gtkbutton.h"
#include "gtklistitem.h"
#include "gtkscrolledwindow.h"
#include "gtkmain.h"
#include "gtksignal.h"
#include "gtkwindow.h"
#include "gdk/gdkkeysyms.h"
#include "gtkcombo.h"
#include "gtkframe.h"

const gchar *gtk_combo_string_key = "gtk-combo-string-value";

#define COMBO_LIST_MAX_HEIGHT 400

static void         gtk_combo_class_init      (GtkComboClass *klass);
static void         gtk_combo_init            (GtkCombo      *combo);
static void         gtk_combo_destroy         (GtkObject     *combo);
static GtkListItem *gtk_combo_find            (GtkCombo      *combo);
static gchar *      gtk_combo_func            (GtkListItem  *li);
static gint         gtk_combo_focus_idle      (GtkCombo      *combo);
static gint         gtk_combo_entry_focus_out (GtkEntry      *entry, 
                                               GdkEventFocus *event, 
                                               GtkCombo      *combo);
static void         gtk_combo_get_pos         (GtkCombo      *combo, 
                                               gint          *x, 
                                               gint          *y, 
                                               gint          *height, 
                                               gint          *width);
static void         gtk_combo_popup_list      (GtkButton     *button, 
                                               GtkCombo      *combo);
static void         gtk_combo_update_entry    (GtkList       *list, 
                                               GtkCombo      *combo);
static void         gtk_combo_update_list     (GtkEntry      *entry, 
                                               GtkCombo      *combo);
static gint         gtk_combo_button_press    (GtkWidget     *widget,
				               GdkEvent      *event,
				               GtkCombo      *combo);
static gint         gtk_combo_list_key_press  (GtkWidget     *widget, 
                                               GdkEventKey   *event, 
                                               GtkCombo      *combo);
static gint         gtk_combo_entry_key_press (GtkEntry      *widget, 
                                               GdkEventKey   *event, 
                                               GtkCombo      *combo);
static void         gtk_combo_item_destroy    (GtkObject     *object);
static void         gtk_combo_size_allocate   (GtkWidget     *widget,
					       GtkAllocation *allocation);

static GtkHBoxClass *parent_class = NULL;

static void
gtk_combo_class_init (GtkComboClass * klass)
{
  GtkObjectClass *oclass;
  GtkWidgetClass *widget_class;

  parent_class = gtk_type_class (gtk_hbox_get_type ());
  oclass = (GtkObjectClass *) klass;
  widget_class = (GtkWidgetClass *) klass;

  oclass->destroy = gtk_combo_destroy;
  
  widget_class->size_allocate = gtk_combo_size_allocate;
}

static void
gtk_combo_destroy (GtkObject * combo)
{
  gtk_widget_destroy (GTK_COMBO (combo)->popwin);
  gtk_widget_unref (GTK_COMBO (combo)->popwin);

  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (*GTK_OBJECT_CLASS (parent_class)->destroy) (combo);
}

static int
gtk_combo_entry_key_press (GtkEntry * entry, GdkEventKey * event, GtkCombo * combo)
{
  GList *li;
  /* completion? */
  /*if ( event->keyval == GDK_Tab ) {
     gtk_signal_emit_stop_by_name (GTK_OBJECT (entry), "key_press_event");
     return TRUE;
     } else */
  if (!combo->use_arrows || !GTK_LIST (combo->list)->children)
    return FALSE;
  li = g_list_find (GTK_LIST (combo->list)->children, gtk_combo_find (combo));

  if ((event->keyval == GDK_Up)
      || (event->keyval == GDK_KP_Up)
      || ((event->state & GDK_MOD1_MASK) && ((event->keyval == 'p') || (event->keyval == 'P'))))
    {
      if (li)
	li = li->prev;
      if (!li && combo->use_arrows_always)
	{
	  li = g_list_last (GTK_LIST (combo->list)->children);
	}
      if (li)
	{
	  gtk_list_select_child (GTK_LIST (combo->list), GTK_WIDGET (li->data));
	  gtk_signal_emit_stop_by_name (GTK_OBJECT (entry), "key_press_event");
	  return TRUE;
	}
    }
  else if ((event->keyval == GDK_Down)
	   || (event->keyval == GDK_KP_Down)
	   || ((event->state & GDK_MOD1_MASK) && ((event->keyval == 'n') || (event->keyval == 'N'))))
    {
      if (li)
	li = li->next;
      if (!li && combo->use_arrows_always)
	{
	  li = GTK_LIST (combo->list)->children;
	}
      if (li)
	{
	  gtk_list_select_child (GTK_LIST (combo->list), GTK_WIDGET (li->data));
	  gtk_signal_emit_stop_by_name (GTK_OBJECT (entry), "key_press_event");
	  return TRUE;
	}
    }
  return FALSE;
}

static GtkListItem *
gtk_combo_find (GtkCombo * combo)
{
  gchar *text;
  gchar *ltext;
  GList *clist;
  int (*string_compare) (const char *, const char *);

  if (combo->case_sensitive)
    string_compare = strcmp;
  else
    string_compare = g_strcasecmp;

  text = gtk_entry_get_text (GTK_ENTRY (combo->entry));
  clist = GTK_LIST (combo->list)->children;

  while (clist && clist->data)
    {
      ltext = gtk_combo_func (GTK_LIST_ITEM (clist->data));
      if (!ltext)
	continue;
      if (!(*string_compare) (ltext, text))
	return (GtkListItem *) clist->data;
      clist = clist->next;
    }

  return NULL;
}

static gchar *
gtk_combo_func (GtkListItem * li)
{
  GtkWidget *label;
  gchar *ltext = NULL;

  ltext = (gchar *) gtk_object_get_data (GTK_OBJECT (li), gtk_combo_string_key);
  if (!ltext)
    {
      label = GTK_BIN (li)->child;
      if (!label || !GTK_IS_LABEL (label))
	return NULL;
      gtk_label_get (GTK_LABEL (label), &ltext);
    }
  return ltext;
}

static gint
gtk_combo_focus_idle (GtkCombo * combo)
{
  if (combo)
    gtk_widget_grab_focus (combo->entry);
  return FALSE;
}

static gint
gtk_combo_entry_focus_out (GtkEntry * entry, GdkEventFocus * event, GtkCombo * combo)
{

  if (combo->value_in_list && !gtk_combo_find (combo))
    {
      /* gdk_beep(); *//* this can be annoying */
      if (combo->ok_if_empty && !strcmp (gtk_entry_get_text (entry), ""))
	return FALSE;
#ifdef TEST
      printf ("INVALID ENTRY: `%s'\n", gtk_entry_get_text (entry));
#endif
      gtk_grab_add (GTK_WIDGET (combo));
      /* this is needed because if we call gtk_widget_grab_focus() 
         it isn't guaranteed it's the *last* call before the main-loop,
         so the focus can be lost anyway...
         the signal_emit_stop doesn't seem to work either...
       */
      gtk_idle_add ((GtkFunction) gtk_combo_focus_idle, combo);
      /*gtk_signal_emit_stop_by_name(GTK_OBJECT(entry), "focus_out_event"); */
      return TRUE;
    }
  return FALSE;
}

static void
gtk_combo_get_pos (GtkCombo * combo, gint * x, gint * y, gint * height, gint * width)
{
  GtkBin *popwin;
  GtkWidget *widget;
  GtkScrolledWindow *popup;

  gint real_height;
  GtkRequisition list_requisition;
  gboolean show_hscroll = FALSE;
  gboolean show_vscroll = FALSE;
  gint avail_height;
  gint min_height;
  gint alloc_width;
  gint work_height;
  gint old_height;
  gint old_width;

  widget = GTK_WIDGET(combo);
  popup  = GTK_SCROLLED_WINDOW (combo->popup);
  popwin = GTK_BIN (combo->popwin);

  gdk_window_get_origin (combo->entry->window, x, y);
  real_height = MIN (combo->entry->requisition.height, 
		     combo->entry->allocation.height);
  *y += real_height;
  avail_height = gdk_screen_height () - *y;

  gtk_widget_size_request (combo->list, &list_requisition);
  min_height = MIN (list_requisition.height, 
		    popup->vscrollbar->requisition.height);


  alloc_width = widget->allocation.width -
    2 * popwin->child->style->klass->xthickness -
    2 * GTK_CONTAINER (popwin->child)->border_width -
    2 * GTK_CONTAINER (combo->popup)->border_width -
    2 * GTK_CONTAINER (popup->viewport)->border_width - 
    2 * popup->viewport->style->klass->xthickness;

  work_height =  
    2 * popwin->child->style->klass->ythickness +
    2 * GTK_CONTAINER (popwin->child)->border_width +
    2 * GTK_CONTAINER (combo->popup)->border_width +
    2 * GTK_CONTAINER (popup->viewport)->border_width +
    2 * popup->viewport->style->klass->xthickness;

  do 
    {
      old_width = alloc_width;
      old_height = work_height;

      if (!show_hscroll &&
	  alloc_width < list_requisition.width)
	{
	  work_height += popup->hscrollbar->requisition.height +
	    GTK_SCROLLED_WINDOW_CLASS 
	    (GTK_OBJECT (combo->popup)->klass)->scrollbar_spacing;
	  show_hscroll = TRUE;
	}
      if (!show_vscroll && 
	  work_height + list_requisition.height > avail_height)
	{
	  if (work_height + min_height > avail_height && 
	      *y - real_height > avail_height)
	    {
	      *y -= (work_height +list_requisition.height + real_height);
	      break;
	    }
	  alloc_width -= 
	    popup->vscrollbar->requisition.width +
	    GTK_SCROLLED_WINDOW_CLASS 
	    (GTK_OBJECT (combo->popup)->klass)->scrollbar_spacing;
	  show_vscroll = TRUE;
	}
    } while (old_width != alloc_width || old_height != work_height);

  *width = widget->allocation.width;
  if (show_vscroll)
      *height = avail_height;
  else
    *height = work_height + list_requisition.height;
  
  if (*x < 0)
    *x = 0;
}

static void
gtk_combo_popup_list (GtkButton * button, GtkCombo * combo)
{
  gint height, width, x, y;
  gint old_width, old_height;

  if (!GTK_LIST (combo->list)->children)
    return;

  old_width = combo->popwin->allocation.width;
  old_height  = combo->popwin->allocation.height;

  gtk_combo_get_pos (combo, &x, &y, &height, &width);

  /* workaround for gtk_scrolled_window_size_allocate bug */
  if (old_width != width || old_height != height)
    {
      gtk_widget_hide (GTK_SCROLLED_WINDOW (combo->popup)->hscrollbar);
      gtk_widget_hide (GTK_SCROLLED_WINDOW (combo->popup)->vscrollbar);
    }

  gtk_widget_set_uposition (combo->popwin, x, y);
  gtk_widget_set_usize (combo->popwin, width, height);
  gtk_widget_realize (combo->popwin);
  gdk_window_resize (combo->popwin->window, width, height);
  gtk_widget_show (combo->popwin);

  gtk_widget_grab_focus (combo->popwin);
  gtk_grab_add (combo->popwin);
  gdk_pointer_grab (combo->popwin->window, TRUE,
		    GDK_BUTTON_PRESS_MASK | 
		    GDK_BUTTON_RELEASE_MASK |
		    GDK_POINTER_MOTION_MASK, 
		    NULL, NULL, GDK_CURRENT_TIME);
}

#if 0
static void
prelight_bug (GtkButton * b, GtkCombo * combo)
{
  /* avoid prelight state... */
  gtk_widget_set_state (combo->button, GTK_STATE_NORMAL);

}
#endif

static void
gtk_combo_update_entry (GtkList * list, GtkCombo * combo)
{
  char *text;

  gtk_grab_remove (GTK_WIDGET (combo));
  gtk_signal_handler_block (GTK_OBJECT (list), combo->list_change_id);
  if (list->selection)
    {
      text = gtk_combo_func (GTK_LIST_ITEM (list->selection->data));
      if (!text)
	text = "";
      gtk_entry_set_text (GTK_ENTRY (combo->entry), text);
    }
  gtk_widget_hide (combo->popwin);
  gtk_grab_remove (combo->popwin);
  gdk_pointer_ungrab (GDK_CURRENT_TIME);
  gtk_signal_handler_unblock (GTK_OBJECT (list), combo->list_change_id);
}

static void
gtk_combo_update_list (GtkEntry * entry, GtkCombo * combo)
{
  GtkList *list = GTK_LIST (combo->list);
  GList *slist = list->selection;
  GtkListItem *li;

  gtk_grab_remove (GTK_WIDGET (combo));

  gtk_signal_handler_block (GTK_OBJECT (entry), combo->entry_change_id);
  if (slist && slist->data)
    gtk_list_unselect_child (list, GTK_WIDGET (slist->data));
  li = gtk_combo_find (combo);
  if (li)
    gtk_list_select_child (list, GTK_WIDGET (li));
  gtk_signal_handler_unblock (GTK_OBJECT (entry), combo->entry_change_id);
}

static gint
gtk_combo_button_press (GtkWidget * widget, GdkEvent * event, GtkCombo * combo)
{
  GtkWidget *child;

  child = gtk_get_event_widget (event);

  /* We don't ask for button press events on the grab widget, so
   *  if an event is reported directly to the grab widget, it must
   *  be on a window outside the application (and thus we remove
   *  the popup window). Otherwise, we check if the widget is a child
   *  of the grab widget, and only remove the popup window if it
   *  is not.
   */
  if (child != widget)
    {
      while (child)
	{
	  if (child == widget)
	    return FALSE;
	  child = child->parent;
	}
    }

  gtk_widget_hide (combo->popwin);
  gtk_grab_remove (combo->popwin);
  gdk_pointer_ungrab (GDK_CURRENT_TIME);

  return TRUE;
}

static int
gtk_combo_list_key_press (GtkWidget * widget, GdkEventKey * event, GtkCombo * combo)
{
  if (event->keyval == GDK_Escape)
    {
      gtk_widget_hide (combo->popwin);
      gtk_grab_remove (combo->popwin);
      gdk_pointer_ungrab (GDK_CURRENT_TIME);
      return TRUE;
    }
  return FALSE;
}

static void
gtk_combo_init (GtkCombo * combo)
{
  GtkWidget *arrow;
  GtkWidget *frame;
  GtkWidget *event_box;
  GdkCursor *cursor;

  combo->case_sensitive = 0;
  combo->value_in_list = 0;
  combo->ok_if_empty = 1;
  combo->use_arrows = 1;
  combo->use_arrows_always = 0;
  combo->entry = gtk_entry_new ();
  combo->button = gtk_button_new ();
  arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_OUT);
  gtk_widget_show (arrow);
  gtk_container_add (GTK_CONTAINER (combo->button), arrow);
  gtk_box_pack_start (GTK_BOX (combo), combo->entry, TRUE, TRUE, 0);
  gtk_box_pack_end (GTK_BOX (combo), combo->button, FALSE, FALSE, 0);
  gtk_widget_show (combo->entry);
  gtk_widget_show (combo->button);
  combo->entry_change_id = gtk_signal_connect (GTK_OBJECT (combo->entry), "changed",
			      (GtkSignalFunc) gtk_combo_update_list, combo);
  gtk_signal_connect (GTK_OBJECT (combo->entry), "key_press_event",
		      (GtkSignalFunc) gtk_combo_entry_key_press, combo);
  gtk_signal_connect_after (GTK_OBJECT (combo->entry), "focus_out_event",
			    (GtkSignalFunc) gtk_combo_entry_focus_out, combo);
  combo->activate_id = gtk_signal_connect (GTK_OBJECT (combo->entry), "activate",
		      (GtkSignalFunc) gtk_combo_popup_list, combo);
  gtk_signal_connect (GTK_OBJECT (combo->button), "clicked",
		      (GtkSignalFunc) gtk_combo_popup_list, combo);
  /*gtk_signal_connect(GTK_OBJECT(combo->button), "clicked",
     (GtkSignalFunc)prelight_bug, combo); */

  combo->popwin = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_widget_ref (combo->popwin);
  gtk_window_set_policy (GTK_WINDOW (combo->popwin), 1, 1, 0);
  
  gtk_widget_set_events (combo->popwin, GDK_KEY_PRESS_MASK);

  event_box = gtk_event_box_new ();
  gtk_container_add (GTK_CONTAINER (combo->popwin), event_box);
  gtk_widget_show (event_box);

  gtk_widget_realize (event_box);
  cursor = gdk_cursor_new (GDK_TOP_LEFT_ARROW);
  gdk_window_set_cursor (event_box->window, cursor);
  gdk_cursor_destroy (cursor);

  frame = gtk_frame_new (NULL);
  gtk_container_add (GTK_CONTAINER (event_box), frame);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
  gtk_widget_show (frame);

  combo->popup = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (combo->popup),
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  combo->list = gtk_list_new ();
  gtk_list_set_selection_mode(GTK_LIST(combo->list), GTK_SELECTION_BROWSE);
  gtk_container_add (GTK_CONTAINER (frame), combo->popup);
  gtk_container_add (GTK_CONTAINER (combo->popup), combo->list);
  gtk_widget_show (combo->list);
  gtk_widget_show (combo->popup);

  combo->list_change_id = gtk_signal_connect (GTK_OBJECT (combo->list), "selection_changed",
			     (GtkSignalFunc) gtk_combo_update_entry, combo);
  gtk_signal_connect (GTK_OBJECT (combo->popwin), "key_press_event",
		      (GtkSignalFunc) gtk_combo_list_key_press, combo);
  gtk_signal_connect (GTK_OBJECT (combo->popwin), "button_press_event",
		      GTK_SIGNAL_FUNC (gtk_combo_button_press), combo);
  
}

guint
gtk_combo_get_type ()
{
  static guint combo_type = 0;

  if (!combo_type)
    {
      GtkTypeInfo combo_info =
      {
	"GtkCombo",
	sizeof (GtkCombo),
	sizeof (GtkComboClass),
	(GtkClassInitFunc) gtk_combo_class_init,
	(GtkObjectInitFunc) gtk_combo_init,
	(GtkArgSetFunc) NULL,
	(GtkArgGetFunc) NULL,
      };
      combo_type = gtk_type_unique (gtk_hbox_get_type (), &combo_info);
    }
  return combo_type;
}

GtkWidget *
gtk_combo_new ()
{
  return GTK_WIDGET (gtk_type_new (gtk_combo_get_type ()));
}

void
gtk_combo_set_value_in_list (GtkCombo * combo, gint val, gint ok_if_empty)
{
  g_return_if_fail (combo != NULL);
  g_return_if_fail (GTK_IS_COMBO (combo));

  combo->value_in_list = val;
  combo->ok_if_empty = ok_if_empty;
}

void
gtk_combo_set_case_sensitive (GtkCombo * combo, gint val)
{
  g_return_if_fail (combo != NULL);
  g_return_if_fail (GTK_IS_COMBO (combo));

  combo->case_sensitive = val;
}

void
gtk_combo_set_use_arrows (GtkCombo * combo, gint val)
{
  g_return_if_fail (combo != NULL);
  g_return_if_fail (GTK_IS_COMBO (combo));

  combo->use_arrows = val;
}

void
gtk_combo_set_use_arrows_always (GtkCombo * combo, gint val)
{
  g_return_if_fail (combo != NULL);
  g_return_if_fail (GTK_IS_COMBO (combo));

  combo->use_arrows_always = val;
  combo->use_arrows = 1;
}

void
gtk_combo_set_popdown_strings (GtkCombo * combo, GList * strings)
{
  GList *list;
  GtkWidget *li;

  g_return_if_fail (combo != NULL);
  g_return_if_fail (GTK_IS_COMBO (combo));
  g_return_if_fail (strings != NULL);

  gtk_list_clear_items (GTK_LIST (combo->list), 0, -1);
  list = strings;
  while (list)
    {
      li = gtk_list_item_new_with_label ((gchar *) list->data);
      gtk_widget_show (li);
      gtk_container_add (GTK_CONTAINER (combo->list), li);
      list = list->next;
    }
}

static void
gtk_combo_item_destroy (GtkObject * object)
{
  gchar *key;

  key = gtk_object_get_data (object, gtk_combo_string_key);
  if (key)
    g_free (key);
}

void
gtk_combo_set_item_string (GtkCombo * combo, GtkItem * item, const gchar * item_value)
{
  gchar *val;
  gint connected = 0;

  g_return_if_fail (combo != NULL);
  g_return_if_fail (GTK_IS_COMBO (combo));
  g_return_if_fail (item != NULL);

  val = gtk_object_get_data (GTK_OBJECT (item), gtk_combo_string_key);
  if (val) 
    {
      g_free (val);
      connected = 1;
    }
  if (item_value)
    {
      val = g_strdup(item_value);
      gtk_object_set_data (GTK_OBJECT (item), gtk_combo_string_key, val);
      if (!connected)
        gtk_signal_connect (GTK_OBJECT (item), "destroy",
			  (GtkSignalFunc) gtk_combo_item_destroy, val);
    }
  else 
    {
      gtk_object_set_data (GTK_OBJECT (item), gtk_combo_string_key, NULL);
      if (connected)
	gtk_signal_disconnect_by_data(GTK_OBJECT (item), val);
    }
}

static void
gtk_combo_size_allocate (GtkWidget     *widget,
			 GtkAllocation *allocation)
{
  GtkCombo *combo;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_COMBO (widget));
  g_return_if_fail (allocation != NULL);

  GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);
  
  combo = GTK_COMBO (widget);

  if (combo->entry->allocation.height > combo->entry->requisition.height)
    {
      GtkAllocation button_allocation;

      button_allocation = combo->button->allocation;
      button_allocation.height = combo->entry->requisition.height;
      button_allocation.y = combo->entry->allocation.y + 
	(combo->entry->allocation.height - combo->entry->requisition.height) 
	/ 2;
      gtk_widget_size_allocate (combo->button, &button_allocation);
    }
}

void
gtk_combo_disable_activate (GtkCombo* combo)
{
  g_return_if_fail (combo != NULL);
  g_return_if_fail (GTK_IS_COMBO (combo));

  if ( combo->activate_id ) {
    gtk_signal_disconnect(GTK_OBJECT(combo), combo->activate_id);
    combo->activate_id = 0;
  }
}
