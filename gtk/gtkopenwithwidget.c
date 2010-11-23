/*
 * gtkopenwithwidget.c: an open-with widget
 *
 * Copyright (C) 2004 Novell, Inc.
 * Copyright (C) 2007, 2010 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Dave Camp <dave@novell.com>
 *          Alexander Larsson <alexl@redhat.com>
 *          Cosimo Cecchi <ccecchi@redhat.com>
 */

#include <config.h>

#include "gtkopenwithwidget.h"

#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtkopenwith.h"
#include "gtkopenwithprivate.h"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

struct _GtkOpenWithWidgetPrivate {
  GAppInfo *selected_app_info;

  gchar *content_type;
  gchar *default_text;
  gboolean show_recommended;
  gboolean show_fallback;
  gboolean show_other;
  gboolean show_all;
  gboolean radio_mode;

  GtkWidget *program_list;
  GtkListStore *program_list_store;

  GtkCellRenderer *padding_renderer;
};

enum {
  COLUMN_APP_INFO,
  COLUMN_GICON,
  COLUMN_NAME,
  COLUMN_DESC,
  COLUMN_EXEC,
  COLUMN_HEADING,
  COLUMN_HEADING_TEXT,
  COLUMN_RECOMMENDED,
  COLUMN_FALLBACK,
  NUM_COLUMNS
};


enum {
  PROP_CONTENT_TYPE = 1,
  PROP_GFILE,
  PROP_SHOW_RECOMMENDED,
  PROP_SHOW_FALLBACK,
  PROP_SHOW_OTHER,
  PROP_SHOW_ALL,
  PROP_RADIO_MODE,
  PROP_DEFAULT_TEXT,
  N_PROPERTIES
};

enum {
  SIGNAL_APPLICATION_SELECTED,
  SIGNAL_APPLICATION_ACTIVATED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };

static void gtk_open_with_widget_iface_init (GtkOpenWithIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkOpenWithWidget, gtk_open_with_widget, GTK_TYPE_BOX,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_OPEN_WITH,
						gtk_open_with_widget_iface_init));

static void
refresh_and_emit_app_selected (GtkOpenWithWidget *self,
			       GtkTreeSelection *selection)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  GAppInfo *info = NULL;
  gboolean should_emit = FALSE;

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gtk_tree_model_get (model, &iter,
			  COLUMN_APP_INFO, &info,
			  -1);
    }

  if (info == NULL)
    return;

  if (self->priv->selected_app_info)
    {
      if (!g_app_info_equal (self->priv->selected_app_info, info))
	{
	  should_emit = TRUE;
	  g_object_unref (self->priv->selected_app_info);

	  self->priv->selected_app_info = info;
	}
    }
  else
    {
      should_emit = TRUE;
      self->priv->selected_app_info = info;
    }

  if (should_emit)
    g_signal_emit (self, signals[SIGNAL_APPLICATION_SELECTED], 0,
		   self->priv->selected_app_info);
}

static gboolean
path_is_heading (GtkTreeView *view,
		 GtkTreePath *path)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  gboolean res;

  model = gtk_tree_view_get_model (view);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter,
		      COLUMN_HEADING, &res,
		      -1);

  return res;
}

static void
program_list_selection_activated (GtkTreeView *view,
				  GtkTreePath *path,
				  GtkTreeViewColumn *column,
				  gpointer user_data)
{
  GtkOpenWithWidget *self = user_data;
  GtkTreeSelection *selection;

  if (path_is_heading (view, path))
    return;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->priv->program_list));

  refresh_and_emit_app_selected (self, selection);

  g_signal_emit (self, signals[SIGNAL_APPLICATION_ACTIVATED], 0,
		 self->priv->selected_app_info);
}

static void
item_forget_association_cb (GtkMenuItem *item,
			    gpointer user_data)
{
  GtkOpenWithWidget *self = user_data;
  GtkTreePath *path = NULL;
  GtkTreeIter iter;
  GtkTreeModel *model;
  GAppInfo *info;

  gtk_tree_view_get_cursor (GTK_TREE_VIEW (self->priv->program_list), &path, NULL);
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (self->priv->program_list));

  if (path != NULL)
    {
      gtk_tree_model_get_iter (model, &iter, path);
      gtk_tree_model_get (model, &iter,
			  COLUMN_APP_INFO, &info,
			  -1);

      if (info != NULL && g_app_info_can_remove_supports_type (info))
	g_app_info_remove_supports_type (info, self->priv->content_type, NULL);
    }

  gtk_open_with_refresh (GTK_OPEN_WITH (self));
}

static GtkWidget *
gtk_open_with_widget_build_popup_menu (GtkOpenWithWidget *self)
{
  GtkWidget *menu;
  GtkWidget *item;

  menu = gtk_menu_new ();

  item = gtk_menu_item_new_with_label (_("Forget association"));
  g_signal_connect (item, "activate",
		    G_CALLBACK (item_forget_association_cb), self);
  gtk_widget_show (item);

  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  return menu;
}

static gboolean
should_show_menu (GtkOpenWithWidget *self,
		  GdkEventButton *event)
{
  GtkTreeIter iter;
  GtkTreePath *path;
  GtkTreeModel *model;
  gboolean recommended, retval;
  GAppInfo *info;

  gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (self->priv->program_list),
				 event->x, event->y,
				 &path, NULL, NULL, NULL);

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (self->priv->program_list));
  gtk_tree_model_get_iter (model, &iter, path);

  gtk_tree_model_get (model, &iter,
		      COLUMN_RECOMMENDED, &recommended,
		      COLUMN_APP_INFO, &info,
		      -1);

  retval = recommended && (info != NULL);

  gtk_tree_path_free (path);

  if (info != NULL)
    g_object_unref (info);

  return retval;
}

static void
do_popup_menu (GtkOpenWithWidget *self,
	       GdkEventButton *event)
{
  GtkWidget *menu;

  if (!should_show_menu (self, event))
    return;

  menu = gtk_open_with_widget_build_popup_menu (self);
  gtk_menu_attach_to_widget (GTK_MENU (menu), self->priv->program_list, NULL);
  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
		  event->button, event->time);
}

static gboolean
program_list_button_press_event_cb (GtkWidget *treeview,
				    GdkEventButton *event,
				    gpointer user_data)
{
  GtkOpenWithWidget *self = user_data;

  if (event->button == 3 && event->type == GDK_BUTTON_PRESS)
    do_popup_menu (self, event);

  return FALSE;
}

static gboolean
gtk_open_with_search_equal_func (GtkTreeModel *model,
				 int column,
				 const char *key,
				 GtkTreeIter *iter,
				 gpointer user_data)
{
  char *normalized_key;
  char *name, *normalized_name;
  char *path, *normalized_path;
  char *basename, *normalized_basename;
  gboolean ret;

  if (key != NULL)
    {
      normalized_key = g_utf8_casefold (key, -1);
      g_assert (normalized_key != NULL);

      ret = TRUE;

      gtk_tree_model_get (model, iter,
			  COLUMN_NAME, &name,
			  COLUMN_EXEC, &path,
			  -1);

      if (name != NULL)
	{
	  normalized_name = g_utf8_casefold (name, -1);
	  g_assert (normalized_name != NULL);

	  if (strncmp (normalized_name, normalized_key, strlen (normalized_key)) == 0) {
	    ret = FALSE;
	  }

	  g_free (normalized_name);
	}

      if (ret && path != NULL)
	{
	  normalized_path = g_utf8_casefold (path, -1);
	  g_assert (normalized_path != NULL);

	  basename = g_path_get_basename (path);
	  g_assert (basename != NULL);

	  normalized_basename = g_utf8_casefold (basename, -1);
	  g_assert (normalized_basename != NULL);

	  if (strncmp (normalized_path, normalized_key, strlen (normalized_key)) == 0 ||
	      strncmp (normalized_basename, normalized_key, strlen (normalized_key)) == 0) {
	    ret = FALSE;
	  }

	  g_free (basename);
	  g_free (normalized_basename);
	  g_free (normalized_path);
	}

      g_free (name);
      g_free (path);
      g_free (normalized_key);

      return ret;
    }
  else
    {
      return TRUE;
    }
}

static gint
gtk_open_with_sort_func (GtkTreeModel *model,
			 GtkTreeIter *a,
			 GtkTreeIter *b,
			 gpointer user_data)
{
  gboolean a_recommended, b_recommended;
  gboolean a_fallback, b_fallback;
  gboolean a_heading, b_heading;
  gchar *a_name, *b_name, *a_casefold, *b_casefold;
  gint retval = 0;

  /* this returns:
   * - <0 if a should show before b
   * - =0 if a is the same as b
   * - >0 if a should show after b
   */

  gtk_tree_model_get (model, a,
		      COLUMN_NAME, &a_name,
		      COLUMN_RECOMMENDED, &a_recommended,
		      COLUMN_FALLBACK, &a_fallback,
		      COLUMN_HEADING, &a_heading,
		      -1);

  gtk_tree_model_get (model, b,
		      COLUMN_NAME, &b_name,
		      COLUMN_RECOMMENDED, &b_recommended,
		      COLUMN_FALLBACK, &b_fallback,
		      COLUMN_HEADING, &b_heading,
		      -1);

  /* the recommended one always wins */
  if (a_recommended && !b_recommended)
    {
      retval = -1;
      goto out;
    }

  if (b_recommended && !a_recommended)
    {
      retval = 1;
      goto out;
    }

  /* the recommended one always wins */
  if (a_fallback && !b_fallback)
    {
      retval = -1;
      goto out;
    }

  if (b_fallback && !a_fallback)
    {
      retval = 1;
      goto out;
    }

  /* they're both recommended/falback or not, so if one is a heading, wins */
  if (a_heading)
    {
      return -1;
      goto out;
    }

  if (b_heading)
    {
      return 1;
      goto out;
    }

  /* don't order by name recommended applications, but use GLib's ordering */
  if (!a_recommended)
    {
      a_casefold = a_name != NULL ?
	g_utf8_casefold (a_name, -1) : NULL;
      b_casefold = b_name != NULL ?
	g_utf8_casefold (b_name, -1) : NULL;

      retval = g_strcmp0 (a_casefold, b_casefold);

      g_free (a_casefold);
      g_free (b_casefold);
    }

 out:
  g_free (a_name);
  g_free (b_name);

  return retval;
}

static void
radio_cell_renderer_func (GtkTreeViewColumn *column,
			  GtkCellRenderer *cell,
			  GtkTreeModel *model,
			  GtkTreeIter *iter,
			  gpointer user_data)
{
  GtkOpenWithWidget *self = user_data;
  gboolean heading;

  gtk_tree_model_get (model, iter,
		      COLUMN_HEADING, &heading,
		      -1);

  g_object_set (cell,
		"visible", !heading && self->priv->radio_mode,
		NULL);
}

static void
padding_cell_renderer_func (GtkTreeViewColumn *column,
			    GtkCellRenderer *cell,
			    GtkTreeModel *model,
			    GtkTreeIter *iter,
			    gpointer user_data)
{
  gboolean heading;

  gtk_tree_model_get (model, iter,
		      COLUMN_HEADING, &heading,
		      -1);
  if (heading)
    g_object_set (cell,
		  "visible", FALSE,
		  "xpad", 0,
		  "ypad", 0,
		  NULL);
  else
    g_object_set (cell,
		  "visible", TRUE,
		  "xpad", 3,
		  "ypad", 3,
		  NULL);
}

static gboolean
gtk_open_with_selection_func (GtkTreeSelection *selection,
			      GtkTreeModel *model,
			      GtkTreePath *path,
			      gboolean path_currently_selected,
			      gpointer user_data)
{
  GtkTreeIter iter;
  gboolean heading;

  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter,
		      COLUMN_HEADING, &heading,
		      -1);

  return !heading;
}

static gint
compare_apps_func (gconstpointer a,
		   gconstpointer b)
{
  return !g_app_info_equal (G_APP_INFO (a), G_APP_INFO (b));
}

static gboolean
gtk_open_with_widget_add_section (GtkOpenWithWidget *self,
				  const gchar *heading_title,
				  gboolean show_headings,
				  gboolean recommended,
				  gboolean fallback,
				  GList *applications,
				  GList *exclude_apps)
{
  gboolean heading_added, unref_icon;
  GtkTreeIter iter;
  GAppInfo *app;
  gchar *app_string, *bold_string;
  GIcon *icon;
  GList *l;
  gboolean retval;

  retval = FALSE;
  heading_added = FALSE;
  bold_string = g_strdup_printf ("<b>%s</b>", heading_title);
  
  for (l = applications; l != NULL; l = l->next)
    {
      app = l->data;

      if (!g_app_info_supports_uris (app) &&
	  !g_app_info_supports_files (app))
	continue;

      if (exclude_apps != NULL &&
	  g_list_find_custom (exclude_apps, app,
			      (GCompareFunc) compare_apps_func))
	continue;

      if (!heading_added && show_headings)
	{
	  gtk_list_store_append (self->priv->program_list_store, &iter);
	  gtk_list_store_set (self->priv->program_list_store, &iter,
			      COLUMN_HEADING_TEXT, bold_string,
			      COLUMN_HEADING, TRUE,
			      COLUMN_RECOMMENDED, recommended,
			      COLUMN_FALLBACK, fallback,
			      -1);

	  heading_added = TRUE;
	}

      app_string = g_strdup_printf ("<b>%s</b>\n%s",
				    g_app_info_get_display_name (app) != NULL ?
				    g_app_info_get_display_name (app) : "",
				    g_app_info_get_description (app) != NULL ?
				    g_app_info_get_description (app) : "");

      icon = g_app_info_get_icon (app);
      if (icon == NULL)
	{
	  icon = g_themed_icon_new ("application-x-executable");
	  unref_icon = TRUE;
	}

      gtk_list_store_append (self->priv->program_list_store, &iter);
      gtk_list_store_set (self->priv->program_list_store, &iter,
			  COLUMN_APP_INFO, app,
			  COLUMN_GICON, icon,
			  COLUMN_NAME, g_app_info_get_display_name (app),
			  COLUMN_DESC, app_string,
			  COLUMN_EXEC, g_app_info_get_executable (app),
			  COLUMN_HEADING, FALSE,
			  COLUMN_RECOMMENDED, recommended,
			  COLUMN_FALLBACK, fallback,
			  -1);

      retval = TRUE;

      g_free (app_string);
      if (unref_icon)
	g_object_unref (icon);

      unref_icon = FALSE;
    }

  g_free (bold_string);

  return retval;
}

static void
add_no_applications_label (GtkOpenWithWidget *self)
{
  gchar *text = NULL, *desc;
  const gchar *string;
  GtkTreeIter iter;

  if (self->priv->default_text == NULL)
    {
      desc = g_content_type_get_description (self->priv->content_type);
      string = text = g_strdup_printf (_("No applications available to open \"%s\""),
				       desc);
      g_free (desc);
    }
  else
    {
      string = self->priv->default_text;
    }

  gtk_list_store_append (self->priv->program_list_store, &iter);
  gtk_list_store_set (self->priv->program_list_store, &iter,
		      COLUMN_HEADING_TEXT, string,
		      COLUMN_HEADING, TRUE,
		      COLUMN_RECOMMENDED, TRUE,
		      -1);

  g_free (text); 
}

static void
gtk_open_with_widget_real_add_items (GtkOpenWithWidget *self)
{
  GList *all_applications = NULL, *content_type_apps = NULL, *recommended_apps = NULL, *fallback_apps = NULL;
  gboolean show_headings;
  gboolean apps_added;

  show_headings = TRUE;
  apps_added = FALSE;

  if (self->priv->show_all)
    show_headings = FALSE;

  if (self->priv->show_recommended || self->priv->show_all)
    {
      recommended_apps = g_app_info_get_recommended_for_type (self->priv->content_type);

      apps_added |= gtk_open_with_widget_add_section (self, _("Recommended Applications"),
						      show_headings,
						      !self->priv->show_all, /* mark as recommended */
						      FALSE, /* mark as fallback */
						      recommended_apps, NULL);
    }

  if (self->priv->show_fallback || self->priv->show_all)
    {
      fallback_apps = g_app_info_get_fallback_for_type (self->priv->content_type);

      apps_added |= gtk_open_with_widget_add_section (self, _("Related Applications"),
						      show_headings,
						      FALSE, /* mark as recommended */
						      !self->priv->show_all, /* mark as fallback */
						      fallback_apps, recommended_apps);
    }

  if (self->priv->show_other || self->priv->show_all)
    {
      content_type_apps = g_list_concat (g_list_copy (recommended_apps),
					 g_list_copy (fallback_apps));
      all_applications = g_app_info_get_all ();

      apps_added |= gtk_open_with_widget_add_section (self, _("Other Applications"),
						      show_headings,
						      FALSE,
						      FALSE,
						      all_applications, content_type_apps);
    }

  if (!apps_added)
    add_no_applications_label (self);

  if (all_applications != NULL)
    g_list_free_full (all_applications, g_object_unref);

  if (recommended_apps != NULL)
    g_list_free_full (recommended_apps, g_object_unref);

  if (fallback_apps != NULL)
    g_list_free_full (fallback_apps, g_object_unref);

  if (content_type_apps != NULL)
    g_list_free (content_type_apps);
}

static void
gtk_open_with_widget_add_items (GtkOpenWithWidget *self)
{
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkTreeModel *sort;

  /* create list store */
  self->priv->program_list_store = gtk_list_store_new (NUM_COLUMNS,
						       G_TYPE_APP_INFO,
						       G_TYPE_ICON,
						       G_TYPE_STRING,
						       G_TYPE_STRING,
						       G_TYPE_STRING,
						       G_TYPE_BOOLEAN,
						       G_TYPE_STRING,
						       G_TYPE_BOOLEAN,
						       G_TYPE_BOOLEAN);
  sort = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (self->priv->program_list_store));

  /* populate the widget */
  gtk_open_with_widget_real_add_items (self);

  gtk_tree_view_set_model (GTK_TREE_VIEW (self->priv->program_list), 
			   GTK_TREE_MODEL (sort));
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sort),
					COLUMN_NAME,
					GTK_SORT_ASCENDING);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (sort),
				   COLUMN_NAME,
				   gtk_open_with_sort_func,
				   self, NULL);
  gtk_tree_view_set_search_column (GTK_TREE_VIEW (self->priv->program_list),
				   COLUMN_NAME);
  gtk_tree_view_set_search_equal_func (GTK_TREE_VIEW (self->priv->program_list),
  				       gtk_open_with_search_equal_func,
  				       NULL, NULL);

  column = gtk_tree_view_column_new ();

  /* initial padding */
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  g_object_set (renderer,
		"xpad", self->priv->show_all ? 0 : 6,
		NULL);
  self->priv->padding_renderer = renderer;

  /* heading text renderer */
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
				       "markup", COLUMN_HEADING_TEXT,
				       "visible", COLUMN_HEADING,
				       NULL);
  g_object_set (renderer,
		"ypad", 6,
		"xpad", 0,
		"wrap-width", 350,
		"wrap-mode", PANGO_WRAP_WORD,
		NULL);

  /* padding renderer for non-heading cells */
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_cell_data_func (column, renderer,
					   padding_cell_renderer_func,
					   NULL, NULL);

  /* radio renderer */
  renderer = gtk_cell_renderer_toggle_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_cell_data_func (column, renderer,
					   radio_cell_renderer_func,
					   self, NULL);
  g_object_set (renderer,
		"xpad", 6,
		"radio", TRUE,
		NULL);

  /* app icon renderer */
  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
				       "gicon", COLUMN_GICON,
				       NULL);
  g_object_set (renderer,
		"stock-size", GTK_ICON_SIZE_DIALOG,
		NULL);

  /* app name renderer */
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
				       "markup", COLUMN_DESC,
				       NULL);
  g_object_set (renderer,
		"ellipsize", PANGO_ELLIPSIZE_END,
		"ellipsize-set", TRUE,
		NULL);
  
  gtk_tree_view_column_set_sort_column_id (column, COLUMN_NAME);
  gtk_tree_view_append_column (GTK_TREE_VIEW (self->priv->program_list), column);
}

static void
gtk_open_with_widget_set_property (GObject *object,
				   guint property_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
  GtkOpenWithWidget *self = GTK_OPEN_WITH_WIDGET (object);

  switch (property_id)
    {
    case PROP_CONTENT_TYPE:
      self->priv->content_type = g_value_dup_string (value);
      break;
    case PROP_SHOW_RECOMMENDED:
      gtk_open_with_widget_set_show_recommended (self, g_value_get_boolean (value));
      break;
    case PROP_SHOW_FALLBACK:
      gtk_open_with_widget_set_show_fallback (self, g_value_get_boolean (value));
      break;
    case PROP_SHOW_OTHER:
      gtk_open_with_widget_set_show_other (self, g_value_get_boolean (value));
      break;
    case PROP_SHOW_ALL:
      gtk_open_with_widget_set_show_all (self, g_value_get_boolean (value));
      break;
    case PROP_RADIO_MODE:
      gtk_open_with_widget_set_radio_mode (self, g_value_get_boolean (value));
      break;
    case PROP_DEFAULT_TEXT:
      gtk_open_with_widget_set_default_text (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_open_with_widget_get_property (GObject *object,
				   guint property_id,
				   GValue *value,
				   GParamSpec *pspec)
{
  GtkOpenWithWidget *self = GTK_OPEN_WITH_WIDGET (object);

  switch (property_id)
    {
    case PROP_CONTENT_TYPE:
      g_value_set_string (value, self->priv->content_type);
      break;
    case PROP_SHOW_RECOMMENDED:
      g_value_set_boolean (value, self->priv->show_recommended);
      break;
    case PROP_SHOW_FALLBACK:
      g_value_set_boolean (value, self->priv->show_fallback);
      break;
    case PROP_SHOW_OTHER:
      g_value_set_boolean (value, self->priv->show_other);
      break;
    case PROP_SHOW_ALL:
      g_value_set_boolean (value, self->priv->show_all);
      break;
    case PROP_RADIO_MODE:
      g_value_set_boolean (value, self->priv->radio_mode);
      break;
    case PROP_DEFAULT_TEXT:
      g_value_set_string (value, self->priv->default_text);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_open_with_widget_constructed (GObject *object)
{
  GtkOpenWithWidget *self = GTK_OPEN_WITH_WIDGET (object);

  g_assert (self->priv->content_type != NULL);

  if (G_OBJECT_CLASS (gtk_open_with_widget_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (gtk_open_with_widget_parent_class)->constructed (object);

  gtk_open_with_widget_add_items (self);
}

static void
gtk_open_with_widget_finalize (GObject *object)
{
  GtkOpenWithWidget *self = GTK_OPEN_WITH_WIDGET (object);

  g_free (self->priv->content_type);
  g_free (self->priv->default_text);

  G_OBJECT_CLASS (gtk_open_with_widget_parent_class)->finalize (object);
}

static void
gtk_open_with_widget_dispose (GObject *object)
{
  GtkOpenWithWidget *self = GTK_OPEN_WITH_WIDGET (object);

  if (self->priv->selected_app_info != NULL)
    {
      g_object_unref (self->priv->selected_app_info);
      self->priv->selected_app_info = NULL;
    }

  G_OBJECT_CLASS (gtk_open_with_widget_parent_class)->dispose (object);
}

static void
gtk_open_with_widget_class_init (GtkOpenWithWidgetClass *klass)
{
  GObjectClass *gobject_class;
  GParamSpec *pspec;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = gtk_open_with_widget_dispose;
  gobject_class->finalize = gtk_open_with_widget_finalize;
  gobject_class->set_property = gtk_open_with_widget_set_property;
  gobject_class->get_property = gtk_open_with_widget_get_property;
  gobject_class->constructed = gtk_open_with_widget_constructed;

  g_object_class_override_property (gobject_class, PROP_CONTENT_TYPE, "content-type");

  pspec = g_param_spec_boolean ("show-recommended",
				P_("Show recommended apps"),
				P_("Whether the widget should show recommended applications"),
				TRUE,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class, PROP_SHOW_RECOMMENDED, pspec);

  pspec = g_param_spec_boolean ("show-fallback",
				P_("Show fallback apps"),
				P_("Whether the widget should show fallback applications"),
				FALSE,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class, PROP_SHOW_FALLBACK, pspec);

  pspec = g_param_spec_boolean ("show-other",
				P_("Show other apps"),
				P_("Whether the widget should show other applications"),
				FALSE,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class, PROP_SHOW_OTHER, pspec);

  pspec = g_param_spec_boolean ("show-all",
				P_("Show all apps"),
				P_("Whether the widget should show all applications"),
				FALSE,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class, PROP_SHOW_ALL, pspec);

  pspec = g_param_spec_boolean ("radio-mode",
				P_("Show radio buttons"),
				P_("Show radio buttons for selected application"),
				FALSE,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class, PROP_RADIO_MODE, pspec);

  pspec = g_param_spec_string ("default-text",
			       P_("Widget's default text"),
			       P_("The default text appearing when there are no applications"),
			       NULL,
			       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class, PROP_DEFAULT_TEXT, pspec);

  signals[SIGNAL_APPLICATION_SELECTED] =
    g_signal_new ("application-selected",
		  GTK_TYPE_OPEN_WITH_WIDGET,
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkOpenWithWidgetClass, application_selected),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE,
		  1, G_TYPE_APP_INFO);

  signals[SIGNAL_APPLICATION_ACTIVATED] =
    g_signal_new ("application-activated",
		  GTK_TYPE_OPEN_WITH_WIDGET,
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkOpenWithWidgetClass, application_activated),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE,
		  1, G_TYPE_APP_INFO);

  g_type_class_add_private (klass, sizeof (GtkOpenWithWidgetPrivate));
}

static void
gtk_open_with_widget_init (GtkOpenWithWidget *self)
{
  GtkWidget *scrolled_window;
  GtkTreeSelection *selection;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GTK_TYPE_OPEN_WITH_WIDGET,
					    GtkOpenWithWidgetPrivate);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_size_request (scrolled_window, 400, 300);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window),
				       GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				  GTK_POLICY_NEVER,
				  GTK_POLICY_AUTOMATIC);
  gtk_widget_show (scrolled_window);

  self->priv->program_list = gtk_tree_view_new ();
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (self->priv->program_list),
				     FALSE);
  gtk_container_add (GTK_CONTAINER (scrolled_window), self->priv->program_list);
  gtk_box_pack_start (GTK_BOX (self), scrolled_window, TRUE, TRUE, 0);
  gtk_widget_show (self->priv->program_list);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->priv->program_list));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
  gtk_tree_selection_set_select_function (selection, gtk_open_with_selection_func,
					  self, NULL);
  g_signal_connect_swapped (selection, "changed",
			    G_CALLBACK (refresh_and_emit_app_selected),
			    self);
  g_signal_connect (self->priv->program_list, "row-activated",
		    G_CALLBACK (program_list_selection_activated),
		    self);
  g_signal_connect (self->priv->program_list, "button-press-event",
		    G_CALLBACK (program_list_button_press_event_cb),
		    self);
}

static GAppInfo *
gtk_open_with_widget_get_app_info (GtkOpenWith *object)
{
  GtkOpenWithWidget *self = GTK_OPEN_WITH_WIDGET (object);

  if (self->priv->selected_app_info == NULL)
    return NULL;

  return g_object_ref (self->priv->selected_app_info);
}

static void
gtk_open_with_widget_refresh (GtkOpenWith *object)
{
  GtkOpenWithWidget *self = GTK_OPEN_WITH_WIDGET (object);

  if (self->priv->program_list_store != NULL)
    {
      gtk_list_store_clear (self->priv->program_list_store);

      /* don't add additional xpad if we don't have headings */
      g_object_set (self->priv->padding_renderer,
		    "visible", !self->priv->show_all,
		    NULL);

      gtk_open_with_widget_real_add_items (self);
    }
}

static void
gtk_open_with_widget_iface_init (GtkOpenWithIface *iface)
{
  iface->get_app_info = gtk_open_with_widget_get_app_info;
  iface->refresh = gtk_open_with_widget_refresh;
}

GtkWidget *
gtk_open_with_widget_new (const gchar *content_type)
{
  return g_object_new (GTK_TYPE_OPEN_WITH_WIDGET,
		       "content-type", content_type,
		       NULL);
}

void
gtk_open_with_widget_set_show_recommended (GtkOpenWithWidget *self,
					   gboolean setting)
{
  g_return_if_fail (GTK_IS_OPEN_WITH_WIDGET (self));

  if (self->priv->show_recommended != setting)
    {
      self->priv->show_recommended = setting;

      g_object_notify (G_OBJECT (self), "show-recommended");

      gtk_open_with_refresh (GTK_OPEN_WITH (self));
    }
}

gboolean
gtk_open_with_widget_get_show_recommended (GtkOpenWithWidget *self)
{
  g_return_val_if_fail (GTK_IS_OPEN_WITH_WIDGET (self), FALSE);

  return self->priv->show_recommended;
}

void
gtk_open_with_widget_set_show_fallback (GtkOpenWithWidget *self,
					gboolean setting)
{
  g_return_if_fail (GTK_IS_OPEN_WITH_WIDGET (self));

  if (self->priv->show_fallback != setting)
    {
      self->priv->show_fallback = setting;

      g_object_notify (G_OBJECT (self), "show-fallback");

      gtk_open_with_refresh (GTK_OPEN_WITH (self));
    }
}

gboolean
gtk_open_with_widget_get_show_fallback (GtkOpenWithWidget *self)
{
  g_return_val_if_fail (GTK_IS_OPEN_WITH_WIDGET (self), FALSE);

  return self->priv->show_fallback;
}

void
gtk_open_with_widget_set_show_other (GtkOpenWithWidget *self,
				     gboolean setting)
{
  g_return_if_fail (GTK_IS_OPEN_WITH_WIDGET (self));

  if (self->priv->show_other != setting)
    {
      self->priv->show_other = setting;

      g_object_notify (G_OBJECT (self), "show-other");

      gtk_open_with_refresh (GTK_OPEN_WITH (self));
    }
}

gboolean gtk_open_with_widget_get_show_other (GtkOpenWithWidget *self)
{
  g_return_val_if_fail (GTK_IS_OPEN_WITH_WIDGET (self), FALSE);

  return self->priv->show_other;
}

void
gtk_open_with_widget_set_show_all (GtkOpenWithWidget *self,
				   gboolean setting)
{
  g_return_if_fail (GTK_IS_OPEN_WITH_WIDGET (self));

  if (self->priv->show_all != setting)
    {
      self->priv->show_all = setting;

      g_object_notify (G_OBJECT (self), "show-all");

      gtk_open_with_refresh (GTK_OPEN_WITH (self));
    }
}

gboolean
gtk_open_with_widget_get_show_all (GtkOpenWithWidget *self)
{
  g_return_val_if_fail (GTK_IS_OPEN_WITH_WIDGET (self), FALSE);

  return self->priv->show_all;  
}

void
gtk_open_with_widget_set_radio_mode (GtkOpenWithWidget *self,
				     gboolean setting)
{
  g_return_if_fail (GTK_IS_OPEN_WITH_WIDGET (self));

  if (self->priv->radio_mode != setting)
    {
      self->priv->radio_mode = setting;

      g_object_notify (G_OBJECT (self), "radio-mode");

      gtk_open_with_refresh (GTK_OPEN_WITH (self));
    }
}

gboolean
gtk_open_with_widget_get_radio_mode (GtkOpenWithWidget *self)
{
  g_return_val_if_fail (GTK_IS_OPEN_WITH_WIDGET (self), FALSE);

  return self->priv->radio_mode;
}

void
gtk_open_with_widget_set_default_text (GtkOpenWithWidget *self,
				       const gchar *text)
{
  g_return_if_fail (GTK_IS_OPEN_WITH_WIDGET (self));

  if (g_strcmp0 (text, self->priv->default_text) != 0)
    {
      g_free (self->priv->default_text);
      self->priv->default_text = g_strdup (text);

      g_object_notify (G_OBJECT (self), "default-text");

      gtk_open_with_refresh (GTK_OPEN_WITH (self));
    }
}

const gchar *
gtk_open_with_widget_get_default_text (GtkOpenWithWidget *self)
{
  g_return_val_if_fail (GTK_IS_OPEN_WITH_WIDGET (self), NULL);

  return self->priv->default_text;
}
