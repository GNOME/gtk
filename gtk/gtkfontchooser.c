/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Alberto Ruiz <aruiz@gnome.org>
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * Massively updated to rework the user interface by Alberto Ruiz, 2011
 * Massively updated for Pango by Owen Taylor, May 2000
 * GtkFontChooser widget for Gtk+, by Damon Chaplin, May 1998.
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

#include "config.h"

#include <stdlib.h>
#include <glib/gprintf.h>
#include <string.h>

#include <atk/atk.h>

#include "gtkfontchooser.h"
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
#include "gtkalignment.h"
#include "gtkscale.h"
#include "gtkbox.h"
#include "gtkspinbutton.h"
#include "gtkwidget.h"

/**
 * SECTION:gtkfontchooser
 * @Short_description: A widget for selecting fonts
 * @Title: GtkFontChooser
 * @See_also: #GtkFontChooserDialog
 *
 * The #GtkFontChooser widget lists the available fonts, styles and sizes,
 * allowing the user to select a font.
 * It is used in the #GtkFontChooserDialog widget to provide a dialog box for
 * selecting fonts.
 *
 * To set the font which is initially selected, use
 * gtk_font_chooser_set_font_name().
 *
 * To get the selected font use gtk_font_chooser_get_font_name().
 *
 * To change the text which is shown in the preview area, use
 * gtk_font_chooser_set_preview_text().
 */


struct _GtkFontChooserPrivate
{
  GtkWidget    *search_entry;
  GtkWidget    *family_face_list;
  GtkListStore *model;  
  GtkTreeModel *filter;

  GtkWidget       *preview;
  GtkWidget       *preview_scrolled_window;
  gchar           *preview_text;
  gboolean         show_preview_entry;

  GtkWidget *size_spin;
  GtkWidget *size_slider;
  gboolean   ignore_slider;

  gint             size;
  PangoFontFace   *face;
  PangoFontFamily *family;
};


struct _GtkFontChooserDialogPrivate
{
  GtkWidget *fontchooser;

  GtkWidget *select_button;
  GtkWidget *cancel_button;
};


#define DEFAULT_FONT_NAME "Sans 10"
#define MAX_FONT_SIZE 999

/* This is the initial fixed height and the top padding of the preview entry */
#define PREVIEW_HEIGHT 72
#define PREVIEW_TOP_PADDING 6

/* Widget default geometry */
#define FONT_CHOOSER_WIDTH           540
#define FONT_CHOOSER_HEIGHT          408

/* These are the sizes of the font, style & size lists. */
#define FONT_LIST_HEIGHT  136
#define FONT_LIST_WIDTH   190
#define FONT_STYLE_LIST_WIDTH 170
#define FONT_SIZE_LIST_WIDTH  60

#define ROW_FORMAT_STRING "<span weight=\"bold\" size=\"small\" foreground=\"%s\">%s</span>\n<span size=\"x-large\" font_desc=\"%s\">%s</span>"

/* These are what we use as the standard font sizes, for the size list.
 */
#define FONT_SIZES_LENGTH 14
static const gint font_sizes[] = {
  6, 8, 9, 10, 11, 12, 13, 14, 16, 20, 24, 36, 48, 72
};

enum {
   PROP_0,
   PROP_FONT_NAME,
   PROP_PREVIEW_TEXT,
   PROP_SHOW_PREVIEW_ENTRY
};


enum {
  FAMILY_COLUMN,
  FACE_COLUMN,
  PREVIEW_TEXT_COLUMN,
  PREVIEW_TITLE_COLUMN
};

static void  gtk_font_chooser_set_property       (GObject         *object,
                                                  guint            prop_id,
                                                  const GValue    *value,
                                                  GParamSpec      *pspec);
static void  gtk_font_chooser_get_property       (GObject         *object,
                                                  guint            prop_id,
                                                  GValue          *value,
                                                  GParamSpec      *pspec);
static void  gtk_font_chooser_finalize           (GObject         *object);

static void  gtk_font_chooser_screen_changed     (GtkWidget       *widget,
                                                  GdkScreen       *previous_screen);
static void  gtk_font_chooser_style_updated      (GtkWidget      *widget);

static void  gtk_font_chooser_ref_family         (GtkFontChooser *fontchooser,
                                                  PangoFontFamily  *family);
static void  gtk_font_chooser_ref_face           (GtkFontChooser *fontchooser,
                                                  PangoFontFace    *face);

static void gtk_font_chooser_bootstrap_fontlist (GtkFontChooser *fontchooser);

G_DEFINE_TYPE (GtkFontChooser, gtk_font_chooser, GTK_TYPE_VBOX)

static void
gtk_font_chooser_class_init (GtkFontChooserClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->screen_changed = gtk_font_chooser_screen_changed;
  widget_class->style_updated = gtk_font_chooser_style_updated;

  gobject_class->finalize = gtk_font_chooser_finalize;
  gobject_class->set_property = gtk_font_chooser_set_property;
  gobject_class->get_property = gtk_font_chooser_get_property;

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
                                                        pango_language_get_sample_string (NULL),
                                                        GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_PREVIEW_ENTRY,
                                   g_param_spec_boolean ("show-preview-entry",
                                                        P_("Show preview text entry"),
                                                        P_("Whether the preview text entry is shown or not"),
                                                        TRUE,
                                                        GTK_PARAM_READWRITE));

  g_type_class_add_private (klass, sizeof (GtkFontChooserPrivate));
}

static void 
gtk_font_chooser_set_property (GObject         *object,
                                 guint            prop_id,
                                 const GValue    *value,
                                 GParamSpec      *pspec)
{
  GtkFontChooser *fontchooser;

  fontchooser = GTK_FONT_CHOOSER (object);

  switch (prop_id)
    {
    case PROP_FONT_NAME:
      gtk_font_chooser_set_font_name (fontchooser, g_value_get_string (value));
      break;
    case PROP_PREVIEW_TEXT:
      gtk_font_chooser_set_preview_text (fontchooser, g_value_get_string (value));
      break;
    case PROP_SHOW_PREVIEW_ENTRY:
      gtk_font_chooser_set_show_preview_entry (fontchooser, g_value_get_boolean (value));
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_font_chooser_get_property (GObject         *object,
                                 guint            prop_id,
                                 GValue          *value,
                                 GParamSpec      *pspec)
{
  GtkFontChooser *fontchooser;

  fontchooser = GTK_FONT_CHOOSER (object);

  switch (prop_id)
    {
    case PROP_FONT_NAME:
      g_value_take_string (value, gtk_font_chooser_get_font_name (fontchooser));
      break;
    case PROP_PREVIEW_TEXT:
      g_value_set_string (value, gtk_font_chooser_get_preview_text (fontchooser));
      break;
    case PROP_SHOW_PREVIEW_ENTRY:
      g_value_set_boolean (value, gtk_font_chooser_get_show_preview_entry (fontchooser));
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

void
refilter_and_focus (GtkFontChooserPrivate *priv)
{
  GtkTreeIter  iter;
  GtkTreeView *treeview = GTK_TREE_VIEW (priv->family_face_list);
  GtkTreePath *path = gtk_tree_path_new ();

  gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (priv->filter));

  if (!path)
    return;

  gtk_tree_view_get_cursor (treeview, &path, NULL);

  if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->filter), &iter, path))
    {
      gtk_tree_path_free (path);
      return;
    }

  gtk_tree_view_scroll_to_cell (treeview, path, NULL, FALSE, 0.5, 0.5);
  gtk_tree_path_free (path);
}

void
deleted_text_cb (GtkEntryBuffer *buffer,
                 guint           position,
                 guint           n_chars,
                 gpointer        user_data)
{
  GtkFontChooserPrivate *priv  = (GtkFontChooserPrivate*)user_data;
  GtkWidget             *entry = priv->search_entry;
  
  if (gtk_entry_buffer_get_length (buffer) == 0)
    {
      gtk_entry_set_icon_from_stock (GTK_ENTRY (entry),
                                     GTK_ENTRY_ICON_SECONDARY,
                                     GTK_STOCK_FIND);
    }

  gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (priv->filter));
}

void
inserted_text_cb (GtkEntryBuffer *buffer,
                  guint           position,
                  gchar          *chars,
                  guint           n_chars,
                  gpointer        user_data) 
{
  GtkFontChooserPrivate *priv  = (GtkFontChooserPrivate*)user_data;
  GtkWidget             *entry = priv->search_entry;

  if (g_strcmp0 (gtk_entry_get_icon_stock (GTK_ENTRY (entry), GTK_ENTRY_ICON_SECONDARY),
                 GTK_STOCK_CLEAR))
    gtk_entry_set_icon_from_stock (GTK_ENTRY (entry),
                                   GTK_ENTRY_ICON_SECONDARY,
                                   GTK_STOCK_CLEAR);


  gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (priv->filter));
}

void
icon_press_cb (GtkEntry             *entry,
               GtkEntryIconPosition  pos,
               GdkEvent             *event,
               gpointer              user_data)
{
  gtk_entry_buffer_delete_text (gtk_entry_get_buffer (entry), 0, -1);
}

void
slider_change_cb (GtkAdjustment *adjustment, gpointer data)
{
  GtkFontChooserPrivate *priv = (GtkFontChooserPrivate*)data;

  /* If we set the silder value manually, we ignore this callback */
  if (priv->ignore_slider)
    {
      priv->ignore_slider = FALSE;
      return;
    }

  gtk_adjustment_set_value (gtk_spin_button_get_adjustment( GTK_SPIN_BUTTON(priv->size_spin)),
                            gtk_adjustment_get_value (adjustment));
}

void
spin_change_cb (GtkAdjustment *adjustment, gpointer data)
{
  PangoFontDescription    *desc;
  GtkFontChooser          *fontchooser = (GtkFontChooser*)data;
  GtkFontChooserPrivate   *priv        = fontchooser->priv;

  gdouble size = gtk_adjustment_get_value (adjustment);
  
  GtkAdjustment *slider_adj = gtk_range_get_adjustment (GTK_RANGE (priv->size_slider));

  /* We ignore the slider value change callback for both of this set_value call */
  priv->ignore_slider = TRUE;
  if (size < gtk_adjustment_get_lower (slider_adj))
    gtk_adjustment_set_value (slider_adj, gtk_adjustment_get_lower (slider_adj));
  else if (size > gtk_adjustment_get_upper (slider_adj))
    gtk_adjustment_set_value (slider_adj, gtk_adjustment_get_upper (slider_adj));
  else
    gtk_adjustment_set_value (slider_adj, size);

  priv->size = ((gint)gtk_adjustment_get_value (adjustment)) * PANGO_SCALE;

  desc = pango_context_get_font_description (gtk_widget_get_pango_context (priv->preview));
  pango_font_description_set_size (desc, priv->size);
  gtk_widget_override_font (priv->preview, desc);
  
  g_object_notify (G_OBJECT (fontchooser), "font-name");

  gtk_widget_queue_draw (priv->preview);
}

void
set_range_marks (GtkFontChooserPrivate *priv,
                 GtkWidget* size_slider,
                 gint* sizes,
                 gint length)
{
  GtkAdjustment *adj;
  gint i;
  gdouble value;

  if (length<2)
    {
      sizes = (gint*)font_sizes;
      length = FONT_SIZES_LENGTH;
    }
  
  gtk_scale_clear_marks (GTK_SCALE (size_slider));
  
  adj = gtk_range_get_adjustment(GTK_RANGE (size_slider));
  
  gtk_adjustment_set_lower (adj, (gdouble) sizes[0]);
  gtk_adjustment_set_upper (adj, (gdouble) sizes[length-1]);

  value = gtk_adjustment_get_value (adj);
  if (value > (gdouble) sizes[length-1])
    {
      gtk_adjustment_set_value (adj, (gdouble) sizes[length-1]);
      priv->ignore_slider = TRUE;
    }
  else if (value < (gdouble) sizes[0])
    {
      gtk_adjustment_set_value (adj, (gdouble) sizes[0]);
      priv->ignore_slider = TRUE; 
    }
  
  for (i=0; i<length; i++)
    gtk_scale_add_mark (GTK_SCALE (size_slider),
                        (gdouble) sizes[i],
                        GTK_POS_BOTTOM, NULL);
}

void
cursor_changed_cb (GtkTreeView *treeview, gpointer data)
{
  PangoFontFamily      *family;
  PangoFontFace        *face;
  PangoFontDescription *desc;
  
  gint *sizes;
  gint  i, n_sizes;

  GtkTreeIter  iter;
  GtkTreePath *path = gtk_tree_path_new ();
  
  GtkFontChooser *fontchooser = (GtkFontChooser*)data;
  
  gtk_tree_view_get_cursor (treeview, &path, NULL);
  
  if (!path)
    return;

  if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (fontchooser->priv->filter), &iter, path))
    {
      gtk_tree_path_free (path);
      return;
    } 
  
  
  gtk_tree_model_get (GTK_TREE_MODEL (fontchooser->priv->filter), &iter,
                      FACE_COLUMN, &face,
                      FAMILY_COLUMN, &family,
                      -1);

  gtk_tree_view_scroll_to_cell (treeview, path, NULL, FALSE, 0.5, 0.5);

  gtk_tree_path_free (path);
  path = NULL;
  
  if (!face || !family)
    {
      g_object_unref (face);
      g_object_unref (family);
      return;
    }

  desc = pango_font_face_describe (face);
  pango_font_description_set_size (desc, fontchooser->priv->size);
  gtk_widget_override_font (fontchooser->priv->preview, desc);

  pango_font_face_list_sizes (face, &sizes, &n_sizes);
  /* It seems not many fonts actually have a sane set of sizes */
  for (i=0; i<n_sizes; i++)
    sizes[i] = sizes[i] / PANGO_SCALE;
  
  set_range_marks (fontchooser->priv, fontchooser->priv->size_slider, sizes, n_sizes);

  gtk_font_chooser_ref_family (fontchooser, family);
  gtk_font_chooser_ref_face   (fontchooser, face);

  /* Free resources */
  g_object_unref ((gpointer)family);
  g_object_unref ((gpointer)face);
  pango_font_description_free(desc);

  g_object_notify (G_OBJECT (fontchooser), "font-name");
}

gboolean
zoom_preview_cb (GtkWidget *scrolled_window, GdkEventScroll *event, gpointer data)
{
  GtkFontChooserPrivate *priv = (GtkFontChooserPrivate*)data;

  GtkAdjustment *adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (priv->size_spin));

  if (event->direction == GDK_SCROLL_UP || event->direction == GDK_SCROLL_RIGHT)
    gtk_adjustment_set_value (adj,
                              gtk_adjustment_get_value (adj) +
                              gtk_adjustment_get_step_increment (adj));
  else if (event->direction == GDK_SCROLL_DOWN || event->direction == GDK_SCROLL_LEFT)
    gtk_adjustment_set_value (adj,
                              gtk_adjustment_get_value (adj) -
                              gtk_adjustment_get_step_increment (adj));
  return TRUE;
}

static void
gtk_font_chooser_init (GtkFontChooser *fontchooser)
{
  GtkFontChooserPrivate   *priv;
  PangoFontDescription    *font_desc;
  GtkWidget               *scrolled_win;
  GtkWidget               *preview_and_size;
  GtkWidget               *size_controls;

  fontchooser->priv = G_TYPE_INSTANCE_GET_PRIVATE (fontchooser,
                                                   GTK_TYPE_FONT_CHOOSER,
                                                   GtkFontChooserPrivate);

  priv = fontchooser->priv;

  /* Default preview string  */
  priv->preview_text = g_strdup (pango_language_get_sample_string (NULL));
  priv->show_preview_entry = TRUE;

  /* Getting the default size */
  font_desc  = pango_context_get_font_description (gtk_widget_get_pango_context (GTK_WIDGET (fontchooser)));
  priv->size = pango_font_description_get_size (font_desc);
  priv->face = NULL;
  priv->family = NULL;

  gtk_widget_push_composite_child ();

  /* Creating fundamental widgets for the private struct */
  priv->search_entry = gtk_entry_new ();
  priv->family_face_list = gtk_tree_view_new ();
  priv->preview = gtk_entry_new ();
  priv->size_slider = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                                (gdouble) font_sizes[0],
                                                (gdouble) font_sizes[FONT_SIZES_LENGTH - 1],
                                                1.0);

  priv->size_spin = gtk_spin_button_new_with_range (0.0, (gdouble)(G_MAXINT / PANGO_SCALE), 1.0);

  /** Bootstrapping widget layout **/
  gtk_box_set_spacing (GTK_BOX (fontchooser), 6);
  gtk_box_pack_start (GTK_BOX (fontchooser), priv->search_entry, FALSE, TRUE, 0);

  /* Main font family/face view */
  scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_win),
                                       GTK_SHADOW_ETCHED_IN);
  gtk_container_add (GTK_CONTAINER (scrolled_win), priv->family_face_list);

  /* Alignment for the preview and size controls */
  gtk_box_pack_start (GTK_BOX (fontchooser), scrolled_win, TRUE, TRUE, 0);

  preview_and_size = gtk_vbox_new (TRUE, 0);
  gtk_box_set_homogeneous (GTK_BOX (preview_and_size), FALSE);
  gtk_box_set_spacing (GTK_BOX (preview_and_size), 6);

  /* The preview entry needs a scrolled window to make sure we have a */
  scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  priv->preview_scrolled_window = scrolled_win;
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_win),
                                       GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_win),
                                         priv->preview);
  gtk_box_pack_start (GTK_BOX (preview_and_size), scrolled_win, FALSE, FALSE, 0);
  
  /* Setting the size requests for various widgets */
  gtk_widget_set_size_request (GTK_WIDGET (fontchooser), FONT_CHOOSER_WIDTH, FONT_CHOOSER_HEIGHT);
  gtk_widget_set_size_request (scrolled_win,  -1, PREVIEW_HEIGHT);
  gtk_widget_set_size_request (priv->preview, -1, PREVIEW_HEIGHT - 6);

  /* Unset the frame on the preview entry */
  gtk_entry_set_has_frame (GTK_ENTRY (priv->preview), FALSE);

  /* Packing the slider and the spin in a hbox */
  size_controls = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_scale_set_draw_value (GTK_SCALE (priv->size_slider), FALSE);
  gtk_box_set_spacing (GTK_BOX (size_controls), 6);
  gtk_box_pack_start  (GTK_BOX (size_controls), priv->size_slider, TRUE, TRUE, 0);
  gtk_box_pack_start  (GTK_BOX (size_controls), priv->size_spin, FALSE, TRUE, 0);
  
  gtk_widget_set_valign (priv->size_spin, GTK_ALIGN_START);

  gtk_box_pack_start (GTK_BOX (preview_and_size), size_controls, FALSE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (fontchooser), GTK_WIDGET(preview_and_size), FALSE, TRUE, 0);
  
  gtk_adjustment_set_value (gtk_range_get_adjustment (GTK_RANGE (priv->size_slider)),
                            (gdouble)(priv->size / PANGO_SCALE));
  gtk_adjustment_set_value (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (priv->size_spin)),
                            (gdouble)(priv->size / PANGO_SCALE));

  gtk_widget_show_all (GTK_WIDGET (fontchooser));
  gtk_widget_hide     (GTK_WIDGET (fontchooser));

  /* Treeview column and model bootstrapping */
  gtk_font_chooser_bootstrap_fontlist (fontchooser);
  
  /* Set default preview text */
  gtk_entry_set_text (GTK_ENTRY (priv->preview),
                      pango_language_get_sample_string (NULL));
  
  /* Set search icon and place holder text */
  gtk_entry_set_icon_from_stock (GTK_ENTRY (priv->search_entry),
                                 GTK_ENTRY_ICON_SECONDARY,
                                 GTK_STOCK_FIND);
  gtk_entry_set_placeholder_text (GTK_ENTRY (priv->search_entry), _("Search font name"));
  
  /** Callback connections **/
  /* Connect to callback for the live search text entry */
  g_signal_connect (G_OBJECT (gtk_entry_get_buffer (GTK_ENTRY (priv->search_entry))),
                    "deleted-text", G_CALLBACK (deleted_text_cb), priv);
  g_signal_connect (G_OBJECT (gtk_entry_get_buffer (GTK_ENTRY (priv->search_entry))),
                    "inserted-text", G_CALLBACK (inserted_text_cb), priv);
  g_signal_connect (G_OBJECT (priv->search_entry),
                    "icon-press", G_CALLBACK (icon_press_cb), priv);

  /* Size controls callbacks */
  g_signal_connect (G_OBJECT (gtk_range_get_adjustment (GTK_RANGE (priv->size_slider))),
                    "value-changed", G_CALLBACK (slider_change_cb), priv);
  g_signal_connect (G_OBJECT (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (priv->size_spin))),
                    "value-changed", G_CALLBACK (spin_change_cb), fontchooser);
  priv->ignore_slider = FALSE;
  
  /* Font selection callback */
  g_signal_connect (G_OBJECT (priv->family_face_list), "cursor-changed",
                    G_CALLBACK (cursor_changed_cb),    fontchooser);

  /* Zoom on preview scroll*/
  g_signal_connect (G_OBJECT (scrolled_win),      "scroll-event",
                    G_CALLBACK (zoom_preview_cb), priv);

  g_signal_connect (G_OBJECT (priv->size_slider), "scroll-event",
                    G_CALLBACK (zoom_preview_cb), priv);

  set_range_marks (priv, priv->size_slider, (gint*)font_sizes, FONT_SIZES_LENGTH);

  /* Set default focus */
  gtk_widget_pop_composite_child();
}

/**
 * gtk_font_chooser_new:
 *
 * Creates a new #GtkFontChooser.
 *
 * Return value: a new #GtkFontChooser
 */
GtkWidget *
gtk_font_chooser_new (void)
{
  GtkFontChooser *fontchooser;
  
  fontchooser = g_object_new (GTK_TYPE_FONT_CHOOSER, NULL);
  
  return GTK_WIDGET (fontchooser);
}

static int
cmp_families (const void *a, const void *b)
{
  const char *a_name = pango_font_family_get_name (*(PangoFontFamily **)a);
  const char *b_name = pango_font_family_get_name (*(PangoFontFamily **)b);

  return g_utf8_collate (a_name, b_name);
}

static void 
populate_list (GtkFontChooser *fontchooser, GtkTreeView* treeview, GtkListStore* model)
{
  GtkStyleContext      *style_context;
  GdkRGBA               g_color;
  PangoColor            p_color;
  gchar                *color_string;
  PangoFontDescription *default_font;

  GtkTreeIter   match_row;
  GtkTreePath  *path;

  gint n_families, i;  
  PangoFontFamily **families;

  GString     *tmp = g_string_new (NULL);
  GString     *family_and_face = g_string_new (NULL);

  pango_context_list_families (gtk_widget_get_pango_context (GTK_WIDGET (treeview)),
                               &families,
                               &n_families);

  qsort (families, n_families, sizeof (PangoFontFamily *), cmp_families);

  gtk_list_store_clear (model);

  /* Get row header font color */
  style_context = gtk_widget_get_style_context (GTK_WIDGET (treeview));
  gtk_style_context_get_color (style_context,
                               GTK_STATE_FLAG_NORMAL | GTK_STATE_FLAG_INSENSITIVE,
                               &g_color);

  p_color.red   = (guint16)((gdouble)G_MAXUINT16 * g_color.red);
  p_color.green = (guint16)((gdouble)G_MAXUINT16 * g_color.green);
  p_color.blue  = (guint16)((gdouble)G_MAXUINT16 * g_color.blue);
  color_string  = pango_color_to_string (&p_color);

  /* Get theme font */
  default_font  = (PangoFontDescription*) gtk_style_context_get_font (style_context,
                                                                      GTK_STATE_NORMAL);

  /* Iterate over families and faces */
  for (i=0; i<n_families; i++)
    {
      GtkTreeIter     iter;
      PangoFontFace **faces;
      
      int             j, n_faces;
      const gchar    *fam_name = pango_font_family_get_name (families[i]);

      pango_font_family_list_faces (families[i], &faces, &n_faces);
      
      for (j=0; j<n_faces; j++)
        {
          PangoFontDescription *pango_desc = pango_font_face_describe (faces[j]);
          const gchar *face_name = pango_font_face_get_face_name (faces[j]);
          gchar       *font_desc = pango_font_description_to_string (pango_desc);
          
          /* foreground_color, family_name, face_name, desc, sample string */
          g_string_printf (family_and_face, "%s %s",
                                            fam_name,
                                            face_name);
          
          g_string_printf (tmp, ROW_FORMAT_STRING,
                           color_string,
                           family_and_face->str,
                           font_desc,
                           fontchooser->priv->preview_text);

          gtk_list_store_append (model, &iter);
          gtk_list_store_set (model, &iter,
                              FAMILY_COLUMN, families[i],
                              FACE_COLUMN, faces[j],
                              PREVIEW_TITLE_COLUMN, family_and_face->str,
                              PREVIEW_TEXT_COLUMN, tmp->str,
                              -1);

          /* Select the first font or the default font/face from the style context */
          if ((i == 0 && j == 0) ||
              (!strcmp (fam_name, pango_font_description_get_family (default_font)) && j == 0))
            match_row = iter;

          pango_font_description_free(pango_desc);
          g_free (font_desc);
        }

      g_free (faces);
    }

  path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &match_row);

  if (path)
    {
      gtk_tree_view_set_cursor (treeview, path, NULL, FALSE);
      gtk_tree_view_scroll_to_cell (treeview, path, NULL, FALSE, 0.5, 0.5);
      gtk_tree_path_free(path);
    }

  g_string_free (family_and_face, TRUE);
  g_string_free (tmp, TRUE);
  g_free (color_string);
  g_free (families);
}

gboolean
visible_func (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
  gboolean               result = FALSE;
  GtkFontChooserPrivate *priv = (GtkFontChooserPrivate*) data;

  const gchar *search_text = (const gchar*)gtk_entry_get_text (GTK_ENTRY (priv->search_entry));
  gchar       *font_name;
  gchar       *font_name_casefold;
  gchar       *search_text_casefold;

  gtk_tree_model_get (model, iter,
                      PREVIEW_TITLE_COLUMN, &font_name,
                      -1);

  /* Covering some corner cases to speed up the result */
  if (font_name == NULL ||
      strlen (search_text) > strlen (font_name))
    {
      g_free (font_name);
      return FALSE;
    }  
  if (strlen (search_text) == 0)
    {
      g_free (font_name);
      return TRUE;
    }
  
  font_name_casefold = g_utf8_casefold (font_name, -1);
  search_text_casefold = g_utf8_casefold (search_text, -1);
  
  if (g_strrstr (font_name_casefold, search_text_casefold))
    result = TRUE;

  g_free (search_text_casefold);
  g_free (font_name_casefold);
  g_free (font_name);
  return result;
}

static void
gtk_font_chooser_bootstrap_fontlist (GtkFontChooser* fontchooser)
{
  GtkTreeView       *treeview = GTK_TREE_VIEW (fontchooser->priv->family_face_list);
  GtkCellRenderer   *cell;
  GtkTreeViewColumn *col;

  fontchooser->priv->model = gtk_list_store_new (4,
                                             PANGO_TYPE_FONT_FAMILY,
                                             PANGO_TYPE_FONT_FACE,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING);

  fontchooser->priv->filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (fontchooser->priv->model),
                                                     NULL);
  g_object_unref (fontchooser->priv->model);

  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (fontchooser->priv->filter),
                                          visible_func,
                                          (gpointer)fontchooser->priv,
                                          NULL);

  gtk_tree_view_set_model (treeview, GTK_TREE_MODEL (fontchooser->priv->filter));
  g_object_unref (fontchooser->priv->filter);

  gtk_tree_view_set_rules_hint      (treeview, TRUE);
  gtk_tree_view_set_headers_visible (treeview, FALSE);

  cell = gtk_cell_renderer_text_new ();
  col = gtk_tree_view_column_new_with_attributes ("Family",
                                                  cell,
                                                  "markup", PREVIEW_TEXT_COLUMN,
                                                  NULL);
                                                  
  g_object_set (cell, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

  gtk_tree_view_append_column (treeview, col);

  populate_list (fontchooser, treeview, fontchooser->priv->model);
}


static void
gtk_font_chooser_finalize (GObject *object)
{
  GtkFontChooser *fontchooser = GTK_FONT_CHOOSER (object);

  gtk_font_chooser_ref_family (fontchooser, NULL);
  gtk_font_chooser_ref_face (fontchooser, NULL);

  G_OBJECT_CLASS (gtk_font_chooser_parent_class)->finalize (object);
}


static void
gtk_font_chooser_screen_changed (GtkWidget *widget,
                                   GdkScreen *previous_screen)
{
  GtkFontChooser *fontchooser = GTK_FONT_CHOOSER (widget);

  populate_list (fontchooser,
                 GTK_TREE_VIEW (fontchooser->priv->family_face_list),
                 fontchooser->priv->model);
  return;
}

static void
gtk_font_chooser_style_updated (GtkWidget *widget)
{
  GtkFontChooser *fontchooser = GTK_FONT_CHOOSER (widget);

  GTK_WIDGET_CLASS (gtk_font_chooser_parent_class)->style_updated (widget);

  populate_list (fontchooser,
                 GTK_TREE_VIEW (fontchooser->priv->family_face_list),
                 fontchooser->priv->model);
  return;
}

static void
gtk_font_chooser_ref_family (GtkFontChooser *fontchooser,
                               PangoFontFamily  *family)
{
  GtkFontChooserPrivate *priv = fontchooser->priv;

  if (family)
    family = g_object_ref (family);
  if (priv->family)
    g_object_unref (priv->family);
  priv->family = family;
}

static void
gtk_font_chooser_ref_face (GtkFontChooser *fontchooser,
                             PangoFontFace    *face)
{
  GtkFontChooserPrivate *priv = fontchooser->priv;

  if (face)
    face = g_object_ref (face);
  if (priv->face)
    g_object_unref (priv->face);
  priv->face = face;
}


/*****************************************************************************
 * These functions are the main public interface for getting/setting the font.
 *****************************************************************************/

/**
 * gtk_font_chooser_get_family:
 * @fontchooser: a #GtkFontChooser
 *
 * Gets the #PangoFontFamily representing the selected font family.
 *
 * Return value: (transfer none): A #PangoFontFamily representing the
 *     selected font family. Font families are a collection of font
 *     faces. The returned object is owned by @fontchooser and must not
 *     be modified or freed.
 *
 * Since: 3.2
 */
PangoFontFamily *
gtk_font_chooser_get_family (GtkFontChooser *fontchooser)
{
  g_return_val_if_fail (GTK_IS_FONT_CHOOSER (fontchooser), NULL);

  return fontchooser->priv->family;
}

/**
 * gtk_font_chooser_get_face:
 * @fontchooser: a #GtkFontChooser
 *
 * Gets the #PangoFontFace representing the selected font group
 * details (i.e. family, slant, weight, width, etc).
 *
 * Return value: (transfer none): A #PangoFontFace representing the
 *     selected font group details. The returned object is owned by
 *     @fontchooser and must not be modified or freed.
 *
 * Since: 3.2
 */
PangoFontFace *
gtk_font_chooser_get_face (GtkFontChooser *fontchooser)
{
  g_return_val_if_fail (GTK_IS_FONT_CHOOSER (fontchooser), NULL);

  return fontchooser->priv->face;
}

/**
 * gtk_font_chooser_get_size:
 * @fontchooser: a #GtkFontChooser
 *
 * The selected font size.
 *
 * Return value: A n integer representing the selected font size,
 *     or -1 if no font size is selected.
 *
 * Since: 3.2
 **/
gint
gtk_font_chooser_get_size (GtkFontChooser *fontchooser)
{
  g_return_val_if_fail (GTK_IS_FONT_CHOOSER (fontchooser), -1);

  return fontchooser->priv->size;
}

/**
 * gtk_font_chooser_get_font_name:
 * @fontchooser: a #GtkFontChooser
 * 
 * Gets the currently-selected font name. 
 *
 * Note that this can be a different string than what you set with 
 * gtk_font_chooser_set_font_name(), as the font chooser widget may 
 * normalize font names and thus return a string with a different structure. 
 * For example, "Helvetica Italic Bold 12" could be normalized to 
 * "Helvetica Bold Italic 12". Use pango_font_description_equal()
 * if you want to compare two font descriptions.
 * 
 * Return value: (transfer full) (allow-none): A string with the name of the
 *     current font, or %NULL if  no font is selected. You must free this
 *     string with g_free().
 *
 * Since: 3.2
 */
gchar *
gtk_font_chooser_get_font_name (GtkFontChooser *fontchooser)
{
  gchar                *font_name;
  PangoFontDescription *desc;

  if (!fontchooser->priv->face)
    return NULL;

  desc = pango_font_face_describe (fontchooser->priv->face);
  font_name = pango_font_description_to_string (desc);
  pango_font_description_free (desc);
  return font_name;
}

/* This sets the current font, then selecting the appropriate list rows. */

/**
 * gtk_font_chooser_set_font_name:
 * @fontchooser: a #GtkFontChooser
 * @fontname: a font name like "Helvetica 12" or "Times Bold 18"
 * 
 * Sets the currently-selected font. 
 *
 * Note that the @fontchooser needs to know the screen in which it will appear 
 * for this to work; this can be guaranteed by simply making sure that the 
 * @fontchooser is inserted in a toplevel window before you call this function.
 * 
 * Return value: %TRUE if the font could be set successfully; %FALSE if no 
 *     such font exists or if the @fontchooser doesn't belong to a particular 
 *     screen yet.
 *
 * Since: 3.2
 */
gboolean
gtk_font_chooser_set_font_name (GtkFontChooser *fontchooser,
                                  const gchar      *fontname)
{
  GtkFontChooserPrivate *priv = fontchooser->priv;
  GtkTreeIter           iter;
  gboolean              valid;
  gchar                *family_name;
  PangoFontDescription *desc;
  gboolean              found = FALSE;
  
  g_return_val_if_fail (GTK_IS_FONT_CHOOSER (fontchooser), FALSE);
  g_return_val_if_fail (fontname != NULL, FALSE);

  if (!gtk_widget_has_screen (GTK_WIDGET (fontchooser)))
    return FALSE;

  desc = pango_font_description_from_string (fontname);
  family_name = (gchar*)pango_font_description_get_family (desc);

  if (!family_name)
  {
    pango_font_description_free (desc);
    return FALSE;
  }

  /* We make sure the filter is clear */
  gtk_entry_set_text (GTK_ENTRY (priv->search_entry), "");

  /* We find the matching family/face */
  for (valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->filter), &iter);
       valid;
       valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->filter), &iter))
    {
      PangoFontFace        *face;
      PangoFontDescription *tmp_desc;

      gtk_tree_model_get (GTK_TREE_MODEL (priv->filter), &iter,
                          FACE_COLUMN,   &face,
                          -1);

      tmp_desc = pango_font_face_describe (face);
      if (pango_font_description_get_size_is_absolute (desc))
        pango_font_description_set_absolute_size (tmp_desc,
                                                  pango_font_description_get_size (desc));
      else
        pango_font_description_set_size (tmp_desc,
                                         pango_font_description_get_size (desc));        


      if (pango_font_description_equal (desc, tmp_desc))
        {
          GtkTreePath *path;
          gint size = pango_font_description_get_size (desc);

          if (size)
            {
              if (pango_font_description_get_size_is_absolute (desc))
                size = size * PANGO_SCALE;
              gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->size_spin),
                                         size / PANGO_SCALE);
            }

          path = gtk_tree_model_get_path (GTK_TREE_MODEL (priv->filter),
                                          &iter);

          if (path)
            {
              gtk_tree_view_set_cursor (GTK_TREE_VIEW (priv->family_face_list),
                                        path,
                                        NULL,
                                        FALSE);
              gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (priv->family_face_list),
                                            path,
                                            NULL,
                                            FALSE,
                                            0.5,
                                            0.5);
              gtk_tree_path_free (path);
            }

          found = TRUE;
        }

      g_object_unref (face);
      pango_font_description_free (tmp_desc);

      if (found)
        break;
    }

  pango_font_description_free (desc);
  g_object_notify (G_OBJECT (fontchooser), "font-name");

  return found;
}

/**
 * gtk_font_chooser_get_preview_text:
 * @fontchooser: a #GtkFontChooser
 *
 * Gets the text displayed in the preview area.
 * 
 * Return value: (transfer none): the text displayed in the
 *     preview area. This string is owned by the widget and
 *     should not be modified or freed
 *
 * Since: 3.2
 */
G_CONST_RETURN gchar*
gtk_font_chooser_get_preview_text (GtkFontChooser *fontchooser)
{
  g_return_val_if_fail (GTK_IS_FONT_CHOOSER (fontchooser), NULL);
  return (const gchar*)fontchooser->priv->preview_text;
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
gtk_font_chooser_set_preview_text  (GtkFontChooser *fontchooser,
                                      const gchar      *text)
{
  g_return_if_fail (GTK_IS_FONT_CHOOSER (fontchooser));
  g_return_if_fail (text != NULL);

  g_free (fontchooser->priv->preview_text);
  fontchooser->priv->preview_text = g_strdup (text);

  populate_list (fontchooser,
                 GTK_TREE_VIEW (fontchooser->priv->family_face_list),
                 fontchooser->priv->model);

  gtk_entry_set_text (GTK_ENTRY (fontchooser->priv->preview), text);

  g_object_notify (G_OBJECT (fontchooser), "preview-text");
}

/**
 * gtk_font_chooser_get_show_preview_entry:
 * @fontchooser: a #GtkFontChooser
 *
 * Return value: %TRUE if the preview entry is shown or %FALSE if
 *               it is hidden.
 * Since: 3.2
 */
gboolean
gtk_font_chooser_get_show_preview_entry (GtkFontChooser *fontchooser)
{
  g_return_val_if_fail (GTK_IS_FONT_CHOOSER (fontchooser), FALSE);

  return fontchooser->priv->show_preview_entry;
}

/**
 * gtk_font_chooser_set_show_preview_entry:
 * @fontchooser: a #GtkFontChooser
 * @show_preview_entry: whether to show the editable preview entry or not
 *
 * Shows or hides the editable preview entry.
 * Since: 3.2
 */
void
gtk_font_chooser_set_show_preview_entry (GtkFontChooser *fontchooser,
                                           gboolean          show_preview_entry)
{
  g_return_if_fail (GTK_IS_FONT_CHOOSER (fontchooser));

  if (show_preview_entry)
    gtk_widget_show (fontchooser->priv->preview_scrolled_window);
  else
    gtk_widget_hide (fontchooser->priv->preview_scrolled_window);

  fontchooser->priv->show_preview_entry = show_preview_entry;
  g_object_notify (G_OBJECT (fontchooser), "show-preview-entry");
}


/**
 * SECTION:gtkfontchooserdlg
 * @Short_description: A dialog box for selecting fonts
 * @Title: GtkFontChooserDialog
 * @See_also: #GtkFontChooser, #GtkDialog
 *
 * The #GtkFontChooserDialog widget is a dialog box for selecting a font.
 *
 * To set the font which is initially selected, use
 * gtk_font_chooser_dialog_set_font_name().
 *
 * To get the selected font use gtk_font_chooser_dialog_get_font_name().
 *
 * To change the text which is shown in the preview area, use
 * gtk_font_chooser_dialog_set_preview_text().
 *
 * <refsect2 id="GtkFontChooserDialog-BUILDER-UI">
 * <title>GtkFontChooserDialog as GtkBuildable</title>
 * The GtkFontChooserDialog implementation of the GtkBuildable interface
 * exposes the embedded #GtkFontChooser as internal child with the
 * name "font_chooser". It also exposes the buttons with the names
 * "select_button" and "cancel_button. The buttons with the names 
 * "ok_button" and "apply_button" are exposed but deprecated.
 * </refsect2>
 */

static void gtk_font_chooser_dialog_buildable_interface_init     (GtkBuildableIface *iface);
static GObject * gtk_font_chooser_dialog_buildable_get_internal_child (GtkBuildable *buildable,
                    GtkBuilder   *builder,
                    const gchar  *childname);

G_DEFINE_TYPE_WITH_CODE (GtkFontChooserDialog, gtk_font_chooser_dialog,
       GTK_TYPE_DIALOG,
       G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
            gtk_font_chooser_dialog_buildable_interface_init))

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_font_chooser_dialog_class_init (GtkFontChooserDialogClass *klass)
{
  g_type_class_add_private (klass, sizeof (GtkFontChooserDialogPrivate));
}

static void
gtk_font_chooser_dialog_init (GtkFontChooserDialog *fontchooserdiag)
{
  GtkFontChooserDialogPrivate *priv;
  GtkDialog *dialog = GTK_DIALOG (fontchooserdiag);
  GtkWidget *action_area, *content_area;

  fontchooserdiag->priv = G_TYPE_INSTANCE_GET_PRIVATE (fontchooserdiag,
                                                   GTK_TYPE_FONT_CHOOSER_DIALOG,
                                                   GtkFontChooserDialogPrivate);
  priv = fontchooserdiag->priv;

  content_area = gtk_dialog_get_content_area (dialog);
  action_area = gtk_dialog_get_action_area (dialog);

  gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
  gtk_box_set_spacing (GTK_BOX (content_area), 2); /* 2 * 5 + 2 = 12 */
  gtk_container_set_border_width (GTK_CONTAINER (action_area), 5);
  gtk_box_set_spacing (GTK_BOX (action_area), 6);

  gtk_widget_push_composite_child ();

  gtk_window_set_resizable (GTK_WINDOW (fontchooserdiag), TRUE);

  /* Create the content area */
  priv->fontchooser = gtk_font_chooser_new ();
  gtk_container_set_border_width (GTK_CONTAINER (priv->fontchooser), 5);
  gtk_widget_show (priv->fontchooser);
  gtk_box_pack_start (GTK_BOX (content_area),
                      priv->fontchooser, TRUE, TRUE, 0);

  /* Create the action area */
  priv->cancel_button = gtk_dialog_add_button (dialog,
                                               GTK_STOCK_CANCEL,
                                               GTK_RESPONSE_CANCEL);
  priv->select_button = gtk_dialog_add_button (dialog,
                                               _("Select"),
                                               GTK_RESPONSE_OK);
  gtk_widget_grab_default (priv->select_button);

  gtk_dialog_set_alternative_button_order (GTK_DIALOG (fontchooserdiag),
             GTK_RESPONSE_OK,
             GTK_RESPONSE_CANCEL,
             -1);

  gtk_window_set_title (GTK_WINDOW (fontchooserdiag),
                        _("Font Selection"));

  gtk_widget_pop_composite_child ();
}

/**
 * gtk_font_chooser_dialog_new:
 * @title: (allow-none): the title of the dialog window 
 *
 * Creates a new #GtkFontChooserDialog.
 *
 * Return value: a new #GtkFontChooserDialog
 */
GtkWidget*
gtk_font_chooser_dialog_new (const gchar *title)
{
  GtkFontChooserDialog *fontchooserdiag;
  
  fontchooserdiag = g_object_new (GTK_TYPE_FONT_CHOOSER_DIALOG, NULL);

  if (title)
    gtk_window_set_title (GTK_WINDOW (fontchooserdiag), title);
  
  return GTK_WIDGET (fontchooserdiag);
}

/**
 * gtk_font_chooser_dialog_get_font_chooser:
 * @fcd: a #GtkFontChooserDialog
 *
 * Retrieves the #GtkFontChooser widget embedded in the dialog.
 *
 * Returns: (transfer none): the embedded #GtkFontChooser
 *
 * Since: 2.22
 **/
GtkWidget*
gtk_font_chooser_dialog_get_font_chooser (GtkFontChooserDialog *fcd)
{
  g_return_val_if_fail (GTK_IS_FONT_CHOOSER_DIALOG (fcd), NULL);

  return fcd->priv->fontchooser;
}

static void
gtk_font_chooser_dialog_buildable_interface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->get_internal_child = gtk_font_chooser_dialog_buildable_get_internal_child;
}

static GObject *
gtk_font_chooser_dialog_buildable_get_internal_child (GtkBuildable *buildable,
              GtkBuilder   *builder,
              const gchar  *childname)
{
  GtkFontChooserDialogPrivate *priv;

  priv = GTK_FONT_CHOOSER_DIALOG (buildable)->priv;

  if (g_strcmp0 (childname, "select_button") == 0)
    return G_OBJECT (priv->select_button);
  else if (g_strcmp0 (childname, "cancel_button") == 0)
    return G_OBJECT (priv->cancel_button);
  else if (g_strcmp0 (childname, "font_chooser") == 0)
    return G_OBJECT (priv->fontchooser);

  return parent_buildable_iface->get_internal_child (buildable, builder, childname);
}

/**
 * gtk_font_chooser_dialog_get_font_name:
 * @fcd: a #GtkFontChooserDialog
 * 
 * Gets the currently-selected font name.
 *
 * Note that this can be a different string than what you set with 
 * gtk_font_chooser_dialog_set_font_name(), as the font chooser widget
 * may normalize font names and thus return a string with a different 
 * structure. For example, "Helvetica Italic Bold 12" could be normalized 
 * to "Helvetica Bold Italic 12".  Use pango_font_description_equal()
 * if you want to compare two font descriptions.
 * 
 * Return value: A string with the name of the current font, or %NULL if no 
 *     font is selected. You must free this string with g_free().
 */
gchar*
gtk_font_chooser_dialog_get_font_name (GtkFontChooserDialog *fcd)
{
  GtkFontChooserDialogPrivate *priv;

  g_return_val_if_fail (GTK_IS_FONT_CHOOSER_DIALOG (fcd), NULL);

  priv = fcd->priv;

  return gtk_font_chooser_get_font_name (GTK_FONT_CHOOSER (priv->fontchooser));
}

/**
 * gtk_font_chooser_dialog_set_font_name:
 * @fcd: a #GtkFontChooserDialog
 * @fontname: a font name like "Helvetica 12" or "Times Bold 18"
 *
 * Sets the currently selected font. 
 * 
 * Return value: %TRUE if the font selected in @fcd is now the
 *     @fontname specified, %FALSE otherwise. 
 */
gboolean
gtk_font_chooser_dialog_set_font_name (GtkFontChooserDialog *fcd,
           const gchar          *fontname)
{
  GtkFontChooserDialogPrivate *priv;

  g_return_val_if_fail (GTK_IS_FONT_CHOOSER_DIALOG (fcd), FALSE);
  g_return_val_if_fail (fontname, FALSE);

  priv = fcd->priv;

  return gtk_font_chooser_set_font_name (GTK_FONT_CHOOSER (priv->fontchooser), fontname);
}

/**
 * gtk_font_chooser_dialog_get_preview_text:
 * @fcd: a #GtkFontChooserDialog
 *
 * Gets the text displayed in the preview area.
 * 
 * Return value: the text displayed in the preview area. 
 *     This string is owned by the widget and should not be 
 *     modified or freed 
 */
G_CONST_RETURN gchar*
gtk_font_chooser_dialog_get_preview_text (GtkFontChooserDialog *fcd)
{
  GtkFontChooserDialogPrivate *priv;

  g_return_val_if_fail (GTK_IS_FONT_CHOOSER_DIALOG (fcd), NULL);

  priv = fcd->priv;

  return gtk_font_chooser_get_preview_text (GTK_FONT_CHOOSER (priv->fontchooser));
}

/**
 * gtk_font_chooser_dialog_set_preview_text:
 * @fcd: a #GtkFontChooserDialog
 * @text: the text to display in the preview area
 *
 * Sets the text displayed in the preview area. 
 */
void
gtk_font_chooser_dialog_set_preview_text (GtkFontChooserDialog *fcd,
              const gchar            *text)
{
  GtkFontChooserDialogPrivate *priv;

  g_return_if_fail (GTK_IS_FONT_CHOOSER_DIALOG (fcd));
  g_return_if_fail (text != NULL);

  priv = fcd->priv;

  gtk_font_chooser_set_preview_text (GTK_FONT_CHOOSER (priv->fontchooser), text);
}
