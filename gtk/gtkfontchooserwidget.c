/* GTK - The GIMP Toolkit
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <stdlib.h>
#include <glib/gprintf.h>
#include <string.h>

#include <atk/atk.h>

#include "gtkfontchooserwidget.h"
#include "gtkfontchooser.h"
#include "gtkfontchooserutils.h"
#include "gtkcellrenderertext.h"
#include "gtkentry.h"
#include "gtkframe.h"
#include "gtkbbox.h"
#include "gtkbox.h"
#include "gtklabel.h"
#include "gtkliststore.h"
#include "gtkstock.h"
#include "gtktextview.h"
#include "gtktreeselection.h"
#include "gtktreeview.h"
#include "gtkscrolledwindow.h"
#include "gtkintl.h"
#include "gtkaccessible.h"
#include "gtkbuildable.h"
#include "gtkprivate.h"
#include "gtkscale.h"
#include "gtkspinbutton.h"
#include "gtknotebook.h"
#include "gtkwidget.h"
#include "gtkgrid.h"

/**
 * SECTION:gtkfontchooser
 * @Short_description: A widget for selecting fonts
 * @Title: GtkFontChooserWidget
 * @See_also: #GtkFontChooserDialog
 *
 * The #GtkFontChooserWidget widget lists the available fonts,
 * styles and sizes, allowing the user to select a font. It is
 * used in the #GtkFontChooserDialog widget to provide a
 * dialog box for selecting fonts.
 *
 * To set the font which is initially selected, use
 * gtk_font_chooser_set_font() or gtk_font_chooser_set_font_desc().
 *
 * To get the selected font use gtk_font_chooser_get_font() or
 * gtk_font_chooser_get_font_desc().
 *
 * To change the text which is shown in the preview area, use
 * gtk_font_chooser_set_preview_text().
 *
 * Since: 3.2
 */


struct _GtkFontChooserWidgetPrivate
{
  GtkWidget    *search_entry;
  GtkWidget    *family_face_list;
  GtkWidget    *list_scrolled_window;
  GtkWidget    *empty_list;
  GtkWidget    *list_notebook;
  GtkListStore *model;
  GtkTreeModel *filter_model;

  GtkWidget       *preview;
  gchar           *preview_text;
  gboolean         show_preview_entry;

  GtkWidget *size_spin;
  GtkWidget *size_slider;

  PangoFontDescription *font_desc;
  gint             size;
  PangoFontFace   *face;
  PangoFontFamily *family;

  gulong           cursor_changed_handler;

  GtkFontFilterFunc filter_func;
  gpointer          filter_data;
  GDestroyNotify    filter_data_destroy;
};

/* This is the initial fixed height and the top padding of the preview entry */
#define PREVIEW_HEIGHT 72
#define PREVIEW_TOP_PADDING 6

/* These are the sizes of the font, style & size lists. */
#define FONT_LIST_HEIGHT  136
#define FONT_LIST_WIDTH   190
#define FONT_STYLE_LIST_WIDTH 170
#define FONT_SIZE_LIST_WIDTH  60

#define ROW_FORMAT_STRING "<span weight=\"bold\" size=\"small\">%s</span>\n<span size=\"x-large\" font_desc=\"%s\">%s</span>"

#define NO_FONT_MATCHED_SEARCH N_("No fonts matched your search. You can revise your search and try again.")

/* These are what we use as the standard font sizes, for the size list.
 */
static const gint font_sizes[] = {
  6, 8, 9, 10, 11, 12, 13, 14, 16, 20, 24, 36, 48, 72
};

enum {
  FAMILY_COLUMN,
  FACE_COLUMN,
  PREVIEW_TEXT_COLUMN,
  PREVIEW_TITLE_COLUMN
};

static void gtk_font_chooser_widget_set_property         (GObject         *object,
                                                          guint            prop_id,
                                                          const GValue    *value,
                                                          GParamSpec      *pspec);
static void gtk_font_chooser_widget_get_property         (GObject         *object,
                                                          guint            prop_id,
                                                          GValue          *value,
                                                          GParamSpec      *pspec);
static void gtk_font_chooser_widget_finalize             (GObject         *object);
static void gtk_font_chooser_widget_dispose              (GObject         *object);

static void gtk_font_chooser_widget_screen_changed       (GtkWidget       *widget,
                                                          GdkScreen       *previous_screen);
static void gtk_font_chooser_widget_style_updated        (GtkWidget       *widget);

static void gtk_font_chooser_widget_bootstrap_fontlist   (GtkFontChooserWidget *fontchooser);

static void gtk_font_chooser_widget_select_font          (GtkFontChooserWidget *fontchooser);

static gchar   *gtk_font_chooser_widget_get_font         (GtkFontChooserWidget *fontchooser);
static void     gtk_font_chooser_widget_set_font         (GtkFontChooserWidget *fontchooser,
                                                          const gchar          *fontname);

static PangoFontDescription *gtk_font_chooser_widget_get_font_desc  (GtkFontChooserWidget *fontchooser);
static void                  gtk_font_chooser_widget_take_font_desc (GtkFontChooserWidget *fontchooser,
                                                                     PangoFontDescription *font_desc);


static const gchar *gtk_font_chooser_widget_get_preview_text (GtkFontChooserWidget *fontchooser);
static void         gtk_font_chooser_widget_set_preview_text (GtkFontChooserWidget *fontchooser,
                                                              const gchar          *text);

static gboolean gtk_font_chooser_widget_get_show_preview_entry (GtkFontChooserWidget *fontchooser);
static void     gtk_font_chooser_widget_set_show_preview_entry (GtkFontChooserWidget *fontchooser,
                                                                gboolean              show_preview_entry);

static void gtk_font_chooser_widget_iface_init (GtkFontChooserIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkFontChooserWidget, gtk_font_chooser_widget, GTK_TYPE_BOX,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_FONT_CHOOSER,
                                                gtk_font_chooser_widget_iface_init))

static void
gtk_font_chooser_widget_class_init (GtkFontChooserWidgetClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->screen_changed = gtk_font_chooser_widget_screen_changed;
  widget_class->style_updated = gtk_font_chooser_widget_style_updated;

  gobject_class->dispose = gtk_font_chooser_widget_dispose;
  gobject_class->finalize = gtk_font_chooser_widget_finalize;
  gobject_class->set_property = gtk_font_chooser_widget_set_property;
  gobject_class->get_property = gtk_font_chooser_widget_get_property;

  _gtk_font_chooser_install_properties (gobject_class);

  g_type_class_add_private (klass, sizeof (GtkFontChooserWidgetPrivate));
}

static void
gtk_font_chooser_widget_set_property (GObject         *object,
                                      guint            prop_id,
                                      const GValue    *value,
                                      GParamSpec      *pspec)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (object);

  switch (prop_id)
    {
    case GTK_FONT_CHOOSER_PROP_FONT:
      gtk_font_chooser_widget_set_font (fontchooser, g_value_get_string (value));
      break;
    case GTK_FONT_CHOOSER_PROP_FONT_DESC:
      gtk_font_chooser_widget_take_font_desc (fontchooser, g_value_dup_boxed (value));
      break;
    case GTK_FONT_CHOOSER_PROP_PREVIEW_TEXT:
      gtk_font_chooser_widget_set_preview_text (fontchooser, g_value_get_string (value));
      break;
    case GTK_FONT_CHOOSER_PROP_SHOW_PREVIEW_ENTRY:
      gtk_font_chooser_widget_set_show_preview_entry (fontchooser, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_font_chooser_widget_get_property (GObject         *object,
                                      guint            prop_id,
                                      GValue          *value,
                                      GParamSpec      *pspec)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (object);

  switch (prop_id)
    {
    case GTK_FONT_CHOOSER_PROP_FONT:
      g_value_take_string (value, gtk_font_chooser_widget_get_font (fontchooser));
      break;
    case GTK_FONT_CHOOSER_PROP_FONT_DESC:
      g_value_set_boxed (value, gtk_font_chooser_widget_get_font_desc (fontchooser));
      break;
    case GTK_FONT_CHOOSER_PROP_PREVIEW_TEXT:
      g_value_set_string (value, gtk_font_chooser_widget_get_preview_text (fontchooser));
      break;
    case GTK_FONT_CHOOSER_PROP_SHOW_PREVIEW_ENTRY:
      g_value_set_boolean (value, gtk_font_chooser_widget_get_show_preview_entry (fontchooser));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
text_changed_cb (GtkEntry       *entry,
                 GParamSpec     *pspec,
                 GtkFontChooserWidget *fc)
{
  GtkFontChooserWidgetPrivate *priv = fc->priv;
  const gchar *text;

  text = gtk_entry_get_text (entry);

  if (text == NULL || text[0] == '\0')
    {
      GIcon *icon;

      icon = g_themed_icon_new_with_default_fallbacks ("edit-find-symbolic");
      g_object_set (G_OBJECT (priv->search_entry),
                    "secondary-icon-gicon", icon,
                    "secondary-icon-activatable", FALSE,
                    "secondary-icon-sensitive", FALSE,
                    NULL);
      g_object_unref (icon);
    }
  else
    {
      if (!gtk_entry_get_icon_activatable (GTK_ENTRY (priv->search_entry), GTK_ENTRY_ICON_SECONDARY))
        {
          GIcon *icon;

          icon = g_themed_icon_new_with_default_fallbacks ("edit-clear-symbolic");
          g_object_set (G_OBJECT (priv->search_entry),
                        "secondary-icon-gicon", icon,
                        "secondary-icon-activatable", TRUE,
                        "secondary-icon-sensitive", TRUE,
                        NULL);
          g_object_unref (icon);
        }
    }

  gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (priv->filter_model));
}

static void
icon_press_cb (GtkEntry             *entry,
               GtkEntryIconPosition  pos,
               GdkEvent             *event,
               gpointer              user_data)
{
  gtk_entry_set_text (entry, "");
}

static void
slider_change_cb (GtkAdjustment *adjustment,
                  gpointer       user_data)
{
  GtkFontChooserWidget        *fc    = (GtkFontChooserWidget*)user_data;
  GtkFontChooserWidgetPrivate *priv  = fc->priv;
  GtkAdjustment         *spin_adj     = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON (priv->size_spin));
  gdouble                slider_value = gtk_adjustment_get_value (adjustment);
  gdouble                spin_value   = gtk_adjustment_get_value (spin_adj);

  if (slider_value != spin_value)
    gtk_adjustment_set_value (spin_adj,
                              gtk_adjustment_get_value (adjustment));
}

static void
spin_change_cb (GtkAdjustment *adjustment,
                gpointer       user_data)
{
  PangoFontDescription    *desc;
  GtkFontChooserWidget          *fontchooser = (GtkFontChooserWidget*)user_data;
  GtkFontChooserWidgetPrivate   *priv        = fontchooser->priv;
  GtkAdjustment           *slider_adj  = gtk_range_get_adjustment (GTK_RANGE (priv->size_slider));
  gdouble size = gtk_adjustment_get_value (adjustment);

  priv->size = ((gint)size) * PANGO_SCALE;

  desc = pango_context_get_font_description (gtk_widget_get_pango_context (priv->preview));
  pango_font_description_set_size (desc, priv->size);
  gtk_widget_override_font (priv->preview, desc);

  if (pango_font_description_get_size_is_absolute (priv->font_desc))
    pango_font_description_set_absolute_size (priv->font_desc, size);
  else
    pango_font_description_set_size (priv->font_desc, priv->size);

  /* If the new value is lower than the lower bound of the slider, we set
   * the slider adjustment to the lower bound value if it is not already set
   */
  if (size < gtk_adjustment_get_lower (slider_adj) &&
      gtk_adjustment_get_value (slider_adj) != gtk_adjustment_get_lower (slider_adj))
    gtk_adjustment_set_value (slider_adj, gtk_adjustment_get_lower (slider_adj));

  /* If the new value is upper than the upper bound of the slider, we set
   * the slider adjustment to the upper bound value if it is not already set
   */
  else if (size > gtk_adjustment_get_upper (slider_adj) &&
           gtk_adjustment_get_value (slider_adj) != gtk_adjustment_get_upper (slider_adj))
    gtk_adjustment_set_value (slider_adj, gtk_adjustment_get_upper (slider_adj));

  /* If the new value is not already set on the slider we set it */
  else if (size != gtk_adjustment_get_value (slider_adj))
    gtk_adjustment_set_value (slider_adj, size);

  gtk_widget_queue_draw (priv->preview);

  g_object_notify (G_OBJECT (fontchooser), "font");
  g_object_notify (G_OBJECT (fontchooser), "font-desc");
}

static void
set_range_marks (GtkFontChooserWidgetPrivate *priv,
                 GtkWidget             *size_slider,
                 gint                  *sizes,
                 gint                   length)
{
  GtkAdjustment *adj;
  gint i;
  gdouble value;

  if (length < 2)
    {
      sizes = (gint*)font_sizes;
      length = G_N_ELEMENTS (font_sizes);
    }

  gtk_scale_clear_marks (GTK_SCALE (size_slider));

  adj = gtk_range_get_adjustment(GTK_RANGE (size_slider));

  gtk_adjustment_set_lower (adj, (gdouble) sizes[0]);
  gtk_adjustment_set_upper (adj, (gdouble) sizes[length-1]);

  value = gtk_adjustment_get_value (adj);
  if (value > (gdouble) sizes[length-1])
    gtk_adjustment_set_value (adj, (gdouble) sizes[length-1]);
  else if (value < (gdouble) sizes[0])
    gtk_adjustment_set_value (adj, (gdouble) sizes[0]);

  for (i = 0; i < length; i++)
    gtk_scale_add_mark (GTK_SCALE (size_slider),
                        (gdouble) sizes[i],
                        GTK_POS_BOTTOM, NULL);
}

static void
row_activated_cb (GtkTreeView       *view,
                  GtkTreePath       *path,
                  GtkTreeViewColumn *column,
                  gpointer           user_data)
{
  GtkFontChooserWidget *fontchooser = user_data;
  gchar *fontname;

  fontname = gtk_font_chooser_widget_get_font (fontchooser);
  _gtk_font_chooser_font_activated (GTK_FONT_CHOOSER (fontchooser), fontname);
  g_free (fontname);
}

static void
cursor_changed_cb (GtkTreeView *treeview,
                   gpointer     user_data)
{
  GtkFontChooserWidget *fontchooser = (GtkFontChooserWidget*)user_data;
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;

  PangoFontFamily      *family;
  PangoFontFace        *face;
  PangoFontDescription *desc;

  gint *sizes;
  gint  i, n_sizes;

  GtkTreeIter  iter;
  GtkTreePath *path = gtk_tree_path_new ();

  gtk_tree_view_get_cursor (treeview, &path, NULL);

  if (!path)
    return;

  if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->filter_model), &iter, path))
    {
      gtk_tree_path_free (path);
      return;
    }

  gtk_tree_model_get (GTK_TREE_MODEL (priv->filter_model), &iter,
                      FACE_COLUMN, &face,
                      FAMILY_COLUMN, &family,
                      -1);

  gtk_tree_view_scroll_to_cell (treeview, path, NULL, FALSE, 0.5, 0.5);

  gtk_tree_path_free (path);
  path = NULL;

  if (face == NULL || family == NULL)
    {
      if (face)
        g_object_unref (face);
      if (family)
        g_object_unref (family);

      return;
    }

  desc = pango_font_face_describe (face);
  pango_font_description_set_size (desc, priv->size);
  gtk_widget_override_font (priv->preview, desc);

  pango_font_face_list_sizes (face, &sizes, &n_sizes);
  /* It seems not many fonts actually have a sane set of sizes */
  for (i = 0; i < n_sizes; i++)
    sizes[i] = sizes[i] / PANGO_SCALE;

  set_range_marks (priv, priv->size_slider, sizes, n_sizes);

  if (priv->family)
    g_object_unref (priv->family);
  priv->family = family;

  if (priv->face)
    g_object_unref (priv->face);
  priv->face = face;

  if (priv->font_desc)
    pango_font_description_free (priv->font_desc);
  priv->font_desc = desc;

  g_object_notify (G_OBJECT (fontchooser), "font");
  g_object_notify (G_OBJECT (fontchooser), "font-desc");
}

static gboolean
zoom_preview_cb (GtkWidget      *scrolled_window,
                 GdkEventScroll *event,
                 gpointer        user_data)
{
  GtkFontChooserWidget        *fc    = (GtkFontChooserWidget*)user_data;
  GtkFontChooserWidgetPrivate *priv  = fc->priv;

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
row_inserted_cb (GtkTreeModel *model,
                 GtkTreePath  *path,
                 GtkTreeIter  *iter,
                 gpointer      user_data)
{
  GtkFontChooserWidget        *fontchooser = (GtkFontChooserWidget*)user_data;
  GtkFontChooserWidgetPrivate *priv        = fontchooser->priv;

  gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->list_notebook), 0);
}

static void
row_deleted_cb  (GtkTreeModel *model,
                 GtkTreePath  *path,
                 gpointer      user_data)
{
  GtkFontChooserWidget        *fontchooser = (GtkFontChooserWidget*)user_data;
  GtkFontChooserWidgetPrivate *priv        = fontchooser->priv;

  if (gtk_tree_model_iter_n_children (model, NULL) == 0)
    gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->list_notebook), 1);
}

static void
gtk_font_chooser_widget_init (GtkFontChooserWidget *fontchooser)
{
  GIcon                   *icon;
  GtkFontChooserWidgetPrivate   *priv;
  const PangoFontDescription *font_desc;
  GtkWidget               *scrolled_win;
  GtkWidget               *grid;

  fontchooser->priv = G_TYPE_INSTANCE_GET_PRIVATE (fontchooser,
                                                   GTK_TYPE_FONT_CHOOSER_WIDGET,
                                                   GtkFontChooserWidgetPrivate);

  priv = fontchooser->priv;

  /* Default preview string  */
  priv->preview_text = g_strdup (pango_language_get_sample_string (NULL));
  priv->show_preview_entry = TRUE;

  /* Getting the default size */
  font_desc  = pango_context_get_font_description (gtk_widget_get_pango_context (GTK_WIDGET (fontchooser)));
  priv->size = pango_font_description_get_size (font_desc);
  priv->face = NULL;
  priv->family = NULL;
  priv->font_desc = NULL;

  gtk_widget_push_composite_child ();

  /* Creating fundamental widgets for the private struct */
  priv->search_entry = gtk_entry_new ();
  priv->family_face_list = gtk_tree_view_new ();
  priv->preview = gtk_entry_new ();
  priv->size_slider = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                                (gdouble) font_sizes[0],
                                                (gdouble) font_sizes[G_N_ELEMENTS (font_sizes) - 1],
                                                1.0);

  priv->size_spin = gtk_spin_button_new_with_range (0.0, (gdouble)(G_MAXINT / PANGO_SCALE), 1.0);

  /** Bootstrapping widget layout **/
  gtk_box_set_spacing (GTK_BOX (fontchooser), 6);

  /* Main font family/face view */
  priv->list_scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  scrolled_win = priv->list_scrolled_window;
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_win),
                                       GTK_SHADOW_ETCHED_IN);
  gtk_widget_set_size_request (scrolled_win, 400, 300);
  gtk_container_add (GTK_CONTAINER (scrolled_win), priv->family_face_list);

  /* Text to display when list is empty */
  priv->empty_list = gtk_label_new (_(NO_FONT_MATCHED_SEARCH));
  gtk_widget_set_margin_top    (priv->empty_list, 12);
  gtk_widget_set_margin_left   (priv->empty_list, 12);
  gtk_widget_set_margin_right  (priv->empty_list, 12);
  gtk_widget_set_margin_bottom (priv->empty_list, 12);
  gtk_widget_set_halign (priv->empty_list, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (priv->empty_list, GTK_ALIGN_START);

  priv->list_notebook = gtk_notebook_new ();
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->list_notebook), FALSE);
  gtk_notebook_append_page (GTK_NOTEBOOK (priv->list_notebook), scrolled_win, NULL);
  gtk_notebook_append_page (GTK_NOTEBOOK (priv->list_notebook), priv->empty_list, NULL);

  /* Basic layout */
  grid = gtk_grid_new ();

  gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);

  gtk_grid_attach (GTK_GRID (grid), priv->search_entry, 0, 0, 2, 1);
  gtk_grid_attach (GTK_GRID (grid), priv->list_notebook, 0, 1, 2, 1);
  gtk_grid_attach (GTK_GRID (grid), priv->preview,      0, 2, 2, 1);

  gtk_grid_attach (GTK_GRID (grid), priv->size_slider,  0, 3, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), priv->size_spin,    1, 3, 1, 1);

  gtk_widget_set_hexpand  (GTK_WIDGET (scrolled_win),      TRUE);
  gtk_widget_set_vexpand  (GTK_WIDGET (scrolled_win),      TRUE);
  gtk_widget_set_hexpand  (GTK_WIDGET (priv->search_entry), TRUE);

  gtk_widget_set_hexpand  (GTK_WIDGET (priv->size_slider), TRUE);
  gtk_widget_set_hexpand  (GTK_WIDGET (priv->size_spin),   FALSE);

  gtk_box_pack_start (GTK_BOX (fontchooser), grid, TRUE, TRUE, 0);

  /* Setting the adjustment values for the size slider */
  gtk_adjustment_set_value (gtk_range_get_adjustment (GTK_RANGE (priv->size_slider)),
                            (gdouble)(priv->size / PANGO_SCALE));
  gtk_adjustment_set_value (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (priv->size_spin)),
                            (gdouble)(priv->size / PANGO_SCALE));

  gtk_widget_show_all (GTK_WIDGET (fontchooser));
  gtk_widget_hide     (GTK_WIDGET (fontchooser));

  /* Treeview column and model bootstrapping */
  gtk_font_chooser_widget_bootstrap_fontlist (fontchooser);

  /* Set default preview text */
  gtk_entry_set_text (GTK_ENTRY (priv->preview),
                      pango_language_get_sample_string (NULL));

  /* Set search icon and place holder text */
  icon = g_themed_icon_new_with_default_fallbacks ("edit-find-symbolic");
  g_object_set (G_OBJECT (priv->search_entry),
                "secondary-icon-gicon", icon,
                "secondary-icon-activatable", FALSE,
                "secondary-icon-sensitive", FALSE,
                NULL);
  g_object_unref (icon);

  gtk_entry_set_placeholder_text (GTK_ENTRY (priv->search_entry), _("Search font name"));

  /** Callback connections **/
  g_signal_connect (priv->search_entry, "notify::text",
                    G_CALLBACK (text_changed_cb), fontchooser);
  g_signal_connect (priv->search_entry,
                    "icon-press", G_CALLBACK (icon_press_cb), NULL);

  g_signal_connect (gtk_range_get_adjustment (GTK_RANGE (priv->size_slider)),
                    "value-changed", G_CALLBACK (slider_change_cb), fontchooser);
  g_signal_connect (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (priv->size_spin)),
                    "value-changed", G_CALLBACK (spin_change_cb), fontchooser);

  priv->cursor_changed_handler =
      g_signal_connect (priv->family_face_list, "cursor-changed",
                        G_CALLBACK (cursor_changed_cb), fontchooser);

  g_signal_connect (priv->family_face_list, "row-activated",
                    G_CALLBACK (row_activated_cb), fontchooser);

  /* Zoom on preview scroll */
  g_signal_connect (priv->preview, "scroll-event",
                    G_CALLBACK (zoom_preview_cb), fontchooser);

  g_signal_connect (priv->size_slider, "scroll-event",
                    G_CALLBACK (zoom_preview_cb), fontchooser);

  set_range_marks (priv, priv->size_slider, (gint*)font_sizes, G_N_ELEMENTS (font_sizes));

  /* Font list empty hides the scrolledwindow */
  g_signal_connect (G_OBJECT (priv->filter_model), "row-deleted",
                    G_CALLBACK (row_deleted_cb), fontchooser);
  g_signal_connect (G_OBJECT (priv->filter_model), "row-inserted",
                    G_CALLBACK (row_inserted_cb), fontchooser);

  /* Set default focus */
  gtk_widget_pop_composite_child ();

  gtk_font_chooser_widget_take_font_desc (fontchooser, NULL);
}

/**
 * gtk_font_chooser_widget_new:
 *
 * Creates a new #GtkFontChooserWidget.
 *
 * Return value: a new #GtkFontChooserWidget
 *
 * Since: 3.2
 */
GtkWidget *
gtk_font_chooser_widget_new (void)
{
  return g_object_new (GTK_TYPE_FONT_CHOOSER_WIDGET, NULL);
}

static int
cmp_families (const void *a,
              const void *b)
{
  const char *a_name = pango_font_family_get_name (*(PangoFontFamily **)a);
  const char *b_name = pango_font_family_get_name (*(PangoFontFamily **)b);

  return g_utf8_collate (a_name, b_name);
}

static void
populate_list (GtkFontChooserWidget *fontchooser,
               GtkTreeView    *treeview,
               GtkListStore   *model)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  GtkStyleContext *style_context;
  PangoFontDescription *default_font;
  PangoFontDescription *selected_font;

  gint match;
  GtkTreeIter match_row;
  GtkTreePath *path;

  gint n_families, i;
  PangoFontFamily **families;
  gchar *tmp;
  gchar *family_and_face;

  if (!gtk_widget_has_screen (GTK_WIDGET (fontchooser)))
    return;

  pango_context_list_families (gtk_widget_get_pango_context (GTK_WIDGET (treeview)),
                               &families,
                               &n_families);

  qsort (families, n_families, sizeof (PangoFontFamily *), cmp_families);

  style_context = gtk_widget_get_style_context (GTK_WIDGET (treeview));
  default_font = (PangoFontDescription*) gtk_style_context_get_font (style_context,
                                                                     GTK_STATE_NORMAL);

  if (priv->face)
    selected_font = pango_font_face_describe (priv->face);
  else
    selected_font = NULL;

  gtk_list_store_clear (model);

  match = 0;

  /* Iterate over families and faces */
  for (i = 0; i < n_families; i++)
    {
      GtkTreeIter     iter;
      PangoFontFace **faces;
      int             j, n_faces;
      const gchar    *fam_name = pango_font_family_get_name (families[i]);

      pango_font_family_list_faces (families[i], &faces, &n_faces);

      for (j = 0; j < n_faces; j++)
        {
          PangoFontDescription *pango_desc;
          const gchar *face_name;
          gchar *font_desc;

          if (priv->filter_func != NULL &&
              !priv->filter_func (families[i], faces[j], priv->filter_data))
            continue;

          pango_desc = pango_font_face_describe (faces[j]);
          face_name = pango_font_face_get_face_name (faces[j]);
          font_desc = pango_font_description_to_string (pango_desc);

          family_and_face = g_strconcat (fam_name, " ", face_name, NULL);
          tmp = g_markup_printf_escaped (ROW_FORMAT_STRING,
                                         family_and_face,
                                         font_desc,
                                         fontchooser->priv->preview_text);

          gtk_list_store_append (model, &iter);
          gtk_list_store_set (model, &iter,
                              FAMILY_COLUMN, families[i],
                              FACE_COLUMN, faces[j],
                              PREVIEW_TITLE_COLUMN, family_and_face,
                              PREVIEW_TEXT_COLUMN, tmp,
                              -1);

          /* Select the current font,
           * the default font/face from the theme,
           * or the first font
           */
          if (match < 3 &&
              selected_font != NULL &&
              pango_font_description_equal (selected_font, pango_desc))
            {
              match_row = iter;
              match = 3;
            }
          if (match < 2 &&
              strcmp (fam_name, pango_font_description_get_family (default_font)) == 0)
            {
              match_row = iter;
              match = 2;
            }
          if (match < 1)
            {
              match_row = iter;
              match = 1;
            }

          pango_font_description_free (pango_desc);
          g_free (family_and_face);
          g_free (tmp);
          g_free (font_desc);
        }

      g_free (faces);
    }

  path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &match_row);
  if (path)
    {
      gtk_tree_view_set_cursor (treeview, path, NULL, FALSE);
      gtk_tree_view_scroll_to_cell (treeview, path, NULL, FALSE, 0.5, 0.5);
      gtk_tree_path_free (path);
    }

  if (selected_font)
    pango_font_description_free (selected_font);

  g_free (families);
}

static gboolean
visible_func (GtkTreeModel *model,
              GtkTreeIter  *iter,
              gpointer      user_data)
{
  gboolean result = TRUE;
  GtkFontChooserWidgetPrivate *priv = (GtkFontChooserWidgetPrivate*)user_data;

  const gchar *search_text = (const gchar*)gtk_entry_get_text (GTK_ENTRY (priv->search_entry));
  gchar       *font_name;
  gchar       *term;
  gchar      **split_terms;
  gint         n_terms = 0;

  /* If there's no filter string we show the item */
  if (strlen (search_text) == 0)
    return TRUE;

  gtk_tree_model_get (model, iter,
                      PREVIEW_TITLE_COLUMN, &font_name,
                      -1);

  if (font_name == NULL)
    return FALSE;

  split_terms = g_strsplit (search_text, " ", 0);
  term = split_terms[0];

  while (term && result)
  {
    gchar* font_name_casefold = g_utf8_casefold (font_name, -1);
    gchar* term_casefold = g_utf8_casefold (term, -1);

    if (g_strrstr (font_name_casefold, term_casefold))
      result = result && TRUE;
    else
      result = FALSE;

    n_terms++;
    term = split_terms[n_terms];

    g_free (term_casefold);
    g_free (font_name_casefold);
  }

  g_free (font_name);
  g_strfreev (split_terms);

  return result;
}

static void
gtk_font_chooser_widget_bootstrap_fontlist (GtkFontChooserWidget *fontchooser)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  GtkTreeView       *treeview = GTK_TREE_VIEW (priv->family_face_list);
  GtkCellRenderer   *cell;
  GtkTreeViewColumn *col;

  priv->model = gtk_list_store_new (4,
                                    PANGO_TYPE_FONT_FAMILY,
                                    PANGO_TYPE_FONT_FACE,
                                    G_TYPE_STRING,
                                    G_TYPE_STRING);

  priv->filter_model = gtk_tree_model_filter_new (GTK_TREE_MODEL (priv->model), NULL);
  g_object_unref (priv->model);

  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (priv->filter_model),
                                          visible_func, (gpointer)priv, NULL);

  gtk_tree_view_set_model (treeview, GTK_TREE_MODEL (priv->filter_model));
  g_object_unref (priv->filter_model);

  gtk_tree_view_set_rules_hint      (treeview, TRUE);
  gtk_tree_view_set_headers_visible (treeview, FALSE);

  cell = gtk_cell_renderer_text_new ();
  col = gtk_tree_view_column_new_with_attributes (_("Font Family"),
                                                  cell,
                                                  "markup", PREVIEW_TEXT_COLUMN,
                                                  NULL);

  g_object_set (cell, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

  gtk_tree_view_append_column (treeview, col);

  populate_list (fontchooser, treeview, priv->model);
}

static void
gtk_font_chooser_widget_dispose (GObject *object)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (object);
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;

  if (priv->cursor_changed_handler != 0)
    {
      g_signal_handler_disconnect (priv->family_face_list,
                                   priv->cursor_changed_handler);
      priv->cursor_changed_handler = 0;
    }

  G_OBJECT_CLASS (gtk_font_chooser_widget_parent_class)->dispose (object);
}

static void
gtk_font_chooser_widget_finalize (GObject *object)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (object);
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;

  if (priv->font_desc)
    pango_font_description_free (priv->font_desc);

  if (priv->family)
    g_object_unref (priv->family);

  if (priv->face)
    g_object_unref (priv->face);

  if (priv->filter_data_destroy)
    priv->filter_data_destroy (priv->filter_data);

  G_OBJECT_CLASS (gtk_font_chooser_widget_parent_class)->finalize (object);
}

static void
gtk_font_chooser_widget_screen_changed (GtkWidget *widget,
                                        GdkScreen *previous_screen)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (widget);
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  void (* screen_changed) (GtkWidget *, GdkScreen *) =
    GTK_WIDGET_CLASS (gtk_font_chooser_widget_parent_class)->screen_changed;

  if (screen_changed)
    screen_changed (widget, previous_screen);

  populate_list (fontchooser,
                 GTK_TREE_VIEW (priv->family_face_list),
                 priv->model);

  gtk_font_chooser_widget_select_font (fontchooser);
}

static void
gtk_font_chooser_widget_style_updated (GtkWidget *widget)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (widget);

  GTK_WIDGET_CLASS (gtk_font_chooser_widget_parent_class)->style_updated (widget);

  populate_list (fontchooser,
                 GTK_TREE_VIEW (fontchooser->priv->family_face_list),
                 fontchooser->priv->model);
}

static PangoFontFamily *
gtk_font_chooser_widget_get_family (GtkFontChooser *chooser)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (chooser);

  return fontchooser->priv->family;
}

static PangoFontFace *
gtk_font_chooser_widget_get_face (GtkFontChooser *chooser)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (chooser);

  return fontchooser->priv->face;
}

static gint
gtk_font_chooser_widget_get_size (GtkFontChooser *chooser)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (chooser);

  return fontchooser->priv->size;
}

static gchar *
gtk_font_chooser_widget_get_font (GtkFontChooserWidget *fontchooser)
{
  return pango_font_description_to_string (fontchooser->priv->font_desc);
}

static PangoFontDescription *
gtk_font_chooser_widget_get_font_desc (GtkFontChooserWidget *fontchooser)
{
  return fontchooser->priv->font_desc;
}

static void
gtk_font_chooser_widget_set_font (GtkFontChooserWidget *fontchooser,
                                  const gchar          *fontname)
{
  PangoFontDescription *font_desc;

  font_desc = pango_font_description_from_string (fontname);
  gtk_font_chooser_widget_take_font_desc (fontchooser, font_desc);
}

static void
gtk_font_chooser_widget_take_font_desc (GtkFontChooserWidget *fontchooser,
                                        PangoFontDescription *font_desc)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;

  if (font_desc && priv->font_desc &&
      pango_font_description_equal (font_desc, priv->font_desc))
    {
      pango_font_description_free (font_desc);
      return;
    }

  if (priv->font_desc)
    pango_font_description_free (priv->font_desc);
  if (font_desc)
    priv->font_desc = font_desc; /* adopted */
  else
    priv->font_desc = pango_font_description_from_string (GTK_FONT_CHOOSER_DEFAULT_FONT_NAME);

  gtk_font_chooser_widget_select_font (fontchooser);
}

static void
gtk_font_chooser_widget_select_font (GtkFontChooserWidget *fontchooser)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  const PangoFontDescription *desc;
  const gchar *family_name;
  gint font_size;
  gboolean font_size_is_absolute;
  GtkTreeIter iter;
  gboolean valid;
  gboolean found = FALSE;

  desc = priv->font_desc;
  g_assert (desc != NULL);

  font_size = pango_font_description_get_size (desc);
  font_size_is_absolute = pango_font_description_get_size_is_absolute (desc);
  family_name = pango_font_description_get_family (desc);

  /* We make sure the filter is clear */
  gtk_entry_set_text (GTK_ENTRY (priv->search_entry), "");

  if (!family_name)
    goto deselect;

  /* We find the matching family/face */
  for (valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->filter_model), &iter);
       valid;
       valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->filter_model), &iter))
    {
      PangoFontFace        *face;
      PangoFontDescription *tmp_desc;

      gtk_tree_model_get (GTK_TREE_MODEL (priv->filter_model), &iter,
                          FACE_COLUMN, &face,
                          -1);

      tmp_desc = pango_font_face_describe (face);
      if (font_size_is_absolute)
        pango_font_description_set_absolute_size (tmp_desc, font_size);
      else
        pango_font_description_set_size (tmp_desc, font_size);

      if (pango_font_description_equal (desc, tmp_desc))
        {
          GtkTreePath *path;

          if (font_size)
            {
              if (font_size_is_absolute)
                font_size *= PANGO_SCALE;
              gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->size_spin),
                                         font_size / PANGO_SCALE);
            }

          path = gtk_tree_model_get_path (GTK_TREE_MODEL (priv->filter_model), &iter);

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

deselect:
  if (!found)
    {
      gtk_tree_selection_unselect_all
        (gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->family_face_list)));
    }
}

static const gchar*
gtk_font_chooser_widget_get_preview_text (GtkFontChooserWidget *fontchooser)
{
  return fontchooser->priv->preview_text;
}

static void
gtk_font_chooser_widget_set_preview_text (GtkFontChooserWidget *fontchooser,
                                          const gchar          *text)
{
  g_free (fontchooser->priv->preview_text);
  fontchooser->priv->preview_text = g_strdup (text);

  populate_list (fontchooser,
                 GTK_TREE_VIEW (fontchooser->priv->family_face_list),
                 fontchooser->priv->model);

  gtk_entry_set_text (GTK_ENTRY (fontchooser->priv->preview), text);

  g_object_notify (G_OBJECT (fontchooser), "preview-text");
}

static gboolean
gtk_font_chooser_widget_get_show_preview_entry (GtkFontChooserWidget *fontchooser)
{
  return fontchooser->priv->show_preview_entry;
}

static void
gtk_font_chooser_widget_set_show_preview_entry (GtkFontChooserWidget *fontchooser,
                                                gboolean              show_preview_entry)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;

  if (priv->show_preview_entry != show_preview_entry)
    {
      fontchooser->priv->show_preview_entry = show_preview_entry;

      if (show_preview_entry)
        gtk_widget_show (fontchooser->priv->preview);
      else
        gtk_widget_hide (fontchooser->priv->preview);

      g_object_notify (G_OBJECT (fontchooser), "show-preview-entry");
    }
}

static void
gtk_font_chooser_widget_set_filter_func (GtkFontChooser  *chooser,
                                         GtkFontFilterFunc filter,
                                         gpointer          data,
                                         GDestroyNotify    destroy)
{
  GtkFontChooserWidget        *fontchooser = GTK_FONT_CHOOSER_WIDGET (chooser);
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;

  if (priv->filter_data_destroy)
    priv->filter_data_destroy (priv->filter_data);

  priv->filter_func = filter;
  priv->filter_data = data;
  priv->filter_data_destroy = destroy;

  populate_list (fontchooser,
                 GTK_TREE_VIEW (priv->family_face_list),
                 priv->model);
}

static void
gtk_font_chooser_widget_iface_init (GtkFontChooserIface *iface)
{
  iface->get_font_family = gtk_font_chooser_widget_get_family;
  iface->get_font_face = gtk_font_chooser_widget_get_face;
  iface->get_font_size = gtk_font_chooser_widget_get_size;
  iface->set_filter_func = gtk_font_chooser_widget_set_filter_func;
}
