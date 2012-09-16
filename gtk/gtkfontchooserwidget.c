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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdlib.h>
#include <glib/gprintf.h>
#include <string.h>

#include <atk/atk.h>

#include "gtkfontchooserwidget.h"

#include "gtkadjustment.h"
#include "gtkbuildable.h"
#include "gtkbox.h"
#include "gtkcellrenderertext.h"
#include "gtkentry.h"
#include "gtksearchentry.h"
#include "gtkgrid.h"
#include "gtkfontchooser.h"
#include "gtkfontchooserutils.h"
#include "gtkintl.h"
#include "gtklabel.h"
#include "gtkliststore.h"
#include "gtknotebook.h"
#include "gtkprivate.h"
#include "gtkscale.h"
#include "gtkscrolledwindow.h"
#include "gtkspinbutton.h"
#include "gtktextview.h"
#include "gtktreeselection.h"
#include "gtktreeview.h"
#include "gtkwidget.h"

/**
 * SECTION:gtkfontchooserwidget
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
  GtkCellRenderer *family_face_cell;
  GtkWidget    *list_scrolled_window;
  GtkWidget    *empty_list;
  GtkWidget    *list_notebook;
  GtkTreeModel *model;
  GtkTreeModel *filter_model;

  GtkWidget       *preview;
  gchar           *preview_text;
  gboolean         show_preview_entry;

  GtkWidget *size_spin;
  GtkWidget *size_slider;

  PangoFontDescription *font_desc;
  GtkTreeIter           font_iter;      /* invalid if font not available or pointer into model
                                           (not filter_model) to the row containing font */
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

#define NO_FONT_MATCHED_SEARCH N_("No fonts matched your search. You can revise your search and try again.")

enum {
  FAMILY_COLUMN,
  FACE_COLUMN,
  FONT_DESC_COLUMN,
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

static void gtk_font_chooser_widget_screen_changed       (GtkWidget       *widget,
                                                          GdkScreen       *previous_screen);

static void gtk_font_chooser_widget_bootstrap_fontlist   (GtkFontChooserWidget *fontchooser);

static gboolean gtk_font_chooser_widget_find_font        (GtkFontChooserWidget *fontchooser,
                                                          const PangoFontDescription *font_desc,
                                                          GtkTreeIter          *iter);
static void     gtk_font_chooser_widget_ensure_selection (GtkFontChooserWidget *fontchooser);

static gchar   *gtk_font_chooser_widget_get_font         (GtkFontChooserWidget *fontchooser);
static void     gtk_font_chooser_widget_set_font         (GtkFontChooserWidget *fontchooser,
                                                          const gchar          *fontname);

static PangoFontDescription *gtk_font_chooser_widget_get_font_desc  (GtkFontChooserWidget *fontchooser);
static void                  gtk_font_chooser_widget_merge_font_desc(GtkFontChooserWidget *fontchooser,
                                                                     PangoFontDescription *font_desc,
                                                                     GtkTreeIter          *iter);
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
gtk_font_chooser_widget_refilter_font_list (GtkFontChooserWidget *fontchooser)
{
  gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (fontchooser->priv->filter_model));
  gtk_font_chooser_widget_ensure_selection (fontchooser);
}

static void
text_changed_cb (GtkEntry             *entry,
                 GParamSpec           *pspec,
                 GtkFontChooserWidget *fc)
{
  gtk_font_chooser_widget_refilter_font_list (fc);
}

static void
size_change_cb (GtkAdjustment *adjustment,
                gpointer       user_data)
{
  GtkFontChooserWidget *fontchooser = user_data;
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  PangoFontDescription *font_desc;
  gdouble size = gtk_adjustment_get_value (adjustment);

  font_desc = pango_font_description_new ();
  if (pango_font_description_get_size_is_absolute (priv->font_desc))
    pango_font_description_set_absolute_size (font_desc, size * PANGO_SCALE);
  else
    pango_font_description_set_size (font_desc, size * PANGO_SCALE);

  gtk_font_chooser_widget_take_font_desc (fontchooser, font_desc);
}

static void
gtk_font_chooser_widget_update_marks (GtkFontChooserWidget *fontchooser)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  GtkAdjustment *adj;
  const int *sizes;
  gint *font_sizes;
  gint i, n_sizes;

  if (gtk_list_store_iter_is_valid (GTK_LIST_STORE (priv->model), &priv->font_iter))
    {
      PangoFontFace *face;

      gtk_tree_model_get (priv->model, &priv->font_iter,
                          FACE_COLUMN, &face,
                          -1);

      pango_font_face_list_sizes (face, &font_sizes, &n_sizes);

      /* It seems not many fonts actually have a sane set of sizes */
      for (i = 0; i < n_sizes; i++)
        font_sizes[i] = font_sizes[i] / PANGO_SCALE;

      g_object_unref (face);
    }
  else
    {
      font_sizes = NULL;
      n_sizes = 0;
    }

  if (n_sizes < 2)
    {
      static const gint fallback_sizes[] = {
        6, 8, 9, 10, 11, 12, 13, 14, 16, 20, 24, 36, 48, 72
      };

      sizes = fallback_sizes;
      n_sizes = G_N_ELEMENTS (fallback_sizes);
    }
  else
    {
      sizes = font_sizes;
    }

  gtk_scale_clear_marks (GTK_SCALE (priv->size_slider));

  adj = gtk_range_get_adjustment(GTK_RANGE (priv->size_slider));

  /* ensure clamping doesn't callback into font resizing code */
  g_signal_handlers_block_by_func (adj, size_change_cb, fontchooser);
  gtk_adjustment_configure (adj,
                            gtk_adjustment_get_value (adj),
                            sizes[0],
                            sizes[n_sizes - 1],
                            gtk_adjustment_get_step_increment (adj),
                            gtk_adjustment_get_page_increment (adj),
                            gtk_adjustment_get_page_size (adj));
  g_signal_handlers_unblock_by_func (adj, size_change_cb, fontchooser);

  for (i = 0; i < n_sizes; i++)
    {
      gtk_scale_add_mark (GTK_SCALE (priv->size_slider),
                          sizes[i],
                          GTK_POS_BOTTOM, NULL);
    }

  g_free (font_sizes);
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

static PangoFontDescription *
tree_model_get_font_description (GtkTreeModel *model,
                                 GtkTreeIter  *iter)
{
  PangoFontDescription *desc;
  PangoFontFace *face;
  GtkTreeIter child_iter;

  gtk_tree_model_get (model, iter,
                      FONT_DESC_COLUMN, &desc,
                      -1);
  if (desc != NULL)
    return desc;

  gtk_tree_model_get (model, iter,
                      FACE_COLUMN, &face,
                      -1);
  desc = pango_font_face_describe (face);
  g_object_unref (face);
  
  if (GTK_IS_TREE_MODEL_FILTER (model))
    {
      gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model),
                                                        &child_iter,
                                                        iter);
      iter = &child_iter;
      model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
    }

  gtk_list_store_set (GTK_LIST_STORE (model), iter,
                      FONT_DESC_COLUMN, desc,
                      -1);

  return desc;
}

static void
cursor_changed_cb (GtkTreeView *treeview,
                   gpointer     user_data)
{
  GtkFontChooserWidget *fontchooser = user_data;
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  PangoFontDescription *desc;
  GtkTreeIter filter_iter, iter;
  GtkTreePath *path = NULL;

  gtk_tree_view_get_cursor (treeview, &path, NULL);

  if (!path)
    return;

  if (!gtk_tree_model_get_iter (priv->filter_model, &filter_iter, path))
    {
      gtk_tree_path_free (path);
      return;
    }

  gtk_tree_path_free (path);

  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (priv->filter_model),
                                                    &iter,
                                                    &filter_iter);
  desc = tree_model_get_font_description (priv->model, &iter);

  gtk_font_chooser_widget_merge_font_desc (fontchooser, desc, &iter);
}

static gboolean
zoom_preview_cb (GtkWidget      *scrolled_window,
                 GdkEventScroll *event,
                 gpointer        user_data)
{
  GtkFontChooserWidget *fc = user_data;
  GtkFontChooserWidgetPrivate *priv = fc->priv;
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
  GtkFontChooserWidget *fontchooser = user_data;
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;

  gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->list_notebook), 0);
}

static void
row_deleted_cb  (GtkTreeModel *model,
                 GtkTreePath  *path,
                 gpointer      user_data)
{
  GtkFontChooserWidget *fontchooser = user_data;
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;

  if (gtk_tree_model_iter_n_children (model, NULL) == 0)
    gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->list_notebook), 1);
}

static void
gtk_font_chooser_widget_init (GtkFontChooserWidget *fontchooser)
{
  GtkFontChooserWidgetPrivate *priv;
  GtkWidget *scrolled_win;
  GtkWidget *grid;

  fontchooser->priv = G_TYPE_INSTANCE_GET_PRIVATE (fontchooser,
                                                   GTK_TYPE_FONT_CHOOSER_WIDGET,
                                                   GtkFontChooserWidgetPrivate);

  priv = fontchooser->priv;

  /* Default preview string  */
  priv->preview_text = g_strdup (pango_language_get_sample_string (NULL));
  priv->show_preview_entry = TRUE;
  priv->font_desc = pango_font_description_new ();

  gtk_widget_push_composite_child ();

  /* Creating fundamental widgets for the private struct */
  priv->search_entry = gtk_search_entry_new ();
  priv->family_face_list = gtk_tree_view_new ();
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (priv->family_face_list), FALSE);
  priv->preview = gtk_entry_new ();
  priv->size_slider = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                                0.0,
                                                (gdouble)(G_MAXINT / PANGO_SCALE),
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

  gtk_widget_show_all (GTK_WIDGET (fontchooser));
  gtk_widget_hide     (GTK_WIDGET (fontchooser));

  /* Treeview column and model bootstrapping */
  gtk_font_chooser_widget_bootstrap_fontlist (fontchooser);

  /* Set default preview text */
  gtk_entry_set_text (GTK_ENTRY (priv->preview),
                      pango_language_get_sample_string (NULL));

  gtk_entry_set_placeholder_text (GTK_ENTRY (priv->search_entry), _("Search font name"));

  /* Callback connections */
  g_signal_connect (priv->search_entry, "notify::text",
                    G_CALLBACK (text_changed_cb), fontchooser);

  g_signal_connect (gtk_range_get_adjustment (GTK_RANGE (priv->size_slider)),
                    "value-changed", G_CALLBACK (size_change_cb), fontchooser);
  g_signal_connect (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (priv->size_spin)),
                    "value-changed", G_CALLBACK (size_change_cb), fontchooser);

  g_signal_connect (priv->family_face_list, "cursor-changed",
                    G_CALLBACK (cursor_changed_cb), fontchooser);
  g_signal_connect (priv->family_face_list, "row-activated",
                    G_CALLBACK (row_activated_cb), fontchooser);

  /* Zoom on preview scroll */
  g_signal_connect (priv->preview, "scroll-event",
                    G_CALLBACK (zoom_preview_cb), fontchooser);

  g_signal_connect (priv->size_slider, "scroll-event",
                    G_CALLBACK (zoom_preview_cb), fontchooser);

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
gtk_font_chooser_widget_load_fonts (GtkFontChooserWidget *fontchooser)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  GtkListStore *list_store;
  gint n_families, i;
  PangoFontFamily **families;
  gchar *family_and_face;

  list_store = GTK_LIST_STORE (priv->model);

  pango_context_list_families (gtk_widget_get_pango_context (GTK_WIDGET (fontchooser)),
                               &families,
                               &n_families);

  qsort (families, n_families, sizeof (PangoFontFamily *), cmp_families);

  gtk_list_store_clear (list_store);

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
          const gchar *face_name;

          face_name = pango_font_face_get_face_name (faces[j]);

          family_and_face = g_strconcat (fam_name, " ", face_name, NULL);

          gtk_list_store_insert_with_values (list_store, &iter, -1,
                                             FAMILY_COLUMN, families[i],
                                             FACE_COLUMN, faces[j],
                                             PREVIEW_TITLE_COLUMN, family_and_face,
                                             -1);

          g_free (family_and_face);
        }

      g_free (faces);
    }

  g_free (families);

  /* now make sure the font list looks right */
  if (!gtk_font_chooser_widget_find_font (fontchooser,
                                          priv->font_desc,
                                          &priv->font_iter))
    memset (&priv->font_iter, 0, sizeof (GtkTreeIter));

  gtk_font_chooser_widget_ensure_selection (fontchooser);
}

static gboolean
visible_func (GtkTreeModel *model,
              GtkTreeIter  *iter,
              gpointer      user_data)
{
  GtkFontChooserWidgetPrivate *priv = user_data;
  gboolean result = TRUE;
  const gchar *search_text;
  gchar **split_terms;
  gchar *font_name, *font_name_casefold;
  guint i;

  if (priv->filter_func != NULL)
    {
      PangoFontFamily *family;
      PangoFontFace *face;

      gtk_tree_model_get (model, iter,
                          FAMILY_COLUMN, &family,
                          FACE_COLUMN, &face,
                          -1);

      result = priv->filter_func (family, face, priv->filter_data);

      g_object_unref (family);
      g_object_unref (face);
      
      if (!result)
        return FALSE;
    }

  /* If there's no filter string we show the item */
  search_text = gtk_entry_get_text (GTK_ENTRY (priv->search_entry));
  if (strlen (search_text) == 0)
    return TRUE;

  gtk_tree_model_get (model, iter,
                      PREVIEW_TITLE_COLUMN, &font_name,
                      -1);

  if (font_name == NULL)
    return FALSE;

  split_terms = g_strsplit (search_text, " ", 0);
  font_name_casefold = g_utf8_casefold (font_name, -1);

  for (i = 0; split_terms[i] && result; i++)
    {
      gchar* term_casefold = g_utf8_casefold (split_terms[i], -1);

      if (!strstr (font_name_casefold, term_casefold))
        result = FALSE;

      g_free (term_casefold);
    }

  g_free (font_name_casefold);
  g_free (font_name);
  g_strfreev (split_terms);

  return result;
}

/* in pango units */
static int
gtk_font_chooser_widget_get_preview_text_height (GtkFontChooserWidget *fontchooser)
{
  GtkWidget *treeview = fontchooser->priv->family_face_list;
  double dpi, font_size;

  dpi = gdk_screen_get_resolution (gtk_widget_get_screen (treeview));
  gtk_style_context_get (gtk_widget_get_style_context (treeview),
                         gtk_widget_get_state_flags (treeview),
                         "font-size", &font_size,
                         NULL);

  return (dpi < 0.0 ? 96.0 : dpi) / 72.0 * PANGO_SCALE_X_LARGE * font_size * PANGO_SCALE;
}

static PangoAttrList *
gtk_font_chooser_widget_get_preview_attributes (GtkFontChooserWidget       *fontchooser,
                                                const PangoFontDescription *font_desc,
                                                gsize                       first_line_len)
{
  PangoAttribute *attribute;
  PangoAttrList *attrs;

  attrs = pango_attr_list_new ();

  attribute = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
  attribute->end_index = first_line_len;
  pango_attr_list_insert (attrs, attribute);

  attribute = pango_attr_scale_new (PANGO_SCALE_SMALL);
  attribute->end_index = first_line_len;
  pango_attr_list_insert (attrs, attribute);

  if (font_desc)
    {
      attribute = pango_attr_font_desc_new (font_desc);
      attribute->start_index = first_line_len;
      pango_attr_list_insert (attrs, attribute);
    }

  attribute = pango_attr_fallback_new (FALSE);
  attribute->start_index = first_line_len;
  pango_attr_list_insert (attrs, attribute);

  attribute = pango_attr_size_new_absolute (gtk_font_chooser_widget_get_preview_text_height (fontchooser));
  attribute->start_index = first_line_len;
  pango_attr_list_insert (attrs, attribute);

  return attrs;
}

static void
gtk_font_chooser_widget_cell_data_func (GtkTreeViewColumn *column,
                                        GtkCellRenderer   *cell,
                                        GtkTreeModel      *tree_model,
                                        GtkTreeIter       *iter,
                                        gpointer           user_data)
{
  GtkFontChooserWidget *fontchooser = user_data;
  PangoFontDescription *font_desc;
  PangoAttrList *attrs;
  char *to_string, *text;
  gsize first_line_len;

  font_desc = tree_model_get_font_description (tree_model, iter);

  to_string = pango_font_description_to_string (font_desc);

  text = g_strconcat (to_string, "\n", fontchooser->priv->preview_text, NULL);
  first_line_len = strlen (to_string) + 1;
  
  attrs = gtk_font_chooser_widget_get_preview_attributes (fontchooser, 
                                                          font_desc,
                                                          first_line_len);

  g_object_set (cell,
                "attributes", attrs,
                "text", text,
                NULL);

  pango_font_description_free (font_desc);
  pango_attr_list_unref (attrs);
  g_free (to_string);
  g_free (text);
}

static void
gtk_font_chooser_widget_set_cell_size (GtkFontChooserWidget *fontchooser)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  PangoAttrList *attrs;
  GtkRequisition size;

  gtk_cell_renderer_set_fixed_size (priv->family_face_cell, -1, -1);

  attrs = gtk_font_chooser_widget_get_preview_attributes (fontchooser, 
                                                          NULL,
                                                          1);
  
  g_object_set (priv->family_face_cell,
                "attributes", attrs,
                "text", "x\nx",
                NULL);

  pango_attr_list_unref (attrs);

  gtk_cell_renderer_get_preferred_size (priv->family_face_cell,
                                        priv->family_face_list,
                                        &size,
                                        NULL);
  gtk_cell_renderer_set_fixed_size (priv->family_face_cell, size.width, size.height);
}

static void
gtk_font_chooser_widget_bootstrap_fontlist (GtkFontChooserWidget *fontchooser)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  GtkTreeView *treeview = GTK_TREE_VIEW (priv->family_face_list);
  GtkTreeViewColumn *col;

  g_signal_connect_data (priv->family_face_list,
                         "style-updated",
                         G_CALLBACK (gtk_font_chooser_widget_set_cell_size),
                         fontchooser,
                         NULL,
                         G_CONNECT_AFTER | G_CONNECT_SWAPPED);

  priv->model = GTK_TREE_MODEL (gtk_list_store_new (4,
                                                    PANGO_TYPE_FONT_FAMILY,
                                                    PANGO_TYPE_FONT_FACE,
                                                    PANGO_TYPE_FONT_DESCRIPTION,
                                                    G_TYPE_STRING));

  priv->filter_model = gtk_tree_model_filter_new (priv->model, NULL);
  g_object_unref (priv->model);

  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (priv->filter_model),
                                          visible_func, (gpointer)priv, NULL);

  gtk_tree_view_set_model (treeview, priv->filter_model);
  g_object_unref (priv->filter_model);

  gtk_tree_view_set_rules_hint      (treeview, TRUE);
  gtk_tree_view_set_headers_visible (treeview, FALSE);
  gtk_tree_view_set_fixed_height_mode (treeview, TRUE);

  priv->family_face_cell = gtk_cell_renderer_text_new ();
  g_object_set (priv->family_face_cell, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

  col = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title (col, _("Font Family"));
  gtk_tree_view_column_set_sizing (col, GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_pack_start (col, priv->family_face_cell, TRUE);
  gtk_tree_view_column_set_cell_data_func (col,
                                           priv->family_face_cell,
                                           gtk_font_chooser_widget_cell_data_func,
                                           fontchooser,
                                           NULL);

  gtk_tree_view_append_column (treeview, col);

  gtk_font_chooser_widget_load_fonts (fontchooser);

  gtk_font_chooser_widget_set_cell_size (fontchooser);
}

static void
gtk_font_chooser_widget_finalize (GObject *object)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (object);
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;

  if (priv->font_desc)
    pango_font_description_free (priv->font_desc);

  if (priv->filter_data_destroy)
    priv->filter_data_destroy (priv->filter_data);

  G_OBJECT_CLASS (gtk_font_chooser_widget_parent_class)->finalize (object);
}

static gboolean
my_pango_font_family_equal (const char *familya,
                            const char *familyb)
{
  return g_ascii_strcasecmp (familya, familyb) == 0;
}

static gboolean
gtk_font_chooser_widget_find_font (GtkFontChooserWidget        *fontchooser,
                                   const PangoFontDescription  *font_desc,
                                   /* out arguments */
                                   GtkTreeIter                 *iter)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  PangoFontDescription *desc;
  PangoFontFamily *family;
  gboolean valid;

  if (pango_font_description_get_family (font_desc) == NULL)
    return FALSE;

  for (valid = gtk_tree_model_get_iter_first (priv->model, iter);
       valid;
       valid = gtk_tree_model_iter_next (priv->model, iter))
    {
      gtk_tree_model_get (priv->model, iter,
                          FAMILY_COLUMN, &family,
                          -1);

      if (!my_pango_font_family_equal (pango_font_description_get_family (font_desc),
                                       pango_font_family_get_name (family)))
        continue;

      desc = tree_model_get_font_description (priv->model, iter);

      pango_font_description_merge_static (desc, font_desc, FALSE);
      if (pango_font_description_equal (desc, font_desc))
        break;

      pango_font_description_free (desc);
    }
  
  return valid;
}

static void
gtk_font_chooser_widget_screen_changed (GtkWidget *widget,
                                        GdkScreen *previous_screen)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (widget);

  if (GTK_WIDGET_CLASS (gtk_font_chooser_widget_parent_class)->screen_changed)
    GTK_WIDGET_CLASS (gtk_font_chooser_widget_parent_class)->screen_changed (widget, previous_screen);

  if (previous_screen == NULL)
    previous_screen = gdk_screen_get_default ();

  if (previous_screen == gtk_widget_get_screen (widget))
    return;

  gtk_font_chooser_widget_load_fonts (fontchooser);
}

static PangoFontFamily *
gtk_font_chooser_widget_get_family (GtkFontChooser *chooser)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (chooser);
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  PangoFontFamily *family;

  if (!gtk_list_store_iter_is_valid (GTK_LIST_STORE (priv->model), &priv->font_iter))
    return NULL;

  gtk_tree_model_get (priv->model, &priv->font_iter,
                      FAMILY_COLUMN, &family,
                      -1);
  g_object_unref (family);

  return family;
}

static PangoFontFace *
gtk_font_chooser_widget_get_face (GtkFontChooser *chooser)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (chooser);
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  PangoFontFace *face;

  if (!gtk_list_store_iter_is_valid (GTK_LIST_STORE (priv->model), &priv->font_iter))
    return NULL;

  gtk_tree_model_get (priv->model, &priv->font_iter,
                      FACE_COLUMN, &face,
                      -1);
  g_object_unref (face);

  return face;
}

static gint
gtk_font_chooser_widget_get_size (GtkFontChooser *chooser)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (chooser);

  return pango_font_description_get_size (fontchooser->priv->font_desc);
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
gtk_font_chooser_widget_ensure_selection (GtkFontChooserWidget *fontchooser)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  GtkTreeSelection *selection;
  GtkTreeIter filter_iter;
  
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->family_face_list));

  if (gtk_list_store_iter_is_valid (GTK_LIST_STORE (priv->model), &priv->font_iter) &&
      gtk_tree_model_filter_convert_child_iter_to_iter (GTK_TREE_MODEL_FILTER (priv->filter_model),
                                                        &filter_iter,
                                                        &priv->font_iter))
    {
      GtkTreePath *path = gtk_tree_model_get_path (GTK_TREE_MODEL (priv->filter_model),
                                                   &filter_iter);

      gtk_tree_selection_select_iter (selection, &filter_iter);
      gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (priv->family_face_list),
                                    path, NULL, FALSE, 0.0, 0.0);
      gtk_tree_path_free (path);
    }
  else
    {
      gtk_tree_selection_unselect_all (selection);
    }
}

static void
gtk_font_chooser_widget_merge_font_desc (GtkFontChooserWidget *fontchooser,
                                         PangoFontDescription *font_desc,
                                         GtkTreeIter          *iter)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  PangoFontMask mask;

  g_assert (font_desc != NULL);
  /* iter may be NULL if the font doesn't exist on the list */

  mask = pango_font_description_get_set_fields (font_desc);

  /* sucky test, because we can't restrict the comparison to 
   * only the parts that actually do get merged */
  if (pango_font_description_equal (font_desc, priv->font_desc))
    {
      pango_font_description_free (font_desc);
      return;
    }

  pango_font_description_merge (priv->font_desc, font_desc, TRUE);
  
  if (mask & PANGO_FONT_MASK_SIZE)
    {
      double font_size = (double) pango_font_description_get_size (priv->font_desc) / PANGO_SCALE;
      /* XXX: This clamps, which can cause it to reloop into here, do we need
       * to block its signal handler? */
      gtk_range_set_value (GTK_RANGE (priv->size_slider), font_size);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->size_spin), font_size);
    }
  if (mask & (PANGO_FONT_MASK_FAMILY | PANGO_FONT_MASK_STYLE | PANGO_FONT_MASK_VARIANT |
              PANGO_FONT_MASK_WEIGHT | PANGO_FONT_MASK_STRETCH))
    {
      if (&priv->font_iter != iter)
        {
          if (iter == NULL)
            memset (&priv->font_iter, 0, sizeof (GtkTreeIter));
          else
            memcpy (&priv->font_iter, iter, sizeof (GtkTreeIter));
          
          gtk_font_chooser_widget_ensure_selection (fontchooser);
        }

      gtk_font_chooser_widget_update_marks (fontchooser);
    }

  gtk_widget_override_font (priv->preview, priv->font_desc);

  pango_font_description_free (font_desc); /* adopted */

  g_object_notify (G_OBJECT (fontchooser), "font");
  g_object_notify (G_OBJECT (fontchooser), "font-desc");
}

static void
gtk_font_chooser_widget_take_font_desc (GtkFontChooserWidget *fontchooser,
                                        PangoFontDescription *font_desc)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  PangoFontMask mask;

  if (font_desc == NULL)
    font_desc = pango_font_description_from_string (GTK_FONT_CHOOSER_DEFAULT_FONT_NAME);

  mask = pango_font_description_get_set_fields (font_desc);
  if (mask & (PANGO_FONT_MASK_FAMILY | PANGO_FONT_MASK_STYLE | PANGO_FONT_MASK_VARIANT |
              PANGO_FONT_MASK_WEIGHT | PANGO_FONT_MASK_STRETCH))
    {
      GtkTreeIter iter;

      if (gtk_font_chooser_widget_find_font (fontchooser,
                                             font_desc,
                                             &iter))
        {
          gtk_font_chooser_widget_merge_font_desc (fontchooser,
                                                   font_desc,
                                                   &iter);
        }
      else
        {
          gtk_font_chooser_widget_merge_font_desc (fontchooser,
                                                   font_desc,
                                                   NULL);
        }
    }
  else
    {
      gtk_font_chooser_widget_merge_font_desc (fontchooser,
                                               font_desc,
                                               &priv->font_iter);
                                                          
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
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;

  g_free (priv->preview_text);
  priv->preview_text = g_strdup (text);

  gtk_entry_set_text (GTK_ENTRY (priv->preview), text);

  g_object_notify (G_OBJECT (fontchooser), "preview-text");

  /* XXX: There's no API to tell the treeview that a column has changed,
   * so we just */
  gtk_widget_queue_draw (priv->family_face_list);
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
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (chooser);
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;

  if (priv->filter_data_destroy)
    priv->filter_data_destroy (priv->filter_data);

  priv->filter_func = filter;
  priv->filter_data = data;
  priv->filter_data_destroy = destroy;

  gtk_font_chooser_widget_refilter_font_list (fontchooser);
}

static void
gtk_font_chooser_widget_iface_init (GtkFontChooserIface *iface)
{
  iface->get_font_family = gtk_font_chooser_widget_get_family;
  iface->get_font_face = gtk_font_chooser_widget_get_face;
  iface->get_font_size = gtk_font_chooser_widget_get_size;
  iface->set_filter_func = gtk_font_chooser_widget_set_filter_func;
}
