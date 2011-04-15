/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * Massively updated for Pango by Owen Taylor, May 2000
 * GtkFontSelection widget for Gtk+, by Damon Chaplin, May 1998.
 * Based on the GnomeFontSelector widget, by Elliot Lee, but major changes.
 * The GnomeFontSelector was derived from app/text_tool.c in the GIMP.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include <stdlib.h>
#include <glib/gprintf.h>
#include <string.h>

#include <atk/atk.h>

#include "gtkfontsel.h"
#include "gtkbutton.h"
#include "gtkcellrenderertext.h"
#include "gtkentry.h"
#include "gtkframe.h"
#include "gtkhbbox.h"
#include "gtkhbox.h"
#include "gtklabel.h"
#include "gtkliststore.h"
#include "gtkrc.h"
#include "gtkstock.h"
#include "gtktable.h"
#include "gtktreeselection.h"
#include "gtktreeview.h"
#include "gtkvbox.h"
#include "gtkscrolledwindow.h"
#include "gtkintl.h"
#include "gtkaccessible.h"
#include "gtkbuildable.h"
#include "gtkprivate.h"


/**
 * SECTION:gtkfontsel
 * @Short_description: A widget for selecting fonts
 * @Title: GtkFontSelection
 * @See_also: #GtkFontSelectionDialog
 *
 * The #GtkFontSelection widget lists the available fonts, styles and sizes,
 * allowing the user to select a font.
 * It is used in the #GtkFontSelectionDialog widget to provide a dialog box for
 * selecting fonts.
 *
 * To set the font which is initially selected, use
 * gtk_font_selection_set_font_name().
 *
 * To get the selected font use gtk_font_selection_get_font_name().
 *
 * To change the text which is shown in the preview area, use
 * gtk_font_selection_set_preview_text().
 */


struct _GtkFontSelectionPrivate
{
  GtkWidget *search_entry;
  GtkWidget *family_face_list;
  GtkWidget *size_slider;
  GtkWidget *size_spinner;

  gint             size;
  PangoFontFace   *face;
  PangoFontFamily *family;
};


struct _GtkFontSelectionDialogPrivate
{
  GtkWidget *fontsel;

  GtkWidget *ok_button;
  GtkWidget *apply_button;
  GtkWidget *cancel_button;
};


/* We don't enable the font and style entries because they don't add
 * much in terms of visible effect and have a weird effect on keynav.
 * the Windows font selector has entries similarly positioned but they
 * act in conjunction with the associated lists to form a single focus
 * location.
 */
#undef INCLUDE_FONT_ENTRIES

/* This is the default text shown in the preview entry, though the user
   can set it. Remember that some fonts only have capital letters. */
#define PREVIEW_TEXT N_("abcdefghijk ABCDEFGHIJK")

#define DEFAULT_FONT_NAME "Sans 10"

/* This is the initial and maximum height of the preview entry (it expands
   when large font sizes are selected). Initial height is also the minimum. */
#define INITIAL_PREVIEW_HEIGHT 44
#define MAX_PREVIEW_HEIGHT 300

/* These are the sizes of the font, style & size lists. */
#define FONT_LIST_HEIGHT	136
#define FONT_LIST_WIDTH		190
#define FONT_STYLE_LIST_WIDTH	170
#define FONT_SIZE_LIST_WIDTH	60

/* These are what we use as the standard font sizes, for the size list.
 */
static const guint16 font_sizes[] = {
  6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 22, 24, 26, 28,
  32, 36, 40, 48, 56, 64, 72
};

enum {
   PROP_0,
   PROP_FONT_NAME,
   PROP_PREVIEW_TEXT
};


enum {
  FAMILY_COLUMN,
  FAMILY_NAME_COLUMN
};

enum {
  FACE_COLUMN,
  FACE_NAME_COLUMN
};

enum {
  SIZE_COLUMN
};

static void    gtk_font_selection_set_property       (GObject         *object,
						      guint            prop_id,
						      const GValue    *value,
						      GParamSpec      *pspec);
static void    gtk_font_selection_get_property       (GObject         *object,
						      guint            prop_id,
						      GValue          *value,
						      GParamSpec      *pspec);
static void    gtk_font_selection_finalize	     (GObject         *object);
static void    gtk_font_selection_screen_changed     (GtkWidget	      *widget,
						      GdkScreen       *previous_screen);
static void    gtk_font_selection_style_updated      (GtkWidget      *widget);

static void     gtk_font_selection_ref_family            (GtkFontSelection *fontsel,
							  PangoFontFamily  *family);
static void     gtk_font_selection_ref_face              (GtkFontSelection *fontsel,
							  PangoFontFace    *face);

G_DEFINE_TYPE (GtkFontSelection, gtk_font_selection, GTK_TYPE_VBOX)

static void
gtk_font_selection_class_init (GtkFontSelectionClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->finalize = gtk_font_selection_finalize;
  gobject_class->set_property = gtk_font_selection_set_property;
  gobject_class->get_property = gtk_font_selection_get_property;

  widget_class->screen_changed = gtk_font_selection_screen_changed;
  widget_class->style_updated = gtk_font_selection_style_updated;
   
  g_object_class_install_property (gobject_class,
                                   PROP_FONT_NAME,
                                   g_param_spec_string ("font-name",
                                                        P_("Font name"),
                                                        P_("The string that represents this font"),
                                                        DEFAULT_FONT_NAME,
                                                        GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_PREVIEW_TEXT,
                                   g_param_spec_string ("preview-text",
                                                        P_("Preview text"),
                                                        P_("The text to display in order to demonstrate the selected font"),
                                                        _(PREVIEW_TEXT),
                                                        GTK_PARAM_READWRITE));

  g_type_class_add_private (klass, sizeof (GtkFontSelectionPrivate));
}

static void 
gtk_font_selection_set_property (GObject         *object,
				 guint            prop_id,
				 const GValue    *value,
				 GParamSpec      *pspec)
{
  GtkFontSelection *fontsel;

  fontsel = GTK_FONT_SELECTION (object);

  switch (prop_id)
    {
    case PROP_FONT_NAME:
      gtk_font_selection_set_font_name (fontsel, g_value_get_string (value));
      break;
    case PROP_PREVIEW_TEXT:
      gtk_font_selection_set_preview_text (fontsel, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void gtk_font_selection_get_property (GObject         *object,
					     guint            prop_id,
					     GValue          *value,
					     GParamSpec      *pspec)
{
  GtkFontSelection *fontsel;

  fontsel = GTK_FONT_SELECTION (object);

  switch (prop_id)
    {
    case PROP_FONT_NAME:
      g_value_take_string (value, gtk_font_selection_get_font_name (fontsel));
      break;
    case PROP_PREVIEW_TEXT:
      g_value_set_string (value, gtk_font_selection_get_preview_text (fontsel));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/* Handles key press events on the lists, so that we can trap Enter to
 * activate the default button on our own.
 */
static gboolean
list_row_activated (GtkWidget *widget)
{
  GtkWidget *default_widget, *focus_widget;
  GtkWindow *window;
  
  window = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (widget)));
  if (!gtk_widget_is_toplevel (GTK_WIDGET (window)))
    window = NULL;

  if (window)
    {
      default_widget = gtk_window_get_default_widget (window);
      focus_widget = gtk_window_get_focus (window);

      if (widget != default_widget &&
          !(widget == focus_widget && (!default_widget || !gtk_widget_get_sensitive (default_widget))))
        gtk_window_activate_default (window);
    }

  return TRUE;
}

static void
gtk_font_selection_init (GtkFontSelection *fontsel)
{
  GtkFontSelectionPrivate *priv;
  GtkWidget *scrolled_win;
  GtkWidget *text_box;
  GtkWidget *table, *label;
  GtkWidget *font_label, *style_label;
  GtkWidget *vbox;
  GtkListStore *model;
  GtkTreeViewColumn *column;
  GList *focus_chain = NULL;
  AtkObject *atk_obj;

  fontsel->priv = G_TYPE_INSTANCE_GET_PRIVATE (fontsel,
                                               GTK_TYPE_FONT_SELECTION,
                                               GtkFontSelectionPrivate);
  priv = fontsel->priv;
  gtk_widget_push_composite_child ();
  gtk_widget_pop_composite_child();
}

/**
 * gtk_font_selection_new:
 *
 * Creates a new #GtkFontSelection.
 *
 * Return value: a n ew #GtkFontSelection
 */
GtkWidget *
gtk_font_selection_new (void)
{
  GtkFontSelection *fontsel;
  
  fontsel = g_object_new (GTK_TYPE_FONT_SELECTION, NULL);
  
  return GTK_WIDGET (fontsel);
}

static void
gtk_font_selection_finalize (GObject *object)
{
  GtkFontSelection *fontsel = GTK_FONT_SELECTION (object);

  gtk_font_selection_ref_family (fontsel, NULL);
  gtk_font_selection_ref_face (fontsel, NULL);

  G_OBJECT_CLASS (gtk_font_selection_parent_class)->finalize (object);
}

static void
gtk_font_selection_screen_changed (GtkWidget *widget,
                                  GdkScreen *previous_screen)
{
  return;
}

static void
gtk_font_selection_style_updated (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (gtk_font_selection_parent_class)->style_updated (widget);

  return;
}

static void
gtk_font_selection_ref_family (GtkFontSelection *fontsel,
                              PangoFontFamily  *family)
{
  GtkFontSelectionPrivate *priv = fontsel->priv;
#if 0
  if (family)
    family = g_object_ref (family);
  if (priv->family)
    g_object_unref (priv->family);
  priv->family = family;
#endif
}

static void
gtk_font_selection_ref_face (GtkFontSelection *fontsel,
                                        PangoFontFace    *face)
{
  GtkFontSelectionPrivate *priv = fontsel->priv;
#if 0
  if (face)
    face = g_object_ref (face);
  if (priv->face)
    g_object_unref (priv->face);
  priv->face = face;
#endif
}

/*****************************************************************************
 * These functions are the main public interface for getting/setting the font.
 *****************************************************************************/

/**
 * gtk_font_selection_get_family_list:
 * @fontsel: a #GtkFontSelection
 *
 * This returns the #GtkTreeView that lists font families, for
 * example, 'Sans', 'Serif', etc.
 *
 * Return value: (transfer none): A #GtkWidget that is part of @fontsel
 *
 * Deprecated: 3.2
 */
GtkWidget *
gtk_font_selection_get_family_list (GtkFontSelection *fontsel)
{
  g_return_val_if_fail (GTK_IS_FONT_SELECTION (fontsel), NULL);

  return NULL;
}

/**
 * gtk_font_selection_get_face_list:
 * @fontsel: a #GtkFontSelection
 *
 * This returns the #GtkTreeView which lists all styles available for
 * the selected font. For example, 'Regular', 'Bold', etc.
 * 
 * Return value: (transfer none): A #GtkWidget that is part of @fontsel
 *
 * Deprecated: 3.2
 */
GtkWidget *
gtk_font_selection_get_face_list (GtkFontSelection *fontsel)
{
  g_return_val_if_fail (GTK_IS_FONT_SELECTION (fontsel), NULL);

  return NULL;
}

/**
 * gtk_font_selection_get_size_entry:
 * @fontsel: a #GtkFontSelection
 *
 * This returns the #GtkEntry used to allow the user to edit the font
 * number manually instead of selecting it from the list of font sizes.
 *
 * Return value: (transfer none): A #GtkWidget that is part of @fontsel
 *
 * Deprecated: 3.2
 */
GtkWidget *
gtk_font_selection_get_size_entry (GtkFontSelection *fontsel)
{
  g_return_val_if_fail (GTK_IS_FONT_SELECTION (fontsel), NULL);

  return NULL;
}

/**
 * gtk_font_selection_get_size_list:
 * @fontsel: a #GtkFontSelection
 *
 * This returns the #GtkTreeeView used to list font sizes.
 *
 * Return value: (transfer none): A #GtkWidget that is part of @fontsel
 *
 * Deprecated: 3.2
 */
GtkWidget *
gtk_font_selection_get_size_list (GtkFontSelection *fontsel)
{
  g_return_val_if_fail (GTK_IS_FONT_SELECTION (fontsel), NULL);

  return NULL;
}

/**
 * gtk_font_selection_get_preview_entry:
 * @fontsel: a #GtkFontSelection
 *
 * This returns the #GtkEntry used to display the font as a preview.
 *
 * Return value: (transfer none): A #GtkWidget that is part of @fontsel
 *
 * Deprecated: 3.2
 */
GtkWidget *
gtk_font_selection_get_preview_entry (GtkFontSelection *fontsel)
{
  g_return_val_if_fail (GTK_IS_FONT_SELECTION (fontsel), NULL);

  return NULL;
}

/**
 * gtk_font_selection_get_family:
 * @fontsel: a #GtkFontSelection
 *
 * Gets the #PangoFontFamily representing the selected font family.
 *
 * Return value: (transfer none): A #PangoFontFamily representing the
 *     selected font family. Font families are a collection of font
 *     faces. The returned object is owned by @fontsel and must not
 *     be modified or freed.
 *
 * Since: 2.14
 */
PangoFontFamily *
gtk_font_selection_get_family (GtkFontSelection *fontsel)
{
  g_return_val_if_fail (GTK_IS_FONT_SELECTION (fontsel), NULL);

  return NULL;
}

/**
 * gtk_font_selection_get_face:
 * @fontsel: a #GtkFontSelection
 *
 * Gets the #PangoFontFace representing the selected font group
 * details (i.e. family, slant, weight, width, etc).
 *
 * Return value: (transfer none): A #PangoFontFace representing the
 *     selected font group details. The returned object is owned by
 *     @fontsel and must not be modified or freed.
 *
 * Since: 2.14
 */
PangoFontFace *
gtk_font_selection_get_face (GtkFontSelection *fontsel)
{
  g_return_val_if_fail (GTK_IS_FONT_SELECTION (fontsel), NULL);

  return NULL;
}

/**
 * gtk_font_selection_get_size:
 * @fontsel: a #GtkFontSelection
 *
 * The selected font size.
 *
 * Return value: A n integer representing the selected font size,
 *     or -1 if no font size is selected.
 *
 * Since: 2.14
 **/
gint
gtk_font_selection_get_size (GtkFontSelection *fontsel)
{
  g_return_val_if_fail (GTK_IS_FONT_SELECTION (fontsel), -1);

  return NULL;
}

/**
 * gtk_font_selection_get_font_name:
 * @fontsel: a #GtkFontSelection
 * 
 * Gets the currently-selected font name. 
 *
 * Note that this can be a different string than what you set with 
 * gtk_font_selection_set_font_name(), as the font selection widget may 
 * normalize font names and thus return a string with a different structure. 
 * For example, "Helvetica Italic Bold 12" could be normalized to 
 * "Helvetica Bold Italic 12". Use pango_font_description_equal()
 * if you want to compare two font descriptions.
 * 
 * Return value: A string with the name of the current font, or %NULL if 
 *     no font is selected. You must free this string with g_free().
 */
gchar *
gtk_font_selection_get_font_name (GtkFontSelection *fontsel)
{
  return NULL;
}

/* This sets the current font, then selecting the appropriate list rows. */

/**
 * gtk_font_selection_set_font_name:
 * @fontsel: a #GtkFontSelection
 * @fontname: a font name like "Helvetica 12" or "Times Bold 18"
 * 
 * Sets the currently-selected font. 
 *
 * Note that the @fontsel needs to know the screen in which it will appear 
 * for this to work; this can be guaranteed by simply making sure that the 
 * @fontsel is inserted in a toplevel window before you call this function.
 * 
 * Return value: %TRUE if the font could be set successfully; %FALSE if no 
 *     such font exists or if the @fontsel doesn't belong to a particular 
 *     screen yet.
 */
gboolean
gtk_font_selection_set_font_name (GtkFontSelection *fontsel,
				  const gchar      *fontname)
{
  PangoFontFamily *family = NULL;
  PangoFontFace *face = NULL;
  PangoFontDescription *new_desc;
  
  g_return_val_if_fail (GTK_IS_FONT_SELECTION (fontsel), FALSE);

  return TRUE;
}

/**
 * gtk_font_selection_get_preview_text:
 * @fontsel: a #GtkFontSelection
 *
 * Gets the text displayed in the preview area.
 * 
 * Return value: the text displayed in the preview area. 
 *     This string is owned by the widget and should not be 
 *     modified or freed 
 */
G_CONST_RETURN gchar*
gtk_font_selection_get_preview_text (GtkFontSelection *fontsel)
{
  return NULL;
}


/**
 * gtk_font_selection_set_preview_text:
 * @fontsel: a #GtkFontSelection
 * @text: the text to display in the preview area 
 *
 * Sets the text displayed in the preview area.
 * The @text is used to show how the selected font looks.
 */
void
gtk_font_selection_set_preview_text  (GtkFontSelection *fontsel,
				      const gchar      *text)
{
  GtkFontSelectionPrivate *priv;

  g_return_if_fail (GTK_IS_FONT_SELECTION (fontsel));
  g_return_if_fail (text != NULL);

  priv = fontsel->priv;
}


/**
 * SECTION:gtkfontseldlg
 * @Short_description: A dialog box for selecting fonts
 * @Title: GtkFontSelectionDialog
 * @See_also: #GtkFontSelection, #GtkDialog
 *
 * The #GtkFontSelectionDialog widget is a dialog box for selecting a font.
 *
 * To set the font which is initially selected, use
 * gtk_font_selection_dialog_set_font_name().
 *
 * To get the selected font use gtk_font_selection_dialog_get_font_name().
 *
 * To change the text which is shown in the preview area, use
 * gtk_font_selection_dialog_set_preview_text().
 *
 * <refsect2 id="GtkFontSelectionDialog-BUILDER-UI">
 * <title>GtkFontSelectionDialog as GtkBuildable</title>
 * The GtkFontSelectionDialog implementation of the GtkBuildable interface
 * exposes the embedded #GtkFontSelection as internal child with the
 * name "font_selection". It also exposes the buttons with the names
 * "ok_button", "cancel_button" and "apply_button".
 * </refsect2>
 */

static void gtk_font_selection_dialog_buildable_interface_init     (GtkBuildableIface *iface);
static GObject * gtk_font_selection_dialog_buildable_get_internal_child (GtkBuildable *buildable,
									  GtkBuilder   *builder,
									  const gchar  *childname);

G_DEFINE_TYPE_WITH_CODE (GtkFontSelectionDialog, gtk_font_selection_dialog,
			 GTK_TYPE_DIALOG,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
						gtk_font_selection_dialog_buildable_interface_init))

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_font_selection_dialog_class_init (GtkFontSelectionDialogClass *klass)
{
  g_type_class_add_private (klass, sizeof (GtkFontSelectionDialogPrivate));
}

static void
gtk_font_selection_dialog_init (GtkFontSelectionDialog *fontseldiag)
{
  GtkFontSelectionDialogPrivate *priv;
  GtkDialog *dialog = GTK_DIALOG (fontseldiag);
  GtkWidget *action_area, *content_area;

  fontseldiag->priv = G_TYPE_INSTANCE_GET_PRIVATE (fontseldiag,
                                                   GTK_TYPE_FONT_SELECTION_DIALOG,
                                                   GtkFontSelectionDialogPrivate);
  priv = fontseldiag->priv;

  content_area = gtk_dialog_get_content_area (dialog);
  action_area = gtk_dialog_get_action_area (dialog);

  gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
  gtk_box_set_spacing (GTK_BOX (content_area), 2); /* 2 * 5 + 2 = 12 */
  gtk_container_set_border_width (GTK_CONTAINER (action_area), 5);
  gtk_box_set_spacing (GTK_BOX (action_area), 6);

  gtk_widget_push_composite_child ();

  gtk_window_set_resizable (GTK_WINDOW (fontseldiag), TRUE);

  /* Create the content area */
  priv->fontsel = gtk_font_selection_new ();
  gtk_container_set_border_width (GTK_CONTAINER (priv->fontsel), 5);
  gtk_widget_show (priv->fontsel);
  gtk_box_pack_start (GTK_BOX (content_area),
		      priv->fontsel, TRUE, TRUE, 0);

  /* Create the action area */
  priv->cancel_button = gtk_dialog_add_button (dialog,
                                               GTK_STOCK_CANCEL,
                                               GTK_RESPONSE_CANCEL);

  priv->apply_button = gtk_dialog_add_button (dialog,
                                              GTK_STOCK_APPLY,
                                              GTK_RESPONSE_APPLY);
  gtk_widget_hide (priv->apply_button);

  priv->ok_button = gtk_dialog_add_button (dialog,
                                           GTK_STOCK_OK,
                                           GTK_RESPONSE_OK);
  gtk_widget_grab_default (priv->ok_button);

  gtk_dialog_set_alternative_button_order (GTK_DIALOG (fontseldiag),
					   GTK_RESPONSE_OK,
					   GTK_RESPONSE_APPLY,
					   GTK_RESPONSE_CANCEL,
					   -1);

  gtk_window_set_title (GTK_WINDOW (fontseldiag),
                        _("Font Selection"));

  gtk_widget_pop_composite_child ();
}

/**
 * gtk_font_selection_dialog_new:
 * @title: the title of the dialog window 
 *
 * Creates a new #GtkFontSelectionDialog.
 *
 * Return value: a new #GtkFontSelectionDialog
 */
GtkWidget*
gtk_font_selection_dialog_new (const gchar *title)
{
  GtkFontSelectionDialog *fontseldiag;
  
  fontseldiag = g_object_new (GTK_TYPE_FONT_SELECTION_DIALOG, NULL);

  if (title)
    gtk_window_set_title (GTK_WINDOW (fontseldiag), title);
  
  return GTK_WIDGET (fontseldiag);
}

/**
 * gtk_font_selection_dialog_get_font_selection:
 * @fsd: a #GtkFontSelectionDialog
 *
 * Retrieves the #GtkFontSelection widget embedded in the dialog.
 *
 * Returns: (transfer none): the embedded #GtkFontSelection
 *
 * Since: 2.22
 **/
GtkWidget*
gtk_font_selection_dialog_get_font_selection (GtkFontSelectionDialog *fsd)
{
  g_return_val_if_fail (GTK_IS_FONT_SELECTION_DIALOG (fsd), NULL);

  return fsd->priv->fontsel;
}


/**
 * gtk_font_selection_dialog_get_ok_button:
 * @fsd: a #GtkFontSelectionDialog
 *
 * Gets the 'OK' button.
 *
 * Return value: (transfer none): the #GtkWidget used in the dialog
 *     for the 'OK' button.
 *
 * Since: 2.14
 */
GtkWidget *
gtk_font_selection_dialog_get_ok_button (GtkFontSelectionDialog *fsd)
{
  g_return_val_if_fail (GTK_IS_FONT_SELECTION_DIALOG (fsd), NULL);

  return fsd->priv->ok_button;
}

/**
 * gtk_font_selection_dialog_get_cancel_button:
 * @fsd: a #GtkFontSelectionDialog
 *
 * Gets the 'Cancel' button.
 *
 * Return value: (transfer none): the #GtkWidget used in the dialog
 *     for the 'Cancel' button.
 *
 * Since: 2.14
 */
GtkWidget *
gtk_font_selection_dialog_get_cancel_button (GtkFontSelectionDialog *fsd)
{
  g_return_val_if_fail (GTK_IS_FONT_SELECTION_DIALOG (fsd), NULL);

  return fsd->priv->cancel_button;
}

static void
gtk_font_selection_dialog_buildable_interface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->get_internal_child = gtk_font_selection_dialog_buildable_get_internal_child;
}

static GObject *
gtk_font_selection_dialog_buildable_get_internal_child (GtkBuildable *buildable,
							GtkBuilder   *builder,
							const gchar  *childname)
{
  GtkFontSelectionDialogPrivate *priv;

  priv = GTK_FONT_SELECTION_DIALOG (buildable)->priv;

  if (g_strcmp0 (childname, "ok_button") == 0)
    return G_OBJECT (priv->ok_button);
  else if (g_strcmp0 (childname, "cancel_button") == 0)
    return G_OBJECT (priv->cancel_button);
  else if (g_strcmp0 (childname, "apply_button") == 0)
    return G_OBJECT (priv->apply_button);
  else if (g_strcmp0 (childname, "font_selection") == 0)
    return G_OBJECT (priv->fontsel);

  return parent_buildable_iface->get_internal_child (buildable, builder, childname);
}

/**
 * gtk_font_selection_dialog_get_font_name:
 * @fsd: a #GtkFontSelectionDialog
 * 
 * Gets the currently-selected font name.
 *
 * Note that this can be a different string than what you set with 
 * gtk_font_selection_dialog_set_font_name(), as the font selection widget
 * may normalize font names and thus return a string with a different 
 * structure. For example, "Helvetica Italic Bold 12" could be normalized 
 * to "Helvetica Bold Italic 12".  Use pango_font_description_equal()
 * if you want to compare two font descriptions.
 * 
 * Return value: A string with the name of the current font, or %NULL if no 
 *     font is selected. You must free this string with g_free().
 */
gchar*
gtk_font_selection_dialog_get_font_name (GtkFontSelectionDialog *fsd)
{
  GtkFontSelectionDialogPrivate *priv;

  g_return_val_if_fail (GTK_IS_FONT_SELECTION_DIALOG (fsd), NULL);

  priv = fsd->priv;

  return gtk_font_selection_get_font_name (GTK_FONT_SELECTION (priv->fontsel));
}

/**
 * gtk_font_selection_dialog_set_font_name:
 * @fsd: a #GtkFontSelectionDialog
 * @fontname: a font name like "Helvetica 12" or "Times Bold 18"
 *
 * Sets the currently selected font. 
 * 
 * Return value: %TRUE if the font selected in @fsd is now the
 *     @fontname specified, %FALSE otherwise. 
 */
gboolean
gtk_font_selection_dialog_set_font_name (GtkFontSelectionDialog *fsd,
					 const gchar	        *fontname)
{
  GtkFontSelectionDialogPrivate *priv;

  g_return_val_if_fail (GTK_IS_FONT_SELECTION_DIALOG (fsd), FALSE);
  g_return_val_if_fail (fontname, FALSE);

  priv = fsd->priv;

  return gtk_font_selection_set_font_name (GTK_FONT_SELECTION (priv->fontsel), fontname);
}

/**
 * gtk_font_selection_dialog_get_preview_text:
 * @fsd: a #GtkFontSelectionDialog
 *
 * Gets the text displayed in the preview area.
 * 
 * Return value: the text displayed in the preview area. 
 *     This string is owned by the widget and should not be 
 *     modified or freed 
 */
G_CONST_RETURN gchar*
gtk_font_selection_dialog_get_preview_text (GtkFontSelectionDialog *fsd)
{
  GtkFontSelectionDialogPrivate *priv;

  g_return_val_if_fail (GTK_IS_FONT_SELECTION_DIALOG (fsd), NULL);

  priv = fsd->priv;

  return gtk_font_selection_get_preview_text (GTK_FONT_SELECTION (priv->fontsel));
}

/**
 * gtk_font_selection_dialog_set_preview_text:
 * @fsd: a #GtkFontSelectionDialog
 * @text: the text to display in the preview area
 *
 * Sets the text displayed in the preview area. 
 */
void
gtk_font_selection_dialog_set_preview_text (GtkFontSelectionDialog *fsd,
					    const gchar	           *text)
{
  GtkFontSelectionDialogPrivate *priv;

  g_return_if_fail (GTK_IS_FONT_SELECTION_DIALOG (fsd));
  g_return_if_fail (text != NULL);

  priv = fsd->priv;

  gtk_font_selection_set_preview_text (GTK_FONT_SELECTION (priv->fontsel), text);
}
