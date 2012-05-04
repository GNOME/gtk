/*
 * gtkappchooserdialog.c: an app-chooser dialog
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

/**
 * SECTION:gtkappchooserdialog
 * @Title: GtkAppChooserDialog
 * @Short_description: An application chooser dialog
 *
 * #GtkAppChooserDialog shows a #GtkAppChooserWidget inside a #GtkDialog.
 *
 * Note that #GtkAppChooserDialog does not have any interesting methods
 * of its own. Instead, you should get the embedded #GtkAppChooserWidget
 * using gtk_app_chooser_dialog_get_widget() and call its methods if
 * the generic #GtkAppChooser interface is not sufficient for your needs.
 *
 * To set the heading that is shown above the #GtkAppChooserWidget,
 * use gtk_app_chooser_dialog_set_heading().
 */
#include "config.h"

#include "gtkappchooserdialog.h"

#include "gtkintl.h"
#include "gtkappchooser.h"
#include "gtkappchooseronline.h"
#include "gtkappchooserprivate.h"
#include "gtkappchooserprivate.h"

#include "gtkmessagedialog.h"
#include "gtklabel.h"
#include "gtkbbox.h"
#include "gtkbutton.h"
#include "gtkmenuitem.h"
#include "gtkstock.h"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <gio/gio.h>

#define sure_string(s) ((const char *) ((s) != NULL ? (s) : ""))

struct _GtkAppChooserDialogPrivate {
  char *content_type;
  GFile *gfile;
  char *heading;

  GtkWidget *label;
  GtkWidget *button;
  GtkWidget *online_button;

  GtkWidget *open_label;

  GtkWidget *app_chooser_widget;
  GtkWidget *show_more_button;

  GtkAppChooserOnline *online;
  GCancellable *online_cancellable;

  gboolean show_more_clicked;
  gboolean dismissed;
};

enum {
  PROP_GFILE = 1,
  PROP_CONTENT_TYPE,
  PROP_HEADING
};

static void gtk_app_chooser_dialog_iface_init (GtkAppChooserIface *iface);
G_DEFINE_TYPE_WITH_CODE (GtkAppChooserDialog, gtk_app_chooser_dialog, GTK_TYPE_DIALOG,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_APP_CHOOSER,
                                                gtk_app_chooser_dialog_iface_init));

static void
show_error_dialog (const gchar *primary,
                   const gchar *secondary,
                   GtkWindow   *parent)
{
  GtkWidget *message_dialog;

  message_dialog = gtk_message_dialog_new (parent, 0,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_OK,
                                           NULL);
  g_object_set (message_dialog,
                "text", primary,
                "secondary-text", secondary,
                NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (message_dialog), GTK_RESPONSE_OK);

  gtk_widget_show (message_dialog);

  g_signal_connect (message_dialog, "response",
                    G_CALLBACK (gtk_widget_destroy), NULL);
}

static void
search_for_mimetype_ready_cb (GObject      *source,
                              GAsyncResult *res,
                              gpointer      user_data)
{
  GtkAppChooserOnline *online = GTK_APP_CHOOSER_ONLINE (source);
  GtkAppChooserDialog *self = user_data;
  GError *error = NULL;

  gdk_threads_enter ();

  _gtk_app_chooser_online_search_for_mimetype_finish (online, res, &error);

  if (self->priv->dismissed)
    goto out;

  if (error != NULL &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      show_error_dialog (_("Failed to look for applications online"),
                         error->message, GTK_WINDOW (self));
    }
  else
    {
      gtk_widget_set_sensitive (self->priv->online_button, TRUE);
      gtk_app_chooser_refresh (GTK_APP_CHOOSER (self->priv->app_chooser_widget));
    }

 out:
  g_clear_object (&self->priv->online_cancellable);
  g_clear_error (&error);
  g_object_unref (self);

  gdk_threads_leave ();
}

static void
online_button_clicked_cb (GtkButton *b,
                          gpointer   user_data)
{
  GtkAppChooserDialog *self = user_data;

  self->priv->online_cancellable = g_cancellable_new ();
  gtk_widget_set_sensitive (self->priv->online_button, FALSE);

  _gtk_app_chooser_online_search_for_mimetype_async (self->priv->online,
						     self->priv->content_type,
						     GTK_WINDOW (self),
                                                     self->priv->online_cancellable,
						     search_for_mimetype_ready_cb,
						     g_object_ref (self));
}

static void
app_chooser_online_get_default_ready_cb (GObject *source,
                                         GAsyncResult *res,
                                         gpointer user_data)
{
  GtkAppChooserDialog *self = user_data;

  gdk_threads_enter ();

  self->priv->online = _gtk_app_chooser_online_get_default_finish (source, res);

  if (self->priv->online != NULL &&
      !self->priv->dismissed)
    {
      GtkWidget *action_area;

      action_area = gtk_dialog_get_action_area (GTK_DIALOG (self));
      self->priv->online_button = gtk_button_new_with_mnemonic (_("_Find applications online"));
      gtk_box_pack_start (GTK_BOX (action_area), self->priv->online_button,
                          FALSE, FALSE, 0);
      gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (action_area), self->priv->online_button,
                                          TRUE);
      g_signal_connect (self->priv->online_button, "clicked",
                        G_CALLBACK (online_button_clicked_cb), self);


      if (!self->priv->content_type)
	gtk_widget_set_sensitive (self->priv->online_button, FALSE);

      gtk_widget_show (self->priv->online_button);
    }

  g_object_unref (self);

  gdk_threads_leave ();
}

static void
ensure_online_button (GtkAppChooserDialog *self)
{
  _gtk_app_chooser_online_get_default_async (app_chooser_online_get_default_ready_cb,
                                             g_object_ref (self));
}

static void
add_or_find_application (GtkAppChooserDialog *self)
{
  GAppInfo *app;

  app = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (self));

  if (app)
    {
      /* we don't care about reporting errors here */
      if (self->priv->content_type)
        g_app_info_set_as_last_used_for_type (app,
                                              self->priv->content_type,
                                              NULL);
      g_object_unref (app);
    }
}

static void
cancel_and_clear_cancellable (GtkAppChooserDialog *self)
{                                                               
  if (self->priv->online_cancellable != NULL)
    {
      g_cancellable_cancel (self->priv->online_cancellable);
      g_clear_object (&self->priv->online_cancellable);
    }
}

static void
gtk_app_chooser_dialog_response (GtkDialog *dialog,
                                 gint       response_id,
                                 gpointer   user_data)
{
  GtkAppChooserDialog *self = GTK_APP_CHOOSER_DIALOG (dialog);

  switch (response_id)
    {
    case GTK_RESPONSE_OK:
      add_or_find_application (self);
      break;
    case GTK_RESPONSE_CANCEL:
    case GTK_RESPONSE_DELETE_EVENT:
      cancel_and_clear_cancellable (self);
      self->priv->dismissed = TRUE;
    default :
      break;
    }
}

static void
widget_application_selected_cb (GtkAppChooserWidget *widget,
                                GAppInfo            *app_info,
                                gpointer             user_data)
{
  GtkAppChooserDialog *self = user_data;

  gtk_widget_set_sensitive (self->priv->button, TRUE);
}

static void
widget_application_activated_cb (GtkAppChooserWidget *widget,
                                 GAppInfo            *app_info,
                                 gpointer             user_data)
{
  GtkAppChooserDialog *self = user_data;

  gtk_dialog_response (GTK_DIALOG (self), GTK_RESPONSE_OK);
}

static char *
get_extension (const char *basename)
{
  char *p;

  p = strrchr (basename, '.');

  if (p && *(p + 1) != '\0')
    return g_strdup (p + 1);

  return NULL;
}

static void
set_dialog_properties (GtkAppChooserDialog *self)
{
  gchar *label;
  gchar *name;
  gchar *extension;
  gchar *description;
  gchar *default_text;
  gchar *string;
  gboolean unknown;
  PangoFontDescription *font_desc;

  name = NULL;
  extension = NULL;
  label = NULL;
  description = NULL;
  unknown = TRUE;

  if (self->priv->gfile != NULL)
    {
      name = g_file_get_basename (self->priv->gfile);
      extension = get_extension (name);
    }

  if (self->priv->content_type)
    {
      description = g_content_type_get_description (self->priv->content_type);
      unknown = g_content_type_is_unknown (self->priv->content_type);
    }

  gtk_window_set_title (GTK_WINDOW (self), "");

  if (name != NULL)
    {
      /* Translators: %s is a filename */
      label = g_strdup_printf (_("Select an application to open \"%s\""), name);
      string = g_strdup_printf (_("No applications available to open \"%s\""),
                                name);
    }
  else
    {
      /* Translators: %s is a file type description */
      label = g_strdup_printf (_("Select an application for \"%s\" files"),
                               unknown ? self->priv->content_type : description);
      string = g_strdup_printf (_("No applications available to open \"%s\" files"),
                               unknown ? self->priv->content_type : description);
    }

  font_desc = pango_font_description_new ();
  pango_font_description_set_weight (font_desc, PANGO_WEIGHT_BOLD);
  gtk_widget_override_font (self->priv->label, font_desc);
  pango_font_description_free (font_desc);

  if (self->priv->heading != NULL)
    gtk_label_set_markup (GTK_LABEL (self->priv->label), self->priv->heading);
  else
    gtk_label_set_markup (GTK_LABEL (self->priv->label), label);

  default_text = g_strdup_printf ("<big><b>%s</b></big>\n%s",
                                  string,
                                  _("Click \"Show other applications\", for more options, or "
                                    "\"Find applications online\" to install a new application"));

  gtk_app_chooser_widget_set_default_text (GTK_APP_CHOOSER_WIDGET (self->priv->app_chooser_widget),
                                           default_text);

  g_free (label);
  g_free (name);
  g_free (extension);
  g_free (description);
  g_free (string);
  g_free (default_text);
}

static void
show_more_button_clicked_cb (GtkButton *button,
                             gpointer   user_data)
{
  GtkAppChooserDialog *self = user_data;

  g_object_set (self->priv->app_chooser_widget,
                "show-recommended", TRUE,
                "show-fallback", TRUE,
                "show-other", TRUE,
                NULL);

  gtk_widget_hide (self->priv->show_more_button);
  self->priv->show_more_clicked = TRUE;
}

static void
widget_notify_for_button_cb (GObject    *source,
                             GParamSpec *pspec,
                             gpointer    user_data)
{
  GtkAppChooserDialog *self = user_data;
  GtkAppChooserWidget *widget = GTK_APP_CHOOSER_WIDGET (source);
  gboolean should_hide;

  should_hide = gtk_app_chooser_widget_get_show_other (widget) ||
    self->priv->show_more_clicked;

  if (should_hide)
    gtk_widget_hide (self->priv->show_more_button);
}

static void
forget_menu_item_activate_cb (GtkMenuItem *item,
                              gpointer     user_data)
{
  GtkAppChooserDialog *self = user_data;
  GAppInfo *info;

  info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (self));

  if (info != NULL)
    {
      g_app_info_remove_supports_type (info, self->priv->content_type, NULL);

      gtk_app_chooser_refresh (GTK_APP_CHOOSER (self));

      g_object_unref (info);
    }
}

static GtkWidget *
build_forget_menu_item (GtkAppChooserDialog *self)
{
  GtkWidget *retval;

  retval = gtk_menu_item_new_with_label (_("Forget association"));
  gtk_widget_show (retval);

  g_signal_connect (retval, "activate",
                    G_CALLBACK (forget_menu_item_activate_cb), self);

  return retval;
}

static void
widget_populate_popup_cb (GtkAppChooserWidget *widget,
                          GtkMenu             *menu,
                          GAppInfo            *info,
                          gpointer             user_data)
{
  GtkAppChooserDialog *self = user_data;
  GtkWidget *menu_item;

  if (g_app_info_can_remove_supports_type (info))
    {
      menu_item = build_forget_menu_item (self);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
    }
}

static void
build_dialog_ui (GtkAppChooserDialog *self)
{
  GtkWidget *vbox;
  GtkWidget *vbox2;
  GtkWidget *button, *w;
  GAppInfo *info;

  gtk_container_set_border_width (GTK_CONTAINER (self), 5);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (self))), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_pack_start (GTK_BOX (vbox), vbox2, TRUE, TRUE, 0);
  gtk_widget_show (vbox2);

  self->priv->label = gtk_label_new ("");
  gtk_widget_set_halign (self->priv->label, GTK_ALIGN_START);
  gtk_widget_set_valign (self->priv->label, GTK_ALIGN_CENTER);
  gtk_label_set_line_wrap (GTK_LABEL (self->priv->label), TRUE);
  gtk_box_pack_start (GTK_BOX (vbox2), self->priv->label,
                      FALSE, FALSE, 0);
  gtk_widget_show (self->priv->label);

  self->priv->app_chooser_widget =
    gtk_app_chooser_widget_new (self->priv->content_type);
  gtk_box_pack_start (GTK_BOX (vbox2), self->priv->app_chooser_widget, TRUE, TRUE, 0);
  gtk_widget_show (self->priv->app_chooser_widget);

  g_signal_connect (self->priv->app_chooser_widget, "application-selected",
                    G_CALLBACK (widget_application_selected_cb), self);
  g_signal_connect (self->priv->app_chooser_widget, "application-activated",
                    G_CALLBACK (widget_application_activated_cb), self);
  g_signal_connect (self->priv->app_chooser_widget, "notify::show-other",
                    G_CALLBACK (widget_notify_for_button_cb), self);
  g_signal_connect (self->priv->app_chooser_widget, "populate-popup",
                    G_CALLBACK (widget_populate_popup_cb), self);

  button = gtk_button_new_with_label (_("Show other applications"));
  self->priv->show_more_button = button;
  w = gtk_image_new_from_stock (GTK_STOCK_ADD,
                                GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (GTK_BUTTON (button), w);
  gtk_box_pack_start (GTK_BOX (self->priv->app_chooser_widget), button, FALSE, FALSE, 6);
  gtk_widget_show_all (button);

  g_signal_connect (button, "clicked",
                    G_CALLBACK (show_more_button_clicked_cb), self);

  gtk_dialog_add_button (GTK_DIALOG (self),
                         GTK_STOCK_CANCEL,
                         GTK_RESPONSE_CANCEL);

  self->priv->button = gtk_dialog_add_button (GTK_DIALOG (self),
                                              _("_Select"),
                                              GTK_RESPONSE_OK);

  info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (self->priv->app_chooser_widget));
  gtk_widget_set_sensitive (self->priv->button, info != NULL);
  if (info)
    g_object_unref (info);

  gtk_dialog_set_default_response (GTK_DIALOG (self),
                                   GTK_RESPONSE_OK);
}

static void
set_gfile_and_content_type (GtkAppChooserDialog *self,
                            GFile *file)
{
  GFileInfo *info;

  if (file == NULL)
    return;

  self->priv->gfile = g_object_ref (file);

  info = g_file_query_info (self->priv->gfile,
                            G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                            0, NULL, NULL);
  self->priv->content_type = g_strdup (g_file_info_get_content_type (info));

  g_object_unref (info);
}

static GAppInfo *
gtk_app_chooser_dialog_get_app_info (GtkAppChooser *object)
{
  GtkAppChooserDialog *self = GTK_APP_CHOOSER_DIALOG (object);
  return gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (self->priv->app_chooser_widget));
}

static void
gtk_app_chooser_dialog_refresh (GtkAppChooser *object)
{
  GtkAppChooserDialog *self = GTK_APP_CHOOSER_DIALOG (object);

  gtk_app_chooser_refresh (GTK_APP_CHOOSER (self->priv->app_chooser_widget));
}

static void
gtk_app_chooser_dialog_constructed (GObject *object)
{
  GtkAppChooserDialog *self = GTK_APP_CHOOSER_DIALOG (object);

  if (G_OBJECT_CLASS (gtk_app_chooser_dialog_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (gtk_app_chooser_dialog_parent_class)->constructed (object);

  build_dialog_ui (self);
  set_dialog_properties (self);
  ensure_online_button (self);
}

static void
gtk_app_chooser_dialog_dispose (GObject *object)
{
  GtkAppChooserDialog *self = GTK_APP_CHOOSER_DIALOG (object);
  
  g_clear_object (&self->priv->gfile);
  cancel_and_clear_cancellable (self);
  g_clear_object (&self->priv->online);

  G_OBJECT_CLASS (gtk_app_chooser_dialog_parent_class)->dispose (object);
}

static void
gtk_app_chooser_dialog_finalize (GObject *object)
{
  GtkAppChooserDialog *self = GTK_APP_CHOOSER_DIALOG (object);

  g_free (self->priv->content_type);

  G_OBJECT_CLASS (gtk_app_chooser_dialog_parent_class)->finalize (object);
}

static void
gtk_app_chooser_dialog_set_property (GObject      *object,
                                     guint         property_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkAppChooserDialog *self = GTK_APP_CHOOSER_DIALOG (object);

  switch (property_id)
    {
    case PROP_GFILE:
      set_gfile_and_content_type (self, g_value_get_object (value));
      break;
    case PROP_CONTENT_TYPE:
      /* don't try to override a value previously set with the GFile */
      if (self->priv->content_type == NULL)
        self->priv->content_type = g_value_dup_string (value);
      break;
    case PROP_HEADING:
      gtk_app_chooser_dialog_set_heading (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_app_chooser_dialog_get_property (GObject    *object,
                                     guint       property_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GtkAppChooserDialog *self = GTK_APP_CHOOSER_DIALOG (object);

  switch (property_id)
    {
    case PROP_GFILE:
      if (self->priv->gfile != NULL)
        g_value_set_object (value, self->priv->gfile);
      break;
    case PROP_CONTENT_TYPE:
      g_value_set_string (value, self->priv->content_type);
      break;
    case PROP_HEADING:
      g_value_set_string (value, self->priv->heading);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_app_chooser_dialog_iface_init (GtkAppChooserIface *iface)
{
  iface->get_app_info = gtk_app_chooser_dialog_get_app_info;
  iface->refresh = gtk_app_chooser_dialog_refresh;
}

static void
gtk_app_chooser_dialog_class_init (GtkAppChooserDialogClass *klass)
{
  GObjectClass *gobject_class;
  GParamSpec *pspec;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = gtk_app_chooser_dialog_dispose;
  gobject_class->finalize = gtk_app_chooser_dialog_finalize;
  gobject_class->set_property = gtk_app_chooser_dialog_set_property;
  gobject_class->get_property = gtk_app_chooser_dialog_get_property;
  gobject_class->constructed = gtk_app_chooser_dialog_constructed;

  g_object_class_override_property (gobject_class, PROP_CONTENT_TYPE, "content-type");

  /**
   * GtkAppChooserDialog:gfile:
   *
   * The GFile used by the #GtkAppChooserDialog.
   * The dialog's #GtkAppChooserWidget content type will be guessed from the
   * file, if present.
   */
  pspec = g_param_spec_object ("gfile",
                               P_("GFile"),
                               P_("The GFile used by the app chooser dialog"),
                               G_TYPE_FILE,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                               G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class, PROP_GFILE, pspec);

  /**
   * GtkAppChooserDialog:heading:
   *
   * The text to show at the top of the dialog.
   * The string may contain Pango markup.
   */
  pspec = g_param_spec_string ("heading",
                               P_("Heading"),
                               P_("The text to show at the top of the dialog"),
                               NULL,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class, PROP_HEADING, pspec);


  g_type_class_add_private (klass, sizeof (GtkAppChooserDialogPrivate));
}

static void
gtk_app_chooser_dialog_init (GtkAppChooserDialog *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GTK_TYPE_APP_CHOOSER_DIALOG,
                                            GtkAppChooserDialogPrivate);

  /* we can't override the class signal handler here, as it's a RUN_LAST;
   * we want our signal handler instead to be executed before any user code.
   */
  g_signal_connect (self, "response",
                    G_CALLBACK (gtk_app_chooser_dialog_response), NULL);
}

static void
set_parent_and_flags (GtkWidget      *dialog,
                      GtkWindow      *parent,
                      GtkDialogFlags  flags)
{
  if (parent != NULL)
    gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);

  if (flags & GTK_DIALOG_MODAL)
    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

  if (flags & GTK_DIALOG_DESTROY_WITH_PARENT)
    gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
}

/**
 * gtk_app_chooser_dialog_new:
 * @parent: (allow-none): a #GtkWindow, or %NULL
 * @flags: flags for this dialog
 * @file: a #GFile
 *
 * Creates a new #GtkAppChooserDialog for the provided #GFile,
 * to allow the user to select an application for it.
 *
 * Returns: a newly created #GtkAppChooserDialog
 *
 * Since: 3.0
 **/
GtkWidget *
gtk_app_chooser_dialog_new (GtkWindow      *parent,
                            GtkDialogFlags  flags,
                            GFile          *file)
{
  GtkWidget *retval;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  retval = g_object_new (GTK_TYPE_APP_CHOOSER_DIALOG,
                         "gfile", file,
                         NULL);

  set_parent_and_flags (retval, parent, flags);

  return retval;
}

/**
 * gtk_app_chooser_dialog_new_for_content_type:
 * @parent: (allow-none): a #GtkWindow, or %NULL
 * @flags: flags for this dialog
 * @content_type: a content type string
 *
 * Creates a new #GtkAppChooserDialog for the provided content type,
 * to allow the user to select an application for it.
 *
 * Returns: a newly created #GtkAppChooserDialog
 *
 * Since: 3.0
 **/
GtkWidget *
gtk_app_chooser_dialog_new_for_content_type (GtkWindow      *parent,
                                             GtkDialogFlags  flags,
                                             const gchar    *content_type)
{
  GtkWidget *retval;

  g_return_val_if_fail (content_type != NULL, NULL);

  retval = g_object_new (GTK_TYPE_APP_CHOOSER_DIALOG,
                         "content-type", content_type,
                         NULL);

  set_parent_and_flags (retval, parent, flags);

  return retval;
}

/**
 * gtk_app_chooser_dialog_get_widget:
 * @self: a #GtkAppChooserDialog
 *
 * Returns the #GtkAppChooserWidget of this dialog.
 *
 * Returns: (transfer none): the #GtkAppChooserWidget of @self
 *
 * Since: 3.0
 */
GtkWidget *
gtk_app_chooser_dialog_get_widget (GtkAppChooserDialog *self)
{
  g_return_val_if_fail (GTK_IS_APP_CHOOSER_DIALOG (self), NULL);

  return self->priv->app_chooser_widget;
}

/**
 * gtk_app_chooser_dialog_set_heading:
 * @self: a #GtkAppChooserDialog
 * @heading: a string containing Pango markup
 *
 * Sets the text to display at the top of the dialog.
 * If the heading is not set, the dialog displays a default text.
 */
void
gtk_app_chooser_dialog_set_heading (GtkAppChooserDialog *self,
                                    const gchar         *heading)
{
  g_return_if_fail (GTK_IS_APP_CHOOSER_DIALOG (self));

  g_free (self->priv->heading);
  self->priv->heading = g_strdup (heading);

  if (self->priv->label && self->priv->heading)
    gtk_label_set_markup (GTK_LABEL (self->priv->label), self->priv->heading);

  g_object_notify (G_OBJECT (self), "heading");
}

/**
 * gtk_app_chooser_dialog_get_heading:
 * @self: a #GtkAppChooserDialog
 *
 * Returns the text to display at the top of the dialog.
 *
 * Returns: the text to display at the top of the dialog, or %NULL, in which
 *     case a default text is displayed
 */
const gchar *
gtk_app_chooser_dialog_get_heading (GtkAppChooserDialog *self)
{
  g_return_val_if_fail (GTK_IS_APP_CHOOSER_DIALOG (self), NULL);

  return self->priv->heading;
}
