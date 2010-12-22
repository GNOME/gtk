/* gtkcombo - combo widget for gtk+
 * Copyright 1997 Paolo Molaro
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
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

/* Do NOT, I repeat, NOT, copy any of the code in this file.
 * The code here relies on all sorts of internal details of GTK+
 */

#undef GTK_DISABLE_DEPRECATED
/* For GCompletion */
#undef G_DISABLE_DEPRECATED

#include "config.h"
#include <string.h>

#include <gdk/gdkkeysyms.h>

#include "gtkarrow.h"
#include "gtklabel.h"
#include "gtklist.h"
#include "gtkentry.h"
#include "gtkeventbox.h"
#include "gtkbutton.h"
#include "gtklistitem.h"
#include "gtkscrolledwindow.h"
#include "gtkmain.h"
#include "gtkwindow.h"
#include "gtkcombo.h"
#include "gtkframe.h"
#include "gtkprivate.h"
#include "gtkintl.h"

#include "gtkalias.h"

static const gchar gtk_combo_string_key[] = "gtk-combo-string-value";

#define COMBO_LIST_MAX_HEIGHT	(400)
#define	EMPTY_LIST_HEIGHT	(15)

enum {
  PROP_0,
  PROP_ENABLE_ARROW_KEYS,
  PROP_ENABLE_ARROWS_ALWAYS,
  PROP_CASE_SENSITIVE,
  PROP_ALLOW_EMPTY,
  PROP_VALUE_IN_LIST
};

static void         gtk_combo_realize		 (GtkWidget	   *widget);
static void         gtk_combo_unrealize		 (GtkWidget	   *widget);
static void         gtk_combo_destroy            (GtkObject        *combo);
static GtkListItem *gtk_combo_find               (GtkCombo         *combo);
static gchar *      gtk_combo_func               (GtkListItem      *li);
static gboolean     gtk_combo_focus_idle         (GtkCombo         *combo);
static gint         gtk_combo_entry_focus_out    (GtkEntry         *entry,
						  GdkEventFocus    *event,
						  GtkCombo         *combo);
static void         gtk_combo_get_pos            (GtkCombo         *combo,
						  gint             *x,
						  gint             *y,
						  gint             *height,
						  gint             *width);
static void         gtk_combo_popup_list         (GtkCombo         *combo);
static void         gtk_combo_popdown_list       (GtkCombo         *combo);

static void         gtk_combo_activate           (GtkWidget        *widget,
						  GtkCombo         *combo);
static gboolean     gtk_combo_popup_button_press (GtkWidget        *button,
						  GdkEventButton   *event,
						  GtkCombo         *combo);
static gboolean     gtk_combo_popup_button_leave (GtkWidget        *button,
						  GdkEventCrossing *event,
						  GtkCombo         *combo);
static void         gtk_combo_update_entry       (GtkCombo         *combo);
static void         gtk_combo_update_list        (GtkEntry         *entry,
						  GtkCombo         *combo);
static gint         gtk_combo_button_press       (GtkWidget        *widget,
						  GdkEvent         *event,
						  GtkCombo         *combo);
static void         gtk_combo_button_event_after (GtkWidget        *widget,
						  GdkEvent         *event,
						  GtkCombo         *combo);
static gint         gtk_combo_list_enter         (GtkWidget        *widget,
						  GdkEventCrossing *event,
						  GtkCombo         *combo);
static gint         gtk_combo_list_key_press     (GtkWidget        *widget,
						  GdkEventKey      *event,
						  GtkCombo         *combo);
static gint         gtk_combo_entry_key_press    (GtkEntry         *widget,
						  GdkEventKey      *event,
						  GtkCombo         *combo);
static gint         gtk_combo_window_key_press   (GtkWidget        *window,
						  GdkEventKey      *event,
						  GtkCombo         *combo);
static void         gtk_combo_size_allocate      (GtkWidget        *widget,
						  GtkAllocation   *allocation);
static void         gtk_combo_set_property       (GObject         *object,
						  guint            prop_id,
						  const GValue    *value,
						  GParamSpec      *pspec);
static void         gtk_combo_get_property       (GObject         *object,
						  guint            prop_id,
						  GValue          *value,
						  GParamSpec      *pspec);

G_DEFINE_TYPE (GtkCombo, gtk_combo, GTK_TYPE_HBOX)

static void
gtk_combo_class_init (GtkComboClass * klass)
{
  GObjectClass *gobject_class;
  GtkObjectClass *oclass;
  GtkWidgetClass *widget_class;

  gobject_class = (GObjectClass *) klass;
  oclass = (GtkObjectClass *) klass;
  widget_class = (GtkWidgetClass *) klass;

  gobject_class->set_property = gtk_combo_set_property; 
  gobject_class->get_property = gtk_combo_get_property; 

  g_object_class_install_property (gobject_class,
                                   PROP_ENABLE_ARROW_KEYS,
                                   g_param_spec_boolean ("enable-arrow-keys",
                                                         P_("Enable arrow keys"),
                                                         P_("Whether the arrow keys move through the list of items"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_ENABLE_ARROWS_ALWAYS,
                                   g_param_spec_boolean ("enable-arrows-always",
                                                         P_("Always enable arrows"),
                                                         P_("Obsolete property, ignored"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_CASE_SENSITIVE,
                                   g_param_spec_boolean ("case-sensitive",
                                                         P_("Case sensitive"),
                                                         P_("Whether list item matching is case sensitive"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_ALLOW_EMPTY,
                                   g_param_spec_boolean ("allow-empty",
                                                         P_("Allow empty"),
							 P_("Whether an empty value may be entered in this field"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_VALUE_IN_LIST,
                                   g_param_spec_boolean ("value-in-list",
                                                         P_("Value in list"),
                                                         P_("Whether entered values must already be present in the list"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));
  
   
  oclass->destroy = gtk_combo_destroy;
  
  widget_class->size_allocate = gtk_combo_size_allocate;
  widget_class->realize = gtk_combo_realize;
  widget_class->unrealize = gtk_combo_unrealize;
}

static void
gtk_combo_destroy (GtkObject *object)
{
  GtkCombo *combo = GTK_COMBO (object);

  if (combo->popwin)
    {
      gtk_widget_destroy (combo->popwin);
      g_object_unref (combo->popwin);
      combo->popwin = NULL;
    }

  GTK_OBJECT_CLASS (gtk_combo_parent_class)->destroy (object);
}

static int
gtk_combo_entry_key_press (GtkEntry * entry, GdkEventKey * event, GtkCombo * combo)
{
  GList *li;
  guint state = event->state & gtk_accelerator_get_default_mod_mask ();

  /* completion */
  if ((event->keyval == GDK_Tab ||  event->keyval == GDK_KP_Tab) &&
      state == GDK_MOD1_MASK)
    {
      GtkEditable *editable = GTK_EDITABLE (entry);
      GCompletion * cmpl;
      gchar* prefix;
      gchar* nprefix = NULL;
      gint pos;

      if ( !GTK_LIST (combo->list)->children )
	return FALSE;
    
      cmpl = g_completion_new ((GCompletionFunc)gtk_combo_func);
      g_completion_add_items (cmpl, GTK_LIST (combo->list)->children);

      pos = gtk_editable_get_position (editable);
      prefix = gtk_editable_get_chars (editable, 0, pos);

      g_completion_complete_utf8 (cmpl, prefix, &nprefix);

      if (nprefix && strlen (nprefix) > strlen (prefix)) 
	{
	  gtk_editable_insert_text (editable, g_utf8_offset_to_pointer (nprefix, pos), 
				    strlen (nprefix) - strlen (prefix), &pos);
	  gtk_editable_set_position (editable, pos);
	}

      g_free (nprefix);
      g_free (prefix);
      g_completion_free (cmpl);

      return TRUE;
    }

  if ((event->keyval == GDK_Down || event->keyval == GDK_KP_Down) &&
      state == GDK_MOD1_MASK)
    {
      gtk_combo_activate (NULL, combo);
      return TRUE;
    }

  if (!combo->use_arrows || !GTK_LIST (combo->list)->children)
    return FALSE;

  gtk_combo_update_list (GTK_ENTRY (combo->entry), combo);
  li = g_list_find (GTK_LIST (combo->list)->children, gtk_combo_find (combo));

  if (((event->keyval == GDK_Up || event->keyval == GDK_KP_Up) && state == 0) ||
      ((event->keyval == 'p' || event->keyval == 'P') && state == GDK_MOD1_MASK))
    {
      if (!li)
	li = g_list_last (GTK_LIST (combo->list)->children);
      else
	li = li->prev;

      if (li)
	{
	  gtk_list_select_child (GTK_LIST (combo->list), GTK_WIDGET (li->data));
	  gtk_combo_update_entry (combo);
	}
      
      return TRUE;
    }
  if (((event->keyval == GDK_Down || event->keyval == GDK_KP_Down) && state == 0) ||
      ((event->keyval == 'n' || event->keyval == 'N') && state == GDK_MOD1_MASK))
    {
      if (!li)
	li = GTK_LIST (combo->list)->children;
      else if (li)
	li = li->next;
      if (li)
	{
	  gtk_list_select_child (GTK_LIST (combo->list), GTK_WIDGET (li->data));
	  gtk_combo_update_entry (combo);
	}
      
      return TRUE;
    }
  return FALSE;
}

static int
gtk_combo_window_key_press (GtkWidget   *window,
			    GdkEventKey *event,
			    GtkCombo    *combo)
{
  guint state = event->state & gtk_accelerator_get_default_mod_mask ();

  if ((event->keyval == GDK_Return ||
       event->keyval == GDK_ISO_Enter ||
       event->keyval == GDK_KP_Enter) &&
      state == 0)
    {
      gtk_combo_popdown_list (combo);
      gtk_combo_update_entry (combo);

      return TRUE;
    }
  else if ((event->keyval == GDK_Up || event->keyval == GDK_KP_Up) &&
	   state == GDK_MOD1_MASK)
    {
      gtk_combo_popdown_list (combo);

      return TRUE;
    }
  else if ((event->keyval == GDK_space || event->keyval == GDK_KP_Space) &&
	   state == 0)
    {
      gtk_combo_update_entry (combo);
    }

  return FALSE;
}

static GtkListItem *
gtk_combo_find (GtkCombo * combo)
{
  const gchar *text;
  GtkListItem *found = NULL;
  gchar *ltext;
  gchar *compare_text;
  GList *clist;

  text = gtk_entry_get_text (GTK_ENTRY (combo->entry));
  if (combo->case_sensitive)
    compare_text = (gchar *)text;
  else
    compare_text = g_utf8_casefold (text, -1);
  
  for (clist = GTK_LIST (combo->list)->children;
       !found && clist;
       clist = clist->next)
    {
      ltext = gtk_combo_func (GTK_LIST_ITEM (clist->data));
      if (!ltext)
	continue;

      if (!combo->case_sensitive)
	ltext = g_utf8_casefold (ltext, -1);

      if (strcmp (ltext, compare_text) == 0)
	found = clist->data;

      if (!combo->case_sensitive)
	g_free (ltext);
    }

  if (!combo->case_sensitive)
    g_free (compare_text);

  return found;
}

static gchar *
gtk_combo_func (GtkListItem * li)
{
  GtkWidget *label;
  gchar *ltext = NULL;

  ltext = g_object_get_data (G_OBJECT (li), I_(gtk_combo_string_key));
  if (!ltext)
    {
      label = GTK_BIN (li)->child;
      if (!label || !GTK_IS_LABEL (label))
	return NULL;
      ltext = (gchar *) gtk_label_get_text (GTK_LABEL (label));
    }
  return ltext;
}

static gint
gtk_combo_focus_idle (GtkCombo * combo)
{
  if (combo)
    {
      GDK_THREADS_ENTER ();
      gtk_widget_grab_focus (combo->entry);
      GDK_THREADS_LEAVE ();
    }
  return FALSE;
}

static gint
gtk_combo_entry_focus_out (GtkEntry * entry, GdkEventFocus * event, GtkCombo * combo)
{

  if (combo->value_in_list && !gtk_combo_find (combo))
    {
      GSource *focus_idle;
      
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
         the signal_stop_emission doesn't seem to work either...
       */
      focus_idle = g_idle_source_new ();
      g_source_set_closure (focus_idle,
			    g_cclosure_new_object (G_CALLBACK (gtk_combo_focus_idle),
						   G_OBJECT (combo)));
      g_source_attach (focus_idle, NULL);
	g_source_unref (focus_idle);
      
      /*g_signal_stop_emission_by_name (entry, "focus_out_event"); */
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
  gint scrollbar_spacing;
  
  widget = GTK_WIDGET (combo);
  popup  = GTK_SCROLLED_WINDOW (combo->popup);
  popwin = GTK_BIN (combo->popwin);

  scrollbar_spacing = _gtk_scrolled_window_get_scrollbar_spacing (popup);

  gdk_window_get_origin (combo->entry->window, x, y);
  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) 
    *x -= widget->allocation.width - combo->entry->allocation.width;
  real_height = MIN (combo->entry->requisition.height, 
		     combo->entry->allocation.height);
  *y += real_height;
  avail_height = gdk_screen_get_height (gtk_widget_get_screen (widget)) - *y;
  
  gtk_widget_size_request (combo->list, &list_requisition);
  min_height = MIN (list_requisition.height, 
		    popup->vscrollbar->requisition.height);
  if (!GTK_LIST (combo->list)->children)
    list_requisition.height += EMPTY_LIST_HEIGHT;
  
  alloc_width = (widget->allocation.width -
		 2 * popwin->child->style->xthickness -
		 2 * GTK_CONTAINER (popwin->child)->border_width -
		 2 * GTK_CONTAINER (combo->popup)->border_width -
		 2 * GTK_CONTAINER (GTK_BIN (popup)->child)->border_width - 
		 2 * GTK_BIN (popup)->child->style->xthickness);
  
  work_height = (2 * popwin->child->style->ythickness +
		 2 * GTK_CONTAINER (popwin->child)->border_width +
		 2 * GTK_CONTAINER (combo->popup)->border_width +
		 2 * GTK_CONTAINER (GTK_BIN (popup)->child)->border_width +
		 2 * GTK_BIN (popup)->child->style->ythickness);
  
  do 
    {
      old_width = alloc_width;
      old_height = work_height;
      
      if (!show_hscroll &&
	  alloc_width < list_requisition.width)
	{
	  GtkRequisition requisition;
	  
	  gtk_widget_size_request (popup->hscrollbar, &requisition);
	  work_height += (requisition.height + scrollbar_spacing);
	  
	  show_hscroll = TRUE;
	}
      if (!show_vscroll && 
	  work_height + list_requisition.height > avail_height)
	{
	  GtkRequisition requisition;
	  
	  if (work_height + min_height > avail_height && 
	      *y - real_height > avail_height)
	    {
	      *y -= (work_height + list_requisition.height + real_height);
	      break;
	    }
	  gtk_widget_size_request (popup->hscrollbar, &requisition);
	  alloc_width -= (requisition.width + scrollbar_spacing);
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
gtk_combo_popup_list (GtkCombo *combo)
{
  GtkWidget *toplevel;
  GtkList *list;
  gint height, width, x, y;
  gint old_width, old_height;

  old_width = combo->popwin->allocation.width;
  old_height  = combo->popwin->allocation.height;

  gtk_combo_get_pos (combo, &x, &y, &height, &width);

  /* workaround for gtk_scrolled_window_size_allocate bug */
  if (old_width != width || old_height != height)
    {
      gtk_widget_hide (GTK_SCROLLED_WINDOW (combo->popup)->hscrollbar);
      gtk_widget_hide (GTK_SCROLLED_WINDOW (combo->popup)->vscrollbar);
    }

  gtk_combo_update_list (GTK_ENTRY (combo->entry), combo);

  /* We need to make sure some child of combo->popwin
   * is focused to disable GtkWindow's automatic
   * "focus-the-first-item" code. If there is no selected
   * child, we focus the list itself with some hackery.
   */
  list = GTK_LIST (combo->list);
  
  if (list->selection)
    {
      gtk_widget_grab_focus (list->selection->data);
    }
  else
    {
      gtk_widget_set_can_focus (GTK_WIDGET (list), TRUE);
      gtk_widget_grab_focus (combo->list);
      GTK_LIST (combo->list)->last_focus_child = NULL;
      gtk_widget_set_can_focus (GTK_WIDGET (list), FALSE);
    }
  
  gtk_window_move (GTK_WINDOW (combo->popwin), x, y);

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (combo));

  if (GTK_IS_WINDOW (toplevel))
    {
      gtk_window_group_add_window (gtk_window_get_group (GTK_WINDOW (toplevel)), 
                                   GTK_WINDOW (combo->popwin));
      gtk_window_set_transient_for (GTK_WINDOW (combo->popwin), GTK_WINDOW (toplevel));
    }

  gtk_widget_set_size_request (combo->popwin, width, height);
  gtk_widget_show (combo->popwin);

  gtk_widget_grab_focus (combo->popwin);
}

static void
gtk_combo_popdown_list (GtkCombo *combo)
{
  combo->current_button = 0;
      
  if (GTK_BUTTON (combo->button)->in_button)
    {
      GTK_BUTTON (combo->button)->in_button = FALSE;
      g_signal_emit_by_name (combo->button, "released");
    }

  if (GTK_WIDGET_HAS_GRAB (combo->popwin))
    {
      gtk_grab_remove (combo->popwin);
      gdk_display_pointer_ungrab (gtk_widget_get_display (GTK_WIDGET (combo)),
				  gtk_get_current_event_time ());
      gdk_display_keyboard_ungrab (gtk_widget_get_display (GTK_WIDGET (combo)),
				   gtk_get_current_event_time ());
    }
  
  gtk_widget_hide (combo->popwin);

  gtk_window_group_add_window (gtk_window_get_group (NULL), GTK_WINDOW (combo->popwin));
}

static gboolean
popup_grab_on_window (GdkWindow *window,
		      guint32    activate_time)
{
  if ((gdk_pointer_grab (window, TRUE,
			 GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
			 GDK_POINTER_MOTION_MASK,
			 NULL, NULL, activate_time) == 0))
    {
      if (gdk_keyboard_grab (window, TRUE,
			     activate_time) == 0)
	return TRUE;
      else
	{
	  gdk_display_pointer_ungrab (gdk_window_get_display (window),
				      activate_time);
	  return FALSE;
	}
    }

  return FALSE;
}

static void        
gtk_combo_activate (GtkWidget        *widget,
		    GtkCombo         *combo)
{
  if (!combo->button->window ||
      !popup_grab_on_window (combo->button->window,
			     gtk_get_current_event_time ()))
    return;

  gtk_combo_popup_list (combo);
  
  /* This must succeed since we already have the grab */
  popup_grab_on_window (combo->popwin->window,
			gtk_get_current_event_time ());

  if (!gtk_widget_has_focus (combo->entry))
    gtk_widget_grab_focus (combo->entry);

  gtk_grab_add (combo->popwin);
}

static gboolean
gtk_combo_popup_button_press (GtkWidget        *button,
			      GdkEventButton   *event,
			      GtkCombo         *combo)
{
  if (!gtk_widget_has_focus (combo->entry))
    gtk_widget_grab_focus (combo->entry);

  if (event->button != 1)
    return FALSE;

  if (!popup_grab_on_window (combo->button->window,
			     gtk_get_current_event_time ()))
    return FALSE;

  combo->current_button = event->button;

  gtk_combo_popup_list (combo);

  /* This must succeed since we already have the grab */
  popup_grab_on_window (combo->popwin->window,
			gtk_get_current_event_time ());

  g_signal_emit_by_name (button, "pressed");

  gtk_grab_add (combo->popwin);

  return TRUE;
}

static gboolean
gtk_combo_popup_button_leave (GtkWidget        *button,
			      GdkEventCrossing *event,
			      GtkCombo         *combo)
{
  /* The idea here is that we want to keep the button down if the
   * popup is popped up.
   */
  return combo->current_button != 0;
}

static void
gtk_combo_update_entry (GtkCombo * combo)
{
  GtkList *list = GTK_LIST (combo->list);
  char *text;

  g_signal_handler_block (list, combo->list_change_id);
  if (list->selection)
    {
      text = gtk_combo_func (GTK_LIST_ITEM (list->selection->data));
      if (!text)
	text = "";
      gtk_entry_set_text (GTK_ENTRY (combo->entry), text);
    }
  g_signal_handler_unblock (list, combo->list_change_id);
}

static void
gtk_combo_selection_changed (GtkList  *list,
			     GtkCombo *combo)
{
  if (!gtk_widget_get_visible (combo->popwin))
    gtk_combo_update_entry (combo);
}

static void
gtk_combo_update_list (GtkEntry * entry, GtkCombo * combo)
{
  GtkList *list = GTK_LIST (combo->list);
  GList *slist = list->selection;
  GtkListItem *li;

  gtk_grab_remove (GTK_WIDGET (combo));

  g_signal_handler_block (entry, combo->entry_change_id);
  if (slist && slist->data)
    gtk_list_unselect_child (list, GTK_WIDGET (slist->data));
  li = gtk_combo_find (combo);
  if (li)
    gtk_list_select_child (list, GTK_WIDGET (li));
  g_signal_handler_unblock (entry, combo->entry_change_id);
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

  gtk_combo_popdown_list (combo);

  return TRUE;
}

static gboolean
is_within (GtkWidget *widget,
	   GtkWidget *ancestor)
{
  return widget == ancestor || gtk_widget_is_ancestor (widget, ancestor);
}

static void
gtk_combo_button_event_after (GtkWidget *widget,
			      GdkEvent  *event,
			      GtkCombo  *combo)
{
  GtkWidget *child;

  if (event->type != GDK_BUTTON_RELEASE)
    return;
  
  child = gtk_get_event_widget ((GdkEvent*) event);

  if ((combo->current_button != 0) && (event->button.button == 1))
    {
      /* This was the initial button press */

      combo->current_button = 0;

      /* Check to see if we released inside the button */
      if (child && is_within (child, combo->button))
	{
	  gtk_grab_add (combo->popwin);
	  gdk_pointer_grab (combo->popwin->window, TRUE,
			    GDK_BUTTON_PRESS_MASK | 
			    GDK_BUTTON_RELEASE_MASK |
			    GDK_POINTER_MOTION_MASK, 
			    NULL, NULL, GDK_CURRENT_TIME);
	  return;
	}
    }

  if (is_within (child, combo->list))
    gtk_combo_update_entry (combo);
    
  gtk_combo_popdown_list (combo);

}

static void
find_child_foreach (GtkWidget *widget,
		    gpointer   data)
{
  GdkEventButton *event = data;

  if (!event->window)
    {
      if (event->x >= widget->allocation.x &&
	  event->x < widget->allocation.x + widget->allocation.width &&
	  event->y >= widget->allocation.y &&
	  event->y < widget->allocation.y + widget->allocation.height)
	event->window = g_object_ref (widget->window);
    }
}

static void
find_child_window (GtkContainer   *container,
		   GdkEventButton *event)
{
  gtk_container_foreach (container, find_child_foreach, event);
}

static gint         
gtk_combo_list_enter (GtkWidget        *widget,
		      GdkEventCrossing *event,
		      GtkCombo         *combo)
{
  GtkWidget *event_widget;

  event_widget = gtk_get_event_widget ((GdkEvent*) event);

  if ((event_widget == combo->list) &&
      (combo->current_button != 0) && 
      (!GTK_WIDGET_HAS_GRAB (combo->list)))
    {
      GdkEvent *tmp_event = gdk_event_new (GDK_BUTTON_PRESS);
      gint x, y;
      GdkModifierType mask;

      gtk_grab_remove (combo->popwin);

      /* Transfer the grab over to the list by synthesizing
       * a button press event
       */
      gdk_window_get_pointer (combo->list->window, &x, &y, &mask);

      tmp_event->button.send_event = TRUE;
      tmp_event->button.time = GDK_CURRENT_TIME; /* bad */
      tmp_event->button.x = x;
      tmp_event->button.y = y;
      /* We leave all the XInput fields unfilled here, in the expectation
       * that GtkList doesn't care.
       */
      tmp_event->button.button = combo->current_button;
      tmp_event->button.state = mask;

      find_child_window (GTK_CONTAINER (combo->list), &tmp_event->button);
      if (!tmp_event->button.window)
	{
	  GtkWidget *child;
	  
	  if (GTK_LIST (combo->list)->children)
	    child = GTK_LIST (combo->list)->children->data;
	  else
	    child = combo->list;

	  tmp_event->button.window = g_object_ref (child->window);
	}

      gtk_widget_event (combo->list, tmp_event);
      gdk_event_free (tmp_event);
    }

  return FALSE;
}

static int
gtk_combo_list_key_press (GtkWidget * widget, GdkEventKey * event, GtkCombo * combo)
{
  guint state = event->state & gtk_accelerator_get_default_mod_mask ();

  if (event->keyval == GDK_Escape && state == 0)
    {
      if (GTK_WIDGET_HAS_GRAB (combo->list))
	gtk_list_end_drag_selection (GTK_LIST (combo->list));

      gtk_combo_popdown_list (combo);
      
      return TRUE;
    }
  return FALSE;
}

static void
combo_event_box_realize (GtkWidget *widget)
{
  GdkCursor *cursor = gdk_cursor_new_for_display (gtk_widget_get_display (widget),
						  GDK_TOP_LEFT_ARROW);
  gdk_window_set_cursor (widget->window, cursor);
  gdk_cursor_unref (cursor);
}

static void
gtk_combo_init (GtkCombo * combo)
{
  GtkWidget *arrow;
  GtkWidget *frame;
  GtkWidget *event_box;

  combo->case_sensitive = FALSE;
  combo->value_in_list = FALSE;
  combo->ok_if_empty = TRUE;
  combo->use_arrows = TRUE;
  combo->use_arrows_always = TRUE;
  combo->entry = gtk_entry_new ();
  combo->button = gtk_button_new ();
  combo->current_button = 0;
  arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_OUT);
  gtk_widget_show (arrow);
  gtk_container_add (GTK_CONTAINER (combo->button), arrow);
  gtk_box_pack_start (GTK_BOX (combo), combo->entry, TRUE, TRUE, 0);
  gtk_box_pack_end (GTK_BOX (combo), combo->button, FALSE, FALSE, 0);
  gtk_widget_set_can_focus (combo->button, FALSE);
  gtk_widget_show (combo->entry);
  gtk_widget_show (combo->button);
  combo->entry_change_id = g_signal_connect (combo->entry, "changed",
					     G_CALLBACK (gtk_combo_update_list),
					     combo);
  g_signal_connect_after (combo->entry, "key-press-event",
			  G_CALLBACK (gtk_combo_entry_key_press), combo);
  g_signal_connect_after (combo->entry, "focus-out-event",
			  G_CALLBACK (gtk_combo_entry_focus_out), combo);
  combo->activate_id = g_signal_connect (combo->entry, "activate",
					 G_CALLBACK (gtk_combo_activate),
					 combo);
  g_signal_connect (combo->button, "button-press-event",
		    G_CALLBACK (gtk_combo_popup_button_press), combo);
  g_signal_connect (combo->button, "leave-notify-event",
		    G_CALLBACK (gtk_combo_popup_button_leave), combo);

  combo->popwin = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_widget_set_name (combo->popwin, "gtk-combo-popup-window");
  gtk_window_set_type_hint (GTK_WINDOW (combo->popwin), GDK_WINDOW_TYPE_HINT_COMBO);
  g_object_ref (combo->popwin);
  gtk_window_set_resizable (GTK_WINDOW (combo->popwin), FALSE);

  g_signal_connect (combo->popwin, "key-press-event",
		    G_CALLBACK (gtk_combo_window_key_press), combo);
  
  gtk_widget_set_events (combo->popwin, GDK_KEY_PRESS_MASK);

  event_box = gtk_event_box_new ();
  gtk_container_add (GTK_CONTAINER (combo->popwin), event_box);
  g_signal_connect (event_box, "realize",
		    G_CALLBACK (combo_event_box_realize), NULL);
  gtk_widget_show (event_box);


  frame = gtk_frame_new (NULL);
  gtk_container_add (GTK_CONTAINER (event_box), frame);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
  gtk_widget_show (frame);

  combo->popup = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (combo->popup),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_widget_set_can_focus (GTK_SCROLLED_WINDOW (combo->popup)->hscrollbar, FALSE);
  gtk_widget_set_can_focus (GTK_SCROLLED_WINDOW (combo->popup)->vscrollbar, FALSE);
  gtk_container_add (GTK_CONTAINER (frame), combo->popup);
  gtk_widget_show (combo->popup);

  combo->list = gtk_list_new ();
  /* We'll use enter notify events to figure out when to transfer
   * the grab to the list
   */
  gtk_widget_set_events (combo->list, GDK_ENTER_NOTIFY_MASK);

  gtk_list_set_selection_mode (GTK_LIST(combo->list), GTK_SELECTION_BROWSE);
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (combo->popup), combo->list);
  gtk_container_set_focus_vadjustment (GTK_CONTAINER (combo->list),
				       gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (combo->popup)));
  gtk_container_set_focus_hadjustment (GTK_CONTAINER (combo->list),
				       gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (combo->popup)));
  gtk_widget_show (combo->list);

  combo->list_change_id = g_signal_connect (combo->list, "selection-changed",
					    G_CALLBACK (gtk_combo_selection_changed), combo);
  
  g_signal_connect (combo->popwin, "key-press-event",
		    G_CALLBACK (gtk_combo_list_key_press), combo);
  g_signal_connect (combo->popwin, "button-press-event",
		    G_CALLBACK (gtk_combo_button_press), combo);

  g_signal_connect (combo->popwin, "event-after",
		    G_CALLBACK (gtk_combo_button_event_after), combo);
  g_signal_connect (combo->list, "event-after",
		    G_CALLBACK (gtk_combo_button_event_after), combo);

  g_signal_connect (combo->list, "enter-notify-event",
		    G_CALLBACK (gtk_combo_list_enter), combo);
}

static void
gtk_combo_realize (GtkWidget *widget)
{
  GtkCombo *combo = GTK_COMBO (widget);

  gtk_window_set_screen (GTK_WINDOW (combo->popwin), 
			 gtk_widget_get_screen (widget));
  
  GTK_WIDGET_CLASS (gtk_combo_parent_class)->realize (widget);  
}

static void        
gtk_combo_unrealize (GtkWidget *widget)
{
  GtkCombo *combo = GTK_COMBO (widget);

  gtk_combo_popdown_list (combo);
  gtk_widget_unrealize (combo->popwin);
  
  GTK_WIDGET_CLASS (gtk_combo_parent_class)->unrealize (widget);
}

GtkWidget*
gtk_combo_new (void)
{
  return g_object_new (GTK_TYPE_COMBO, NULL);
}

void
gtk_combo_set_value_in_list (GtkCombo * combo, gboolean val, gboolean ok_if_empty)
{
  g_return_if_fail (GTK_IS_COMBO (combo));
  val = val != FALSE;
  ok_if_empty = ok_if_empty != FALSE;

  g_object_freeze_notify (G_OBJECT (combo));
  if (combo->value_in_list != val)
    {
       combo->value_in_list = val;
  g_object_notify (G_OBJECT (combo), "value-in-list");
    }
  if (combo->ok_if_empty != ok_if_empty)
    {
       combo->ok_if_empty = ok_if_empty;
  g_object_notify (G_OBJECT (combo), "allow-empty");
    }
  g_object_thaw_notify (G_OBJECT (combo));
}

void
gtk_combo_set_case_sensitive (GtkCombo * combo, gboolean val)
{
  g_return_if_fail (GTK_IS_COMBO (combo));
  val = val != FALSE;

  if (combo->case_sensitive != val) 
    {
  combo->case_sensitive = val;
  g_object_notify (G_OBJECT (combo), "case-sensitive");
    }
}

void
gtk_combo_set_use_arrows (GtkCombo * combo, gboolean val)
{
  g_return_if_fail (GTK_IS_COMBO (combo));
  val = val != FALSE;

  if (combo->use_arrows != val) 
    {
  combo->use_arrows = val;
  g_object_notify (G_OBJECT (combo), "enable-arrow-keys");
    }
}

void
gtk_combo_set_use_arrows_always (GtkCombo * combo, gboolean val)
{
  g_return_if_fail (GTK_IS_COMBO (combo));
  val = val != FALSE;

  if (combo->use_arrows_always != val) 
    {
       g_object_freeze_notify (G_OBJECT (combo));
  combo->use_arrows_always = val;
       g_object_notify (G_OBJECT (combo), "enable-arrows-always");

       if (combo->use_arrows != TRUE) 
         {
  combo->use_arrows = TRUE;
  g_object_notify (G_OBJECT (combo), "enable-arrow-keys");
         }
  g_object_thaw_notify (G_OBJECT (combo));
    }
}

void
gtk_combo_set_popdown_strings (GtkCombo *combo, 
			       GList    *strings)
{
  GList *list;
  GtkWidget *li;

  g_return_if_fail (GTK_IS_COMBO (combo));

  gtk_combo_popdown_list (combo);

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

void
gtk_combo_set_item_string (GtkCombo    *combo,
                           GtkItem     *item,
                           const gchar *item_value)
{
  g_return_if_fail (GTK_IS_COMBO (combo));
  g_return_if_fail (item != NULL);

  g_object_set_data_full (G_OBJECT (item), I_(gtk_combo_string_key),
			  g_strdup (item_value), g_free);
}

static void
gtk_combo_size_allocate (GtkWidget     *widget,
			 GtkAllocation *allocation)
{
  GtkCombo *combo = GTK_COMBO (widget);

  GTK_WIDGET_CLASS (gtk_combo_parent_class)->size_allocate (widget, allocation);

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
gtk_combo_disable_activate (GtkCombo *combo)
{
  g_return_if_fail (GTK_IS_COMBO (combo));

  if ( combo->activate_id ) {
    g_signal_handler_disconnect (combo->entry, combo->activate_id);
    combo->activate_id = 0;
  }
}

static void
gtk_combo_set_property (GObject      *object,
			guint         prop_id,
			const GValue *value,
			GParamSpec   *pspec)
{
  GtkCombo *combo = GTK_COMBO (object);
  
  switch (prop_id)
    {
    case PROP_ENABLE_ARROW_KEYS:
      gtk_combo_set_use_arrows (combo, g_value_get_boolean (value));
      break;
    case PROP_ENABLE_ARROWS_ALWAYS:
      gtk_combo_set_use_arrows_always (combo, g_value_get_boolean (value));
      break;
    case PROP_CASE_SENSITIVE:
      gtk_combo_set_case_sensitive (combo, g_value_get_boolean (value));
      break;
    case PROP_ALLOW_EMPTY:
      combo->ok_if_empty = g_value_get_boolean (value);
      break;
    case PROP_VALUE_IN_LIST:
      combo->value_in_list = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
  
}

static void
gtk_combo_get_property (GObject    *object,
			guint       prop_id,
			GValue     *value,
			GParamSpec *pspec)
{
  GtkCombo *combo = GTK_COMBO (object);
  
  switch (prop_id)
    {
    case PROP_ENABLE_ARROW_KEYS:
      g_value_set_boolean (value, combo->use_arrows);
      break;
    case PROP_ENABLE_ARROWS_ALWAYS:
      g_value_set_boolean (value, combo->use_arrows_always);
      break;
    case PROP_CASE_SENSITIVE:
      g_value_set_boolean (value, combo->case_sensitive);
      break;
    case PROP_ALLOW_EMPTY:
      g_value_set_boolean (value, combo->ok_if_empty);
      break;
    case PROP_VALUE_IN_LIST:
      g_value_set_boolean (value, combo->value_in_list);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
   
}

#define __GTK_SMART_COMBO_C__
#include "gtkaliasdef.c"
