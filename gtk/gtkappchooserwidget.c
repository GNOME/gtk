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

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

#include "gtkmarshalers.h"
#include "gtkappchooserwidget.h"
#include "deprecated/gtkappchooserprivate.h"
#include "gtkeventcontrollerkey.h"
#include "gtkflattenlistmodel.h"
#include "gtklistheader.h"
#include "gtklistview.h"
#include "gtksignallistitemfactory.h"
#include "gtksingleselection.h"
#include "gtksortlistmodel.h"
#include "gtkstringsorter.h"
#include "gtkorientable.h"
#include "gtkscrolledwindow.h"
#include "gtklabel.h"
#include "gtkgestureclick.h"
#include "gtkwidgetprivate.h"
#include "gtkprivate.h"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <gio/gio.h>


/**
 * GtkAppChooserWidget:
 *
 * `GtkAppChooserWidget` is a widget for selecting applications.
 *
 * It is the main building block for [class@Gtk.AppChooserDialog].
 * Most applications only need to use the latter; but you can use
 * this widget as part of a larger widget if you have special needs.
 *
 * `GtkAppChooserWidget` offers detailed control over what applications
 * are shown, using the
 * [property@Gtk.AppChooserWidget:show-default],
 * [property@Gtk.AppChooserWidget:show-recommended],
 * [property@Gtk.AppChooserWidget:show-fallback],
 * [property@Gtk.AppChooserWidget:show-other] and
 * [property@Gtk.AppChooserWidget:show-all] properties. See the
 * [iface@Gtk.AppChooser] documentation for more information about these
 * groups of applications.
 *
 * To keep track of the selected application, use the
 * [signal@Gtk.AppChooserWidget::application-selected] and
 * [signal@Gtk.AppChooserWidget::application-activated] signals.
 *
 * ## CSS nodes
 *
 * `GtkAppChooserWidget` has a single CSS node with name appchooser.
 */

typedef struct _GtkAppChooserWidgetClass   GtkAppChooserWidgetClass;

struct _GtkAppChooserWidget {
  GtkWidget parent_instance;

  GtkWidget *overlay;

  char *content_type;
  char *default_text;

  guint show_default     : 1;
  guint show_recommended : 1;
  guint show_fallback    : 1;
  guint show_other       : 1;
  guint show_all         : 1;

  GListModel *program_list_model;
  GListModel *sorted_program_list_model;
  GListStore *default_app;
  GListStore *recommended_apps;
  GListStore *related_apps;
  GListStore *other_apps;
  GtkSelectionModel *selection_model;

  GtkWidget *program_list;
  GtkListItemFactory *header_factory;
  GtkWidget *no_apps_label;
  GtkWidget *no_apps;

  GString *search_string;
  guint search_timeout;
  gboolean custom_search_entry;

  GAppInfoMonitor *monitor;

  GtkWidget *popup_menu;
};

struct _GtkAppChooserWidgetClass {
  GtkWidgetClass parent_class;

  void (* application_selected)  (GtkAppChooserWidget *self,
                                  GAppInfo            *app_info);

  void (* application_activated) (GtkAppChooserWidget *self,
                                  GAppInfo            *app_info);
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
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };

static void gtk_app_chooser_widget_iface_init (GtkAppChooserIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkAppChooserWidget, gtk_app_chooser_widget, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_APP_CHOOSER,
                                                gtk_app_chooser_widget_iface_init));

static char *
get_description (GtkListItem *list_item,
                 GAppInfo    *app_info)
{
  if (!app_info)
    return NULL;

  return g_markup_printf_escaped ("%s",
                                  g_app_info_get_name (app_info) != NULL ?
                                  g_app_info_get_name (app_info) : "");
}

static char *
get_description_closure (GAppInfo *app_info)
{
  return get_description (NULL, app_info);
}

static GIcon *
get_icon (GtkListItem *list_item,
          GAppInfo    *app_info)
{
  GIcon *icon;

  if (!app_info)
    return NULL;

  icon = g_app_info_get_icon (app_info);
  if (icon == NULL)
    return g_themed_icon_new ("application-x-executable");
  else
    return g_object_ref (icon);
}

static void
selection_changed (GtkSelectionModel   *model,
                   guint                position,
                   guint                n_items,
                   GtkAppChooserWidget *self)
{
  g_signal_emit (self, signals[SIGNAL_APPLICATION_SELECTED], 0,
                 gtk_single_selection_get_selected_item (GTK_SINGLE_SELECTION (model)));
}

static void
program_list_selection_activated (GtkListView *view,
                                  guint        pos,
                                  gpointer     user_data)
{
  GtkAppChooserWidget *self = user_data;
  GAppInfo *selection;

  selection = g_list_model_get_item (G_LIST_MODEL (self->selection_model), pos);

  g_signal_emit (self, signals[SIGNAL_APPLICATION_ACTIVATED], 0, selection);

  g_object_unref (selection);
}

static int
compare_apps_func (gconstpointer a,
                   gconstpointer b)
{
  return !g_app_info_equal (G_APP_INFO (a), G_APP_INFO (b));
}

static void
gtk_app_chooser_widget_add_section (GtkAppChooserWidget *self,
                                    GListStore          *store,
                                    GList               *applications,
                                    GList               *exclude_apps)
{
  GAppInfo *app;
  GList *l;

  for (l = applications; l != NULL; l = l->next)
    {
      app = l->data;

      if (self->content_type != NULL &&
          !g_app_info_supports_uris (app) &&
          !g_app_info_supports_files (app))
        continue;

      if (g_list_find_custom (exclude_apps, app,
                              (GCompareFunc) compare_apps_func))
        continue;

      g_list_store_append (store, app);
    }
}

static void
update_no_applications_label (GtkAppChooserWidget *self)
{
  char *text = NULL, *desc = NULL;
  const char *string;

  if (self->default_text == NULL)
    {
      if (self->content_type)
	desc = g_content_type_get_description (self->content_type);

      string = text = g_strdup_printf (_("No apps found for “%s”."), desc);
      g_free (desc);
    }
  else
    {
      string = self->default_text;
    }

  gtk_label_set_text (GTK_LABEL (self->no_apps_label), string);

  g_free (text);
}

static void
gtk_app_chooser_widget_real_add_items (GtkAppChooserWidget *self)
{
  GList *all_applications = NULL;
  GList *recommended_apps = NULL;
  GList *fallback_apps = NULL;
  GList *exclude_apps = NULL;
  GAppInfo *default_app = NULL;
  gboolean apps_added;

  apps_added = FALSE;

  if (self->show_default && self->content_type)
    {
      default_app = g_app_info_get_default_for_type (self->content_type, FALSE);

      if (default_app != NULL)
        {
          g_list_store_append (self->default_app, default_app);
          exclude_apps = g_list_prepend (exclude_apps, default_app);
        }
    }

#ifndef G_OS_WIN32
  if ((self->content_type && self->show_recommended) || self->show_all)
    {
      if (self->content_type)
	recommended_apps = g_app_info_get_recommended_for_type (self->content_type);

      gtk_app_chooser_widget_add_section (self,
                                          self->recommended_apps,
                                          recommended_apps, exclude_apps);

      exclude_apps = g_list_concat (exclude_apps,
                                    g_list_copy (recommended_apps));
    }

  if ((self->content_type && self->show_fallback) || self->show_all)
    {
      if (self->content_type)
	fallback_apps = g_app_info_get_fallback_for_type (self->content_type);

      gtk_app_chooser_widget_add_section (self,
                                          self->related_apps,
                                          fallback_apps, exclude_apps);
      exclude_apps = g_list_concat (exclude_apps,
                                    g_list_copy (fallback_apps));
    }
#endif

  if (self->show_other || self->show_all)
    {
      all_applications = g_app_info_get_all ();

      gtk_app_chooser_widget_add_section (self,
                                          self->other_apps,
                                          all_applications, exclude_apps);
    }

  apps_added = g_list_model_get_n_items (G_LIST_MODEL (self->selection_model)) > 0;

  if (!apps_added)
    update_no_applications_label (self);
  else
    gtk_list_view_scroll_to (GTK_LIST_VIEW (self->program_list), 0,
                             GTK_LIST_SCROLL_SELECT | GTK_LIST_SCROLL_FOCUS,
                             NULL);

  gtk_widget_set_visible (self->no_apps, !apps_added);

  if (default_app != NULL)
    g_object_unref (default_app);

  g_list_free_full (all_applications, g_object_unref);
  g_list_free_full (recommended_apps, g_object_unref);
  g_list_free_full (fallback_apps, g_object_unref);
  g_list_free (exclude_apps);
}

static void
setup_header_factory (GtkListItemFactory  *factory,
                      GtkListHeader       *list_header,
                      GtkAppChooserWidget *self)
{
  GtkWidget *label;

  label = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_list_header_set_child (list_header, label);
}

static void
bind_header_factory (GtkListItemFactory  *factory,
                     GtkListHeader       *list_header,
                     GtkAppChooserWidget *self)
{
  GListModel *model;
  GtkWidget *label = gtk_list_header_get_child (list_header);
  guint pos = gtk_list_header_get_start (list_header);

  model = gtk_flatten_list_model_get_model_for_item (GTK_FLATTEN_LIST_MODEL (self->program_list_model),
                                                     pos);
  if (GTK_IS_SORT_LIST_MODEL (model))
    model = gtk_sort_list_model_get_model (GTK_SORT_LIST_MODEL (model));

  if (model == G_LIST_MODEL (self->default_app))
    gtk_label_set_label (GTK_LABEL (label), _("Default App"));
  else if (model == G_LIST_MODEL (self->recommended_apps))
    gtk_label_set_label (GTK_LABEL (label), _("Recommended Apps"));
  else if (model == G_LIST_MODEL (self->related_apps))
    gtk_label_set_label (GTK_LABEL (label), _("Related Apps"));
  else if (model == G_LIST_MODEL (self->other_apps))
    gtk_label_set_label (GTK_LABEL (label), _("Other Apps"));
}

static void
clear_search (gpointer user_data)
{
  GtkAppChooserWidget *self = user_data;

  g_string_erase (self->search_string, 0, -1);
  self->search_timeout = 0;
}

static gboolean
on_key_pressed (GtkEventControllerKey *controller,
                guint                  keyval,
                guint                  keycode,
                GdkModifierType        state,
                GtkAppChooserWidget   *self)
{
  guint32 character = gdk_keyval_to_unicode (keyval);
  guint i;
  gboolean match_found = FALSE;

  if (character == 0 || self->custom_search_entry)
    return FALSE;

  g_string_append_c (self->search_string, character);

  g_clear_handle_id (&self->search_timeout, g_source_remove);
  self->search_timeout = g_timeout_add_once (2000, clear_search, self);

  for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->selection_model)); i++)
    {
      GAppInfo *app_info = g_list_model_get_item (G_LIST_MODEL (self->selection_model), i);
      const char *name = g_app_info_get_name (app_info);
      const char *exec_name = g_app_info_get_executable (app_info);

      if ((name != NULL && g_str_match_string (self->search_string->str, name, TRUE)) ||
          (exec_name != NULL && g_str_match_string (self->search_string->str, exec_name, FALSE)))
        {
          match_found = TRUE;
          g_object_unref (app_info);
          break;
        }

      g_object_unref (app_info);
    }

  if (match_found)
      gtk_list_view_scroll_to (GTK_LIST_VIEW (self->program_list), i,
                               GTK_LIST_SCROLL_SELECT | GTK_LIST_SCROLL_FOCUS,
                               NULL);

  return TRUE;
}

static void
gtk_app_chooser_widget_initialize_items (GtkAppChooserWidget *self)
{
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

  switch (property_id)
    {
    case PROP_CONTENT_TYPE:
      self->content_type = g_value_dup_string (value);
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

  switch (property_id)
    {
    case PROP_CONTENT_TYPE:
      g_value_set_string (value, self->content_type);
      break;
    case PROP_SHOW_DEFAULT:
      g_value_set_boolean (value, self->show_default);
      break;
    case PROP_SHOW_RECOMMENDED:
      g_value_set_boolean (value, self->show_recommended);
      break;
    case PROP_SHOW_FALLBACK:
      g_value_set_boolean (value, self->show_fallback);
      break;
    case PROP_SHOW_OTHER:
      g_value_set_boolean (value, self->show_other);
      break;
    case PROP_SHOW_ALL:
      g_value_set_boolean (value, self->show_all);
      break;
    case PROP_DEFAULT_TEXT:
      g_value_set_string (value, self->default_text);
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

  g_free (self->content_type);
  g_free (self->default_text);
  g_signal_handlers_disconnect_by_func (self->monitor, app_info_changed, self);
  g_object_unref (self->monitor);
  g_clear_object (&self->header_factory);

  g_clear_handle_id (&self->search_timeout, g_source_remove);
  g_string_free (self->search_string, TRUE);
  self->search_string = NULL;

  g_clear_object (&self->program_list_model);
  g_clear_object (&self->sorted_program_list_model);
  g_clear_object (&self->selection_model);
  g_clear_object (&self->default_app);
  g_clear_object (&self->recommended_apps);
  g_clear_object (&self->related_apps);
  g_clear_object (&self->other_apps);

  G_OBJECT_CLASS (gtk_app_chooser_widget_parent_class)->finalize (object);
}

static void
gtk_app_chooser_widget_dispose (GObject *object)
{
  GtkAppChooserWidget *self = GTK_APP_CHOOSER_WIDGET (object);

  if (self->overlay)
    {
      gtk_widget_unparent (self->overlay);
      self->overlay = NULL;
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

  gtk_widget_measure (self->overlay, orientation, for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);
}

static void
gtk_app_chooser_widget_snapshot (GtkWidget   *widget,
                                 GtkSnapshot *snapshot)
{
  GtkAppChooserWidget *self = GTK_APP_CHOOSER_WIDGET (widget);

  gtk_widget_snapshot_child (widget, self->overlay, snapshot);
}

static void
gtk_app_chooser_widget_size_allocate (GtkWidget *widget,
                                      int        width,
                                      int        height,
                                      int        baseline)
{
  GtkAppChooserWidget *self = GTK_APP_CHOOSER_WIDGET (widget);

  GTK_WIDGET_CLASS (gtk_app_chooser_widget_parent_class)->size_allocate (widget, width, height, baseline);

  gtk_widget_size_allocate (self->overlay,
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
   * Determines whether the app chooser should show the default
   * handler for the content type in a separate section.
   *
   * If %FALSE, the default handler is listed among the recommended
   * applications.
   */
  pspec = g_param_spec_boolean ("show-default", NULL, NULL,
                                FALSE,
                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);
  g_object_class_install_property (gobject_class, PROP_SHOW_DEFAULT, pspec);

  /**
   * GtkAppChooserWidget:show-recommended:
   *
   * Determines whether the app chooser should show a section
   * for recommended applications.
   *
   * If %FALSE, the recommended applications are listed
   * among the other applications.
   */
  pspec = g_param_spec_boolean ("show-recommended", NULL, NULL,
                                TRUE,
                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);
  g_object_class_install_property (gobject_class, PROP_SHOW_RECOMMENDED, pspec);

  /**
   * GtkAppChooserWidget:show-fallback:
   *
   * Determines whether the app chooser should show a section
   * for fallback applications.
   *
   * If %FALSE, the fallback applications are listed among the
   * other applications.
   */
  pspec = g_param_spec_boolean ("show-fallback", NULL, NULL,
                                FALSE,
                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);
  g_object_class_install_property (gobject_class, PROP_SHOW_FALLBACK, pspec);

  /**
   * GtkAppChooserWidget:show-other:
   *
   * Determines whether the app chooser should show a section
   * for other applications.
   */
  pspec = g_param_spec_boolean ("show-other", NULL, NULL,
                                FALSE,
                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);
  g_object_class_install_property (gobject_class, PROP_SHOW_OTHER, pspec);

  /**
   * GtkAppChooserWidget:show-all:
   *
   * If %TRUE, the app chooser presents all applications
   * in a single list, without subsections for default,
   * recommended or related applications.
   */
  pspec = g_param_spec_boolean ("show-all", NULL, NULL,
                                FALSE,
                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);
  g_object_class_install_property (gobject_class, PROP_SHOW_ALL, pspec);

  /**
   * GtkAppChooserWidget:default-text:
   *
   * The text that appears in the widget when there are no applications
   * for the given content type.
   */
  pspec = g_param_spec_string ("default-text", NULL, NULL,
                               NULL,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);
  g_object_class_install_property (gobject_class, PROP_DEFAULT_TEXT, pspec);

  /**
   * GtkAppChooserWidget::application-selected:
   * @self: the object which received the signal
   * @application: the selected `GAppInfo`
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
   * @application: the activated `GAppInfo`
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

  /* Bind class to template
   */
  gtk_widget_class_set_template_from_resource (widget_class,
					       "/org/gtk/libgtk/ui/gtkappchooserwidget.ui");
  gtk_widget_class_bind_template_child (widget_class, GtkAppChooserWidget, program_list);
  gtk_widget_class_bind_template_child (widget_class, GtkAppChooserWidget, no_apps_label);
  gtk_widget_class_bind_template_child (widget_class, GtkAppChooserWidget, no_apps);
  gtk_widget_class_bind_template_child (widget_class, GtkAppChooserWidget, overlay);
  gtk_widget_class_bind_template_callback (widget_class, get_description);
  gtk_widget_class_bind_template_callback (widget_class, get_icon);
  gtk_widget_class_bind_template_callback (widget_class, program_list_selection_activated);

  gtk_widget_class_set_css_name (widget_class, I_("appchooser"));
}

static void
gtk_app_chooser_widget_init (GtkAppChooserWidget *self)
{
  GListStore *store = g_list_store_new (G_TYPE_LIST_MODEL);
  GtkExpression *sorter_expression;
  GtkSortListModel *other_apps_sorted;
  GtkSorter *sorter;
  GtkEventController *controller;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->default_app = g_list_store_new (G_TYPE_APP_INFO);
  self->recommended_apps = g_list_store_new (G_TYPE_APP_INFO);
  self->related_apps = g_list_store_new (G_TYPE_APP_INFO);
  self->other_apps = g_list_store_new (G_TYPE_APP_INFO);
  g_list_store_append (store, self->default_app);
  g_list_store_append (store, self->recommended_apps);
  g_list_store_append (store, self->related_apps);

  sorter_expression = gtk_cclosure_expression_new (G_TYPE_STRING,
                                                   NULL, 0, NULL,
                                                   G_CALLBACK (get_description_closure),
                                                   NULL, NULL);
  sorter = GTK_SORTER (gtk_string_sorter_new (sorter_expression));
  other_apps_sorted = gtk_sort_list_model_new (G_LIST_MODEL (g_object_ref (self->other_apps)),
                                               g_object_ref (sorter));
  g_list_store_append (store, other_apps_sorted);
  g_object_unref (other_apps_sorted);

  self->program_list_model = G_LIST_MODEL (gtk_flatten_list_model_new (G_LIST_MODEL (store)));
  self->sorted_program_list_model = G_LIST_MODEL (gtk_sort_list_model_new (g_object_ref (self->program_list_model), sorter));
  self->selection_model = GTK_SELECTION_MODEL (gtk_single_selection_new (g_object_ref (self->program_list_model)));
  gtk_list_view_set_model (GTK_LIST_VIEW (self->program_list), self->selection_model);
  g_signal_connect (self->selection_model, "selection-changed", G_CALLBACK (selection_changed), self);

  self->header_factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (self->header_factory, "setup", G_CALLBACK (setup_header_factory), self);
  g_signal_connect (self->header_factory, "bind", G_CALLBACK (bind_header_factory), self);
  gtk_list_view_set_header_factory (GTK_LIST_VIEW (self->program_list), self->header_factory);

  self->search_string = g_string_new ("");
  controller = gtk_event_controller_key_new ();
  g_signal_connect (controller, "key-pressed", G_CALLBACK (on_key_pressed), self);
  gtk_widget_add_controller (self->program_list, controller);

  self->monitor = g_app_info_monitor_get ();
  g_signal_connect (self->monitor, "changed",
		    G_CALLBACK (app_info_changed), self);
}

static GAppInfo *
gtk_app_chooser_widget_get_app_info (GtkAppChooser *object)
{
  GtkAppChooserWidget *self = GTK_APP_CHOOSER_WIDGET (object);
  GAppInfo *app_info;

  app_info = gtk_single_selection_get_selected_item (GTK_SINGLE_SELECTION (self->selection_model));
  if (!app_info)
    return NULL;

  return g_object_ref (app_info);
}

static void
gtk_app_chooser_widget_refresh (GtkAppChooser *object)
{
  GtkAppChooserWidget *self = GTK_APP_CHOOSER_WIDGET (object);

  g_list_store_remove_all (self->default_app);
  g_list_store_remove_all (self->recommended_apps);
  g_list_store_remove_all (self->related_apps);
  g_list_store_remove_all (self->other_apps);

  gtk_app_chooser_widget_real_add_items (self);
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
 * Creates a new `GtkAppChooserWidget` for applications
 * that can handle content of the given type.
 *
 * Returns: a newly created `GtkAppChooserWidget`
 */
GtkWidget *
gtk_app_chooser_widget_new (const char *content_type)
{
  return g_object_new (GTK_TYPE_APP_CHOOSER_WIDGET,
                       "content-type", content_type,
                       NULL);
}

/**
 * gtk_app_chooser_widget_set_show_default:
 * @self: a `GtkAppChooserWidget`
 * @setting: the new value for [property@Gtk.AppChooserWidget:show-default]
 *
 * Sets whether the app chooser should show the default handler
 * for the content type in a separate section.
 */
void
gtk_app_chooser_widget_set_show_default (GtkAppChooserWidget *self,
                                         gboolean             setting)
{
  g_return_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self));

  if (self->show_default != setting)
    {
      self->show_default = setting;

      g_object_notify (G_OBJECT (self), "show-default");

      gtk_app_chooser_refresh (GTK_APP_CHOOSER (self));
    }
}

/**
 * gtk_app_chooser_widget_get_show_default:
 * @self: a `GtkAppChooserWidget`
 *
 * Gets whether the app chooser should show the default handler
 * for the content type in a separate section.
 *
 * Returns: the value of [property@Gtk.AppChooserWidget:show-default]
 */
gboolean
gtk_app_chooser_widget_get_show_default (GtkAppChooserWidget *self)
{
  g_return_val_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self), FALSE);

  return self->show_default;
}

/**
 * gtk_app_chooser_widget_set_show_recommended:
 * @self: a `GtkAppChooserWidget`
 * @setting: the new value for [property@Gtk.AppChooserWidget:show-recommended]
 *
 * Sets whether the app chooser should show recommended applications
 * for the content type in a separate section.
 */
void
gtk_app_chooser_widget_set_show_recommended (GtkAppChooserWidget *self,
                                             gboolean             setting)
{
  g_return_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self));

  if (self->show_recommended != setting)
    {
      self->show_recommended = setting;

      g_object_notify (G_OBJECT (self), "show-recommended");

      gtk_app_chooser_refresh (GTK_APP_CHOOSER (self));
    }
}

/**
 * gtk_app_chooser_widget_get_show_recommended:
 * @self: a `GtkAppChooserWidget`
 *
 * Gets whether the app chooser should show recommended applications
 * for the content type in a separate section.
 *
 * Returns: the value of [property@Gtk.AppChooserWidget:show-recommended]
 */
gboolean
gtk_app_chooser_widget_get_show_recommended (GtkAppChooserWidget *self)
{
  g_return_val_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self), FALSE);

  return self->show_recommended;
}

/**
 * gtk_app_chooser_widget_set_show_fallback:
 * @self: a `GtkAppChooserWidget`
 * @setting: the new value for [property@Gtk.AppChooserWidget:show-fallback]
 *
 * Sets whether the app chooser should show related applications
 * for the content type in a separate section.
 */
void
gtk_app_chooser_widget_set_show_fallback (GtkAppChooserWidget *self,
                                          gboolean             setting)
{
  g_return_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self));

  if (self->show_fallback != setting)
    {
      self->show_fallback = setting;

      g_object_notify (G_OBJECT (self), "show-fallback");

      gtk_app_chooser_refresh (GTK_APP_CHOOSER (self));
    }
}

/**
 * gtk_app_chooser_widget_get_show_fallback:
 * @self: a `GtkAppChooserWidget`
 *
 * Gets whether the app chooser should show related applications
 * for the content type in a separate section.
 *
 * Returns: the value of [property@Gtk.AppChooserWidget:show-fallback]
 */
gboolean
gtk_app_chooser_widget_get_show_fallback (GtkAppChooserWidget *self)
{
  g_return_val_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self), FALSE);

  return self->show_fallback;
}

/**
 * gtk_app_chooser_widget_set_show_other:
 * @self: a `GtkAppChooserWidget`
 * @setting: the new value for [property@Gtk.AppChooserWidget:show-other]
 *
 * Sets whether the app chooser should show applications
 * which are unrelated to the content type.
 */
void
gtk_app_chooser_widget_set_show_other (GtkAppChooserWidget *self,
                                       gboolean             setting)
{
  g_return_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self));

  if (self->show_other != setting)
    {
      self->show_other = setting;

      g_object_notify (G_OBJECT (self), "show-other");

      gtk_app_chooser_refresh (GTK_APP_CHOOSER (self));
    }
}

/**
 * gtk_app_chooser_widget_get_show_other:
 * @self: a `GtkAppChooserWidget`
 *
 * Gets whether the app chooser should show applications
 * which are unrelated to the content type.
 *
 * Returns: the value of [property@Gtk.AppChooserWidget:show-other]
 */
gboolean
gtk_app_chooser_widget_get_show_other (GtkAppChooserWidget *self)
{
  g_return_val_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self), FALSE);

  return self->show_other;
}

/**
 * gtk_app_chooser_widget_set_show_all:
 * @self: a `GtkAppChooserWidget`
 * @setting: the new value for [property@Gtk.AppChooserWidget:show-all]
 *
 * Sets whether the app chooser should show all applications
 * in a flat list.
 */
void
gtk_app_chooser_widget_set_show_all (GtkAppChooserWidget *self,
                                     gboolean             setting)
{
  g_return_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self));

  if (self->show_all != setting)
    {
      GtkListItemFactory *header_factory = setting ? NULL : self->header_factory;
      GListModel *model = setting ? self->sorted_program_list_model : self->program_list_model;

      self->show_all = setting;

      g_object_notify (G_OBJECT (self), "show-all");

      gtk_app_chooser_refresh (GTK_APP_CHOOSER (self));

      /* Don't show sections when show_all==TRUE. In which case all items should be sorted. */
      gtk_list_view_set_header_factory (GTK_LIST_VIEW (self->program_list), header_factory);
      gtk_single_selection_set_model (GTK_SINGLE_SELECTION (self->selection_model), model);
    }
}

/**
 * gtk_app_chooser_widget_get_show_all:
 * @self: a `GtkAppChooserWidget`
 *
 * Gets whether the app chooser should show all applications
 * in a flat list.
 *
 * Returns: the value of [property@Gtk.AppChooserWidget:show-all]
 */
gboolean
gtk_app_chooser_widget_get_show_all (GtkAppChooserWidget *self)
{
  g_return_val_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self), FALSE);

  return self->show_all;
}

/**
 * gtk_app_chooser_widget_set_default_text:
 * @self: a `GtkAppChooserWidget`
 * @text: the new value for [property@Gtk.AppChooserWidget:default-text]
 *
 * Sets the text that is shown if there are not applications
 * that can handle the content type.
 */
void
gtk_app_chooser_widget_set_default_text (GtkAppChooserWidget *self,
                                         const char          *text)
{
  g_return_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self));

  if (g_strcmp0 (text, self->default_text) != 0)
    {
      g_free (self->default_text);
      self->default_text = g_strdup (text);

      g_object_notify (G_OBJECT (self), "default-text");

      gtk_app_chooser_refresh (GTK_APP_CHOOSER (self));
    }
}

/**
 * gtk_app_chooser_widget_get_default_text:
 * @self: a `GtkAppChooserWidget`
 *
 * Returns the text that is shown if there are not applications
 * that can handle the content type.
 *
 * Returns: (nullable): the value of [property@Gtk.AppChooserWidget:default-text]
 */
const char *
gtk_app_chooser_widget_get_default_text (GtkAppChooserWidget *self)
{
  g_return_val_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self), NULL);

  return self->default_text;
}

void
_gtk_app_chooser_widget_set_search_entry (GtkAppChooserWidget *self,
                                          GtkEditable         *entry)
{
  self->custom_search_entry = TRUE;

  g_object_bind_property (self->no_apps, "visible",
                          entry, "sensitive",
                          G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
}
