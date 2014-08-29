/* GTK - The GIMP Toolkit
 * Copyright (C) 2001 CodeFactory AB
 * Copyright (C) 2001, 2002 Anders Carlsson
 * Copyright (C) 2003, 2004 Matthias Clasen <mclasen@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Author: Anders Carlsson <andersca@gnome.org>
 *
 * Modified by the GTK+ Team and others 1997-2004.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include <string.h>

#include "gtkaboutdialog.h"
#include "gtkbutton.h"
#include "gtkbbox.h"
#include "gtkdialog.h"
#include "gtkgrid.h"
#include "gtkbox.h"
#include "gtkicontheme.h"
#include "gtkimage.h"
#include "gtklabel.h"
#include "gtklinkbutton.h"
#include "gtkmarshalers.h"
#include "gtkstack.h"
#include "gtkorientable.h"
#include "gtkscrolledwindow.h"
#include "gtktextview.h"
#include "gtkshow.h"
#include "gtkmain.h"
#include "gtkmessagedialog.h"
#include "gtktogglebutton.h"
#include "gtktypebuiltins.h"
#include "gtkstack.h"
#include "gtkstackswitcher.h"
#include "gtksettings.h"
#include "gtkheaderbar.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkdialogprivate.h"


/**
 * SECTION:gtkaboutdialog
 * @Short_description: Display information about an application
 * @Title: GtkAboutDialog
 *
 * The GtkAboutDialog offers a simple way to display information about
 * a program like its logo, name, copyright, website and license. It is
 * also possible to give credits to the authors, documenters, translators
 * and artists who have worked on the program. An about dialog is typically
 * opened when the user selects the `About` option from the `Help` menu.
 * All parts of the dialog are optional.
 *
 * About dialogs often contain links and email addresses. GtkAboutDialog
 * displays these as clickable links. By default, it calls gtk_show_uri()
 * when a user clicks one. The behaviour can be overridden with the
 * #GtkAboutDialog::activate-link signal.
 *
 * To specify a person with an email address, use a string like
 * "Edgar Allan Poe <edgar@poe.com>". To specify a website with a title,
 * use a string like "GTK+ team http://www.gtk.org".
 *
 * To make constructing a GtkAboutDialog as convenient as possible, you can
 * use the function gtk_show_about_dialog() which constructs and shows a dialog
 * and keeps it around so that it can be shown again.
 *
 * Note that GTK+ sets a default title of `_("About %s")` on the dialog
 * window (where \%s is replaced by the name of the application, but in
 * order to ensure proper translation of the title, applications should
 * set the title property explicitly when constructing a GtkAboutDialog,
 * as shown in the following example:
 * |[<!-- language="C" -->
 * gtk_show_about_dialog (NULL,
 *                        "program-name", "ExampleCode",
 *                        "logo", example_logo,
 *                        "title" _("About ExampleCode"),
 *                        NULL);
 * ]|
 *
 * It is also possible to show a #GtkAboutDialog like any other #GtkDialog,
 * e.g. using gtk_dialog_run(). In this case, you might need to know that
 * the “Close” button returns the #GTK_RESPONSE_CANCEL response id.
 */

typedef struct
{
  const gchar *name;
  const gchar *url;
} LicenseInfo;

/* Translators: this is the license preamble; the string at the end
 * contains the name of the license as link text.
 */
static const gchar *gtk_license_preamble = N_("This program comes with ABSOLUTELY NO WARRANTY.\nSee the <a href=\"%s\">%s</a> for details.");

/* LicenseInfo for each GtkLicense type; keep in the same order as the enumeration */
static const LicenseInfo gtk_license_info [] = {
  { N_("License"), NULL },
  { N_("Custom License") , NULL },
  { N_("GNU General Public License, version 2 or later"), "http://www.gnu.org/licenses/old-licenses/gpl-2.0.html" },
  { N_("GNU General Public License, version 3 or later"), "http://www.gnu.org/licenses/gpl-3.0.html" },
  { N_("GNU Lesser General Public License, version 2.1 or later"), "http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html" },
  { N_("GNU Lesser General Public License, version 3 or later"), "http://www.gnu.org/licenses/lgpl-3.0.html" },
  { N_("BSD 2-Clause License"), "http://opensource.org/licenses/bsd-license.php" },
  { N_("The MIT License (MIT)"), "http://opensource.org/licenses/mit-license.php" },
  { N_("Artistic License 2.0"), "http://opensource.org/licenses/artistic-license-2.0.php" },
  { N_("GNU General Public License, version 2 only"), "http://www.gnu.org/licenses/old-licenses/gpl-2.0.html" },
  { N_("GNU General Public License, version 3 only"), "http://www.gnu.org/licenses/gpl-3.0.html" },
  { N_("GNU Lesser General Public License, version 2.1 only"), "http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html" },
  { N_("GNU Lesser General Public License, version 3 only"), "http://www.gnu.org/licenses/lgpl-3.0.html" }
};

typedef struct
{
  gchar *heading;
  gchar **people;
} CreditSection;

struct _GtkAboutDialogPrivate
{
  gchar *name;
  gchar *version;
  gchar *copyright;
  gchar *comments;
  gchar *website_url;
  gchar *website_text;
  gchar *translator_credits;
  gchar *license;

  gchar **authors;
  gchar **documenters;
  gchar **artists;


  GSList *credit_sections;

  gboolean credits_page_initialized;
  gboolean license_page_initialized;

  GtkWidget *stack;
  GtkWidget *stack_switcher;
  GtkWidget *credits_button;
  GtkWidget *license_button;

  GtkWidget *logo_image;
  GtkWidget *name_label;
  GtkWidget *version_label;
  GtkWidget *comments_label;
  GtkWidget *copyright_label;
  GtkWidget *license_label;
  GtkWidget *website_label;

  GtkWidget *credits_page;
  GtkWidget *license_page;

  GtkWidget *credits_grid;
  GtkWidget *license_view;

  GdkCursor *hand_cursor;
  GdkCursor *regular_cursor;

  GSList *visited_links;

  GtkLicense license_type;

  guint hovering_over_link : 1;
  guint wrap_license : 1;
  guint in_child_changed : 1;
  guint in_switch_page : 1;
};

enum
{
  PROP_0,
  PROP_NAME,
  PROP_VERSION,
  PROP_COPYRIGHT,
  PROP_COMMENTS,
  PROP_WEBSITE,
  PROP_WEBSITE_LABEL,
  PROP_LICENSE,
  PROP_AUTHORS,
  PROP_DOCUMENTERS,
  PROP_TRANSLATOR_CREDITS,
  PROP_ARTISTS,
  PROP_LOGO,
  PROP_LOGO_ICON_NAME,
  PROP_WRAP_LICENSE,
  PROP_LICENSE_TYPE,
  LAST_PROP
};

static void                 gtk_about_dialog_finalize       (GObject            *object);
static void                 gtk_about_dialog_get_property   (GObject            *object,
                                                             guint               prop_id,
                                                             GValue             *value,
                                                             GParamSpec         *pspec);
static void                 gtk_about_dialog_set_property   (GObject            *object,
                                                             guint               prop_id,
                                                             const GValue       *value,
                                                             GParamSpec         *pspec);
static void                 gtk_about_dialog_show           (GtkWidget          *widge);
static void                 update_name_version             (GtkAboutDialog     *about);
static void                 follow_if_link                  (GtkAboutDialog     *about,
                                                             GtkTextView        *text_view,
                                                             GtkTextIter        *iter);
static void                 set_cursor_if_appropriate       (GtkAboutDialog     *about,
                                                             GtkTextView        *text_view,
                                                             GdkDevice          *device,
                                                             gint                x,
                                                             gint                y);
static void                 populate_credits_page           (GtkAboutDialog     *about);
static void                 populate_license_page           (GtkAboutDialog     *about);
static void                 close_cb                        (GtkAboutDialog     *about);
static gboolean             gtk_about_dialog_activate_link  (GtkAboutDialog     *about,
                                                             const gchar        *uri);
static gboolean             emit_activate_link              (GtkAboutDialog     *about,
							     const gchar        *uri);
static gboolean             text_view_key_press_event       (GtkWidget          *text_view,
							     GdkEventKey        *event,
							     GtkAboutDialog     *about);
static gboolean             text_view_event_after           (GtkWidget          *text_view,
							     GdkEvent           *event,
							     GtkAboutDialog     *about);
static gboolean             text_view_motion_notify_event   (GtkWidget          *text_view,
							     GdkEventMotion     *event,
							     GtkAboutDialog     *about);
static void                 toggle_credits                  (GtkToggleButton    *button,
                                                             gpointer            user_data);
static void                 toggle_license                  (GtkToggleButton    *button,
                                                             gpointer            user_data);

enum {
  ACTIVATE_LINK,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };
static GParamSpec *props[LAST_PROP] = { NULL, };

G_DEFINE_TYPE_WITH_PRIVATE (GtkAboutDialog, gtk_about_dialog, GTK_TYPE_DIALOG)

static gboolean
stack_visible_child_notify (GtkStack       *stack,
                            GParamSpec     *pspec,
                            GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv = about->priv;
  GtkWidget *child;

  child = gtk_stack_get_visible_child (stack);
  if (child == priv->credits_page)
    {
      if (!priv->credits_page_initialized)
        {
          populate_credits_page (about);
          priv->credits_page_initialized = TRUE;
        }
    }
  else if (child == priv->license_page)
    {
      if (!priv->license_page_initialized)
        {
          populate_license_page (about);
          priv->license_page_initialized = TRUE;
        }
    }

  return FALSE;
}

static void
gtk_about_dialog_class_init (GtkAboutDialogClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GObjectClass *)klass;
  widget_class = (GtkWidgetClass *)klass;

  object_class->set_property = gtk_about_dialog_set_property;
  object_class->get_property = gtk_about_dialog_get_property;

  object_class->finalize = gtk_about_dialog_finalize;

  widget_class->show = gtk_about_dialog_show;

  klass->activate_link = gtk_about_dialog_activate_link;

  /**
   * GtkAboutDialog::activate-link:
   * @label: The object on which the signal was emitted
   * @uri: the URI that is activated
   *
   * The signal which gets emitted to activate a URI.
   * Applications may connect to it to override the default behaviour,
   * which is to call gtk_show_uri().
   *
   * Returns: %TRUE if the link has been activated
   *
   * Since: 2.24
   */
  signals[ACTIVATE_LINK] =
    g_signal_new ("activate-link",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkAboutDialogClass, activate_link),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__STRING,
                  G_TYPE_BOOLEAN, 1, G_TYPE_STRING);

  /**
   * GtkAboutDialog:program-name:
   *
   * The name of the program.
   * If this is not set, it defaults to g_get_application_name().
   *
   * Since: 2.12
   */
  props[PROP_NAME] =
    g_param_spec_string ("program-name",
                         P_("Program name"),
                         P_("The name of the program. If this is not set, it defaults to g_get_application_name()"),
                         NULL,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkAboutDialog:version:
   *
   * The version of the program.
   *
   * Since: 2.6
   */
  props[PROP_VERSION] =
    g_param_spec_string ("version",
                         P_("Program version"),
                         P_("The version of the program"),
                         NULL,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkAboutDialog:copyright:
   *
   * Copyright information for the program.
   *
   * Since: 2.6
   */
  props[PROP_COPYRIGHT] =
    g_param_spec_string ("copyright",
                         P_("Copyright string"),
                         P_("Copyright information for the program"),
                         NULL,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
        

  /**
   * GtkAboutDialog:comments:
   *
   * Comments about the program. This string is displayed in a label
   * in the main dialog, thus it should be a short explanation of
   * the main purpose of the program, not a detailed list of features.
   *
   * Since: 2.6
   */
  props[PROP_COMMENTS] =
    g_param_spec_string ("comments",
                         P_("Comments string"),
                         P_("Comments about the program"),
                         NULL,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkAboutDialog:license:
   *
   * The license of the program. This string is displayed in a
   * text view in a secondary dialog, therefore it is fine to use
   * a long multi-paragraph text. Note that the text is only wrapped
   * in the text view if the "wrap-license" property is set to %TRUE;
   * otherwise the text itself must contain the intended linebreaks.
   * When setting this property to a non-%NULL value, the
   * #GtkAboutDialog:license-type property is set to %GTK_LICENSE_CUSTOM
   * as a side effect.
   *
   * Since: 2.6
   */
  props[PROP_LICENSE] =
    g_param_spec_string ("license",
                         _("License"),
                         _("The license of the program"),
                         NULL,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkAboutDialog:license-type:
   *
   * The license of the program, as a value of the %GtkLicense enumeration.
   *
   * The #GtkAboutDialog will automatically fill out a standard disclaimer
   * and link the user to the appropriate online resource for the license
   * text.
   *
   * If %GTK_LICENSE_UNKNOWN is used, the link used will be the same
   * specified in the #GtkAboutDialog:website property.
   *
   * If %GTK_LICENSE_CUSTOM is used, the current contents of the
   * #GtkAboutDialog:license property are used.
   *
   * For any other #GtkLicense value, the contents of the
   * #GtkAboutDialog:license property are also set by this property as
   * a side effect.
   *
   * Since: 3.0
   */
  props[PROP_LICENSE_TYPE] =
    g_param_spec_enum ("license-type",
                       P_("License Type"),
                       P_("The license type of the program"),
                       GTK_TYPE_LICENSE,
                       GTK_LICENSE_UNKNOWN,
                       GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkAboutDialog:website:
   *
   * The URL for the link to the website of the program.
   * This should be a string starting with "http://.
   *
   * Since: 2.6
   */
  props[PROP_WEBSITE] =
    g_param_spec_string ("website",
                         P_("Website URL"),
                         P_("The URL for the link to the website of the program"),
                         NULL,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkAboutDialog:website-label:
   *
   * The label for the link to the website of the program.
   *
   * Since: 2.6
   */
  props[PROP_WEBSITE_LABEL] =
    g_param_spec_string ("website-label",
                         P_("Website label"),
                         P_("The label for the link to the website of the program"),
                         NULL,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkAboutDialog:authors:
   *
   * The authors of the program, as a %NULL-terminated array of strings.
   * Each string may contain email addresses and URLs, which will be displayed
   * as links, see the introduction for more details.
   *
   * Since: 2.6
   */
  props[PROP_AUTHORS] =
    g_param_spec_boxed ("authors",
                        P_("Authors"),
                        P_("List of authors of the program"),
                        G_TYPE_STRV,
                        GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkAboutDialog:documenters:
   *
   * The people documenting the program, as a %NULL-terminated array of strings.
   * Each string may contain email addresses and URLs, which will be displayed
   * as links, see the introduction for more details.
   *
   * Since: 2.6
   */
  props[PROP_DOCUMENTERS] =
    g_param_spec_boxed ("documenters",
                        P_("Documenters"),
                        P_("List of people documenting the program"),
                        G_TYPE_STRV,
                        GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkAboutDialog:artists:
   *
   * The people who contributed artwork to the program, as a %NULL-terminated
   * array of strings. Each string may contain email addresses and URLs, which
   * will be displayed as links, see the introduction for more details.
   *
   * Since: 2.6
   */
  props[PROP_ARTISTS] =
    g_param_spec_boxed ("artists",
                        P_("Artists"),
                        P_("List of people who have contributed artwork to the program"),
                        G_TYPE_STRV,
                        GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkAboutDialog:translator-credits:
   *
   * Credits to the translators. This string should be marked as translatable.
   * The string may contain email addresses and URLs, which will be displayed
   * as links, see the introduction for more details.
   *
   * Since: 2.6
   */
  props[PROP_TRANSLATOR_CREDITS] =
    g_param_spec_string ("translator-credits",
                         P_("Translator credits"),
                         P_("Credits to the translators. This string should be marked as translatable"),
                         NULL,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkAboutDialog:logo:
   *
   * A logo for the about box. If it is %NULL, the default window icon
   * set with gtk_window_set_default_icon() will be used.
   *
   * Since: 2.6
   */
  props[PROP_LOGO] =
    g_param_spec_object ("logo",
                         P_("Logo"),
                         P_("A logo for the about box. If this is not set, it defaults to gtk_window_get_default_icon_list()"),
                         GDK_TYPE_PIXBUF,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkAboutDialog:logo-icon-name:
   *
   * A named icon to use as the logo for the about box. This property
   * overrides the #GtkAboutDialog:logo property.
   *
   * Since: 2.6
   */
  props[PROP_LOGO_ICON_NAME] =
    g_param_spec_string ("logo-icon-name",
                         P_("Logo Icon Name"),
                         P_("A named icon to use as the logo for the about box."),
                         "image-missing",
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkAboutDialog:wrap-license:
   *
   * Whether to wrap the text in the license dialog.
   *
   * Since: 2.8
   */
  props[PROP_WRAP_LICENSE] =
    g_param_spec_boolean ("wrap-license",
                          P_("Wrap license"),
                          P_("Whether to wrap the license text."),
                          FALSE,
                          GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  /* Bind class to template
   */
  gtk_widget_class_set_template_from_resource (widget_class,
					       "/org/gtk/libgtk/ui/gtkaboutdialog.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GtkAboutDialog, stack);
  gtk_widget_class_bind_template_child_private (widget_class, GtkAboutDialog, stack_switcher);
  gtk_widget_class_bind_template_child_private (widget_class, GtkAboutDialog, logo_image);
  gtk_widget_class_bind_template_child_private (widget_class, GtkAboutDialog, name_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkAboutDialog, version_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkAboutDialog, comments_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkAboutDialog, copyright_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkAboutDialog, license_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkAboutDialog, website_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkAboutDialog, credits_page);
  gtk_widget_class_bind_template_child_private (widget_class, GtkAboutDialog, license_page);
  gtk_widget_class_bind_template_child_private (widget_class, GtkAboutDialog, credits_grid);
  gtk_widget_class_bind_template_child_private (widget_class, GtkAboutDialog, license_view);

  gtk_widget_class_bind_template_callback (widget_class, emit_activate_link);
  gtk_widget_class_bind_template_callback (widget_class, text_view_event_after);
  gtk_widget_class_bind_template_callback (widget_class, text_view_key_press_event);
  gtk_widget_class_bind_template_callback (widget_class, text_view_motion_notify_event);
  gtk_widget_class_bind_template_callback (widget_class, stack_visible_child_notify);
}

static gboolean
emit_activate_link (GtkAboutDialog *about,
                    const gchar    *uri)
{
  gboolean handled = FALSE;

  g_signal_emit (about, signals[ACTIVATE_LINK], 0, uri, &handled);

  return TRUE;
}

static void
update_stack_switcher_visibility (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv = about->priv;

  if (gtk_widget_get_visible (priv->credits_page) ||
      gtk_widget_get_visible (priv->license_page))
    gtk_widget_show (priv->stack_switcher);
  else
    gtk_widget_hide (priv->stack_switcher);
}

static void
update_license_button_visibility (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv = about->priv;

  if (priv->license_type == GTK_LICENSE_CUSTOM && priv->license != NULL && priv->license[0] != '\0')
    gtk_widget_show (priv->license_page);
  else
    gtk_widget_hide (priv->license_page);

  update_stack_switcher_visibility (about);
}

static void
update_credits_button_visibility (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv = about->priv;
  gboolean show;

  show = (priv->authors != NULL ||
          priv->documenters != NULL ||
          priv->artists != NULL ||
          priv->credit_sections != NULL ||
         (priv->translator_credits != NULL &&
          strcmp (priv->translator_credits, "translator_credits") &&
          strcmp (priv->translator_credits, "translator-credits")));
  if (show)
    gtk_widget_show (priv->credits_page);
  else
    gtk_widget_hide (priv->credits_page);

  update_stack_switcher_visibility (about);
}

static void
switch_page (GtkAboutDialog *about,
             const gchar    *name)
{
  GtkAboutDialogPrivate *priv = about->priv;

  gtk_stack_set_visible_child_name (GTK_STACK (priv->stack), name);

  priv->in_switch_page = TRUE;
  if (priv->credits_button)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->credits_button),
                                  g_str_equal (name, "credits"));
  if (priv->license_button)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->license_button),
                                  g_str_equal (name, "license"));
  priv->in_switch_page = FALSE;
}

static void
apply_use_header_bar (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv = about->priv;
  gboolean use_header_bar;

  g_object_get (about, "use-header-bar", &use_header_bar, NULL);
  if (!use_header_bar)
    {
      GtkWidget *action_area;

      G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      action_area = gtk_dialog_get_action_area (GTK_DIALOG (about));
      G_GNUC_END_IGNORE_DEPRECATIONS

      priv->credits_button = gtk_toggle_button_new_with_mnemonic ("C_redits");
      g_object_bind_property (priv->credits_page, "visible",
                              priv->credits_button, "visible", G_BINDING_SYNC_CREATE);
      g_signal_connect (priv->credits_button, "toggled", G_CALLBACK (toggle_credits), about);
      gtk_container_add_with_properties (GTK_CONTAINER (action_area), priv->credits_button,
                                         "secondary", TRUE,
                                         NULL);

      priv->license_button = gtk_toggle_button_new_with_mnemonic ("_License");
      g_object_bind_property (priv->license_page, "visible",
                              priv->license_button, "visible", G_BINDING_SYNC_CREATE);
      g_signal_connect (priv->license_button, "toggled", G_CALLBACK (toggle_license), about);
      gtk_container_add_with_properties (GTK_CONTAINER (action_area), priv->license_button,
                                         "secondary", TRUE,
                                         NULL);


      gtk_dialog_add_button (GTK_DIALOG (about), "_Close", GTK_RESPONSE_DELETE_EVENT);
    }
}

static void
gtk_about_dialog_init (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;

  /* Data */
  priv = gtk_about_dialog_get_instance_private (about);
  about->priv = priv;

  priv->name = NULL;
  priv->version = NULL;
  priv->copyright = NULL;
  priv->comments = NULL;
  priv->website_url = NULL;
  priv->website_text = NULL;
  priv->translator_credits = NULL;
  priv->license = NULL;
  priv->authors = NULL;
  priv->documenters = NULL;
  priv->artists = NULL;

  priv->hand_cursor = gdk_cursor_new (GDK_HAND2);
  priv->regular_cursor = gdk_cursor_new (GDK_XTERM);
  priv->hovering_over_link = FALSE;
  priv->wrap_license = FALSE;

  priv->license_type = GTK_LICENSE_UNKNOWN;

  gtk_dialog_set_default_response (GTK_DIALOG (about), GTK_RESPONSE_CANCEL);

  gtk_widget_init_template (GTK_WIDGET (about));
  gtk_dialog_set_use_header_bar_from_setting (GTK_DIALOG (about));

  apply_use_header_bar (about);

  switch_page (about, "main");
  update_stack_switcher_visibility (about);

  /* force defaults */
  gtk_about_dialog_set_program_name (about, NULL);
  gtk_about_dialog_set_logo (about, NULL);
}

static void
destroy_credit_section (gpointer data)
{
  CreditSection *cs = data;
  g_free (cs->heading);
  g_strfreev (cs->people);
  g_slice_free (CreditSection, data);
}

static void
gtk_about_dialog_finalize (GObject *object)
{
  GtkAboutDialog *about = GTK_ABOUT_DIALOG (object);
  GtkAboutDialogPrivate *priv = about->priv;

  g_free (priv->name);
  g_free (priv->version);
  g_free (priv->copyright);
  g_free (priv->comments);
  g_free (priv->license);
  g_free (priv->website_url);
  g_free (priv->website_text);
  g_free (priv->translator_credits);

  g_strfreev (priv->authors);
  g_strfreev (priv->documenters);
  g_strfreev (priv->artists);

  g_slist_free_full (priv->credit_sections, destroy_credit_section);

  g_slist_foreach (priv->visited_links, (GFunc)g_free, NULL);
  g_slist_free (priv->visited_links);

  g_object_unref (priv->hand_cursor);
  g_object_unref (priv->regular_cursor);

  G_OBJECT_CLASS (gtk_about_dialog_parent_class)->finalize (object);
}

static void
gtk_about_dialog_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkAboutDialog *about = GTK_ABOUT_DIALOG (object);

  switch (prop_id)
    {
    case PROP_NAME:
      gtk_about_dialog_set_program_name (about, g_value_get_string (value));
      break;
    case PROP_VERSION:
      gtk_about_dialog_set_version (about, g_value_get_string (value));
      break;
    case PROP_COMMENTS:
      gtk_about_dialog_set_comments (about, g_value_get_string (value));
      break;
    case PROP_WEBSITE:
      gtk_about_dialog_set_website (about, g_value_get_string (value));
      break;
    case PROP_WEBSITE_LABEL:
      gtk_about_dialog_set_website_label (about, g_value_get_string (value));
      break;
    case PROP_LICENSE:
      gtk_about_dialog_set_license (about, g_value_get_string (value));
      break;
    case PROP_LICENSE_TYPE:
      gtk_about_dialog_set_license_type (about, g_value_get_enum (value));
      break;
    case PROP_COPYRIGHT:
      gtk_about_dialog_set_copyright (about, g_value_get_string (value));
      break;
    case PROP_LOGO:
      gtk_about_dialog_set_logo (about, g_value_get_object (value));
      break;
    case PROP_AUTHORS:
      gtk_about_dialog_set_authors (about, (const gchar**)g_value_get_boxed (value));
      break;
    case PROP_DOCUMENTERS:
      gtk_about_dialog_set_documenters (about, (const gchar**)g_value_get_boxed (value));
      break;
    case PROP_ARTISTS:
      gtk_about_dialog_set_artists (about, (const gchar**)g_value_get_boxed (value));
      break;
    case PROP_TRANSLATOR_CREDITS:
      gtk_about_dialog_set_translator_credits (about, g_value_get_string (value));
      break;
    case PROP_LOGO_ICON_NAME:
      gtk_about_dialog_set_logo_icon_name (about, g_value_get_string (value));
      break;
    case PROP_WRAP_LICENSE:
      gtk_about_dialog_set_wrap_license (about, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_about_dialog_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GtkAboutDialog *about = GTK_ABOUT_DIALOG (object);
  GtkAboutDialogPrivate *priv = about->priv;
        
  switch (prop_id) 
    {
    case PROP_NAME:
      g_value_set_string (value, priv->name);
      break;
    case PROP_VERSION:
      g_value_set_string (value, priv->version);
      break;
    case PROP_COPYRIGHT:
      g_value_set_string (value, priv->copyright);
      break;
    case PROP_COMMENTS:
      g_value_set_string (value, priv->comments);
      break;
    case PROP_WEBSITE:
      g_value_set_string (value, priv->website_url);
      break;
    case PROP_WEBSITE_LABEL:
      g_value_set_string (value, priv->website_text);
      break;
    case PROP_LICENSE:
      g_value_set_string (value, priv->license);
      break;
    case PROP_LICENSE_TYPE:
      g_value_set_enum (value, priv->license_type);
      break;
    case PROP_TRANSLATOR_CREDITS:
      g_value_set_string (value, priv->translator_credits);
      break;
    case PROP_AUTHORS:
      g_value_set_boxed (value, priv->authors);
      break;
    case PROP_DOCUMENTERS:
      g_value_set_boxed (value, priv->documenters);
      break;
    case PROP_ARTISTS:
      g_value_set_boxed (value, priv->artists);
      break;
    case PROP_LOGO:
      if (gtk_image_get_storage_type (GTK_IMAGE (priv->logo_image)) == GTK_IMAGE_PIXBUF)
        g_value_set_object (value, gtk_image_get_pixbuf (GTK_IMAGE (priv->logo_image)));
      else
        g_value_set_object (value, NULL);
      break;
    case PROP_LOGO_ICON_NAME:
      if (gtk_image_get_storage_type (GTK_IMAGE (priv->logo_image)) == GTK_IMAGE_ICON_NAME)
        {
          const gchar *icon_name;

          gtk_image_get_icon_name (GTK_IMAGE (priv->logo_image), &icon_name, NULL);
          g_value_set_string (value, icon_name);
        }
      else
        g_value_set_string (value, NULL);
      break;
    case PROP_WRAP_LICENSE:
      g_value_set_boolean (value, priv->wrap_license);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
toggle_credits (GtkToggleButton *button,
                gpointer         user_data)
{
  GtkAboutDialog *about = user_data;
  GtkAboutDialogPrivate *priv = about->priv;
  gboolean show_credits;

  if (priv->in_switch_page)
    return;

  show_credits = gtk_toggle_button_get_active (button);
  switch_page (about, show_credits ? "credits" : "main");
}

static void
toggle_license (GtkToggleButton *button,
                gpointer         user_data)
{
  GtkAboutDialog *about = user_data;
  GtkAboutDialogPrivate *priv = about->priv;
  gboolean show_license;

  if (priv->in_switch_page)
    return;

  show_license = gtk_toggle_button_get_active (button);
  switch_page (about, show_license ? "license" : "main");
}

static gboolean
gtk_about_dialog_activate_link (GtkAboutDialog *about,
                                const gchar    *uri)
{
  GdkScreen *screen;
  GError *error = NULL;

  screen = gtk_widget_get_screen (GTK_WIDGET (about));

  if (!gtk_show_uri (screen, uri, gtk_get_current_event_time (), &error))
    {
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new (GTK_WINDOW (about),
                                       GTK_DIALOG_DESTROY_WITH_PARENT |
                                       GTK_DIALOG_MODAL,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       "%s", _("Could not show link"));
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                "%s", error->message);
      g_error_free (error);

      g_signal_connect (dialog, "response",
                        G_CALLBACK (gtk_widget_destroy), NULL);

      gtk_window_present (GTK_WINDOW (dialog));
    }

  return TRUE;
}

static void
update_website (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv = about->priv;

  gtk_widget_show (priv->website_label);

  if (priv->website_url)
    {
      gchar *markup;

      if (priv->website_text)
        {
          gchar *escaped;

          escaped = g_markup_escape_text (priv->website_text, -1);
          markup = g_strdup_printf ("<a href=\"%s\">%s</a>",
                                    priv->website_url, escaped);
          g_free (escaped);
        }
      else
        {
          markup = g_strdup_printf ("<a href=\"%s\">%s</a>",
                                    priv->website_url, _("Website"));
        }

      gtk_label_set_markup (GTK_LABEL (priv->website_label), markup);
      g_free (markup);
    }
  else
    {
      if (priv->website_text)
        gtk_label_set_text (GTK_LABEL (priv->website_label), priv->website_text);
      else
        gtk_widget_hide (priv->website_label);
    }
}

static void
gtk_about_dialog_show (GtkWidget *widget)
{
  update_website (GTK_ABOUT_DIALOG (widget));

  GTK_WIDGET_CLASS (gtk_about_dialog_parent_class)->show (widget);
}

/**
 * gtk_about_dialog_get_program_name:
 * @about: a #GtkAboutDialog
 *
 * Returns the program name displayed in the about dialog.
 *
 * Returns: The program name. The string is owned by the about
 *  dialog and must not be modified.
 *
 * Since: 2.12
 */
const gchar *
gtk_about_dialog_get_program_name (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;

  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  priv = about->priv;

  return priv->name;
}

static void
update_name_version (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;
  gchar *title_string, *name_string;

  priv = about->priv;

  title_string = g_strdup_printf (_("About %s"), priv->name);
  gtk_window_set_title (GTK_WINDOW (about), title_string);
  g_free (title_string);

  if (priv->version != NULL)
    {
      gtk_label_set_markup (GTK_LABEL (priv->version_label), priv->version);
      gtk_widget_show (priv->version_label);
    }
  else
    gtk_widget_hide (priv->version_label);

  name_string = g_markup_printf_escaped ("<span weight=\"bold\">%s</span>",
                                         priv->name);
  gtk_label_set_markup (GTK_LABEL (priv->name_label), name_string);
  g_free (name_string);
}

/**
 * gtk_about_dialog_set_program_name:
 * @about: a #GtkAboutDialog
 * @name: the program name
 *
 * Sets the name to display in the about dialog.
 * If this is not set, it defaults to g_get_application_name().
 *
 * Since: 2.12
 */
void
gtk_about_dialog_set_program_name (GtkAboutDialog *about,
                                   const gchar    *name)
{
  GtkAboutDialogPrivate *priv;
  gchar *tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = about->priv;

  tmp = priv->name;
  priv->name = g_strdup (name ? name : g_get_application_name ());
  g_free (tmp);

  update_name_version (about);

  g_object_notify_by_pspec (G_OBJECT (about), props[PROP_NAME]);
}


/**
 * gtk_about_dialog_get_version:
 * @about: a #GtkAboutDialog
 *
 * Returns the version string.
 *
 * Returns: The version string. The string is owned by the about
 *  dialog and must not be modified.
 *
 * Since: 2.6
 */
const gchar *
gtk_about_dialog_get_version (GtkAboutDialog *about)
{
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  return about->priv->version;
}

/**
 * gtk_about_dialog_set_version:
 * @about: a #GtkAboutDialog
 * @version: (allow-none): the version string
 *
 * Sets the version string to display in the about dialog.
 *
 * Since: 2.6
 */
void
gtk_about_dialog_set_version (GtkAboutDialog *about,
                              const gchar    *version)
{
  GtkAboutDialogPrivate *priv;
  gchar *tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = about->priv;

  tmp = priv->version;
  priv->version = g_strdup (version);
  g_free (tmp);

  update_name_version (about);

  g_object_notify_by_pspec (G_OBJECT (about), props[PROP_VERSION]);
}

/**
 * gtk_about_dialog_get_copyright:
 * @about: a #GtkAboutDialog
 *
 * Returns the copyright string.
 *
 * Returns: The copyright string. The string is owned by the about
 *  dialog and must not be modified.
 *
 * Since: 2.6
 */
const gchar *
gtk_about_dialog_get_copyright (GtkAboutDialog *about)
{
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  return about->priv->copyright;
}

/**
 * gtk_about_dialog_set_copyright:
 * @about: a #GtkAboutDialog
 * @copyright: (allow-none): the copyright string
 *
 * Sets the copyright string to display in the about dialog.
 * This should be a short string of one or two lines.
 *
 * Since: 2.6
 */
void
gtk_about_dialog_set_copyright (GtkAboutDialog *about,
                                const gchar    *copyright)
{
  GtkAboutDialogPrivate *priv;
  gchar *copyright_string, *tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = about->priv;

  tmp = priv->copyright;
  priv->copyright = g_strdup (copyright);
  g_free (tmp);

  if (priv->copyright != NULL)
    {
      copyright_string = g_markup_printf_escaped ("<span size=\"small\">%s</span>",
                                                  priv->copyright);
      gtk_label_set_markup (GTK_LABEL (priv->copyright_label), copyright_string);
      g_free (copyright_string);

      gtk_widget_show (priv->copyright_label);
    }
  else
    gtk_widget_hide (priv->copyright_label);

  g_object_notify_by_pspec (G_OBJECT (about), props[PROP_COPYRIGHT]);
}

/**
 * gtk_about_dialog_get_comments:
 * @about: a #GtkAboutDialog
 *
 * Returns the comments string.
 *
 * Returns: The comments. The string is owned by the about
 *  dialog and must not be modified.
 *
 * Since: 2.6
 */
const gchar *
gtk_about_dialog_get_comments (GtkAboutDialog *about)
{
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  return about->priv->comments;
}

/**
 * gtk_about_dialog_set_comments:
 * @about: a #GtkAboutDialog
 * @comments: (allow-none): a comments string
 *
 * Sets the comments string to display in the about dialog.
 * This should be a short string of one or two lines.
 *
 * Since: 2.6
 */
void
gtk_about_dialog_set_comments (GtkAboutDialog *about,
                               const gchar    *comments)
{
  GtkAboutDialogPrivate *priv;
  gchar *tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = about->priv;

  tmp = priv->comments;
  if (comments)
    {
      priv->comments = g_strdup (comments);
      gtk_label_set_text (GTK_LABEL (priv->comments_label), priv->comments);
      gtk_widget_show (priv->comments_label);
    }
  else
    {
      priv->comments = NULL;
      gtk_widget_hide (priv->comments_label);
    }
  g_free (tmp);

  g_object_notify_by_pspec (G_OBJECT (about), props[PROP_COMMENTS]);
}

/**
 * gtk_about_dialog_get_license:
 * @about: a #GtkAboutDialog
 *
 * Returns the license information.
 *
 * Returns: The license information. The string is owned by the about
 *  dialog and must not be modified.
 *
 * Since: 2.6
 */
const gchar *
gtk_about_dialog_get_license (GtkAboutDialog *about)
{
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  return about->priv->license;
}

/**
 * gtk_about_dialog_set_license:
 * @about: a #GtkAboutDialog
 * @license: (allow-none): the license information or %NULL
 *
 * Sets the license information to be displayed in the secondary
 * license dialog. If @license is %NULL, the license button is
 * hidden.
 *
 * Since: 2.6
 */
void
gtk_about_dialog_set_license (GtkAboutDialog *about,
                              const gchar    *license)
{
  GtkAboutDialogPrivate *priv;
  gchar *tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = about->priv;

  tmp = priv->license;
  if (license)
    {
      priv->license = g_strdup (license);
      priv->license_type = GTK_LICENSE_CUSTOM;
    }
  else
    {
      priv->license = NULL;
      priv->license_type = GTK_LICENSE_UNKNOWN;
    }
  g_free (tmp);

  gtk_widget_hide (priv->license_label);

  update_license_button_visibility (about);

  g_object_notify_by_pspec (G_OBJECT (about), props[PROP_LICENSE]);
  g_object_notify_by_pspec (G_OBJECT (about), props[PROP_LICENSE_TYPE]);
}

/**
 * gtk_about_dialog_get_wrap_license:
 * @about: a #GtkAboutDialog
 *
 * Returns whether the license text in @about is
 * automatically wrapped.
 *
 * Returns: %TRUE if the license text is wrapped
 *
 * Since: 2.8
 */
gboolean
gtk_about_dialog_get_wrap_license (GtkAboutDialog *about)
{
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), FALSE);

  return about->priv->wrap_license;
}

/**
 * gtk_about_dialog_set_wrap_license:
 * @about: a #GtkAboutDialog
 * @wrap_license: whether to wrap the license
 *
 * Sets whether the license text in @about is
 * automatically wrapped.
 *
 * Since: 2.8
 */
void
gtk_about_dialog_set_wrap_license (GtkAboutDialog *about,
                                   gboolean        wrap_license)
{
  GtkAboutDialogPrivate *priv;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = about->priv;

  wrap_license = wrap_license != FALSE;

  if (priv->wrap_license != wrap_license)
    {
       priv->wrap_license = wrap_license;

       g_object_notify_by_pspec (G_OBJECT (about), props[PROP_WRAP_LICENSE]);
    }
}

/**
 * gtk_about_dialog_get_website:
 * @about: a #GtkAboutDialog
 *
 * Returns the website URL.
 *
 * Returns: The website URL. The string is owned by the about
 *  dialog and must not be modified.
 *
 * Since: 2.6
 */
const gchar *
gtk_about_dialog_get_website (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;

  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  priv = about->priv;

  return priv->website_url;
}

/**
 * gtk_about_dialog_set_website:
 * @about: a #GtkAboutDialog
 * @website: (allow-none): a URL string starting with "http://"
 *
 * Sets the URL to use for the website link.
 *
 * Since: 2.6
 */
void
gtk_about_dialog_set_website (GtkAboutDialog *about,
                              const gchar    *website)
{
  GtkAboutDialogPrivate *priv;
  gchar *tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = about->priv;

  tmp = priv->website_url;
  priv->website_url = g_strdup (website);
  g_free (tmp);

  update_website (about);

  g_object_notify_by_pspec (G_OBJECT (about), props[PROP_WEBSITE]);
}

/**
 * gtk_about_dialog_get_website_label:
 * @about: a #GtkAboutDialog
 *
 * Returns the label used for the website link.
 *
 * Returns: The label used for the website link. The string is
 *     owned by the about dialog and must not be modified.
 *
 * Since: 2.6
 */
const gchar *
gtk_about_dialog_get_website_label (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;

  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  priv = about->priv;

  return priv->website_text;
}

/**
 * gtk_about_dialog_set_website_label:
 * @about: a #GtkAboutDialog
 * @website_label: the label used for the website link
 *
 * Sets the label to be used for the website link.
 *
 * Since: 2.6
 */
void
gtk_about_dialog_set_website_label (GtkAboutDialog *about,
                                    const gchar    *website_label)
{
  GtkAboutDialogPrivate *priv;
  gchar *tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = about->priv;

  tmp = priv->website_text;
  priv->website_text = g_strdup (website_label);
  g_free (tmp);

  update_website (about);

  g_object_notify_by_pspec (G_OBJECT (about), props[PROP_WEBSITE_LABEL]);
}

/**
 * gtk_about_dialog_get_authors:
 * @about: a #GtkAboutDialog
 *
 * Returns the string which are displayed in the authors tab
 * of the secondary credits dialog.
 *
 * Returns: (array zero-terminated=1) (transfer none): A
 *  %NULL-terminated string array containing the authors. The array is
 *  owned by the about dialog and must not be modified.
 *
 * Since: 2.6
 */
const gchar * const *
gtk_about_dialog_get_authors (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;

  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  priv = about->priv;

  return (const gchar * const *) priv->authors;
}

/**
 * gtk_about_dialog_set_authors:
 * @about: a #GtkAboutDialog
 * @authors: (array zero-terminated=1): a %NULL-terminated array of strings
 *
 * Sets the strings which are displayed in the authors tab
 * of the secondary credits dialog.
 *
 * Since: 2.6
 */
void
gtk_about_dialog_set_authors (GtkAboutDialog  *about,
                              const gchar    **authors)
{
  GtkAboutDialogPrivate *priv;
  gchar **tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = about->priv;

  tmp = priv->authors;
  priv->authors = g_strdupv ((gchar **)authors);
  g_strfreev (tmp);

  update_credits_button_visibility (about);

  g_object_notify_by_pspec (G_OBJECT (about), props[PROP_AUTHORS]);
}

/**
 * gtk_about_dialog_get_documenters:
 * @about: a #GtkAboutDialog
 *
 * Returns the string which are displayed in the documenters
 * tab of the secondary credits dialog.
 *
 * Returns: (array zero-terminated=1) (transfer none): A
 *  %NULL-terminated string array containing the documenters. The
 *  array is owned by the about dialog and must not be modified.
 *
 * Since: 2.6
 */
const gchar * const *
gtk_about_dialog_get_documenters (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;

  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  priv = about->priv;

  return (const gchar * const *)priv->documenters;
}

/**
 * gtk_about_dialog_set_documenters:
 * @about: a #GtkAboutDialog
 * @documenters: (array zero-terminated=1): a %NULL-terminated array of strings
 *
 * Sets the strings which are displayed in the documenters tab
 * of the secondary credits dialog.
 *
 * Since: 2.6
 */
void
gtk_about_dialog_set_documenters (GtkAboutDialog *about,
                                  const gchar   **documenters)
{
  GtkAboutDialogPrivate *priv;
  gchar **tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = about->priv;

  tmp = priv->documenters;
  priv->documenters = g_strdupv ((gchar **)documenters);
  g_strfreev (tmp);

  update_credits_button_visibility (about);

  g_object_notify_by_pspec (G_OBJECT (about), props[PROP_DOCUMENTERS]);
}

/**
 * gtk_about_dialog_get_artists:
 * @about: a #GtkAboutDialog
 *
 * Returns the string which are displayed in the artists tab
 * of the secondary credits dialog.
 *
 * Returns: (array zero-terminated=1) (transfer none): A
 *  %NULL-terminated string array containing the artists. The array is
 *  owned by the about dialog and must not be modified.
 *
 * Since: 2.6
 */
const gchar * const *
gtk_about_dialog_get_artists (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;

  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  priv = about->priv;

  return (const gchar * const *)priv->artists;
}

/**
 * gtk_about_dialog_set_artists:
 * @about: a #GtkAboutDialog
 * @artists: (array zero-terminated=1): a %NULL-terminated array of strings
 *
 * Sets the strings which are displayed in the artists tab
 * of the secondary credits dialog.
 *
 * Since: 2.6
 */
void
gtk_about_dialog_set_artists (GtkAboutDialog *about,
                              const gchar   **artists)
{
  GtkAboutDialogPrivate *priv;
  gchar **tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = about->priv;

  tmp = priv->artists;
  priv->artists = g_strdupv ((gchar **)artists);
  g_strfreev (tmp);

  update_credits_button_visibility (about);

  g_object_notify_by_pspec (G_OBJECT (about), props[PROP_ARTISTS]);
}

/**
 * gtk_about_dialog_get_translator_credits:
 * @about: a #GtkAboutDialog
 *
 * Returns the translator credits string which is displayed
 * in the translators tab of the secondary credits dialog.
 *
 * Returns: The translator credits string. The string is
 *   owned by the about dialog and must not be modified.
 *
 * Since: 2.6
 */
const gchar *
gtk_about_dialog_get_translator_credits (GtkAboutDialog *about)
{
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  return about->priv->translator_credits;
}

/**
 * gtk_about_dialog_set_translator_credits:
 * @about: a #GtkAboutDialog
 * @translator_credits: (allow-none): the translator credits
 *
 * Sets the translator credits string which is displayed in
 * the translators tab of the secondary credits dialog.
 *
 * The intended use for this string is to display the translator
 * of the language which is currently used in the user interface.
 * Using gettext(), a simple way to achieve that is to mark the
 * string for translation:
 * |[<!-- language="C" -->
 *  gtk_about_dialog_set_translator_credits (about,
 *                                           _("translator-credits"));
 * ]|
 * It is a good idea to use the customary msgid “translator-credits” for this
 * purpose, since translators will already know the purpose of that msgid, and
 * since #GtkAboutDialog will detect if “translator-credits” is untranslated
 * and hide the tab.
 *
 * Since: 2.6
 */
void
gtk_about_dialog_set_translator_credits (GtkAboutDialog *about,
                                         const gchar    *translator_credits)
{
  GtkAboutDialogPrivate *priv;
  gchar *tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = about->priv;

  tmp = priv->translator_credits;
  priv->translator_credits = g_strdup (translator_credits);
  g_free (tmp);

  update_credits_button_visibility (about);

  g_object_notify_by_pspec (G_OBJECT (about), props[PROP_TRANSLATOR_CREDITS]);
}

/**
 * gtk_about_dialog_get_logo:
 * @about: a #GtkAboutDialog
 *
 * Returns the pixbuf displayed as logo in the about dialog.
 *
 * Returns: (transfer none): the pixbuf displayed as logo. The
 *   pixbuf is owned by the about dialog. If you want to keep a
 *   reference to it, you have to call g_object_ref() on it.
 *
 * Since: 2.6
 */
GdkPixbuf *
gtk_about_dialog_get_logo (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;

  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  priv = about->priv;

  if (gtk_image_get_storage_type (GTK_IMAGE (priv->logo_image)) == GTK_IMAGE_PIXBUF)
    return gtk_image_get_pixbuf (GTK_IMAGE (priv->logo_image));
  else
    return NULL;
}

/**
 * gtk_about_dialog_set_logo:
 * @about: a #GtkAboutDialog
 * @logo: (allow-none): a #GdkPixbuf, or %NULL
 *
 * Sets the pixbuf to be displayed as logo in the about dialog.
 * If it is %NULL, the default window icon set with
 * gtk_window_set_default_icon() will be used.
 *
 * Since: 2.6
 */
void
gtk_about_dialog_set_logo (GtkAboutDialog *about,
                           GdkPixbuf      *logo)
{
  GtkAboutDialogPrivate *priv;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = about->priv;

  g_object_freeze_notify (G_OBJECT (about));

  if (gtk_image_get_storage_type (GTK_IMAGE (priv->logo_image)) == GTK_IMAGE_ICON_NAME)
    g_object_notify_by_pspec (G_OBJECT (about), props[PROP_LOGO_ICON_NAME]);

  if (logo != NULL)
    gtk_image_set_from_pixbuf (GTK_IMAGE (priv->logo_image), logo);
  else
    {
      GList *pixbufs = gtk_window_get_default_icon_list ();

      if (pixbufs != NULL)
        {
          gtk_image_set_from_pixbuf (GTK_IMAGE (priv->logo_image),
                                     GDK_PIXBUF (pixbufs->data));

          g_list_free (pixbufs);
        }
    }

  g_object_notify_by_pspec (G_OBJECT (about), props[PROP_LOGO]);

  g_object_thaw_notify (G_OBJECT (about));
}

/**
 * gtk_about_dialog_get_logo_icon_name:
 * @about: a #GtkAboutDialog
 *
 * Returns the icon name displayed as logo in the about dialog.
 *
 * Returns: the icon name displayed as logo. The string is
 *   owned by the dialog. If you want to keep a reference
 *   to it, you have to call g_strdup() on it.
 *
 * Since: 2.6
 */
const gchar *
gtk_about_dialog_get_logo_icon_name (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;
  const gchar *icon_name = NULL;

  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  priv = about->priv;

  if (gtk_image_get_storage_type (GTK_IMAGE (priv->logo_image)) == GTK_IMAGE_ICON_NAME)
    gtk_image_get_icon_name (GTK_IMAGE (priv->logo_image), &icon_name, NULL);

  return icon_name;
}

/**
 * gtk_about_dialog_set_logo_icon_name:
 * @about: a #GtkAboutDialog
 * @icon_name: (allow-none): an icon name, or %NULL
 *
 * Sets the pixbuf to be displayed as logo in the about dialog.
 * If it is %NULL, the default window icon set with
 * gtk_window_set_default_icon() will be used.
 *
 * Since: 2.6
 */
void
gtk_about_dialog_set_logo_icon_name (GtkAboutDialog *about,
                                     const gchar    *icon_name)
{
  GtkAboutDialogPrivate *priv;
  GList *icons;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = about->priv;

  g_object_freeze_notify (G_OBJECT (about));

  if (gtk_image_get_storage_type (GTK_IMAGE (priv->logo_image)) == GTK_IMAGE_PIXBUF)
    g_object_notify_by_pspec (G_OBJECT (about), props[PROP_LOGO]);

  if (icon_name)
    {
      gint *sizes = gtk_icon_theme_get_icon_sizes (gtk_icon_theme_get_default (),
                                                   icon_name);
      gint i, best_size = 0;

      for (i = 0; sizes[i]; i++)
        {
          if (sizes[i] >= 128 || sizes[i] == -1)
            {
              best_size = 128;
              break;
            }
          else if (sizes[i] >= 96)
            {
              best_size = MAX (96, best_size);
            }
          else if (sizes[i] >= 64)
            {
              best_size = MAX (64, best_size);
            }
          else
            {
              best_size = MAX (48, best_size);
            }
        }
      g_free (sizes);

      gtk_image_set_from_icon_name (GTK_IMAGE (priv->logo_image), icon_name,
                                GTK_ICON_SIZE_DIALOG);
      gtk_image_set_pixel_size (GTK_IMAGE (priv->logo_image), best_size);
    }
  else if ((icons = gtk_window_get_default_icon_list ()))
    {
      gtk_image_set_from_pixbuf (GTK_IMAGE (priv->logo_image), icons->data);
      g_list_free (icons);
    }
  else
    {
      gtk_image_clear (GTK_IMAGE (priv->logo_image));
    }

  g_object_notify_by_pspec (G_OBJECT (about), props[PROP_LOGO_ICON_NAME]);

  g_object_thaw_notify (G_OBJECT (about));
}

static void
follow_if_link (GtkAboutDialog *about,
                GtkTextView    *text_view,
                GtkTextIter    *iter)
{
  GSList *tags = NULL, *tagp = NULL;
  GtkAboutDialogPrivate *priv = about->priv;
  gchar *uri = NULL;

  tags = gtk_text_iter_get_tags (iter);
  for (tagp = tags; tagp != NULL && !uri; tagp = tagp->next)
    {
      GtkTextTag *tag = tagp->data;

      uri = g_object_get_data (G_OBJECT (tag), "uri");
      if (uri)
        emit_activate_link (about, uri);

      if (uri && !g_slist_find_custom (priv->visited_links, uri, (GCompareFunc)strcmp))
        {
          GdkRGBA visited_link_color;
          GtkStyleContext *context = gtk_widget_get_style_context (GTK_WIDGET (about));
          GtkStateFlags state = gtk_widget_get_state_flags (GTK_WIDGET (about));
          gtk_style_context_get_color (context, state | GTK_STATE_FLAG_VISITED, &visited_link_color);

          g_object_set (G_OBJECT (tag), "foreground-rgba", visited_link_color, NULL);

          priv->visited_links = g_slist_prepend (priv->visited_links, g_strdup (uri));
        }
    }

  if (tags)
    g_slist_free (tags);
}

static gboolean
text_view_key_press_event (GtkWidget      *text_view,
                           GdkEventKey    *event,
                           GtkAboutDialog *about)
{
  GtkTextIter iter;
  GtkTextBuffer *buffer;

  switch (event->keyval)
    {
      case GDK_KEY_Return:
      case GDK_KEY_ISO_Enter:
      case GDK_KEY_KP_Enter:
        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
        gtk_text_buffer_get_iter_at_mark (buffer, &iter,
                                          gtk_text_buffer_get_insert (buffer));
        follow_if_link (about, GTK_TEXT_VIEW (text_view), &iter);
        break;

      default:
        break;
    }

  return FALSE;
}

static gboolean
text_view_event_after (GtkWidget      *text_view,
                       GdkEvent       *event,
                       GtkAboutDialog *about)
{
  GtkTextIter start, end, iter;
  GtkTextBuffer *buffer;
  GdkEventButton *button_event;
  gint x, y;

  if (event->type != GDK_BUTTON_RELEASE)
    return FALSE;

  button_event = (GdkEventButton *)event;

  if (button_event->button != GDK_BUTTON_PRIMARY)
    return FALSE;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));

  /* we shouldn't follow a link if the user has selected something */
  gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
  if (gtk_text_iter_get_offset (&start) != gtk_text_iter_get_offset (&end))
    return FALSE;

  gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (text_view),
                                         GTK_TEXT_WINDOW_WIDGET,
                                         button_event->x, button_event->y, &x, &y);

  gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (text_view), &iter, x, y);

  follow_if_link (about, GTK_TEXT_VIEW (text_view), &iter);

  return FALSE;
}

static void
set_cursor_if_appropriate (GtkAboutDialog *about,
                           GtkTextView    *text_view,
                           GdkDevice      *device,
                           gint            x,
                           gint            y)
{
  GtkAboutDialogPrivate *priv = about->priv;
  GSList *tags = NULL, *tagp = NULL;
  GtkTextIter iter;
  gboolean hovering_over_link = FALSE;

  gtk_text_view_get_iter_at_location (text_view, &iter, x, y);

  tags = gtk_text_iter_get_tags (&iter);
  for (tagp = tags;  tagp != NULL;  tagp = tagp->next)
    {
      GtkTextTag *tag = tagp->data;
      gchar *uri = g_object_get_data (G_OBJECT (tag), "uri");

      if (uri != NULL)
        {
          hovering_over_link = TRUE;
          break;
        }
    }

  if (hovering_over_link != priv->hovering_over_link)
    {
      priv->hovering_over_link = hovering_over_link;

      if (hovering_over_link)
        gdk_window_set_device_cursor (gtk_text_view_get_window (text_view, GTK_TEXT_WINDOW_TEXT), device, priv->hand_cursor);
      else
        gdk_window_set_device_cursor (gtk_text_view_get_window (text_view, GTK_TEXT_WINDOW_TEXT), device, priv->regular_cursor);
    }

  if (tags)
    g_slist_free (tags);
}

static gboolean
text_view_motion_notify_event (GtkWidget *text_view,
                               GdkEventMotion *event,
                               GtkAboutDialog *about)
{
  gint x, y;

  gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (text_view),
                                         GTK_TEXT_WINDOW_WIDGET,
                                         event->x, event->y, &x, &y);

  set_cursor_if_appropriate (about, GTK_TEXT_VIEW (text_view), event->device, x, y);

  gdk_event_request_motions (event);

  return FALSE;
}

static GtkTextBuffer *
text_buffer_new (GtkAboutDialog  *about,
		 gchar          **strings)
{
  gchar **p;
  gchar *q0, *q1, *q2, *r1, *r2;
  GtkTextBuffer *buffer;
  GdkRGBA color;
  GdkRGBA link_color;
  GdkRGBA visited_link_color;
  GtkAboutDialogPrivate *priv = about->priv;
  GtkTextIter start_iter, end_iter;
  GtkTextTag *tag;
  GtkStateFlags state = gtk_widget_get_state_flags (GTK_WIDGET (about));
  GtkStyleContext *context = gtk_widget_get_style_context (GTK_WIDGET (about));

  gtk_style_context_get_color (context, state | GTK_STATE_FLAG_LINK, &link_color);
  gtk_style_context_get_color (context, state | GTK_STATE_FLAG_VISITED, &visited_link_color);
  buffer = gtk_text_buffer_new (NULL);

  for (p = strings; *p; p++)
    {
      q0  = *p;
      while (*q0)
        {
          q1 = strchr (q0, '<');
          q2 = q1 ? strchr (q1, '>') : NULL;
          r1 = strstr (q0, "http://");
          r2 = strstr (q0, "https://");
          if (!r1 || (r1 && r2 && r2 < r1))
            r1 = r2;
          if (r1)
            {
              r2 = strpbrk (r1, " \n\t>");
              if (!r2)
                r2 = strchr (r1, '\0');
            }
          else
            r2 = NULL;

          if (r1 && r2 && (!q1 || !q2 || (r1 <= q1 + 1)))
            {
              q1 = r1;
              q2 = r2;
            }

          if (q1 && q2)
            {
              GtkTextIter end;
              gchar *link;
              gchar *uri;
              const gchar *link_type;
              GtkTextTag *tag;

              if (*q1 == '<')
                {
                  gtk_text_buffer_insert_at_cursor (buffer, q0, q1 - q0 + 1);
                  gtk_text_buffer_get_end_iter (buffer, &end);
                  q1++;
                  link_type = "email";
                }
              else
                {
                  gtk_text_buffer_insert_at_cursor (buffer, q0, q1 - q0);
                  gtk_text_buffer_get_end_iter (buffer, &end);
                  link_type = "uri";
                }

              q0 = q2;

              link = g_strndup (q1, q2 - q1);

              if (g_slist_find_custom (priv->visited_links, link, (GCompareFunc)strcmp))
                color = visited_link_color;
              else
                color = link_color;

              tag = gtk_text_buffer_create_tag (buffer, NULL,
                                                "foreground-rgba", &color,
                                                "underline", PANGO_UNDERLINE_SINGLE,
                                                NULL);
              if (strcmp (link_type, "email") == 0)
                {
                  gchar *escaped;

                  escaped = g_uri_escape_string (link, NULL, FALSE);
                  uri = g_strconcat ("mailto:", escaped, NULL);
                  g_free (escaped);
                }
              else
                {
                  uri = g_strdup (link);
                }
              g_object_set_data_full (G_OBJECT (tag), I_("uri"), uri, g_free);
              gtk_text_buffer_insert_with_tags (buffer, &end, link, -1, tag, NULL);

              g_free (link);
            }
          else
            {
              gtk_text_buffer_insert_at_cursor (buffer, q0, -1);
              break;
            }
        }

      if (p[1])
        gtk_text_buffer_insert_at_cursor (buffer, "\n", 1);
    }

  tag = gtk_text_buffer_create_tag (buffer, NULL,
                                    "scale", PANGO_SCALE_SMALL,
                                    NULL);

  gtk_text_buffer_get_start_iter (buffer, &start_iter);
  gtk_text_buffer_get_end_iter (buffer, &end_iter);
  gtk_text_buffer_apply_tag (buffer, tag, &start_iter, &end_iter);

  return buffer;
}

static void
add_credits_section (GtkAboutDialog *about,
                     GtkGrid        *grid,
                     gint           *row,
                     gchar          *title,
                     gchar         **people)
{
  GtkWidget *label;
  gchar *markup;
  gchar **p;
  gchar *q0, *q1, *q2, *r1, *r2;

  if (people == NULL)
    return;

  markup = g_strdup_printf ("<span size=\"small\">%s</span>", title);
  label = gtk_label_new (markup);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  g_free (markup);
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
  gtk_grid_attach (grid, label, 0, *row, 1, 1);
  gtk_widget_show (label);

  for (p = people; *p; p++)
    {
      GString *str;

      str = g_string_new ("<span size=\"small\">");

      q0 = *p;
      while (*q0)
        {
          q1 = strchr (q0, '<');
          q2 = q1 ? strchr (q1, '>') : NULL;
          r1 = strstr (q0, "http://");
          r2 = strstr (q0, "https://");
          if (!r1 || (r1 && r2 && r2 < r1))
            r1 = r2;
          if (r1)
            {
              r2 = strpbrk (r1, " \n\t");
              if (!r2)
                r2 = strchr (r1, '\0');
            }
          else
            r2 = NULL;

          if (r1 && r2 && (!q1 || !q2 || (r1 < q1)))
            {
              q1 = r1;
              q2 = r2;
            }
          else if (q1 && (q1[1] == 'a' || q1[1] == 'A') && q1[2] == ' ')
            {
              /* if it is a <a> link leave it for the label to parse */
              q1 = NULL;
            }

          if (q1 && q2)
            {
              gchar *link;
              gchar *text;
              gchar *name;

              if (*q1 == '<')
                {
                  /* email */
                  gchar *escaped;

                  text = g_strstrip (g_strndup (q0, q1 - q0));
                  name = g_markup_escape_text (text, -1);
                  q1++;
                  link = g_strndup (q1, q2 - q1);
                  q2++;
                  escaped = g_uri_escape_string (link, NULL, FALSE);
                  g_string_append_printf (str,
                                          "<a href=\"mailto:%s\">%s</a>",
                                          escaped,
                                          name[0] ? name : link);
                  g_free (escaped);
                  g_free (link);
                  g_free (text);
                  g_free (name);
                }
              else
                {
                  /* uri */
                  text = g_strstrip (g_strndup (q0, q1 - q0));
                  name = g_markup_escape_text (text, -1);
                  link = g_strndup (q1, q2 - q1);
                  g_string_append_printf (str,
                                          "<a href=\"%s\">%s</a>",
                                          link,
                                          name[0] ? name : link);
                  g_free (link);
                  g_free (text);
                  g_free (name);
                }

              q0 = q2;
            }
          else
            {
              g_string_append (str, q0);
              break;
            }
        }
      g_string_append (str, "</span>");

      label = gtk_label_new (str->str);
      gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
      g_signal_connect_swapped (label, "activate-link",
                                G_CALLBACK (emit_activate_link), about);
      g_string_free (str, TRUE);
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_grid_attach (grid, label, 1, *row, 1, 1);
      gtk_widget_show (label);
      (*row)++;
    }

  /* skip one at the end */
  label = gtk_label_new ("");
  gtk_grid_attach (grid, label, 1, *row, 1, 1);
  (*row)++;
}

static void
populate_credits_page (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv = about->priv;
  gint row;

  row = 0;

  if (priv->authors != NULL)
    add_credits_section (about, GTK_GRID (priv->credits_grid), &row, _("Created by"), priv->authors);

  if (priv->documenters != NULL)
    add_credits_section (about, GTK_GRID (priv->credits_grid), &row, _("Documented by"), priv->documenters);

  /* Don't show an untranslated gettext msgid */
  if (priv->translator_credits != NULL &&
      strcmp (priv->translator_credits, "translator_credits") != 0 &&
      strcmp (priv->translator_credits, "translator-credits") != 0)
    {
      gchar **translators;

      translators = g_strsplit (priv->translator_credits, "\n", 0);
      add_credits_section (about, GTK_GRID (priv->credits_grid), &row, _("Translated by"), translators);
      g_strfreev (translators);
    }

  if (priv->artists != NULL)
    add_credits_section (about, GTK_GRID (priv->credits_grid), &row, _("Artwork by"), priv->artists);

  if (priv->credit_sections != NULL)
    {
      GSList *cs;
      for (cs = priv->credit_sections; cs != NULL; cs = cs->next)
	{
	  CreditSection *section = cs->data;
	  add_credits_section (about, GTK_GRID (priv->credits_grid), &row, section->heading, section->people);
	}
    }
}

static void
populate_license_page (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv = about->priv;
  GtkTextBuffer *buffer;
  gchar *strings[2];

  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (priv->license_view), priv->wrap_license ? GTK_WRAP_WORD : GTK_WRAP_NONE);

  strings[0] = priv->license;
  strings[1] = NULL;
  buffer = text_buffer_new (about, strings);
  gtk_text_view_set_buffer (GTK_TEXT_VIEW (priv->license_view), buffer);
  g_object_unref (buffer);
}

/**
 * gtk_about_dialog_new:
 *
 * Creates a new #GtkAboutDialog.
 *
 * Returns: a newly created #GtkAboutDialog
 *
 * Since: 2.6
 */
GtkWidget *
gtk_about_dialog_new (void)
{
  return g_object_new (GTK_TYPE_ABOUT_DIALOG, NULL);
}

static void
close_cb (GtkAboutDialog *about)
{
  switch_page (about, "main");

  gtk_widget_hide (GTK_WIDGET (about));
}

/**
 * gtk_show_about_dialog:
 * @parent: (allow-none): transient parent, or %NULL for none
 * @first_property_name: the name of the first property
 * @...: value of first property, followed by more properties, %NULL-terminated
 *
 * This is a convenience function for showing an application’s about box.
 * The constructed dialog is associated with the parent window and
 * reused for future invocations of this function.
 *
 * Since: 2.6
 */
void
gtk_show_about_dialog (GtkWindow   *parent,
                       const gchar *first_property_name,
                       ...)
{
  static GtkWidget *global_about_dialog = NULL;
  GtkWidget *dialog = NULL;
  va_list var_args;

  if (parent)
    dialog = g_object_get_data (G_OBJECT (parent), "gtk-about-dialog");
  else
    dialog = global_about_dialog;

  if (!dialog)
    {
      dialog = gtk_about_dialog_new ();

      g_object_ref_sink (dialog);

      g_signal_connect (dialog, "delete-event",
                        G_CALLBACK (gtk_widget_hide_on_delete), NULL);

      /* Close dialog on user response */
      g_signal_connect (dialog, "response",
                        G_CALLBACK (close_cb), NULL);

      va_start (var_args, first_property_name);
      g_object_set_valist (G_OBJECT (dialog), first_property_name, var_args);
      va_end (var_args);

      if (parent)
        {
          gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
          gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
          gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
          g_object_set_data_full (G_OBJECT (parent),
                                  I_("gtk-about-dialog"),
                                  dialog, g_object_unref);
        }
      else
        global_about_dialog = dialog;

    }

  gtk_window_present (GTK_WINDOW (dialog));
}

/**
 * gtk_about_dialog_set_license_type:
 * @about: a #GtkAboutDialog
 * @license_type: the type of license
 *
 * Sets the license of the application showing the @about dialog from a
 * list of known licenses.
 *
 * This function overrides the license set using
 * gtk_about_dialog_set_license().
 *
 * Since: 3.0
 */
void
gtk_about_dialog_set_license_type (GtkAboutDialog *about,
                                   GtkLicense      license_type)
{
  GtkAboutDialogPrivate *priv;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));
  g_return_if_fail (license_type >= GTK_LICENSE_UNKNOWN &&
                    license_type <= GTK_LICENSE_LGPL_3_0_ONLY);

  priv = about->priv;

  if (priv->license_type != license_type)
    {
      g_object_freeze_notify (G_OBJECT (about));

      priv->license_type = license_type;

      /* custom licenses use the contents of the :license property */
      if (priv->license_type != GTK_LICENSE_CUSTOM)
        {
          const gchar *name;
          const gchar *url;
          gchar *license_string;
          GString *str;

          name = _(gtk_license_info[priv->license_type].name);
          url = gtk_license_info[priv->license_type].url;
          if (url == NULL)
            url = priv->website_url;

          str = g_string_sized_new (256);
          g_string_append_printf (str, _(gtk_license_preamble), url, name);

          g_free (priv->license);
          priv->license = g_string_free (str, FALSE);
          priv->wrap_license = TRUE;

          license_string = g_strdup_printf ("<span size=\"small\">%s</span>",
                                            priv->license);
          gtk_label_set_markup (GTK_LABEL (priv->license_label), license_string);
          g_free (license_string);
          gtk_widget_show (priv->license_label);

          update_license_button_visibility (about);

          g_object_notify_by_pspec (G_OBJECT (about), props[PROP_WRAP_LICENSE]);
          g_object_notify_by_pspec (G_OBJECT (about), props[PROP_LICENSE]);
        }
      else
        {
          gtk_widget_show (priv->license_label);
        }

      g_object_notify_by_pspec (G_OBJECT (about), props[PROP_LICENSE_TYPE]);

      g_object_thaw_notify (G_OBJECT (about));
    }
}

/**
 * gtk_about_dialog_get_license_type:
 * @about: a #GtkAboutDialog
 *
 * Retrieves the license set using gtk_about_dialog_set_license_type()
 *
 * Returns: a #GtkLicense value
 *
 * Since: 3.0
 */
GtkLicense
gtk_about_dialog_get_license_type (GtkAboutDialog *about)
{
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), GTK_LICENSE_UNKNOWN);

  return about->priv->license_type;
}

/**
 * gtk_about_dialog_add_credit_section:
 * @about: A #GtkAboutDialog
 * @section_name: The name of the section
 * @people: (array zero-terminated=1): The people who belong to that section
 *
 * Creates a new section in the Credits page.
 *
 * Since: 3.4
 */
void
gtk_about_dialog_add_credit_section (GtkAboutDialog  *about,
                                     const gchar     *section_name,
                                     const gchar    **people)
{
  GtkAboutDialogPrivate *priv;
  CreditSection *new_entry;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));
  g_return_if_fail (section_name != NULL);
  g_return_if_fail (people != NULL);

  priv = about->priv;

  new_entry = g_slice_new (CreditSection);
  new_entry->heading = g_strdup ((gchar *)section_name);
  new_entry->people = g_strdupv ((gchar **)people);

  priv->credit_sections = g_slist_append (priv->credit_sections, new_entry);
  update_credits_button_visibility (about);
}
