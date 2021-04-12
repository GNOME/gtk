/* GTK - The GIMP Toolkit
 * gtkfontchooser.c - Abstract interface for font file selectors GUIs
 *
 * Copyright (C) 2006, Emmanuele Bassi
 * Copyright (C) 2011 Alberto Ruiz <aruiz@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkfontchooser.h"
#include "gtkfontchooserprivate.h"
#include "gtkintl.h"
#include "gtktypebuiltins.h"
#include "gtkprivate.h"

/**
 * SECTION:gtkfontchooser
 * @Short_description: Interface implemented by widgets displaying fonts
 * @Title: GtkFontChooser
 * @See_also: #GtkFontChooserDialog, #GtkFontChooserWidget, #GtkFontButton
 *
 * #GtkFontChooser is an interface that can be implemented by widgets
 * displaying the list of fonts. In GTK+, the main objects
 * that implement this interface are #GtkFontChooserWidget,
 * #GtkFontChooserDialog and #GtkFontButton. The GtkFontChooser interface
 * has been introducted in GTK+ 3.2.
 */

enum
{
  SIGNAL_FONT_ACTIVATED,
  LAST_SIGNAL
};

static guint chooser_signals[LAST_SIGNAL];

typedef GtkFontChooserIface GtkFontChooserInterface;
G_DEFINE_INTERFACE (GtkFontChooser, gtk_font_chooser, G_TYPE_OBJECT);

static void
gtk_font_chooser_default_init (GtkFontChooserInterface *iface)
{
  /**
   * GtkFontChooser:font:
   *
   * The font description as a string, e.g. "Sans Italic 12".
   */
  g_object_interface_install_property
     (iface,
      g_param_spec_string ("font",
                          P_("Font"),
                           P_("Font description as a string, e.g. \"Sans Italic 12\""),
                           GTK_FONT_CHOOSER_DEFAULT_FONT_NAME,
                           GTK_PARAM_READWRITE));

  /**
   * GtkFontChooser:font-desc:
   *
   * The font description as a #PangoFontDescription.
   */
  g_object_interface_install_property
     (iface,
      g_param_spec_boxed ("font-desc",
                          P_("Font description"),
                          P_("Font description as a PangoFontDescription struct"),
                          PANGO_TYPE_FONT_DESCRIPTION,
                          GTK_PARAM_READWRITE));

  /**
   * GtkFontChooser:preview-text:
   *
   * The string with which to preview the font.
   */
  g_object_interface_install_property
     (iface,
      g_param_spec_string ("preview-text",
                          P_("Preview text"),
                          P_("The text to display in order to demonstrate the selected font"),
                          pango_language_get_sample_string (NULL),
                          GTK_PARAM_READWRITE));

  /**
   * GtkFontChooser:show-preview-entry:
   *
   * Whether to show an entry to change the preview text.
   */
  g_object_interface_install_property
     (iface,
      g_param_spec_boolean ("show-preview-entry",
                          P_("Show preview text entry"),
                          P_("Whether the preview text entry is shown or not"),
                          TRUE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkFontChooser:level:
   *
   * The level of granularity to offer for selecting fonts.
   *
   * Since: 3.24.1
   */
  g_object_interface_install_property
     (iface,
      g_param_spec_flags ("level",
                          P_("Selection level"),
                          P_("Whether to select family, face or font"),
                          GTK_TYPE_FONT_CHOOSER_LEVEL,
                          GTK_FONT_CHOOSER_LEVEL_FAMILY |
                          GTK_FONT_CHOOSER_LEVEL_STYLE |
                          GTK_FONT_CHOOSER_LEVEL_SIZE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkFontChooser:font-features:
   *
   * The selected font features, in a format that is compatible with
   * CSS and with Pango attributes.
   *
   * Since: 3.24.1
   */
  g_object_interface_install_property
     (iface,
      g_param_spec_string ("font-features",
                          P_("Font features"),
                          P_("Font features as a string"),
                          "",
                          GTK_PARAM_READABLE));

  /**
   * GtkFontChooser:language:
   *
   * The language for which the #GtkFontChooser:font-features were
   * selected, in a format that is compatible with CSS and with Pango
   * attributes.
   *
   * Since: 3.24.1
   */
  g_object_interface_install_property
     (iface,
      g_param_spec_string ("language",
                          P_("Language"),
                          P_("Language for which features have been selected"),
                          "",
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkFontChooser::font-activated:
   * @self: the object which received the signal
   * @fontname: the font name
   *
   * Emitted when a font is activated.
   * This usually happens when the user double clicks an item,
   * or an item is selected and the user presses one of the keys
   * Space, Shift+Space, Return or Enter.
    */
  chooser_signals[SIGNAL_FONT_ACTIVATED] =
    g_signal_new (I_("font-activated"),
                  GTK_TYPE_FONT_CHOOSER,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkFontChooserIface, font_activated),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  1, G_TYPE_STRING);
}

/**
 * gtk_font_chooser_get_font_family:
 * @fontchooser: a #GtkFontChooser
 *
 * Gets the #PangoFontFamily representing the selected font family.
 * Font families are a collection of font faces.
 *
 * If the selected font is not installed, returns %NULL.
 *
 * Returns: (nullable) (transfer none): A #PangoFontFamily representing the
 *     selected font family, or %NULL. The returned object is owned by @fontchooser
 *     and must not be modified or freed.
 *
 * Since: 3.2
 */
PangoFontFamily *
gtk_font_chooser_get_font_family (GtkFontChooser *fontchooser)
{
  g_return_val_if_fail (GTK_IS_FONT_CHOOSER (fontchooser), NULL);

  return GTK_FONT_CHOOSER_GET_IFACE (fontchooser)->get_font_family (fontchooser);
}

/**
 * gtk_font_chooser_get_font_face:
 * @fontchooser: a #GtkFontChooser
 *
 * Gets the #PangoFontFace representing the selected font group
 * details (i.e. family, slant, weight, width, etc).
 *
 * If the selected font is not installed, returns %NULL.
 *
 * Returns: (nullable) (transfer none): A #PangoFontFace representing the
 *     selected font group details, or %NULL. The returned object is owned by
 *     @fontchooser and must not be modified or freed.
 *
 * Since: 3.2
 */
PangoFontFace *
gtk_font_chooser_get_font_face (GtkFontChooser *fontchooser)
{
  g_return_val_if_fail (GTK_IS_FONT_CHOOSER (fontchooser), NULL);

  return GTK_FONT_CHOOSER_GET_IFACE (fontchooser)->get_font_face (fontchooser);
}

/**
 * gtk_font_chooser_get_font_size:
 * @fontchooser: a #GtkFontChooser
 *
 * The selected font size.
 *
 * Returns: A n integer representing the selected font size,
 *     or -1 if no font size is selected.
 *
 * Since: 3.2
 */
gint
gtk_font_chooser_get_font_size (GtkFontChooser *fontchooser)
{
  g_return_val_if_fail (GTK_IS_FONT_CHOOSER (fontchooser), -1);

  return GTK_FONT_CHOOSER_GET_IFACE (fontchooser)->get_font_size (fontchooser);
}

/**
 * gtk_font_chooser_get_font:
 * @fontchooser: a #GtkFontChooser
 *
 * Gets the currently-selected font name.
 *
 * Note that this can be a different string than what you set with
 * gtk_font_chooser_set_font(), as the font chooser widget may
 * normalize font names and thus return a string with a different
 * structure. For example, “Helvetica Italic Bold 12” could be
 * normalized to “Helvetica Bold Italic 12”.
 *
 * Use pango_font_description_equal() if you want to compare two
 * font descriptions.
 *
 * Returns: (nullable) (transfer full): A string with the name
 *     of the current font, or %NULL if  no font is selected. You must
 *     free this string with g_free().
 *
 * Since: 3.2
 */
gchar *
gtk_font_chooser_get_font (GtkFontChooser *fontchooser)
{
  gchar *fontname;

  g_return_val_if_fail (GTK_IS_FONT_CHOOSER (fontchooser), NULL);

  g_object_get (fontchooser, "font", &fontname, NULL);


  return fontname;
}

/**
 * gtk_font_chooser_set_font:
 * @fontchooser: a #GtkFontChooser
 * @fontname: a font name like “Helvetica 12” or “Times Bold 18”
 *
 * Sets the currently-selected font.
 *
 * Since: 3.2
 */
void
gtk_font_chooser_set_font (GtkFontChooser *fontchooser,
                           const gchar    *fontname)
{
  g_return_if_fail (GTK_IS_FONT_CHOOSER (fontchooser));
  g_return_if_fail (fontname != NULL);

  g_object_set (fontchooser, "font", fontname, NULL);
}

/**
 * gtk_font_chooser_get_font_desc:
 * @fontchooser: a #GtkFontChooser
 *
 * Gets the currently-selected font.
 *
 * Note that this can be a different string than what you set with
 * gtk_font_chooser_set_font(), as the font chooser widget may
 * normalize font names and thus return a string with a different
 * structure. For example, “Helvetica Italic Bold 12” could be
 * normalized to “Helvetica Bold Italic 12”.
 *
 * Use pango_font_description_equal() if you want to compare two
 * font descriptions.
 *
 * Returns: (nullable) (transfer full): A #PangoFontDescription for the
 *     current font, or %NULL if  no font is selected.
 *
 * Since: 3.2
 */
PangoFontDescription *
gtk_font_chooser_get_font_desc (GtkFontChooser *fontchooser)
{
  PangoFontDescription *font_desc;

  g_return_val_if_fail (GTK_IS_FONT_CHOOSER (fontchooser), NULL);

  g_object_get (fontchooser, "font-desc", &font_desc, NULL);

  return font_desc;
}

/**
 * gtk_font_chooser_set_font_desc:
 * @fontchooser: a #GtkFontChooser
 * @font_desc: a #PangoFontDescription
 *
 * Sets the currently-selected font from @font_desc.
 *
 * Since: 3.2
 */
void
gtk_font_chooser_set_font_desc (GtkFontChooser             *fontchooser,
                                const PangoFontDescription *font_desc)
{
  g_return_if_fail (GTK_IS_FONT_CHOOSER (fontchooser));
  g_return_if_fail (font_desc != NULL);

  g_object_set (fontchooser, "font-desc", font_desc, NULL);
}

/**
 * gtk_font_chooser_get_preview_text:
 * @fontchooser: a #GtkFontChooser
 *
 * Gets the text displayed in the preview area.
 *
 * Returns: (transfer full): the text displayed in the
 *     preview area
 *
 * Since: 3.2
 */
gchar *
gtk_font_chooser_get_preview_text (GtkFontChooser *fontchooser)
{
  char *text;

  g_return_val_if_fail (GTK_IS_FONT_CHOOSER (fontchooser), NULL);

  g_object_get (fontchooser, "preview-text", &text, NULL);

  return text;
}

/**
 * gtk_font_chooser_set_preview_text:
 * @fontchooser: a #GtkFontChooser
 * @text: (transfer none): the text to display in the preview area
 *
 * Sets the text displayed in the preview area.
 * The @text is used to show how the selected font looks.
 *
 * Since: 3.2
 */
void
gtk_font_chooser_set_preview_text (GtkFontChooser *fontchooser,
                                   const gchar    *text)
{
  g_return_if_fail (GTK_IS_FONT_CHOOSER (fontchooser));
  g_return_if_fail (text != NULL);

  g_object_set (fontchooser, "preview-text", text, NULL);
}

/**
 * gtk_font_chooser_get_show_preview_entry:
 * @fontchooser: a #GtkFontChooser
 *
 * Returns whether the preview entry is shown or not.
 *
 * Returns: %TRUE if the preview entry is shown
 *     or %FALSE if it is hidden.
 *
 * Since: 3.2
 */
gboolean
gtk_font_chooser_get_show_preview_entry (GtkFontChooser *fontchooser)
{
  gboolean show;

  g_return_val_if_fail (GTK_IS_FONT_CHOOSER (fontchooser), FALSE);

  g_object_get (fontchooser, "show-preview-entry", &show, NULL);

  return show;
}

/**
 * gtk_font_chooser_set_show_preview_entry:
 * @fontchooser: a #GtkFontChooser
 * @show_preview_entry: whether to show the editable preview entry or not
 *
 * Shows or hides the editable preview entry.
 *
 * Since: 3.2
 */
void
gtk_font_chooser_set_show_preview_entry (GtkFontChooser *fontchooser,
                                         gboolean        show_preview_entry)
{
  g_return_if_fail (GTK_IS_FONT_CHOOSER (fontchooser));

  show_preview_entry = show_preview_entry != FALSE;
  g_object_set (fontchooser, "show-preview-entry", show_preview_entry, NULL);
}

/**
 * gtk_font_chooser_set_filter_func:
 * @fontchooser: a #GtkFontChooser
 * @filter: (allow-none): a #GtkFontFilterFunc, or %NULL
 * @user_data: data to pass to @filter
 * @destroy: function to call to free @data when it is no longer needed
 *
 * Adds a filter function that decides which fonts to display
 * in the font chooser.
 *
 * Since: 3.2
 */
void
gtk_font_chooser_set_filter_func (GtkFontChooser   *fontchooser,
                                  GtkFontFilterFunc filter,
                                  gpointer          user_data,
                                  GDestroyNotify    destroy)
{
  g_return_if_fail (GTK_IS_FONT_CHOOSER (fontchooser));

  GTK_FONT_CHOOSER_GET_IFACE (fontchooser)->set_filter_func (fontchooser,
                                                             filter,
                                                             user_data,
                                                             destroy);
}

void
_gtk_font_chooser_font_activated (GtkFontChooser *chooser,
                                  const gchar    *fontname)
{
  g_return_if_fail (GTK_IS_FONT_CHOOSER (chooser));

  g_signal_emit (chooser, chooser_signals[SIGNAL_FONT_ACTIVATED], 0, fontname);
}

/**
 * gtk_font_chooser_set_font_map:
 * @fontchooser: a #GtkFontChooser
 * @fontmap: (allow-none): a #PangoFontMap
 *
 * Sets a custom font map to use for this font chooser widget.
 * A custom font map can be used to present application-specific
 * fonts instead of or in addition to the normal system fonts.
 *
 * |[<!-- language="C" -->
 * FcConfig *config;
 * PangoFontMap *fontmap;
 *
 * config = FcInitLoadConfigAndFonts ();
 * FcConfigAppFontAddFile (config, my_app_font_file);
 *
 * fontmap = pango_cairo_font_map_new_for_font_type (CAIRO_FONT_TYPE_FT);
 * pango_fc_font_map_set_config (PANGO_FC_FONT_MAP (fontmap), config);
 *
 * gtk_font_chooser_set_font_map (font_chooser, fontmap);
 * ]|
 *
 * Note that other GTK+ widgets will only be able to use the application-specific
 * font if it is present in the font map they use:
 *
 * |[
 * context = gtk_widget_get_pango_context (label);
 * pango_context_set_font_map (context, fontmap);
 * ]|
 *
 * Since: 3.18
 */
void
gtk_font_chooser_set_font_map (GtkFontChooser *fontchooser,
                               PangoFontMap   *fontmap)
{
  g_return_if_fail (GTK_IS_FONT_CHOOSER (fontchooser));
  g_return_if_fail (fontmap == NULL || PANGO_IS_FONT_MAP (fontmap));

  if (GTK_FONT_CHOOSER_GET_IFACE (fontchooser)->set_font_map)
    GTK_FONT_CHOOSER_GET_IFACE (fontchooser)->set_font_map (fontchooser, fontmap);
}

/**
 * gtk_font_chooser_get_font_map:
 * @fontchooser: a #GtkFontChooser
 *
 * Gets the custom font map of this font chooser widget,
 * or %NULL if it does not have one.
 *
 * Returns: (nullable) (transfer full): a #PangoFontMap, or %NULL
 *
 * Since: 3.18
 */
PangoFontMap *
gtk_font_chooser_get_font_map (GtkFontChooser *fontchooser)
{
  PangoFontMap *fontmap = NULL;

  g_return_val_if_fail (GTK_IS_FONT_CHOOSER (fontchooser), NULL);

  if (GTK_FONT_CHOOSER_GET_IFACE (fontchooser)->get_font_map)
    fontmap = GTK_FONT_CHOOSER_GET_IFACE (fontchooser)->get_font_map (fontchooser);

  return fontmap;
}

/**
 * gtk_font_chooser_set_level:
 * @fontchooser: a #GtkFontChooser
 * @level: the desired level of granularity
 *
 * Sets the desired level of granularity for selecting fonts.
 *
 * Since: 3.24
 */
void
gtk_font_chooser_set_level (GtkFontChooser      *fontchooser,
                            GtkFontChooserLevel  level)
{
  g_return_if_fail (GTK_IS_FONT_CHOOSER (fontchooser));

  g_object_set (fontchooser, "level", level, NULL);
}

/**
 * gtk_font_chooser_get_level:
 * @fontchooser: a #GtkFontChooser
 *
 * Returns the current level of granularity for selecting fonts.
 *
 * Returns: the current granularity level
 *
 * Since: 3.24
 */
GtkFontChooserLevel
gtk_font_chooser_get_level (GtkFontChooser *fontchooser)
{
  GtkFontChooserLevel level;

  g_return_val_if_fail (GTK_IS_FONT_CHOOSER (fontchooser), 0);

  g_object_get (fontchooser, "level", &level, NULL);

  return level;
}

/**
 * gtk_font_chooser_get_font_features:
 * @fontchooser: a #GtkFontChooser
 *
 * Gets the currently-selected font features.
 *
 * Returns: the currently selected font features
 *
 * Since: 3.24
 */
char *
gtk_font_chooser_get_font_features (GtkFontChooser *fontchooser)
{
  char *text;

  g_return_val_if_fail (GTK_IS_FONT_CHOOSER (fontchooser), NULL);

  g_object_get (fontchooser, "font-features", &text, NULL);

  return text;
}

/**
 * gtk_font_chooser_get_language:
 * @fontchooser: a #GtkFontChooser
 *
 * Gets the language that is used for font features.
 *
 * Returns: the currently selected language
 *
 * Since: 3.24
 */
char *
gtk_font_chooser_get_language (GtkFontChooser *fontchooser)
{
  char *text;

  g_return_val_if_fail (GTK_IS_FONT_CHOOSER (fontchooser), NULL);

  g_object_get (fontchooser, "language", &text, NULL);

  return text;
}

/**
 * gtk_font_chooser_set_language:
 * @fontchooser: a #GtkFontChooser
 * @language: a language
 *
 * Sets the language to use for font features.
 *
 * Since: 3.24
 */
void
gtk_font_chooser_set_language (GtkFontChooser *fontchooser,
                               const char     *language)
{
  g_return_if_fail (GTK_IS_FONT_CHOOSER (fontchooser));

  g_object_set (fontchooser, "language", language, NULL);
}
