/*
 * gtkappchooserwidget.c: an app-chooser widget
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Dave Camp <dave@novell.com>
 *          Alexander Larsson <alexl@redhat.com>
 *          Cosimo Cecchi <ccecchi@redhat.com>
 */

#include "config.h"

#include "gtkappchooserwidget.h"

#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtkappchooserwidget.h"
#include "gtkappchooserprivate.h"
#include "gtkliststore.h"
#include "gtktreeview.h"
#include "gtktreeselection.h"
#include "gtktreemodelsort.h"
#include "gtkorientable.h"
#include "gtkscrolledwindow.h"
#include "gtklabel.h"
#include "gtkgestureclick.h"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <gio/gio.h>

/**
 * SECTION:gtkappchooserwidget
 * @Title: GtkAppChooserWidget
 * @Short_description: Application chooser widget that can be embedded in other widgets
 *
 * #GtkAppChooserWidget is a widget for selecting applications.
 * It is the main building block for #GtkAppChooserDialog. Most
 * applications only need to use the latter; but you can use
 * this widget as part of a larger widget if you have special needs.
 *
 * #GtkAppChooserWidget offers detailed control over what applications
 * are shown, using the
 * #GtkAppChooserWidget:show-default,
 * #GtkAppChooserWidget:show-recommended,
 * #GtkAppChooserWidget:show-fallback,
 * #GtkAppChooserWidget:show-other and
 * #GtkAppChooserWidget:show-all
 * properties. See the #GtkAppChooser documentation for more information
 * about these groups of applications.
 *
 * To keep track of the selected application, use the
 * #GtkAppChooserWidget::application-selected and #GtkAppChooserWidget::application-activated signals.
 *
 * # CSS nodes
 *
 * GtkAppChooserWidget has a single CSS node with name appchooser.
 */

typedef struct _GtkAppChooserWidgetClass   GtkAppChooserWidgetClass;
typedef struct _GtkAppChooserWidgetPrivate GtkAppChooserWidgetPrivate;

struct _GtkAppChooserWidget {
  GtkWidget parent_instance;
};

struct _GtkAppChooserWidgetClass {
  GtkWidgetClass parent_class;

  void (* application_selected)  (GtkAppChooserWidget *self,
                                  GAppInfo            *app_info);

  void (* application_activated) (GtkAppChooserWidget *self,
                                  GAppInfo            *app_info);

  void (* populate_popup)        (GtkAppChooserWidget *self,
                                  GtkMenu             *menu,
                                  GAppInfo            *app_info);
};

struct _GtkAppChooserWidgetPrivate {
  GAppInfo *selected_app_info;

  GtkWidget *overlay;

  gchar *content_type;
  gchar *default_text;

  guint show_default     : 1;
  guint show_recommended : 1;
  guint show_fallback    : 1;
  guint show_other       : 1;
  guint show_all         : 1;

  GtkWidget *program_list;
  GtkListStore *program_list_store;
  GtkWidget *no_apps_label;
  GtkWidget *no_apps;

  GtkTreeViewColumn *column;
  GtkCellRenderer *padding_renderer;
  GtkCellRenderer *secondary_padding;

  GAppInfoMonitor *monitor;

  GtkWidget *popup_menu;
};

enum {
  COLUMN_APP_INFO,
  COLUMN_GICON,
  COLUMN_NAME,
  COLUMN_DESC,
  COLUMN_EXEC,
  COLUMN_DEFAULT,
  COLUMN_HEADING,
  COLUMN_HEADING_TEXT,
  COLUMN_RECOMMENDED,
  COLUMN_FALLBACK,
  NUM_COLUMNS
};


enum {
  PROP_CONTENT_TYPE = 1,
  PROP_GFILE,
  PROP_SHOW_DEFAULT,
  PROP_SHOW_RECOMMENDED,
  PROP_SHOW_FALLBACK,
  PROP_SHOW_OTHER,
  PROP_SHOW_ALL,
  PROP_DEFAULT_TEXT,
  N_PROPERTIES
};

enum {
  SIGNAL_APPLICATION_SELECTED,
  SIGNAL_APPLICATION_ACTIVATED,
  SIGNAL_POPULATE_POPUP,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };

static void gtk_app_chooser_widget_iface_init (GtkAppChooserIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkAppChooserWidget, gtk_app_chooser_widget, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GtkAppChooserWidget)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_APP_CHOOSER,
                                                gtk_app_chooser_widget_iface_init));

static void
refresh_and_emit_app_selected (GtkAppChooserWidget *self,
                               GtkTreeSelection    *selection)
{
  GtkAppChooserWidgetPrivate *priv = gtk_app_chooser_widget_get_instance_private (self);
  GtkTreeModel *model;
  GtkTreeIter iter;
  GAppInfo *info = NULL;
  gboolean should_emit = FALSE;

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    gtk_tree_model_get (model, &iter, COLUMN_APP_INFO, &info, -1);

  if (info == NULL)
    return;

  if (priv->selected_app_info)
    {
      if (!g_app_info_equal (priv->selected_app_info, info))
        {
          should_emit = TRUE;
          g_set_object (&priv->selected_app_info, info);
        }
    }
  else
    {
      should_emit = TRUE;
      g_set_object (&priv->selected_app_info, info);
    }

  g_object_unref (info);

  if (should_emit)
    g_signal_emit (self, signals[SIGNAL_APPLICATION_SELECTED], 0,
                   priv->selected_app_info);
}

static GAppInfo *
get_app_info_for_coords (GtkAppChooserWidget *self,
                         double               x,
                         double               y)
{
  GtkAppChooserWidgetPrivate *priv = gtk_app_chooser_widget_get_instance_private (self);
  GtkTreePath *path = NULL;
  GtkTreeIter iter;
  GtkTreeModel *model;
  GAppInfo *info;
  gboolean recommended;

  if (!gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (priv->program_list),
                                      x, y,
                                      &path,
                                      NULL, NULL, NULL))
    return NULL;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->program_list));

  if (!gtk_tree_model_get_iter (model, &iter, path))
    {
      gtk_tree_path_free (path);
      return NULL;
    }

  /* we only allow interaction with recommended applications */
  gtk_tree_model_get (model, &iter,
                      COLUMN_APP_INFO, &info,
                      COLUMN_RECOMMENDED, &recommended,
                      -1);

  if (!recommended)
    g_clear_object (&info);

  return info;
}

static void
popup_menu_detach (GtkWidget *attach_widget,
                   GtkMenu   *menu)
{
  GtkAppChooserWidget *self = GTK_APP_CHOOSER_WIDGET (attach_widget);
  GtkAppChooserWidgetPrivate *priv = gtk_app_chooser_widget_get_instance_private (self);

  priv->popup_menu = NULL;
}

static void
gtk_app_chooser_row_pressed_cb (GtkGesture *gesture,
                                int         n_press,
                                double      x,
                                double      y,
                                gpointer    user_data)
{
  GtkAppChooserWidget *self = user_data;
  GtkAppChooserWidgetPrivate *priv = gtk_app_chooser_widget_get_instance_private (self);
  GAppInfo *info;
  GtkWidget *menu;
  GList *children;
  gint n_children;

  info = get_app_info_for_coords (self, x, y);

  if (info == NULL)
    return;

  if (priv->popup_menu)
    gtk_widget_destroy (priv->popup_menu);

  priv->popup_menu = menu = gtk_menu_new ();
  gtk_menu_attach_to_widget (GTK_MENU (menu), GTK_WIDGET (self), popup_menu_detach);

  g_signal_emit (self, signals[SIGNAL_POPULATE_POPUP], 0, menu, info);

  g_object_unref (info);

  /* see if clients added menu items to this container */
  children = gtk_container_get_children (GTK_CONTAINER (menu));
  n_children = g_list_length (children);

  if (n_children > 0) /* actually popup the menu */
    gtk_menu_popup_at_pointer (GTK_MENU (menu), NULL);

  g_list_free (children);
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
program_list_selection_activated (GtkTreeView       *view,
                                  GtkTreePath       *path,
                                  GtkTreeViewColumn *column,
                                  gpointer           user_data)
{
  GtkAppChooserWidget *self = user_data;
  GtkAppChooserWidgetPrivate *priv = gtk_app_chooser_widget_get_instance_private (self);
  GtkTreeSelection *selection;

  if (path_is_heading (view, path))
    return;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->program_list));

  refresh_and_emit_app_selected (self, selection);

  g_signal_emit (self, signals[SIGNAL_APPLICATION_ACTIVATED], 0,
                 priv->selected_app_info);
}

static gboolean
gtk_app_chooser_search_equal_func (GtkTreeModel *model,
                                   gint          column,
                                   const gchar  *key,
                                   GtkTreeIter  *iter,
                                   gpointer      user_data)
{
  gchar *name;
  gchar *exec_name;
  gboolean ret;

  if (key != NULL)
    {
      ret = TRUE;

      gtk_tree_model_get (model, iter,
                          COLUMN_NAME, &name,
                          COLUMN_EXEC, &exec_name,
                          -1);

      if ((name != NULL && g_str_match_string (key, name, TRUE)) ||
          (exec_name != NULL && g_str_match_string (key, exec_name, FALSE)))
        ret = FALSE;

      g_free (name);
      g_free (exec_name);

      return ret;
    }
  else
    {
      return TRUE;
    }
}

static gint
gtk_app_chooser_sort_func (GtkTreeModel *model,
                           GtkTreeIter  *a,
                           GtkTreeIter  *b,
                           gpointer      user_data)
{
  gboolean a_recommended, b_recommended;
  gboolean a_fallback, b_fallback;
  gboolean a_heading, b_heading;
  gboolean a_default, b_default;
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
                      COLUMN_DEFAULT, &a_default,
                      -1);

  gtk_tree_model_get (model, b,
                      COLUMN_NAME, &b_name,
                      COLUMN_RECOMMENDED, &b_recommended,
                      COLUMN_FALLBACK, &b_fallback,
                      COLUMN_HEADING, &b_heading,
                      COLUMN_DEFAULT, &b_default,
                      -1);

  /* the default one always wins */
  if (a_default && !b_default)
    {
      retval = -1;
      goto out;
    }

  if (b_default && !a_default)
    {
      retval = 1;
      goto out;
    }
  
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

  /* they're both recommended/fallback or not, so if one is a heading, wins */
  if (a_heading)
    {
      retval = -1;
      goto out;
    }

  if (b_heading)
    {
      retval = 1;
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
padding_cell_renderer_func (GtkTreeViewColumn *column,
                            GtkCellRenderer   *cell,
                            GtkTreeModel      *model,
                            GtkTreeIter       *iter,
                            gpointer           user_data)
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
gtk_app_chooser_selection_func (GtkTreeSelection *selection,
                                GtkTreeModel     *model,
                                GtkTreePath      *path,
                                gboolean          path_currently_selected,
                                gpointer          user_data)
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
gtk_app_chooser_widget_add_section (GtkAppChooserWidget *self,
                                    const gchar         *heading_title,
                                    gboolean             show_headings,
                                    gboolean             recommended,
                                    gboolean             fallback,
                                    GList               *applications,
                                    GList               *exclude_apps)
{
  GtkAppChooserWidgetPrivate *priv = gtk_app_chooser_widget_get_instance_private (self);
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

      if (priv->content_type != NULL &&
          !g_app_info_supports_uris (app) &&
          !g_app_info_supports_files (app))
        continue;

      if (g_list_find_custom (exclude_apps, app,
                              (GCompareFunc) compare_apps_func))
        continue;

      if (!heading_added && show_headings)
        {
          gtk_list_store_append (priv->program_list_store, &iter);
          gtk_list_store_set (priv->program_list_store, &iter,
                              COLUMN_HEADING_TEXT, bold_string,
                              COLUMN_HEADING, TRUE,
                              COLUMN_RECOMMENDED, recommended,
                              COLUMN_FALLBACK, fallback,
                              -1);

          heading_added = TRUE;
        }

      app_string = g_markup_printf_escaped ("%s",
                                            g_app_info_get_name (app) != NULL ?
                                            g_app_info_get_name (app) : "");

      icon = g_app_info_get_icon (app);
      unref_icon = FALSE;
      if (icon == NULL)
        {
          icon = g_themed_icon_new ("application-x-executable");
          unref_icon = TRUE;
        }

      gtk_list_store_append (priv->program_list_store, &iter);
      gtk_list_store_set (priv->program_list_store, &iter,
                          COLUMN_APP_INFO, app,
                          COLUMN_GICON, icon,
                          COLUMN_NAME, g_app_info_get_name (app),
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
    }

  g_free (bold_string);

  return retval;
}


static void
gtk_app_chooser_add_default (GtkAppChooserWidget *self,
                             GAppInfo            *app)
{
  GtkAppChooserWidgetPrivate *priv = gtk_app_chooser_widget_get_instance_private (self);
  GtkTreeIter iter;
  GIcon *icon;
  gchar *string;
  gboolean unref_icon;

  unref_icon = FALSE;
  string = g_strdup_printf ("<b>%s</b>", _("Default Application"));

  gtk_list_store_append (priv->program_list_store, &iter);
  gtk_list_store_set (priv->program_list_store, &iter,
                      COLUMN_HEADING_TEXT, string,
                      COLUMN_HEADING, TRUE,
                      COLUMN_DEFAULT, TRUE,
                      -1);

  g_free (string);

  string = g_markup_printf_escaped ("%s",
                                    g_app_info_get_name (app) != NULL ?
                                    g_app_info_get_name (app) : "");

  icon = g_app_info_get_icon (app);
  if (icon == NULL)
    {
      icon = g_themed_icon_new ("application-x-executable");
      unref_icon = TRUE;
    }

  gtk_list_store_append (priv->program_list_store, &iter);
  gtk_list_store_set (priv->program_list_store, &iter,
                      COLUMN_APP_INFO, app,
                      COLUMN_GICON, icon,
                      COLUMN_NAME, g_app_info_get_name (app),
                      COLUMN_DESC, string,
                      COLUMN_EXEC, g_app_info_get_executable (app),
                      COLUMN_HEADING, FALSE,
                      COLUMN_DEFAULT, TRUE,
                      -1);

  g_free (string);

  if (unref_icon)
    g_object_unref (icon);
}

static void
update_no_applications_label (GtkAppChooserWidget *self)
{
  GtkAppChooserWidgetPrivate *priv = gtk_app_chooser_widget_get_instance_private (self);
  gchar *text = NULL, *desc = NULL;
  const gchar *string;

  if (priv->default_text == NULL)
    {
      if (priv->content_type)
	desc = g_content_type_get_description (priv->content_type);

      string = text = g_strdup_printf (_("No applications found for “%s”."), desc);
      g_free (desc);
    }
  else
    {
      string = priv->default_text;
    }

  gtk_label_set_text (GTK_LABEL (priv->no_apps_label), string);

  g_free (text);
}

static void
gtk_app_chooser_widget_select_first (GtkAppChooserWidget *self)
{
  GtkAppChooserWidgetPrivate *priv = gtk_app_chooser_widget_get_instance_private (self);
  GtkTreeIter iter;
  GAppInfo *info = NULL;
  GtkTreeModel *model;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->program_list));
  if (!gtk_tree_model_get_iter_first (model, &iter))
    return;

  while (info == NULL)
    {
      gtk_tree_model_get (model, &iter,
                          COLUMN_APP_INFO, &info,
                          -1);

      if (info != NULL)
        break;

      if (!gtk_tree_model_iter_next (model, &iter))
        break;
    }

  if (info != NULL)
    {
      GtkTreeSelection *selection;

      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->program_list));
      gtk_tree_selection_select_iter (selection, &iter);

      g_object_unref (info);
    }
}

static void
gtk_app_chooser_widget_real_add_items (GtkAppChooserWidget *self)
{
  GtkAppChooserWidgetPrivate *priv = gtk_app_chooser_widget_get_instance_private (self);
  GList *all_applications = NULL;
  GList *recommended_apps = NULL;
  GList *fallback_apps = NULL;
  GList *exclude_apps = NULL;
  GAppInfo *default_app = NULL;
  gboolean show_headings;
  gboolean apps_added;

  show_headings = TRUE;
  apps_added = FALSE;

  if (priv->show_all)
    show_headings = FALSE;

  if (priv->show_default && priv->content_type)
    {
      default_app = g_app_info_get_default_for_type (priv->content_type, FALSE);

      if (default_app != NULL)
        {
          gtk_app_chooser_add_default (self, default_app);
          apps_added = TRUE;
          exclude_apps = g_list_prepend (exclude_apps, default_app);
        }
    }

#ifndef G_OS_WIN32
  if ((priv->content_type && priv->show_recommended) || priv->show_all)
    {
      if (priv->content_type)
	recommended_apps = g_app_info_get_recommended_for_type (priv->content_type);

      apps_added |= gtk_app_chooser_widget_add_section (self, _("Recommended Applications"),
                                                        show_headings,
                                                        !priv->show_all, /* mark as recommended */
                                                        FALSE, /* mark as fallback */
                                                        recommended_apps, exclude_apps);

      exclude_apps = g_list_concat (exclude_apps,
                                    g_list_copy (recommended_apps));
    }

  if ((priv->content_type && priv->show_fallback) || priv->show_all)
    {
      if (priv->content_type)
	fallback_apps = g_app_info_get_fallback_for_type (priv->content_type);

      apps_added |= gtk_app_chooser_widget_add_section (self, _("Related Applications"),
                                                        show_headings,
                                                        FALSE, /* mark as recommended */
                                                        !priv->show_all, /* mark as fallback */
                                                        fallback_apps, exclude_apps);
      exclude_apps = g_list_concat (exclude_apps,
                                    g_list_copy (fallback_apps));
    }
#endif

  if (priv->show_other || priv->show_all)
    {
      all_applications = g_app_info_get_all ();

      apps_added |= gtk_app_chooser_widget_add_section (self, _("Other Applications"),
                                                        show_headings,
                                                        FALSE,
                                                        FALSE,
                                                        all_applications, exclude_apps);
    }

  if (!apps_added)
    update_no_applications_label (self);

  gtk_widget_set_visible (priv->no_apps, !apps_added);

  gtk_app_chooser_widget_select_first (self);

  if (default_app != NULL)
    g_object_unref (default_app);

  g_list_free_full (all_applications, g_object_unref);
  g_list_free_full (recommended_apps, g_object_unref);
  g_list_free_full (fallback_apps, g_object_unref);
  g_list_free (exclude_apps);
}

static void
gtk_app_chooser_widget_initialize_items (GtkAppChooserWidget *self)
{
  GtkAppChooserWidgetPrivate *priv = gtk_app_chooser_widget_get_instance_private (self);

  /* initial padding */
  g_object_set (priv->padding_renderer,
                "xpad", priv->show_all ? 0 : 6,
                NULL);

  /* populate the widget */
  gtk_app_chooser_refresh (GTK_APP_CHOOSER (self));
}

static void
app_info_changed (GAppInfoMonitor     *monitor,
                  GtkAppChooserWidget *self)
{
  gtk_app_chooser_refresh (GTK_APP_CHOOSER (self));
}

static void
gtk_app_chooser_widget_set_property (GObject      *object,
                                     guint         property_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkAppChooserWidget *self = GTK_APP_CHOOSER_WIDGET (object);
  GtkAppChooserWidgetPrivate *priv = gtk_app_chooser_widget_get_instance_private (self);

  switch (property_id)
    {
    case PROP_CONTENT_TYPE:
      priv->content_type = g_value_dup_string (value);
      break;
    case PROP_SHOW_DEFAULT:
      gtk_app_chooser_widget_set_show_default (self, g_value_get_boolean (value));
      break;
    case PROP_SHOW_RECOMMENDED:
      gtk_app_chooser_widget_set_show_recommended (self, g_value_get_boolean (value));
      break;
    case PROP_SHOW_FALLBACK:
      gtk_app_chooser_widget_set_show_fallback (self, g_value_get_boolean (value));
      break;
    case PROP_SHOW_OTHER:
      gtk_app_chooser_widget_set_show_other (self, g_value_get_boolean (value));
      break;
    case PROP_SHOW_ALL:
      gtk_app_chooser_widget_set_show_all (self, g_value_get_boolean (value));
      break;
    case PROP_DEFAULT_TEXT:
      gtk_app_chooser_widget_set_default_text (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_app_chooser_widget_get_property (GObject    *object,
                                     guint       property_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GtkAppChooserWidget *self = GTK_APP_CHOOSER_WIDGET (object);
  GtkAppChooserWidgetPrivate *priv = gtk_app_chooser_widget_get_instance_private (self);

  switch (property_id)
    {
    case PROP_CONTENT_TYPE:
      g_value_set_string (value, priv->content_type);
      break;
    case PROP_SHOW_DEFAULT:
      g_value_set_boolean (value, priv->show_default);
      break;
    case PROP_SHOW_RECOMMENDED:
      g_value_set_boolean (value, priv->show_recommended);
      break;
    case PROP_SHOW_FALLBACK:
      g_value_set_boolean (value, priv->show_fallback);
      break;
    case PROP_SHOW_OTHER:
      g_value_set_boolean (value, priv->show_other);
      break;
    case PROP_SHOW_ALL:
      g_value_set_boolean (value, priv->show_all);
      break;
    case PROP_DEFAULT_TEXT:
      g_value_set_string (value, priv->default_text);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_app_chooser_widget_constructed (GObject *object)
{
  GtkAppChooserWidget *self = GTK_APP_CHOOSER_WIDGET (object);

  if (G_OBJECT_CLASS (gtk_app_chooser_widget_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (gtk_app_chooser_widget_parent_class)->constructed (object);

  gtk_app_chooser_widget_initialize_items (self);
}

static void
gtk_app_chooser_widget_finalize (GObject *object)
{
  GtkAppChooserWidget *self = GTK_APP_CHOOSER_WIDGET (object);
  GtkAppChooserWidgetPrivate *priv = gtk_app_chooser_widget_get_instance_private (self);

  g_free (priv->content_type);
  g_free (priv->default_text);
  g_signal_handlers_disconnect_by_func (priv->monitor, app_info_changed, self);
  g_object_unref (priv->monitor);

  G_OBJECT_CLASS (gtk_app_chooser_widget_parent_class)->finalize (object);
}

static void
gtk_app_chooser_widget_dispose (GObject *object)
{
  GtkAppChooserWidget *self = GTK_APP_CHOOSER_WIDGET (object);
  GtkAppChooserWidgetPrivate *priv = gtk_app_chooser_widget_get_instance_private (self);

  g_clear_object (&priv->selected_app_info);

  if (priv->overlay)
    {
      gtk_widget_unparent (priv->overlay);
      priv->overlay = NULL;
    }

  G_OBJECT_CLASS (gtk_app_chooser_widget_parent_class)->dispose (object);
}

static void
gtk_app_chooser_widget_measure (GtkWidget       *widget,
                                GtkOrientation  orientation,
                                int             for_size,
                                int            *minimum,
                                int            *natural,
                                int            *minimum_baseline,
                                int            *natural_baseline)
{
  GtkAppChooserWidget *self = GTK_APP_CHOOSER_WIDGET (widget);
  GtkAppChooserWidgetPrivate *priv = gtk_app_chooser_widget_get_instance_private (self);

  gtk_widget_measure (priv->overlay, orientation, for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);
}

static void
gtk_app_chooser_widget_snapshot (GtkWidget   *widget,
                                 GtkSnapshot *snapshot)
{
  GtkAppChooserWidget *self = GTK_APP_CHOOSER_WIDGET (widget);
  GtkAppChooserWidgetPrivate *priv = gtk_app_chooser_widget_get_instance_private (self);

  gtk_widget_snapshot_child (widget, priv->overlay, snapshot);
}

static void
gtk_app_chooser_widget_size_allocate (GtkWidget *widget,
                                      int        width,
                                      int        height,
                                      int        baseline)
{
  GtkAppChooserWidget *self = GTK_APP_CHOOSER_WIDGET (widget);
  GtkAppChooserWidgetPrivate *priv = gtk_app_chooser_widget_get_instance_private (self);

  GTK_WIDGET_CLASS (gtk_app_chooser_widget_parent_class)->size_allocate (widget, width, height, baseline);

  gtk_widget_size_allocate (priv->overlay,
                            &(GtkAllocation) {
                              0, 0,
                              width, height
                            },baseline);
}

static void
gtk_app_chooser_widget_class_init (GtkAppChooserWidgetClass *klass)
{
  GtkWidgetClass *widget_class;
  GObjectClass *gobject_class;
  GParamSpec *pspec;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = gtk_app_chooser_widget_dispose;
  gobject_class->finalize = gtk_app_chooser_widget_finalize;
  gobject_class->set_property = gtk_app_chooser_widget_set_property;
  gobject_class->get_property = gtk_app_chooser_widget_get_property;
  gobject_class->constructed = gtk_app_chooser_widget_constructed;

  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->measure = gtk_app_chooser_widget_measure;
  widget_class->size_allocate = gtk_app_chooser_widget_size_allocate;
  widget_class->snapshot = gtk_app_chooser_widget_snapshot;


  g_object_class_override_property (gobject_class, PROP_CONTENT_TYPE, "content-type");

  /**
   * GtkAppChooserWidget:show-default:
   *
   * The ::show-default property determines whether the app chooser
   * should show the default handler for the content type in a
   * separate section. If %FALSE, the default handler is listed
   * among the recommended applications.
   */
  pspec = g_param_spec_boolean ("show-default",
                                P_("Show default app"),
                                P_("Whether the widget should show the default application"),
                                FALSE,
                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);
  g_object_class_install_property (gobject_class, PROP_SHOW_DEFAULT, pspec);

  /**
   * GtkAppChooserWidget:show-recommended:
   *
   * The #GtkAppChooserWidget:show-recommended property determines
   * whether the app chooser should show a section for recommended
   * applications. If %FALSE, the recommended applications are listed
   * among the other applications.
   */
  pspec = g_param_spec_boolean ("show-recommended",
                                P_("Show recommended apps"),
                                P_("Whether the widget should show recommended applications"),
                                TRUE,
                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);
  g_object_class_install_property (gobject_class, PROP_SHOW_RECOMMENDED, pspec);

  /**
   * GtkAppChooserWidget:show-fallback:
   *
   * The #GtkAppChooserWidget:show-fallback property determines whether
   * the app chooser should show a section for fallback applications.
   * If %FALSE, the fallback applications are listed among the other
   * applications.
   */
  pspec = g_param_spec_boolean ("show-fallback",
                                P_("Show fallback apps"),
                                P_("Whether the widget should show fallback applications"),
                                FALSE,
                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);
  g_object_class_install_property (gobject_class, PROP_SHOW_FALLBACK, pspec);

  /**
   * GtkAppChooserWidget:show-other:
   *
   * The #GtkAppChooserWidget:show-other property determines whether
   * the app chooser should show a section for other applications.
   */
  pspec = g_param_spec_boolean ("show-other",
                                P_("Show other apps"),
                                P_("Whether the widget should show other applications"),
                                FALSE,
                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);
  g_object_class_install_property (gobject_class, PROP_SHOW_OTHER, pspec);

  /**
   * GtkAppChooserWidget:show-all:
   *
   * If the #GtkAppChooserWidget:show-all property is %TRUE, the app
   * chooser presents all applications in a single list, without
   * subsections for default, recommended or related applications.
   */
  pspec = g_param_spec_boolean ("show-all",
                                P_("Show all apps"),
                                P_("Whether the widget should show all applications"),
                                FALSE,
                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);
  g_object_class_install_property (gobject_class, PROP_SHOW_ALL, pspec);

  /**
   * GtkAppChooserWidget:default-text:
   *
   * The #GtkAppChooserWidget:default-text property determines the text
   * that appears in the widget when there are no applications for the
   * given content type.
   * See also gtk_app_chooser_widget_set_default_text().
   */
  pspec = g_param_spec_string ("default-text",
                               P_("Widget’s default text"),
                               P_("The default text appearing when there are no applications"),
                               NULL,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);
  g_object_class_install_property (gobject_class, PROP_DEFAULT_TEXT, pspec);

  /**
   * GtkAppChooserWidget::application-selected:
   * @self: the object which received the signal
   * @application: the selected #GAppInfo
   *
   * Emitted when an application item is selected from the widget's list.
   */
  signals[SIGNAL_APPLICATION_SELECTED] =
    g_signal_new (I_("application-selected"),
                  GTK_TYPE_APP_CHOOSER_WIDGET,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkAppChooserWidgetClass, application_selected),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  1, G_TYPE_APP_INFO);

  /**
   * GtkAppChooserWidget::application-activated:
   * @self: the object which received the signal
   * @application: the activated #GAppInfo
   *
   * Emitted when an application item is activated from the widget's list.
   *
   * This usually happens when the user double clicks an item, or an item
   * is selected and the user presses one of the keys Space, Shift+Space,
   * Return or Enter.
   */
  signals[SIGNAL_APPLICATION_ACTIVATED] =
    g_signal_new (I_("application-activated"),
                  GTK_TYPE_APP_CHOOSER_WIDGET,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkAppChooserWidgetClass, application_activated),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  1, G_TYPE_APP_INFO);

  /**
   * GtkAppChooserWidget::populate-popup:
   * @self: the object which received the signal
   * @menu: the #GtkMenu to populate
   * @application: the current #GAppInfo
   *
   * Emitted when a context menu is about to popup over an application item.
   * Clients can insert menu items into the provided #GtkMenu object in the
   * callback of this signal; the context menu will be shown over the item
   * if at least one item has been added to the menu.
   */
  signals[SIGNAL_POPULATE_POPUP] =
    g_signal_new (I_("populate-popup"),
                  GTK_TYPE_APP_CHOOSER_WIDGET,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkAppChooserWidgetClass, populate_popup),
                  NULL, NULL,
                  _gtk_marshal_VOID__OBJECT_OBJECT,
                  G_TYPE_NONE,
                  2, GTK_TYPE_MENU, G_TYPE_APP_INFO);

  /* Bind class to template
   */
  gtk_widget_class_set_template_from_resource (widget_class,
					       "/org/gtk/libgtk/ui/gtkappchooserwidget.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkAppChooserWidget, program_list);
  gtk_widget_class_bind_template_child_private (widget_class, GtkAppChooserWidget, program_list_store);
  gtk_widget_class_bind_template_child_private (widget_class, GtkAppChooserWidget, column);
  gtk_widget_class_bind_template_child_private (widget_class, GtkAppChooserWidget, padding_renderer);
  gtk_widget_class_bind_template_child_private (widget_class, GtkAppChooserWidget, secondary_padding);
  gtk_widget_class_bind_template_child_private (widget_class, GtkAppChooserWidget, no_apps_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkAppChooserWidget, no_apps);
  gtk_widget_class_bind_template_child_private (widget_class, GtkAppChooserWidget, overlay);
  gtk_widget_class_bind_template_callback (widget_class, refresh_and_emit_app_selected);
  gtk_widget_class_bind_template_callback (widget_class, program_list_selection_activated);

  gtk_widget_class_set_css_name (widget_class, I_("appchooser"));
}

static void
gtk_app_chooser_widget_init (GtkAppChooserWidget *self)
{
  GtkAppChooserWidgetPrivate *priv = gtk_app_chooser_widget_get_instance_private (self);
  GtkTreeSelection *selection;
  GtkTreeModel *sort;
  GtkGesture *gesture;

  gtk_widget_init_template (GTK_WIDGET (self));

  /* Various parts of the GtkTreeView code need custom code to setup, mostly
   * because we lack signals to connect to, or properties to set.
   */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->program_list));
  gtk_tree_selection_set_select_function (selection, gtk_app_chooser_selection_func,
                                          self, NULL);

  sort = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->program_list));
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sort),
                                        COLUMN_NAME,
                                        GTK_SORT_ASCENDING);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (sort),
                                   COLUMN_NAME,
                                   gtk_app_chooser_sort_func,
                                   self, NULL);

  gtk_tree_view_set_search_column (GTK_TREE_VIEW (priv->program_list), COLUMN_NAME);
  gtk_tree_view_set_search_equal_func (GTK_TREE_VIEW (priv->program_list),
                                       gtk_app_chooser_search_equal_func,
                                       NULL, NULL);

  gtk_tree_view_column_set_cell_data_func (priv->column,
					   priv->secondary_padding,
                                           padding_cell_renderer_func,
                                           NULL, NULL);

  priv->monitor = g_app_info_monitor_get ();
  g_signal_connect (priv->monitor, "changed",
		    G_CALLBACK (app_info_changed), self);

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_SECONDARY);
  g_signal_connect (gesture, "pressed",
                    G_CALLBACK (gtk_app_chooser_row_pressed_cb), self);
  gtk_widget_add_controller (priv->program_list, GTK_EVENT_CONTROLLER (gesture));
}

static GAppInfo *
gtk_app_chooser_widget_get_app_info (GtkAppChooser *object)
{
  GtkAppChooserWidget *self = GTK_APP_CHOOSER_WIDGET (object);
  GtkAppChooserWidgetPrivate *priv = gtk_app_chooser_widget_get_instance_private (self);

  if (priv->selected_app_info == NULL)
    return NULL;

  return g_object_ref (priv->selected_app_info);
}

static void
gtk_app_chooser_widget_refresh (GtkAppChooser *object)
{
  GtkAppChooserWidget *self = GTK_APP_CHOOSER_WIDGET (object);
  GtkAppChooserWidgetPrivate *priv = gtk_app_chooser_widget_get_instance_private (self);

  if (priv->program_list_store != NULL)
    {
      gtk_list_store_clear (priv->program_list_store);

      /* don't add additional xpad if we don't have headings */
      g_object_set (priv->padding_renderer,
                    "visible", !priv->show_all,
                    NULL);

      gtk_app_chooser_widget_real_add_items (self);
    }
}

static void
gtk_app_chooser_widget_iface_init (GtkAppChooserIface *iface)
{
  iface->get_app_info = gtk_app_chooser_widget_get_app_info;
  iface->refresh = gtk_app_chooser_widget_refresh;
}

/**
 * gtk_app_chooser_widget_new:
 * @content_type: the content type to show applications for
 *
 * Creates a new #GtkAppChooserWidget for applications
 * that can handle content of the given type.
 *
 * Returns: a newly created #GtkAppChooserWidget
 */
GtkWidget *
gtk_app_chooser_widget_new (const gchar *content_type)
{
  return g_object_new (GTK_TYPE_APP_CHOOSER_WIDGET,
                       "content-type", content_type,
                       NULL);
}

/**
 * gtk_app_chooser_widget_set_show_default:
 * @self: a #GtkAppChooserWidget
 * @setting: the new value for #GtkAppChooserWidget:show-default
 *
 * Sets whether the app chooser should show the default handler
 * for the content type in a separate section.
 */
void
gtk_app_chooser_widget_set_show_default (GtkAppChooserWidget *self,
                                         gboolean             setting)
{
  GtkAppChooserWidgetPrivate *priv = gtk_app_chooser_widget_get_instance_private (self);

  g_return_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self));

  if (priv->show_default != setting)
    {
      priv->show_default = setting;

      g_object_notify (G_OBJECT (self), "show-default");

      gtk_app_chooser_refresh (GTK_APP_CHOOSER (self));
    }
}

/**
 * gtk_app_chooser_widget_get_show_default:
 * @self: a #GtkAppChooserWidget
 *
 * Returns the current value of the #GtkAppChooserWidget:show-default
 * property.
 *
 * Returns: the value of #GtkAppChooserWidget:show-default
 */
gboolean
gtk_app_chooser_widget_get_show_default (GtkAppChooserWidget *self)
{
  GtkAppChooserWidgetPrivate *priv = gtk_app_chooser_widget_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self), FALSE);

  return priv->show_default;
}

/**
 * gtk_app_chooser_widget_set_show_recommended:
 * @self: a #GtkAppChooserWidget
 * @setting: the new value for #GtkAppChooserWidget:show-recommended
 *
 * Sets whether the app chooser should show recommended applications
 * for the content type in a separate section.
 */
void
gtk_app_chooser_widget_set_show_recommended (GtkAppChooserWidget *self,
                                             gboolean             setting)
{
  GtkAppChooserWidgetPrivate *priv = gtk_app_chooser_widget_get_instance_private (self);

  g_return_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self));

  if (priv->show_recommended != setting)
    {
      priv->show_recommended = setting;

      g_object_notify (G_OBJECT (self), "show-recommended");

      gtk_app_chooser_refresh (GTK_APP_CHOOSER (self));
    }
}

/**
 * gtk_app_chooser_widget_get_show_recommended:
 * @self: a #GtkAppChooserWidget
 *
 * Returns the current value of the #GtkAppChooserWidget:show-recommended
 * property.
 *
 * Returns: the value of #GtkAppChooserWidget:show-recommended
 */
gboolean
gtk_app_chooser_widget_get_show_recommended (GtkAppChooserWidget *self)
{
  GtkAppChooserWidgetPrivate *priv = gtk_app_chooser_widget_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self), FALSE);

  return priv->show_recommended;
}

/**
 * gtk_app_chooser_widget_set_show_fallback:
 * @self: a #GtkAppChooserWidget
 * @setting: the new value for #GtkAppChooserWidget:show-fallback
 *
 * Sets whether the app chooser should show related applications
 * for the content type in a separate section.
 */
void
gtk_app_chooser_widget_set_show_fallback (GtkAppChooserWidget *self,
                                          gboolean             setting)
{
  GtkAppChooserWidgetPrivate *priv = gtk_app_chooser_widget_get_instance_private (self);

  g_return_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self));

  if (priv->show_fallback != setting)
    {
      priv->show_fallback = setting;

      g_object_notify (G_OBJECT (self), "show-fallback");

      gtk_app_chooser_refresh (GTK_APP_CHOOSER (self));
    }
}

/**
 * gtk_app_chooser_widget_get_show_fallback:
 * @self: a #GtkAppChooserWidget
 *
 * Returns the current value of the #GtkAppChooserWidget:show-fallback
 * property.
 *
 * Returns: the value of #GtkAppChooserWidget:show-fallback
 */
gboolean
gtk_app_chooser_widget_get_show_fallback (GtkAppChooserWidget *self)
{
  GtkAppChooserWidgetPrivate *priv = gtk_app_chooser_widget_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self), FALSE);

  return priv->show_fallback;
}

/**
 * gtk_app_chooser_widget_set_show_other:
 * @self: a #GtkAppChooserWidget
 * @setting: the new value for #GtkAppChooserWidget:show-other
 *
 * Sets whether the app chooser should show applications
 * which are unrelated to the content type.
 */
void
gtk_app_chooser_widget_set_show_other (GtkAppChooserWidget *self,
                                       gboolean             setting)
{
  GtkAppChooserWidgetPrivate *priv = gtk_app_chooser_widget_get_instance_private (self);

  g_return_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self));

  if (priv->show_other != setting)
    {
      priv->show_other = setting;

      g_object_notify (G_OBJECT (self), "show-other");

      gtk_app_chooser_refresh (GTK_APP_CHOOSER (self));
    }
}

/**
 * gtk_app_chooser_widget_get_show_other:
 * @self: a #GtkAppChooserWidget
 *
 * Returns the current value of the #GtkAppChooserWidget:show-other
 * property.
 *
 * Returns: the value of #GtkAppChooserWidget:show-other
 */
gboolean
gtk_app_chooser_widget_get_show_other (GtkAppChooserWidget *self)
{
  GtkAppChooserWidgetPrivate *priv = gtk_app_chooser_widget_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self), FALSE);

  return priv->show_other;
}

/**
 * gtk_app_chooser_widget_set_show_all:
 * @self: a #GtkAppChooserWidget
 * @setting: the new value for #GtkAppChooserWidget:show-all
 *
 * Sets whether the app chooser should show all applications
 * in a flat list.
 */
void
gtk_app_chooser_widget_set_show_all (GtkAppChooserWidget *self,
                                     gboolean             setting)
{
  GtkAppChooserWidgetPrivate *priv = gtk_app_chooser_widget_get_instance_private (self);

  g_return_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self));

  if (priv->show_all != setting)
    {
      priv->show_all = setting;

      g_object_notify (G_OBJECT (self), "show-all");

      gtk_app_chooser_refresh (GTK_APP_CHOOSER (self));
    }
}

/**
 * gtk_app_chooser_widget_get_show_all:
 * @self: a #GtkAppChooserWidget
 *
 * Returns the current value of the #GtkAppChooserWidget:show-all
 * property.
 *
 * Returns: the value of #GtkAppChooserWidget:show-all
 */
gboolean
gtk_app_chooser_widget_get_show_all (GtkAppChooserWidget *self)
{
  GtkAppChooserWidgetPrivate *priv = gtk_app_chooser_widget_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self), FALSE);

  return priv->show_all;
}

/**
 * gtk_app_chooser_widget_set_default_text:
 * @self: a #GtkAppChooserWidget
 * @text: the new value for #GtkAppChooserWidget:default-text
 *
 * Sets the text that is shown if there are not applications
 * that can handle the content type.
 */
void
gtk_app_chooser_widget_set_default_text (GtkAppChooserWidget *self,
                                         const gchar         *text)
{
  GtkAppChooserWidgetPrivate *priv = gtk_app_chooser_widget_get_instance_private (self);

  g_return_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self));

  if (g_strcmp0 (text, priv->default_text) != 0)
    {
      g_free (priv->default_text);
      priv->default_text = g_strdup (text);

      g_object_notify (G_OBJECT (self), "default-text");

      gtk_app_chooser_refresh (GTK_APP_CHOOSER (self));
    }
}

/**
 * gtk_app_chooser_widget_get_default_text:
 * @self: a #GtkAppChooserWidget
 *
 * Returns the text that is shown if there are not applications
 * that can handle the content type.
 *
 * Returns: the value of #GtkAppChooserWidget:default-text
 */
const gchar *
gtk_app_chooser_widget_get_default_text (GtkAppChooserWidget *self)
{
  GtkAppChooserWidgetPrivate *priv = gtk_app_chooser_widget_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self), NULL);

  return priv->default_text;
}

void
_gtk_app_chooser_widget_set_search_entry (GtkAppChooserWidget *self,
                                          GtkEditable         *entry)
{
  GtkAppChooserWidgetPrivate *priv = gtk_app_chooser_widget_get_instance_private (self);

  gtk_tree_view_set_search_entry (GTK_TREE_VIEW (priv->program_list), entry);

  g_object_bind_property (priv->no_apps, "visible",
                          entry, "sensitive",
                          G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
}
