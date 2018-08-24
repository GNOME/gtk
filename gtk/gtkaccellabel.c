/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GtkAccelLabel: GtkLabel with accelerator monitoring facilities.
 * Copyright (C) 1998 Tim Janik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2001.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"
#include <string.h>

#include "gtklabel.h"
#include "gtkaccellabel.h"
#include "gtkaccellabelprivate.h"
#include "gtkintl.h"
#include "gtkmain.h"
#include "gtkprivate.h"
#include "gtksizerequest.h"
#include "gtkstylecontextprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkboxlayout.h"

/**
 * SECTION:gtkaccellabel
 * @Short_description: A label which displays an accelerator key on the right of the text
 * @Title: GtkAccelLabel
 * @See_also: #GtkAccelGroup
 *
 * The #GtkAccelLabel is a widget that shows an accelerator next to a description
 * of said accelerator, e.g. “Save Document Ctrl+S”.
 * It is commonly used in menus to show the keyboard short-cuts for commands.
 *
 * The accelerator key to display is typically not set explicitly (although it
 * can be, with gtk_accel_label_set_accel()). Instead, the #GtkAccelLabel displays
 * the accelerators which have been added to a particular widget. This widget is
 * set by calling gtk_accel_label_set_accel_widget().
 *
 * For example, a #GtkMenuItem widget may have an accelerator added to emit
 * the “activate” signal when the “Ctrl+S” key combination is pressed.
 * A #GtkAccelLabel is created and added to the #GtkMenuItem, and
 * gtk_accel_label_set_accel_widget() is called with the #GtkMenuItem as the
 * second argument. The #GtkAccelLabel will now display “Ctrl+S” after its label.
 *
 * Note that creating a #GtkMenuItem with gtk_menu_item_new_with_label() (or
 * one of the similar functions for #GtkCheckMenuItem and #GtkRadioMenuItem)
 * automatically adds a #GtkAccelLabel to the #GtkMenuItem and calls
 * gtk_accel_label_set_accel_widget() to set it up for you.
 *
 * A #GtkAccelLabel will only display accelerators which have %GTK_ACCEL_VISIBLE
 * set (see #GtkAccelFlags).
 * A #GtkAccelLabel can display multiple accelerators and even signal names,
 * though it is almost always used to display just one accelerator key.
 *
 * ## Creating a simple menu item with an accelerator key.
 *
 * |[<!-- language="C" -->
 *   GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
 *   GtkWidget *menu = gtk_menu_new ();
 *   GtkWidget *save_item;
 *
 *   // Create the menu item using the convenience function.
 *   save_item = gtk_menu_item_new_with_label ("Save");
 *   gtk_container_add (GTK_CONTAINER (menu), save_item);
 * ]|
 *
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * accellabel
 *   ├── label
 *   ╰── accelerator
 * ]|
 *
 * #GtkAccelLabel has a main CSS node with the name accellabel.
 * It contains the two child nodes with name label and accelerator.
 */

enum {
  PROP_0,
  PROP_LABEL,
  PROP_USE_UNDERLINE,
  LAST_PROP
};

struct _GtkAccelLabel
{
  GtkWidget parent_instance;
};

struct _GtkAccelLabelClass
{
  GtkWidgetClass parent_class;

  char *mod_name_shift;
  char *mod_name_control;
  char *mod_name_alt;
  char *mod_separator;
};

typedef struct _GtkAccelLabelPrivate GtkAccelLabelPrivate;
struct _GtkAccelLabelPrivate
{
  GtkWidget     *text_label;
  GtkWidget     *accel_label;

  guint           accel_key;         /* manual accel key specification if != 0 */
  GdkModifierType accel_mods;
};

GParamSpec *props[LAST_PROP] = { NULL, };

static void         gtk_accel_label_set_property (GObject            *object,
						  guint               prop_id,
						  const GValue       *value,
						  GParamSpec         *pspec);
static void         gtk_accel_label_get_property (GObject            *object,
						  guint               prop_id,
						  GValue             *value,
						  GParamSpec         *pspec);
static void         gtk_accel_label_finalize     (GObject            *object);

G_DEFINE_TYPE_WITH_PRIVATE (GtkAccelLabel, gtk_accel_label, GTK_TYPE_WIDGET)

static void
gtk_accel_label_class_init (GtkAccelLabelClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  gobject_class->finalize = gtk_accel_label_finalize;
  gobject_class->set_property = gtk_accel_label_set_property;
  gobject_class->get_property = gtk_accel_label_get_property;

  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_ACCEL_LABEL);

  props[PROP_LABEL] =
    g_param_spec_string ("label",
                         P_("Label"),
                         P_("The text displayed next to the accelerator"),
                         "",
                         GTK_PARAM_READWRITE);

  props[PROP_USE_UNDERLINE] =
    g_param_spec_boolean ("use-underline",
                          P_("Use underline"),
                          P_("If set, an underline in the text indicates the next character should be used for the mnemonic accelerator key"),
                          FALSE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, LAST_PROP, props);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, I_("accellabel"));
}

static void
gtk_accel_label_set_property (GObject      *object,
			      guint         prop_id,
			      const GValue *value,
			      GParamSpec   *pspec)
{
  GtkAccelLabel  *accel_label;

  accel_label = GTK_ACCEL_LABEL (object);

  switch (prop_id)
    {
    case PROP_LABEL:
      gtk_accel_label_set_label (accel_label, g_value_get_string (value));
      break;
    case PROP_USE_UNDERLINE:
      gtk_accel_label_set_use_underline (accel_label, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_accel_label_get_property (GObject    *object,
			      guint       prop_id,
			      GValue     *value,
			      GParamSpec *pspec)
{
  GtkAccelLabel  *accel_label = GTK_ACCEL_LABEL (object);

  switch (prop_id)
    {
    case PROP_LABEL:
      g_value_set_string (value, gtk_accel_label_get_label (accel_label));
      break;
    case PROP_USE_UNDERLINE:
      g_value_set_boolean (value, gtk_accel_label_get_use_underline (accel_label));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_accel_label_init (GtkAccelLabel *accel_label)
{
  GtkAccelLabelPrivate *priv = gtk_accel_label_get_instance_private (accel_label);

  priv->text_label = gtk_label_new ("");
  gtk_widget_set_hexpand (priv->text_label, TRUE);
  gtk_label_set_xalign (GTK_LABEL (priv->text_label), 0.0f);
  priv->accel_label = g_object_new (GTK_TYPE_LABEL,
                                    "css-name", "accelerator",
                                    NULL);
  gtk_widget_set_parent (priv->text_label, GTK_WIDGET (accel_label));
  gtk_widget_set_parent (priv->accel_label, GTK_WIDGET (accel_label));
}

/**
 * gtk_accel_label_new:
 * @string: the label string. Must be non-%NULL.
 *
 * Creates a new #GtkAccelLabel.
 *
 * Returns: a new #GtkAccelLabel.
 */
GtkWidget *
gtk_accel_label_new (const gchar *string)
{
  GtkAccelLabel *accel_label;

  g_return_val_if_fail (string != NULL, NULL);

  accel_label = g_object_new (GTK_TYPE_ACCEL_LABEL,
                              "label", string,
                              NULL);

  return GTK_WIDGET (accel_label);
}

static void
gtk_accel_label_finalize (GObject *object)
{
  GtkAccelLabel *accel_label = GTK_ACCEL_LABEL (object);
  GtkAccelLabelPrivate *priv = gtk_accel_label_get_instance_private (accel_label);

  gtk_widget_unparent (priv->accel_label);
  gtk_widget_unparent (priv->text_label);

  G_OBJECT_CLASS (gtk_accel_label_parent_class)->finalize (object);
}

/**
 * gtk_accel_label_get_accel_width:
 * @accel_label: a #GtkAccelLabel.
 *
 * Returns the width needed to display the accelerator key(s).
 * This is used by menus to align all of the #GtkMenuItem widgets, and shouldn't
 * be needed by applications.
 *
 * Returns: the width needed to display the accelerator key(s).
 */
guint
gtk_accel_label_get_accel_width (GtkAccelLabel *accel_label)
{
  GtkAccelLabelPrivate *priv = gtk_accel_label_get_instance_private (accel_label);
  int min;

  g_return_val_if_fail (GTK_IS_ACCEL_LABEL (accel_label), 0);

  gtk_widget_measure (priv->accel_label, GTK_ORIENTATION_HORIZONTAL, -1,
                      &min, NULL, NULL, NULL);

  return min;
}

static void
gtk_accel_label_reset (GtkAccelLabel *accel_label)
{
  gtk_accel_label_refetch (accel_label);
}

/**
 * gtk_accel_label_refetch:
 * @accel_label: a #GtkAccelLabel.
 *
 * Recreates the string representing the accelerator keys.
 * This should not be needed since the string is automatically updated whenever
 * accelerators are added or removed from the associated widget.
 *
 * Returns: always returns %FALSE.
 */
gboolean
gtk_accel_label_refetch (GtkAccelLabel *accel_label)
{
  GtkAccelLabelPrivate *priv = gtk_accel_label_get_instance_private (accel_label);
  gboolean enable_accels;
  char *accel_string = NULL;

  g_return_val_if_fail (GTK_IS_ACCEL_LABEL (accel_label), FALSE);

  g_object_get (gtk_widget_get_settings (GTK_WIDGET (accel_label)),
                "gtk-enable-accels", &enable_accels,
                NULL);

  if (enable_accels && priv->accel_key)
    {
      gboolean have_accel = FALSE;
      guint accel_key;
      GdkModifierType accel_mods;

      /* First check for a manual accel set with _set_accel() */
      if (priv->accel_key)
        {
          accel_mods = priv->accel_mods;
          accel_key = priv->accel_key;
          have_accel = TRUE;
        }

      /* If we found a key using either method, set it */
      if (have_accel)
        accel_string = gtk_accelerator_get_label (accel_key, accel_mods);
      else
        /* Otherwise we have a closure with no key.  Show "-/-". */
        accel_string = g_strdup ("-/-");
    }

  if (!accel_string)
    accel_string = g_strdup ("");

  gtk_label_set_label (GTK_LABEL (priv->accel_label), accel_string);

  g_free (accel_string);

  return FALSE;
}

/**
 * gtk_accel_label_set_accel:
 * @accel_label: a #GtkAccelLabel
 * @accelerator_key: a keyval, or 0
 * @accelerator_mods: the modifier mask for the accel
 *
 * Manually sets a keyval and modifier mask as the accelerator rendered
 * by @accel_label.
 *
 * If a keyval and modifier are explicitly set then these values are
 * used regardless of any associated accel closure or widget.
 *
 * Providing an @accelerator_key of 0 removes the manual setting.
 */
void
gtk_accel_label_set_accel (GtkAccelLabel   *accel_label,
                           guint            accelerator_key,
                           GdkModifierType  accelerator_mods)
{
  GtkAccelLabelPrivate *priv = gtk_accel_label_get_instance_private (accel_label);

  g_return_if_fail (GTK_IS_ACCEL_LABEL (accel_label));

  priv->accel_key = accelerator_key;
  priv->accel_mods = accelerator_mods;

  gtk_accel_label_reset (accel_label);
}

/**
 * gtk_accel_label_get_accel:
 * @accel_label: a #GtkAccelLabel
 * @accelerator_key: (out): return location for the keyval
 * @accelerator_mods: (out): return location for the modifier mask
 *
 * Gets the keyval and modifier mask set with
 * gtk_accel_label_set_accel().
 */
void
gtk_accel_label_get_accel (GtkAccelLabel   *accel_label,
                           guint           *accelerator_key,
                           GdkModifierType *accelerator_mods)
{
  GtkAccelLabelPrivate *priv = gtk_accel_label_get_instance_private (accel_label);

  g_return_if_fail (GTK_IS_ACCEL_LABEL (accel_label));

  *accelerator_key = priv->accel_key;
  *accelerator_mods = priv->accel_mods;
}

/**
 * gtk_accel_label_set_label:
 * @accel_label: a #GtkAccelLabel
 * @text: The new label text
 *
 * Sets the label part of the accel label.
 *
 */
void
gtk_accel_label_set_label (GtkAccelLabel *accel_label,
                           const char    *text)
{
  GtkAccelLabelPrivate *priv = gtk_accel_label_get_instance_private (accel_label);

  g_return_if_fail (GTK_IS_ACCEL_LABEL (accel_label));

  gtk_label_set_text_with_mnemonic (GTK_LABEL (priv->text_label), text);
}

/**
 * gtk_accel_label_get_label:
 * @accel_label: a #GtkAccelLabel
 *
 * Returns the current label, set via gtk_accel_label_set_label()
 *
 * Returns: (transfer none): @accel_label's label
 *
 */
const char *
gtk_accel_label_get_label (GtkAccelLabel *accel_label)
{
  GtkAccelLabelPrivate *priv = gtk_accel_label_get_instance_private (accel_label);

  g_return_val_if_fail (GTK_IS_ACCEL_LABEL (accel_label), NULL);

  return gtk_label_get_label (GTK_LABEL (priv->text_label));
}

/**
 * gtk_accel_label_set_use_underline:
 * @accel_label: a #GtkAccelLabel
 * @setting: Whether to use underlines in the label or not
 *
 * Controls whether to interpret underscores in the text label of @accel_label
 * as mnemonic indicators. See also gtk_label_set_use_underline()
 */
void
gtk_accel_label_set_use_underline (GtkAccelLabel *accel_label,
                                   gboolean       setting)
{
  GtkAccelLabelPrivate *priv = gtk_accel_label_get_instance_private (accel_label);

  g_return_if_fail (GTK_IS_ACCEL_LABEL (accel_label));

  setting = !!setting;

  if (setting != gtk_label_get_use_underline (GTK_LABEL (priv->text_label)))
    {
      gtk_label_set_use_underline (GTK_LABEL (priv->text_label), setting);

      g_object_notify_by_pspec (G_OBJECT (accel_label), props[PROP_USE_UNDERLINE]);
    }
}

/**
 * gtk_accel_label_get_use_underline:
 * @accel_label: a #GtkAccelLabel
 *
 * Returns whether the accel label interprets underscores in it's
 * label property as mnemonic indicators.
 * See gtk_accel_label_set_use_underline() and gtk_label_set_use_underline();
 *
 * Returns: whether the accel label uses mnemonic underlines
 */
gboolean
gtk_accel_label_get_use_underline (GtkAccelLabel *accel_label)
{
  GtkAccelLabelPrivate *priv = gtk_accel_label_get_instance_private (accel_label);

  g_return_val_if_fail (GTK_IS_ACCEL_LABEL (accel_label), FALSE);

  return gtk_label_get_use_underline (GTK_LABEL (priv->text_label));
}
