/*
 * GTK - The GIMP Toolkit
 * Copyright (C) 2022 Red Hat, Inc.
 * All rights reserved.
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkfontdialogbutton.h"

#include "gtkbinlayout.h"
#include "gtkbox.h"
#include "gtkseparator.h"
#include "gtkbutton.h"
#include "gtklabel.h"
#include <glib/gi18n-lib.h>
#include "gtkmain.h"
#include "gtkprivate.h"
#include "gtkwidgetprivate.h"


static void     button_clicked (GtkFontDialogButton *self);
static void     update_button_sensitivity
                               (GtkFontDialogButton *self);

/**
 * GtkFontDialogButton:
 *
 * The `GtkFontDialogButton` is wrapped around a [class@Gtk.FontDialog]
 * and allows to open a font chooser dialog to change the font.
 *
 * ![An example GtkFontDialogButton](font-button.png)
 *
 * It is suitable widget for selecting a font in a preference dialog.
 *
 * # CSS nodes
 *
 * ```
 * fontbutton
 * ╰── button.font
 *     ╰── [content]
 * ```
 *
 * `GtkFontDialogButton` has a single CSS node with name fontbutton which
 * contains a button node with the .font style class.
 */

/* {{{ GObject implementation */

struct _GtkFontDialogButton
{
  GtkWidget parent_instance;

  GtkWidget *button;
  GtkWidget *font_label;
  GtkWidget *size_label;
  GtkWidget *font_size_box;

  guint use_font : 1;
  guint use_size : 1;

  GtkFontDialog *dialog;
  GCancellable *cancellable;
  PangoFontDescription *font_desc;
  char *font_features;

  PangoFontFamily *font_family;
  PangoFontFace *font_face;
};

/* Properties */
enum
{
  PROP_DIALOG = 1,
  PROP_FONT_DESC,
  PROP_FONT_FEATURES,
  PROP_USE_FONT,
  PROP_USE_SIZE,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

G_DEFINE_TYPE (GtkFontDialogButton, gtk_font_dialog_button, GTK_TYPE_WIDGET)

static void
gtk_font_dialog_button_init (GtkFontDialogButton *self)
{
  GtkWidget *box;
  PangoFontDescription *font_desc;

  self->button = gtk_button_new ();
  g_signal_connect_swapped (self->button, "clicked", G_CALLBACK (button_clicked), self);
  self->font_label = gtk_label_new (_("Font"));
  gtk_widget_set_hexpand (self->font_label, TRUE);
  self->size_label = gtk_label_new ("14");
  self->font_size_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_append (GTK_BOX (box), self->font_label);

  gtk_box_append (GTK_BOX (self->font_size_box), gtk_separator_new (GTK_ORIENTATION_VERTICAL));
  gtk_box_append (GTK_BOX (self->font_size_box), self->size_label);
  gtk_box_append (GTK_BOX (box), self->font_size_box);

  gtk_button_set_child (GTK_BUTTON (self->button), box);
  gtk_widget_set_parent (self->button, GTK_WIDGET (self));

  self->use_font = FALSE;
  self->use_size = FALSE;

  font_desc = pango_font_description_from_string ("Sans 12");
  gtk_font_dialog_button_set_font_desc (self, font_desc);
  pango_font_description_free (font_desc);

  gtk_widget_add_css_class (self->button, "font");
}

static void
gtk_font_dialog_button_unroot (GtkWidget *widget)
{
  GtkFontDialogButton *self = GTK_FONT_DIALOG_BUTTON (widget);

  if (self->cancellable)
    {
      g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);
      update_button_sensitivity (self);
    }

  GTK_WIDGET_CLASS (gtk_font_dialog_button_parent_class)->unroot (widget);
}

static void
gtk_font_dialog_button_set_property (GObject      *object,
                                     unsigned int  param_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkFontDialogButton *self = GTK_FONT_DIALOG_BUTTON (object);

  switch (param_id)
    {
    case PROP_DIALOG:
      gtk_font_dialog_button_set_dialog (self, g_value_get_object (value));
      break;

    case PROP_FONT_DESC:
      gtk_font_dialog_button_set_font_desc (self, g_value_get_boxed (value));
      break;

    case PROP_FONT_FEATURES:
      gtk_font_dialog_button_set_font_features (self, g_value_get_string (value));
      break;

    case PROP_USE_FONT:
      gtk_font_dialog_button_set_use_font (self, g_value_get_boolean (value));
      break;

    case PROP_USE_SIZE:
      gtk_font_dialog_button_set_use_size (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
gtk_font_dialog_button_get_property (GObject      *object,
                                     unsigned int  param_id,
                                     GValue       *value,
                                     GParamSpec   *pspec)
{
  GtkFontDialogButton *self = GTK_FONT_DIALOG_BUTTON (object);

  switch (param_id)
    {
    case PROP_DIALOG:
      g_value_set_object (value, self->dialog);
      break;

    case PROP_FONT_DESC:
      g_value_set_boxed (value, self->font_desc);
      break;

    case PROP_FONT_FEATURES:
      g_value_set_string (value, self->font_features);
      break;

    case PROP_USE_FONT:
      g_value_set_boolean (value, self->use_font);
      break;

    case PROP_USE_SIZE:
      g_value_set_boolean (value, self->use_size);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
gtk_font_dialog_button_dispose (GObject *object)
{
  GtkFontDialogButton *self = GTK_FONT_DIALOG_BUTTON (object);

  g_clear_pointer (&self->button, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_font_dialog_button_parent_class)->dispose (object);
}

static void
gtk_font_dialog_button_finalize (GObject *object)
{
  GtkFontDialogButton *self = GTK_FONT_DIALOG_BUTTON (object);

  g_assert (self->cancellable == NULL);
  g_clear_object (&self->dialog);
  pango_font_description_free (self->font_desc);
  g_clear_object (&self->font_family);
  g_clear_object (&self->font_face);
  g_free (self->font_features);

  G_OBJECT_CLASS (gtk_font_dialog_button_parent_class)->finalize (object);
}

static void
gtk_font_dialog_button_class_init (GtkFontDialogButtonClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->get_property = gtk_font_dialog_button_get_property;
  object_class->set_property = gtk_font_dialog_button_set_property;
  object_class->dispose = gtk_font_dialog_button_dispose;
  object_class->finalize = gtk_font_dialog_button_finalize;

  widget_class->grab_focus = gtk_widget_grab_focus_child;
  widget_class->focus = gtk_widget_focus_child;
  widget_class->unroot = gtk_font_dialog_button_unroot;

  /**
   * GtkFontDialogButton:dialog: (attributes org.gtk.Property.get=gtk_font_dialog_button_get_dialog org.gtk.Property.set=gtk_font_dialog_button_set_dialog)
   *
   * The `GtkFontDialog` that contains parameters for
   * the font chooser dialog.
   *
   * Since: 4.10
   */
  properties[PROP_DIALOG] =
      g_param_spec_object ("dialog", NULL, NULL,
                           GTK_TYPE_FONT_DIALOG,
                           G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkFontDialogButton:font-desc: (attributes org.gtk.Property.get=gtk_font_dialog_button_get_font_desc org.gtk.Property.set=gtk_font_dialog_button_set_font_desc)
   *
   * The selected font.
   *
   * This property can be set to give the button its initial
   * font, and it will be updated to reflect the users choice
   * in the font chooser dialog.
   *
   * Listen to `notify::font-desc` to get informed about changes
   * to the buttons font.
   *
   * Since: 4.10
   */
  properties[PROP_FONT_DESC] =
      g_param_spec_boxed ("font-desc", NULL, NULL,
                          PANGO_TYPE_FONT_DESCRIPTION,
                          G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkFontDialogButton:font-features: (attributes org.gtk.Property.get=gtk_font_dialog_button_get_font_features org.gtk.Property.set=gtk_font_dialog_button_set_font_features)
   *
   * The selected font features.
   *
   * This property will be updated to reflect the users choice
   * in the font chooser dialog.
   *
   * Listen to `notify::font-features` to get informed about changes
   * to the buttons font features.
   *
   * Since: 4.10
   */
  properties[PROP_FONT_FEATURES] =
      g_param_spec_string ("font-features", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkFontDialogButton:use-font: (attributes org.gtk.Property.get=gtk_font_dialog_button_get_use_font org.gtk.Property.set=gtk_font_dialog_button_set_use_font)
   *
   * Whether the buttons label will be drawn in the selected font.
   */
  properties[PROP_USE_FONT] =
      g_param_spec_boolean ("use-font", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkFontDialogButton:use-size: (attributes org.gtk.Property.get=gtk_font_dialog_button_get_use_size org.gtk.Property.set=gtk_font_dialog_button_set_use_size)
   *
   * Whether the buttons label will use the selected font size.
   */
  properties[PROP_USE_SIZE] =
      g_param_spec_boolean ("use-size", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "fontbutton");
}

/* }}} */
/* {{{ Private API, callbacks */

static void
update_button_sensitivity (GtkFontDialogButton *self)
{
  gtk_widget_set_sensitive (self->button,
                            self->dialog != NULL && self->cancellable == NULL);
}

static void
font_chosen (GObject      *source,
             GAsyncResult *result,
             gpointer      data)
{
  GtkFontDialogButton *self = data;
  PangoFontDescription *desc;
  GError *error = NULL;

  desc = gtk_font_dialog_choose_font_finish (self->dialog, result, &error);
  if (desc)
    {
      gtk_font_dialog_button_set_font_desc (self, desc);
    }
  else
    {
      g_print ("%s\n", error->message);
      g_error_free (error);
    }

  g_clear_object (&self->cancellable);
  update_button_sensitivity (self);
}

static void
font_and_features_chosen (GObject      *source,
                          GAsyncResult *result,
                          gpointer      data)
{
  GtkFontDialogButton *self = data;
  PangoFontDescription *desc;
  char *features;
  GError *error = NULL;

  if (!gtk_font_dialog_choose_font_and_features_finish (self->dialog, result,
                                                        &desc, &features,
                                                        &error))
    {
      g_print ("%s\n", error->message);
      g_error_free (error);
      return;
    }

  gtk_font_dialog_button_set_font_desc (self, desc);
  gtk_font_dialog_button_set_font_features (self, features);
}

static void
button_clicked (GtkFontDialogButton *self)
{
  GtkRoot *root = gtk_widget_get_root (GTK_WIDGET (self));
  GtkWindow *parent = NULL;

  g_assert (self->cancellable == NULL);
  self->cancellable = g_cancellable_new ();

  update_button_sensitivity (self);

  if (GTK_IS_WINDOW (root))
    parent = GTK_WINDOW (root);

  if (gtk_font_dialog_get_level (self->dialog) & GTK_FONT_CHOOSER_LEVEL_FEATURES)
    gtk_font_dialog_choose_font_and_features (self->dialog, parent, self->font_desc,
                                              self->cancellable, font_and_features_chosen, self);
  else
    gtk_font_dialog_choose_font (self->dialog, parent, self->font_desc,
                                 self->cancellable, font_chosen, self);
}

static gboolean
font_description_style_equal (const PangoFontDescription *a,
                              const PangoFontDescription *b)
{
  return (pango_font_description_get_weight (a) == pango_font_description_get_weight (b) &&
          pango_font_description_get_style (a) == pango_font_description_get_style (b) &&
          pango_font_description_get_stretch (a) == pango_font_description_get_stretch (b) &&
          pango_font_description_get_variant (a) == pango_font_description_get_variant (b));
}

static void
update_font_data (GtkFontDialogButton *self)
{
  PangoFontMap *fontmap = NULL;
  const char *family_name;

  g_assert (self->font_desc != NULL);

  g_clear_object (&self->font_family);
  g_clear_object (&self->font_face);

  family_name = pango_font_description_get_family (self->font_desc);
  if (family_name == NULL)
    return;

  if (self->dialog)
    fontmap = gtk_font_dialog_get_fontmap (self->dialog);
  if (!fontmap)
    fontmap = pango_cairo_font_map_get_default ();

  for (unsigned int i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (fontmap)); i++)
    {
      PangoFontFamily *family = g_list_model_get_item (G_LIST_MODEL (fontmap), i);
      const char *name = pango_font_family_get_name (family);
      g_object_unref (family);

      if (g_ascii_strcasecmp (name, family_name) == 0)
        {
          g_set_object (&self->font_family, family);
          break;
        }
    }

  for (unsigned i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->font_family)); i++)
    {
      PangoFontFace *face = g_list_model_get_item (G_LIST_MODEL (self->font_family), i);
      PangoFontDescription *tmp_desc = pango_font_face_describe (face);
      g_object_unref (face);

      if (font_description_style_equal (tmp_desc, self->font_desc))
        {
          g_set_object (&self->font_face, face);
          pango_font_description_free (tmp_desc);
          break;
        }
      else
        pango_font_description_free (tmp_desc);
    }
}

static void
update_font_info (GtkFontDialogButton *self)
{
  const char *fam_name;
  const char *face_name;
  char *family_style;
  GtkFontChooserLevel level;

  if (self->font_family)
    fam_name = pango_font_family_get_name (self->font_family);
  else
    fam_name = C_("font", "None");
  if (self->font_face)
    face_name = pango_font_face_get_face_name (self->font_face);
  else
    face_name = "";

  if (self->dialog)
    level = gtk_font_dialog_get_level (self->dialog);
  else
    level = GTK_FONT_CHOOSER_LEVEL_STYLE|GTK_FONT_CHOOSER_LEVEL_SIZE;

  if ((level & GTK_FONT_CHOOSER_LEVEL_STYLE) != 0)
    family_style = g_strconcat (fam_name, " ", face_name, NULL);
  else
    family_style = g_strdup (fam_name);

  gtk_label_set_text (GTK_LABEL (self->font_label), family_style);
  g_free (family_style);

  if ((level & GTK_FONT_CHOOSER_LEVEL_SIZE) != 0)
    {
      /* mirror Pango, which doesn't translate this either */
      char *size = g_strdup_printf ("%2.4g%s",
                                    pango_font_description_get_size (self->font_desc) / (double)PANGO_SCALE,
                                    pango_font_description_get_size_is_absolute (self->font_desc) ? "px" : "");

      gtk_label_set_text (GTK_LABEL (self->size_label), size);

      g_free (size);

      gtk_widget_show (self->font_size_box);
    }
  else
    gtk_widget_hide (self->font_size_box);
}

static void
apply_use_font (GtkFontDialogButton *self)
{
  if (!self->use_font)
    gtk_label_set_attributes (GTK_LABEL (self->font_label), NULL);
  else
    {
      PangoFontDescription *desc;
      PangoAttrList *attrs;
      PangoLanguage *language = NULL;

      desc = pango_font_description_copy (self->font_desc);

      if (!self->use_size)
        pango_font_description_unset_fields (desc, PANGO_FONT_MASK_SIZE);

      attrs = pango_attr_list_new ();

      /* Prevent font fallback */
      pango_attr_list_insert (attrs, pango_attr_fallback_new (FALSE));

      /* Force current font and features */
      pango_attr_list_insert (attrs, pango_attr_font_desc_new (desc));
      if (self->font_features)
        pango_attr_list_insert (attrs, pango_attr_font_features_new (self->font_features));
      if (self->dialog)
        language = gtk_font_dialog_get_language (self->dialog);
      if (language)
        pango_attr_list_insert (attrs, pango_attr_language_new (language));

      gtk_label_set_attributes (GTK_LABEL (self->font_label), attrs);

      pango_attr_list_unref (attrs);
      pango_font_description_free (desc);
    }
}

/* }}} */
/* {{{ Public API */
 /* {{{ Constructor */

/**
 * gtk_font_dialog_button_new:
 * @dialog: (nullable) (transfer full): the `GtkFontDialog` to use
 *
 * Creates a new `GtkFontDialogButton` with the
 * given `GtkFontDialog`.
 *
 * You can pass `NULL` to this function and set a `GtkFontDialog`
 * later. The button will be insensitive until that happens.
 *
 * Returns: the new `GtkFontDialogButton`
 *
 * Since: 4.10
 */
GtkWidget *
gtk_font_dialog_button_new (GtkFontDialog *dialog)
{
  GtkWidget *self;

  g_return_val_if_fail (GTK_IS_FONT_DIALOG (dialog), NULL);

  self = g_object_new (GTK_TYPE_FONT_DIALOG_BUTTON,
                       "dialog", dialog,
                       NULL);

  g_clear_object (&dialog);

  return self;
}

/* }}} */
/* {{{ Setters and Getters */

/**
 * gtk_font_dialog_button_set_dialog:
 * @self: a `GtkFontDialogButton`
 * @dialog: the new `GtkFontDialog`
 *
 * Sets a `GtkFontDialog` object to use for
 * creating the font chooser dialog that is
 * presented when the user clicks the button.
 *
 * Since: 4.10
 */
void
gtk_font_dialog_button_set_dialog (GtkFontDialogButton *self,
                                   GtkFontDialog       *dialog)
{
  g_return_if_fail (GTK_IS_FONT_DIALOG_BUTTON (self));
  g_return_if_fail (dialog == NULL || GTK_IS_FONT_DIALOG (dialog));

  if (!g_set_object (&self->dialog, dialog))
    return;

  update_button_sensitivity (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DIALOG]);
}

/**
 * gtk_font_dialog_button_get_dialog:
 * @self: a `GtkFontDialogButton`
 *
 * Returns the `GtkFontDialog` of @self.
 *
 * Returns: (nullable) (transfer none): the `GtkFontDialog`
 *
 * Since: 4.10
 */
GtkFontDialog *
gtk_font_dialog_button_get_dialog (GtkFontDialogButton *self)
{
  g_return_val_if_fail (GTK_IS_FONT_DIALOG_BUTTON (self), NULL);

  return self->dialog;
}

/**
 * gtk_font_dialog_button_set_font_desc:
 * @self: a `GtkFontDialogButton`
 * @font_desc: the new font
 *
 * Sets the font of the button.
 *
 * Since: 4.10
 */
void
gtk_font_dialog_button_set_font_desc (GtkFontDialogButton *self,
                                      PangoFontDescription *font_desc)
{
  g_return_if_fail (GTK_IS_FONT_DIALOG_BUTTON (self));
  g_return_if_fail (font_desc != NULL);

  if (self->font_desc == font_desc ||
      (self->font_desc && font_desc &&
       pango_font_description_equal (self->font_desc, font_desc)))
    return;

  if (self->font_desc)
    pango_font_description_free (self->font_desc);

  self->font_desc = pango_font_description_copy (font_desc);

  update_font_data (self);
  update_font_info (self);
  apply_use_font (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FONT_DESC]);
}

/**
 * gtk_font_dialog_button_get_font_desc:
 * @self: a `GtkFontDialogButton`
 *
 * Returns the font of the button.
 *
 * This function is what should be used to obtain
 * the font that was choosen by the user. To get
 * informed about changes, listen to "notify::font-desc".
 *
 * Returns: (transfer none) (nullable): the font
 *
 * Since: 4.10
 */
PangoFontDescription *
gtk_font_dialog_button_get_font_desc (GtkFontDialogButton *self)
{
  g_return_val_if_fail (GTK_IS_FONT_DIALOG_BUTTON (self), NULL);

  return self->font_desc;
}

/**
 * gtk_font_dialog_button_set_font_features:
 * @self: a `GtkFontDialogButton`
 * @font_features: (nullable): the font features
 *
 * Sets the font features of the button.
 *
 * Since: 4.10
 */
void
gtk_font_dialog_button_set_font_features (GtkFontDialogButton *self,
                                          const char          *font_features)
{
  char *new_features;

  g_return_if_fail (GTK_IS_FONT_DIALOG_BUTTON (self));

  if (g_strcmp0 (self->font_features, font_features) == 0)
    return;

  new_features = g_strdup (font_features);
  g_free (self->font_features);
  self->font_features = new_features;

  apply_use_font (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FONT_FEATURES]);
}

/**
 * gtk_font_dialog_button_get_font_features:
 * @self: a `GtkFontDialogButton`
 *
 * Returns the font features of the button.
 *
 * This function is what should be used to obtain the font features
 * that were choosen by the user. To get informed about changes, listen
 * to "notify::font-features".
 *
 * Note that font features will only be available if the
 * [property@Gtk.FontDialog:level] property includes
 * `GTK_FONT_CHOOSER_LEVEL_FEATURES`.
 *
 * Returns: (transfer none) (nullable): the font features
 *
 * Since: 4.10
 */
const char *
gtk_font_dialog_button_get_font_features (GtkFontDialogButton *self)
{
  g_return_val_if_fail (GTK_IS_FONT_DIALOG_BUTTON (self), NULL);

  return self->font_features;
}

/**
 * gtk_font_dialog_button_set_use_font:
 * @self: a `GtkFontDialogButton`
 * @use_font: If `TRUE`, font name will be written using
 *   the chosen font
 *
 * If @use_font is `TRUE`, the font name will be written
 * using the selected font.
 *
 * Since: 4.10
 */
void
gtk_font_dialog_button_set_use_font (GtkFontDialogButton *self,
                                     gboolean             use_font)
{
  g_return_if_fail (GTK_IS_FONT_DIALOG_BUTTON (self));

  if (self->use_font == use_font)
    return;

  self->use_font = use_font;

  apply_use_font (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_USE_FONT]);
}

/**
 * gtk_font_dialog_button_get_use_font:
 * @self: a `GtkFontDialogButton`
 *
 * Returns whether the selected font is used in the label.
 *
 * Returns: whether the selected font is used in the label
 *
 * Since: 4.10
 */
gboolean
gtk_font_dialog_button_get_use_font (GtkFontDialogButton *self)
{
  g_return_val_if_fail (GTK_IS_FONT_DIALOG_BUTTON (self), FALSE);

  return self->use_font;
}

/**
 * gtk_font_dialog_button_set_use_size:
 * @self: a `GtkFontDialogButton`
 * @use_size: If `TRUE`, font name will be written using
 *   the chosen font size
 *
 * If @use_size is `TRUE`, the font name will be written
 * using the selected font size.
 *
 * Since: 4.10
 */
void
gtk_font_dialog_button_set_use_size (GtkFontDialogButton *self,
                                     gboolean             use_size)
{
  g_return_if_fail (GTK_IS_FONT_DIALOG_BUTTON (self));

  if (self->use_size == use_size)
    return;

  self->use_size = use_size;

  apply_use_font (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_USE_SIZE]);
}

/**
 * gtk_font_dialog_button_get_use_size:
 * @self: a `GtkFontDialogButton`
 *
 * Returns whether the selected font size is used in the label.
 *
 * Returns: whether the selected font size is used in the label
 *
 * Since: 4.10
 */
gboolean
gtk_font_dialog_button_get_use_size (GtkFontDialogButton *self)
{
  g_return_val_if_fail (GTK_IS_FONT_DIALOG_BUTTON (self), FALSE);

  return self->use_size;
}

/* }}} */
/* }}} */

/* vim:set foldmethod=marker expandtab: */
