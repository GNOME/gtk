/*
 * gtkappchooserbutton.h: an app-chooser combobox
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Cosimo Cecchi <ccecchi@redhat.com>
 */

/**
 * SECTION:gtkappchooserbutton
 * @Title: GtkAppChooserButton
 * @Short_description: A button to launch an application chooser dialog
 *
 * The #GtkAppChooserButton is a widget that lets the user select
 * an application. It implements the #GtkAppChooser interface.
 *
 * Initially, a #GtkAppChooserButton selects the first application
 * in its list, which will either be the most-recently used application
 * or, if #GtkAppChooserButton:show-default-item is %TRUE, the
 * default application.
 *
 * The list of applications shown in a #GtkAppChooserButton includes
 * the recommended applications for the given content type. When
 * #GtkAppChooserButton:show-default-item is set, the default application
 * is also included. To let the user chooser other applications,
 * you can set the #GtkAppChooserButton:show-dialog-item property,
 * which allows to open a full #GtkAppChooserDialog.
 *
 * It is possible to add custom items to the list, using
 * gtk_app_chooser_button_append_custom_item(). These items cause
 * the #GtkAppChooserButton::custom-item-activated signal to be
 * emitted when they are selected.
 *
 * To track changes in the selected application, use the
 * #GtkComboBox::changed signal.
 */
#include "config.h"

#include "gtkappchooserbutton.h"

#include "gtkappchooser.h"
#include "gtkappchooserdialog.h"
#include "gtkappchooserprivate.h"
#include "gtkcelllayout.h"
#include "gtkcellrendererpixbuf.h"
#include "gtkcellrenderertext.h"
#include "gtkcombobox.h"
#include "gtkdialog.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"

enum {
  PROP_CONTENT_TYPE = 1,
  PROP_SHOW_DIALOG_ITEM,
  PROP_SHOW_DEFAULT_ITEM,
  PROP_HEADING
};

enum {
  SIGNAL_CUSTOM_ITEM_ACTIVATED,
  NUM_SIGNALS
};

enum {
  COLUMN_APP_INFO,
  COLUMN_NAME,
  COLUMN_LABEL,
  COLUMN_ICON,
  COLUMN_CUSTOM,
  COLUMN_SEPARATOR,
  NUM_COLUMNS,
};

#define CUSTOM_ITEM_OTHER_APP "gtk-internal-item-other-app"

static void app_chooser_iface_init (GtkAppChooserIface *iface);

static void real_insert_custom_item (GtkAppChooserButton *self,
                                     const gchar *name,
                                     const gchar *label,
                                     GIcon *icon,
                                     gboolean custom,
                                     GtkTreeIter *iter);

static void real_insert_separator (GtkAppChooserButton *self,
                                   gboolean custom,
                                   GtkTreeIter *iter);

static guint signals[NUM_SIGNALS] = { 0, };

struct _GtkAppChooserButtonPrivate {
  GtkListStore *store;

  gchar *content_type;
  gchar *heading;
  gint last_active;
  gboolean show_dialog_item;
  gboolean show_default_item;

  GHashTable *custom_item_names;
};

G_DEFINE_TYPE_WITH_CODE (GtkAppChooserButton, gtk_app_chooser_button, GTK_TYPE_COMBO_BOX,
                         G_ADD_PRIVATE (GtkAppChooserButton)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_APP_CHOOSER,
                                                app_chooser_iface_init));

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
  GtkAppChooserButton *self;
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
  gboolean result;

  gtk_tree_model_get (model, iter,
                      COLUMN_APP_INFO, &app,
                      COLUMN_CUSTOM, &custom,
                      -1);

  /* custom items are always after GAppInfos, so iterating further here
   * is just useless.
   */
  if (custom)
    result = TRUE;
  else if (g_app_info_equal (app, app_to_match))
    {
      gtk_combo_box_set_active_iter (GTK_COMBO_BOX (data->self), iter);
      result = TRUE;
    }
  else
    result = FALSE;

  g_object_unref (app);

  return result;
}

static void
gtk_app_chooser_button_select_application (GtkAppChooserButton *self,
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
  GtkAppChooserButton *self = user_data;
  GAppInfo *info;

  if (response_id != GTK_RESPONSE_OK)
    {
      /* reset the active item, otherwise we are stuck on
       * 'Other application…'
       */
      gtk_combo_box_set_active (GTK_COMBO_BOX (self), self->priv->last_active);
      gtk_widget_destroy (GTK_WIDGET (dialog));
      return;
    }

  info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (dialog));

  gtk_widget_destroy (GTK_WIDGET (dialog));

  /* refresh the combobox to get the new application */
  gtk_app_chooser_refresh (GTK_APP_CHOOSER (self));
  gtk_app_chooser_button_select_application (self, info);

  g_object_unref (info);
}

static void
other_application_item_activated_cb (GtkAppChooserButton *self)
{
  GtkWidget *dialog, *widget;
  GtkWindow *toplevel;

  toplevel = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self)));
  dialog = gtk_app_chooser_dialog_new_for_content_type (toplevel, GTK_DIALOG_DESTROY_WITH_PARENT,
                                                        self->priv->content_type);

  gtk_window_set_modal (GTK_WINDOW (dialog), gtk_window_get_modal (toplevel));
  gtk_app_chooser_dialog_set_heading (GTK_APP_CHOOSER_DIALOG (dialog),
                                      self->priv->heading);

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
gtk_app_chooser_button_ensure_dialog_item (GtkAppChooserButton *self,
                                           GtkTreeIter *prev_iter)
{
  GtkTreeIter iter, iter2;

  if (!self->priv->show_dialog_item || !self->priv->content_type)
    return;

  if (prev_iter == NULL)
    gtk_list_store_append (self->priv->store, &iter);
  else
    gtk_list_store_insert_after (self->priv->store, &iter, prev_iter);

  real_insert_separator (self, FALSE, &iter);
  iter2 = iter;

  gtk_list_store_insert_after (self->priv->store, &iter, &iter2);
  real_insert_custom_item (self, CUSTOM_ITEM_OTHER_APP,
                           _("Other application…"), NULL,
                           FALSE, &iter);
}

static void
insert_one_application (GtkAppChooserButton *self,
                        GAppInfo            *app,
                        GtkTreeIter         *iter)
{
  GIcon *icon;

  icon = g_app_info_get_icon (app);

  if (icon == NULL)
    icon = g_themed_icon_new ("application-x-executable");
  else
    g_object_ref (icon);

  gtk_list_store_set (self->priv->store, iter,
                      COLUMN_APP_INFO, app,
                      COLUMN_LABEL, g_app_info_get_name (app),
                      COLUMN_ICON, icon,
                      COLUMN_CUSTOM, FALSE,
                      -1);

  g_object_unref (icon);
}

static void
gtk_app_chooser_button_populate (GtkAppChooserButton *self)
{
  GList *recommended_apps = NULL, *l;
  GAppInfo *app, *default_app = NULL;
  GtkTreeIter iter, iter2;
  gboolean cycled_recommended;

#ifndef G_OS_WIN32
  if (self->priv->content_type)
    recommended_apps = g_app_info_get_recommended_for_type (self->priv->content_type);
#endif
  cycled_recommended = FALSE;

  if (self->priv->show_default_item)
    {
      if (self->priv->content_type)
        default_app = g_app_info_get_default_for_type (self->priv->content_type, FALSE);

      if (default_app != NULL)
        {
          get_first_iter (self->priv->store, &iter);
          cycled_recommended = TRUE;

          insert_one_application (self, default_app, &iter);

          g_object_unref (default_app);
        }
    }

  for (l = recommended_apps; l != NULL; l = l->next)
    {
      app = l->data;

      if (default_app != NULL && g_app_info_equal (app, default_app))
        continue;

      if (cycled_recommended)
        {
          gtk_list_store_insert_after (self->priv->store, &iter2, &iter);
          iter = iter2;
        }
      else
        {
          get_first_iter (self->priv->store, &iter);
          cycled_recommended = TRUE;
        }

      insert_one_application (self, app, &iter);
    }

  if (recommended_apps != NULL)
    g_list_free_full (recommended_apps, g_object_unref);

  if (!cycled_recommended)
    gtk_app_chooser_button_ensure_dialog_item (self, NULL);
  else
    gtk_app_chooser_button_ensure_dialog_item (self, &iter);

  gtk_combo_box_set_active (GTK_COMBO_BOX (self), 0);
}

static void
gtk_app_chooser_button_build_ui (GtkAppChooserButton *self)
{
  GtkCellRenderer *cell;
  GtkCellArea *area;

  gtk_combo_box_set_model (GTK_COMBO_BOX (self),
                           GTK_TREE_MODEL (self->priv->store));

  area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (self));

  gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (self),
                                        row_separator_func, NULL, NULL);

  cell = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_area_add_with_properties (area, cell,
                                     "align", FALSE,
                                     "expand", FALSE,
                                     "fixed-size", FALSE,
                                     NULL);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (self), cell,
                                  "gicon", COLUMN_ICON,
                                  NULL);

  cell = gtk_cell_renderer_text_new ();
  gtk_cell_area_add_with_properties (area, cell,
                                     "align", FALSE,
                                     "expand", TRUE,
                                     NULL);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (self), cell,
                                  "text", COLUMN_LABEL,
                                  NULL);

  gtk_app_chooser_button_populate (self);
}

static void
gtk_app_chooser_button_remove_non_custom (GtkAppChooserButton *self)
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
gtk_app_chooser_button_changed (GtkComboBox *object)
{
  GtkAppChooserButton *self = GTK_APP_CHOOSER_BUTTON (object);
  GtkTreeIter iter;
  gchar *name = NULL;
  gboolean custom;
  GQuark name_quark;

  if (!gtk_combo_box_get_active_iter (object, &iter))
    return;

  gtk_tree_model_get (GTK_TREE_MODEL (self->priv->store), &iter,
                      COLUMN_NAME, &name,
                      COLUMN_CUSTOM, &custom,
                      -1);

  if (name != NULL)
    {
      if (custom)
        {
          name_quark = g_quark_from_string (name);
          g_signal_emit (self, signals[SIGNAL_CUSTOM_ITEM_ACTIVATED], name_quark, name);
          self->priv->last_active = gtk_combo_box_get_active (object);
        }
      else
        {
          /* trigger the dialog internally */
          other_application_item_activated_cb (self);
        }

      g_free (name);
    }
  else
    self->priv->last_active = gtk_combo_box_get_active (object);
}

static void
gtk_app_chooser_button_refresh (GtkAppChooser *object)
{
  GtkAppChooserButton *self = GTK_APP_CHOOSER_BUTTON (object);

  gtk_app_chooser_button_remove_non_custom (self);
  gtk_app_chooser_button_populate (self);
}

static GAppInfo *
gtk_app_chooser_button_get_app_info (GtkAppChooser *object)
{
  GtkAppChooserButton *self = GTK_APP_CHOOSER_BUTTON (object);
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
gtk_app_chooser_button_constructed (GObject *obj)
{
  GtkAppChooserButton *self = GTK_APP_CHOOSER_BUTTON (obj);

  if (G_OBJECT_CLASS (gtk_app_chooser_button_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (gtk_app_chooser_button_parent_class)->constructed (obj);

  gtk_app_chooser_button_build_ui (self);
}

static void
gtk_app_chooser_button_set_property (GObject      *obj,
                                     guint         property_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkAppChooserButton *self = GTK_APP_CHOOSER_BUTTON (obj);

  switch (property_id)
    {
    case PROP_CONTENT_TYPE:
      self->priv->content_type = g_value_dup_string (value);
      break;
    case PROP_SHOW_DIALOG_ITEM:
      gtk_app_chooser_button_set_show_dialog_item (self, g_value_get_boolean (value));
      break;
    case PROP_SHOW_DEFAULT_ITEM:
      gtk_app_chooser_button_set_show_default_item (self, g_value_get_boolean (value));
      break;
    case PROP_HEADING:
      gtk_app_chooser_button_set_heading (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
      break;
    }
}

static void
gtk_app_chooser_button_get_property (GObject    *obj,
                                     guint       property_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GtkAppChooserButton *self = GTK_APP_CHOOSER_BUTTON (obj);

  switch (property_id)
    {
    case PROP_CONTENT_TYPE:
      g_value_set_string (value, self->priv->content_type);
      break;
    case PROP_SHOW_DIALOG_ITEM:
      g_value_set_boolean (value, self->priv->show_dialog_item);
      break;
    case PROP_SHOW_DEFAULT_ITEM:
      g_value_set_boolean (value, self->priv->show_default_item);
      break;
    case PROP_HEADING:
      g_value_set_string (value, self->priv->heading);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
      break;
    }
}

static void
gtk_app_chooser_button_finalize (GObject *obj)
{
  GtkAppChooserButton *self = GTK_APP_CHOOSER_BUTTON (obj);

  g_hash_table_destroy (self->priv->custom_item_names);
  g_free (self->priv->content_type);
  g_free (self->priv->heading);

  g_object_unref (self->priv->store);

  G_OBJECT_CLASS (gtk_app_chooser_button_parent_class)->finalize (obj);
}

static void
app_chooser_iface_init (GtkAppChooserIface *iface)
{
  iface->get_app_info = gtk_app_chooser_button_get_app_info;
  iface->refresh = gtk_app_chooser_button_refresh;
}

static void
gtk_app_chooser_button_class_init (GtkAppChooserButtonClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GtkComboBoxClass *combo_class = GTK_COMBO_BOX_CLASS (klass);
  GParamSpec *pspec;

  oclass->set_property = gtk_app_chooser_button_set_property;
  oclass->get_property = gtk_app_chooser_button_get_property;
  oclass->finalize = gtk_app_chooser_button_finalize;
  oclass->constructed = gtk_app_chooser_button_constructed;

  combo_class->changed = gtk_app_chooser_button_changed;

  g_object_class_override_property (oclass, PROP_CONTENT_TYPE, "content-type");

  /**
   * GtkAppChooserButton:show-dialog-item:
   *
   * The #GtkAppChooserButton:show-dialog-item property determines
   * whether the dropdown menu should show an item that triggers
   * a #GtkAppChooserDialog when clicked.
   */
  pspec =
    g_param_spec_boolean ("show-dialog-item",
                          P_("Include an 'Other…' item"),
                          P_("Whether the combobox should include an item that triggers a GtkAppChooserDialog"),
                          FALSE,
                          G_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);
  g_object_class_install_property (oclass, PROP_SHOW_DIALOG_ITEM, pspec);

  /**
   * GtkAppChooserButton:show-default-item:
   *
   * The #GtkAppChooserButton:show-default-item property determines
   * whether the dropdown menu should show the default application
   * on top for the provided content type.
   *
   * Since: 3.2
   */
  pspec =
    g_param_spec_boolean ("show-default-item",
                          P_("Show default item"),
                          P_("Whether the combobox should show the default application on top"),
                          FALSE,
                          G_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);
  g_object_class_install_property (oclass, PROP_SHOW_DEFAULT_ITEM, pspec);


  /**
   * GtkAppChooserButton:heading:
   *
   * The text to show at the top of the dialog that can be
   * opened from the button. The string may contain Pango markup.
   */
  pspec = g_param_spec_string ("heading",
                               P_("Heading"),
                               P_("The text to show at the top of the dialog"),
                               NULL,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);
  g_object_class_install_property (oclass, PROP_HEADING, pspec);


  /**
   * GtkAppChooserButton::custom-item-activated:
   * @self: the object which received the signal
   * @item_name: the name of the activated item
   *
   * Emitted when a custom item, previously added with
   * gtk_app_chooser_button_append_custom_item(), is activated from the
   * dropdown menu.
   */
  signals[SIGNAL_CUSTOM_ITEM_ACTIVATED] =
    g_signal_new ("custom-item-activated",
                  GTK_TYPE_APP_CHOOSER_BUTTON,
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_DETAILED,
                  G_STRUCT_OFFSET (GtkAppChooserButtonClass, custom_item_activated),
                  NULL, NULL,
                  _gtk_marshal_VOID__STRING,
                  G_TYPE_NONE,
                  1, G_TYPE_STRING);
}

static void
gtk_app_chooser_button_init (GtkAppChooserButton *self)
{
  self->priv = gtk_app_chooser_button_get_instance_private (self);
  self->priv->custom_item_names =
    g_hash_table_new_full (g_str_hash, g_str_equal,
                           g_free, NULL);

  self->priv->store = gtk_list_store_new (NUM_COLUMNS,
                                          G_TYPE_APP_INFO,
                                          G_TYPE_STRING, /* name */
                                          G_TYPE_STRING, /* label */
                                          G_TYPE_ICON,
                                          G_TYPE_BOOLEAN, /* separator */
                                          G_TYPE_BOOLEAN); /* custom */
}

static gboolean
app_chooser_button_iter_from_custom_name (GtkAppChooserButton *self,
                                          const gchar         *name,
                                          GtkTreeIter         *set_me)
{
  GtkTreeIter iter;
  gchar *custom_name = NULL;

  if (!gtk_tree_model_get_iter_first
      (GTK_TREE_MODEL (self->priv->store), &iter))
    return FALSE;

  do {
    gtk_tree_model_get (GTK_TREE_MODEL (self->priv->store), &iter,
                        COLUMN_NAME, &custom_name,
                        -1);

    if (g_strcmp0 (custom_name, name) == 0)
      {
        g_free (custom_name);
        *set_me = iter;

        return TRUE;
      }

    g_free (custom_name);
  } while (gtk_tree_model_iter_next (GTK_TREE_MODEL (self->priv->store), &iter));

  return FALSE;
}

static void
real_insert_custom_item (GtkAppChooserButton *self,
                         const gchar *name,
			 const gchar *label,
			 GIcon *icon,
			 gboolean custom,
			 GtkTreeIter *iter)
{
  if (custom)
    {
      if (g_hash_table_lookup (self->priv->custom_item_names,
                               name) != NULL)
        {
          g_warning ("Attempting to add custom item %s to GtkAppChooserButton, "
                     "when there's already an item with the same name", name);
          return;
        }

      g_hash_table_insert (self->priv->custom_item_names,
                           g_strdup (name), GINT_TO_POINTER (1));
    }

  gtk_list_store_set (self->priv->store, iter,
                      COLUMN_NAME, name,
                      COLUMN_LABEL, label,
                      COLUMN_ICON, icon,
                      COLUMN_CUSTOM, custom,
                      COLUMN_SEPARATOR, FALSE,
                      -1);
}

static void
real_insert_separator (GtkAppChooserButton *self,
		       gboolean custom,
		       GtkTreeIter *iter)
{
  gtk_list_store_set (self->priv->store, iter,
		      COLUMN_CUSTOM, custom,
		      COLUMN_SEPARATOR, TRUE,
		      -1);
}

/**
 * gtk_app_chooser_button_new:
 * @content_type: the content type to show applications for
 *
 * Creates a new #GtkAppChooserButton for applications
 * that can handle content of the given type.
 *
 * Returns: a newly created #GtkAppChooserButton
 *
 * Since: 3.0
 */
GtkWidget *
gtk_app_chooser_button_new (const gchar *content_type)
{
  g_return_val_if_fail (content_type != NULL, NULL);

  return g_object_new (GTK_TYPE_APP_CHOOSER_BUTTON,
                       "content-type", content_type,
                       NULL);
}

/**
 * gtk_app_chooser_button_append_separator:
 * @self: a #GtkAppChooserButton
 *
 * Appends a separator to the list of applications that is shown
 * in the popup.
 *
 * Since: 3.0
 */
void
gtk_app_chooser_button_append_separator (GtkAppChooserButton *self)
{
  GtkTreeIter iter;

  g_return_if_fail (GTK_IS_APP_CHOOSER_BUTTON (self));

  gtk_list_store_append (self->priv->store, &iter);
  real_insert_separator (self, TRUE, &iter);
}

/**
 * gtk_app_chooser_button_append_custom_item:
 * @self: a #GtkAppChooserButton
 * @name: the name of the custom item
 * @label: the label for the custom item
 * @icon: the icon for the custom item
 *
 * Appends a custom item to the list of applications that is shown
 * in the popup; the item name must be unique per-widget.
 * Clients can use the provided name as a detail for the
 * #GtkAppChooserButton::custom-item-activated signal, to add a
 * callback for the activation of a particular custom item in the list.
 * See also gtk_app_chooser_button_append_separator().
 *
 * Since: 3.0
 */
void
gtk_app_chooser_button_append_custom_item (GtkAppChooserButton *self,
                                           const gchar         *name,
                                           const gchar         *label,
                                           GIcon               *icon)
{
  GtkTreeIter iter;

  g_return_if_fail (GTK_IS_APP_CHOOSER_BUTTON (self));
  g_return_if_fail (name != NULL);

  gtk_list_store_append (self->priv->store, &iter);
  real_insert_custom_item (self, name, label, icon, TRUE, &iter);
}

/**
 * gtk_app_chooser_button_set_active_custom_item:
 * @self: a #GtkAppChooserButton
 * @name: the name of the custom item
 *
 * Selects a custom item previously added with
 * gtk_app_chooser_button_append_custom_item().
 *
 * Use gtk_app_chooser_refresh() to bring the selection
 * to its initial state.
 *
 * Since: 3.0
 */
void
gtk_app_chooser_button_set_active_custom_item (GtkAppChooserButton *self,
                                               const gchar         *name)
{
  GtkTreeIter iter;

  g_return_if_fail (GTK_IS_APP_CHOOSER_BUTTON (self));
  g_return_if_fail (name != NULL);

  if (g_hash_table_lookup (self->priv->custom_item_names, name) == NULL ||
      !app_chooser_button_iter_from_custom_name (self, name, &iter))
    {
      g_warning ("Can't find the item named %s in the app chooser.",
                 name);
      return;
    }

  gtk_combo_box_set_active_iter (GTK_COMBO_BOX (self), &iter);
}

/**
 * gtk_app_chooser_button_get_show_dialog_item:
 * @self: a #GtkAppChooserButton
 *
 * Returns the current value of the #GtkAppChooserButton:show-dialog-item
 * property.
 *
 * Returns: the value of #GtkAppChooserButton:show-dialog-item
 *
 * Since: 3.0
 */
gboolean
gtk_app_chooser_button_get_show_dialog_item (GtkAppChooserButton *self)
{
  g_return_val_if_fail (GTK_IS_APP_CHOOSER_BUTTON (self), FALSE);

  return self->priv->show_dialog_item;
}

/**
 * gtk_app_chooser_button_set_show_dialog_item:
 * @self: a #GtkAppChooserButton
 * @setting: the new value for #GtkAppChooserButton:show-dialog-item
 *
 * Sets whether the dropdown menu of this button should show an
 * entry to trigger a #GtkAppChooserDialog.
 *
 * Since: 3.0
 */
void
gtk_app_chooser_button_set_show_dialog_item (GtkAppChooserButton *self,
                                             gboolean             setting)
{
  if (self->priv->show_dialog_item != setting)
    {
      self->priv->show_dialog_item = setting;

      g_object_notify (G_OBJECT (self), "show-dialog-item");

      gtk_app_chooser_refresh (GTK_APP_CHOOSER (self));
    }
}

/**
 * gtk_app_chooser_button_get_show_default_item:
 * @self: a #GtkAppChooserButton
 *
 * Returns the current value of the #GtkAppChooserButton:show-default-item
 * property.
 *
 * Returns: the value of #GtkAppChooserButton:show-default-item
 *
 * Since: 3.2
 */
gboolean
gtk_app_chooser_button_get_show_default_item (GtkAppChooserButton *self)
{
  g_return_val_if_fail (GTK_IS_APP_CHOOSER_BUTTON (self), FALSE);

  return self->priv->show_default_item;
}

/**
 * gtk_app_chooser_button_set_show_default_item:
 * @self: a #GtkAppChooserButton
 * @setting: the new value for #GtkAppChooserButton:show-default-item
 *
 * Sets whether the dropdown menu of this button should show the
 * default application for the given content type at top.
 *
 * Since: 3.2
 */
void
gtk_app_chooser_button_set_show_default_item (GtkAppChooserButton *self,
                                              gboolean             setting)
{
  if (self->priv->show_default_item != setting)
    {
      self->priv->show_default_item = setting;

      g_object_notify (G_OBJECT (self), "show-default-item");

      gtk_app_chooser_refresh (GTK_APP_CHOOSER (self));
    }
}

/**
 * gtk_app_chooser_button_set_heading:
 * @self: a #GtkAppChooserButton
 * @heading: a string containing Pango markup
 *
 * Sets the text to display at the top of the dialog.
 * If the heading is not set, the dialog displays a default text.
 */
void
gtk_app_chooser_button_set_heading (GtkAppChooserButton *self,
                                    const gchar         *heading)
{
  g_return_if_fail (GTK_IS_APP_CHOOSER_BUTTON (self));

  g_free (self->priv->heading);
  self->priv->heading = g_strdup (heading);

  g_object_notify (G_OBJECT (self), "heading");
}

/**
 * gtk_app_chooser_button_get_heading:
 * @self: a #GtkAppChooserButton
 *
 * Returns the text to display at the top of the dialog.
 *
 * Returns: the text to display at the top of the dialog,
 *     or %NULL, in which case a default text is displayed
 */
const gchar *
gtk_app_chooser_button_get_heading (GtkAppChooserButton *self)
{
  g_return_val_if_fail (GTK_IS_APP_CHOOSER_BUTTON (self), NULL);

  return self->priv->heading;
}
