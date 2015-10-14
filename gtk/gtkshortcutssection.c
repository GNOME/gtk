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

#include "gtkshortcutssectionprivate.h"

#include "gtkshortcutsgroupprivate.h"
#include "gtktogglebutton.h"
#include "gtkstack.h"
#include "gtkstackswitcher.h"
#include "gtkstylecontext.h"
#include "gtkorientable.h"
#include "gtksizegroup.h"
#include "gtkprivate.h"
#include "gtkintl.h"

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

  gboolean          has_filtered_group;
  guint             last_page_num;

  GtkWidget        *current_page;
  GtkWidget        *current_column;
  guint             n_columns;
  guint             n_rows;
};

struct _GtkShortcutsSectionClass
{
  GtkBoxClass parent_class;
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

static GParamSpec *properties[LAST_PROP];

static void
adjust_page_buttons (GtkWidget *widget,
                     gpointer   data)
{
  gint *count = data;

  /*
   * TODO: This is a hack to get the GtkStackSwitcher radio
   *       buttons to look how we want. However, it's very
   *       much font size specific.
   */
  gtk_widget_set_size_request (widget, 34, 34);

  (*count)++;
}

static void
gtk_shortcuts_section_add (GtkContainer *container,
                           GtkWidget    *child)
{
  GtkShortcutsSection *self = (GtkShortcutsSection *)container;

  if (GTK_IS_SHORTCUTS_GROUP (child))
    gtk_shortcuts_section_add_group (self, GTK_SHORTCUTS_GROUP (child));
  else
    GTK_CONTAINER_CLASS (gtk_shortcuts_section_parent_class)->add (container, child);
}

static void
gtk_shortcuts_section_finalize (GObject *object)
{
  GtkShortcutsSection *self = (GtkShortcutsSection *)object;

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->title, g_free);

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
      self->max_height = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_shortcuts_section_class_init (GtkShortcutsSectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->finalize = gtk_shortcuts_section_finalize;
  object_class->get_property = gtk_shortcuts_section_get_property;
  object_class->set_property = gtk_shortcuts_section_set_property;

  container_class->add = gtk_shortcuts_section_add;

  properties[PROP_SECTION_NAME] =
    g_param_spec_string ("section-name", P_("Section Name"), P_("Section Name"),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties[PROP_VIEW_NAME] =
    g_param_spec_string ("view-name", P_("View Name"), P_("View Name"),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties[PROP_TITLE] =
    g_param_spec_string ("title", P_("Title"), P_("Title"),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties[PROP_MAX_HEIGHT] =
    g_param_spec_uint ("max-height", P_("Maximum Height"), P_("Maximum Height"),
                       0, G_MAXUINT, 15,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
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
      match = view == NULL || self->view_name == NULL ||
              strcmp (view, self->view_name) == 0;

      gtk_widget_set_visible (child,
                              match || gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->show_all)));
      self->has_filtered_group |= !match;

      g_free (view);
    }
  else if (GTK_IS_CONTAINER (child))
    {
      gtk_container_foreach (GTK_CONTAINER (child), update_group_visibility, data);
    }
}

static void
filter_groups_by_view (GtkShortcutsSection *self)
{
  self->has_filtered_group = FALSE;
  gtk_container_foreach (GTK_CONTAINER (self), update_group_visibility, self);
  gtk_widget_set_visible (GTK_WIDGET (self->show_all), self->has_filtered_group);
  if (self->has_filtered_group)
    gtk_widget_show (gtk_widget_get_parent (GTK_WIDGET (self->show_all)));
}

static void
gtk_shortcuts_section_init (GtkShortcutsSection *self)
{
  GtkWidget *box;

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
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->stack));

  self->switcher = g_object_new (GTK_TYPE_STACK_SWITCHER,
                                 "halign", GTK_ALIGN_CENTER,
                                 "stack", self->stack,
                                 "spacing", 12,
                                 "no-show-all", TRUE,
                                 NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self->switcher)), "round");
  gtk_style_context_remove_class (gtk_widget_get_style_context (GTK_WIDGET (self->switcher)), "linked");

  self->show_all = gtk_toggle_button_new_with_label (_("Show All"));
  gtk_widget_set_no_show_all (self->show_all, TRUE);
  g_signal_connect_swapped (self->show_all, "toggled",
                            G_CALLBACK (filter_groups_by_view), self);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 20);
  gtk_container_add (GTK_CONTAINER (self), box);

  gtk_box_set_center_widget (GTK_BOX (box), GTK_WIDGET (self->switcher));
  gtk_box_pack_end (GTK_BOX (box), self->show_all, TRUE, TRUE, 0);
  gtk_widget_set_halign (self->show_all, GTK_ALIGN_END);
}

const gchar *
gtk_shortcuts_section_get_section_name (GtkShortcutsSection *self)
{
  g_return_val_if_fail (GTK_IS_SHORTCUTS_SECTION (self), NULL);

  return self->name;
}

const gchar *
gtk_shortcuts_section_get_title (GtkShortcutsSection *self)
{
  g_return_val_if_fail (GTK_IS_SHORTCUTS_SECTION (self), NULL);

  return self->title;
}

void
gtk_shortcuts_section_set_view_name (GtkShortcutsSection *self,
                                     const gchar         *view_name)
{
  g_return_if_fail (GTK_IS_SHORTCUTS_SECTION (self));

  g_free (self->view_name);
  self->view_name = g_strdup (view_name);

  filter_groups_by_view (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VIEW_NAME]);
}

void
gtk_shortcuts_section_add_group (GtkShortcutsSection *self,
                                 GtkShortcutsGroup   *group)
{
  guint height;

  g_return_if_fail (GTK_IS_SHORTCUTS_SECTION (self));
  g_return_if_fail (GTK_IS_SHORTCUTS_GROUP (group));

  g_object_get (group, "height", &height, NULL);

  if (self->n_rows == 0 || self->n_rows + height > self->max_height)
    {
      GtkWidget *column;

      column = gtk_box_new (GTK_ORIENTATION_VERTICAL, 22);
      gtk_widget_show (column);

      g_object_set_data_full (G_OBJECT (column), "accel-size-group", gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL), g_object_unref);
      g_object_set_data_full (G_OBJECT (column), "title-size-group", gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL), g_object_unref);

      if (self->n_columns % 2 == 0)
        {
          GtkWidget *page;
          gchar *title = NULL;
          guint count = 0;

          page = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 22);
          gtk_widget_show (page);

          title = g_strdup_printf ("%u", ++self->last_page_num);
          gtk_container_add_with_properties (GTK_CONTAINER (self->stack), page,
                                             "title", title,
                                             NULL);

          gtk_container_foreach (GTK_CONTAINER (self->switcher), adjust_page_buttons, &count);
          gtk_widget_set_visible (GTK_WIDGET (self->switcher), (count > 1));
          if (count > 1)
            gtk_widget_show (gtk_widget_get_parent (GTK_WIDGET (self->switcher)));
          g_free (title);

          self->current_page = page;
        }

      gtk_container_add (GTK_CONTAINER (self->current_page), column);
      self->current_column = column;
      self->n_columns += 1;
      self->n_rows = 0;
    }

  self->n_rows += height;

  g_object_set (group, "accel-size-group", g_object_get_data (G_OBJECT (self->current_column), "accel-size-group"), NULL);
  g_object_set (group, "title-size-group", g_object_get_data (G_OBJECT (self->current_column), "title-size-group"), NULL);

  gtk_container_add (GTK_CONTAINER (self->current_column), GTK_WIDGET (group));
}
