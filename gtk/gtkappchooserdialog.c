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
#include "gtkappchooserprivate.h"

#include "gtkmessagedialog.h"
#include "gtksettings.h"
#include "gtklabel.h"
#include "gtkbbox.h"
#include "gtkbutton.h"
#include "gtkentry.h"
#include "gtktogglebutton.h"
#include "gtkstylecontext.h"
#include "gtkmenuitem.h"
#include "gtkheaderbar.h"
#include "gtkdialogprivate.h"
#include "gtksearchbar.h"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <gio/gio.h>

#define sure_string(s) ((const char *) ((s) != NULL ? (s) : ""))

struct _GtkAppChooserDialogPrivate {
  char *content_type;
  GFile *gfile;
  char *heading;

  GtkWidget *label;
  GtkWidget *inner_box;

  GtkWidget *open_label;

  GtkWidget *search_bar;
  GtkWidget *search_entry;
  GtkWidget *app_chooser_widget;
  GtkWidget *show_more_button;
  GtkWidget *software_button;

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
                         G_ADD_PRIVATE (GtkAppChooserDialog)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_APP_CHOOSER,
                                                gtk_app_chooser_dialog_iface_init));


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
      self->priv->dismissed = TRUE;
    default:
      break;
    }
}

static void
widget_application_selected_cb (GtkAppChooserWidget *widget,
                                GAppInfo            *app_info,
                                gpointer             user_data)
{
  GtkDialog *self = user_data;

  gtk_dialog_set_response_sensitive (self, GTK_RESPONSE_OK, TRUE);
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
  gchar *name;
  gchar *extension;
  gchar *description;
  gchar *string;
  gboolean unknown;
  gchar *title;
  gchar *subtitle;
  gboolean use_header;
  GtkWidget *header;

  name = NULL;
  extension = NULL;
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

  if (name != NULL)
    {
      title = g_strdup (_("Select Application"));
      /* Translators: %s is a filename */
      subtitle = g_strdup_printf (_("Opening “%s”."), name);
      string = g_strdup_printf (_("No applications found for “%s”"), name);
    }
  else
    {
      title = g_strdup (_("Select Application"));
      /* Translators: %s is a file type description */
      subtitle = g_strdup_printf (_("Opening “%s” files."), 
                                  unknown ? self->priv->content_type : description);
      string = g_strdup_printf (_("No applications found for “%s” files"),
                                unknown ? self->priv->content_type : description);
    }

  g_object_get (self, "use-header-bar", &use_header, NULL); 
  if (use_header)
    {
      header = gtk_dialog_get_header_bar (GTK_DIALOG (self));
      gtk_header_bar_set_title (GTK_HEADER_BAR (header), title);
      gtk_header_bar_set_subtitle (GTK_HEADER_BAR (header), subtitle);
    }
  else
    {
      gtk_window_set_title (GTK_WINDOW (self), _("Select Application"));
    }

  if (self->priv->heading != NULL)
    {
      gtk_label_set_markup (GTK_LABEL (self->priv->label), self->priv->heading);
      gtk_widget_show (self->priv->label);
    }
  else
    {
      gtk_widget_hide (self->priv->label);
    }

  gtk_app_chooser_widget_set_default_text (GTK_APP_CHOOSER_WIDGET (self->priv->app_chooser_widget),
                                           string);

  g_free (title);
  g_free (subtitle);
  g_free (name);
  g_free (extension);
  g_free (description);
  g_free (string);
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

static gboolean
key_press_event_cb (GtkWidget    *widget,
                    GdkEvent     *event,
                    GtkSearchBar *bar)
{
  return gtk_search_bar_handle_event (bar, event);
}

static void
construct_appchooser_widget (GtkAppChooserDialog *self)
{
  GAppInfo *info;

  /* Need to build the appchooser widget after, because of the content-type construct-only property */
  self->priv->app_chooser_widget = gtk_app_chooser_widget_new (self->priv->content_type);
  gtk_box_pack_start (GTK_BOX (self->priv->inner_box), self->priv->app_chooser_widget, TRUE, TRUE, 0);
  gtk_widget_show (self->priv->app_chooser_widget);

  g_signal_connect (self->priv->app_chooser_widget, "application-selected",
                    G_CALLBACK (widget_application_selected_cb), self);
  g_signal_connect (self->priv->app_chooser_widget, "application-activated",
                    G_CALLBACK (widget_application_activated_cb), self);
  g_signal_connect (self->priv->app_chooser_widget, "notify::show-other",
                    G_CALLBACK (widget_notify_for_button_cb), self);
  g_signal_connect (self->priv->app_chooser_widget, "populate-popup",
                    G_CALLBACK (widget_populate_popup_cb), self);

  /* Add the custom button to the new appchooser */
  gtk_box_pack_start (GTK_BOX (self->priv->inner_box),
		      self->priv->show_more_button, FALSE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (self->priv->inner_box),
		      self->priv->software_button, FALSE, FALSE, 0);

  info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (self->priv->app_chooser_widget));
  gtk_dialog_set_response_sensitive (GTK_DIALOG (self), GTK_RESPONSE_OK, info != NULL);
  if (info)
    g_object_unref (info);

  _gtk_app_chooser_widget_set_search_entry (GTK_APP_CHOOSER_WIDGET (self->priv->app_chooser_widget),
                                            GTK_ENTRY (self->priv->search_entry));
  g_signal_connect (self, "key-press-event",
                    G_CALLBACK (key_press_event_cb), self->priv->search_bar);
}

static void
set_gfile_and_content_type (GtkAppChooserDialog *self,
                            GFile               *file)
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
software_button_clicked_cb (GtkButton           *button,
                            GtkAppChooserDialog *self)
{
  GSubprocess *process;
  GError *error = NULL;
  gchar *option;

  if (self->priv->content_type)
    option = g_strconcat ("--search=", self->priv->content_type, NULL);
  else
    option = g_strdup ("--mode=overview");

  process = g_subprocess_new (0, &error, "gnome-software", option, NULL);
  if (!process)
    {
      show_error_dialog (_("Failed to start GNOME Software"),
                         error->message, GTK_WINDOW (self));
      g_error_free (error);
    }
  else
    g_object_unref (process);

  g_free (option);
}

static void
ensure_software_button (GtkAppChooserDialog *self)
{
  gchar *path;

  path = g_find_program_in_path ("gnome-software");
  if (path != NULL)
    gtk_widget_show (self->priv->software_button);
  else
    gtk_widget_hide (self->priv->software_button);

  g_free (path);
}

static void
setup_search (GtkAppChooserDialog *self)
{
  gboolean use_header;

  g_object_get (self, "use-header-bar", &use_header, NULL);
  if (use_header)
    {
      GtkWidget *button;
      GtkWidget *image;
      GtkWidget *header;

      button = gtk_toggle_button_new ();
      gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
      image = gtk_image_new_from_icon_name ("edit-find-symbolic", GTK_ICON_SIZE_MENU);
      gtk_widget_show (image);
      gtk_container_add (GTK_CONTAINER (button), image);
      gtk_style_context_add_class (gtk_widget_get_style_context (button), "image-button");
      gtk_style_context_remove_class (gtk_widget_get_style_context (button), "text-button");
      gtk_widget_show (button);

      header = gtk_dialog_get_header_bar (GTK_DIALOG (self));
      gtk_header_bar_pack_end (GTK_HEADER_BAR (header), button);

      g_object_bind_property (button, "active",
                              self->priv->search_bar, "search-mode-enabled",
                              G_BINDING_BIDIRECTIONAL);
      g_object_bind_property (self->priv->search_entry, "sensitive",
                              button, "sensitive",
                              G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
    }
}

static void
gtk_app_chooser_dialog_constructed (GObject *object)
{
  GtkAppChooserDialog *self = GTK_APP_CHOOSER_DIALOG (object);

  if (G_OBJECT_CLASS (gtk_app_chooser_dialog_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (gtk_app_chooser_dialog_parent_class)->constructed (object);

  construct_appchooser_widget (self);
  set_dialog_properties (self);
  ensure_software_button (self);
  setup_search (self);
}

/* This is necessary do deal with the fact that GtkDialog
 * exposes bits of its internal spacing as style properties,
 * and puts the action area inside the content area.
 * To achieve a flush-top search bar, we need the content
 * area border to be 0, and distribute the spacing to other
 * containers to compensate.
 */
static void
update_spacings (GtkAppChooserDialog *self)
{
  GtkWidget *widget;
  gint content_area_border;
  gint action_area_border;

  gtk_widget_style_get (GTK_WIDGET (self),
                        "content-area-border", &content_area_border,
                        "action-area-border", &action_area_border,
                        NULL);

  widget = gtk_dialog_get_content_area (GTK_DIALOG (self));
  gtk_container_set_border_width (GTK_CONTAINER (widget), 0);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  widget = gtk_dialog_get_action_area (GTK_DIALOG (self));
G_GNUC_END_IGNORE_DEPRECATIONS
  gtk_container_set_border_width (GTK_CONTAINER (widget), 5 + content_area_border + action_area_border);

  widget = self->priv->inner_box;
  gtk_container_set_border_width (GTK_CONTAINER (widget), 10 + content_area_border);
}

static void
gtk_app_chooser_dialog_style_updated (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (gtk_app_chooser_dialog_parent_class)->style_updated (widget);

  update_spacings (GTK_APP_CHOOSER_DIALOG (widget));
}

static void
gtk_app_chooser_dialog_dispose (GObject *object)
{
  GtkAppChooserDialog *self = GTK_APP_CHOOSER_DIALOG (object);
  
  g_clear_object (&self->priv->gfile);

  self->priv->dismissed = TRUE;

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
  GtkWidgetClass *widget_class;
  GObjectClass *gobject_class;
  GParamSpec *pspec;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = gtk_app_chooser_dialog_dispose;
  gobject_class->finalize = gtk_app_chooser_dialog_finalize;
  gobject_class->set_property = gtk_app_chooser_dialog_set_property;
  gobject_class->get_property = gtk_app_chooser_dialog_get_property;
  gobject_class->constructed = gtk_app_chooser_dialog_constructed;

  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->style_updated = gtk_app_chooser_dialog_style_updated;

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

  /* Bind class to template
   */
  gtk_widget_class_set_template_from_resource (widget_class,
					       "/org/gtk/libgtk/ui/gtkappchooserdialog.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkAppChooserDialog, label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkAppChooserDialog, show_more_button);
  gtk_widget_class_bind_template_child_private (widget_class, GtkAppChooserDialog, software_button);
  gtk_widget_class_bind_template_child_private (widget_class, GtkAppChooserDialog, inner_box);
  gtk_widget_class_bind_template_child_private (widget_class, GtkAppChooserDialog, search_bar);
  gtk_widget_class_bind_template_child_private (widget_class, GtkAppChooserDialog, search_entry);
  gtk_widget_class_bind_template_callback (widget_class, show_more_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, software_button_clicked_cb);
}

static void
gtk_app_chooser_dialog_init (GtkAppChooserDialog *self)
{
  self->priv = gtk_app_chooser_dialog_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
  gtk_dialog_set_use_header_bar_from_setting (GTK_DIALOG (self));

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_dialog_set_alternative_button_order (GTK_DIALOG (self),
                                           GTK_RESPONSE_OK,
                                           GTK_RESPONSE_CANCEL,
                                           -1);
G_GNUC_END_IGNORE_DEPRECATIONS

  /* we can't override the class signal handler here, as it's a RUN_LAST;
   * we want our signal handler instead to be executed before any user code.
   */
  g_signal_connect (self, "response",
                    G_CALLBACK (gtk_app_chooser_dialog_response), NULL);

  update_spacings (self);
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

  if (self->priv->label)
    {
      if (self->priv->heading)
        {
          gtk_label_set_markup (GTK_LABEL (self->priv->label), self->priv->heading);
          gtk_widget_show (self->priv->label);
        }
      else
        {
          gtk_widget_hide (self->priv->label);
        }
    }

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
