/* gtkshortcutsshortcut.c
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

#include "gtkshortcutsshortcut.h"

#include "gtkshortcutlabelprivate.h"
#include "gtkprivate.h"
#include "gtkintl.h"

/**
 * SECTION:gtkshortcutsshortcut
 * @Title: GtkShortcutsShortcut
 * @Short_description: Represents a keyboard shortcut in a GtkShortcutsWindow
 *
 * A GtkShortcutsShortcut represents a single keyboard shortcut with
 * a short text. This widget is only meant to be used with
 * #GtkShortcutsWindow.
 */

struct _GtkShortcutsShortcut
{
  GtkBox            parent_instance;

  GtkShortcutLabel *accelerator;
  GtkLabel         *title;

  GtkSizeGroup *accel_size_group;
  GtkSizeGroup *title_size_group;
};

struct _GtkShortcutsShortcutClass
{
  GtkBoxClass parent_class;
};

G_DEFINE_TYPE (GtkShortcutsShortcut, gtk_shortcuts_shortcut, GTK_TYPE_BOX)

enum {
  PROP_0,
  PROP_ACCELERATOR,
  PROP_TITLE,
  PROP_ACCEL_SIZE_GROUP,
  PROP_TITLE_SIZE_GROUP,
  LAST_PROP
};

static GParamSpec *properties[LAST_PROP];

static void
gtk_shortcuts_shortcut_set_accel_size_group (GtkShortcutsShortcut *self,
                                             GtkSizeGroup         *group)
{
  if (self->accel_size_group)
    gtk_size_group_remove_widget (self->accel_size_group, GTK_WIDGET (self->accelerator));
  if (group)
    gtk_size_group_add_widget (group, GTK_WIDGET (self->accelerator));

  g_set_object (&self->accel_size_group, group);
}

static void
gtk_shortcuts_shortcut_set_title_size_group (GtkShortcutsShortcut *self,
                                             GtkSizeGroup         *group)
{
  if (self->title_size_group)
    gtk_size_group_remove_widget (self->title_size_group, GTK_WIDGET (self->title));
  if (group)
    gtk_size_group_add_widget (group, GTK_WIDGET (self->title));

  g_set_object (&self->title_size_group, group);
}

static void
gtk_shortcuts_shortcut_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GtkShortcutsShortcut *self = GTK_SHORTCUTS_SHORTCUT (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      g_value_set_string (value, gtk_label_get_label (self->title));
      break;

    case PROP_ACCELERATOR:
      g_value_set_string (value, gtk_shortcut_label_get_accelerator (self->accelerator));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_shortcuts_shortcut_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkShortcutsShortcut *self = GTK_SHORTCUTS_SHORTCUT (object);

  switch (prop_id)
    {
    case PROP_ACCELERATOR:
      gtk_shortcut_label_set_accelerator (self->accelerator, g_value_get_string (value));
      break;

    case PROP_ACCEL_SIZE_GROUP:
      gtk_shortcuts_shortcut_set_accel_size_group (self, GTK_SIZE_GROUP (g_value_get_object (value)));
      break;

    case PROP_TITLE:
      gtk_label_set_label (self->title, g_value_get_string (value));
      break;

    case PROP_TITLE_SIZE_GROUP:
      gtk_shortcuts_shortcut_set_title_size_group (self, GTK_SIZE_GROUP (g_value_get_object (value)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_shortcuts_shortcut_finalize (GObject *object)
{
  GtkShortcutsShortcut *self = GTK_SHORTCUTS_SHORTCUT (object);

  g_clear_object (&self->accel_size_group);
  g_clear_object (&self->title_size_group);

  G_OBJECT_CLASS (gtk_shortcuts_shortcut_parent_class)->finalize (object);
}

static void
gtk_shortcuts_shortcut_add (GtkContainer *container,
                            GtkWidget    *widget)
{
  g_warning ("Can't add children to %s", G_OBJECT_TYPE_NAME (container));
}

static GType
gtk_shortcuts_shortcut_child_type (GtkContainer *container)
{
  return G_TYPE_NONE;
}

static void
gtk_shortcuts_shortcut_class_init (GtkShortcutsShortcutClass *klass)
{
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_shortcuts_shortcut_finalize;
  object_class->get_property = gtk_shortcuts_shortcut_get_property;
  object_class->set_property = gtk_shortcuts_shortcut_set_property;

  container_class->add = gtk_shortcuts_shortcut_add;
  container_class->child_type = gtk_shortcuts_shortcut_child_type;

  /**
   * GtkShortcutsShortcut:accelerator:
   *
   * The accelerator(s) represented by this object, in the syntax
   * understood by gtk_accelerator_parse(). Multiple accelerators
   * can be specified by separating them with a space, but keep in
   * mind that the available width is limited.
   *
   * Here is an example: <ctrl>? F1
   *
   * Note that < and > need to escaped as &lt; and &gt; when used
   * in .ui files.
   */
  properties[PROP_ACCELERATOR] =
    g_param_spec_string ("accelerator",
                         P_("Accelerator"),
                         P_("Accelerator"),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GtkShortcutsShortcut:title:
   *
   * The textual description for the accelerators represented by
   * this object. This should be a short string that can fit in
   * a single line.
   */
  properties[PROP_TITLE] =
    g_param_spec_string ("title",
                         P_("Title"),
                         P_("Title"),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GtkShortcutsShortcut:accel-size-group:
   *
   * The size group for the accelerator portion of this shortcut.
   *
   * This is used internally by GTK+, and must not be modified by applications.
   */
  properties[PROP_ACCEL_SIZE_GROUP] =
    g_param_spec_object ("accel-size-group",
                         P_("Accelerator Size Group"),
                         P_("Accelerator Size Group"),
                         GTK_TYPE_SIZE_GROUP,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GtkShortcutsShortcut:title-size-group:
   *
   * The size group for the textual portion of this shortcut.
   *
   * This is used internally by GTK+, and must not be modified by applications.
   */
  properties[PROP_TITLE_SIZE_GROUP] =
    g_param_spec_object ("title-size-group",
                         P_("Title Size Group"),
                         P_("Title Size Group"),
                         GTK_TYPE_SIZE_GROUP,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
gtk_shortcuts_shortcut_init (GtkShortcutsShortcut *self)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_HORIZONTAL);
  gtk_box_set_spacing (GTK_BOX (self), 12);

  self->accelerator = g_object_new (GTK_TYPE_SHORTCUT_LABEL,
                                    "visible", TRUE,
                                    NULL);
  GTK_CONTAINER_CLASS (gtk_shortcuts_shortcut_parent_class)->add (GTK_CONTAINER (self), GTK_WIDGET (self->accelerator));

  self->title = g_object_new (GTK_TYPE_LABEL,
                              "hexpand", TRUE,
                              "visible", TRUE,
                              "xalign", 0.0f,
                              NULL);
  GTK_CONTAINER_CLASS (gtk_shortcuts_shortcut_parent_class)->add (GTK_CONTAINER (self), GTK_WIDGET (self->title));
}
