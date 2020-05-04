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

#include "gtkbinlayout.h"
#include "gtkbox.h"
#include "gtkcssprovider.h"
#include "gtkfontchooser.h"
#include "gtkfontchooserdialog.h"
#include "gtkfontchooserutils.h"
#include "gtkintl.h"
#include "gtklabel.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkseparator.h"
#include "gtkstylecontext.h"
#include "gtkwidgetprivate.h"

#include <string.h>
#include <stdio.h>


/**
 * SECTION:gtkfontbutton
 * @Short_description: A button to launch a font chooser dialog
 * @Title: GtkFontButton
 * @See_also: #GtkFontChooserDialog, #GtkColorButton.
 *
 * The #GtkFontButton is a button which displays the currently selected
 * font an allows to open a font chooser dialog to change the font.
 * It is suitable widget for selecting a font in a preference dialog.
 *
 * # CSS nodes
 *
 * GtkFontButton has a single CSS node with name fontbutton.
 */

typedef struct _GtkFontButtonClass   GtkFontButtonClass;

struct _GtkFontButton
{
  GtkWidget parent_instance;

  gchar         *title;
  gchar         *fontname;

  guint         use_font : 1;
  guint         use_size : 1;
  guint         show_preview_entry : 1;
  guint         modal    : 1;

  GtkWidget     *button;
  GtkWidget     *font_dialog;
  GtkWidget     *font_label;
  GtkWidget     *size_label;
  GtkWidget     *font_size_box;

  PangoFontDescription *font_desc;
  PangoFontFamily      *font_family;
  PangoFontFace        *font_face;
  PangoFontMap         *font_map;
  gint                  font_size;
  char                 *font_features;
  PangoLanguage        *language;
  gchar                *preview_text;
  GtkFontFilterFunc     font_filter;
  gpointer              font_filter_data;
  GDestroyNotify        font_filter_data_destroy;
  GtkCssProvider       *provider;

  GtkFontChooserLevel level;
};

struct _GtkFontButtonClass
{
  GtkWidgetClass parent_class;

  void (* font_set) (GtkFontButton *gfp);
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
  PROP_MODAL,
  PROP_USE_FONT,
  PROP_USE_SIZE
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

static void gtk_font_button_clicked                 (GtkButton         *button,
                                                     gpointer           user_data);

/* Dialog response functions */
static void response_cb                             (GtkDialog         *dialog,
                                                     gint               response_id,
                                                     gpointer           data);
static void dialog_destroy                          (GtkWidget         *widget,
                                                     gpointer           data);

/* Auxiliary functions */
static void gtk_font_button_label_use_font          (GtkFontButton     *gfs);
static void gtk_font_button_update_font_info        (GtkFontButton     *gfs);

static void        gtk_font_button_set_font_name (GtkFontButton *button,
                                                  const char    *fontname);
static const char *gtk_font_button_get_font_name (GtkFontButton *button);
static void        gtk_font_button_set_level     (GtkFontButton       *font_button,
                                                  GtkFontChooserLevel  level);
static void        gtk_font_button_set_language  (GtkFontButton *button,
                                                  const char    *language);

static guint font_button_signals[LAST_SIGNAL] = { 0 };

static PangoFontFamily * gtk_font_button_font_chooser_get_font_family (GtkFontChooser    *chooser);
static PangoFontFace *   gtk_font_button_font_chooser_get_font_face   (GtkFontChooser    *chooser);
static int               gtk_font_button_font_chooser_get_font_size   (GtkFontChooser    *chooser);
static void              gtk_font_button_font_chooser_set_filter_func (GtkFontChooser    *chooser,
                                                                       GtkFontFilterFunc  filter_func,
                                                                       gpointer           filter_data,
                                                                       GDestroyNotify     data_destroy);
static void              gtk_font_button_font_chooser_set_font_map    (GtkFontChooser    *chooser,
                                                                       PangoFontMap      *font_map);
static PangoFontMap *    gtk_font_button_font_chooser_get_font_map    (GtkFontChooser    *chooser);


static void
gtk_font_button_font_chooser_iface_init (GtkFontChooserIface *iface)
{
  iface->get_font_family = gtk_font_button_font_chooser_get_font_family;
  iface->get_font_face = gtk_font_button_font_chooser_get_font_face;
  iface->get_font_size = gtk_font_button_font_chooser_get_font_size;
  iface->set_filter_func = gtk_font_button_font_chooser_set_filter_func;
  iface->set_font_map = gtk_font_button_font_chooser_set_font_map;
  iface->get_font_map = gtk_font_button_font_chooser_get_font_map;
}

G_DEFINE_TYPE_WITH_CODE (GtkFontButton, gtk_font_button, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_FONT_CHOOSER,
                                                gtk_font_button_font_chooser_iface_init))

static void
clear_font_data (GtkFontButton *font_button)
{
  g_clear_object (&font_button->font_family);
  g_clear_object (&font_button->font_face);
  g_clear_pointer (&font_button->font_desc, pango_font_description_free);
  g_clear_pointer (&font_button->fontname, g_free);
  g_clear_pointer (&font_button->font_features, g_free);
}

static void
clear_font_filter_data (GtkFontButton *font_button)
{
  if (font_button->font_filter_data_destroy)
    font_button->font_filter_data_destroy (font_button->font_filter_data);
  font_button->font_filter = NULL;
  font_button->font_filter_data = NULL;
  font_button->font_filter_data_destroy = NULL;
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
  PangoFontFamily **families;
  PangoFontFace **faces;
  gint n_families, n_faces, i;
  const gchar *family;

  g_assert (font_button->font_desc != NULL);

  font_button->fontname = pango_font_description_to_string (font_button->font_desc);

  family = pango_font_description_get_family (font_button->font_desc);
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
          font_button->font_family = g_object_ref (families[i]);

          pango_font_family_list_faces (families[i], &faces, &n_faces);
          break;
        }
    }
  g_free (families);

  for (i = 0; i < n_faces; i++)
    {
      PangoFontDescription *tmp_desc = pango_font_face_describe (faces[i]);

      if (font_description_style_equal (tmp_desc, font_button->font_desc))
        {
          font_button->font_face = g_object_ref (faces[i]);

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
  if (font_button->font_dialog)
    return gtk_font_chooser_get_preview_text (GTK_FONT_CHOOSER (font_button->font_dialog));

  return g_strdup (font_button->preview_text);
}

static void
gtk_font_button_set_preview_text (GtkFontButton *font_button,
                                  const gchar   *preview_text)
{
  if (font_button->font_dialog)
    {
      gtk_font_chooser_set_preview_text (GTK_FONT_CHOOSER (font_button->font_dialog),
                                         preview_text);
      return;
    }

  g_free (font_button->preview_text);
  font_button->preview_text = g_strdup (preview_text);
}


static gboolean
gtk_font_button_get_show_preview_entry (GtkFontButton *font_button)
{
  if (font_button->font_dialog)
    return gtk_font_chooser_get_show_preview_entry (GTK_FONT_CHOOSER (font_button->font_dialog));

  return font_button->show_preview_entry;
}

static void
gtk_font_button_set_show_preview_entry (GtkFontButton *font_button,
                                        gboolean       show)
{
  show = show != FALSE;

  if (font_button->show_preview_entry != show)
    {
      font_button->show_preview_entry = show;
      if (font_button->font_dialog)
        gtk_font_chooser_set_show_preview_entry (GTK_FONT_CHOOSER (font_button->font_dialog), show);
      g_object_notify (G_OBJECT (font_button), "show-preview-entry");
    }
}

static PangoFontFamily *
gtk_font_button_font_chooser_get_font_family (GtkFontChooser *chooser)
{
  GtkFontButton *font_button = GTK_FONT_BUTTON (chooser);

  return font_button->font_family;
}

static PangoFontFace *
gtk_font_button_font_chooser_get_font_face (GtkFontChooser *chooser)
{
  GtkFontButton *font_button = GTK_FONT_BUTTON (chooser);

  return font_button->font_face;
}

static int
gtk_font_button_font_chooser_get_font_size (GtkFontChooser *chooser)
{
  GtkFontButton *font_button = GTK_FONT_BUTTON (chooser);

  return font_button->font_size;
}

static void
gtk_font_button_font_chooser_set_filter_func (GtkFontChooser    *chooser,
                                              GtkFontFilterFunc  filter_func,
                                              gpointer           filter_data,
                                              GDestroyNotify     data_destroy)
{
  GtkFontButton *font_button = GTK_FONT_BUTTON (chooser);

  if (font_button->font_dialog)
    {
      gtk_font_chooser_set_filter_func (GTK_FONT_CHOOSER (font_button->font_dialog),
                                        filter_func,
                                        filter_data,
                                        data_destroy);
      return;
    }

  clear_font_filter_data (font_button);
  font_button->font_filter = filter_func;
  font_button->font_filter_data = filter_data;
  font_button->font_filter_data_destroy = data_destroy;
}

static void
gtk_font_button_take_font_desc (GtkFontButton        *font_button,
                                PangoFontDescription *font_desc)
{
  GObject *object = G_OBJECT (font_button);

  if (font_button->font_desc && font_desc &&
      pango_font_description_equal (font_button->font_desc, font_desc))
    {
      pango_font_description_free (font_desc);
      return;
    }

  g_object_freeze_notify (object);

  clear_font_data (font_button);

  if (font_desc)
    font_button->font_desc = font_desc; /* adopted */
  else
    font_button->font_desc = pango_font_description_from_string (_("Sans 12"));

  if (pango_font_description_get_size_is_absolute (font_button->font_desc))
    font_button->font_size = pango_font_description_get_size (font_button->font_desc);
  else
    font_button->font_size = pango_font_description_get_size (font_button->font_desc) / PANGO_SCALE;

  gtk_font_button_update_font_data (font_button);
  gtk_font_button_update_font_info (font_button);

  if (font_button->font_dialog)
    gtk_font_chooser_set_font_desc (GTK_FONT_CHOOSER (font_button->font_dialog),
                                    font_button->font_desc);

  g_object_notify (G_OBJECT (font_button), "font");
  g_object_notify (G_OBJECT (font_button), "font-desc");

  g_object_thaw_notify (object);
}

static const PangoFontDescription *
gtk_font_button_get_font_desc (GtkFontButton *font_button)
{
  return font_button->font_desc;
}

static void
gtk_font_button_font_chooser_set_font_map (GtkFontChooser *chooser,
                                           PangoFontMap   *font_map)
{
  GtkFontButton *font_button = GTK_FONT_BUTTON (chooser);

  if (g_set_object (&font_button->font_map, font_map))
    {
      PangoContext *context;

      if (!font_map)
        font_map = pango_cairo_font_map_get_default ();

      context = gtk_widget_get_pango_context (font_button->font_label);
      pango_context_set_font_map (context, font_map);
    }
}

static PangoFontMap *
gtk_font_button_font_chooser_get_font_map (GtkFontChooser *chooser)
{
  GtkFontButton *font_button = GTK_FONT_BUTTON (chooser);

  return font_button->font_map;
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
gtk_font_button_class_init (GtkFontButtonClass *klass)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = (GObjectClass *) klass;
  widget_class = (GtkWidgetClass *) klass;

  gobject_class->finalize = gtk_font_button_finalize;
  gobject_class->set_property = gtk_font_button_set_property;
  gobject_class->get_property = gtk_font_button_get_property;

  widget_class->grab_focus = gtk_widget_grab_focus_child;
  widget_class->focus = gtk_widget_focus_child;

  klass->font_set = NULL;

  _gtk_font_chooser_install_properties (gobject_class);

  /**
   * GtkFontButton:title:
   * 
   * The title of the font chooser dialog.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_TITLE,
                                   g_param_spec_string ("title",
                                                        P_("Title"),
                                                        P_("The title of the font chooser dialog"),
                                                        _("Pick a Font"),
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkFontButton:use-font:
   * 
   * If this property is set to %TRUE, the label will be drawn 
   * in the selected font.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_USE_FONT,
                                   g_param_spec_boolean ("use-font",
                                                         P_("Use font in label"),
                                                         P_("Whether the label is drawn in the selected font"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkFontButton:use-size:
   * 
   * If this property is set to %TRUE, the label will be drawn 
   * with the selected font size.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_USE_SIZE,
                                   g_param_spec_boolean ("use-size",
                                                         P_("Use size in label"),
                                                         P_("Whether the label is drawn with the selected font size"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (gobject_class,
                                   PROP_MODAL,
                                   g_param_spec_boolean ("modal",
                                                         P_("Modal"),
                                                         P_("Whether the dialog is modal"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkFontButton::font-set:
   * @widget: the object which received the signal.
   * 
   * The ::font-set signal is emitted when the user selects a font. 
   * When handling this signal, use gtk_font_chooser_get_font()
   * to find out which font was just selected.
   *
   * Note that this signal is only emitted when the user
   * changes the font. If you need to react to programmatic font changes
   * as well, use the notify::font signal.
   */
  font_button_signals[FONT_SET] = g_signal_new (I_("font-set"),
                                                G_TYPE_FROM_CLASS (gobject_class),
                                                G_SIGNAL_RUN_FIRST,
                                                G_STRUCT_OFFSET (GtkFontButtonClass, font_set),
                                                NULL, NULL,
                                                NULL,
                                                G_TYPE_NONE, 0);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, I_("fontbutton"));
}

static void
gtk_font_button_init (GtkFontButton *font_button)
{
  GtkWidget *box;

  font_button->button = gtk_button_new ();
  g_signal_connect (font_button->button, "clicked", G_CALLBACK (gtk_font_button_clicked), font_button);
  font_button->font_label = gtk_label_new (_("Font"));
  gtk_widget_set_hexpand (font_button->font_label, TRUE);
  font_button->size_label = gtk_label_new ("14");
  font_button->font_size_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (box), font_button->font_label);

  gtk_container_add (GTK_CONTAINER (font_button->font_size_box), gtk_separator_new (GTK_ORIENTATION_VERTICAL));
  gtk_container_add (GTK_CONTAINER (font_button->font_size_box), font_button->size_label);
  gtk_container_add (GTK_CONTAINER (box), font_button->font_size_box);

  gtk_button_set_child (GTK_BUTTON (font_button->button), box);
  gtk_widget_set_parent (font_button->button, GTK_WIDGET (font_button));

  /* Initialize fields */
  font_button->modal = TRUE;
  font_button->use_font = FALSE;
  font_button->use_size = FALSE;
  font_button->show_preview_entry = TRUE;
  font_button->font_dialog = NULL;
  font_button->font_family = NULL;
  font_button->font_face = NULL;
  font_button->font_size = -1;
  font_button->title = g_strdup (_("Pick a Font"));
  font_button->level = GTK_FONT_CHOOSER_LEVEL_FAMILY |
                       GTK_FONT_CHOOSER_LEVEL_STYLE |
                       GTK_FONT_CHOOSER_LEVEL_SIZE;
  font_button->language = pango_language_get_default ();

  gtk_font_button_take_font_desc (font_button, NULL);

  gtk_widget_add_css_class (font_button->button, "font");
}

static void
gtk_font_button_finalize (GObject *object)
{
  GtkFontButton *font_button = GTK_FONT_BUTTON (object);

  if (font_button->font_dialog != NULL) 
    gtk_widget_destroy (font_button->font_dialog);

  g_free (font_button->title);

  clear_font_data (font_button);
  clear_font_filter_data (font_button);

  g_free (font_button->preview_text);

  g_clear_object (&font_button->provider);

  gtk_widget_unparent (font_button->button);

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
    case PROP_MODAL:
      gtk_font_button_set_modal (font_button, g_value_get_boolean (value));
      break;
    case GTK_FONT_CHOOSER_PROP_FONT_DESC:
      gtk_font_button_take_font_desc (font_button, g_value_dup_boxed (value));
      break;
    case GTK_FONT_CHOOSER_PROP_LANGUAGE:
      gtk_font_button_set_language (font_button, g_value_get_string (value));
      break;
    case GTK_FONT_CHOOSER_PROP_LEVEL:
      gtk_font_button_set_level (font_button, g_value_get_flags (value));
      break;
    case GTK_FONT_CHOOSER_PROP_FONT:
      gtk_font_button_set_font_name (font_button, g_value_get_string (value));
      break;
    case PROP_USE_FONT:
      gtk_font_button_set_use_font (font_button, g_value_get_boolean (value));
      break;
    case PROP_USE_SIZE:
      gtk_font_button_set_use_size (font_button, g_value_get_boolean (value));
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
    case PROP_MODAL:
      g_value_set_boolean (value, gtk_font_button_get_modal (font_button));
      break;
    case GTK_FONT_CHOOSER_PROP_FONT_DESC:
      g_value_set_boxed (value, gtk_font_button_get_font_desc (font_button));
      break;
    case GTK_FONT_CHOOSER_PROP_FONT_FEATURES:
      g_value_set_string (value, font_button->font_features);
      break;
    case GTK_FONT_CHOOSER_PROP_LANGUAGE:
      g_value_set_string (value, pango_language_to_string (font_button->language));
      break;
    case GTK_FONT_CHOOSER_PROP_LEVEL:
      g_value_set_flags (value, font_button->level);
      break;
    case GTK_FONT_CHOOSER_PROP_FONT:
      g_value_set_string (value, gtk_font_button_get_font_name (font_button));
      break;
    case PROP_USE_FONT:
      g_value_set_boolean (value, gtk_font_button_get_use_font (font_button));
      break;
    case PROP_USE_SIZE:
      g_value_set_boolean (value, gtk_font_button_get_use_size (font_button));
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
 */
GtkWidget *
gtk_font_button_new_with_font (const gchar *fontname)
{
  return g_object_new (GTK_TYPE_FONT_BUTTON, "font", fontname, NULL);
} 

/**
 * gtk_font_button_set_title:
 * @font_button: a #GtkFontButton
 * @title: a string containing the font chooser dialog title
 *
 * Sets the title for the font chooser dialog.  
 */
void
gtk_font_button_set_title (GtkFontButton *font_button, 
                           const gchar   *title)
{
  gchar *old_title;
  g_return_if_fail (GTK_IS_FONT_BUTTON (font_button));

  old_title = font_button->title;
  font_button->title = g_strdup (title);
  g_free (old_title);

  if (font_button->font_dialog)
    gtk_window_set_title (GTK_WINDOW (font_button->font_dialog), font_button->title);

  g_object_notify (G_OBJECT (font_button), "title");
}

/**
 * gtk_font_button_get_title:
 * @font_button: a #GtkFontButton
 *
 * Retrieves the title of the font chooser dialog.
 *
 * Returns: an internal copy of the title string which must not be freed.
 */
const gchar*
gtk_font_button_get_title (GtkFontButton *font_button)
{
  g_return_val_if_fail (GTK_IS_FONT_BUTTON (font_button), NULL);

  return font_button->title;
} 

/**
 * gtk_font_button_set_modal:
 * @font_button: a #GtkFontButton
 * @modal: %TRUE to make the dialog modal
 *
 * Sets whether the dialog should be modal.
 */
void
gtk_font_button_set_modal (GtkFontButton *font_button,
                           gboolean       modal)
{
  g_return_if_fail (GTK_IS_FONT_BUTTON (font_button));

  if (font_button->modal == modal)
    return;

  font_button->modal = modal;

  if (font_button->font_dialog)
    gtk_window_set_modal (GTK_WINDOW (font_button->font_dialog), font_button->modal);

  g_object_notify (G_OBJECT (font_button), "modal");
}

/**
 * gtk_font_button_get_modal:
 * @font_button: a #GtkFontButton
 *
 * Gets whether the dialog is modal.
 *
 * Returns: %TRUE if the dialog is modal
 */
gboolean
gtk_font_button_get_modal (GtkFontButton *font_button)
{
  g_return_val_if_fail (GTK_IS_FONT_BUTTON (font_button), FALSE);

  return font_button->modal;
}

/**
 * gtk_font_button_get_use_font:
 * @font_button: a #GtkFontButton
 *
 * Returns whether the selected font is used in the label.
 *
 * Returns: whether the selected font is used in the label.
 */
gboolean
gtk_font_button_get_use_font (GtkFontButton *font_button)
{
  g_return_val_if_fail (GTK_IS_FONT_BUTTON (font_button), FALSE);

  return font_button->use_font;
} 

/**
 * gtk_font_button_set_use_font:
 * @font_button: a #GtkFontButton
 * @use_font: If %TRUE, font name will be written using font chosen.
 *
 * If @use_font is %TRUE, the font name will be written using the selected font.  
 */
void  
gtk_font_button_set_use_font (GtkFontButton *font_button,
			      gboolean       use_font)
{
  g_return_if_fail (GTK_IS_FONT_BUTTON (font_button));

  use_font = (use_font != FALSE);

  if (font_button->use_font != use_font) 
    {
      font_button->use_font = use_font;

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
 */
gboolean
gtk_font_button_get_use_size (GtkFontButton *font_button)
{
  g_return_val_if_fail (GTK_IS_FONT_BUTTON (font_button), FALSE);

  return font_button->use_size;
} 

/**
 * gtk_font_button_set_use_size:
 * @font_button: a #GtkFontButton
 * @use_size: If %TRUE, font name will be written using the selected size.
 *
 * If @use_size is %TRUE, the font name will be written using the selected size.
 */
void
gtk_font_button_set_use_size (GtkFontButton *font_button,
                              gboolean       use_size)
{
  g_return_if_fail (GTK_IS_FONT_BUTTON (font_button));

  use_size = (use_size != FALSE);
  if (font_button->use_size != use_size) 
    {
      font_button->use_size = use_size;

      gtk_font_button_label_use_font (font_button);

      g_object_notify (G_OBJECT (font_button), "use-size");
    }
}

static const gchar *
gtk_font_button_get_font_name (GtkFontButton *font_button)
{
  g_return_val_if_fail (GTK_IS_FONT_BUTTON (font_button), NULL);

  return font_button->fontname;
}

static void
gtk_font_button_set_font_name (GtkFontButton *font_button,
                               const gchar    *fontname)
{
  PangoFontDescription *font_desc;

  font_desc = pango_font_description_from_string (fontname);
  gtk_font_button_take_font_desc (font_button, font_desc);
}

static void
gtk_font_button_clicked (GtkButton *button,
                         gpointer   user_data)
{
  GtkFontChooser *font_dialog;
  GtkFontButton  *font_button = user_data;
  
  if (!font_button->font_dialog) 
    {
      GtkWidget *parent;
      
      parent = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (font_button)));

      font_button->font_dialog = gtk_font_chooser_dialog_new (font_button->title, NULL);
      gtk_window_set_hide_on_close (GTK_WINDOW (font_button->font_dialog), TRUE);
      gtk_window_set_modal (GTK_WINDOW (font_button->font_dialog), font_button->modal);

      font_dialog = GTK_FONT_CHOOSER (font_button->font_dialog);

      if (font_button->font_map)
        gtk_font_chooser_set_font_map (font_dialog, font_button->font_map);

      gtk_font_chooser_set_show_preview_entry (font_dialog, font_button->show_preview_entry);
      gtk_font_chooser_set_level (GTK_FONT_CHOOSER (font_dialog), font_button->level);
      gtk_font_chooser_set_language (GTK_FONT_CHOOSER (font_dialog), pango_language_to_string (font_button->language));

      if (font_button->preview_text)
        {
          gtk_font_chooser_set_preview_text (font_dialog, font_button->preview_text);
          g_free (font_button->preview_text);
          font_button->preview_text = NULL;
        }

      if (font_button->font_filter)
        {
          gtk_font_chooser_set_filter_func (font_dialog,
                                            font_button->font_filter,
                                            font_button->font_filter_data,
                                            font_button->font_filter_data_destroy);
          font_button->font_filter = NULL;
          font_button->font_filter_data = NULL;
          font_button->font_filter_data_destroy = NULL;
        }

      if (GTK_IS_WINDOW (parent))
        {
          if (GTK_WINDOW (parent) != gtk_window_get_transient_for (GTK_WINDOW (font_dialog)))
            gtk_window_set_transient_for (GTK_WINDOW (font_dialog), GTK_WINDOW (parent));

          if (gtk_window_get_modal (GTK_WINDOW (parent)))
            gtk_window_set_modal (GTK_WINDOW (font_dialog), TRUE);
        }

      g_signal_connect (font_dialog, "notify",
                        G_CALLBACK (gtk_font_button_font_chooser_notify), button);

      g_signal_connect (font_dialog, "response",
                        G_CALLBACK (response_cb), font_button);

      g_signal_connect (font_dialog, "destroy",
                        G_CALLBACK (dialog_destroy), font_button);
    }

  if (!gtk_widget_get_visible (font_button->font_dialog))
    {
      font_dialog = GTK_FONT_CHOOSER (font_button->font_dialog);
      gtk_font_chooser_set_font_desc (font_dialog, font_button->font_desc);
    }

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_window_present (GTK_WINDOW (font_button->font_dialog));
  G_GNUC_END_IGNORE_DEPRECATIONS
}


static void
response_cb (GtkDialog *dialog,
             gint       response_id,
             gpointer   data)
{
  GtkFontButton *font_button = GTK_FONT_BUTTON (data);
  GtkFontChooser *font_chooser;
  GObject *object;

  gtk_widget_hide (font_button->font_dialog);

  if (response_id != GTK_RESPONSE_OK)
    return;

  font_chooser = GTK_FONT_CHOOSER (font_button->font_dialog);
  object = G_OBJECT (font_chooser);

  g_object_freeze_notify (object);

  clear_font_data (font_button);

  font_button->font_desc = gtk_font_chooser_get_font_desc (font_chooser);
  if (font_button->font_desc)
    font_button->fontname = pango_font_description_to_string (font_button->font_desc);
  font_button->font_family = gtk_font_chooser_get_font_family (font_chooser);
  if (font_button->font_family)
    g_object_ref (font_button->font_family);
  font_button->font_face = gtk_font_chooser_get_font_face (font_chooser);
  if (font_button->font_face)
    g_object_ref (font_button->font_face);
  font_button->font_size = gtk_font_chooser_get_font_size (font_chooser);
  g_free (font_button->font_features);
  font_button->font_features = gtk_font_chooser_get_font_features (font_chooser);
  font_button->language = pango_language_from_string (gtk_font_chooser_get_language (font_chooser));

  /* Set label font */
  gtk_font_button_update_font_info (font_button);

  g_object_notify (G_OBJECT (font_button), "font");
  g_object_notify (G_OBJECT (font_button), "font-desc");
  g_object_notify (G_OBJECT (font_button), "font-features");

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
  font_button->font_dialog = NULL;
}

static void
add_css_variations (GString    *s,
                    const char *variations)
{
  const char *p;
  const char *sep = "";

  if (variations == NULL || variations[0] == '\0')
    {
      g_string_append (s, "normal");
      return;
    }

  p = variations;
  while (p && *p)
    {
      const char *start;
      const char *end, *end2;
      double value;
      char name[5];

      while (g_ascii_isspace (*p)) p++;

      start = p;
      end = strchr (p, ',');
      if (end && (end - p < 6))
        goto skip;

      name[0] = p[0];
      name[1] = p[1];
      name[2] = p[2];
      name[3] = p[3];
      name[4] = '\0';

      p += 4;
      while (g_ascii_isspace (*p)) p++;
      if (*p == '=') p++;

      if (p - start < 5)
        goto skip;

      value = g_ascii_strtod (p, (char **) &end2);

      while (end2 && g_ascii_isspace (*end2)) end2++;

      if (end2 && (*end2 != ',' && *end2 != '\0'))
        goto skip;

      g_string_append_printf (s, "%s\"%s\" %g", sep, name, value);
      sep = ", ";

skip:
      p = end ? end + 1 : NULL;
    }
}

static gchar *
pango_font_description_to_css (PangoFontDescription *desc,
                               const char           *features,
                               const char           *language)
{
  GString *s;
  PangoFontMask set;

  s = g_string_new ("* { ");

  set = pango_font_description_get_set_fields (desc);
  if (set & PANGO_FONT_MASK_FAMILY)
    {
      g_string_append (s, "font-family: ");
      g_string_append (s, pango_font_description_get_family (desc));
      g_string_append (s, "; ");
    }
  if (set & PANGO_FONT_MASK_STYLE)
    {
      switch (pango_font_description_get_style (desc))
        {
        case PANGO_STYLE_NORMAL:
          g_string_append (s, "font-style: normal; ");
          break;
        case PANGO_STYLE_OBLIQUE:
          g_string_append (s, "font-style: oblique; ");
          break;
        case PANGO_STYLE_ITALIC:
          g_string_append (s, "font-style: italic; ");
          break;
        default:
          break;
        }
    }
  if (set & PANGO_FONT_MASK_VARIANT)
    {
      switch (pango_font_description_get_variant (desc))
        {
        case PANGO_VARIANT_NORMAL:
          g_string_append (s, "font-variant: normal; ");
          break;
        case PANGO_VARIANT_SMALL_CAPS:
          g_string_append (s, "font-variant: small-caps; ");
          break;
        default:
          break;
        }
    }
  if (set & PANGO_FONT_MASK_WEIGHT)
    {
      switch (pango_font_description_get_weight (desc))
        {
        case PANGO_WEIGHT_THIN:
          g_string_append (s, "font-weight: 100; ");
          break;
        case PANGO_WEIGHT_ULTRALIGHT:
          g_string_append (s, "font-weight: 200; ");
          break;
        case PANGO_WEIGHT_LIGHT:
        case PANGO_WEIGHT_SEMILIGHT:
          g_string_append (s, "font-weight: 300; ");
          break;
        case PANGO_WEIGHT_BOOK:
        case PANGO_WEIGHT_NORMAL:
          g_string_append (s, "font-weight: 400; ");
          break;
        case PANGO_WEIGHT_MEDIUM:
          g_string_append (s, "font-weight: 500; ");
          break;
        case PANGO_WEIGHT_SEMIBOLD:
          g_string_append (s, "font-weight: 600; ");
          break;
        case PANGO_WEIGHT_BOLD:
          g_string_append (s, "font-weight: 700; ");
          break;
        case PANGO_WEIGHT_ULTRABOLD:
          g_string_append (s, "font-weight: 800; ");
          break;
        case PANGO_WEIGHT_HEAVY:
        case PANGO_WEIGHT_ULTRAHEAVY:
          g_string_append (s, "font-weight: 900; ");
          break;
        default:
          break;
        }
    }
  if (set & PANGO_FONT_MASK_STRETCH)
    {
      switch (pango_font_description_get_stretch (desc))
        {
        case PANGO_STRETCH_ULTRA_CONDENSED:
          g_string_append (s, "font-stretch: ultra-condensed; ");
          break;
        case PANGO_STRETCH_EXTRA_CONDENSED:
          g_string_append (s, "font-stretch: extra-condensed; ");
          break;
        case PANGO_STRETCH_CONDENSED:
          g_string_append (s, "font-stretch: condensed; ");
          break;
        case PANGO_STRETCH_SEMI_CONDENSED:
          g_string_append (s, "font-stretch: semi-condensed; ");
          break;
        case PANGO_STRETCH_NORMAL:
          g_string_append (s, "font-stretch: normal; ");
          break;
        case PANGO_STRETCH_SEMI_EXPANDED:
          g_string_append (s, "font-stretch: semi-expanded; ");
          break;
        case PANGO_STRETCH_EXPANDED:
          g_string_append (s, "font-stretch: expanded; ");
          break;
        case PANGO_STRETCH_EXTRA_EXPANDED:
          break;
        case PANGO_STRETCH_ULTRA_EXPANDED:
          g_string_append (s, "font-stretch: ultra-expanded; ");
          break;
        default:
          break;
        }
    }
  if (set & PANGO_FONT_MASK_SIZE)
    {
      g_string_append_printf (s, "font-size: %dpt; ", pango_font_description_get_size (desc) / PANGO_SCALE);
    }

  if (set & PANGO_FONT_MASK_VARIATIONS)
    {
      const char *variations;

      g_string_append (s, "font-variation-settings: ");
      variations = pango_font_description_get_variations (desc);
      add_css_variations (s, variations);
      g_string_append (s, "; ");
    }
  if (features)
    {
      g_string_append_printf (s, "font-feature-settings: %s;", features);
    }

  g_string_append (s, "}");

  return g_string_free (s, FALSE);
}

static void
gtk_font_button_label_use_font (GtkFontButton *font_button)
{
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (font_button->font_label);

  if (!font_button->use_font)
    {
      if (font_button->provider)
        {
          gtk_style_context_remove_provider (context, GTK_STYLE_PROVIDER (font_button->provider));
          g_clear_object (&font_button->provider);
        }
    }
  else
    {
      PangoFontDescription *desc;
      gchar *data;

      if (!font_button->provider)
        {
          font_button->provider = gtk_css_provider_new ();
          gtk_style_context_add_provider (context,
                                          GTK_STYLE_PROVIDER (font_button->provider),
                                          GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        }

      desc = pango_font_description_copy (font_button->font_desc);

      if (!font_button->use_size)
        pango_font_description_unset_fields (desc, PANGO_FONT_MASK_SIZE);

      data = pango_font_description_to_css (desc,
                                            font_button->font_features,
                                            pango_language_to_string (font_button->language));
      gtk_css_provider_load_from_data (font_button->provider, data, -1);

      g_free (data);
      pango_font_description_free (desc);
    }
}

static void
gtk_font_button_update_font_info (GtkFontButton *font_button)
{
  const gchar *fam_name;
  const gchar *face_name;
  gchar *family_style;

  if (font_button->font_family)
    fam_name = pango_font_family_get_name (font_button->font_family);
  else
    fam_name = C_("font", "None");
  if (font_button->font_face)
    face_name = pango_font_face_get_face_name (font_button->font_face);
  else
    face_name = "";

  if ((font_button->level & GTK_FONT_CHOOSER_LEVEL_STYLE) != 0)
    family_style = g_strconcat (fam_name, " ", face_name, NULL);
  else
    family_style = g_strdup (fam_name);

  gtk_label_set_text (GTK_LABEL (font_button->font_label), family_style);
  g_free (family_style);

  if ((font_button->level & GTK_FONT_CHOOSER_LEVEL_SIZE) != 0)
    {
      /* mirror Pango, which doesn't translate this either */
      gchar *size = g_strdup_printf ("%2.4g%s",
                                     pango_font_description_get_size (font_button->font_desc) / (double)PANGO_SCALE,
                                     pango_font_description_get_size_is_absolute (font_button->font_desc) ? "px" : "");

      gtk_label_set_text (GTK_LABEL (font_button->size_label), size);

      g_free (size);

      gtk_widget_show (font_button->font_size_box);
    }
  else
    gtk_widget_hide (font_button->font_size_box);


  gtk_font_button_label_use_font (font_button);
}

static void
gtk_font_button_set_level (GtkFontButton       *font_button,
                           GtkFontChooserLevel  level)
{
  if (font_button->level == level)
    return;

  font_button->level = level;

  if (font_button->font_dialog)
    gtk_font_chooser_set_level (GTK_FONT_CHOOSER (font_button->font_dialog), level);

  gtk_font_button_update_font_info (font_button);

  g_object_notify (G_OBJECT (font_button), "level");
}

static void
gtk_font_button_set_language (GtkFontButton *font_button,
                              const char    *language)
{
  font_button->language = pango_language_from_string (language);

  if (font_button->font_dialog)
    gtk_font_chooser_set_language (GTK_FONT_CHOOSER (font_button->font_dialog), language);

  g_object_notify (G_OBJECT (font_button), "language");
}
