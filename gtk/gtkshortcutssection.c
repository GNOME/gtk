/* gtkshortcutssection.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkshortcutssection.h"

#include "gtkshortcutsgroup.h"
#include "gtkbutton.h"
#include "gtklabel.h"
#include "gtkstack.h"
#include "gtkstackswitcher.h"
#include "gtkstylecontext.h"
#include "gtkorientable.h"
#include "gtksizegroup.h"
#include "gtkwidget.h"
#include "gtkbindings.h"
#include "gtkprivate.h"
#include "gtkmarshalers.h"
#include "gtkgesturepan.h"
#include "gtkwidgetprivate.h"
#include "gtkintl.h"

/**
 * SECTION:gtkshortcutssection
 * @Title: GtkShortcutsSection
 * @Short_description: Represents an application mode in a GtkShortcutsWindow
 *
 * A GtkShortcutsSection collects all the keyboard shortcuts and gestures
 * for a major application mode. If your application needs multiple sections,
 * you should give each section a unique #GtkShortcutsSection:section-name and
 * a #GtkShortcutsSection:title that can be shown in the section selector of
 * the GtkShortcutsWindow.
 *
 * The #GtkShortcutsSection:max-height property can be used to influence how
 * the groups in the section are distributed over pages and columns.
 *
 * This widget is only meant to be used with #GtkShortcutsWindow.
 */

struct _GtkShortcutsSection
{
  GtkBox            parent_instance;

  gchar            *name;
  gchar            *title;
  gchar            *view_name;
  guint             max_height;

  GtkStack         *stack;
  GtkStackSwitcher *switcher;
  GtkWidget        *show_all;
  GtkWidget        *footer;
  GList            *groups;

  gboolean          has_filtered_group;
  gboolean          need_reflow;

  GtkGesture       *pan_gesture;
};

struct _GtkShortcutsSectionClass
{
  GtkBoxClass parent_class;

  gboolean (* change_current_page) (GtkShortcutsSection *self,
                                    gint                 offset);

};

G_DEFINE_TYPE (GtkShortcutsSection, gtk_shortcuts_section, GTK_TYPE_BOX)

enum {
  PROP_0,
  PROP_TITLE,
  PROP_SECTION_NAME,
  PROP_VIEW_NAME,
  PROP_MAX_HEIGHT,
  LAST_PROP
};

enum {
  CHANGE_CURRENT_PAGE,
  LAST_SIGNAL
};

static GParamSpec *properties[LAST_PROP];
static guint signals[LAST_SIGNAL];

static void gtk_shortcuts_section_set_view_name    (GtkShortcutsSection *self,
                                                    const gchar         *view_name);
static void gtk_shortcuts_section_set_max_height   (GtkShortcutsSection *self,
                                                    guint                max_height);
static void gtk_shortcuts_section_add_group        (GtkShortcutsSection *self,
                                                    GtkShortcutsGroup   *group);

static void gtk_shortcuts_section_show_all         (GtkShortcutsSection *self);
static void gtk_shortcuts_section_filter_groups    (GtkShortcutsSection *self);
static void gtk_shortcuts_section_reflow_groups    (GtkShortcutsSection *self);
static void gtk_shortcuts_section_maybe_reflow     (GtkShortcutsSection *self);

static gboolean gtk_shortcuts_section_change_current_page (GtkShortcutsSection *self,
                                                           gint                 offset);

static void gtk_shortcuts_section_pan_gesture_pan (GtkGesturePan       *gesture,
                                                   GtkPanDirection      direction,
                                                   gdouble              offset,
                                                   GtkShortcutsSection *self);

static void
gtk_shortcuts_section_add (GtkContainer *container,
                           GtkWidget    *child)
{
  GtkShortcutsSection *self = GTK_SHORTCUTS_SECTION (container);

  if (GTK_IS_SHORTCUTS_GROUP (child))
    gtk_shortcuts_section_add_group (self, GTK_SHORTCUTS_GROUP (child));
  else
    g_warning ("Can't add children of type %s to %s",
               G_OBJECT_TYPE_NAME (child),
               G_OBJECT_TYPE_NAME (container));
}

static void
gtk_shortcuts_section_remove (GtkContainer *container,
                              GtkWidget    *child)
{
  GtkShortcutsSection *self = (GtkShortcutsSection *)container;

  if (GTK_IS_SHORTCUTS_GROUP (child) &&
      gtk_widget_is_ancestor (child, GTK_WIDGET (container)))
    {
      self->groups = g_list_remove (self->groups, child);
      gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (child)), child);
    }
  else
    GTK_CONTAINER_CLASS (gtk_shortcuts_section_parent_class)->remove (container, child);
}

static void
gtk_shortcuts_section_forall (GtkContainer *container,
                              gboolean      include_internal,
                              GtkCallback   callback,
                              gpointer      callback_data)
{
  GtkShortcutsSection *self = (GtkShortcutsSection *)container;
  GList *l;

  if (include_internal)
    {
      GTK_CONTAINER_CLASS (gtk_shortcuts_section_parent_class)->forall (container, include_internal, callback, callback_data);
    }
  else
    {
      for (l = self->groups; l; l = l->next)
        {
          GtkWidget *group = l->data;
          callback (group, callback_data);
        }
    }
}

static void
map_child (GtkWidget *child)
{
  if (_gtk_widget_get_visible (child) &&
      _gtk_widget_get_child_visible (child) &&
      !_gtk_widget_get_mapped (child))
    gtk_widget_map (child);
}

static void
gtk_shortcuts_section_map (GtkWidget *widget)
{
  GtkShortcutsSection *self = GTK_SHORTCUTS_SECTION (widget);

  if (self->need_reflow)
    gtk_shortcuts_section_reflow_groups (self);

  gtk_widget_set_mapped (widget, TRUE);

  map_child (GTK_WIDGET (self->stack));
  map_child (GTK_WIDGET (self->footer));
}

static void
gtk_shortcuts_section_unmap (GtkWidget *widget)
{
  GtkShortcutsSection *self = GTK_SHORTCUTS_SECTION (widget);

  gtk_widget_set_mapped (widget, FALSE);

  gtk_widget_unmap (GTK_WIDGET (self->footer));
  gtk_widget_unmap (GTK_WIDGET (self->stack));
}

static void
gtk_shortcuts_section_destroy (GtkWidget *widget)
{
  GtkShortcutsSection *self = GTK_SHORTCUTS_SECTION (widget);

  if (self->stack)
    {
      gtk_widget_destroy (GTK_WIDGET (self->stack));
      self->stack = NULL;
    }

  if (self->footer)
    {
      gtk_widget_destroy (GTK_WIDGET (self->footer));
      self->footer = NULL;
    }

  g_list_free (self->groups);
  self->groups = NULL;

  GTK_WIDGET_CLASS (gtk_shortcuts_section_parent_class)->destroy (widget);
}

static void
gtk_shortcuts_section_finalize (GObject *object)
{
  GtkShortcutsSection *self = (GtkShortcutsSection *)object;

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->view_name, g_free);
  g_clear_object (&self->pan_gesture);

  G_OBJECT_CLASS (gtk_shortcuts_section_parent_class)->finalize (object);
}

static void
gtk_shortcuts_section_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GtkShortcutsSection *self = (GtkShortcutsSection *)object;

  switch (prop_id)
    {
    case PROP_SECTION_NAME:
      g_value_set_string (value, self->name);
      break;

    case PROP_VIEW_NAME:
      g_value_set_string (value, self->view_name);
      break;

    case PROP_TITLE:
      g_value_set_string (value, self->title);
      break;

    case PROP_MAX_HEIGHT:
      g_value_set_uint (value, self->max_height);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_shortcuts_section_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GtkShortcutsSection *self = (GtkShortcutsSection *)object;

  switch (prop_id)
    {
    case PROP_SECTION_NAME:
      g_free (self->name);
      self->name = g_value_dup_string (value);
      break;

    case PROP_VIEW_NAME:
      gtk_shortcuts_section_set_view_name (self, g_value_get_string (value));
      break;

    case PROP_TITLE:
      g_free (self->title);
      self->title = g_value_dup_string (value);
      break;

    case PROP_MAX_HEIGHT:
      gtk_shortcuts_section_set_max_height (self, g_value_get_uint (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static GType
gtk_shortcuts_section_child_type (GtkContainer *container)
{
  return GTK_TYPE_SHORTCUTS_GROUP;
}

static void
gtk_shortcuts_section_class_init (GtkShortcutsSectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
  GtkBindingSet *binding_set;

  object_class->finalize = gtk_shortcuts_section_finalize;
  object_class->get_property = gtk_shortcuts_section_get_property;
  object_class->set_property = gtk_shortcuts_section_set_property;

  widget_class->map = gtk_shortcuts_section_map;
  widget_class->unmap = gtk_shortcuts_section_unmap;
  widget_class->destroy = gtk_shortcuts_section_destroy;

  container_class->add = gtk_shortcuts_section_add;
  container_class->remove = gtk_shortcuts_section_remove;
  container_class->forall = gtk_shortcuts_section_forall;
  container_class->child_type = gtk_shortcuts_section_child_type;

  klass->change_current_page = gtk_shortcuts_section_change_current_page;

  /**
   * GtkShortcutsSection:section-name:
   *
   * A unique name to identify this section among the sections
   * added to the GtkShortcutsWindow. Setting the #GtkShortcutsWindow:section-name
   * property to this string will make this section shown in the
   * GtkShortcutsWindow.
   */
  properties[PROP_SECTION_NAME] =
    g_param_spec_string ("section-name", P_("Section Name"), P_("Section Name"),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GtkShortcutsSection:view-name:
   *
   * A view name to filter the groups in this section by.
   * See #GtkShortcutsGroup:view.
   *
   * Applications are expected to use the #GtkShortcutsWindow:view-name
   * property for this purpose.
   */
  properties[PROP_VIEW_NAME] =
    g_param_spec_string ("view-name", P_("View Name"), P_("View Name"),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkShortcutsSection:title:
   *
   * The string to show in the section selector of the GtkShortcutsWindow
   * for this section. If there is only one section, you don't need to
   * set a title, since the section selector will not be shown in this case.
   */
  properties[PROP_TITLE] =
    g_param_spec_string ("title", P_("Title"), P_("Title"),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GtkShortcutsSection:max-height:
   *
   * The maximum number of lines to allow per column. This property can
   * be used to influence how the groups in this section are distributed
   * across pages and columns. The default value of 15 should work in
   * most cases.
   */
  properties[PROP_MAX_HEIGHT] =
    g_param_spec_uint ("max-height", P_("Maximum Height"), P_("Maximum Height"),
                       0, G_MAXUINT, 15,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  signals[CHANGE_CURRENT_PAGE] =
    g_signal_new (I_("change-current-page"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkShortcutsSectionClass, change_current_page),
                  NULL, NULL,
                  _gtk_marshal_BOOLEAN__INT,
                  G_TYPE_BOOLEAN, 1,
                  G_TYPE_INT);

  binding_set = gtk_binding_set_by_class (klass);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_Page_Up, 0,
                                "change-current-page", 1,
                                G_TYPE_INT, -1);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_Page_Down, 0,
                                "change-current-page", 1,
                                G_TYPE_INT, 1);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_Page_Up, GDK_CONTROL_MASK,
                                "change-current-page", 1,
                                G_TYPE_INT, -1);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_Page_Down, GDK_CONTROL_MASK,
                                "change-current-page", 1,
                                G_TYPE_INT, 1);
}

static void
gtk_shortcuts_section_init (GtkShortcutsSection *self)
{
  self->max_height = 15;

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
  gtk_box_set_homogeneous (GTK_BOX (self), FALSE);
  gtk_box_set_spacing (GTK_BOX (self), 22);
  gtk_container_set_border_width (GTK_CONTAINER (self), 24);

  self->stack = g_object_new (GTK_TYPE_STACK,
                              "homogeneous", TRUE,
                              "transition-type", GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT,
                              "vexpand", TRUE,
                              "visible", TRUE,
                              NULL);
  GTK_CONTAINER_CLASS (gtk_shortcuts_section_parent_class)->add (GTK_CONTAINER (self), GTK_WIDGET (self->stack));

  self->switcher = g_object_new (GTK_TYPE_STACK_SWITCHER,
                                 "halign", GTK_ALIGN_CENTER,
                                 "stack", self->stack,
                                 "spacing", 12,
                                 "no-show-all", TRUE,
                                 NULL);

  gtk_style_context_remove_class (gtk_widget_get_style_context (GTK_WIDGET (self->switcher)), GTK_STYLE_CLASS_LINKED);

  self->show_all = gtk_button_new_with_mnemonic (_("_Show All"));
  gtk_widget_set_no_show_all (self->show_all, TRUE);
  g_signal_connect_swapped (self->show_all, "clicked",
                            G_CALLBACK (gtk_shortcuts_section_show_all), self);

  self->footer = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 20);
  GTK_CONTAINER_CLASS (gtk_shortcuts_section_parent_class)->add (GTK_CONTAINER (self), self->footer);

  gtk_box_set_center_widget (GTK_BOX (self->footer), GTK_WIDGET (self->switcher));
  gtk_box_pack_end (GTK_BOX (self->footer), self->show_all, TRUE, TRUE, 0);
  gtk_widget_set_halign (self->show_all, GTK_ALIGN_END);

  self->pan_gesture = gtk_gesture_pan_new (GTK_WIDGET (self->stack), GTK_ORIENTATION_HORIZONTAL);
  g_signal_connect (self->pan_gesture, "pan",
                    G_CALLBACK (gtk_shortcuts_section_pan_gesture_pan), self);
}

static void
gtk_shortcuts_section_set_view_name (GtkShortcutsSection *self,
                                     const gchar         *view_name)
{
  if (g_strcmp0 (self->view_name, view_name) == 0)
    return;

  g_free (self->view_name);
  self->view_name = g_strdup (view_name);

  gtk_shortcuts_section_filter_groups (self);
  gtk_shortcuts_section_reflow_groups (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VIEW_NAME]);
}

static void
gtk_shortcuts_section_set_max_height (GtkShortcutsSection *self,
                                      guint                max_height)
{
  if (self->max_height == max_height)
    return;

  self->max_height = max_height;

  gtk_shortcuts_section_maybe_reflow (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MAX_HEIGHT]);
}

static void
gtk_shortcuts_section_add_group (GtkShortcutsSection *self,
                                 GtkShortcutsGroup   *group)
{
  GList *children;
  GtkWidget *page, *column;

  children = gtk_container_get_children (GTK_CONTAINER (self->stack));
  if (children)
    page = g_list_last (children)->data;
  else
    {
      page = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 22);
      gtk_stack_add_named (self->stack, page, "1");
    }
  g_list_free (children);

  children = gtk_container_get_children (GTK_CONTAINER (page));
  if (children)
    column = g_list_last (children)->data;
  else
    {
      column = gtk_box_new (GTK_ORIENTATION_VERTICAL, 22);
      gtk_container_add (GTK_CONTAINER (page), column);
    }
  g_list_free (children);

  gtk_container_add (GTK_CONTAINER (column), GTK_WIDGET (group));
  self->groups = g_list_append (self->groups, group);

  gtk_shortcuts_section_maybe_reflow (self);
}

static void
gtk_shortcuts_section_show_all (GtkShortcutsSection *self)
{
  gtk_shortcuts_section_set_view_name (self, NULL);
}

static void
update_group_visibility (GtkWidget *child, gpointer data)
{
  GtkShortcutsSection *self = data;

  if (GTK_IS_SHORTCUTS_GROUP (child))
    {
      gchar *view;
      gboolean match;

      g_object_get (child, "view", &view, NULL);
      match = view == NULL ||
              self->view_name == NULL ||
              strcmp (view, self->view_name) == 0;

      gtk_widget_set_visible (child, match);
      self->has_filtered_group |= !match;

      g_free (view);
    }
  else if (GTK_IS_CONTAINER (child))
    {
      gtk_container_foreach (GTK_CONTAINER (child), update_group_visibility, data);
    }
}

static void
gtk_shortcuts_section_filter_groups (GtkShortcutsSection *self)
{
  self->has_filtered_group = FALSE;

  gtk_container_foreach (GTK_CONTAINER (self), update_group_visibility, self);

  gtk_widget_set_visible (GTK_WIDGET (self->show_all), self->has_filtered_group);
  gtk_widget_set_visible (gtk_widget_get_parent (GTK_WIDGET (self->show_all)),
                          gtk_widget_get_visible (GTK_WIDGET (self->show_all)) ||
                          gtk_widget_get_visible (GTK_WIDGET (self->switcher)));
}

static void
gtk_shortcuts_section_maybe_reflow (GtkShortcutsSection *self)
{
  if (gtk_widget_get_mapped (GTK_WIDGET (self)))
    gtk_shortcuts_section_reflow_groups (self);
  else
    self->need_reflow = TRUE;
}

static void
adjust_page_buttons (GtkWidget *widget,
                     gpointer   data)
{
  GtkWidget *label;

  gtk_style_context_add_class (gtk_widget_get_style_context (widget), "circular");

  label = gtk_bin_get_child (GTK_BIN (widget));
  gtk_label_set_use_underline (GTK_LABEL (label), TRUE);
}

static void
gtk_shortcuts_section_reflow_groups (GtkShortcutsSection *self)
{
  GList *pages, *p;
  GList *columns, *c;
  GList *groups, *g;
  GList *children;
  guint n_rows;
  guint n_columns;
  guint n_pages;
  GtkWidget *current_page, *current_column;

  /* collect all groups from the current pages */
  groups = NULL;
  pages = gtk_container_get_children (GTK_CONTAINER (self->stack));
  for (p = pages; p; p = p->next)
    {
      columns = gtk_container_get_children (GTK_CONTAINER (p->data));
      for (c = columns; c; c = c->next)
        {
          children = gtk_container_get_children (GTK_CONTAINER (c->data));
          groups = g_list_concat (groups, children);
        }
      g_list_free (columns);
    }
  g_list_free (pages);

  /* create new pages */
  current_page = NULL;
  current_column = NULL;
  pages = NULL;
  n_rows = 0;
  n_columns = 0;
  n_pages = 0;
  for (g = groups; g; g = g->next)
    {
      GtkShortcutsGroup *group = g->data;
      guint height;
      gboolean visible;

      g_object_get (group,
                    "visible", &visible,
                    "height", &height,
                    NULL);
      if (!visible)
        height = 0;

      if (current_column == NULL || n_rows + height > self->max_height)
        {
          GtkWidget *column;
          GtkSizeGroup *group;

          column = gtk_box_new (GTK_ORIENTATION_VERTICAL, 22);
          gtk_widget_show (column);

          group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
          gtk_size_group_set_ignore_hidden (group, TRUE);
G_GNUC_END_IGNORE_DEPRECATIONS
          g_object_set_data_full (G_OBJECT (column), "accel-size-group", group, g_object_unref);

          group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
          gtk_size_group_set_ignore_hidden (group, TRUE);
G_GNUC_END_IGNORE_DEPRECATIONS
          g_object_set_data_full (G_OBJECT (column), "title-size-group", group, g_object_unref);

          if (n_columns % 2 == 0)
            {
              GtkWidget *page;

              page = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 22);
              gtk_widget_show (page);

              pages = g_list_append (pages, page);
              current_page = page;
            }

          gtk_container_add (GTK_CONTAINER (current_page), column);
          current_column = column;
          n_columns += 1;
          n_rows = 0;
        }

      n_rows += height;

      g_object_set (group,
                    "accel-size-group", g_object_get_data (G_OBJECT (current_column), "accel-size-group"),
                    "title-size-group", g_object_get_data (G_OBJECT (current_column), "title-size-group"),
                    NULL);

      g_object_ref (group);
      gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (GTK_WIDGET (group))), GTK_WIDGET (group));
      gtk_container_add (GTK_CONTAINER (current_column), GTK_WIDGET (group));
      g_object_unref (group);
    }

  /* balance the last page */
  if (n_columns % 2 == 1)
    {
      GtkWidget *column;
      GtkSizeGroup *group;
      GList *content;
      guint n;

      column = gtk_box_new (GTK_ORIENTATION_VERTICAL, 22);
      gtk_widget_show (column);

      group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gtk_size_group_set_ignore_hidden (group, TRUE);
G_GNUC_END_IGNORE_DEPRECATIONS
      g_object_set_data_full (G_OBJECT (column), "accel-size-group", group, g_object_unref);
      group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gtk_size_group_set_ignore_hidden (group, TRUE);
G_GNUC_END_IGNORE_DEPRECATIONS
      g_object_set_data_full (G_OBJECT (column), "title-size-group", group, g_object_unref);

      gtk_container_add (GTK_CONTAINER (current_page), column);

      content = gtk_container_get_children (GTK_CONTAINER (current_column));
      n = 0;

      for (g = g_list_last (content); g; g = g->prev)
        {
          GtkShortcutsGroup *group = g->data;
          guint height;
          gboolean visible;

          g_object_get (group,
                        "visible", &visible,
                        "height", &height,
                        NULL);
          if (!visible)
            height = 0;

          if (n_rows - height == 0)
            break;
          if (ABS (n_rows - n) < ABS ((n_rows - height) - (n + height)))
            break;

          n_rows -= height;
          n += height;
        }

      for (g = g->next; g; g = g->next)
        {
          GtkShortcutsGroup *group = g->data;

          g_object_set (group,
                        "accel-size-group", g_object_get_data (G_OBJECT (column), "accel-size-group"),
                        "title-size-group", g_object_get_data (G_OBJECT (column), "title-size-group"),
                        NULL);

          g_object_ref (group);
          gtk_container_remove (GTK_CONTAINER (current_column), GTK_WIDGET (group));
          gtk_container_add (GTK_CONTAINER (column), GTK_WIDGET (group));
          g_object_unref (group);
        }

      g_list_free (content);
    }

  /* replace the current pages with the new pages */
  children = gtk_container_get_children (GTK_CONTAINER (self->stack));
  g_list_free_full (children, (GDestroyNotify)gtk_widget_destroy);

  for (p = pages, n_pages = 0; p; p = p->next, n_pages++)
    {
      GtkWidget *page = p->data;
      gchar *title;

      title = g_strdup_printf ("_%u", n_pages + 1);
      gtk_stack_add_titled (self->stack, page, title, title);
      g_free (title);
    }

  /* fix up stack switcher */
  gtk_container_foreach (GTK_CONTAINER (self->switcher), adjust_page_buttons, NULL);
  gtk_widget_set_visible (GTK_WIDGET (self->switcher), (n_pages > 1));
  gtk_widget_set_visible (gtk_widget_get_parent (GTK_WIDGET (self->switcher)),
                          gtk_widget_get_visible (GTK_WIDGET (self->show_all)) ||
                          gtk_widget_get_visible (GTK_WIDGET (self->switcher)));

  /* clean up */
  g_list_free (groups);
  g_list_free (pages);

  self->need_reflow = FALSE;
}

static gboolean
gtk_shortcuts_section_change_current_page (GtkShortcutsSection *self,
                                           gint                 offset)
{
  GtkWidget *child;
  GList *children, *l;

  child = gtk_stack_get_visible_child (self->stack);
  children = gtk_container_get_children (GTK_CONTAINER (self->stack));
  l = g_list_find (children, child);

  if (offset == 1)
    l = l->next;
  else if (offset == -1)
    l = l->prev;
  else
    g_assert_not_reached ();

  if (l)
    gtk_stack_set_visible_child (self->stack, GTK_WIDGET (l->data));
  else
    gtk_widget_error_bell (GTK_WIDGET (self));

  g_list_free (children);

  return TRUE;
}

static void
gtk_shortcuts_section_pan_gesture_pan (GtkGesturePan       *gesture,
                                       GtkPanDirection      direction,
                                       gdouble              offset,
                                       GtkShortcutsSection *self)
{
  if (offset < 50)
    return;

  if (direction == GTK_PAN_DIRECTION_LEFT)
    gtk_shortcuts_section_change_current_page (self, 1);
  else if (direction == GTK_PAN_DIRECTION_RIGHT)
    gtk_shortcuts_section_change_current_page (self, -1);
  else
    g_assert_not_reached ();

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
}
