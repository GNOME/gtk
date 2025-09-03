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

#include "gtkmarshalers.h"
#include "gtkappchooserwidget.h"
#include "gtkappchooserprivate.h"
#include "gtkliststore.h"
#include "gtkorientable.h"
#include "gtkscrolledwindow.h"
#include "gtklabel.h"
#include "gtkgestureclick.h"
#include "gtkwidgetprivate.h"
#include "gtkprivate.h"
#include "gtkbox.h"
#include "gtklistview.h"
#include "gtksignallistitemfactory.h"
#include "gtklistitem.h"
#include "gtksingleselection.h"
#include "gtkfilterlistmodel.h"
#include "gtkstringfilter.h"
#include "gtksortlistmodel.h"
#include "gtkstringsorter.h"
#include "gtkcustomsorter.h"
#include "gtklistheader.h"
#include "gtksearchentry.h"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <gio/gio.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

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
 *
 * Deprecated: 4.10: The application selection widgets should be
 *   implemented according to the design of each platform and/or
 *   application requiring them.
 */

G_DECLARE_FINAL_TYPE (GtkAppItem, gtk_app_item, GTK, APP_ITEM, GObject)

struct _GtkAppItem
{
  GObject parent_instance;
  GAppInfo *app_info;
  gboolean is_default;
  gboolean is_recommended;
  gboolean is_fallback;
};

enum {
  ITEM_PROP_NAME = 1,
  NUM_ITEM_PROPS,
};

static GParamSpec *item_properties[NUM_ITEM_PROPS];

G_DEFINE_TYPE (GtkAppItem, gtk_app_item, G_TYPE_OBJECT)

static void
gtk_app_item_init (GtkAppItem *item)
{
}

static void
gtk_app_item_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GtkAppItem *item = GTK_APP_ITEM (object);

  switch (prop_id)
    {
    case ITEM_PROP_NAME:
      g_value_set_string (value, g_app_info_get_display_name (item->app_info));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_app_item_finalize (GObject *object)
{
  GtkAppItem *item = GTK_APP_ITEM (object);

  g_object_unref (item->app_info);

  G_OBJECT_CLASS (gtk_app_item_parent_class)->finalize (object);
}

static void
gtk_app_item_class_init (GtkAppItemClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->get_property = gtk_app_item_get_property;
  object_class->finalize = gtk_app_item_finalize;

  item_properties[ITEM_PROP_NAME] = g_param_spec_string ("name", NULL, NULL,
                                                         NULL,
                                                         G_PARAM_READABLE);

  g_object_class_install_properties (object_class, NUM_ITEM_PROPS, item_properties);
}

static GtkAppItem *
gtk_app_item_new (GAppInfo *app_info,
                  gboolean is_default,
                  gboolean is_recommended,
                  gboolean is_fallback)
{
  GtkAppItem *item;

  item = g_object_new (gtk_app_item_get_type (), NULL);

  item->app_info = g_object_ref (app_info);
  item->is_default = is_default;
  item->is_recommended = is_recommended;
  item->is_fallback = is_fallback;

  return item;
}

typedef struct _GtkAppChooserWidgetClass   GtkAppChooserWidgetClass;

struct _GtkAppChooserWidget {
  GtkWidget parent_instance;

  GAppInfo *selected_app_info;

  GtkWidget *overlay;

  char *content_type;
  char *default_text;

  guint show_default     : 1;
  guint show_recommended : 1;
  guint show_fallback    : 1;
  guint show_other       : 1;
  guint show_all         : 1;

  GListStore *app_info_store;
  GtkListItemFactory *header_factory;
  GtkStringFilter *filter;
  GtkStringSorter *sorter;
  GtkWidget *program_list;
  GtkWidget *no_apps_label;
  GtkWidget *no_apps;

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

static gboolean
gtk_app_chooser_widget_add_section (GtkAppChooserWidget *self,
                                    gboolean             recommended,
                                    gboolean             fallback,
                                    GList               *applications,
                                    GList               *exclude_apps)
{
  gboolean retval;

  retval = FALSE;

  for (GList *l = applications; l != NULL; l = l->next)
    {
      GAppInfo *app = l->data;
      GtkAppItem *item;

      if (self->content_type != NULL &&
          !g_app_info_supports_uris (app) &&
          !g_app_info_supports_files (app))
        continue;

      if (g_list_find (exclude_apps, app))
        continue;

      item = gtk_app_item_new (app, FALSE, recommended, fallback);
      g_list_store_append (self->app_info_store, item);
      g_object_unref (item);
      retval = TRUE;
    }

  return retval;
}

static void
gtk_app_chooser_add_default (GtkAppChooserWidget *self,
                             GAppInfo            *app)
{
  GtkAppItem *item;

  item = gtk_app_item_new (app, TRUE, FALSE, FALSE);
  g_list_store_append (self->app_info_store, item);
  g_object_unref (item);
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
gtk_app_chooser_widget_select_first (GtkAppChooserWidget *self)
{
  gtk_single_selection_set_selected (GTK_SINGLE_SELECTION (gtk_list_view_get_model (GTK_LIST_VIEW (self->program_list))), 0);
}

static void
gtk_app_chooser_widget_real_add_items (GtkAppChooserWidget *self)
{
  GList *all_applications = NULL;
  GList *recommended_apps = NULL;
  GList *fallback_apps = NULL;
  GList *exclude_apps = NULL;
  GAppInfo *default_app = NULL;
  gboolean show_headings;
  gboolean apps_added;

  show_headings = TRUE;
  apps_added = FALSE;

  if (self->show_all)
    show_headings = FALSE;

  gtk_list_view_set_header_factory (GTK_LIST_VIEW (self->program_list),
                                    show_headings ? self->header_factory : NULL);

  if (self->show_default && self->content_type)
    {
      default_app = g_app_info_get_default_for_type (self->content_type, FALSE);

      if (default_app != NULL)
        {
          gtk_app_chooser_add_default (self, default_app);
          apps_added = TRUE;
          exclude_apps = g_list_prepend (exclude_apps, default_app);
        }
    }

#ifndef G_OS_WIN32
  if ((self->content_type && self->show_recommended) || self->show_all)
    {
      if (self->content_type)
	recommended_apps = g_app_info_get_recommended_for_type (self->content_type);

      apps_added |= gtk_app_chooser_widget_add_section (self,
                                                        !self->show_all, /* mark as recommended */
                                                        FALSE, /* mark as fallback */
                                                        recommended_apps, exclude_apps);

      exclude_apps = g_list_concat (exclude_apps,
                                    g_list_copy (recommended_apps));
    }

  if ((self->content_type && self->show_fallback) || self->show_all)
    {
      if (self->content_type)
	fallback_apps = g_app_info_get_fallback_for_type (self->content_type);

      apps_added |= gtk_app_chooser_widget_add_section (self,
                                                        FALSE, /* mark as recommended */
                                                        !self->show_all, /* mark as fallback */
                                                        fallback_apps, exclude_apps);
      exclude_apps = g_list_concat (exclude_apps,
                                    g_list_copy (fallback_apps));
    }
#endif

  if (self->show_other || self->show_all)
    {
      all_applications = g_app_info_get_all ();

      apps_added |= gtk_app_chooser_widget_add_section (self,
                                                        FALSE,
                                                        FALSE,
                                                        all_applications, exclude_apps);
    }

  if (!apps_added)
    update_no_applications_label (self);

  gtk_widget_set_visible (self->no_apps, !apps_added);

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
  g_object_unref (self->app_info_store);
  g_object_unref (self->header_factory);
  g_object_unref (self->filter);
  g_object_unref (self->sorter);

  G_OBJECT_CLASS (gtk_app_chooser_widget_parent_class)->finalize (object);
}

static void
gtk_app_chooser_widget_dispose (GObject *object)
{
  GtkAppChooserWidget *self = GTK_APP_CHOOSER_WIDGET (object);

  g_clear_object (&self->selected_app_info);

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

  gtk_widget_class_set_css_name (widget_class, I_("appchooser"));
}

static void
setup_listitem_cb (GtkListItemFactory *factory,
                   GtkListItem        *list_item)
{
  GtkWidget *box;
  GtkWidget *image;
  GtkWidget *label;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  image = gtk_image_new ();
  gtk_accessible_update_property (GTK_ACCESSIBLE (image),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL,
                                  "App icon",
                                  -1);
  gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_LARGE);
  gtk_box_append (GTK_BOX (box), image);
  label = gtk_label_new ("");
  gtk_box_append (GTK_BOX (box), label);
  gtk_list_item_set_child (list_item, box);
}

static void
bind_listitem_cb (GtkListItemFactory *factory,
                  GtkListItem        *list_item)
{
  GtkWidget *image;
  GtkWidget *label;
  GtkAppItem *app_item;

  image = gtk_widget_get_first_child (gtk_list_item_get_child (list_item));
  label = gtk_widget_get_next_sibling (image);
  app_item = gtk_list_item_get_item (list_item);

  gtk_image_set_from_gicon (GTK_IMAGE (image), g_app_info_get_icon (app_item->app_info));
  gtk_label_set_label (GTK_LABEL (label), g_app_info_get_display_name (app_item->app_info));
  gtk_list_item_set_accessible_label (list_item, g_app_info_get_display_name (app_item->app_info));
}

static void
setup_header_cb (GtkListItemFactory *factory,
                 GtkListItem        *list_item)
{
  GtkListHeader *header = GTK_LIST_HEADER (list_item);
  GtkWidget *label;

  label = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_widget_add_css_class (label, "heading");
  gtk_widget_set_margin_start (label, 20);
  gtk_widget_set_margin_end (label, 20);
  gtk_widget_set_margin_top (label, 10);
  gtk_widget_set_margin_bottom (label, 10);

  gtk_list_header_set_child (header, label);
}

static void
bind_header_cb (GtkListItemFactory *factory,
                GtkListItem        *list_item)
{
  GtkListHeader *header = GTK_LIST_HEADER (list_item);
  GtkWidget *label;
  GtkAppItem *app_item;

  label = gtk_list_header_get_child (header);
  app_item = gtk_list_header_get_item (header);

  if (app_item->is_default)
    gtk_label_set_label (GTK_LABEL (label), _("Default App"));
  else if (app_item->is_recommended)
    gtk_label_set_label (GTK_LABEL (label), _("Recommended Apps"));
  else if (app_item->is_fallback)
    gtk_label_set_label (GTK_LABEL (label), _("Related Apps"));
  else
    gtk_label_set_label (GTK_LABEL (label), _("Other Apps"));
}

static void
activate_cb (GtkListView         *list,
             guint                position,
             GtkAppChooserWidget *self)
{
  GtkAppItem *app_item;

  app_item = g_list_model_get_item (G_LIST_MODEL (gtk_list_view_get_model (list)), position);

  g_set_object (&self->selected_app_info, app_item->app_info);

  g_signal_emit (self, signals[SIGNAL_APPLICATION_ACTIVATED], 0, self->selected_app_info);

  g_object_unref (app_item);
}

static void
selection_changed_cb (GListModel          *model,
                      GParamSpec          *pspec,
                      GtkAppChooserWidget *self)
{
  guint position;
  GtkAppItem *app_item;

  position = gtk_single_selection_get_selected (GTK_SINGLE_SELECTION (model));
  if (position == GTK_INVALID_LIST_POSITION)
    return;

  app_item = g_list_model_get_item (model, position);

  g_set_object (&self->selected_app_info, app_item->app_info);

  g_signal_emit (self, signals[SIGNAL_APPLICATION_SELECTED], 0, self->selected_app_info);

  g_object_unref (app_item);
}

static int
compare_section (gconstpointer a,
                 gconstpointer b,
                 gpointer      data)
{
  const GtkAppItem *item1 = a;
  const GtkAppItem *item2 = b;

  if (item1->is_default && !item2->is_default)
    return -1;
  else if (!item1->is_default && item2->is_default)
    return 1;

  if (item1->is_recommended && !item2->is_recommended)
    return -1;
  else if (!item1->is_recommended && item2->is_recommended)
    return 1;

  if (item1->is_fallback && !item2->is_fallback)
    return 1;
  else if (!item1->is_fallback && item2->is_fallback)
    return -1;

  return 0;
}

static void
gtk_app_chooser_widget_init (GtkAppChooserWidget *self)
{
  GtkListItemFactory *factory;
  GtkSingleSelection *selection;
  GtkExpression *expression;
  GtkFilterListModel *filter;
  GtkSortListModel *sort;
  GtkCustomSorter *section_sorter;

  gtk_widget_init_template (GTK_WIDGET (self));

  expression = gtk_property_expression_new (gtk_app_item_get_type (), NULL, "name");
  self->filter = gtk_string_filter_new (gtk_expression_ref (expression));
  self->sorter = gtk_string_sorter_new (expression);

  self->app_info_store = g_list_store_new (gtk_app_item_get_type ());
  filter = gtk_filter_list_model_new (G_LIST_MODEL (g_object_ref (self->app_info_store)), GTK_FILTER (g_object_ref (self->filter)));
  sort = gtk_sort_list_model_new (G_LIST_MODEL (filter), GTK_SORTER (g_object_ref (self->sorter)));

  section_sorter = gtk_custom_sorter_new (compare_section, NULL, NULL);
  gtk_sort_list_model_set_section_sorter (sort, GTK_SORTER (section_sorter));
  g_object_unref (section_sorter);

  selection = gtk_single_selection_new (G_LIST_MODEL (sort));

  g_signal_connect (selection, "notify::selected",
                    G_CALLBACK (selection_changed_cb), self);

  gtk_list_view_set_model (GTK_LIST_VIEW (self->program_list), GTK_SELECTION_MODEL (selection));
  g_object_unref (selection);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_listitem_cb), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_listitem_cb), NULL);

  gtk_list_view_set_factory (GTK_LIST_VIEW (self->program_list), factory);
  g_object_unref (factory);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_header_cb), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_header_cb), NULL);

  gtk_list_view_set_header_factory (GTK_LIST_VIEW (self->program_list), factory);
  self->header_factory = factory;

  g_signal_connect (self->program_list, "activate",
                    G_CALLBACK (activate_cb), self);

  self->monitor = g_app_info_monitor_get ();
  g_signal_connect (self->monitor, "changed",
                    G_CALLBACK (app_info_changed), self);
}

static GAppInfo *
gtk_app_chooser_widget_get_app_info (GtkAppChooser *object)
{
  GtkAppChooserWidget *self = GTK_APP_CHOOSER_WIDGET (object);

  if (self->selected_app_info == NULL)
    return NULL;

  return g_object_ref (self->selected_app_info);
}

static void
gtk_app_chooser_widget_refresh (GtkAppChooser *object)
{
  GtkAppChooserWidget *self = GTK_APP_CHOOSER_WIDGET (object);

  g_list_store_remove_all (self->app_info_store);
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
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
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
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
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
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
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
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
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
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
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
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
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
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
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
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
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
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
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
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
void
gtk_app_chooser_widget_set_show_all (GtkAppChooserWidget *self,
                                     gboolean             setting)
{
  g_return_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self));

  if (self->show_all != setting)
    {
      self->show_all = setting;

      g_object_notify (G_OBJECT (self), "show-all");

      gtk_app_chooser_refresh (GTK_APP_CHOOSER (self));
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
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
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
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
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
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
const char *
gtk_app_chooser_widget_get_default_text (GtkAppChooserWidget *self)
{
  g_return_val_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self), NULL);

  return self->default_text;
}

static void
changed_cb (GtkEditable         *editable,
            GtkAppChooserWidget *self)
{
  gtk_string_filter_set_search (self->filter, gtk_editable_get_text (editable));
}

void
_gtk_app_chooser_widget_set_search_entry (GtkAppChooserWidget *self,
                                          GtkEditable         *entry)
{
  g_object_bind_property (self->no_apps, "visible",
                          entry, "sensitive",
                          G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

  g_signal_connect (entry, "changed", G_CALLBACK (changed_cb), self);
}
