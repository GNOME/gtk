/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2003 Sun Microsystems Inc.
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
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include "gailexpander.h"

static void                  gail_expander_class_init       (GailExpanderClass *klass);
static void                  gail_expander_init             (GailExpander      *expander);

static const gchar* gail_expander_get_name         (AtkObject         *obj);
static gint                  gail_expander_get_n_children   (AtkObject         *obj)
;
static AtkObject*            gail_expander_ref_child        (AtkObject         *obj,
                                                             gint              i);

static AtkStateSet*          gail_expander_ref_state_set    (AtkObject         *obj);
static void                  gail_expander_real_notify_gtk  (GObject           *obj,
                                                             GParamSpec        *pspec);
static void                  gail_expander_map_gtk          (GtkWidget         *widget,
                                                             gpointer          data);

static void                  gail_expander_real_initialize  (AtkObject         *obj,
                                                             gpointer          data);
static void                  gail_expander_finalize         (GObject           *object);
static void                  gail_expander_init_textutil    (GailExpander      *expander,
                                                             GtkExpander       *widget);
static const gchar* gail_expander_get_full_text    (GtkExpander       *widget);

static void                  atk_action_interface_init  (AtkActionIface *iface);
static gboolean              gail_expander_do_action    (AtkAction      *action,
                                                         gint           i);
static gboolean              idle_do_action             (gpointer       data);
static gint                  gail_expander_get_n_actions(AtkAction      *action);
static const gchar* gail_expander_get_keybinding
                                                        (AtkAction      *action,
                                                         gint           i);
static const gchar* gail_expander_action_get_name
                                                        (AtkAction      *action,
                                                         gint           i);

G_DEFINE_TYPE_WITH_CODE (GailExpander, gail_expander, GAIL_TYPE_CONTAINER,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, atk_action_interface_init))

static void
gail_expander_class_init (GailExpanderClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);
  GailWidgetClass *widget_class;

  widget_class = (GailWidgetClass*)klass;
  widget_class->notify_gtk = gail_expander_real_notify_gtk;

  gobject_class->finalize = gail_expander_finalize;

  class->get_name = gail_expander_get_name;
  class->get_n_children = gail_expander_get_n_children;
  class->ref_child = gail_expander_ref_child;
  class->ref_state_set = gail_expander_ref_state_set;

  class->initialize = gail_expander_real_initialize;
}

static void
gail_expander_init (GailExpander *expander)
{
  expander->activate_keybinding = NULL;
  expander->action_idle_handler = 0;
  expander->textutil = NULL;
}

static const gchar*
gail_expander_get_name (AtkObject *obj)
{
  const gchar *name;
  g_return_val_if_fail (GAIL_IS_EXPANDER (obj), NULL);

  name = ATK_OBJECT_CLASS (gail_expander_parent_class)->get_name (obj);
  if (name != NULL)
    return name;
  else
    {
      /*
       * Get the text on the label
       */
      GtkWidget *widget;

      widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
      if (widget == NULL)
        /*
         * State is defunct
         */
        return NULL;

      g_return_val_if_fail (GTK_IS_EXPANDER (widget), NULL);

      return gail_expander_get_full_text (GTK_EXPANDER (widget));
    }
}

static gint
gail_expander_get_n_children (AtkObject* obj)
{
  GtkWidget *widget;
  GList *children;
  gint count = 0;

  g_return_val_if_fail (GAIL_IS_CONTAINER (obj), count);

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return 0;

  children = gtk_container_get_children (GTK_CONTAINER(widget));
  count = g_list_length (children);
  g_list_free (children);

  /* See if there is a label - if there is, reduce our count by 1
   * since we don't want the label included with the children.
   */
  if (gtk_expander_get_label_widget (GTK_EXPANDER (widget)))
    count -= 1;

  return count; 
}

static AtkObject*
gail_expander_ref_child (AtkObject *obj,
                         gint      i)
{
  GList *children, *tmp_list;
  AtkObject *accessible;
  GtkWidget *widget;
  GtkWidget *label;
  gint index;

  g_return_val_if_fail (GAIL_IS_CONTAINER (obj), NULL);
  g_return_val_if_fail ((i >= 0), NULL);

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return NULL;

  children = gtk_container_get_children (GTK_CONTAINER (widget));

  /* See if there is a label - if there is, we need to skip it
   * since we don't want the label included with the children.
   */
  label = gtk_expander_get_label_widget (GTK_EXPANDER (widget));
  if (label) {
    for (index = 0; index <= i; index++) {
      tmp_list = g_list_nth (children, index);
      if (label == GTK_WIDGET (tmp_list->data)) {
	i += 1;
	break;
      }
    }
  }

  tmp_list = g_list_nth (children, i);
  if (!tmp_list)
    {
      g_list_free (children);
      return NULL;
    }  
  accessible = gtk_widget_get_accessible (GTK_WIDGET (tmp_list->data));

  g_list_free (children);
  g_object_ref (accessible);
  return accessible; 
}

static void
gail_expander_real_initialize (AtkObject *obj,
                               gpointer   data)
{
  GailExpander *gail_expander = GAIL_EXPANDER (obj);
  GtkWidget  *expander;

  ATK_OBJECT_CLASS (gail_expander_parent_class)->initialize (obj, data);

  expander = GTK_WIDGET (data);
  if (gtk_widget_get_mapped (expander))
    gail_expander_init_textutil (gail_expander, GTK_EXPANDER (expander));
  else 
    g_signal_connect (expander,
                      "map",
                      G_CALLBACK (gail_expander_map_gtk),
                      gail_expander);
 
  obj->role = ATK_ROLE_TOGGLE_BUTTON;
}

static void
gail_expander_map_gtk (GtkWidget *widget,
                       gpointer data)
{
  GailExpander *expander; 

  expander = GAIL_EXPANDER (data);
  gail_expander_init_textutil (expander, GTK_EXPANDER (widget));
}

static void
gail_expander_real_notify_gtk (GObject    *obj,
                               GParamSpec *pspec)
{
  AtkObject* atk_obj;
  GtkExpander *expander;
  GailExpander *gail_expander;

  expander = GTK_EXPANDER (obj);
  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (expander));
;
  if (strcmp (pspec->name, "label") == 0)
    {
      const gchar* label_text;


      label_text = gail_expander_get_full_text (expander);

      gail_expander = GAIL_EXPANDER (atk_obj);
      if (gail_expander->textutil)
        gail_text_util_text_setup (gail_expander->textutil, label_text);

      if (atk_obj->name == NULL)
      {
        /*
         * The label has changed so notify a change in accessible-name
         */
        g_object_notify (G_OBJECT (atk_obj), "accessible-name");
      }
      /*
       * The label is the only property which can be changed
       */
      g_signal_emit_by_name (atk_obj, "visible_data_changed");
    }
  else if (strcmp (pspec->name, "expanded") == 0)
    {
      atk_object_notify_state_change (atk_obj, ATK_STATE_CHECKED, 
                                      gtk_expander_get_expanded (expander));
      atk_object_notify_state_change (atk_obj, ATK_STATE_EXPANDED, 
                                      gtk_expander_get_expanded (expander));
      g_signal_emit_by_name (atk_obj, "visible_data_changed");
    }
  else
    GAIL_WIDGET_CLASS (gail_expander_parent_class)->notify_gtk (obj, pspec);
}

static const gchar*
gail_expander_get_full_text (GtkExpander *widget)
{
  GtkWidget *label_widget;

  label_widget = gtk_expander_get_label_widget (widget);

  if (!GTK_IS_LABEL (label_widget))
    return NULL;

  return gtk_label_get_text (GTK_LABEL (label_widget));
}

static void
gail_expander_init_textutil (GailExpander *expander,
                             GtkExpander  *widget)
{
  const gchar *label_text;

  expander->textutil = gail_text_util_new ();
  label_text = gail_expander_get_full_text (widget);
  gail_text_util_text_setup (expander->textutil, label_text);
}

static void
atk_action_interface_init (AtkActionIface *iface)
{
  iface->do_action = gail_expander_do_action;
  iface->get_n_actions = gail_expander_get_n_actions;
  iface->get_keybinding = gail_expander_get_keybinding;
  iface->get_name = gail_expander_action_get_name;
}

static gboolean
gail_expander_do_action (AtkAction *action,
                         gint      i)
{
  GtkWidget *widget;
  GailExpander *expander;
  gboolean return_value = TRUE;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (action));
  if (widget == NULL)
    /*
     * State is defunct
     */
    return FALSE;

  if (!gtk_widget_is_sensitive (widget) || !gtk_widget_get_visible (widget))
    return FALSE;

  expander = GAIL_EXPANDER (action);
  switch (i)
    {
    case 0:
      if (expander->action_idle_handler)
        return_value = FALSE;
      else
	expander->action_idle_handler = gdk_threads_add_idle (idle_do_action, expander);
      break;
    default:
      return_value = FALSE;
      break;
    }
  return return_value; 
}

static gboolean
idle_do_action (gpointer data)
{
  GtkWidget *widget;
  GailExpander *gail_expander;

  gail_expander = GAIL_EXPANDER (data);
  gail_expander->action_idle_handler = 0;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (gail_expander));
  if (widget == NULL /* State is defunct */ ||
      !gtk_widget_is_sensitive (widget) || !gtk_widget_get_visible (widget))
    return FALSE;

  gtk_widget_activate (widget);

  return FALSE;
}

static gint
gail_expander_get_n_actions (AtkAction *action)
{
  return 1;
}

static const gchar*
gail_expander_get_keybinding (AtkAction *action,
                              gint      i)
{
  GailExpander *expander;
  gchar *return_value = NULL;

  switch (i)
    {
    case 0:
      {
        /*
         * We look for a mnemonic on the label
         */
        GtkWidget *widget;
        GtkWidget *label;

        expander = GAIL_EXPANDER (action);
        widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (expander));
        if (widget == NULL)
          /*
           * State is defunct
           */
          return NULL;

        g_return_val_if_fail (GTK_IS_EXPANDER (widget), NULL);

        label = gtk_expander_get_label_widget (GTK_EXPANDER (widget));
        if (GTK_IS_LABEL (label))
          {
            guint key_val; 

            key_val = gtk_label_get_mnemonic_keyval (GTK_LABEL (label)); 
            if (key_val != GDK_KEY_VoidSymbol)
              return_value = gtk_accelerator_name (key_val, GDK_MOD1_MASK);
            g_free (expander->activate_keybinding);
            expander->activate_keybinding = return_value;
          }
        break;
      }
    default:
      break;
    }
  return return_value; 
}

static const gchar*
gail_expander_action_get_name (AtkAction *action,
                               gint      i)
{
  const gchar *return_value;

  switch (i)
    {
    case 0:
      return_value = "activate";
      break;
    default:
      return_value = NULL;
      break;
    }
  return return_value; 
}

static AtkStateSet*
gail_expander_ref_state_set (AtkObject *obj)
{
  AtkStateSet *state_set;
  GtkWidget *widget;
  GtkExpander *expander;

  state_set = ATK_OBJECT_CLASS (gail_expander_parent_class)->ref_state_set (obj);
  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));

  if (widget == NULL)
    return state_set;

  expander = GTK_EXPANDER (widget);

  atk_state_set_add_state (state_set, ATK_STATE_EXPANDABLE);

  if (gtk_expander_get_expanded (expander)) {
    atk_state_set_add_state (state_set, ATK_STATE_CHECKED);
    atk_state_set_add_state (state_set, ATK_STATE_EXPANDED);
  }

  return state_set;
}

static void
gail_expander_finalize (GObject *object)
{
  GailExpander *expander = GAIL_EXPANDER (object);

  g_free (expander->activate_keybinding);
  if (expander->action_idle_handler)
    {
      g_source_remove (expander->action_idle_handler);
      expander->action_idle_handler = 0;
    }
  if (expander->textutil)
    g_object_unref (expander->textutil);

  G_OBJECT_CLASS (gail_expander_parent_class)->finalize (object);
}
