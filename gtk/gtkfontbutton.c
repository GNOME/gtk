/* 
 * GTK - The GIMP Toolkit
 * Copyright (C) 1998 David Abilleira Freijeiro <odaf@nexo.es>
 * All rights reserved.
 *
 * Based on gnome-color-picker by Federico Mena <federico@nuclecu.unam.mx>
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
 * Modified by the GTK+ Team and others 2003.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include "gtkfontbutton.h"

#include "gtkmain.h"
#include "gtkbox.h"
#include "gtklabel.h"
#include "gtkfontchooser.h"
#include "gtkfontchooserdialog.h"
#include "gtkimage.h"
#include "gtkmarshalers.h"
#include "gtkseparator.h"
#include "gtkprivate.h"
#include "gtkintl.h"

#include <string.h>
#include <stdio.h>
#include "gtkfontchooserutils.h"


/**
 * SECTION:gtkfontbutton
 * @Short_description: A button to launch a font chooser dialog
 * @Title: GtkFontButton
 * @See_also: #GtkFontChooserDialog, #GtkColorButton.
 *
 * The #GtkFontButton is a button which displays the currently selected
 * font an allows to open a font chooser dialog to change the font.
 * It is suitable widget for selecting a font in a preference dialog.
 */


struct _GtkFontButtonPrivate 
{
  gchar         *title;

  gchar         *fontname;
  
  guint         use_font : 1;
  guint         use_size : 1;
  guint         show_style : 1;
  guint         show_size : 1;
  guint         show_preview_entry : 1;
   
  GtkWidget     *font_dialog;
  GtkWidget     *inside;
  GtkWidget     *font_label;
  GtkWidget     *size_label;

  PangoFontDescription *font_desc;
  PangoFontFamily      *font_family;
  PangoFontFace        *font_face;
  gint                  font_size;
  gchar                *preview_text;
  GtkFontFilterFunc     font_filter;
  gpointer              font_filter_data;
  GDestroyNotify        font_filter_data_destroy;
};

/* Signals */
enum 
{
  FONT_SET,
  LAST_SIGNAL
};

enum 
{
  PROP_0,
  PROP_TITLE,
  PROP_FONT_NAME,
  PROP_USE_FONT,
  PROP_USE_SIZE,
  PROP_SHOW_STYLE,
  PROP_SHOW_SIZE
};

/* Prototypes */
static void gtk_font_button_finalize               (GObject            *object);
static void gtk_font_button_get_property           (GObject            *object,
                                                    guint               param_id,
                                                    GValue             *value,
                                                    GParamSpec         *pspec);
static void gtk_font_button_set_property           (GObject            *object,
                                                    guint               param_id,
                                                    const GValue       *value,
                                                    GParamSpec         *pspec);

static void gtk_font_button_clicked                 (GtkButton         *button);

/* Dialog response functions */
static void response_cb                             (GtkDialog         *dialog,
                                                     gint               response_id,
                                                     gpointer           data);
static void dialog_destroy                          (GtkWidget         *widget,
                                                     gpointer           data);

/* Auxiliary functions */
static GtkWidget *gtk_font_button_create_inside     (GtkFontButton     *gfs);
static void gtk_font_button_label_use_font          (GtkFontButton     *gfs);
static void gtk_font_button_update_font_info        (GtkFontButton     *gfs);

static guint font_button_signals[LAST_SIGNAL] = { 0 };

static void
clear_font_data (GtkFontButton *font_button)
{
  GtkFontButtonPrivate *priv = font_button->priv;

  if (priv->font_family)
    g_object_unref (priv->font_family);
  priv->font_family = NULL;

  if (priv->font_face)
    g_object_unref (priv->font_face);
  priv->font_face = NULL;

  if (priv->font_desc)
    pango_font_description_free (priv->font_desc);
  priv->font_desc = NULL;

  g_free (priv->fontname);
  priv->fontname = NULL;
}

static void
clear_font_filter_data (GtkFontButton *font_button)
{
  GtkFontButtonPrivate *priv = font_button->priv;

  if (priv->font_filter_data_destroy)
    priv->font_filter_data_destroy (priv->font_filter_data);
  priv->font_filter = NULL;
  priv->font_filter_data = NULL;
  priv->font_filter_data_destroy = NULL;
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
gtk_font_button_update_font_data (GtkFontButton *font_button)
{
  GtkFontButtonPrivate *priv = font_button->priv;
  PangoFontFamily **families;
  PangoFontFace **faces;
  gint n_families, n_faces, i;
  const gchar *family;

  g_assert (priv->font_desc != NULL);

  priv->fontname = pango_font_description_to_string (priv->font_desc);

  family = pango_font_description_get_family (priv->font_desc);
  if (family == NULL)
    return;

  n_families = 0;
  families = NULL;
  pango_context_list_families (gtk_widget_get_pango_context (GTK_WIDGET (font_button)),
                               &families, &n_families);
  n_faces = 0;
  faces = NULL;
  for (i = 0; i < n_families; i++)
    {
      const gchar *name = pango_font_family_get_name (families[i]);

      if (!g_ascii_strcasecmp (name, family))
        {
          priv->font_family = g_object_ref (families[i]);

          pango_font_family_list_faces (families[i], &faces, &n_faces);
          break;
        }
    }
  g_free (families);

  for (i = 0; i < n_faces; i++)
    {
      PangoFontDescription *tmp_desc = pango_font_face_describe (faces[i]);

      if (font_description_style_equal (tmp_desc, priv->font_desc))
        {
          priv->font_face = g_object_ref (faces[i]);

          pango_font_description_free (tmp_desc);
          break;
        }
      else
        pango_font_description_free (tmp_desc);
    }

  g_free (faces);
}

static gchar *
gtk_font_button_get_preview_text (GtkFontButton *font_button)
{
  GtkFontButtonPrivate *priv = font_button->priv;

  if (priv->font_dialog)
    return gtk_font_chooser_get_preview_text (GTK_FONT_CHOOSER (priv->font_dialog));

  return g_strdup (priv->preview_text);
}

static void
gtk_font_button_set_preview_text (GtkFontButton *font_button,
                                  const gchar   *preview_text)
{
  GtkFontButtonPrivate *priv = font_button->priv;

  if (priv->font_dialog)
    {
      gtk_font_chooser_set_preview_text (GTK_FONT_CHOOSER (priv->font_dialog),
                                         preview_text);
      return;
    }

  g_free (priv->preview_text);
  priv->preview_text = g_strdup (preview_text);
}


static gboolean
gtk_font_button_get_show_preview_entry (GtkFontButton *font_button)
{
  GtkFontButtonPrivate *priv = font_button->priv;

  if (priv->font_dialog)
    return gtk_font_chooser_get_show_preview_entry (GTK_FONT_CHOOSER (priv->font_dialog));

  return priv->show_preview_entry;
}

static void
gtk_font_button_set_show_preview_entry (GtkFontButton *font_button,
                                        gboolean       show)
{
  GtkFontButtonPrivate *priv = font_button->priv;

  if (priv->font_dialog)
    gtk_font_chooser_set_show_preview_entry (GTK_FONT_CHOOSER (priv->font_dialog), show);
  else
    priv->show_preview_entry = show != FALSE;
}

static PangoFontFamily *
gtk_font_button_font_chooser_get_font_family (GtkFontChooser *chooser)
{
  GtkFontButton *font_button = GTK_FONT_BUTTON (chooser);
  GtkFontButtonPrivate *priv = font_button->priv;

  return priv->font_family;
}

static PangoFontFace *
gtk_font_button_font_chooser_get_font_face (GtkFontChooser *chooser)
{
  GtkFontButton *font_button = GTK_FONT_BUTTON (chooser);
  GtkFontButtonPrivate *priv = font_button->priv;

  return priv->font_face;
}

static int
gtk_font_button_font_chooser_get_font_size (GtkFontChooser *chooser)
{
  GtkFontButton *font_button = GTK_FONT_BUTTON (chooser);
  GtkFontButtonPrivate *priv = font_button->priv;

  return priv->font_size;
}

static void
gtk_font_button_font_chooser_set_filter_func (GtkFontChooser    *chooser,
                                              GtkFontFilterFunc  filter_func,
                                              gpointer           filter_data,
                                              GDestroyNotify     data_destroy)
{
  GtkFontButton *font_button = GTK_FONT_BUTTON (chooser);
  GtkFontButtonPrivate *priv = font_button->priv;

  if (priv->font_dialog)
    {
      gtk_font_chooser_set_filter_func (GTK_FONT_CHOOSER (priv->font_dialog),
                                        filter_func,
                                        filter_data,
                                        data_destroy);
      return;
    }

  clear_font_filter_data (font_button);
  priv->font_filter = filter_func;
  priv->font_filter_data = filter_data;
  priv->font_filter_data_destroy = data_destroy;
}

static void
gtk_font_button_take_font_desc (GtkFontButton        *font_button,
                                PangoFontDescription *font_desc)
{
  GtkFontButtonPrivate *priv = font_button->priv;
  GObject *object = G_OBJECT (font_button);

  if (priv->font_desc && font_desc &&
      pango_font_description_equal (priv->font_desc, font_desc))
    {
      pango_font_description_free (font_desc);
      return;
    }

  g_object_freeze_notify (object);

  clear_font_data (font_button);

  if (font_desc)
    priv->font_desc = font_desc; /* adopted */
  else
    priv->font_desc = pango_font_description_from_string (_("Sans 12"));

  if (pango_font_description_get_size_is_absolute (priv->font_desc))
    priv->font_size = pango_font_description_get_size (priv->font_desc);
  else 
    priv->font_size = pango_font_description_get_size (priv->font_desc) / PANGO_SCALE;

  gtk_font_button_update_font_data (font_button);
  gtk_font_button_update_font_info (font_button);

  if (priv->font_dialog)
    gtk_font_chooser_set_font_desc (GTK_FONT_CHOOSER (priv->font_dialog),
                                    priv->font_desc);

  g_object_notify (G_OBJECT (font_button), "font");
  g_object_notify (G_OBJECT (font_button), "font-desc");
  g_object_notify (G_OBJECT (font_button), "font-name");

  g_object_thaw_notify (object);
}

static const PangoFontDescription *
gtk_font_button_get_font_desc (GtkFontButton *font_button)
{
  return font_button->priv->font_desc;
}

static void
gtk_font_button_font_chooser_notify (GObject    *object,
                                     GParamSpec *pspec,
                                     gpointer    user_data)
{
  /* We do not forward the notification of the "font" property to the dialog! */
  if (pspec->name == I_("preview-text") ||
      pspec->name == I_("show-preview-entry"))
    g_object_notify_by_pspec (user_data, pspec);
}

static void
gtk_font_button_font_chooser_iface_init (GtkFontChooserIface *iface)
{
  iface->get_font_family = gtk_font_button_font_chooser_get_font_family;
  iface->get_font_face = gtk_font_button_font_chooser_get_font_face;
  iface->get_font_size = gtk_font_button_font_chooser_get_font_size;
  iface->set_filter_func = gtk_font_button_font_chooser_set_filter_func;
}

G_DEFINE_TYPE_WITH_CODE (GtkFontButton, gtk_font_button, GTK_TYPE_BUTTON,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_FONT_CHOOSER,
                                                gtk_font_button_font_chooser_iface_init))

static void
gtk_font_button_class_init (GtkFontButtonClass *klass)
{
  GObjectClass *gobject_class;
  GtkButtonClass *button_class;
  
  gobject_class = (GObjectClass *) klass;
  button_class = (GtkButtonClass *) klass;

  gobject_class->finalize = gtk_font_button_finalize;
  gobject_class->set_property = gtk_font_button_set_property;
  gobject_class->get_property = gtk_font_button_get_property;
  
  button_class->clicked = gtk_font_button_clicked;
  
  klass->font_set = NULL;

  _gtk_font_chooser_install_properties (gobject_class);

  /**
   * GtkFontButton:title:
   * 
   * The title of the font chooser dialog.
   *
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_TITLE,
                                   g_param_spec_string ("title",
                                                        P_("Title"),
                                                        P_("The title of the font chooser dialog"),
                                                        _("Pick a Font"),
                                                        (GTK_PARAM_READABLE |
                                                         GTK_PARAM_WRITABLE)));

  /**
   * GtkFontButton:font-name:
   * 
   * The name of the currently selected font.
   *
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_FONT_NAME,
                                   g_param_spec_string ("font-name",
                                                        P_("Font name"),
                                                        P_("The name of the selected font"),
                                                        P_("Sans 12"),
                                                        (GTK_PARAM_READABLE |
                                                         GTK_PARAM_WRITABLE)));

  /**
   * GtkFontButton:use-font:
   * 
   * If this property is set to %TRUE, the label will be drawn 
   * in the selected font.
   *
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_USE_FONT,
                                   g_param_spec_boolean ("use-font",
                                                         P_("Use font in label"),
                                                         P_("Whether the label is drawn in the selected font"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkFontButton:use-size:
   * 
   * If this property is set to %TRUE, the label will be drawn 
   * with the selected font size.
   *
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_USE_SIZE,
                                   g_param_spec_boolean ("use-size",
                                                         P_("Use size in label"),
                                                         P_("Whether the label is drawn with the selected font size"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkFontButton:show-style:
   * 
   * If this property is set to %TRUE, the name of the selected font style 
   * will be shown in the label. For a more WYSIWYG way to show the selected 
   * style, see the ::use-font property. 
   *
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_STYLE,
                                   g_param_spec_boolean ("show-style",
                                                         P_("Show style"),
                                                         P_("Whether the selected font style is shown in the label"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE));
  /**
   * GtkFontButton:show-size:
   * 
   * If this property is set to %TRUE, the selected font size will be shown 
   * in the label. For a more WYSIWYG way to show the selected size, see the 
   * ::use-size property. 
   *
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_SIZE,
                                   g_param_spec_boolean ("show-size",
                                                         P_("Show size"),
                                                         P_("Whether selected font size is shown in the label"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkFontButton::font-set:
   * @widget: the object which received the signal.
   * 
   * The ::font-set signal is emitted when the user selects a font. 
   * When handling this signal, use gtk_font_button_get_font_name() 
   * to find out which font was just selected.
   *
   * Note that this signal is only emitted when the <emphasis>user</emphasis>
   * changes the font. If you need to react to programmatic font changes
   * as well, use the notify::font-name signal.
   *
   * Since: 2.4
   */
  font_button_signals[FONT_SET] = g_signal_new (I_("font-set"),
                                                G_TYPE_FROM_CLASS (gobject_class),
                                                G_SIGNAL_RUN_FIRST,
                                                G_STRUCT_OFFSET (GtkFontButtonClass, font_set),
                                                NULL, NULL,
                                                g_cclosure_marshal_VOID__VOID,
                                                G_TYPE_NONE, 0);
  
  g_type_class_add_private (gobject_class, sizeof (GtkFontButtonPrivate));
}

static void
gtk_font_button_init (GtkFontButton *font_button)
{
  font_button->priv = G_TYPE_INSTANCE_GET_PRIVATE (font_button,
                                                   GTK_TYPE_FONT_BUTTON,
                                                   GtkFontButtonPrivate);

  /* Initialize fields */
  font_button->priv->use_font = FALSE;
  font_button->priv->use_size = FALSE;
  font_button->priv->show_style = TRUE;
  font_button->priv->show_size = TRUE;
  font_button->priv->show_preview_entry = TRUE;
  font_button->priv->font_dialog = NULL;
  font_button->priv->font_family = NULL;
  font_button->priv->font_face = NULL;
  font_button->priv->font_size = -1;
  font_button->priv->title = g_strdup (_("Pick a Font"));

  font_button->priv->inside = gtk_font_button_create_inside (font_button);
  gtk_container_add (GTK_CONTAINER (font_button), font_button->priv->inside);

  gtk_font_button_take_font_desc (font_button, NULL);
}

static void
gtk_font_button_finalize (GObject *object)
{
  GtkFontButton *font_button = GTK_FONT_BUTTON (object);

  if (font_button->priv->font_dialog != NULL) 
    gtk_widget_destroy (font_button->priv->font_dialog);
  font_button->priv->font_dialog = NULL;

  g_free (font_button->priv->title);
  font_button->priv->title = NULL;

  clear_font_data (font_button);
  clear_font_filter_data (font_button);

  g_free (font_button->priv->preview_text);
  font_button->priv->preview_text = NULL;

  G_OBJECT_CLASS (gtk_font_button_parent_class)->finalize (object);
}

static void
gtk_font_button_set_property (GObject      *object,
                              guint         param_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkFontButton *font_button = GTK_FONT_BUTTON (object);

  switch (param_id) 
    {
    case GTK_FONT_CHOOSER_PROP_PREVIEW_TEXT:
      gtk_font_button_set_preview_text (font_button, g_value_get_string (value));
      break;
    case GTK_FONT_CHOOSER_PROP_SHOW_PREVIEW_ENTRY:
      gtk_font_button_set_show_preview_entry (font_button, g_value_get_boolean (value));
      break;
    case PROP_TITLE:
      gtk_font_button_set_title (font_button, g_value_get_string (value));
      break;
    case GTK_FONT_CHOOSER_PROP_FONT_DESC:
      gtk_font_button_take_font_desc (font_button, g_value_dup_boxed (value));
      break;
    case GTK_FONT_CHOOSER_PROP_FONT:
    case PROP_FONT_NAME:
      gtk_font_button_set_font_name (font_button, g_value_get_string (value));
      break;
    case PROP_USE_FONT:
      gtk_font_button_set_use_font (font_button, g_value_get_boolean (value));
      break;
    case PROP_USE_SIZE:
      gtk_font_button_set_use_size (font_button, g_value_get_boolean (value));
      break;
    case PROP_SHOW_STYLE:
      gtk_font_button_set_show_style (font_button, g_value_get_boolean (value));
      break;
    case PROP_SHOW_SIZE:
      gtk_font_button_set_show_size (font_button, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
  }
}

static void
gtk_font_button_get_property (GObject    *object,
                              guint       param_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkFontButton *font_button = GTK_FONT_BUTTON (object);
  
  switch (param_id) 
    {
    case GTK_FONT_CHOOSER_PROP_PREVIEW_TEXT:
      g_value_set_string (value, gtk_font_button_get_preview_text (font_button));
      break;
    case GTK_FONT_CHOOSER_PROP_SHOW_PREVIEW_ENTRY:
      g_value_set_boolean (value, gtk_font_button_get_show_preview_entry (font_button));
      break;
    case PROP_TITLE:
      g_value_set_string (value, gtk_font_button_get_title (font_button));
      break;
    case GTK_FONT_CHOOSER_PROP_FONT_DESC:
      g_value_set_boxed (value, gtk_font_button_get_font_desc (font_button));
      break;
    case GTK_FONT_CHOOSER_PROP_FONT:
    case PROP_FONT_NAME:
      g_value_set_string (value, gtk_font_button_get_font_name (font_button));
      break;
    case PROP_USE_FONT:
      g_value_set_boolean (value, gtk_font_button_get_use_font (font_button));
      break;
    case PROP_USE_SIZE:
      g_value_set_boolean (value, gtk_font_button_get_use_size (font_button));
      break;
    case PROP_SHOW_STYLE:
      g_value_set_boolean (value, gtk_font_button_get_show_style (font_button));
      break;
    case PROP_SHOW_SIZE:
      g_value_set_boolean (value, gtk_font_button_get_show_size (font_button));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
} 


/**
 * gtk_font_button_new:
 *
 * Creates a new font picker widget.
 *
 * Returns: a new font picker widget.
 *
 * Since: 2.4
 */
GtkWidget *
gtk_font_button_new (void)
{
  return g_object_new (GTK_TYPE_FONT_BUTTON, NULL);
} 

/**
 * gtk_font_button_new_with_font:
 * @fontname: Name of font to display in font chooser dialog
 *
 * Creates a new font picker widget.
 *
 * Returns: a new font picker widget.
 *
 * Since: 2.4
 */
GtkWidget *
gtk_font_button_new_with_font (const gchar *fontname)
{
  return g_object_new (GTK_TYPE_FONT_BUTTON, "font-name", fontname, NULL);
} 

/**
 * gtk_font_button_set_title:
 * @font_button: a #GtkFontButton
 * @title: a string containing the font chooser dialog title
 *
 * Sets the title for the font chooser dialog.  
 *
 * Since: 2.4
 */
void
gtk_font_button_set_title (GtkFontButton *font_button, 
                           const gchar   *title)
{
  gchar *old_title;
  g_return_if_fail (GTK_IS_FONT_BUTTON (font_button));
  
  old_title = font_button->priv->title;
  font_button->priv->title = g_strdup (title);
  g_free (old_title);
  
  if (font_button->priv->font_dialog)
    gtk_window_set_title (GTK_WINDOW (font_button->priv->font_dialog),
                          font_button->priv->title);

  g_object_notify (G_OBJECT (font_button), "title");
} 

/**
 * gtk_font_button_get_title:
 * @font_button: a #GtkFontButton
 *
 * Retrieves the title of the font chooser dialog.
 *
 * Returns: an internal copy of the title string which must not be freed.
 *
 * Since: 2.4
 */
const gchar*
gtk_font_button_get_title (GtkFontButton *font_button)
{
  g_return_val_if_fail (GTK_IS_FONT_BUTTON (font_button), NULL);

  return font_button->priv->title;
} 

/**
 * gtk_font_button_get_use_font:
 * @font_button: a #GtkFontButton
 *
 * Returns whether the selected font is used in the label.
 *
 * Returns: whether the selected font is used in the label.
 *
 * Since: 2.4
 */
gboolean
gtk_font_button_get_use_font (GtkFontButton *font_button)
{
  g_return_val_if_fail (GTK_IS_FONT_BUTTON (font_button), FALSE);

  return font_button->priv->use_font;
} 

/**
 * gtk_font_button_set_use_font:
 * @font_button: a #GtkFontButton
 * @use_font: If %TRUE, font name will be written using font chosen.
 *
 * If @use_font is %TRUE, the font name will be written using the selected font.  
 *
 * Since: 2.4
 */
void  
gtk_font_button_set_use_font (GtkFontButton *font_button,
			      gboolean       use_font)
{
  g_return_if_fail (GTK_IS_FONT_BUTTON (font_button));
  
  use_font = (use_font != FALSE);
  
  if (font_button->priv->use_font != use_font) 
    {
      font_button->priv->use_font = use_font;

      gtk_font_button_label_use_font (font_button);
 
      g_object_notify (G_OBJECT (font_button), "use-font");
    }
} 


/**
 * gtk_font_button_get_use_size:
 * @font_button: a #GtkFontButton
 *
 * Returns whether the selected size is used in the label.
 *
 * Returns: whether the selected size is used in the label.
 *
 * Since: 2.4
 */
gboolean
gtk_font_button_get_use_size (GtkFontButton *font_button)
{
  g_return_val_if_fail (GTK_IS_FONT_BUTTON (font_button), FALSE);

  return font_button->priv->use_size;
} 

/**
 * gtk_font_button_set_use_size:
 * @font_button: a #GtkFontButton
 * @use_size: If %TRUE, font name will be written using the selected size.
 *
 * If @use_size is %TRUE, the font name will be written using the selected size.
 *
 * Since: 2.4
 */
void  
gtk_font_button_set_use_size (GtkFontButton *font_button,
                              gboolean       use_size)
{
  g_return_if_fail (GTK_IS_FONT_BUTTON (font_button));
  
  use_size = (use_size != FALSE);
  if (font_button->priv->use_size != use_size) 
    {
      font_button->priv->use_size = use_size;

      gtk_font_button_label_use_font (font_button);

      g_object_notify (G_OBJECT (font_button), "use-size");
    }
} 

/**
 * gtk_font_button_get_show_style:
 * @font_button: a #GtkFontButton
 * 
 * Returns whether the name of the font style will be shown in the label.
 * 
 * Return value: whether the font style will be shown in the label.
 *
 * Since: 2.4
 **/
gboolean 
gtk_font_button_get_show_style (GtkFontButton *font_button)
{
  g_return_val_if_fail (GTK_IS_FONT_BUTTON (font_button), FALSE);

  return font_button->priv->show_style;
}

/**
 * gtk_font_button_set_show_style:
 * @font_button: a #GtkFontButton
 * @show_style: %TRUE if font style should be displayed in label.
 *
 * If @show_style is %TRUE, the font style will be displayed along with name of the selected font.
 *
 * Since: 2.4
 */
void
gtk_font_button_set_show_style (GtkFontButton *font_button,
                                gboolean       show_style)
{
  g_return_if_fail (GTK_IS_FONT_BUTTON (font_button));
  
  show_style = (show_style != FALSE);
  if (font_button->priv->show_style != show_style) 
    {
      font_button->priv->show_style = show_style;
      
      gtk_font_button_update_font_info (font_button);
  
      g_object_notify (G_OBJECT (font_button), "show-style");
    }
} 


/**
 * gtk_font_button_get_show_size:
 * @font_button: a #GtkFontButton
 * 
 * Returns whether the font size will be shown in the label.
 * 
 * Return value: whether the font size will be shown in the label.
 *
 * Since: 2.4
 **/
gboolean 
gtk_font_button_get_show_size (GtkFontButton *font_button)
{
  g_return_val_if_fail (GTK_IS_FONT_BUTTON (font_button), FALSE);

  return font_button->priv->show_size;
}

/**
 * gtk_font_button_set_show_size:
 * @font_button: a #GtkFontButton
 * @show_size: %TRUE if font size should be displayed in dialog.
 *
 * If @show_size is %TRUE, the font size will be displayed along with the name of the selected font.
 *
 * Since: 2.4
 */
void
gtk_font_button_set_show_size (GtkFontButton *font_button,
                               gboolean       show_size)
{
  g_return_if_fail (GTK_IS_FONT_BUTTON (font_button));
  
  show_size = (show_size != FALSE);

  if (font_button->priv->show_size != show_size) 
    {
      font_button->priv->show_size = show_size;

      gtk_container_remove (GTK_CONTAINER (font_button), font_button->priv->inside);
      font_button->priv->inside = gtk_font_button_create_inside (font_button);
      gtk_container_add (GTK_CONTAINER (font_button), font_button->priv->inside);
      
      gtk_font_button_update_font_info (font_button);

      g_object_notify (G_OBJECT (font_button), "show-size");
    }
} 


/**
 * gtk_font_button_get_font_name:
 * @font_button: a #GtkFontButton
 *
 * Retrieves the name of the currently selected font. This name includes
 * style and size information as well. If you want to render something
 * with the font, use this string with pango_font_description_from_string() .
 * If you're interested in peeking certain values (family name,
 * style, size, weight) just query these properties from the
 * #PangoFontDescription object.
 *
 * Returns: an internal copy of the font name which must not be freed.
 *
 * Since: 2.4
 */
const gchar *
gtk_font_button_get_font_name (GtkFontButton *font_button)
{
  g_return_val_if_fail (GTK_IS_FONT_BUTTON (font_button), NULL);

  return font_button->priv->fontname;
}

/**
 * gtk_font_button_set_font_name:
 * @font_button: a #GtkFontButton
 * @fontname: Name of font to display in font chooser dialog
 *
 * Sets or updates the currently-displayed font in font picker dialog.
 *
 * Returns: %TRUE
 *
 * Since: 2.4
 */
gboolean 
gtk_font_button_set_font_name (GtkFontButton *font_button,
                               const gchar    *fontname)
{
  PangoFontDescription *font_desc;

  g_return_val_if_fail (GTK_IS_FONT_BUTTON (font_button), FALSE);
  g_return_val_if_fail (fontname != NULL, FALSE);

  font_desc = pango_font_description_from_string (fontname);
  gtk_font_button_take_font_desc (font_button, font_desc);

  return TRUE;
}

static void
gtk_font_button_clicked (GtkButton *button)
{
  GtkFontChooser *font_dialog;
  GtkFontButton  *font_button = GTK_FONT_BUTTON (button);
  GtkFontButtonPrivate *priv = font_button->priv;
  
  if (!font_button->priv->font_dialog) 
    {
      GtkWidget *parent;
      
      parent = gtk_widget_get_toplevel (GTK_WIDGET (font_button));

      priv->font_dialog = gtk_font_chooser_dialog_new (priv->title, NULL);
      font_dialog = GTK_FONT_CHOOSER (font_button->priv->font_dialog);

      gtk_font_chooser_set_show_preview_entry (font_dialog, priv->show_preview_entry);

      if (priv->preview_text)
        {
          gtk_font_chooser_set_preview_text (font_dialog, priv->preview_text);
          g_free (priv->preview_text);
          priv->preview_text = NULL;
        }

      if (priv->font_filter)
        {
          gtk_font_chooser_set_filter_func (font_dialog,
                                            priv->font_filter,
                                            priv->font_filter_data,
                                            priv->font_filter_data_destroy);
          priv->font_filter = NULL;
          priv->font_filter_data = NULL;
          priv->font_filter_data_destroy = NULL;
        }

      if (gtk_widget_is_toplevel (parent) && GTK_IS_WINDOW (parent))
        {
          if (GTK_WINDOW (parent) != gtk_window_get_transient_for (GTK_WINDOW (font_dialog)))
            gtk_window_set_transient_for (GTK_WINDOW (font_dialog), GTK_WINDOW (parent));

          gtk_window_set_modal (GTK_WINDOW (font_dialog),
                                gtk_window_get_modal (GTK_WINDOW (parent)));
        }

      g_signal_connect (font_dialog, "notify",
                        G_CALLBACK (gtk_font_button_font_chooser_notify), button);

      g_signal_connect (font_dialog, "response",
                        G_CALLBACK (response_cb), font_button);

      g_signal_connect (font_dialog, "destroy",
                        G_CALLBACK (dialog_destroy), font_button);
    }
  
  if (!gtk_widget_get_visible (font_button->priv->font_dialog))
    {
      font_dialog = GTK_FONT_CHOOSER (font_button->priv->font_dialog);
      gtk_font_chooser_set_font_desc (font_dialog, font_button->priv->font_desc);
    } 

  gtk_window_present (GTK_WINDOW (font_button->priv->font_dialog));
}


static void
response_cb (GtkDialog *dialog,
             gint       response_id,
             gpointer   data)
{
  GtkFontButton *font_button = GTK_FONT_BUTTON (data);
  GtkFontButtonPrivate *priv = font_button->priv;
  GtkFontChooser *font_chooser;
  GObject *object;

  gtk_widget_hide (font_button->priv->font_dialog);

  if (response_id != GTK_RESPONSE_OK)
    return;

  font_chooser = GTK_FONT_CHOOSER (priv->font_dialog);
  object = G_OBJECT (font_chooser);

  g_object_freeze_notify (object);

  clear_font_data (font_button);

  priv->font_desc = gtk_font_chooser_get_font_desc (font_chooser);
  if (priv->font_desc)
    priv->fontname = pango_font_description_to_string (priv->font_desc);
  priv->font_family = gtk_font_chooser_get_font_family (font_chooser);
  if (priv->font_family)
    g_object_ref (priv->font_family);
  priv->font_face = gtk_font_chooser_get_font_face (font_chooser);
  if (priv->font_face)
    g_object_ref (priv->font_face);
  priv->font_size = gtk_font_chooser_get_font_size (font_chooser);

  /* Set label font */
  gtk_font_button_update_font_info (font_button);

  g_object_notify (G_OBJECT (font_button), "font");
  g_object_notify (G_OBJECT (font_button), "font-desc");
  g_object_notify (G_OBJECT (font_button), "font-name");

  g_object_thaw_notify (object);

  /* Emit font_set signal */
  g_signal_emit (font_button, font_button_signals[FONT_SET], 0);
}

static void
dialog_destroy (GtkWidget *widget,
                gpointer   data)
{
  GtkFontButton *font_button = GTK_FONT_BUTTON (data);
    
  /* Dialog will get destroyed so reference is not valid now */
  font_button->priv->font_dialog = NULL;
} 

static GtkWidget *
gtk_font_button_create_inside (GtkFontButton *font_button)
{
  GtkWidget *widget;
  
  gtk_widget_push_composite_child ();

  widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  font_button->priv->font_label = gtk_label_new (_("Font"));
  
  gtk_label_set_justify (GTK_LABEL (font_button->priv->font_label), GTK_JUSTIFY_LEFT);
  gtk_box_pack_start (GTK_BOX (widget), font_button->priv->font_label, TRUE, TRUE, 5);

  if (font_button->priv->show_size) 
    {
      gtk_box_pack_start (GTK_BOX (widget), gtk_separator_new (GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 0);
      font_button->priv->size_label = gtk_label_new ("14");
      gtk_box_pack_start (GTK_BOX (widget), font_button->priv->size_label, FALSE, FALSE, 5);
    }

  gtk_widget_show_all (widget);

  gtk_widget_pop_composite_child ();

  return widget;
} 

static void
gtk_font_button_label_use_font (GtkFontButton *font_button)
{
  PangoFontDescription *desc;

  if (font_button->priv->use_font)
    {
      desc = pango_font_description_copy (font_button->priv->font_desc);

      if (!font_button->priv->use_size)
        pango_font_description_unset_fields (desc, PANGO_FONT_MASK_SIZE);
    }
  else
    desc = NULL;

  gtk_widget_override_font (font_button->priv->font_label, desc);

  if (desc)
    pango_font_description_free (desc);
}

static void
gtk_font_button_update_font_info (GtkFontButton *font_button)
{
  GtkFontButtonPrivate *priv = font_button->priv;
  gchar *family_style;

  g_assert (priv->font_desc != NULL);

  if (priv->show_style)
    {
      PangoFontDescription *desc = pango_font_description_copy_static (priv->font_desc);

      pango_font_description_unset_fields (desc, PANGO_FONT_MASK_SIZE);
      family_style = pango_font_description_to_string (desc);
      pango_font_description_free (desc);
    }
  else
    family_style = g_strdup (pango_font_description_get_family (priv->font_desc));

  gtk_label_set_text (GTK_LABEL (font_button->priv->font_label), family_style);
  g_free (family_style);

  if (font_button->priv->show_size) 
    {
      /* mirror Pango, which doesn't translate this either */
      gchar *size = g_strdup_printf ("%g%s",
                                     pango_font_description_get_size (priv->font_desc) / (double)PANGO_SCALE,
                                     pango_font_description_get_size_is_absolute (priv->font_desc) ? "px" : "");
      
      gtk_label_set_text (GTK_LABEL (font_button->priv->size_label), size);
      
      g_free (size);
    }

  gtk_font_button_label_use_font (font_button);
} 
