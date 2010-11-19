/*
 * gtkopenwithdialog.c: an open-with dialog
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

#include "gtkopenwithdialog.h"

#include "gtkintl.h"
#include "gtkopenwith.h"
#include "gtkopenwithonline.h"
#include "gtkopenwithprivate.h"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

#define sure_string(s) ((const char *) ((s) != NULL ? (s) : ""))

struct _GtkOpenWithDialogPrivate {
  char *content_type;
  GFile *gfile;

  GtkWidget *label;
  GtkWidget *button;
  GtkWidget *online_button;

  GtkWidget *open_label;

  GtkWidget *open_with_widget;
};

enum {
  PROP_GFILE = 1,
  PROP_CONTENT_TYPE,
  N_PROPERTIES
};

static void gtk_open_with_dialog_iface_init (GtkOpenWithIface *iface);
G_DEFINE_TYPE_WITH_CODE (GtkOpenWithDialog, gtk_open_with_dialog, GTK_TYPE_DIALOG,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_OPEN_WITH,
						gtk_open_with_dialog_iface_init));

static void
show_error_dialog (const gchar *primary,
		   const gchar *secondary,
		   GtkWindow *parent)
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
search_for_mimetype_ready_cb (GObject *source,
			      GAsyncResult *res,
			      gpointer user_data)
{
  GtkOpenWithOnline *online = GTK_OPEN_WITH_ONLINE (source);
  GtkOpenWithDialog *self = user_data;
  GError *error = NULL;

  gtk_open_with_online_search_for_mimetype_finish (online, res, &error);

  if (error != NULL)
    {
      show_error_dialog (_("Failed to look for applications online"),
			 error->message, GTK_WINDOW (self));
      g_error_free (error);
    }
  else
    {
      _gtk_open_with_widget_refilter (GTK_OPEN_WITH_WIDGET (self->priv->open_with_widget));
    }

  g_object_unref (online);
}

static void
online_button_clicked_cb (GtkButton *b,
			  gpointer user_data)
{
  GtkOpenWithOnline *online;
  GtkOpenWithDialog *self = user_data;

  online = gtk_open_with_online_get_default ();

  gtk_open_with_online_search_for_mimetype_async (online,
						  self->priv->content_type,
						  GTK_WINDOW (self),
						  search_for_mimetype_ready_cb,
						  self);
}

/* An application is valid if:
 *
 * 1) The file exists
 * 2) The user has permissions to run the file
 */
static gboolean
check_application (GtkOpenWithDialog *self,
		   GAppInfo **app_out)
{
  const char *command;
  char *path = NULL;
  char **argv = NULL;
  int argc;
  GError *error = NULL;
  gint retval = TRUE;
  GAppInfo *info;

  command = NULL;

  info = gtk_open_with_get_app_info (GTK_OPEN_WITH (self->priv->open_with_widget));
  command = g_app_info_get_executable (info);

  g_shell_parse_argv (command, &argc, &argv, &error);

  if (error)
    {
      show_error_dialog (_("Could not run application"),
			 error->message,
			 GTK_WINDOW (self));
      g_error_free (error);
      retval = FALSE;
      goto cleanup;
    }

  path = g_find_program_in_path (argv[0]);
  if (!path)
    {
      char *error_message;

      error_message = g_strdup_printf (_("Could not find '%s'"),
				       argv[0]);

      show_error_dialog (_("Could not find application"),
			 error_message,
			 GTK_WINDOW (self));
      g_free (error_message);
      retval = FALSE;
      goto cleanup;
    }

  *app_out = info;

 cleanup:
  g_strfreev (argv);
  g_free (path);

  return retval;
}

static void
add_or_find_application (GtkOpenWithDialog *self)
{
  GAppInfo *app;
  GList *applications;

  app = gtk_open_with_get_app_info (GTK_OPEN_WITH (self));
  
  if (app == NULL)
    {
      /* TODO: better error? */
      show_error_dialog (_("Could not add application"),
			 NULL,
			 GTK_WINDOW (self));
      return;
    }

  applications = g_app_info_get_all_for_type (self->priv->content_type);
  if (self->priv->content_type != NULL && applications != NULL)
    {
      /* we don't care about reporting errors here */
      g_app_info_add_supports_type (app,
				    self->priv->content_type,
				    NULL);
    }

  if (applications != NULL)
    g_list_free_full (applications, g_object_unref);

  g_object_unref (app);
}

static void
gtk_open_with_dialog_response (GtkDialog *dialog,
			       gint response_id,
			       gpointer user_data)
{
  GtkOpenWithDialog *self = GTK_OPEN_WITH_DIALOG (dialog);

  switch (response_id)
    {
    case GTK_RESPONSE_OK:
      add_or_find_application (self);
      break;
    default :
      break;
    }
}

static void
widget_application_selected_cb (GtkOpenWithWidget *widget,
				GAppInfo *app_info,
				gpointer user_data)
{
  GtkOpenWithDialog *self = user_data;

  gtk_widget_set_sensitive (self->priv->button, TRUE);
}

static void
widget_application_activated_cb (GtkOpenWithWidget *widget,
				 GAppInfo *app_info,
				 gpointer user_data)
{
  GtkOpenWithDialog *self = user_data;

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
set_dialog_properties (GtkOpenWithDialog *self)
{
  char *label, *name, *extension, *description;
  PangoFontDescription *font_desc;

  name = NULL;
  extension = NULL;
  label = NULL;
  description = NULL;

  if (self->priv->gfile != NULL)
    {
      name = g_file_get_basename (self->priv->gfile);
      extension = get_extension (name);
    }

  description = g_content_type_get_description (self->priv->content_type);
  gtk_window_set_title (GTK_WINDOW (self), "");

  if (name != NULL)
    {
      /* Translators: %s is a filename */
      label = g_strdup_printf (_("Select an application to open \"%s\""), name);
    }
  else
    {
      /* Translators: %s is a file type description */
      label = g_strdup_printf (_("Select an application for \"%s\" files"),
			       g_content_type_is_unknown (self->priv->content_type) ?
			       self->priv->content_type : description);
    }

  font_desc = pango_font_description_new ();
  pango_font_description_set_weight (font_desc, PANGO_WEIGHT_BOLD);
  gtk_widget_modify_font (self->priv->label, font_desc);
  pango_font_description_free (font_desc);

  gtk_label_set_markup (GTK_LABEL (self->priv->label), label);

  g_free (label);
  g_free (name);
  g_free (extension);
  g_free (description);
}

static void
build_dialog_ui (GtkOpenWithDialog *self)
{
  GtkWidget *vbox;
  GtkWidget *vbox2;
  GtkWidget *label;
  GtkWidget *action_area;

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
  gtk_label_set_line_wrap (GTK_LABEL (self->priv->label), TRUE);
  gtk_box_pack_start (GTK_BOX (vbox2), self->priv->label,
		      FALSE, FALSE, 0);
  gtk_widget_show (self->priv->label);

  self->priv->open_with_widget =
    gtk_open_with_widget_new (self->priv->content_type);
  g_signal_connect (self->priv->open_with_widget, "application-selected",
		    G_CALLBACK (widget_application_selected_cb), self);
  g_signal_connect (self->priv->open_with_widget, "application-activated",
		    G_CALLBACK (widget_application_activated_cb), self);
  gtk_box_pack_start (GTK_BOX (vbox2), self->priv->open_with_widget, TRUE, TRUE, 0);
  gtk_widget_show (self->priv->open_with_widget);

  gtk_dialog_add_button (GTK_DIALOG (self),
			 GTK_STOCK_CANCEL,
			 GTK_RESPONSE_CANCEL);

  /* Create a custom stock icon */
  self->priv->button = gtk_button_new ();

  label = gtk_label_new_with_mnemonic (_("_Open"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), GTK_WIDGET (self->priv->button));
  gtk_widget_set_halign (label, GTK_ALIGN_CENTER);
  gtk_widget_show (label);
  self->priv->open_label = label;

  gtk_container_add (GTK_CONTAINER (self->priv->button),
		     self->priv->open_label);

  gtk_widget_show (self->priv->button);
  gtk_widget_set_can_default (self->priv->button, TRUE);

  gtk_dialog_add_action_widget (GTK_DIALOG (self),
				self->priv->button, GTK_RESPONSE_OK);

  action_area = gtk_dialog_get_action_area (GTK_DIALOG (self));
  self->priv->online_button = gtk_button_new_with_label (_("Find applications online"));
  gtk_box_pack_start (GTK_BOX (action_area), self->priv->online_button,
		      FALSE, FALSE, 0);
  gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (action_area), self->priv->online_button,
				      TRUE);
  gtk_widget_show (self->priv->online_button);
  g_signal_connect (self->priv->online_button, "clicked",
		    G_CALLBACK (online_button_clicked_cb), self);

  gtk_dialog_set_default_response (GTK_DIALOG (self),
				   GTK_RESPONSE_OK);
}

static void
set_gfile_and_content_type (GtkOpenWithDialog *self,
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
gtk_open_with_dialog_get_app_info (GtkOpenWith *object)
{
  GtkOpenWithDialog *self = GTK_OPEN_WITH_DIALOG (object);
  GAppInfo *app = NULL;

  if (!check_application (self, &app))
    return NULL;

  return app;
}

static void
gtk_open_with_dialog_constructed (GObject *object)
{
  GtkOpenWithDialog *self = GTK_OPEN_WITH_DIALOG (object);

  g_assert (self->priv->content_type != NULL ||
	    self->priv->gfile != NULL);

  if (G_OBJECT_CLASS (gtk_open_with_dialog_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (gtk_open_with_dialog_parent_class)->constructed (object);

  build_dialog_ui (self);
  set_dialog_properties (self);
}

static void
gtk_open_with_dialog_finalize (GObject *object)
{
  GtkOpenWithDialog *self = GTK_OPEN_WITH_DIALOG (object);

  if (self->priv->gfile)
    g_object_unref (self->priv->gfile);

  g_free (self->priv->content_type);

  G_OBJECT_CLASS (gtk_open_with_dialog_parent_class)->finalize (object);
}

static void
gtk_open_with_dialog_set_property (GObject *object,
				   guint property_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
  GtkOpenWithDialog *self = GTK_OPEN_WITH_DIALOG (object);

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
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_open_with_dialog_get_property (GObject *object,
				   guint property_id,
				   GValue *value,
				   GParamSpec *pspec)
{
  GtkOpenWithDialog *self = GTK_OPEN_WITH_DIALOG (object);

  switch (property_id)
    {
    case PROP_GFILE:
      if (self->priv->gfile != NULL)
	g_value_set_object (value, self->priv->gfile);
      break;
    case PROP_CONTENT_TYPE:
      g_value_set_string (value, self->priv->content_type);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_open_with_dialog_iface_init (GtkOpenWithIface *iface)
{
  iface->get_app_info = gtk_open_with_dialog_get_app_info;
}

static void
gtk_open_with_dialog_class_init (GtkOpenWithDialogClass *klass)
{
  GObjectClass *gobject_class;
  GParamSpec *pspec;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = gtk_open_with_dialog_finalize;
  gobject_class->set_property = gtk_open_with_dialog_set_property;
  gobject_class->get_property = gtk_open_with_dialog_get_property;
  gobject_class->constructed = gtk_open_with_dialog_constructed;

  g_object_class_override_property (gobject_class, PROP_CONTENT_TYPE, "content-type");

  pspec = g_param_spec_object ("gfile",
			       P_("GFile"),
			       P_("The GFile used by the open with dialog"),
			       G_TYPE_FILE,
			       G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
			       G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class, PROP_GFILE, pspec);

  g_type_class_add_private (klass, sizeof (GtkOpenWithDialogPrivate));
}

static void
gtk_open_with_dialog_init (GtkOpenWithDialog *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GTK_TYPE_OPEN_WITH_DIALOG,
					    GtkOpenWithDialogPrivate);

  /* we can't override the class signal handler here, as it's a RUN_LAST;
   * we want our signal handler instead to be executed before any user code.
   */
  g_signal_connect (self, "response",
		    G_CALLBACK (gtk_open_with_dialog_response), NULL);
}

static void
set_parent_and_flags (GtkWidget *dialog,
		      GtkWindow *parent,
		      GtkDialogFlags flags)
{
  if (parent != NULL)
    gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                  parent);
  
  if (flags & GTK_DIALOG_MODAL)
    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

  if (flags & GTK_DIALOG_DESTROY_WITH_PARENT)
    gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
}

/**
 * gtk_open_with_dialog_new:
 * @parent: (allow-none): a #GtkWindow, or %NULL
 * @flags: flags for this dialog
 * @file: a #GFile
 *
 * Creates a new #GtkOpenWithDialog for the provided #GFile, to allow
 * the user to select an application for it.
 *
 * Returns: a newly created #GtkOpenWithDialog
 *
 * Since: 3.0
 **/
GtkWidget *
gtk_open_with_dialog_new (GtkWindow *parent,
			  GtkDialogFlags flags,
			  GFile *file)
{
  GtkWidget *retval;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  retval = g_object_new (GTK_TYPE_OPEN_WITH_DIALOG,
			 "gfile", file,
			 NULL);

  set_parent_and_flags (retval, parent, flags);

  return retval;
}

/**
 * gtk_open_with_dialog_new_for_content_type:
 * @parent: (allow-none): a #GtkWindow, or %NULL
 * @flags: flags for this dialog
 * @content_type: a content type string
 *
 * Creates a new #GtkOpenWithDialog for the provided content type, to allow
 * the user to select an application for it.
 *
 * Returns: a newly created #GtkOpenWithDialog
 *
 * Since: 3.0
 **/
GtkWidget *
gtk_open_with_dialog_new_for_content_type (GtkWindow *parent,
					   GtkDialogFlags flags,
					   const gchar *content_type)
{
  GtkWidget *retval;

  g_return_val_if_fail (content_type != NULL, NULL);

  retval = g_object_new (GTK_TYPE_OPEN_WITH_DIALOG,
			 "content-type", content_type,
			 NULL);

  set_parent_and_flags (retval, parent, flags);

  return retval;
}

GtkWidget *
gtk_open_with_dialog_get_widget (GtkOpenWithDialog *self)
{
  g_return_val_if_fail (GTK_IS_OPEN_WITH_DIALOG (self), NULL);

  return self->priv->open_with_widget;
}
