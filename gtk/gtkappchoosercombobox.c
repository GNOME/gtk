/*
 * gtkappchoosercombobox.h: an app-chooser combobox
 *
 * Copyright (C) 2010 Red Hat, Inc.
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
 * Authors: Cosimo Cecchi <ccecchi@redhat.com>
 */

#include "config.h"

#include "gtkappchoosercombobox.h"

#include "gtkappchooser.h"
#include "gtkappchooserdialog.h"
#include "gtkappchooserprivate.h"
#include "gtkcelllayout.h"
#include "gtkcellrendererpixbuf.h"
#include "gtkcellrenderertext.h"
#include "gtkcombobox.h"
#include "gtkdialog.h"
#include "gtkintl.h"

enum {
  PROP_CONTENT_TYPE = 1,
  PROP_SHOW_DIALOG_ITEM,
};

enum {
  COLUMN_APP_INFO,
  COLUMN_NAME,
  COLUMN_ICON,
  COLUMN_CUSTOM,
  COLUMN_SEPARATOR,
  COLUMN_CALLBACK,
  NUM_COLUMNS,
};

typedef struct {
  GtkAppChooserComboBoxItemFunc func;
  gpointer user_data;
} CustomAppComboData;

static gpointer
custom_app_data_copy (gpointer boxed)
{
  CustomAppComboData *retval, *original;

  original = boxed;

  retval = g_slice_new0 (CustomAppComboData);
  retval->func = original->func;
  retval->user_data = original->user_data;

  return retval;
}

static void
custom_app_data_free (gpointer boxed)
{
  g_slice_free (CustomAppComboData, boxed);
}

#define CUSTOM_COMBO_DATA_TYPE custom_app_combo_data_get_type()
G_DEFINE_BOXED_TYPE (CustomAppComboData, custom_app_combo_data,
                     custom_app_data_copy,
                     custom_app_data_free);

static void app_chooser_iface_init (GtkAppChooserIface *iface);

static void real_insert_custom_item (GtkAppChooserComboBox *self,
				     const gchar *label,
				     GIcon *icon,
				     GtkAppChooserComboBoxItemFunc func,
				     gpointer user_data,
				     gboolean custom,
				     GtkTreeIter *iter);

static void real_insert_separator (GtkAppChooserComboBox *self,
				   gboolean custom,
				   GtkTreeIter *iter);

G_DEFINE_TYPE_WITH_CODE (GtkAppChooserComboBox, gtk_app_chooser_combo_box, GTK_TYPE_COMBO_BOX,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_APP_CHOOSER,
                                                app_chooser_iface_init));

struct _GtkAppChooserComboBoxPrivate {
  GtkListStore *store;

  gchar *content_type;
  gboolean show_dialog_item;
};

static gboolean
row_separator_func (GtkTreeModel *model,
                    GtkTreeIter  *iter,
                    gpointer      user_data)
{
  gboolean separator;

  gtk_tree_model_get (model, iter,
                      COLUMN_SEPARATOR, &separator,
                      -1);

  return separator;
}

static void
get_first_iter (GtkListStore *store,
                GtkTreeIter  *iter)
{
  GtkTreeIter iter2;

  if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), iter))
    {
      /* the model is empty, append */
      gtk_list_store_append (store, iter);
    }
  else
    {
      gtk_list_store_insert_before (store, &iter2, iter);
      *iter = iter2;
    }
}

typedef struct {
  GtkAppChooserComboBox *self;
  GAppInfo *info;
  gint active_index;
} SelectAppData;

static void
select_app_data_free (SelectAppData *data)
{
  g_clear_object (&data->self);
  g_clear_object (&data->info);

  g_slice_free (SelectAppData, data);
}

static gboolean
select_application_func_cb (GtkTreeModel *model,
			    GtkTreePath *path,
			    GtkTreeIter *iter,
			    gpointer user_data)
{
  SelectAppData *data = user_data;
  GAppInfo *app_to_match = data->info, *app = NULL;
  gboolean custom;

  gtk_tree_model_get (model, iter,
		      COLUMN_APP_INFO, &app,
		      COLUMN_CUSTOM, &custom,
		      -1);

  /* cutsom items are always after GAppInfos, so iterating further here
   * is just useless.
   */
  if (custom)
    return TRUE;

  if (g_app_info_equal (app, app_to_match))
    {
      gtk_combo_box_set_active_iter (GTK_COMBO_BOX (data->self), iter);
      return TRUE;
    }

  return FALSE;
}

static void
gtk_app_chooser_combo_box_select_application (GtkAppChooserComboBox *self,
					      GAppInfo *info)
{
  SelectAppData *data;

  data = g_slice_new0 (SelectAppData);
  data->self = g_object_ref (self);
  data->info = g_object_ref (info);

  gtk_tree_model_foreach (GTK_TREE_MODEL (self->priv->store),
			  select_application_func_cb, data);

  select_app_data_free (data);
}

static void
other_application_dialog_response_cb (GtkDialog *dialog,
				      gint response_id,
				      gpointer user_data)
{
  GtkAppChooserComboBox *self = user_data;
  GAppInfo *info;

  if (response_id != GTK_RESPONSE_OK)
    {
      /* reset the active item, otherwise we are stuck on
       * 'Other application...'
       */
      gtk_combo_box_set_active (GTK_COMBO_BOX (self), 0);
      gtk_widget_destroy (GTK_WIDGET (dialog));
      return;
    }

  info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (dialog));

  /* refresh the combobox to get the new application */
  gtk_app_chooser_refresh (GTK_APP_CHOOSER (self));
  gtk_app_chooser_combo_box_select_application (self, info);

  g_object_unref (info);
}

static void
other_application_item_activated_cb (GtkAppChooserComboBox *self,
				     gpointer _user_data)
{
  GtkWidget *dialog, *widget;
  GtkWindow *toplevel;

  toplevel = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self)));
  dialog = gtk_app_chooser_dialog_new_for_content_type (toplevel, GTK_DIALOG_DESTROY_WITH_PARENT,
							self->priv->content_type);
  widget = gtk_app_chooser_dialog_get_widget (GTK_APP_CHOOSER_DIALOG (dialog));
  g_object_set (widget,
		"show-fallback", TRUE,
		"show-other", TRUE,
		NULL);
  gtk_widget_show (dialog);

  g_signal_connect (dialog, "response",
		    G_CALLBACK (other_application_dialog_response_cb), self);
}

static void
gtk_app_chooser_combo_box_ensure_dialog_item (GtkAppChooserComboBox *self,
					      GtkTreeIter *prev_iter)
{
  GIcon *icon;
  GtkTreeIter iter;

  if (!self->priv->show_dialog_item)
    return;

  icon = g_themed_icon_new ("application-x-executable");

  gtk_list_store_insert_after (self->priv->store, &iter, prev_iter);
  real_insert_separator (self, FALSE, &iter);
  *prev_iter = iter;

  gtk_list_store_insert_after (self->priv->store, &iter, prev_iter);
  real_insert_custom_item (self,
			   _("Other application..."), icon,
			   other_application_item_activated_cb,
			   NULL, FALSE, &iter);

  g_object_unref (icon);
}

static void
gtk_app_chooser_combo_box_populate (GtkAppChooserComboBox *self)
{
  GList *recommended_apps = NULL, *l;
  GAppInfo *app;
  GtkTreeIter iter, iter2;
  GIcon *icon;
  gboolean first;

  recommended_apps = g_app_info_get_recommended_for_type (self->priv->content_type);
  first = TRUE;

  for (l = recommended_apps; l != NULL; l = l->next)
    {
      app = l->data;

      icon = g_app_info_get_icon (app);

      if (icon == NULL)
        icon = g_themed_icon_new ("application-x-executable");
      else
        g_object_ref (icon);

      if (first)
        {
          get_first_iter (self->priv->store, &iter);
          first = FALSE;
        }
      else
        {
          gtk_list_store_insert_after (self->priv->store, &iter2, &iter);
          iter = iter2;
        }

      gtk_list_store_set (self->priv->store, &iter,
                          COLUMN_APP_INFO, app,
                          COLUMN_NAME, g_app_info_get_display_name (app),
                          COLUMN_ICON, icon,
                          COLUMN_CUSTOM, FALSE,
                          -1);

      g_object_unref (icon);
    }

  gtk_app_chooser_combo_box_ensure_dialog_item (self, &iter);
  gtk_combo_box_set_active (GTK_COMBO_BOX (self), 0);
}

static void
gtk_app_chooser_combo_box_build_ui (GtkAppChooserComboBox *self)
{
  GtkCellRenderer *cell;

  self->priv->store = gtk_list_store_new (NUM_COLUMNS,
                                          G_TYPE_APP_INFO,
                                          G_TYPE_STRING,
                                          G_TYPE_ICON,
                                          G_TYPE_BOOLEAN,
                                          G_TYPE_BOOLEAN,
                                          CUSTOM_COMBO_DATA_TYPE);

  gtk_combo_box_set_model (GTK_COMBO_BOX (self),
                           GTK_TREE_MODEL (self->priv->store));

  gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (self),
                                        row_separator_func, NULL, NULL);

  cell = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (self), cell, FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (self), cell,
                                  "gicon", COLUMN_ICON,
                                  NULL);

  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (self), cell, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (self), cell,
                                  "text", COLUMN_NAME,
                                  "xpad", 6,
                                  NULL);

  gtk_app_chooser_combo_box_populate (self);
}

static void
gtk_app_chooser_combo_box_remove_non_custom (GtkAppChooserComboBox *self)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gboolean custom, res;

  model = GTK_TREE_MODEL (self->priv->store);

  if (!gtk_tree_model_get_iter_first (model, &iter))
    return;

  do {
    gtk_tree_model_get (model, &iter,
                        COLUMN_CUSTOM, &custom,
                        -1);
    if (custom)
      res = gtk_tree_model_iter_next (model, &iter);
    else
      res = gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
  } while (res);
}

static void
gtk_app_chooser_combo_box_changed (GtkComboBox *object)
{
  GtkAppChooserComboBox *self = GTK_APP_CHOOSER_COMBO_BOX (object);
  GtkTreeIter iter;
  CustomAppComboData *custom_data = NULL;

  if (!gtk_combo_box_get_active_iter (object, &iter))
    return;

  gtk_tree_model_get (GTK_TREE_MODEL (self->priv->store), &iter,
                      COLUMN_CALLBACK, &custom_data,
                      -1);

  if (custom_data != NULL && custom_data->func != NULL)
    custom_data->func (self, custom_data->user_data);
}

static void
gtk_app_chooser_combo_box_refresh (GtkAppChooser *object)
{
  GtkAppChooserComboBox *self = GTK_APP_CHOOSER_COMBO_BOX (object);

  gtk_app_chooser_combo_box_remove_non_custom (self);
  gtk_app_chooser_combo_box_populate (self);
}

static GAppInfo *
gtk_app_chooser_combo_box_get_app_info (GtkAppChooser *object)
{
  GtkAppChooserComboBox *self = GTK_APP_CHOOSER_COMBO_BOX (object);
  GtkTreeIter iter;
  GAppInfo *info;

  if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (self), &iter))
    return NULL;

  gtk_tree_model_get (GTK_TREE_MODEL (self->priv->store), &iter,
                      COLUMN_APP_INFO, &info,
                      -1);

  return info;
}

static void
gtk_app_chooser_combo_box_constructed (GObject *obj)
{
  GtkAppChooserComboBox *self = GTK_APP_CHOOSER_COMBO_BOX (obj);

  if (G_OBJECT_CLASS (gtk_app_chooser_combo_box_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (gtk_app_chooser_combo_box_parent_class)->constructed (obj);

  g_assert (self->priv->content_type != NULL);

  gtk_app_chooser_combo_box_build_ui (self);
}

static void
gtk_app_chooser_combo_box_set_property (GObject      *obj,
                                        guint         property_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  GtkAppChooserComboBox *self = GTK_APP_CHOOSER_COMBO_BOX (obj);

  switch (property_id)
    {
    case PROP_CONTENT_TYPE:
      self->priv->content_type = g_value_dup_string (value);
      break;
    case PROP_SHOW_DIALOG_ITEM:
      gtk_app_chooser_combo_box_set_show_dialog_item (self, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
      break;
    }
}

static void
gtk_app_chooser_combo_box_get_property (GObject    *obj,
                                        guint       property_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GtkAppChooserComboBox *self = GTK_APP_CHOOSER_COMBO_BOX (obj);

  switch (property_id)
    {
    case PROP_CONTENT_TYPE:
      g_value_set_string (value, self->priv->content_type);
      break;
    case PROP_SHOW_DIALOG_ITEM:
      g_value_set_boolean (value, self->priv->show_dialog_item);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
      break;
    }
}

static void
gtk_app_chooser_combo_box_finalize (GObject *obj)
{
  GtkAppChooserComboBox *self = GTK_APP_CHOOSER_COMBO_BOX (obj);

  g_free (self->priv->content_type);

  G_OBJECT_CLASS (gtk_app_chooser_combo_box_parent_class)->finalize (obj);
}

static void
app_chooser_iface_init (GtkAppChooserIface *iface)
{
  iface->get_app_info = gtk_app_chooser_combo_box_get_app_info;
  iface->refresh = gtk_app_chooser_combo_box_refresh;
}

static void
gtk_app_chooser_combo_box_class_init (GtkAppChooserComboBoxClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GtkComboBoxClass *combo_class = GTK_COMBO_BOX_CLASS (klass);
  GParamSpec *pspec;

  oclass->set_property = gtk_app_chooser_combo_box_set_property;
  oclass->get_property = gtk_app_chooser_combo_box_get_property;
  oclass->finalize = gtk_app_chooser_combo_box_finalize;
  oclass->constructed = gtk_app_chooser_combo_box_constructed;

  combo_class->changed = gtk_app_chooser_combo_box_changed;

  g_object_class_override_property (oclass, PROP_CONTENT_TYPE, "content-type");

  pspec = g_param_spec_boolean ("show-dialog-item",
				P_("Include an 'Other...' item"),
				P_("Whether the combobox should include an item that triggers a GtkAppChooserDialog"),
				FALSE,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (oclass, PROP_SHOW_DIALOG_ITEM, pspec);

  g_type_class_add_private (klass, sizeof (GtkAppChooserComboBoxPrivate));
}

static void
gtk_app_chooser_combo_box_init (GtkAppChooserComboBox *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GTK_TYPE_APP_CHOOSER_COMBO_BOX,
                                            GtkAppChooserComboBoxPrivate);
}

static void
real_insert_custom_item (GtkAppChooserComboBox *self,
			 const gchar *label,
			 GIcon *icon,
			 GtkAppChooserComboBoxItemFunc func,
			 gpointer user_data,
			 gboolean custom,
			 GtkTreeIter *iter)
{
  CustomAppComboData *data;

  data = g_slice_new0 (CustomAppComboData);
  data->func = func;
  data->user_data = user_data;

  gtk_list_store_set (self->priv->store, iter,
		      COLUMN_NAME, label,
		      COLUMN_ICON, icon,
		      COLUMN_CALLBACK, data,
		      COLUMN_CUSTOM, custom,
		      COLUMN_SEPARATOR, FALSE,
		      -1);
}

static void
real_insert_separator (GtkAppChooserComboBox *self,
		       gboolean custom,
		       GtkTreeIter *iter)
{
  gtk_list_store_set (self->priv->store, iter,
		      COLUMN_CUSTOM, custom,
		      COLUMN_SEPARATOR, TRUE,
		      -1);
}

/**
 * gtk_app_chooser_combo_box_new:
 * @content_type: the content type to show applications for
 *
 * Creates a new #GtkAppChooserComboBox for applications
 * that can handle content of the given type.
 *
 * Returns: a newly created #GtkAppChooserComboBox
 *
 * Since: 3.0
 */
GtkWidget *
gtk_app_chooser_combo_box_new (const gchar *content_type)
{
  g_return_val_if_fail (content_type != NULL, NULL);

  return g_object_new (GTK_TYPE_APP_CHOOSER_COMBO_BOX,
                       "content-type", content_type,
                       NULL);
}

/**
 * gtk_app_chooser_combo_box_append_separator:
 * @self: a #GtkAppChooserComboBox
 *
 * Appends a separator to the list of applications that is shown
 * in the popup.
 *
 * Since: 3.0
 */
void
gtk_app_chooser_combo_box_append_separator (GtkAppChooserComboBox *self)
{
  GtkTreeIter iter;

  g_return_if_fail (GTK_IS_APP_CHOOSER_COMBO_BOX (self));

  gtk_list_store_append (self->priv->store, &iter);
  real_insert_separator (self, TRUE, &iter);
}

/**
 * gtk_app_chooser_combo_box_append_custom_item:
 * @self: a #GtkAppChooserComboBox
 * @label: the label for the custom item
 * @icon: the icon for the custom item
 * @func: callback to call if the item is activated
 * @user_data: user data for @func
 *
 * Appends a custom item to the list of applications that is shown
 * in the popup. See also gtk_app_chooser_combo_box_append_separator().
 *
 * Since: 3.0
 */
void
gtk_app_chooser_combo_box_append_custom_item (GtkAppChooserComboBox         *self,
                                              const gchar                   *label,
                                              GIcon                         *icon,
                                              GtkAppChooserComboBoxItemFunc  func,
                                              gpointer                       user_data)
{
  GtkTreeIter iter;

  g_return_if_fail (GTK_IS_APP_CHOOSER_COMBO_BOX (self));

  gtk_list_store_append (self->priv->store, &iter);
  real_insert_custom_item (self, label, icon,
                           func, user_data, TRUE, &iter);
}

gboolean
gtk_app_chooser_combo_box_get_show_dialog_item (GtkAppChooserComboBox *self)
{
  g_return_val_if_fail (GTK_IS_APP_CHOOSER_COMBO_BOX (self), FALSE);

  return self->priv->show_dialog_item;
}

void
gtk_app_chooser_combo_box_set_show_dialog_item (GtkAppChooserComboBox *self,
                                                gboolean setting)
{
  if (self->priv->show_dialog_item != setting)
    {
      self->priv->show_dialog_item = setting;

      g_object_notify (G_OBJECT (self), "show-dialog-item");

      gtk_app_chooser_refresh (GTK_APP_CHOOSER (self));
    }
}
