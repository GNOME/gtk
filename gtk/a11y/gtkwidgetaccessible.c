/* GTK+ - accessibility implementations
 * Copyright 2001, 2002, 2003 Sun Microsystems Inc.
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

#include <gtk/gtk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif
#include "gtkwidgetaccessibleprivate.h"
#include "gtknotebookpageaccessible.h"

struct _GtkWidgetAccessiblePrivate
{
  AtkLayer layer;
};

#define TOOLTIP_KEY "tooltip"

extern GtkWidget *_focus_widget;


static gboolean gtk_widget_accessible_on_screen           (GtkWidget *widget);
static gboolean gtk_widget_accessible_all_parents_visible (GtkWidget *widget);

static void atk_component_interface_init (AtkComponentIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkWidgetAccessible, gtk_widget_accessible, GTK_TYPE_ACCESSIBLE,
                         G_ADD_PRIVATE (GtkWidgetAccessible)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_COMPONENT, atk_component_interface_init))

/* Translate GtkWidget::focus-in/out-event to AtkObject::focus-event */
static gboolean
focus_cb (GtkWidget     *widget,
          GdkEventFocus *event)
{
  AtkObject *obj;

  obj = gtk_widget_get_accessible (widget);

  g_signal_emit_by_name (obj, "focus-event", event->in);

  return FALSE;
}

/* Translate GtkWidget property change notification to the notify_gtk vfunc */
static void
notify_cb (GObject    *obj,
           GParamSpec *pspec)
{
  GtkWidgetAccessible *widget;
  GtkWidgetAccessibleClass *klass;

  widget = GTK_WIDGET_ACCESSIBLE (gtk_widget_get_accessible (GTK_WIDGET (obj)));
  klass = GTK_WIDGET_ACCESSIBLE_GET_CLASS (widget);
  if (klass->notify_gtk)
    klass->notify_gtk (obj, pspec);
}

/* Translate GtkWidget::size-allocate to AtkComponent::bounds-changed */
static void
size_allocate_cb (GtkWidget     *widget,
                  GtkAllocation *allocation)
{
  AtkObject* accessible;
  AtkRectangle rect;

  accessible = gtk_widget_get_accessible (widget);
  if (ATK_IS_COMPONENT (accessible))
    {
      rect.x = allocation->x;
      rect.y = allocation->y;
      rect.width = allocation->width;
      rect.height = allocation->height;
      g_signal_emit_by_name (accessible, "bounds-changed", &rect);
    }
}

/* Translate GtkWidget mapped state into AtkObject showing */
static gint
map_cb (GtkWidget *widget)
{
  AtkObject *accessible;

  accessible = gtk_widget_get_accessible (widget);
  atk_object_notify_state_change (accessible, ATK_STATE_SHOWING,
                                  gtk_widget_get_mapped (widget));
  return 1;
}

static void
gtk_widget_accessible_focus_event (AtkObject *obj,
                                   gboolean   focus_in)
{
  AtkObject *focus_obj;

  focus_obj = g_object_get_data (G_OBJECT (obj), "gail-focus-object");
  if (focus_obj == NULL)
    focus_obj = obj;
  atk_object_notify_state_change (focus_obj, ATK_STATE_FOCUSED, focus_in);
}

static void
gtk_widget_accessible_update_tooltip (GtkWidgetAccessible *accessible,
                                      GtkWidget *widget)
{
  g_object_set_data_full (G_OBJECT (accessible),
                          TOOLTIP_KEY,
                          gtk_widget_get_tooltip_text (widget),
                          g_free);
}

static void
gtk_widget_accessible_initialize (AtkObject *obj,
                                  gpointer   data)
{
  GtkWidget *widget;

  widget = GTK_WIDGET (data);

  g_signal_connect_after (widget, "focus-in-event", G_CALLBACK (focus_cb), NULL);
  g_signal_connect_after (widget, "focus-out-event", G_CALLBACK (focus_cb), NULL);
  g_signal_connect (widget, "notify", G_CALLBACK (notify_cb), NULL);
  g_signal_connect (widget, "size-allocate", G_CALLBACK (size_allocate_cb), NULL);
  g_signal_connect (widget, "map", G_CALLBACK (map_cb), NULL);
  g_signal_connect (widget, "unmap", G_CALLBACK (map_cb), NULL);

  GTK_WIDGET_ACCESSIBLE (obj)->priv->layer = ATK_LAYER_WIDGET;
  obj->role = ATK_ROLE_UNKNOWN;

  gtk_widget_accessible_update_tooltip (GTK_WIDGET_ACCESSIBLE (obj), widget);
}

static const gchar *
gtk_widget_accessible_get_description (AtkObject *accessible)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
  if (widget == NULL)
    return NULL;

  if (accessible->description)
    return accessible->description;

  return g_object_get_data (G_OBJECT (accessible), TOOLTIP_KEY);
}

static AtkObject *
gtk_widget_accessible_get_parent (AtkObject *accessible)
{
  AtkObject *parent;
  GtkWidget *widget, *parent_widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
  if (widget == NULL)
    return NULL;

  parent = accessible->accessible_parent;
  if (parent != NULL)
    return parent;

  parent_widget = gtk_widget_get_parent (widget);
  if (parent_widget == NULL)
    return NULL;

  /* For a widget whose parent is a GtkNoteBook, we return the
   * accessible object corresponding the GtkNotebookPage containing
   * the widget as the accessible parent.
   */
  if (GTK_IS_NOTEBOOK (parent_widget))
    {
      gint page_num;
      GtkWidget *child;
      GtkNotebook *notebook;

      page_num = 0;
      notebook = GTK_NOTEBOOK (parent_widget);
      while (TRUE)
        {
          child = gtk_notebook_get_nth_page (notebook, page_num);
          if (!child)
            break;
          if (child == widget)
            {
              parent = gtk_widget_get_accessible (parent_widget);
              parent = atk_object_ref_accessible_child (parent, page_num);
              g_object_unref (parent);
              return parent;
            }
          page_num++;
        }
    }
  parent = gtk_widget_get_accessible (parent_widget);
  return parent;
}

static GtkWidget *
find_label (GtkWidget *widget)
{
  GList *labels;
  GtkWidget *label;
  GtkWidget *temp_widget;
  GList *ptr;

  labels = gtk_widget_list_mnemonic_labels (widget);
  label = NULL;
  ptr = labels;
  while (ptr)
    {
      if (ptr->data)
        {
          label = ptr->data;
          break;
        }
      ptr = ptr->next;
    }
  g_list_free (labels);

  /* Ignore a label within a button; bug #136602 */
  if (label && GTK_IS_BUTTON (widget))
    {
      temp_widget = label;
      while (temp_widget)
        {
          if (temp_widget == widget)
            {
              label = NULL;
              break;
            }
          temp_widget = gtk_widget_get_parent (temp_widget);
        }
    }
  return label;
}

static AtkRelationSet *
gtk_widget_accessible_ref_relation_set (AtkObject *obj)
{
  GtkWidget *widget;
  AtkRelationSet *relation_set;
  GtkWidget *label;
  AtkObject *array[1];
  AtkRelation* relation;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return NULL;

  relation_set = ATK_OBJECT_CLASS (gtk_widget_accessible_parent_class)->ref_relation_set (obj);

  if (GTK_IS_BOX (widget))
    return relation_set;

  if (!atk_relation_set_contains (relation_set, ATK_RELATION_LABELLED_BY))
    {
      label = find_label (widget);
      if (label == NULL)
        {
          if (GTK_IS_BUTTON (widget) && gtk_widget_get_mapped (widget))
            /*
             * Handle the case where GnomeIconEntry is the mnemonic widget.
             * The GtkButton which is a grandchild of the GnomeIconEntry
             * should really be the mnemonic widget. See bug #133967.
             */
            {
              GtkWidget *temp_widget;

              temp_widget = gtk_widget_get_parent (widget);

              if (GTK_IS_ALIGNMENT (temp_widget))
                {
                  temp_widget = gtk_widget_get_parent (temp_widget);
                  if (GTK_IS_BOX (temp_widget))
                    {
                      label = find_label (temp_widget);
                      if (!label)
                        label = find_label (gtk_widget_get_parent (temp_widget));
                    }
                }
            }
          else if (GTK_IS_COMBO_BOX (widget))
            /*
             * Handle the case when GtkFileChooserButton is the mnemonic
             * widget.  The GtkComboBox which is a child of the
             * GtkFileChooserButton should be the mnemonic widget.
             * See bug #359843.
             */
            {
              GtkWidget *temp_widget;

              temp_widget = gtk_widget_get_parent (widget);
              if (GTK_IS_BOX (temp_widget))
                {
                  label = find_label (temp_widget);
                }
            }
        }

      if (label)
        {
          array[0] = gtk_widget_get_accessible (label);

          relation = atk_relation_new (array, 1, ATK_RELATION_LABELLED_BY);
          atk_relation_set_add (relation_set, relation);
          g_object_unref (relation);
        }
    }

  return relation_set;
}

static AtkStateSet *
gtk_widget_accessible_ref_state_set (AtkObject *accessible)
{
  GtkWidget *widget;
  AtkStateSet *state_set;

  state_set = ATK_OBJECT_CLASS (gtk_widget_accessible_parent_class)->ref_state_set (accessible);

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
  if (widget == NULL)
    atk_state_set_add_state (state_set, ATK_STATE_DEFUNCT);
  else
    {
      if (gtk_widget_is_sensitive (widget))
        {
          atk_state_set_add_state (state_set, ATK_STATE_SENSITIVE);
          atk_state_set_add_state (state_set, ATK_STATE_ENABLED);
        }
  
      if (gtk_widget_get_can_focus (widget))
        {
          atk_state_set_add_state (state_set, ATK_STATE_FOCUSABLE);
        }
      /*
       * We do not currently generate notifications when an ATK object
       * corresponding to a GtkWidget changes visibility by being scrolled
       * on or off the screen.  The testcase for this is the main window
       * of the testgtk application in which a set of buttons in a GtkVBox
       * is in a scrolled window with a viewport.
       *
       * To generate the notifications we would need to do the following:
       * 1) Find the GtkViewport among the ancestors of the objects
       * 2) Create an accessible for the viewport
       * 3) Connect to the value-changed signal on the viewport
       * 4) When the signal is received we need to traverse the children
       *    of the viewport and check whether the children are visible or not
       *    visible; we may want to restrict this to the widgets for which
       *    accessible objects have been created.
       * 5) We probably need to store a variable on_screen in the
       *    GtkWidgetAccessible data structure so we can determine whether
       *    the value has changed.
       */
      if (gtk_widget_get_visible (widget))
        {
          atk_state_set_add_state (state_set, ATK_STATE_VISIBLE);
          if (gtk_widget_accessible_on_screen (widget) &&
              gtk_widget_get_mapped (widget) &&
              gtk_widget_accessible_all_parents_visible (widget))
            atk_state_set_add_state (state_set, ATK_STATE_SHOWING);
        }

      if (gtk_widget_has_focus (widget) && (widget == _focus_widget))
        {
          AtkObject *focus_obj;

          focus_obj = g_object_get_data (G_OBJECT (accessible), "gail-focus-object");
          if (focus_obj == NULL)
            atk_state_set_add_state (state_set, ATK_STATE_FOCUSED);
        }

      if (gtk_widget_has_default (widget))
        atk_state_set_add_state (state_set, ATK_STATE_DEFAULT);

      if (GTK_IS_ORIENTABLE (widget))
        {
          if (gtk_orientable_get_orientation (GTK_ORIENTABLE (widget)) == GTK_ORIENTATION_HORIZONTAL)
            atk_state_set_add_state (state_set, ATK_STATE_HORIZONTAL);
          else
            atk_state_set_add_state (state_set, ATK_STATE_VERTICAL);
        }
    }
  return state_set;
}

static gint
gtk_widget_accessible_get_index_in_parent (AtkObject *accessible)
{
  GtkWidget *widget;
  GtkWidget *parent_widget;
  gint index;
  GList *children;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));

  if (widget == NULL)
    return -1;

  if (accessible->accessible_parent)
    {
      AtkObject *parent;

      parent = accessible->accessible_parent;

      if (GTK_IS_NOTEBOOK_PAGE_ACCESSIBLE (parent))
        return 0;
      else
        {
          gint n_children, i;
          gboolean found = FALSE;

          n_children = atk_object_get_n_accessible_children (parent);
          for (i = 0; i < n_children; i++)
            {
              AtkObject *child;

              child = atk_object_ref_accessible_child (parent, i);
              if (child == accessible)
                found = TRUE;

              g_object_unref (child);
              if (found)
                return i;
            }
        }
    }

  if (!GTK_IS_WIDGET (widget))
    return -1;
  parent_widget = gtk_widget_get_parent (widget);
  if (!GTK_IS_CONTAINER (parent_widget))
    return -1;

  children = gtk_container_get_children (GTK_CONTAINER (parent_widget));

  index = g_list_index (children, widget);
  g_list_free (children);
  return index;
}

/* This function is the default implementation for the notify_gtk
 * vfunc which gets called when a property changes value on the
 * GtkWidget associated with a GtkWidgetAccessible. It constructs
 * an AtkPropertyValues structure and emits a "property_changed"
 * signal which causes the user specified AtkPropertyChangeHandler
 * to be called.
 */
static void
gtk_widget_accessible_notify_gtk (GObject    *obj,
                                  GParamSpec *pspec)
{
  GtkWidget* widget = GTK_WIDGET (obj);
  AtkObject* atk_obj = gtk_widget_get_accessible (widget);
  AtkState state;
  gboolean value;

  if (g_strcmp0 (pspec->name, "has-focus") == 0)
    /*
     * We use focus-in-event and focus-out-event signals to catch
     * focus changes so we ignore this.
     */
    return;
  else if (g_strcmp0 (pspec->name, "tooltip-text") == 0)
    {
      gtk_widget_accessible_update_tooltip (GTK_WIDGET_ACCESSIBLE (atk_obj),
                                            widget);
      return;
    }
  else if (g_strcmp0 (pspec->name, "visible") == 0)
    {
      state = ATK_STATE_VISIBLE;
      value = gtk_widget_get_visible (widget);
    }
  else if (g_strcmp0 (pspec->name, "sensitive") == 0)
    {
      state = ATK_STATE_SENSITIVE;
      value = gtk_widget_get_sensitive (widget);
    }
  else if (g_strcmp0 (pspec->name, "orientation") == 0 &&
           GTK_IS_ORIENTABLE (widget))
    {
      GtkOrientable *orientable;

      orientable = GTK_ORIENTABLE (widget);

      state = ATK_STATE_HORIZONTAL;
      value = (gtk_orientable_get_orientation (orientable) == GTK_ORIENTATION_HORIZONTAL);
    }
  else
    return;

  atk_object_notify_state_change (atk_obj, state, value);
  if (state == ATK_STATE_SENSITIVE)
    atk_object_notify_state_change (atk_obj, ATK_STATE_ENABLED, value);

  if (state == ATK_STATE_HORIZONTAL)
    atk_object_notify_state_change (atk_obj, ATK_STATE_VERTICAL, !value);
}

static AtkAttributeSet *
gtk_widget_accessible_get_attributes (AtkObject *obj)
{
  AtkAttributeSet *attributes;
  AtkAttribute *toolkit;

  toolkit = g_new (AtkAttribute, 1);
  toolkit->name = g_strdup ("toolkit");
  toolkit->value = g_strdup ("gtk");

  attributes = g_slist_append (NULL, toolkit);

  return attributes;
}

static void
gtk_widget_accessible_class_init (GtkWidgetAccessibleClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  klass->notify_gtk = gtk_widget_accessible_notify_gtk;

  class->get_description = gtk_widget_accessible_get_description;
  class->get_parent = gtk_widget_accessible_get_parent;
  class->ref_relation_set = gtk_widget_accessible_ref_relation_set;
  class->ref_state_set = gtk_widget_accessible_ref_state_set;
  class->get_index_in_parent = gtk_widget_accessible_get_index_in_parent;
  class->initialize = gtk_widget_accessible_initialize;
  class->get_attributes = gtk_widget_accessible_get_attributes;
  class->focus_event = gtk_widget_accessible_focus_event;
}

static void
gtk_widget_accessible_init (GtkWidgetAccessible *accessible)
{
  accessible->priv = gtk_widget_accessible_get_instance_private (accessible);
}

static void
gtk_widget_accessible_get_extents (AtkComponent   *component,
                                   gint           *x,
                                   gint           *y,
                                   gint           *width,
                                   gint           *height,
                                   AtkCoordType    coord_type)
{
  GdkWindow *window;
  gint x_window, y_window;
  gint x_toplevel, y_toplevel;
  GtkWidget *widget;
  GtkAllocation allocation;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (component));
  if (widget == NULL)
    return;

  gtk_widget_get_allocation (widget, &allocation);
  *width = allocation.width;
  *height = allocation.height;
  if (!gtk_widget_accessible_on_screen (widget) || (!gtk_widget_is_drawable (widget)))
    {
      *x = G_MININT;
      *y = G_MININT;
      return;
    }

  if (gtk_widget_get_parent (widget))
    {
      *x = allocation.x;
      *y = allocation.y;
      window = gtk_widget_get_parent_window (widget);
    }
  else
    {
      *x = 0;
      *y = 0;
      window = gtk_widget_get_window (widget);
    }
  gdk_window_get_origin (window, &x_window, &y_window);
  *x += x_window;
  *y += y_window;

  if (coord_type == ATK_XY_WINDOW)
    {
      window = gdk_window_get_toplevel (gtk_widget_get_window (widget));
      gdk_window_get_origin (window, &x_toplevel, &y_toplevel);

      *x -= x_toplevel;
      *y -= y_toplevel;
    }
}

static void
gtk_widget_accessible_get_size (AtkComponent *component,
                                gint         *width,
                                gint         *height)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (component));
  if (widget == NULL)
    return;

  *width = gtk_widget_get_allocated_width (widget);
  *height = gtk_widget_get_allocated_height (widget);
}

static AtkLayer
gtk_widget_accessible_get_layer (AtkComponent *component)
{
  GtkWidgetAccessible *accessible = GTK_WIDGET_ACCESSIBLE (component);

  return accessible->priv->layer;
}

static gboolean
gtk_widget_accessible_grab_focus (AtkComponent *component)
{
  GtkWidget *widget;
  GtkWidget *toplevel;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (component));
  if (!widget)
    return FALSE;

  if (!gtk_widget_get_can_focus (widget))
    return FALSE;

  gtk_widget_grab_focus (widget);
  toplevel = gtk_widget_get_toplevel (widget);
  if (gtk_widget_is_toplevel (toplevel))
    {
#ifdef GDK_WINDOWING_X11
      gtk_window_present_with_time (GTK_WINDOW (toplevel),
      gdk_x11_get_server_time (gtk_widget_get_window (widget)));
#else
      gtk_window_present (GTK_WINDOW (toplevel));
#endif
    }
  return TRUE;
}

static gboolean
gtk_widget_accessible_set_extents (AtkComponent *component,
                                   gint          x,
                                   gint          y,
                                   gint          width,
                                   gint          height,
                                   AtkCoordType  coord_type)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (component));
  if (widget == NULL)
    return FALSE;

  if (!gtk_widget_is_toplevel (widget))
    return FALSE;

  if (coord_type == ATK_XY_WINDOW)
    {
      gint x_current, y_current;
      GdkWindow *window = gtk_widget_get_window (widget);

      gdk_window_get_origin (window, &x_current, &y_current);
      x_current += x;
      y_current += y;
      if (x_current < 0 || y_current < 0)
        return FALSE;
      else
        {
          gtk_window_move (GTK_WINDOW (widget), x_current, y_current);
          gtk_widget_set_size_request (widget, width, height);
          return TRUE;
        }
    }
  else if (coord_type == ATK_XY_SCREEN)
    {
      gtk_window_move (GTK_WINDOW (widget), x, y);
      gtk_widget_set_size_request (widget, width, height);
      return TRUE;
    }
  return FALSE;
}

static gboolean
gtk_widget_accessible_set_position (AtkComponent *component,
                                    gint          x,
                                    gint          y,
                                    AtkCoordType  coord_type)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (component));
  if (widget == NULL)
    return FALSE;

  if (gtk_widget_is_toplevel (widget))
    {
      if (coord_type == ATK_XY_WINDOW)
        {
          gint x_current, y_current;
          GdkWindow *window = gtk_widget_get_window (widget);

          gdk_window_get_origin (window, &x_current, &y_current);
          x_current += x;
          y_current += y;
          if (x_current < 0 || y_current < 0)
            return FALSE;
          else
            {
              gtk_window_move (GTK_WINDOW (widget), x_current, y_current);
              return TRUE;
            }
        }
      else if (coord_type == ATK_XY_SCREEN)
        {
          gtk_window_move (GTK_WINDOW (widget), x, y);
          return TRUE;
        }
    }
  return FALSE;
}

static gboolean
gtk_widget_accessible_set_size (AtkComponent *component,
                                gint          width,
                                gint          height)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (component));
  if (widget == NULL)
    return FALSE;

  if (gtk_widget_is_toplevel (widget))
    {
      gtk_widget_set_size_request (widget, width, height);
      return TRUE;
    }
  else
   return FALSE;
}

static void
atk_component_interface_init (AtkComponentIface *iface)
{
  iface->get_extents = gtk_widget_accessible_get_extents;
  iface->get_size = gtk_widget_accessible_get_size;
  iface->get_layer = gtk_widget_accessible_get_layer;
  iface->grab_focus = gtk_widget_accessible_grab_focus;
  iface->set_extents = gtk_widget_accessible_set_extents;
  iface->set_position = gtk_widget_accessible_set_position;
  iface->set_size = gtk_widget_accessible_set_size;
}

/* This function checks whether the widget has an ancestor which is
 * a GtkViewport and, if so, whether any part of the widget intersects
 * the visible rectangle of the GtkViewport.
 */
static gboolean
gtk_widget_accessible_on_screen (GtkWidget *widget)
{
  GtkAllocation allocation;
  GtkWidget *viewport;
  gboolean return_value;

  gtk_widget_get_allocation (widget, &allocation);

  if (!gtk_widget_get_mapped (widget))
    return FALSE;

  viewport = gtk_widget_get_ancestor (widget, GTK_TYPE_VIEWPORT);
  if (viewport)
    {
      GtkAllocation viewport_allocation;
      GtkAdjustment *adjustment;
      GdkRectangle visible_rect;

      gtk_widget_get_allocation (viewport, &viewport_allocation);

      adjustment = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (viewport));
      visible_rect.y = gtk_adjustment_get_value (adjustment);
      adjustment = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (viewport));
      visible_rect.x = gtk_adjustment_get_value (adjustment);
      visible_rect.width = viewport_allocation.width;
      visible_rect.height = viewport_allocation.height;

      if (((allocation.x + allocation.width) < visible_rect.x) ||
         ((allocation.y + allocation.height) < visible_rect.y) ||
         (allocation.x > (visible_rect.x + visible_rect.width)) ||
         (allocation.y > (visible_rect.y + visible_rect.height)))
        return_value = FALSE;
      else
        return_value = TRUE;
    }
  else
    {
      /* Check whether the widget has been placed of the screen.
       * The widget may be MAPPED as when toolbar items do not
       * fit on the toolbar.
       */
      if (allocation.x + allocation.width <= 0 &&
          allocation.y + allocation.height <= 0)
        return_value = FALSE;
      else
        return_value = TRUE;
    }

  return return_value;
}

/* Checks if all the predecessors (the parent widget, his parent, etc)
 * are visible Used to check properly the SHOWING state.
 */
static gboolean
gtk_widget_accessible_all_parents_visible (GtkWidget *widget)
{
  GtkWidget *iter_parent = NULL;
  gboolean result = TRUE;

  for (iter_parent = gtk_widget_get_parent (widget); iter_parent;
       iter_parent = gtk_widget_get_parent (iter_parent))
    {
      if (!gtk_widget_get_visible (iter_parent))
        {
          result = FALSE;
          break;
        }
    }

  return result;
}

void
_gtk_widget_accessible_set_layer (GtkWidgetAccessible *accessible,
                                  AtkLayer             layer)
{
  accessible->priv->layer = layer;
}
