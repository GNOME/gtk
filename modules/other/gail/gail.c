/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001 Sun Microsystems Inc.
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#undef GTK_DISABLE_DEPRECATED

#include <gtk/gtk.h>
#include "gail.h"
#include "gailfactory.h"

#define GNOME_ACCESSIBILITY_ENV "GNOME_ACCESSIBILITY"
#define NO_GAIL_ENV "NO_GAIL"

static gboolean gail_focus_watcher      (GSignalInvocationHint *ihint,
                                         guint                  n_param_values,
                                         const GValue          *param_values,
                                         gpointer               data);
static gboolean gail_select_watcher     (GSignalInvocationHint *ihint,
                                         guint                  n_param_values,
                                         const GValue          *param_values,
                                         gpointer               data);
static gboolean gail_deselect_watcher   (GSignalInvocationHint *ihint,
                                         guint                  n_param_values,
                                         const GValue          *param_values,
                                         gpointer               data);
static gboolean gail_switch_page_watcher(GSignalInvocationHint *ihint,
                                         guint                  n_param_values,
                                         const GValue          *param_values,
                                         gpointer               data);
static AtkObject* gail_get_accessible_for_widget (GtkWidget    *widget,
                                                  gboolean     *transient);
static void     gail_finish_select       (GtkWidget            *widget);
static void     gail_map_cb              (GtkWidget            *widget);
static void     gail_map_submenu_cb      (GtkWidget            *widget);
static gint     gail_focus_idle_handler  (gpointer             data);
static void     gail_focus_notify        (GtkWidget            *widget);
static void     gail_focus_notify_when_idle (GtkWidget            *widget);

static void     gail_focus_tracker_init (void);
static void     gail_focus_object_destroyed (gpointer data);
static void     gail_focus_tracker (AtkObject *object);
static void     gail_set_focus_widget (GtkWidget *focus_widget,
                                       GtkWidget *widget); 
static void     gail_set_focus_object (AtkObject *focus_obj,
                                       AtkObject *obj);

GtkWidget* focus_widget = NULL;
static GtkWidget* next_focus_widget = NULL;
static gboolean was_deselect = FALSE;
static GtkWidget* subsequent_focus_widget = NULL;
static GtkWidget* focus_before_menu = NULL;
static guint focus_notify_handler = 0;    
static guint focus_tracker_id = 0;
static GQuark quark_focus_object = 0;

GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_OBJECT, GailObject, gail_object, GTK_TYPE_OBJECT)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_WIDGET, GailWidget, gail_widget, GTK_TYPE_WIDGET)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_CONTAINER, GailContainer, gail_container, GTK_TYPE_CONTAINER)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_BUTTON, GailButton, gail_button, GTK_TYPE_BUTTON)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_ITEM, GailItem, gail_item, GTK_TYPE_ITEM)
GAIL_IMPLEMENT_FACTORY_WITH_FUNC (GAIL_TYPE_MENU_ITEM, GailMenuItem, gail_menu_item, gail_menu_item_new)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_TOGGLE_BUTTON, GailToggleButton, gail_toggle_button, GTK_TYPE_TOGGLE_BUTTON)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_IMAGE, GailImage, gail_image, GTK_TYPE_IMAGE)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_TEXT_VIEW, GailTextView, gail_text_view, GTK_TYPE_TEXT_VIEW)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_COMBO, GailCombo, gail_combo, GTK_TYPE_COMBO)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_COMBO_BOX, GailComboBox, gail_combo_box, GTK_TYPE_COMBO_BOX)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_ENTRY, GailEntry, gail_entry, GTK_TYPE_ENTRY)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_MENU_SHELL, GailMenuShell, gail_menu_shell, GTK_TYPE_MENU_SHELL)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_MENU, GailMenu, gail_menu, GTK_TYPE_MENU)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_WINDOW, GailWindow, gail_window, GTK_TYPE_BIN)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_RANGE, GailRange, gail_range, GTK_TYPE_RANGE)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_SCALE, GailScale, gail_scale, GTK_TYPE_SCALE)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_SCALE_BUTTON, GailScaleButton, gail_scale_button, GTK_TYPE_SCALE_BUTTON)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_CLIST, GailCList, gail_clist, GTK_TYPE_CLIST)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_LABEL, GailLabel, gail_label, GTK_TYPE_LABEL)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_STATUSBAR, GailStatusbar, gail_statusbar, GTK_TYPE_STATUSBAR)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_NOTEBOOK, GailNotebook, gail_notebook, GTK_TYPE_NOTEBOOK)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_CALENDAR, GailCalendar, gail_calendar, GTK_TYPE_CALENDAR)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_PROGRESS_BAR, GailProgressBar, gail_progress_bar, GTK_TYPE_PROGRESS_BAR)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_SPIN_BUTTON, GailSpinButton, gail_spin_button, GTK_TYPE_SPIN_BUTTON)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_TREE_VIEW, GailTreeView, gail_tree_view, GTK_TYPE_TREE_VIEW)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_FRAME, GailFrame, gail_frame, GTK_TYPE_FRAME)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_RADIO_BUTTON, GailRadioButton, gail_radio_button, GTK_TYPE_RADIO_BUTTON)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_ARROW, GailArrow, gail_arrow, GTK_TYPE_ARROW)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_PIXMAP, GailPixmap, gail_pixmap, GTK_TYPE_PIXMAP)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_SEPARATOR, GailSeparator, gail_separator, GTK_TYPE_SEPARATOR)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_BOX, GailBox, gail_box, GTK_TYPE_BOX)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_SCROLLED_WINDOW, GailScrolledWindow, gail_scrolled_window, GTK_TYPE_SCROLLED_WINDOW)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_LIST, GailList, gail_list, GTK_TYPE_LIST)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_PANED, GailPaned, gail_paned, GTK_TYPE_PANED)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_SCROLLBAR, GailScrollbar, gail_scrollbar, GTK_TYPE_SCROLLBAR)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_OPTION_MENU, GailOptionMenu, gail_option_menu, GTK_TYPE_OPTION_MENU)
GAIL_IMPLEMENT_FACTORY_WITH_FUNC (GAIL_TYPE_CHECK_MENU_ITEM, GailCheckMenuItem, gail_check_menu_item, gail_check_menu_item_new)
GAIL_IMPLEMENT_FACTORY_WITH_FUNC (GAIL_TYPE_RADIO_MENU_ITEM, GailRadioMenuItem, gail_radio_menu_item, gail_radio_menu_item_new)
GAIL_IMPLEMENT_FACTORY (GAIL_TYPE_EXPANDER, GailExpander, gail_expander, GTK_TYPE_EXPANDER)
GAIL_IMPLEMENT_FACTORY_WITH_FUNC_DUMMY (GAIL_TYPE_RENDERER_CELL, GailRendererCell, gail_renderer_cell, GTK_TYPE_CELL_RENDERER, gail_renderer_cell_new)
GAIL_IMPLEMENT_FACTORY_WITH_FUNC_DUMMY (GAIL_TYPE_BOOLEAN_CELL, GailBooleanCell, gail_boolean_cell, GTK_TYPE_CELL_RENDERER_TOGGLE, gail_boolean_cell_new)
GAIL_IMPLEMENT_FACTORY_WITH_FUNC_DUMMY (GAIL_TYPE_IMAGE_CELL, GailImageCell, gail_image_cell, GTK_TYPE_CELL_RENDERER_PIXBUF, gail_image_cell_new)
GAIL_IMPLEMENT_FACTORY_WITH_FUNC_DUMMY (GAIL_TYPE_TEXT_CELL, GailTextCell, gail_text_cell, GTK_TYPE_CELL_RENDERER_TEXT, gail_text_cell_new)

static AtkObject*
gail_get_accessible_for_widget (GtkWidget *widget,
                                gboolean  *transient)
{
  AtkObject *obj = NULL;
  GType gnome_canvas;

  gnome_canvas = g_type_from_name ("GnomeCanvas");

  *transient = FALSE;
  if (!widget)
    return NULL;

  if (GTK_IS_ENTRY (widget))
    {
      GtkWidget *other_widget = widget->parent;
      if (GTK_IS_COMBO (other_widget))
        {
          gail_set_focus_widget (other_widget, widget);
          widget = other_widget;
        }
    } 
  else if (GTK_IS_NOTEBOOK (widget)) 
    {
      GtkNotebook *notebook;
      gint page_num = -1;

      notebook = GTK_NOTEBOOK (widget);
      /*
       * Report the currently focused tab rather than the currently selected tab
       */
      if (notebook->focus_tab)
        {
          page_num = g_list_index (notebook->children, notebook->focus_tab->data);
        }
      if (page_num != -1)
        {
          obj = gtk_widget_get_accessible (widget);
          obj = atk_object_ref_accessible_child (obj, page_num);
          g_object_unref (obj);
        }
    }
  else if (GTK_CHECK_TYPE ((widget), gnome_canvas))
    {
      GObject *focused_item;
      GValue value = {0, };

      g_value_init (&value, G_TYPE_OBJECT);
      g_object_get_property (G_OBJECT (widget), "focused_item", &value);
      focused_item = g_value_get_object (&value);

      if (focused_item)
        {
          AtkObject *tmp;

          obj = atk_gobject_accessible_for_object (G_OBJECT (focused_item));
          tmp = g_object_get_qdata (G_OBJECT (obj), quark_focus_object);
          if (tmp != NULL)
            obj = tmp;
        }
    }
  else if (GTK_IS_TOGGLE_BUTTON (widget))
    {
      GtkWidget *other_widget = widget->parent;
      if (GTK_IS_COMBO_BOX (other_widget))
        {
          gail_set_focus_widget (other_widget, widget);
          widget = other_widget;
        }
    }
  if (obj == NULL)
    {
      AtkObject *focus_object;

      obj = gtk_widget_get_accessible (widget);
      focus_object = g_object_get_qdata (G_OBJECT (obj), quark_focus_object);
      /*
       * We check whether the object for this focus_object has been deleted.
       * This can happen when navigating to an empty directory in nautilus. 
       * See bug #141907.
       */
      if (ATK_IS_GOBJECT_ACCESSIBLE (focus_object))
        {
          if (!atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (focus_object)))
            focus_object = NULL;
        }
      if (focus_object)
        obj = focus_object;
    }

  return obj;
}

static gboolean
gail_focus_watcher (GSignalInvocationHint *ihint,
                    guint                  n_param_values,
                    const GValue          *param_values,
                    gpointer               data)
{
  GObject *object;
  GtkWidget *widget;
  GdkEvent *event;

  object = g_value_get_object (param_values + 0);
  g_return_val_if_fail (GTK_IS_WIDGET(object), FALSE);

  event = g_value_get_boxed (param_values + 1);
  widget = GTK_WIDGET (object);

  if (event->type == GDK_FOCUS_CHANGE) 
    {
      if (event->focus_change.in)
        {
          if (GTK_IS_WINDOW (widget))
            {
              GtkWindow *window;

              window = GTK_WINDOW (widget);
              if (window->focus_widget)
                {
                  /*
                   * If we already have a potential focus widget set this
                   * windows's focus widget to focus_before_menu so that 
                   * it will be reported when menu item is unset.
                   */
                  if (next_focus_widget)
                    {
                      if (GTK_IS_MENU_ITEM (next_focus_widget) &&
                          !focus_before_menu)
                        {
                          void *vp_focus_before_menu = &focus_before_menu;
                          focus_before_menu = window->focus_widget;
                          g_object_add_weak_pointer (G_OBJECT (focus_before_menu), vp_focus_before_menu);
                        }

                      return TRUE;
                    }
                  widget = window->focus_widget;
                }
              else if (window->type == GTK_WINDOW_POPUP) 
                {
	          if (GTK_IS_BIN (widget))
		    {
		      GtkWidget *child = gtk_bin_get_child (GTK_BIN (widget));

		      if (GTK_IS_WIDGET (child) && gtk_widget_has_grab (child))
			{
			  if (GTK_IS_MENU_SHELL (child))
			    {
			      if (GTK_MENU_SHELL (child)->active_menu_item)
				{
				  /*
				   * We have a menu which has a menu item selected
				   * so we do not report focus on the menu.
				   */ 
				  return TRUE; 
				}
			    }
			  widget = child;
			} 
		    }
		  else /* popup window has no children; this edge case occurs in some custom code (OOo for instance) */
		    {
		      return TRUE;
		    }
                }
	      else /* Widget is a non-popup toplevel with no focus children; 
		      don't emit for this case either, as it's useless */
		{
		  return TRUE;
		}
            }
        }
      else
        {
          if (next_focus_widget)
            {
               GtkWidget *toplevel;

               toplevel = gtk_widget_get_toplevel (next_focus_widget);
               if (toplevel == widget)
                 next_focus_widget = NULL; 
            }
          /* focus out */
          widget = NULL;
        }
    }
  else
    {
      if (event->type == GDK_MOTION_NOTIFY && gtk_widget_has_focus (widget))
        {
          if (widget == focus_widget)
            {
              return TRUE;
            }
        }
      else
        {
          return TRUE;
        }
    }
  /*
   * If the focus widget is a GtkSocket without a plug
   * then ignore the focus notification as the embedded
   * plug will report a focus notification.
   */
  if (GTK_IS_SOCKET (widget) &&
      GTK_SOCKET (widget)->plug_widget == NULL)
    return TRUE;
  /*
   * The widget may not yet be visible on the screen so we wait until it is.
   */
  gail_focus_notify_when_idle (widget);
  return TRUE; 
}

static gboolean
gail_select_watcher (GSignalInvocationHint *ihint,
                     guint                  n_param_values,
                     const GValue          *param_values,
                     gpointer               data)
{
  GObject *object;
  GtkWidget *widget;

  object = g_value_get_object (param_values + 0);
  g_return_val_if_fail (GTK_IS_WIDGET(object), FALSE);

  widget = GTK_WIDGET (object);

  if (!gtk_widget_get_mapped (widget))
    {
      g_signal_connect (widget, "map",
                        G_CALLBACK (gail_map_cb),
                        NULL);
    }
  else
    gail_finish_select (widget);

  return TRUE;
}

static void
gail_finish_select (GtkWidget *widget)
{
  if (GTK_IS_MENU_ITEM (widget))
    {
      GtkMenuItem* menu_item;

      menu_item = GTK_MENU_ITEM (widget);
      if (menu_item->submenu &&
          !gtk_widget_get_mapped (menu_item->submenu))
        {
          /*
           * If the submenu is not visble, wait until it is before
           * reporting focus on the menu item.
           */
          gulong handler_id;

          handler_id = g_signal_handler_find (menu_item->submenu,
                                              G_SIGNAL_MATCH_FUNC,
                                              g_signal_lookup ("map",
                                                               GTK_TYPE_WINDOW),
                                              0,
                                              NULL,
                                              (gpointer) gail_map_submenu_cb,
                                              NULL); 
          if (!handler_id)
            g_signal_connect (menu_item->submenu, "map",
                              G_CALLBACK (gail_map_submenu_cb),
                              NULL);
            return;

        }
      /*
       * If we are waiting to report focus on a menubar or a menu item
       * because of a previous deselect, cancel it.
       */
      if (was_deselect &&
          focus_notify_handler &&
          next_focus_widget &&
          (GTK_IS_MENU_BAR (next_focus_widget) ||
           GTK_IS_MENU_ITEM (next_focus_widget)))
        {
          void *vp_next_focus_widget = &next_focus_widget;
          g_source_remove (focus_notify_handler);
          g_object_remove_weak_pointer (G_OBJECT (next_focus_widget), vp_next_focus_widget);
	  next_focus_widget = NULL;
          focus_notify_handler = 0;
          was_deselect = FALSE;
        }
    } 
  /*
   * If previously focused widget is not a GtkMenuItem or a GtkMenu,
   * keep track of it so we can return to it after menubar is deactivated
   */
  if (focus_widget && 
      !GTK_IS_MENU_ITEM (focus_widget) && 
      !GTK_IS_MENU (focus_widget))
    {
      void *vp_focus_before_menu = &focus_before_menu;
      focus_before_menu = focus_widget;
      g_object_add_weak_pointer (G_OBJECT (focus_before_menu), vp_focus_before_menu);

    } 
  gail_focus_notify_when_idle (widget);

  return; 
}

static void
gail_map_cb (GtkWidget *widget)
{
  gail_finish_select (widget);
}

static void
gail_map_submenu_cb (GtkWidget *widget)
{
  if (GTK_IS_MENU (widget))
    {
      if (GTK_MENU (widget)->parent_menu_item)
        gail_finish_select (GTK_MENU (widget)->parent_menu_item);
    }
}


static gboolean
gail_deselect_watcher (GSignalInvocationHint *ihint,
                       guint                  n_param_values,
                       const GValue          *param_values,
                       gpointer               data)
{
  GObject *object;
  GtkWidget *widget;
  GtkWidget *menu_shell;

  object = g_value_get_object (param_values + 0);
  g_return_val_if_fail (GTK_IS_WIDGET(object), FALSE);

  widget = GTK_WIDGET (object);

  if (!GTK_IS_MENU_ITEM (widget))
    return TRUE;

  if (subsequent_focus_widget == widget)
    subsequent_focus_widget = NULL;

  menu_shell = gtk_widget_get_parent (widget);
  if (GTK_IS_MENU_SHELL (menu_shell))
    {
      GtkWidget *parent_menu_shell;

      parent_menu_shell = GTK_MENU_SHELL (menu_shell)->parent_menu_shell;
      if (parent_menu_shell)
        {
          GtkWidget *active_menu_item;

          active_menu_item = GTK_MENU_SHELL (parent_menu_shell)->active_menu_item;
          if (active_menu_item)
            {
              gail_focus_notify_when_idle (active_menu_item);
            }
        }
      else
        {
          if (!GTK_IS_MENU_BAR (menu_shell))
            {
              gail_focus_notify_when_idle (menu_shell);
            }
        }
    }
  was_deselect = TRUE;
  return TRUE; 
}

static gboolean 
gail_switch_page_watcher (GSignalInvocationHint *ihint,
                          guint                  n_param_values,
                          const GValue          *param_values,
                          gpointer               data)
{
  GObject *object;
  GtkWidget *widget;
  GtkNotebook *notebook;

  object = g_value_get_object (param_values + 0);
  g_return_val_if_fail (GTK_IS_WIDGET(object), FALSE);

  widget = GTK_WIDGET (object);

  if (!GTK_IS_NOTEBOOK (widget))
    return TRUE;

  notebook = GTK_NOTEBOOK (widget);
  if (!notebook->focus_tab)
    return TRUE;

  gail_focus_notify_when_idle (widget);
  return TRUE;
}

static gboolean
gail_focus_idle_handler (gpointer data)
{
  focus_notify_handler = 0;
  /*
   * The widget which was to receive focus may have been removed
   */
  if (!next_focus_widget)
    {
      if (next_focus_widget != data)
        return FALSE;
    }
  else
    {
      void *vp_next_focus_widget = &next_focus_widget;
      g_object_remove_weak_pointer (G_OBJECT (next_focus_widget), vp_next_focus_widget);
      next_focus_widget = NULL;
    }
    
  gail_focus_notify (data);

  return FALSE;
}

static void
gail_focus_notify (GtkWidget *widget)
{
  AtkObject *atk_obj;
  gboolean transient;

  if (widget != focus_widget)
    {
      if (focus_widget)
        {
          void *vp_focus_widget = &focus_widget;
          g_object_remove_weak_pointer (G_OBJECT (focus_widget), vp_focus_widget);
        }
      focus_widget = widget;
      if (focus_widget)
        {
          void *vp_focus_widget = &focus_widget;
          g_object_add_weak_pointer (G_OBJECT (focus_widget), vp_focus_widget);
          /*
           * The UI may not have been updated yet; e.g. in gtkhtml2
           * html_view_layout() is called in a idle handler
           */
          if (focus_widget == focus_before_menu)
            {
              void *vp_focus_before_menu = &focus_before_menu;
              g_object_remove_weak_pointer (G_OBJECT (focus_before_menu), vp_focus_before_menu);
              focus_before_menu = NULL;
            }
        }
      gail_focus_notify_when_idle (focus_widget);
    }
  else
    {
      if (focus_widget)
        atk_obj  = gail_get_accessible_for_widget (focus_widget, &transient);
      else
        atk_obj = NULL;
      /*
       * Do not report focus on redundant object
       */
      if (atk_obj && 
	  (atk_object_get_role(atk_obj) != ATK_ROLE_REDUNDANT_OBJECT))
	  atk_focus_tracker_notify (atk_obj);
      if (atk_obj && transient)
        g_object_unref (atk_obj);
      if (subsequent_focus_widget)
        {
          GtkWidget *tmp_widget = subsequent_focus_widget;
          subsequent_focus_widget = NULL;
          gail_focus_notify_when_idle (tmp_widget);
        }
    }
}

static void
gail_focus_notify_when_idle (GtkWidget *widget)
{
  if (focus_notify_handler)
    {
      if (widget)
        {
          /*
           * Ignore focus request when menu item is going to be focused.
           * See bug #124232.
           */
          if (GTK_IS_MENU_ITEM (next_focus_widget) && !GTK_IS_MENU_ITEM (widget))
            return;

          if (next_focus_widget)
            {
              if (GTK_IS_MENU_ITEM (next_focus_widget) && GTK_IS_MENU_ITEM (widget))
                {
                  if (gtk_menu_item_get_submenu (GTK_MENU_ITEM (next_focus_widget)) == gtk_widget_get_parent (widget))
                    {
                      if (subsequent_focus_widget)
                        g_assert_not_reached ();
                      subsequent_focus_widget = widget;
                      return;
                    } 
                }
            }
          g_source_remove (focus_notify_handler);
          if (next_focus_widget)
	    {
	      void *vp_next_focus_widget = &next_focus_widget;
	      g_object_remove_weak_pointer (G_OBJECT (next_focus_widget), vp_next_focus_widget);
	      next_focus_widget = NULL;
	    }
        }
      else
        /*
         * Ignore if focus is being set to NULL and we are waiting to set focus
         */
        return;
    }

  if (widget)
    {
      void *vp_next_focus_widget = &next_focus_widget;
      next_focus_widget = widget;
      g_object_add_weak_pointer (G_OBJECT (next_focus_widget), vp_next_focus_widget);
    }
  else
    {
      /*
       * We are about to report focus as NULL so remove the weak pointer
       * for the widget we were waiting to report focus on.
       */ 
      if (next_focus_widget)
        {
          void *vp_next_focus_widget = &next_focus_widget;
          g_object_remove_weak_pointer (G_OBJECT (next_focus_widget), vp_next_focus_widget);
          next_focus_widget = NULL;
        }
    }

  focus_notify_handler = gdk_threads_add_idle (gail_focus_idle_handler, widget);
}

static gboolean
gail_deactivate_watcher (GSignalInvocationHint *ihint,
                         guint                  n_param_values,
                         const GValue          *param_values,
                         gpointer               data)
{
  GObject *object;
  GtkWidget *widget;
  GtkMenuShell *shell;
  GtkWidget *focus = NULL;

  object = g_value_get_object (param_values + 0);
  g_return_val_if_fail (GTK_IS_WIDGET(object), FALSE);
  widget = GTK_WIDGET (object);

  g_return_val_if_fail (GTK_IS_MENU_SHELL(widget), TRUE);
  shell = GTK_MENU_SHELL(widget);
  if (!shell->parent_menu_shell)
    focus = focus_before_menu;
      
  /*
   * If we are waiting to report focus on a menubar or a menu item
   * because of a previous deselect, cancel it.
   */
  if (was_deselect &&
      focus_notify_handler &&
      next_focus_widget &&
      (GTK_IS_MENU_BAR (next_focus_widget) ||
       GTK_IS_MENU_ITEM (next_focus_widget)))
    {
      void *vp_next_focus_widget = &next_focus_widget;
      g_source_remove (focus_notify_handler);
      g_object_remove_weak_pointer (G_OBJECT (next_focus_widget), vp_next_focus_widget);
      next_focus_widget = NULL;
      focus_notify_handler = 0;
      was_deselect = FALSE;
    }
  gail_focus_notify_when_idle (focus);

  return TRUE; 
}

static void
gail_focus_tracker_init (void)
{
  static gboolean  emission_hooks_added = FALSE;

  if (!emission_hooks_added)
    {
      /*
       * We cannot be sure that the classes exist so we make sure that they do.
       */
      g_type_class_ref (GTK_TYPE_WIDGET);
      g_type_class_ref (GTK_TYPE_ITEM);
      g_type_class_ref (GTK_TYPE_MENU_SHELL);
      g_type_class_ref (GTK_TYPE_NOTEBOOK);

      /*
       * We listen for event_after signal and then check that the
       * event was a focus in event so we get called after the event.
       */
      g_signal_add_emission_hook (
             g_signal_lookup ("event-after", GTK_TYPE_WIDGET), 0,
             gail_focus_watcher, NULL, (GDestroyNotify) NULL);
      /*
       * A "select" signal is emitted when arrow key is used to
       * move to a list item in the popup window of a GtkCombo or
       * a menu item in a menu.
       */
      g_signal_add_emission_hook (
             g_signal_lookup ("select", GTK_TYPE_ITEM), 0,
             gail_select_watcher, NULL, (GDestroyNotify) NULL);

      /*
       * A "deselect" signal is emitted when arrow key is used to
       * move from a menu item in a menu to the parent menu.
       */
      g_signal_add_emission_hook (
             g_signal_lookup ("deselect", GTK_TYPE_ITEM), 0,
             gail_deselect_watcher, NULL, (GDestroyNotify) NULL);

      /*
       * We listen for deactivate signals on menushells to determine
       * when the "focus" has left the menus.
       */
      g_signal_add_emission_hook (
             g_signal_lookup ("deactivate", GTK_TYPE_MENU_SHELL), 0,
             gail_deactivate_watcher, NULL, (GDestroyNotify) NULL);

      /*
       * We listen for "switch-page" signal on a GtkNotebook to notify
       * when page has changed because of clicking on a notebook tab.
       */
      g_signal_add_emission_hook (
             g_signal_lookup ("switch-page", GTK_TYPE_NOTEBOOK), 0,
             gail_switch_page_watcher, NULL, (GDestroyNotify) NULL);
      emission_hooks_added = TRUE;
    }
}

static void
gail_focus_object_destroyed (gpointer data)
{
  GObject *obj;

  obj = G_OBJECT (data);
  g_object_set_qdata (obj, quark_focus_object, NULL);
  g_object_unref (obj); 
}

static void
gail_focus_tracker (AtkObject *focus_object)
{
  /*
   * Do not report focus on redundant object
   */
  if (focus_object && 
      (atk_object_get_role(focus_object) != ATK_ROLE_REDUNDANT_OBJECT))
    {
      AtkObject *old_focus_object;

      if (!GTK_IS_ACCESSIBLE (focus_object))
        {
          AtkObject *parent;

          parent = focus_object;
          while (1)
            {
              parent = atk_object_get_parent (parent);
              if (parent == NULL)
                break;
              if (GTK_IS_ACCESSIBLE (parent))
                break;
            }

          if (parent)
            {
              gail_set_focus_object (focus_object, parent);
            }
        }
      else
        {
          old_focus_object = g_object_get_qdata (G_OBJECT (focus_object), quark_focus_object);
          if (old_focus_object)
            {
              g_object_weak_unref (G_OBJECT (old_focus_object),
                                   (GWeakNotify) gail_focus_object_destroyed,
                                   focus_object);
              g_object_set_qdata (G_OBJECT (focus_object), quark_focus_object, NULL);
              g_object_unref (G_OBJECT (focus_object));
            }
        }
    }
}

static void 
gail_set_focus_widget (GtkWidget *focus_widget,
                       GtkWidget *widget)
{
  AtkObject *focus_obj;
  AtkObject *obj;

  focus_obj = gtk_widget_get_accessible (focus_widget);
  obj = gtk_widget_get_accessible (widget);
  gail_set_focus_object (focus_obj, obj);
}

static void 
gail_set_focus_object (AtkObject *focus_obj,
                       AtkObject *obj)
{
  AtkObject *old_focus_obj;

  old_focus_obj = g_object_get_qdata (G_OBJECT (obj), quark_focus_object);
  if (old_focus_obj != obj)
    {
      if (old_focus_obj)
        g_object_weak_unref (G_OBJECT (old_focus_obj),
                             (GWeakNotify) gail_focus_object_destroyed,
                             obj);
      else
        /*
         * We call g_object_ref as if obj is destroyed 
         * while the weak reference exists then destroying the 
         * focus_obj would cause gail_focus_object_destroyed to be 
         * called when obj is not a valid GObject.
         */
        g_object_ref (obj);

      g_object_weak_ref (G_OBJECT (focus_obj),
                         (GWeakNotify) gail_focus_object_destroyed,
                         obj);
      g_object_set_qdata (G_OBJECT (obj), quark_focus_object, focus_obj);
    }
}

/*
 *   These exported symbols are hooked by gnome-program
 * to provide automatic module initialization and shutdown.
 */
extern void gnome_accessibility_module_init     (void);
extern void gnome_accessibility_module_shutdown (void);

static int gail_initialized = FALSE;

static void
gail_accessibility_module_init (void)
{
  const char *env_a_t_support;
  gboolean a_t_support = FALSE;

  if (gail_initialized)
    {
      return;
    }
  gail_initialized = TRUE;
  quark_focus_object = g_quark_from_static_string ("gail-focus-object");
  
  env_a_t_support = g_getenv (GNOME_ACCESSIBILITY_ENV);

  if (env_a_t_support)
    a_t_support = atoi (env_a_t_support);
  if (a_t_support)
    fprintf (stderr, "GTK Accessibility Module initialized\n");

  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_WIDGET, gail_widget);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_CONTAINER, gail_container);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_BUTTON, gail_button);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_ITEM, gail_item);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_MENU_ITEM, gail_menu_item);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_TOGGLE_BUTTON, gail_toggle_button);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_IMAGE, gail_image);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_TEXT_VIEW, gail_text_view);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_COMBO, gail_combo);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_COMBO_BOX, gail_combo_box);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_ENTRY, gail_entry);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_MENU_BAR, gail_menu_shell);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_MENU, gail_menu);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_WINDOW, gail_window);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_RANGE, gail_range);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_SCALE, gail_scale);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_SCALE_BUTTON, gail_scale_button);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_CLIST, gail_clist);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_LABEL, gail_label);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_STATUSBAR, gail_statusbar);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_NOTEBOOK, gail_notebook);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_CALENDAR, gail_calendar);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_PROGRESS_BAR, gail_progress_bar);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_SPIN_BUTTON, gail_spin_button);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_TREE_VIEW, gail_tree_view);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_FRAME, gail_frame);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_CELL_RENDERER_TEXT, gail_text_cell);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_CELL_RENDERER_TOGGLE, gail_boolean_cell);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_CELL_RENDERER_PIXBUF, gail_image_cell);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_CELL_RENDERER, gail_renderer_cell);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_RADIO_BUTTON, gail_radio_button);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_ARROW, gail_arrow);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_PIXMAP, gail_pixmap);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_SEPARATOR, gail_separator);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_BOX, gail_box);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_SCROLLED_WINDOW, gail_scrolled_window);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_LIST, gail_list);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_PANED, gail_paned);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_SCROLLBAR, gail_scrollbar);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_OPTION_MENU, gail_option_menu);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_CHECK_MENU_ITEM, gail_check_menu_item);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_RADIO_MENU_ITEM, gail_radio_menu_item);
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_EXPANDER, gail_expander);

  /* LIBGNOMECANVAS SUPPORT */
  GAIL_WIDGET_SET_FACTORY (GTK_TYPE_OBJECT, gail_object);

  atk_focus_tracker_init (gail_focus_tracker_init);
  focus_tracker_id = atk_add_focus_tracker (gail_focus_tracker);

  /* Initialize the GailUtility class */
  g_type_class_unref (g_type_class_ref (GAIL_TYPE_UTIL));
  g_type_class_unref (g_type_class_ref (GAIL_TYPE_MISC));
}

/**
 * gnome_accessibility_module_init:
 * @void: 
 * 
 *   This method is invoked by name from libgnome's
 * gnome-program.c to activate accessibility support.
 **/
void
gnome_accessibility_module_init (void)
{
  gail_accessibility_module_init ();
}

/**
 * gnome_accessibility_module_shutdown:
 * @void: 
 * 
 *   This method is invoked by name from libgnome's
 * gnome-program.c to de-activate accessibility support.
 **/
void
gnome_accessibility_module_shutdown (void)
{
  if (!gail_initialized)
    {
      return;
    }
  gail_initialized = FALSE;
  atk_remove_focus_tracker (focus_tracker_id);

  fprintf (stderr, "GTK Accessibility Module shutdown\n");

  /* FIXME: de-register the factory types so we can unload ? */
}

int
gtk_module_init (gint *argc, char** argv[])
{
  const char* env_no_gail;
  gboolean no_gail = FALSE;

  env_no_gail = g_getenv (NO_GAIL_ENV);
  if (env_no_gail)
      no_gail = atoi (env_no_gail);

  if (no_gail)
      return 0;

  gail_accessibility_module_init ();

  return 0;
}

const char *
g_module_check_init (GModule *module)
{
  g_module_make_resident (module);

  return NULL;
}
 
