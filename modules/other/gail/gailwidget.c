/* GAIL - The GNOME Accessibility Implementation Library
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <string.h>

#undef GTK_DISABLE_DEPRECATED

#include <gtk/gtk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif
#include "gailwidget.h"
#include "gailnotebookpage.h"
#include "gail-private-macros.h"

extern GtkWidget *focus_widget;

static void gail_widget_class_init (GailWidgetClass *klass);
static void gail_widget_init                     (GailWidget       *accessible);
static void gail_widget_connect_widget_destroyed (GtkAccessible    *accessible);
static void gail_widget_destroyed                (GtkWidget        *widget,
                                                  GtkAccessible    *accessible);

static const gchar* gail_widget_get_description (AtkObject *accessible);
static AtkObject* gail_widget_get_parent (AtkObject *accessible);
static AtkStateSet* gail_widget_ref_state_set (AtkObject *accessible);
static AtkRelationSet* gail_widget_ref_relation_set (AtkObject *accessible);
static gint gail_widget_get_index_in_parent (AtkObject *accessible);

static void atk_component_interface_init (AtkComponentIface *iface);

static guint    gail_widget_add_focus_handler
                                           (AtkComponent    *component,
                                            AtkFocusHandler handler);

static void     gail_widget_get_extents    (AtkComponent    *component,
                                            gint            *x,
                                            gint            *y,
                                            gint            *width,
                                            gint            *height,
                                            AtkCoordType    coord_type);

static void     gail_widget_get_size       (AtkComponent    *component,
                                            gint            *width,
                                            gint            *height);

static AtkLayer gail_widget_get_layer      (AtkComponent *component);

static gboolean gail_widget_grab_focus     (AtkComponent    *component);


static void     gail_widget_remove_focus_handler 
                                           (AtkComponent    *component,
                                            guint           handler_id);

static gboolean gail_widget_set_extents    (AtkComponent    *component,
                                            gint            x,
                                            gint            y,
                                            gint            width,
                                            gint            height,
                                            AtkCoordType    coord_type);

static gboolean gail_widget_set_position   (AtkComponent    *component,
                                            gint            x,
                                            gint            y,
                                            AtkCoordType    coord_type);

static gboolean gail_widget_set_size       (AtkComponent    *component,
                                            gint            width,
                                            gint            height);

static gint       gail_widget_map_gtk            (GtkWidget     *widget);
static void       gail_widget_real_notify_gtk    (GObject       *obj,
                                                  GParamSpec    *pspec);
static void       gail_widget_notify_gtk         (GObject       *obj,
                                                  GParamSpec    *pspec);
static gboolean   gail_widget_focus_gtk          (GtkWidget     *widget,
                                                  GdkEventFocus *event);
static gboolean   gail_widget_real_focus_gtk     (GtkWidget     *widget,
                                                  GdkEventFocus *event);
static void       gail_widget_size_allocate_gtk  (GtkWidget     *widget,
                                                  GtkAllocation *allocation);

static void       gail_widget_focus_event        (AtkObject     *obj,
                                                  gboolean      focus_in);

static void       gail_widget_real_initialize    (AtkObject     *obj,
                                                  gpointer      data);
static GtkWidget* gail_widget_find_viewport      (GtkWidget     *widget);
static gboolean   gail_widget_on_screen          (GtkWidget     *widget);
static gboolean   gail_widget_all_parents_visible(GtkWidget     *widget);

G_DEFINE_TYPE_WITH_CODE (GailWidget, gail_widget, GTK_TYPE_ACCESSIBLE,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_COMPONENT, atk_component_interface_init))

static void
gail_widget_class_init (GailWidgetClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);
  GtkAccessibleClass *accessible_class = GTK_ACCESSIBLE_CLASS (klass);

  klass->notify_gtk = gail_widget_real_notify_gtk;
  klass->focus_gtk = gail_widget_real_focus_gtk;

  accessible_class->connect_widget_destroyed = gail_widget_connect_widget_destroyed;

  class->get_description = gail_widget_get_description;
  class->get_parent = gail_widget_get_parent;
  class->ref_relation_set = gail_widget_ref_relation_set;
  class->ref_state_set = gail_widget_ref_state_set;
  class->get_index_in_parent = gail_widget_get_index_in_parent;
  class->initialize = gail_widget_real_initialize;
}

static void
gail_widget_init (GailWidget *accessible)
{
}

/**
 * This function  specifies the GtkWidget for which the GailWidget was created 
 * and specifies a handler to be called when the GtkWidget is destroyed.
 **/
static void 
gail_widget_real_initialize (AtkObject *obj,
                             gpointer  data)
{
  GtkAccessible *accessible;
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_WIDGET (data));

  widget = GTK_WIDGET (data);

  accessible = GTK_ACCESSIBLE (obj);
  accessible->widget = widget;
  gtk_accessible_connect_widget_destroyed (accessible);
  g_signal_connect_after (widget,
                          "focus-in-event",
                          G_CALLBACK (gail_widget_focus_gtk),
                          NULL);
  g_signal_connect_after (widget,
                          "focus-out-event",
                          G_CALLBACK (gail_widget_focus_gtk),
                          NULL);
  g_signal_connect (widget,
                    "notify",
                    G_CALLBACK (gail_widget_notify_gtk),
                    NULL);
  g_signal_connect (widget,
                    "size_allocate",
                    G_CALLBACK (gail_widget_size_allocate_gtk),
                    NULL);
  atk_component_add_focus_handler (ATK_COMPONENT (accessible),
                                   gail_widget_focus_event);
  /*
   * Add signal handlers for GTK signals required to support property changes
   */
  g_signal_connect (widget,
                    "map",
                    G_CALLBACK (gail_widget_map_gtk),
                    NULL);
  g_signal_connect (widget,
                    "unmap",
                    G_CALLBACK (gail_widget_map_gtk),
                    NULL);
  g_object_set_data (G_OBJECT (obj), "atk-component-layer",
		     GINT_TO_POINTER (ATK_LAYER_WIDGET));

  obj->role = ATK_ROLE_UNKNOWN;
}

AtkObject* 
gail_widget_new (GtkWidget *widget)
{
  GObject *object;
  AtkObject *accessible;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  object = g_object_new (GAIL_TYPE_WIDGET, NULL);

  accessible = ATK_OBJECT (object);
  atk_object_initialize (accessible, widget);

  return accessible;
}

/*
 * This function specifies the function to be called when the widget
 * is destroyed
 */
static void
gail_widget_connect_widget_destroyed (GtkAccessible *accessible)
{
  if (accessible->widget)
    {
      g_signal_connect_after (accessible->widget,
                              "destroy",
                              G_CALLBACK (gail_widget_destroyed),
                              accessible);
    }
}

/*
 * This function is called when the widget is destroyed.
 * It sets the widget field in the GtkAccessible structure to NULL
 * and emits a state-change signal for the state ATK_STATE_DEFUNCT
 */
static void 
gail_widget_destroyed (GtkWidget     *widget,
                       GtkAccessible *accessible)
{
  accessible->widget = NULL;
  atk_object_notify_state_change (ATK_OBJECT (accessible), ATK_STATE_DEFUNCT,
                                  TRUE);
}

static const gchar*
gail_widget_get_description (AtkObject *accessible)
{
  if (accessible->description)
    return accessible->description;
  else
    {
      /* Get the tooltip from the widget */
      GtkAccessible *obj = GTK_ACCESSIBLE (accessible);

      gail_return_val_if_fail (obj, NULL);

      if (obj->widget == NULL)
        /*
         * Object is defunct
         */
        return NULL;
 
      gail_return_val_if_fail (GTK_WIDGET (obj->widget), NULL);

      return gtk_widget_get_tooltip_text (obj->widget);
    }
}

static AtkObject* 
gail_widget_get_parent (AtkObject *accessible)
{
  AtkObject *parent;

  parent = accessible->accessible_parent;

  if (parent != NULL)
    g_return_val_if_fail (ATK_IS_OBJECT (parent), NULL);
  else
    {
      GtkWidget *widget, *parent_widget;

      widget = GTK_ACCESSIBLE (accessible)->widget;
      if (widget == NULL)
        /*
         * State is defunct
         */
        return NULL;
      gail_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

      parent_widget = widget->parent;
      if (parent_widget == NULL)
        return NULL;

      /*
       * For a widget whose parent is a GtkNoteBook, we return the
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
    }
  return parent;
}

static GtkWidget*
find_label (GtkWidget *widget)
{
  GList *labels;
  GtkWidget *label;
  GtkWidget *temp_widget;

  labels = gtk_widget_list_mnemonic_labels (widget);
  label = NULL;
  if (labels)
    {
      if (labels->data)
        {
          if (labels->next)
            {
              g_warning ("Widget (%s) has more than one label", G_OBJECT_TYPE_NAME (widget));
              
            }
          else
            {
              label = labels->data;
            }
        }
      g_list_free (labels);
    }

  /*
   * Ignore a label within a button; bug #136602
   */
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

static AtkRelationSet*
gail_widget_ref_relation_set (AtkObject *obj)
{
  GtkWidget *widget;
  AtkRelationSet *relation_set;
  GtkWidget *label;
  AtkObject *array[1];
  AtkRelation* relation;

  gail_return_val_if_fail (GAIL_IS_WIDGET (obj), NULL);

  widget = GTK_ACCESSIBLE (obj)->widget;
  if (widget == NULL)
    /*
     * State is defunct
     */
    return NULL;

  relation_set = ATK_OBJECT_CLASS (gail_widget_parent_class)->ref_relation_set (obj);

  if (GTK_IS_BOX (widget) && !GTK_IS_COMBO (widget))
      /*
       * Do not report labelled-by for a GtkBox which could be a 
       * GnomeFileEntry.
       */
    return relation_set;

  if (!atk_relation_set_contains (relation_set, ATK_RELATION_LABELLED_BY))
    {
      label = find_label (widget);
      if (label == NULL)
        {
          if (GTK_IS_BUTTON (widget))
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
          else if (GTK_IS_COMBO (widget))
            /*
             * Handle the case when GnomeFileEntry is the mnemonic widget.
             * The GnomeEntry which is a grandchild of the GnomeFileEntry
             * should be the mnemonic widget. See bug #137584.
             */
            {
              GtkWidget *temp_widget;

              temp_widget = gtk_widget_get_parent (widget);

              if (GTK_IS_HBOX (temp_widget))
                {
                  temp_widget = gtk_widget_get_parent (temp_widget);
                  if (GTK_IS_BOX (temp_widget))
                    {
                      label = find_label (temp_widget);
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
              if (GTK_IS_HBOX (temp_widget))
                {
                  label = find_label (temp_widget);
                }
            }
        }

      if (label)
        {
	  array [0] = gtk_widget_get_accessible (label);

	  relation = atk_relation_new (array, 1, ATK_RELATION_LABELLED_BY);
	  atk_relation_set_add (relation_set, relation);
	  g_object_unref (relation);
        }
    }

  return relation_set;
}

static AtkStateSet*
gail_widget_ref_state_set (AtkObject *accessible)
{
  GtkWidget *widget = GTK_ACCESSIBLE (accessible)->widget;
  AtkStateSet *state_set;

  state_set = ATK_OBJECT_CLASS (gail_widget_parent_class)->ref_state_set (accessible);

  if (widget == NULL)
    {
      atk_state_set_add_state (state_set, ATK_STATE_DEFUNCT);
    }
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
       * is in a scrooled window with a viewport.
       *
       * To generate the notifications we would need to do the following: 
       * 1) Find the GtkViewPort among the antecendents of the objects
       * 2) Create an accesible for the GtkViewPort
       * 3) Connect to the value-changed signal on the viewport
       * 4) When the signal is received we need to traverse the children 
       * of the viewport and check whether the children are visible or not 
       * visible; we may want to restrict this to the widgets for which 
       * accessible objects have been created.
       * 5) We probably need to store a variable on_screen in the 
       * GailWidget data structure so we can determine whether the value has 
       * changed.
       */
      if (gtk_widget_get_visible (widget))
        {
          atk_state_set_add_state (state_set, ATK_STATE_VISIBLE);
          if (gail_widget_on_screen (widget) && gtk_widget_get_mapped (widget) &&
              gail_widget_all_parents_visible (widget))
            {
              atk_state_set_add_state (state_set, ATK_STATE_SHOWING);
            }
        }
  
      if (gtk_widget_has_focus (widget) && (widget == focus_widget))
        {
          AtkObject *focus_obj;

          focus_obj = g_object_get_data (G_OBJECT (accessible), "gail-focus-object");
          if (focus_obj == NULL)
            atk_state_set_add_state (state_set, ATK_STATE_FOCUSED);
        }
      if (gtk_widget_has_default (widget))
        {
          atk_state_set_add_state (state_set, ATK_STATE_DEFAULT);
        }
    }
  return state_set;
}

static gint
gail_widget_get_index_in_parent (AtkObject *accessible)
{
  GtkWidget *widget;
  GtkWidget *parent_widget;
  gint index;
  GList *children;
  GType type;

  type = g_type_from_name ("GailCanvasWidget");
  widget = GTK_ACCESSIBLE (accessible)->widget;

  if (widget == NULL)
    /*
     * State is defunct
     */
    return -1;

  if (accessible->accessible_parent)
    {
      AtkObject *parent;

      parent = accessible->accessible_parent;

      if (GAIL_IS_NOTEBOOK_PAGE (parent) ||
          G_TYPE_CHECK_INSTANCE_TYPE ((parent), type))
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

  gail_return_val_if_fail (GTK_IS_WIDGET (widget), -1);
  parent_widget = widget->parent;
  if (parent_widget == NULL)
    return -1;
  gail_return_val_if_fail (GTK_IS_CONTAINER (parent_widget), -1);

  children = gtk_container_get_children (GTK_CONTAINER (parent_widget));

  index = g_list_index (children, widget);
  g_list_free (children);
  return index;  
}

static void 
atk_component_interface_init (AtkComponentIface *iface)
{
  /*
   * Use default implementation for contains and get_position
   */
  iface->add_focus_handler = gail_widget_add_focus_handler;
  iface->get_extents = gail_widget_get_extents;
  iface->get_size = gail_widget_get_size;
  iface->get_layer = gail_widget_get_layer;
  iface->grab_focus = gail_widget_grab_focus;
  iface->remove_focus_handler = gail_widget_remove_focus_handler;
  iface->set_extents = gail_widget_set_extents;
  iface->set_position = gail_widget_set_position;
  iface->set_size = gail_widget_set_size;
}

static guint 
gail_widget_add_focus_handler (AtkComponent    *component,
                               AtkFocusHandler handler)
{
  GSignalMatchType match_type;
  gulong ret;
  guint signal_id;

  match_type = G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_FUNC;
  signal_id = g_signal_lookup ("focus-event", ATK_TYPE_OBJECT);

  ret = g_signal_handler_find (component, match_type, signal_id, 0, NULL,
                               (gpointer) handler, NULL);
  if (!ret)
    {
      return g_signal_connect_closure_by_id (component, 
                                             signal_id, 0,
                                             g_cclosure_new (
                                             G_CALLBACK (handler), NULL,
                                             (GClosureNotify) NULL),
                                             FALSE);
    }
  else
    {
      return 0;
    }
}

static void 
gail_widget_get_extents (AtkComponent   *component,
                         gint           *x,
                         gint           *y,
                         gint           *width,
                         gint           *height,
                         AtkCoordType   coord_type)
{
  GdkWindow *window;
  gint x_window, y_window;
  gint x_toplevel, y_toplevel;
  GtkWidget *widget = GTK_ACCESSIBLE (component)->widget;

  if (widget == NULL)
    /*
     * Object is defunct
     */
    return;

  gail_return_if_fail (GTK_IS_WIDGET (widget));

  *width = widget->allocation.width;
  *height = widget->allocation.height;
  if (!gail_widget_on_screen (widget) || (!gtk_widget_is_drawable (widget)))
    {
      *x = G_MININT;
      *y = G_MININT;
      return;
    }

  if (widget->parent)
    {
      *x = widget->allocation.x;
      *y = widget->allocation.y;
      window = gtk_widget_get_parent_window (widget);
    }
  else
    {
      *x = 0;
      *y = 0;
      window = widget->window;
    }
  gdk_window_get_origin (window, &x_window, &y_window);
  *x += x_window;
  *y += y_window;

 
 if (coord_type == ATK_XY_WINDOW) 
    { 
      window = gdk_window_get_toplevel (widget->window);
      gdk_window_get_origin (window, &x_toplevel, &y_toplevel);

      *x -= x_toplevel;
      *y -= y_toplevel;
    }
}

static void 
gail_widget_get_size (AtkComponent   *component,
                      gint           *width,
                      gint           *height)
{
  GtkWidget *widget = GTK_ACCESSIBLE (component)->widget;

  if (widget == NULL)
    /*
     * Object is defunct
     */
    return;

  gail_return_if_fail (GTK_IS_WIDGET (widget));

  *width = widget->allocation.width;
  *height = widget->allocation.height;
}

static AtkLayer
gail_widget_get_layer (AtkComponent *component)
{
  gint layer;
  layer = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (component), "atk-component-layer"));

  return (AtkLayer) layer;
}

static gboolean 
gail_widget_grab_focus (AtkComponent   *component)
{
  GtkWidget *widget = GTK_ACCESSIBLE (component)->widget;
  GtkWidget *toplevel;

  gail_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  if (gtk_widget_get_can_focus (widget))
    {
      gtk_widget_grab_focus (widget);
      toplevel = gtk_widget_get_toplevel (widget);
      if (gtk_widget_is_toplevel (toplevel))
	{
#ifdef GDK_WINDOWING_X11
	  gtk_window_present_with_time (GTK_WINDOW (toplevel), gdk_x11_get_server_time (widget->window));
#else
	  gtk_window_present (GTK_WINDOW (toplevel));
#endif
	}
      return TRUE;
    }
  else
    return FALSE;
}

static void 
gail_widget_remove_focus_handler (AtkComponent   *component,
                                  guint          handler_id)
{
  g_signal_handler_disconnect (component, handler_id);
}

static gboolean 
gail_widget_set_extents (AtkComponent   *component,
                         gint           x,
                         gint           y,
                         gint           width,
                         gint           height,
                         AtkCoordType   coord_type)
{
  GtkWidget *widget = GTK_ACCESSIBLE (component)->widget;

  if (widget == NULL)
    /*
     * Object is defunct
     */
    return FALSE;
  gail_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  if (gtk_widget_is_toplevel (widget))
    {
      if (coord_type == ATK_XY_WINDOW)
        {
          gint x_current, y_current;
          GdkWindow *window = widget->window;

          gdk_window_get_origin (window, &x_current, &y_current);
          x_current += x;
          y_current += y;
          if (x_current < 0 || y_current < 0)
            return FALSE;
          else
            {
              gtk_widget_set_uposition (widget, x_current, y_current);
              gtk_widget_set_size_request (widget, width, height);
              return TRUE;
            }
        }
      else if (coord_type == ATK_XY_SCREEN)
        {  
          gtk_widget_set_uposition (widget, x, y);
          gtk_widget_set_size_request (widget, width, height);
          return TRUE;
        }
    }
  return FALSE;
}

static gboolean
gail_widget_set_position (AtkComponent   *component,
                          gint           x,
                          gint           y,
                          AtkCoordType   coord_type)
{
  GtkWidget *widget = GTK_ACCESSIBLE (component)->widget;

  if (widget == NULL)
    /*
     * Object is defunct
     */
    return FALSE;
  gail_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  if (gtk_widget_is_toplevel (widget))
    {
      if (coord_type == ATK_XY_WINDOW)
        {
          gint x_current, y_current;
          GdkWindow *window = widget->window;

          gdk_window_get_origin (window, &x_current, &y_current);
          x_current += x;
          y_current += y;
          if (x_current < 0 || y_current < 0)
            return FALSE;
          else
            {
              gtk_widget_set_uposition (widget, x_current, y_current);
              return TRUE;
            }
        }
      else if (coord_type == ATK_XY_SCREEN)
        {  
          gtk_widget_set_uposition (widget, x, y);
          return TRUE;
        }
    }
  return FALSE;
}

static gboolean 
gail_widget_set_size (AtkComponent   *component,
                      gint           width,
                      gint           height)
{
  GtkWidget *widget = GTK_ACCESSIBLE (component)->widget;

  if (widget == NULL)
    /*
     * Object is defunct
     */
    return FALSE;
  gail_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  if (gtk_widget_is_toplevel (widget))
    {
      gtk_widget_set_size_request (widget, width, height);
      return TRUE;
    }
  else
   return FALSE;
}

/*
 * This function is a signal handler for notify_in_event and focus_out_event
 * signal which gets emitted on a GtkWidget.
 */
static gboolean
gail_widget_focus_gtk (GtkWidget     *widget,
                       GdkEventFocus *event)
{
  GailWidget *gail_widget;
  GailWidgetClass *klass;

  gail_widget = GAIL_WIDGET (gtk_widget_get_accessible (widget));
  klass = GAIL_WIDGET_GET_CLASS (gail_widget);
  if (klass->focus_gtk)
    return klass->focus_gtk (widget, event);
  else
    return FALSE;
}

/*
 * This function is the signal handler defined for focus_in_event and
 * focus_out_event got GailWidget.
 *
 * It emits a focus-event signal on the GailWidget.
 */
static gboolean
gail_widget_real_focus_gtk (GtkWidget     *widget,
                            GdkEventFocus *event)
{
  AtkObject* accessible;
  gboolean return_val;
  return_val = FALSE;

  accessible = gtk_widget_get_accessible (widget);
  g_signal_emit_by_name (accessible, "focus_event", event->in, &return_val);
  return FALSE;
}

static void
gail_widget_size_allocate_gtk (GtkWidget     *widget,
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
      g_signal_emit_by_name (accessible, "bounds_changed", &rect);
    }
}

/*
 * This function is the signal handler defined for map and unmap signals.
 */
static gint
gail_widget_map_gtk (GtkWidget     *widget)
{
  AtkObject* accessible;

  accessible = gtk_widget_get_accessible (widget);
  atk_object_notify_state_change (accessible, ATK_STATE_SHOWING,
                                  gtk_widget_get_mapped (widget));
  return 1;
}

/*
 * This function is a signal handler for notify signal which gets emitted 
 * when a property changes value on the GtkWidget associated with the object.
 *
 * It calls a function for the GailWidget type
 */
static void 
gail_widget_notify_gtk (GObject     *obj,
                        GParamSpec  *pspec)
{
  GailWidget *widget;
  GailWidgetClass *klass;

  widget = GAIL_WIDGET (gtk_widget_get_accessible (GTK_WIDGET (obj)));
  klass = GAIL_WIDGET_GET_CLASS (widget);
  if (klass->notify_gtk)
    klass->notify_gtk (obj, pspec);
}

/*
 * This function is a signal handler for notify signal which gets emitted 
 * when a property changes value on the GtkWidget associated with a GailWidget.
 *
 * It constructs an AtkPropertyValues structure and emits a "property_changed"
 * signal which causes the user specified AtkPropertyChangeHandler
 * to be called.
 */
static void 
gail_widget_real_notify_gtk (GObject     *obj,
                             GParamSpec  *pspec)
{
  GtkWidget* widget = GTK_WIDGET (obj);
  AtkObject* atk_obj = gtk_widget_get_accessible (widget);
  AtkState state;
  gboolean value;

  if (strcmp (pspec->name, "has-focus") == 0)
    /*
     * We use focus-in-event and focus-out-event signals to catch
     * focus changes so we ignore this.
     */
    return;
  else if (strcmp (pspec->name, "visible") == 0)
    {
      state = ATK_STATE_VISIBLE;
      value = gtk_widget_get_visible (widget);
    }
  else if (strcmp (pspec->name, "sensitive") == 0)
    {
      state = ATK_STATE_SENSITIVE;
      value = gtk_widget_get_sensitive (widget);
    }
  else
    return;

  atk_object_notify_state_change (atk_obj, state, value);
  if (state == ATK_STATE_SENSITIVE)
    atk_object_notify_state_change (atk_obj, ATK_STATE_ENABLED, value);

}

static void 
gail_widget_focus_event (AtkObject   *obj,
                         gboolean    focus_in)
{
  AtkObject *focus_obj;

  focus_obj = g_object_get_data (G_OBJECT (obj), "gail-focus-object");
  if (focus_obj == NULL)
    focus_obj = obj;
  atk_object_notify_state_change (focus_obj, ATK_STATE_FOCUSED, focus_in);
}

static GtkWidget*
gail_widget_find_viewport (GtkWidget *widget)
{
  /*
   * Find an antecedent which is a GtkViewPort
   */
  GtkWidget *parent;

  parent = widget->parent;
  while (parent != NULL)
    {
      if (GTK_IS_VIEWPORT (parent))
        break;
      parent = parent->parent;
    }
  return parent;
}

/*
 * This function checks whether the widget has an antecedent which is 
 * a GtkViewport and, if so, whether any part of the widget intersects
 * the visible rectangle of the GtkViewport.
 */ 
static gboolean gail_widget_on_screen (GtkWidget *widget)
{
  GtkWidget *viewport;
  gboolean return_value;

  viewport = gail_widget_find_viewport (widget);
  if (viewport)
    {
      GtkAdjustment *adjustment;
      GdkRectangle visible_rect;

      adjustment = gtk_viewport_get_vadjustment (GTK_VIEWPORT (viewport));
      visible_rect.y = adjustment->value;
      adjustment = gtk_viewport_get_hadjustment (GTK_VIEWPORT (viewport));
      visible_rect.x = adjustment->value;
      visible_rect.width = viewport->allocation.width;
      visible_rect.height = viewport->allocation.height;
             
      if (((widget->allocation.x + widget->allocation.width) < visible_rect.x) ||
         ((widget->allocation.y + widget->allocation.height) < visible_rect.y) ||
         (widget->allocation.x > (visible_rect.x + visible_rect.width)) ||
         (widget->allocation.y > (visible_rect.y + visible_rect.height)))
        return_value = FALSE;
      else
        return_value = TRUE;
    }
  else
    {
      /*
       * Check whether the widget has been placed of the screen. The
       * widget may be MAPPED as when toolbar items do not fit on the toolbar.
       */
      if (widget->allocation.x + widget->allocation.width <= 0 &&
          widget->allocation.y + widget->allocation.height <= 0)
        return_value = FALSE;
      else 
        return_value = TRUE;
    }

  return return_value;
}

/**
 * gail_widget_all_parents_visible:
 * @widget: a #GtkWidget
 *
 * Checks if all the predecesors (the parent widget, his parent, etc) are visible
 * Used to check properly the SHOWING state.
 *
 * Return value: TRUE if all the parent hierarchy is visible, FALSE otherwise
 **/
static gboolean gail_widget_all_parents_visible (GtkWidget *widget)
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
