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

#include <stdlib.h>
#include <string.h>

#undef GTK_DISABLE_DEPRECATED

#include <gtk/gtk.h>

#include "gailtoplevel.h"

static void             gail_toplevel_class_init        (GailToplevelClass      *klass);
static void             gail_toplevel_init              (GailToplevel           *toplevel);
static void             gail_toplevel_initialize        (AtkObject              *accessible,
                                                         gpointer                data);
static void             gail_toplevel_object_finalize   (GObject                *obj);

/* atkobject.h */

static gint             gail_toplevel_get_n_children    (AtkObject              *obj);
static AtkObject*       gail_toplevel_ref_child         (AtkObject              *obj,
                                                        gint                    i);
static AtkObject*       gail_toplevel_get_parent        (AtkObject              *obj);

/* Callbacks */


static void             gail_toplevel_window_destroyed  (GtkWindow              *window,
                                                        GailToplevel            *text);
static gboolean         gail_toplevel_hide_event_watcher (GSignalInvocationHint *ihint,
                                                        guint                   n_param_values,
                                                        const GValue            *param_values,
                                                        gpointer                data);
static gboolean         gail_toplevel_show_event_watcher (GSignalInvocationHint *ihint,
                                                        guint                   n_param_values,
                                                        const GValue            *param_values,
                                                        gpointer                data);

/* Misc */

static void      _gail_toplevel_remove_child            (GailToplevel           *toplevel,
                                                        GtkWindow               *window);
static gboolean  is_attached_menu_window                (GtkWidget              *widget);
static gboolean  is_combo_window                        (GtkWidget              *widget);


G_DEFINE_TYPE (GailToplevel, gail_toplevel, ATK_TYPE_OBJECT)

static void
gail_toplevel_class_init (GailToplevelClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS(klass);
  GObjectClass *g_object_class = G_OBJECT_CLASS(klass);

  class->initialize = gail_toplevel_initialize;
  class->get_n_children = gail_toplevel_get_n_children;
  class->ref_child = gail_toplevel_ref_child;
  class->get_parent = gail_toplevel_get_parent;

  g_object_class->finalize = gail_toplevel_object_finalize;
}

static void
gail_toplevel_init (GailToplevel *toplevel)
{
  GtkWindow *window;
  GtkWidget *widget;
  GList *l;
  guint signal_id;
  
  l = toplevel->window_list = gtk_window_list_toplevels ();

  while (l)
    {
      window = GTK_WINDOW (l->data);
      widget = GTK_WIDGET (window);
      if (!window || 
          !gtk_widget_get_visible (widget) ||
          is_attached_menu_window (widget) ||
          GTK_WIDGET (window)->parent ||
          GTK_IS_PLUG (window))
        {
          GList *temp_l  = l->next;

          toplevel->window_list = g_list_delete_link (toplevel->window_list, l);
          l = temp_l;
        }
      else
        {
          g_signal_connect (G_OBJECT (window), 
                            "destroy",
                            G_CALLBACK (gail_toplevel_window_destroyed),
                            toplevel);
          l = l->next;
        }
    }

  g_type_class_ref (GTK_TYPE_WINDOW);

  signal_id  = g_signal_lookup ("show", GTK_TYPE_WINDOW);
  g_signal_add_emission_hook (signal_id, 0,
    gail_toplevel_show_event_watcher, toplevel, (GDestroyNotify) NULL);

  signal_id  = g_signal_lookup ("hide", GTK_TYPE_WINDOW);
  g_signal_add_emission_hook (signal_id, 0,
    gail_toplevel_hide_event_watcher, toplevel, (GDestroyNotify) NULL);
}

static void
gail_toplevel_initialize (AtkObject *accessible,
                          gpointer  data)
{
  ATK_OBJECT_CLASS (gail_toplevel_parent_class)->initialize (accessible, data);

  accessible->role = ATK_ROLE_APPLICATION;
  accessible->name = g_get_prgname();
  accessible->accessible_parent = NULL;
}

static void
gail_toplevel_object_finalize (GObject *obj)
{
  GailToplevel *toplevel = GAIL_TOPLEVEL (obj);

  if (toplevel->window_list)
    g_list_free (toplevel->window_list);

  G_OBJECT_CLASS (gail_toplevel_parent_class)->finalize (obj);
}

static AtkObject*
gail_toplevel_get_parent (AtkObject *obj)
{
    return NULL;
}

static gint
gail_toplevel_get_n_children (AtkObject *obj)
{
  GailToplevel *toplevel = GAIL_TOPLEVEL (obj);

  gint rc = g_list_length (toplevel->window_list);
  return rc;
}

static AtkObject*
gail_toplevel_ref_child (AtkObject *obj,
                         gint      i)
{
  GailToplevel *toplevel;
  gpointer ptr;
  GtkWidget *widget;
  AtkObject *atk_obj;

  toplevel = GAIL_TOPLEVEL (obj);
  ptr = g_list_nth_data (toplevel->window_list, i);
  if (!ptr)
    return NULL;
  widget = GTK_WIDGET (ptr);
  atk_obj = gtk_widget_get_accessible (widget);

  g_object_ref (atk_obj);
  return atk_obj;
}

/*
 * Window destroy events on GtkWindow cause a child to be removed
 * from the toplevel
 */
static void
gail_toplevel_window_destroyed (GtkWindow    *window,
                                GailToplevel *toplevel)
{
  _gail_toplevel_remove_child (toplevel, window);
}

/*
 * Show events cause a child to be added to the toplevel
 */
static gboolean
gail_toplevel_show_event_watcher (GSignalInvocationHint *ihint,
                                  guint                  n_param_values,
                                  const GValue          *param_values,
                                  gpointer               data)
{
  GailToplevel *toplevel = GAIL_TOPLEVEL (data);
  AtkObject *atk_obj = ATK_OBJECT (toplevel);
  GObject *object;
  GtkWidget *widget;
  gint n_children;
  AtkObject *child;

  object = g_value_get_object (param_values + 0);

  if (!GTK_IS_WINDOW (object))
    return TRUE;

  widget = GTK_WIDGET (object);
  if (widget->parent || 
      is_attached_menu_window (widget) ||
      is_combo_window (widget) ||
      GTK_IS_PLUG (widget))
    return TRUE;

  child = gtk_widget_get_accessible (widget);
  if (atk_object_get_role (child) == ATK_ROLE_REDUNDANT_OBJECT)
    {
      return TRUE;
    }

  /* 
   * Add the window to the list & emit the signal.
   * Don't do this for tooltips (Bug #150649).
   */
  if (atk_object_get_role (child) == ATK_ROLE_TOOL_TIP)
    {
      return TRUE;
    }

  toplevel->window_list = g_list_append (toplevel->window_list, widget);

  n_children = g_list_length (toplevel->window_list);

  /*
   * Must subtract 1 from the n_children since the index is 0-based
   * but g_list_length is 1-based.
   */
  atk_object_set_parent (child, atk_obj);
  g_signal_emit_by_name (atk_obj, "children-changed::add",
                         n_children - 1, 
                         child, NULL);

  /* Connect destroy signal callback */
  g_signal_connect (G_OBJECT(object), 
                    "destroy",
                    G_CALLBACK (gail_toplevel_window_destroyed),
                    toplevel);

  return TRUE;
}

/*
 * Hide events on GtkWindow cause a child to be removed from the toplevel
 */
static gboolean
gail_toplevel_hide_event_watcher (GSignalInvocationHint *ihint,
                                  guint                  n_param_values,
                                  const GValue          *param_values,
                                  gpointer               data)
{
  GailToplevel *toplevel = GAIL_TOPLEVEL (data);
  GObject *object;

  object = g_value_get_object (param_values + 0);

  if (!GTK_IS_WINDOW (object))
    return TRUE;

  _gail_toplevel_remove_child (toplevel, GTK_WINDOW (object));
  return TRUE;
}

/*
 * Common code used by destroy and hide events on GtkWindow
 */
static void
_gail_toplevel_remove_child (GailToplevel *toplevel, 
                             GtkWindow    *window)
{
  AtkObject *atk_obj = ATK_OBJECT (toplevel);
  GList *l;
  guint window_count = 0;
  AtkObject *child;

  if (toplevel->window_list)
    {
        GtkWindow *tmp_window;

        /* Must loop through them all */
        for (l = toplevel->window_list; l; l = l->next)
        {
          tmp_window = GTK_WINDOW (l->data);

          if (window == tmp_window)
            {
              /* Remove the window from the window_list & emit the signal */
              toplevel->window_list = g_list_remove (toplevel->window_list,
                                                     l->data);
              child = gtk_widget_get_accessible (GTK_WIDGET (window));
              g_signal_emit_by_name (atk_obj, "children-changed::remove",
                                     window_count, 
                                     child, NULL);
              atk_object_set_parent (child, NULL);
              break;
            }

          window_count++;
        }
    }
}

static gboolean
is_attached_menu_window (GtkWidget *widget)
{
  GtkWidget *child = GTK_BIN (widget)->child;
  gboolean ret = FALSE;

  if (GTK_IS_MENU (child))
    {
      GtkWidget *attach;

      attach = gtk_menu_get_attach_widget (GTK_MENU (child));
      /* Allow for menu belonging to the Panel Menu, which is a GtkButton */
      if (GTK_IS_MENU_ITEM (attach) ||
          GTK_IS_OPTION_MENU (attach) ||
          GTK_IS_BUTTON (attach))
        ret = TRUE;
    }
  return ret;
}

static gboolean
is_combo_window (GtkWidget *widget)
{
  GtkWidget *child = GTK_BIN (widget)->child;
  AtkObject *obj;
  GtkAccessible *accessible;

  if (!GTK_IS_EVENT_BOX (child))
    return FALSE;

  child = GTK_BIN (child)->child;

  if (!GTK_IS_FRAME (child))
    return FALSE;

  child = GTK_BIN (child)->child;

  if (!GTK_IS_SCROLLED_WINDOW (child))
    return FALSE;

  obj = gtk_widget_get_accessible (child);
  obj = atk_object_get_parent (obj);
  accessible = GTK_ACCESSIBLE (obj);
  if (GTK_IS_COMBO (accessible->widget))
    return TRUE;

  return  FALSE;
}
