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
 * A GtkShortcutsShortcut represents a single keyboard shortcut or gesture
 * with a short text. This widget is only meant to be used with #GtkShortcutsWindow.
 */

struct _GtkShortcutsShortcut
{
  GtkBox            parent_instance;

  GtkImage         *image;
  GtkShortcutLabel *accelerator;
  GtkLabel         *title;
  GtkLabel         *subtitle;
  GtkLabel         *title_box;

  GtkSizeGroup *accel_size_group;
  GtkSizeGroup *title_size_group;

  GtkTextDirection direction;
  GtkShortcutType  shortcut_type;
};

struct _GtkShortcutsShortcutClass
{
  GtkBoxClass parent_class;
};

G_DEFINE_TYPE (GtkShortcutsShortcut, gtk_shortcuts_shortcut, GTK_TYPE_BOX)

enum {
  PROP_0,
  PROP_ACCELERATOR,
  PROP_ICON,
  PROP_TITLE,
  PROP_SUBTITLE,
  PROP_ACCEL_SIZE_GROUP,
  PROP_TITLE_SIZE_GROUP,
  PROP_DIRECTION,
  PROP_SHORTCUT_TYPE,
  LAST_PROP
};

static GParamSpec *properties[LAST_PROP];

static void
gtk_shortcuts_shortcut_set_accel_size_group (GtkShortcutsShortcut *self,
                                             GtkSizeGroup         *group)
{
  if (self->accel_size_group)
    {
      gtk_size_group_remove_widget (self->accel_size_group, GTK_WIDGET (self->accelerator));
      gtk_size_group_remove_widget (self->accel_size_group, GTK_WIDGET (self->image));
    }

  if (group)
    {
      gtk_size_group_add_widget (group, GTK_WIDGET (self->accelerator));
      gtk_size_group_add_widget (group, GTK_WIDGET (self->image));
    }

  g_set_object (&self->accel_size_group, group);
}

static void
gtk_shortcuts_shortcut_set_title_size_group (GtkShortcutsShortcut *self,
                                             GtkSizeGroup         *group)
{
  if (self->title_size_group)
    gtk_size_group_remove_widget (self->title_size_group, GTK_WIDGET (self->title_box));
  if (group)
    gtk_size_group_add_widget (group, GTK_WIDGET (self->title_box));

  g_set_object (&self->title_size_group, group);
}

static void
gtk_shortcuts_shortcut_set_subtitle (GtkShortcutsShortcut *self,
                                     const gchar          *subtitle)
{
  gtk_label_set_label (self->subtitle, subtitle);
  gtk_widget_set_visible (GTK_WIDGET (self->subtitle), subtitle != NULL);
  g_object_notify (G_OBJECT (self), "subtitle");
}

static void
gtk_shortcuts_shortcut_set_accelerator (GtkShortcutsShortcut *self,
                                        const gchar          *accelerator)
{
  gtk_shortcut_label_set_accelerator (self->accelerator, accelerator);
}

static void
gtk_shortcuts_shortcut_set_icon (GtkShortcutsShortcut *self,
                                 GIcon                *gicon)
{
  gtk_image_set_from_gicon (self->image, gicon, GTK_ICON_SIZE_DIALOG);
}

static void
update_visible (GtkShortcutsShortcut *self)
{
  if (self->direction == GTK_TEXT_DIR_NONE ||
      self->direction == gtk_widget_get_direction (GTK_WIDGET (self)))
    gtk_widget_set_visible (GTK_WIDGET (self), TRUE);
  else
    gtk_widget_set_visible (GTK_WIDGET (self), FALSE);
}

static void
gtk_shortcuts_shortcut_set_direction (GtkShortcutsShortcut *self,
                                      GtkTextDirection      direction)
{
  if (self->direction == direction)
    return;

  self->direction = direction;

  update_visible (self);

  g_object_notify (G_OBJECT (self), "direction");
}

static void
gtk_shortcuts_shortcut_direction_changed (GtkWidget        *widget,
                                          GtkTextDirection  previous_dir)
{
  update_visible (GTK_SHORTCUTS_SHORTCUT (widget));

  GTK_WIDGET_CLASS (gtk_shortcuts_shortcut_parent_class)->direction_changed (widget, previous_dir);
}

static void
gtk_shortcuts_shortcut_set_type (GtkShortcutsShortcut *self,
                                 GtkShortcutType       type)
{
  if (self->shortcut_type == type)
    return;

  self->shortcut_type = type;

  switch (type)
    {
    case GTK_SHORTCUT_GESTURE_PINCH:
      gtk_image_set_from_resource (self->image, "/org/gtk/libgtk/gesture/pinch.png");
      gtk_shortcuts_shortcut_set_subtitle (self, _("Two finger pinch"));
      break;

    case GTK_SHORTCUT_GESTURE_STRETCH:
      gtk_image_set_from_resource (self->image, "/org/gtk/libgtk/gesture/stretch.png");
      gtk_shortcuts_shortcut_set_subtitle (self, _("Two finger stretch"));
      break;

    case GTK_SHORTCUT_GESTURE_ROTATE_CLOCKWISE:
      gtk_image_set_from_resource (self->image, "/org/gtk/libgtk/gesture/rotate-clockwise.png");
      gtk_shortcuts_shortcut_set_subtitle (self, _("Rotate clockwise"));
      break;

    case GTK_SHORTCUT_GESTURE_ROTATE_COUNTERCLOCKWISE:
      gtk_image_set_from_resource (self->image, "/org/gtk/libgtk/gesture/rotate-anticlockwise.png");
      gtk_shortcuts_shortcut_set_subtitle (self, _("Rotate counterclockwise"));
      break;

    case GTK_SHORTCUT_GESTURE_TWO_FINGER_SWIPE_LEFT:
      gtk_image_set_from_resource (self->image, "/org/gtk/libgtk/gesture/two-finger-swipe-left.png");
      gtk_shortcuts_shortcut_set_subtitle (self, _("Two finger swipe left"));
      break;

    case GTK_SHORTCUT_GESTURE_TWO_FINGER_SWIPE_RIGHT:
      gtk_image_set_from_resource (self->image, "/org/gtk/libgtk/gesture/two-finger-swipe-right.png");
      gtk_shortcuts_shortcut_set_subtitle (self, _("Two finger swipe right"));
      break;

    default: ;
    }

  gtk_widget_set_visible (GTK_WIDGET (self->accelerator), type == GTK_SHORTCUT_ACCELERATOR);
  gtk_widget_set_visible (GTK_WIDGET (self->image), type != GTK_SHORTCUT_ACCELERATOR);


  g_object_notify (G_OBJECT (self), "shortcut-type");
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

    case PROP_SUBTITLE:
      g_value_set_string (value, gtk_label_get_label (self->subtitle));
      break;

    case PROP_ACCELERATOR:
      g_value_set_string (value, gtk_shortcut_label_get_accelerator (self->accelerator));
      break;

    case PROP_ICON:
      {
        GIcon *icon;

        gtk_image_get_gicon (self->image, &icon, NULL);
        g_value_set_object (value, icon);
      }
      break;

    case PROP_DIRECTION:
      g_value_set_enum (value, self->direction);
      break;

    case PROP_SHORTCUT_TYPE:
      g_value_set_enum (value, self->shortcut_type);
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
      gtk_shortcuts_shortcut_set_accelerator (self, g_value_get_string (value));
      break;

    case PROP_ICON:
      gtk_shortcuts_shortcut_set_icon (self, g_value_get_object (value));
      break;

    case PROP_ACCEL_SIZE_GROUP:
      gtk_shortcuts_shortcut_set_accel_size_group (self, GTK_SIZE_GROUP (g_value_get_object (value)));
      break;

    case PROP_TITLE:
      gtk_label_set_label (self->title, g_value_get_string (value));
      break;

    case PROP_SUBTITLE:
      gtk_shortcuts_shortcut_set_subtitle (self, g_value_get_string (value));
      break;

    case PROP_TITLE_SIZE_GROUP:
      gtk_shortcuts_shortcut_set_title_size_group (self, GTK_SIZE_GROUP (g_value_get_object (value)));
      break;

    case PROP_DIRECTION:
      gtk_shortcuts_shortcut_set_direction (self, g_value_get_enum (value));
      break;

    case PROP_SHORTCUT_TYPE:
      gtk_shortcuts_shortcut_set_type (self, g_value_get_enum (value));
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
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->finalize = gtk_shortcuts_shortcut_finalize;
  object_class->get_property = gtk_shortcuts_shortcut_get_property;
  object_class->set_property = gtk_shortcuts_shortcut_set_property;

  widget_class->direction_changed = gtk_shortcuts_shortcut_direction_changed;

  container_class->add = gtk_shortcuts_shortcut_add;
  container_class->child_type = gtk_shortcuts_shortcut_child_type;

  /**
   * GtkShortcutsShortcut:accelerator:
   *
   * The accelerator(s) represented by this object in (an extension of)
   * the syntax understood by gtk_accelerator_parse(). Multiple accelerators
   * can be specified by separating them with a space, but keep in
   * mind that the available width is limited. It is also possible
   * to specify ranges of shortcuts, using ... between the keys. Sequences
   * of keys can be specified using a + between the keys.
   *
   * Examples:
   *
   * - A single shortcut: <ctl><alt>delete
   * - Two alternative shortcuts: <shift>a Home
   * - A range of shortcuts: <alt>1...<alt>9
   * - A sequence of key combinations: <ctl>c+<ctl>x
   *
   * Note that < and > need to be escaped as &lt; and &gt; when used
   * in .ui files.
   */
  properties[PROP_ACCELERATOR] =
    g_param_spec_string ("accelerator",
                         P_("Accelerator"),
                         P_("Accelerator"),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GtkShortcutsShortcut:icon:
   *
   * An icon to represent the shortcut or gesture. This is used if
   * #GtkShortcutsShortcut:accelerator is not set.
   *
   * Typically used for gestures.
   */
  properties[PROP_ICON] =
    g_param_spec_object ("icon",
                         P_("Icon"),
                         P_("Icon"),
                         G_TYPE_ICON,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GtkShortcutsShortcut:title:
   *
   * The textual description for the shortcut or gesture represented by
   * this object. This should be a short string that can fit in
   * a single line.
   */
  properties[PROP_TITLE] =
    g_param_spec_string ("title",
                         P_("Title"),
                         P_("Title"),
                         "",
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GtkShortcutsShortcut:subtitle:
   *
   * The subtitle for the shortcut or gesture.
   *
   * This is typically used for gestures and should be a short, one-line
   * text that describes the gesture itself, e.g. "Two-finger swipe".
   */
  properties[PROP_SUBTITLE] =
    g_param_spec_string ("subtitle",
                         P_("Subtitle"),
                         P_("Subtitle"),
                         "",
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

  properties[PROP_DIRECTION] =
    g_param_spec_enum ("direction",
                       P_("Direction"),
                       P_("Text direction for which this shortcut is active"),
                       GTK_TYPE_TEXT_DIRECTION,
                       GTK_TEXT_DIR_NONE,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

  properties[PROP_SHORTCUT_TYPE] =
    g_param_spec_enum ("shortcut-type",
                       P_("Shortcut Type"),
                       P_("The type of shortcut that is represented"),
                       GTK_TYPE_SHORTCUT_TYPE,
                       GTK_SHORTCUT_ACCELERATOR,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
gtk_shortcuts_shortcut_init (GtkShortcutsShortcut *self)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_HORIZONTAL);
  gtk_box_set_spacing (GTK_BOX (self), 12);

  self->direction = GTK_TEXT_DIR_NONE;
  self->shortcut_type = GTK_SHORTCUT_ACCELERATOR;

  self->image = g_object_new (GTK_TYPE_IMAGE,
                              "visible", FALSE,
                              "valign", GTK_ALIGN_CENTER,
                              "no-show-all", TRUE,
                              NULL);
  GTK_CONTAINER_CLASS (gtk_shortcuts_shortcut_parent_class)->add (GTK_CONTAINER (self), GTK_WIDGET (self->image));

  self->accelerator = g_object_new (GTK_TYPE_SHORTCUT_LABEL,
                                    "visible", TRUE,
                                    "valign", GTK_ALIGN_CENTER,
                                    "no-show-all", TRUE,
                                    NULL);
  GTK_CONTAINER_CLASS (gtk_shortcuts_shortcut_parent_class)->add (GTK_CONTAINER (self), GTK_WIDGET (self->accelerator));

  self->title_box = g_object_new (GTK_TYPE_BOX,
                                  "visible", TRUE,
                                  "valign", GTK_ALIGN_CENTER,
                                  "hexpand", TRUE,
                                  "orientation", GTK_ORIENTATION_VERTICAL,
                                  NULL);
  GTK_CONTAINER_CLASS (gtk_shortcuts_shortcut_parent_class)->add (GTK_CONTAINER (self), GTK_WIDGET (self->title_box));

  self->title = g_object_new (GTK_TYPE_LABEL,
                              "visible", TRUE,
                              "xalign", 0.0f,
                              NULL);
  gtk_container_add (GTK_CONTAINER (self->title_box), GTK_WIDGET (self->title));

  self->subtitle = g_object_new (GTK_TYPE_LABEL,
                                 "visible", FALSE,
                                 "no-show-all", TRUE,
                                 "xalign", 0.0f,
                                 NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self->subtitle)),
                               GTK_STYLE_CLASS_DIM_LABEL);
  gtk_container_add (GTK_CONTAINER (self->title_box), GTK_WIDGET (self->subtitle));
}
