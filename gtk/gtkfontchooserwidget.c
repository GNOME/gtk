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
#include <pango/pango.h>

#include "gtkfontchooserwidget.h"
#include "gtkfontchooserwidgetprivate.h"

#include "gtkadjustment.h"
#include "gtkbuildable.h"
#include "gtkbox.h"
#include "gtkcellrenderertext.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkentry.h"
#include "gtksearchentry.h"
#include "gtkgrid.h"
#include "gtkfontchooser.h"
#include "gtkfontchooserutils.h"
#include "gtkintl.h"
#include "gtklabel.h"
#include "gtkliststore.h"
#include "gtkstack.h"
#include "gtkprivate.h"
#include "gtkscale.h"
#include "gtkscrolledwindow.h"
#include "gtkspinbutton.h"
#include "gtkstylecontextprivate.h"
#include "gtktextview.h"
#include "gtktreeselection.h"
#include "gtktreeview.h"
#include "gtkwidget.h"
#include "gtksettings.h"
#include "gtkdialog.h"
#include "gtkcombobox.h"
#include "gtkcelllayout.h"
#include "gtkbutton.h"
#include "gtkrevealer.h"
#include "gtklistbox.h"
#include "gtkpopover.h"
#include "gtkradiobutton.h"
#include "gtkcheckbutton.h"

#if defined(HAVE_HARFBUZZ) && defined(HAVE_PANGOFT)
#include <pango/pangofc-font.h>
#include <hb.h>
#include <hb-ot.h>
#include <hb-ft.h>
#include <freetype/freetype.h>
#include <freetype/ftmm.h>
#include "language-names.h"
#include "script-names.h"
#endif

#include "open-type-layout.h"

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
 * # CSS nodes
 *
 * GtkFontChooserWidget has a single CSS node with name fontchooser.
 *
 * Since: 3.2
 */

struct _GtkFontChooserWidgetPrivate
{
  GtkWidget    *stack;
  GtkWidget    *grid;
  GtkWidget    *search_entry;

  GtkWidget    *family_face_list;
  GtkTreeViewColumn *family_face_column;
  GtkCellRenderer *family_face_cell;
  GtkWidget    *list_scrolled_window;
  GtkWidget    *list_stack;
  GtkTreeModel *model;
  GtkTreeModel *filter_model;

  GtkWidget    *filter_popover;
  GtkWidget    *script_filter_revealer;
  GtkWidget    *script_filter_button;
  GtkWidget    *script_filter_button_label;
  GtkWidget    *script_list;
  GtkWidget    *script_filter_entry;
  char         *script_term;

  GtkWidget    *language_filter_revealer;
  GtkWidget    *language_filter_button;
  GtkWidget    *language_filter_button_label;
  GtkWidget    *language_list;
  GtkWidget    *language_filter_entry;
  char         *language_term;

  GtkWidget    *monospace_filter_check;

  GtkWidget       *preview;
  GtkWidget       *preview2;
  gchar           *preview_text;
  gboolean         show_preview_entry;

  GtkWidget *size_label;
  GtkWidget *size_spin;
  GtkWidget *size_slider;

  GtkWidget *font_grid;

  GtkWidget       *axis_grid;
  GtkWidget       *feature_box;
  GtkWidget       *feature_language_combo;

  PangoFontMap         *font_map;

  PangoFontDescription *font_desc;
  GtkTreeIter           font_iter;      /* invalid if font not available or pointer into model
                                           (not filter_model) to the row containing font */
  GtkFontFilterFunc filter_func;
  gpointer          filter_data;
  GDestroyNotify    filter_data_destroy;

  PangoScript script;
  PangoLanguage *language;

  guint last_fontconfig_timestamp;

  GHashTable *axes;
  gboolean updating_variations;

  GtkFontChooserLevel level;

  GList *feature_items;
};

/* Keep in line with GtkTreeStore defined in gtkfontchooserwidget.ui */
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

static void gtk_font_chooser_widget_display_changed      (GtkWidget       *widget,
                                                          GdkDisplay      *previous_display);

static gboolean gtk_font_chooser_widget_find_font        (GtkFontChooserWidget *fontchooser,
                                                          const PangoFontDescription *font_desc,
                                                          GtkTreeIter          *iter);
static void     gtk_font_chooser_widget_ensure_selection (GtkFontChooserWidget *fontchooser);

static gchar   *gtk_font_chooser_widget_get_font         (GtkFontChooserWidget *fontchooser);
static void     gtk_font_chooser_widget_set_font         (GtkFontChooserWidget *fontchooser,
                                                          const gchar          *fontname);

static PangoFontDescription *gtk_font_chooser_widget_get_font_desc  (GtkFontChooserWidget *fontchooser);
static void                  gtk_font_chooser_widget_merge_font_desc(GtkFontChooserWidget       *fontchooser,
                                                                     const PangoFontDescription *font_desc,
                                                                     GtkTreeIter                *iter);
static void                  gtk_font_chooser_widget_take_font_desc (GtkFontChooserWidget *fontchooser,
                                                                     PangoFontDescription *font_desc);


static const gchar *gtk_font_chooser_widget_get_preview_text (GtkFontChooserWidget *fontchooser);
static void         gtk_font_chooser_widget_set_preview_text (GtkFontChooserWidget *fontchooser,
                                                              const gchar          *text);

static gboolean gtk_font_chooser_widget_get_show_preview_entry (GtkFontChooserWidget *fontchooser);
static void     gtk_font_chooser_widget_set_show_preview_entry (GtkFontChooserWidget *fontchooser,
                                                                gboolean              show_preview_entry);

static void     gtk_font_chooser_widget_set_cell_size          (GtkFontChooserWidget *fontchooser);
static void     gtk_font_chooser_widget_load_fonts             (GtkFontChooserWidget *fontchooser,
                                                                gboolean              force);
static void     gtk_font_chooser_widget_populate_filters       (GtkFontChooserWidget *fontchooser);
static void     gtk_font_chooser_widget_populate_features      (GtkFontChooserWidget *fontchooser);
static gboolean visible_func                                   (GtkTreeModel *model,
								GtkTreeIter  *iter,
								gpointer      user_data);
static void     gtk_font_chooser_widget_cell_data_func         (GtkTreeViewColumn *column,
								GtkCellRenderer   *cell,
								GtkTreeModel      *tree_model,
								GtkTreeIter       *iter,
								gpointer           user_data);
static void                gtk_font_chooser_widget_set_level (GtkFontChooserWidget *fontchooser,
                                                              GtkFontChooserLevel   level);
static GtkFontChooserLevel gtk_font_chooser_widget_get_level (GtkFontChooserWidget *fontchooser);

static void script_filter_button_clicked (GtkButton            *button,
                                          GtkFontChooserWidget *fontchooser);
static void script_row_activated (GtkListBox           *list,
                                  GtkListBoxRow        *row,
                                  GtkFontChooserWidget *fontchooser);
static void show_script_list (GtkFontChooserWidget *fontchooser);
static void hide_script_list (GtkFontChooserWidget *fontchooser,
                              gboolean              animate);
static void script_filter_changed (GtkFontChooserWidget *fontchooser);
static void script_filter_stop (GtkFontChooserWidget *fontchooser);
static void script_filter_activated (GtkFontChooserWidget *fontchooser);
static void language_filter_button_clicked (GtkButton            *button,
                                            GtkFontChooserWidget *fontchooser);
static void language_row_activated (GtkListBox           *list,
                                    GtkListBoxRow        *row,
                                    GtkFontChooserWidget *fontchooser);
static void show_language_list (GtkFontChooserWidget *fontchooser);
static void hide_language_list (GtkFontChooserWidget *fontchooser,
                                gboolean              animate);
static void language_filter_changed (GtkFontChooserWidget *fontchooser);
static void language_filter_stop (GtkFontChooserWidget *fontchooser);
static void language_filter_activated (GtkFontChooserWidget *fontchooser);
static void popover_notify (GObject              *object,
                            GParamSpec           *pspec,
                            GtkFontChooserWidget *fontchooser);
static void collect_font_features (GtkFontChooserWidget *fontchooser,
                                   PangoAttrList *attrs);

static void gtk_font_chooser_widget_iface_init (GtkFontChooserIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkFontChooserWidget, gtk_font_chooser_widget, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GtkFontChooserWidget)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_FONT_CHOOSER,
                                                gtk_font_chooser_widget_iface_init))

typedef struct _GtkDelayedFontDescription GtkDelayedFontDescription;
struct _GtkDelayedFontDescription {
  PangoFontFace        *face;
  PangoFontDescription *desc;
  PangoScript          *scripts;
  int                   n_scripts;
  guint                 ref_count;
};

static GtkDelayedFontDescription *
gtk_delayed_font_description_new (PangoFontFace *face)
{
  GtkDelayedFontDescription *result;
  
  result = g_slice_new0 (GtkDelayedFontDescription);

  result->face = g_object_ref (face);
  result->desc = NULL;
  result->scripts = NULL;
  result->n_scripts = -1;
  result->ref_count = 1;

  return result;
}

static GtkDelayedFontDescription *
gtk_delayed_font_description_ref (GtkDelayedFontDescription *desc)
{
  desc->ref_count++;

  return desc;
}

static void
gtk_delayed_font_description_unref (GtkDelayedFontDescription *desc)
{
  desc->ref_count--;

  if (desc->ref_count > 0)
    return;

  g_object_unref (desc->face);
  if (desc->desc)
    pango_font_description_free (desc->desc);

  g_free (desc->scripts);

  g_slice_free (GtkDelayedFontDescription, desc);
}

static const PangoFontDescription *
gtk_delayed_font_description_get (GtkDelayedFontDescription *desc)
{
  if (desc->desc == NULL)
    desc->desc = pango_font_face_describe (desc->face);

  return desc->desc;
}

#define GTK_TYPE_DELAYED_FONT_DESCRIPTION (gtk_delayed_font_description_get_type ())
GType gtk_delayed_font_description_get_type (void);

G_DEFINE_BOXED_TYPE (GtkDelayedFontDescription, gtk_delayed_font_description,
                     gtk_delayed_font_description_ref,
                     gtk_delayed_font_description_unref)
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
    case GTK_FONT_CHOOSER_PROP_LEVEL:
      gtk_font_chooser_widget_set_level (fontchooser, g_value_get_enum (value));
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
    case GTK_FONT_CHOOSER_PROP_LEVEL:
      g_value_set_enum (value, gtk_font_chooser_widget_get_level (fontchooser));
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
                 GtkFontChooserWidget *fc)
{
  gtk_font_chooser_widget_refilter_font_list (fc);
}

static void
stop_search_cb (GtkEntry             *entry,
                GtkFontChooserWidget *fc)
{
  if (gtk_entry_get_text (entry)[0] != 0)
    gtk_entry_set_text (entry, "");
  else
    {
      GtkWidget *dialog;
      GtkWidget *button = NULL;

      dialog = gtk_widget_get_ancestor (GTK_WIDGET (fc), GTK_TYPE_DIALOG);
      if (dialog)
        button = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);

      if (button)
        gtk_widget_activate (button);
    }
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

static gboolean
output_cb (GtkSpinButton *spin,
           gpointer       data)
{
  GtkAdjustment *adjustment;
  gchar *text;
  gdouble value;

  adjustment = gtk_spin_button_get_adjustment (spin);
  value = gtk_adjustment_get_value (adjustment);
  text = g_strdup_printf ("%2.4g", value);
  gtk_spin_button_set_text (spin, text);
  g_free (text);

  return TRUE;
}

static void
gtk_font_chooser_widget_update_marks (GtkFontChooserWidget *fontchooser)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  GtkAdjustment *adj, *spin_adj;
  const int *sizes;
  gint *font_sizes;
  gint i, n_sizes;
  gdouble value, spin_value;

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

  adj        = gtk_range_get_adjustment (GTK_RANGE (priv->size_slider));
  spin_adj   = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (priv->size_spin));
  spin_value = gtk_adjustment_get_value (spin_adj);

  if (spin_value < sizes[0])
    value = (gdouble) sizes[0];
  else if (spin_value > sizes[n_sizes - 1])
    value = (gdouble)sizes[n_sizes - 1];
  else
    value = (gdouble)spin_value;

  /* ensure clamping doesn't callback into font resizing code */
  g_signal_handlers_block_by_func (adj, size_change_cb, fontchooser);
  gtk_adjustment_configure (adj,
                            value,
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

static void
cursor_changed_cb (GtkTreeView *treeview,
                   gpointer     user_data)
{
  GtkFontChooserWidget *fontchooser = user_data;
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  GtkDelayedFontDescription *desc;
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
  gtk_tree_model_get (priv->model, &iter,
                      FONT_DESC_COLUMN, &desc,
                      -1);

  // reset variations explicitly
  pango_font_description_set_variations (priv->font_desc, NULL);
  gtk_font_chooser_widget_merge_font_desc (fontchooser,
                                           gtk_delayed_font_description_get (desc),
                                           &iter);

  gtk_delayed_font_description_unref (desc);
}

static gboolean
resize_by_scroll_cb (GtkWidget      *scrolled_window,
                     GdkEventScroll *event,
                     gpointer        user_data)
{
  GtkFontChooserWidget *fc = user_data;
  GtkFontChooserWidgetPrivate *priv = fc->priv;
  GtkAdjustment *adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (priv->size_spin));
  GdkScrollDirection direction;
  gdouble delta_x, delta_y;

  if (!gdk_event_get_scroll_direction ((GdkEvent *) event, &direction))
    return GDK_EVENT_PROPAGATE;

  gdk_event_get_scroll_deltas ((GdkEvent *) event, &delta_x, &delta_y);

  if (direction == GDK_SCROLL_UP || direction == GDK_SCROLL_RIGHT)
    gtk_adjustment_set_value (adj,
                              gtk_adjustment_get_value (adj) +
                              gtk_adjustment_get_step_increment (adj));
  else if (direction == GDK_SCROLL_DOWN || direction == GDK_SCROLL_LEFT)
    gtk_adjustment_set_value (adj,
                              gtk_adjustment_get_value (adj) -
                              gtk_adjustment_get_step_increment (adj));
  else if (direction == GDK_SCROLL_SMOOTH && delta_x != 0.0)
    gtk_adjustment_set_value (adj,
                              gtk_adjustment_get_value (adj) +
                              gtk_adjustment_get_step_increment (adj) * delta_x);
  else if (direction == GDK_SCROLL_SMOOTH && delta_y != 0.0)
    gtk_adjustment_set_value (adj,
                              gtk_adjustment_get_value (adj) -
                              gtk_adjustment_get_step_increment (adj) * delta_y);

  return TRUE;
}

static void
gtk_font_chooser_widget_update_preview_attributes (GtkFontChooserWidget *fontchooser)
{
  GtkFontChooserWidgetPrivate *priv;
  PangoAttrList *attrs;

  priv = fontchooser->priv;

  attrs = pango_attr_list_new ();

  /* Prevent font fallback */
  pango_attr_list_insert (attrs, pango_attr_fallback_new (FALSE));

  /* Force current font */
  pango_attr_list_insert (attrs, pango_attr_font_desc_new (priv->font_desc));

  collect_font_features (fontchooser, attrs);

  gtk_entry_set_attributes (GTK_ENTRY (priv->preview), attrs);
  pango_attr_list_unref (attrs);
}

static void
rows_changed_cb (GtkFontChooserWidget *fontchooser)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;

  if (gtk_tree_model_iter_n_children (priv->filter_model, NULL) == 0)
    gtk_stack_set_visible_child_name (GTK_STACK (priv->list_stack), "empty");
  else
    gtk_stack_set_visible_child_name (GTK_STACK (priv->list_stack), "list");
}

static void
gtk_font_chooser_widget_measure (GtkWidget       *widget,
                                 GtkOrientation  orientation,
                                 int             for_size,
                                 int            *minimum,
                                 int            *natural,
                                 int            *minimum_baseline,
                                 int            *natural_baseline)
{
  GtkFontChooserWidget *self = GTK_FONT_CHOOSER_WIDGET (widget);
  GtkFontChooserWidgetPrivate *priv = gtk_font_chooser_widget_get_instance_private (self);

  gtk_widget_measure (priv->stack, orientation, for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);
}

static void
gtk_font_chooser_widget_snapshot (GtkWidget   *widget,
                                  GtkSnapshot *snapshot)
{
  GtkFontChooserWidget *self = GTK_FONT_CHOOSER_WIDGET (widget);
  GtkFontChooserWidgetPrivate *priv = gtk_font_chooser_widget_get_instance_private (self);

  gtk_widget_snapshot_child (widget, priv->stack, snapshot);
}

static void
gtk_font_chooser_widget_size_allocate (GtkWidget           *widget,
                                       const GtkAllocation *allocation,
                                       int                  baseline,
                                       GtkAllocation       *out_clip)
{
  GtkFontChooserWidget *self = GTK_FONT_CHOOSER_WIDGET (widget);
  GtkFontChooserWidgetPrivate *priv = gtk_font_chooser_widget_get_instance_private (self);

  GTK_WIDGET_CLASS (gtk_font_chooser_widget_parent_class)->size_allocate (widget, allocation, -1, out_clip);

  gtk_widget_size_allocate (priv->stack, allocation, -1, out_clip);
}

static void
gtk_font_chooser_widget_dispose (GObject *object)
{
  GtkFontChooserWidget *self = GTK_FONT_CHOOSER_WIDGET (object);
  GtkFontChooserWidgetPrivate *priv = gtk_font_chooser_widget_get_instance_private (self);

  if (priv->stack)
    {
      gtk_widget_unparent (priv->stack);
      priv->stack = NULL;
    }

  G_OBJECT_CLASS (gtk_font_chooser_widget_parent_class)->dispose (object);
}

static void
gtk_font_chooser_widget_class_init (GtkFontChooserWidgetClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  g_type_ensure (GTK_TYPE_DELAYED_FONT_DESCRIPTION);
  g_type_ensure (G_TYPE_THEMED_ICON);

  widget_class->display_changed = gtk_font_chooser_widget_display_changed;
  widget_class->measure = gtk_font_chooser_widget_measure;
  widget_class->size_allocate = gtk_font_chooser_widget_size_allocate;
  widget_class->snapshot = gtk_font_chooser_widget_snapshot;

  gobject_class->finalize = gtk_font_chooser_widget_finalize;
  gobject_class->dispose = gtk_font_chooser_widget_dispose;
  gobject_class->set_property = gtk_font_chooser_widget_set_property;
  gobject_class->get_property = gtk_font_chooser_widget_get_property;

  _gtk_font_chooser_install_properties (gobject_class);

  /* Bind class to template */
  gtk_widget_class_set_template_from_resource (widget_class,
					       "/org/gtk/libgtk/ui/gtkfontchooserwidget.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GtkFontChooserWidget, search_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFontChooserWidget, family_face_list);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFontChooserWidget, family_face_column);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFontChooserWidget, family_face_cell);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFontChooserWidget, list_scrolled_window);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFontChooserWidget, list_stack);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFontChooserWidget, model);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFontChooserWidget, filter_model);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFontChooserWidget, preview);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFontChooserWidget, preview2);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFontChooserWidget, size_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFontChooserWidget, size_spin);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFontChooserWidget, size_slider);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFontChooserWidget, stack);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFontChooserWidget, grid);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFontChooserWidget, font_grid);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFontChooserWidget, axis_grid);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFontChooserWidget, feature_box);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFontChooserWidget, feature_language_combo);

  gtk_widget_class_bind_template_child_private (widget_class, GtkFontChooserWidget, filter_popover);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFontChooserWidget, script_filter_revealer);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFontChooserWidget, script_filter_button);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFontChooserWidget, script_filter_button_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFontChooserWidget, script_list);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFontChooserWidget, script_filter_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFontChooserWidget, language_filter_revealer);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFontChooserWidget, language_filter_button);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFontChooserWidget, language_filter_button_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFontChooserWidget, language_list);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFontChooserWidget, language_filter_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFontChooserWidget, monospace_filter_check);

  gtk_widget_class_bind_template_callback (widget_class, text_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, stop_search_cb);
  gtk_widget_class_bind_template_callback (widget_class, cursor_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, gtk_font_chooser_widget_set_cell_size);
  gtk_widget_class_bind_template_callback (widget_class, resize_by_scroll_cb);
  gtk_widget_class_bind_template_callback (widget_class, rows_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, size_change_cb);
  gtk_widget_class_bind_template_callback (widget_class, output_cb);
  gtk_widget_class_bind_template_callback (widget_class, script_filter_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, script_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, script_filter_changed);
  gtk_widget_class_bind_template_callback (widget_class, script_filter_stop);
  gtk_widget_class_bind_template_callback (widget_class, script_filter_activated);
  gtk_widget_class_bind_template_callback (widget_class, language_filter_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, language_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, language_filter_changed);
  gtk_widget_class_bind_template_callback (widget_class, language_filter_stop);
  gtk_widget_class_bind_template_callback (widget_class, language_filter_activated);
  gtk_widget_class_bind_template_callback (widget_class, popover_notify);
  gtk_widget_class_bind_template_callback (widget_class, gtk_font_chooser_widget_refilter_font_list);

  gtk_widget_class_set_css_name (widget_class, I_("fontchooser"));
}

#if defined(HAVE_HARFBUZZ) && defined(HAVE_PANGOFT)

typedef struct {
  guint32 tag;
  GtkAdjustment *adjustment;
  GtkWidget *label;
  GtkWidget *scale;
  GtkWidget *spin;
  GtkWidget *fontchooser;
} Axis;

static guint
axis_hash (gconstpointer v)
{
  const Axis *a = v;

  return a->tag;
}

static gboolean
axis_equal (gconstpointer v1, gconstpointer v2)
{
  const Axis *a1 = v1;
  const Axis *a2 = v2;

  return a1->tag == a2->tag;
}

static void
axis_remove (gpointer key,
             gpointer value,
             gpointer data)
{
  Axis *a = value;

  gtk_widget_destroy (a->label);
  gtk_widget_destroy (a->scale);
  gtk_widget_destroy (a->spin);
}

static void
axis_free (gpointer v)
{
  Axis *a = v;

  g_free (a);
}

#endif
static void
gtk_font_chooser_widget_init (GtkFontChooserWidget *fontchooser)
{
  GtkFontChooserWidgetPrivate *priv;

  fontchooser->priv = gtk_font_chooser_widget_get_instance_private (fontchooser);
  priv = fontchooser->priv;

  gtk_widget_set_has_window (GTK_WIDGET (fontchooser), FALSE);

  gtk_widget_init_template (GTK_WIDGET (fontchooser));

#if defined(HAVE_HARFBUZZ) && defined(HAVE_PANGOFT)
  priv->axes = g_hash_table_new_full (axis_hash, axis_equal, NULL, axis_free);
#endif

  /* Default preview string  */
  priv->preview_text = g_strdup (pango_language_get_sample_string (NULL));
  priv->show_preview_entry = TRUE;
  priv->font_desc = pango_font_description_new ();

  /* Set default preview text */
  gtk_entry_set_text (GTK_ENTRY (priv->preview), priv->preview_text);

  gtk_font_chooser_widget_update_preview_attributes (fontchooser);

  /* Set the upper values of the spin/scale with G_MAXINT / PANGO_SCALE */
  gtk_spin_button_set_range (GTK_SPIN_BUTTON (priv->size_spin),
			     1.0, (gdouble)(G_MAXINT / PANGO_SCALE));
  gtk_adjustment_set_upper (gtk_range_get_adjustment (GTK_RANGE (priv->size_slider)),
			    (gdouble)(G_MAXINT / PANGO_SCALE));

  /* Setup treeview/model auxilary functions */
  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (priv->filter_model),
                                          visible_func, fontchooser, NULL);

  gtk_tree_view_column_set_cell_data_func (priv->family_face_column,
                                           priv->family_face_cell,
                                           gtk_font_chooser_widget_cell_data_func,
                                           fontchooser,
                                           NULL);

  /* Load data and set initial style-dependent parameters */
  gtk_font_chooser_widget_load_fonts (fontchooser, TRUE);
  gtk_font_chooser_widget_populate_filters (fontchooser);
  gtk_font_chooser_widget_populate_features (fontchooser);
  gtk_font_chooser_widget_set_cell_size (fontchooser);
  gtk_font_chooser_widget_take_font_desc (fontchooser, NULL);
}

/**
 * gtk_font_chooser_widget_new:
 *
 * Creates a new #GtkFontChooserWidget.
 *
 * Returns: a new #GtkFontChooserWidget
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
gtk_font_chooser_widget_load_fonts (GtkFontChooserWidget *fontchooser,
                                    gboolean              force)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  GtkListStore *list_store;
  gint n_families, i;
  PangoFontFamily **families;
  guint fontconfig_timestamp;
  gboolean need_reload;
  PangoFontMap *font_map;

  g_object_get (gtk_widget_get_settings (GTK_WIDGET (fontchooser)),
                "gtk-fontconfig-timestamp", &fontconfig_timestamp,
                NULL);

  /* The fontconfig timestamp is only set on systems with fontconfig; every
   * other platform will set it to 0. For those systems, we fall back to
   * reloading the fonts every time.
   */
  need_reload = fontconfig_timestamp == 0 ||
                fontconfig_timestamp != priv->last_fontconfig_timestamp;

  priv->last_fontconfig_timestamp = fontconfig_timestamp;

  if (!need_reload && !force)
    return;

  list_store = GTK_LIST_STORE (priv->model);

  if (priv->font_map)
    font_map = priv->font_map;
  else
    font_map = pango_cairo_font_map_get_default ();
  pango_font_map_list_families (font_map, &families, &n_families);

  qsort (families, n_families, sizeof (PangoFontFamily *), cmp_families);

  g_signal_handlers_block_by_func (priv->family_face_list, cursor_changed_cb, fontchooser);
  g_signal_handlers_block_by_func (priv->filter_model, rows_changed_cb, fontchooser);
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
          GtkDelayedFontDescription *desc;
          const gchar *face_name;
          char *title;

          face_name = pango_font_face_get_face_name (faces[j]);

          if (priv->level == GTK_FONT_CHOOSER_LEVEL_FAMILY)
            title = g_strdup (fam_name);
          else
            title = g_strconcat (fam_name, " ", face_name, NULL);

          desc = gtk_delayed_font_description_new (faces[j]);

          gtk_list_store_insert_with_values (list_store, &iter, -1,
                                             FAMILY_COLUMN, families[i],
                                             FACE_COLUMN, faces[j],
                                             FONT_DESC_COLUMN, desc,
                                             PREVIEW_TITLE_COLUMN, title,
                                             -1);

          g_free (title);
          gtk_delayed_font_description_unref (desc);

          if (priv->level == GTK_FONT_CHOOSER_LEVEL_FAMILY)
            break;
        }

      g_free (faces);
    }

  g_free (families);

  rows_changed_cb (fontchooser);

  g_signal_handlers_unblock_by_func (priv->filter_model, rows_changed_cb, fontchooser);
  g_signal_handlers_unblock_by_func (priv->family_face_list, cursor_changed_cb, fontchooser);

  /* now make sure the font list looks right */
  if (!gtk_font_chooser_widget_find_font (fontchooser, priv->font_desc, &priv->font_iter))
    memset (&priv->font_iter, 0, sizeof (GtkTreeIter));

  gtk_font_chooser_widget_ensure_selection (fontchooser);
}

static GtkWidget *
script_row_new (const char  *title,
                PangoScript  script,
                gboolean     active)
{
  GtkWidget *label;
  GtkWidget *image;
  GtkWidget *box;
  GtkWidget *row;
  char *term;
  char *key;

  label = gtk_label_new (title);
  g_object_set (label, "margin", 10, NULL);
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_widget_set_hexpand (label, TRUE);
  image = gtk_image_new_from_icon_name ("object-select-symbolic");
  g_object_set (image,
                "margin-top", 10,
                "margin-bottom", 10,
                "margin-end", 10,
                NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (image), "dim-label");
  gtk_widget_set_opacity (image, active ? 1 : 0);
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_container_add (GTK_CONTAINER (box), label);
  gtk_container_add (GTK_CONTAINER (box), image);
  row = gtk_list_box_row_new ();
  gtk_container_add (GTK_CONTAINER (row), box);
  g_object_set_data (G_OBJECT (row), "script", GINT_TO_POINTER (script));
  term = g_utf8_casefold (title, -1);
  g_object_set_data_full (G_OBJECT (row), "term", term, g_free);
  key = g_utf8_collate_key (term, -1);
  g_object_set_data_full (G_OBJECT (row), "key", key, g_free);
  g_object_set_data (G_OBJECT (row), "label", label);
  g_object_set_data (G_OBJECT (row), "image", image);

  return row;
}

static void
script_row_activated (GtkListBox           *list,
                      GtkListBoxRow        *row,
                      GtkFontChooserWidget *fontchooser)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  GtkWidget *label;
  const char *title;
  GtkWidget *child;
  GtkWidget *image;

  priv->script = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "script"));

  label = GTK_WIDGET (g_object_get_data (G_OBJECT (row), "label"));
  title = gtk_label_get_label (GTK_LABEL (label));
  gtk_label_set_label (GTK_LABEL (priv->script_filter_button_label), title);

  gtk_font_chooser_widget_refilter_font_list (fontchooser);

  for (child = gtk_widget_get_first_child (GTK_WIDGET (list));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      image = GTK_WIDGET (g_object_get_data (G_OBJECT (child), "image"));
      if (image)
        gtk_widget_set_opacity (image, 0);
    }

  image = GTK_WIDGET (g_object_get_data (G_OBJECT (row), "image"));
  gtk_widget_set_opacity (image, 1);

  hide_script_list (fontchooser, TRUE);
}

static gboolean
script_filter_func (GtkListBoxRow *row,
                    gpointer       data)
{
  GtkFontChooserWidget *fontchooser = data;
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  const char *term;

  if (priv->script_term == NULL)
    return TRUE;

  term = (const char *)g_object_get_data (G_OBJECT (row), "term");

  return g_str_has_prefix (term, priv->script_term);
}

static int
script_sort_func (GtkListBoxRow *row1,
                  GtkListBoxRow *row2,
                  gpointer       data)
{
  const char *key1, *key2;

  key1 = (const char *)g_object_get_data (G_OBJECT (row1), "key");
  key2 = (const char *)g_object_get_data (G_OBJECT (row2), "key");

  if (key1 == NULL)
    return -1;
  else if (key2 == NULL)
    return 1;
  else
    return strcmp (key1, key2);
}

static void
show_script_list (GtkFontChooserWidget *fontchooser)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;

  gtk_widget_hide (priv->script_filter_button);
  gtk_widget_show (priv->script_filter_revealer);
  gtk_revealer_set_reveal_child (GTK_REVEALER (priv->script_filter_revealer), TRUE);
}

static void
hide_script_list (GtkFontChooserWidget *fontchooser,
                  gboolean              animate)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;

  gtk_entry_set_text (GTK_ENTRY (priv->script_filter_entry), "");
  gtk_widget_show (priv->script_filter_button);
  if (!animate)
    gtk_revealer_set_transition_type (GTK_REVEALER (priv->script_filter_revealer),
                                      GTK_REVEALER_TRANSITION_TYPE_NONE);
  gtk_revealer_set_reveal_child (GTK_REVEALER (priv->script_filter_revealer), FALSE);
  if (!animate)
    gtk_revealer_set_transition_type (GTK_REVEALER (priv->script_filter_revealer),
                                      GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
}

static void
script_filter_button_clicked (GtkButton            *button,
                              GtkFontChooserWidget *fontchooser)
{
  hide_language_list (fontchooser, TRUE);
  show_script_list (fontchooser);
}

static void
script_filter_changed (GtkFontChooserWidget *fontchooser)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  const char *term;

  term = gtk_entry_get_text (GTK_ENTRY (priv->script_filter_entry));
  g_free (priv->script_term);
  priv->script_term = g_utf8_casefold (term, -1);
  gtk_list_box_invalidate_filter (GTK_LIST_BOX (priv->script_list));
}

static void
script_filter_stop (GtkFontChooserWidget *fontchooser)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;

  gtk_entry_set_text (GTK_ENTRY (priv->script_filter_entry), "");
}

static void
script_filter_activated (GtkFontChooserWidget *fontchooser)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  GList *children, *l;
  GtkWidget *row;

  row = NULL;
  children = gtk_container_get_children (GTK_CONTAINER (priv->script_list));
  for (l = children; l; l = l->next)
    {
      GtkWidget *r = l->data;

      if (!script_filter_func (GTK_LIST_BOX_ROW (r), fontchooser))
        continue;

      if (row != NULL)
        {
          /* more than one match, don't activate */
          g_list_free (children);
          return;
        }

      row = r;
    }
  g_list_free (children);

  gtk_widget_activate (row);
}

static GtkWidget *
language_row_new (const char    *title,
                  PangoLanguage *language,
                  gboolean       active)
{
  GtkWidget *label;
  GtkWidget *image;
  GtkWidget *box;
  GtkWidget *row;
  char *term;
  char *key;

  label = gtk_label_new (title);
  g_object_set (label, "margin", 10, NULL);
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_widget_set_hexpand (label, TRUE);
  image = gtk_image_new_from_icon_name ("object-select-symbolic");
  g_object_set (image,
                "margin-top", 10,
                "margin-bottom", 10,
                "margin-end", 10,
                NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (image), "dim-label");
  gtk_widget_set_opacity (image, active ? 1 : 0);
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (box), label);
  gtk_container_add (GTK_CONTAINER (box), image);
  row = gtk_list_box_row_new ();
  gtk_container_add (GTK_CONTAINER (row), box);
  g_object_set_data (G_OBJECT (row), "language", language);
  term = g_utf8_casefold (title, -1);
  g_object_set_data_full (G_OBJECT (row), "term", term, g_free);
  key = g_utf8_collate_key (term, -1);
  g_object_set_data_full (G_OBJECT (row), "key", key, g_free);
  g_object_set_data (G_OBJECT (row), "label", label);
  g_object_set_data (G_OBJECT (row), "image", image);

  return row;
}

static void
language_row_activated (GtkListBox           *list,
                        GtkListBoxRow        *row,
                        GtkFontChooserWidget *fontchooser)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  const char *title;
  GtkWidget *child;
  GtkWidget *image;

  priv->language = (PangoLanguage *) g_object_get_data (G_OBJECT (row), "language");

  title = gtk_label_get_label (GTK_LABEL (g_object_get_data (G_OBJECT (row), "label")));
  gtk_label_set_label (GTK_LABEL (priv->language_filter_button_label), title);

  gtk_font_chooser_widget_refilter_font_list (fontchooser);

  for (child = gtk_widget_get_first_child (GTK_WIDGET (list));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      image = GTK_WIDGET (g_object_get_data (G_OBJECT (child), "image"));
      if (image)
        gtk_widget_set_opacity (image, 0);
    }

  image = GTK_WIDGET (g_object_get_data (G_OBJECT (row), "image"));
  gtk_widget_set_opacity (image, 1);

  hide_language_list (fontchooser, TRUE);
}

static gboolean
language_filter_func (GtkListBoxRow        *row,
                      gpointer              data)
{
  GtkFontChooserWidget *fontchooser = data;
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  const char *term;

  if (priv->language_term == NULL)
    return TRUE;

  term = (const char *)g_object_get_data (G_OBJECT (row), "term");

  return g_str_has_prefix (term, priv->language_term);
}

static int
language_sort_func (GtkListBoxRow *row1,
                    GtkListBoxRow *row2,
                    gpointer       data)
{
  const char *key1, *key2;

  key1 = (const char *)g_object_get_data (G_OBJECT (row1), "key");
  key2 = (const char *)g_object_get_data (G_OBJECT (row2), "key");

  if (key1 == NULL)
    return -1;
  else if (key2 == NULL)
    return 1;
  else
    return strcmp (key1, key2);
}

static void
gtk_font_chooser_widget_populate_filters (GtkFontChooserWidget *fontchooser)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
#if defined(HAVE_HARFBUZZ) && defined(HAVE_PANGOFT)

  FcPattern *pat;
  FcObjectSet *os;
  FcFontSet *fs;
  FcLangSet *ls;
  FcStrSet *ss;
  FcStrList *sl;
  int i;
  GHashTable *all_langs;
  GHashTable *all_scripts;
  GHashTableIter iter;
  char *s;

  pat = FcPatternCreate ();
  os = FcObjectSetCreate ();
  FcObjectSetAdd (os, FC_LANG);
  fs = FcFontList (0, pat, os);
  FcObjectSetDestroy (os);
  FcPatternDestroy (pat);
  all_langs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  for (i = 0; i < fs->nfont; i++)
    {
      FcPatternGetLangSet (fs->fonts[i], FC_LANG, 0, (FcLangSet **)&ls);
      ss = FcLangSetGetLangs (ls);
      sl = FcStrListCreate (ss);
      FcStrListFirst (sl);
      while ((s = (char *)FcStrListNext (sl)))
        {
          if (!g_hash_table_contains (all_langs, s))
            g_hash_table_add (all_langs, g_strdup (s));
        }
      FcStrListDone (sl);
      FcStrSetDestroy (ss);
    }

  gtk_container_add (GTK_CONTAINER (priv->script_list), script_row_new (_("All Scripts"), 0, TRUE));
  gtk_container_add (GTK_CONTAINER (priv->language_list), language_row_new (_("All Languages"), NULL, TRUE));

  all_scripts = g_hash_table_new (NULL, NULL);

  g_hash_table_iter_init (&iter, all_langs);
  while (g_hash_table_iter_next (&iter, (gpointer *)&s, NULL))
    {
      PangoLanguage *lang = pango_language_from_string (s);

      if (lang)
        {
          const char *name = get_language_name (lang);
          if (name)
            gtk_container_add (GTK_CONTAINER (priv->language_list), language_row_new (name, lang, FALSE));
        }

      if (lang)
        {
          const PangoScript *scripts;
          int num_scripts;

          scripts = pango_language_get_scripts (lang, &num_scripts);
          for (i = 0; i < num_scripts; i++)
            {
              const char *name;

              if (g_hash_table_contains (all_scripts, GINT_TO_POINTER (scripts[i])))
                continue;

              g_hash_table_add (all_scripts, GINT_TO_POINTER (scripts[i]));
              name = get_script_name ((GUnicodeScript)scripts[i]);
              if (name)
                gtk_container_add (GTK_CONTAINER (priv->script_list), script_row_new (name, scripts[i], FALSE));
            }
        }
    }

  g_hash_table_unref (all_scripts);
  g_hash_table_unref (all_langs);

  gtk_list_box_set_filter_func (GTK_LIST_BOX (priv->language_list), language_filter_func, fontchooser, NULL);
  gtk_list_box_set_sort_func (GTK_LIST_BOX (priv->language_list), language_sort_func, fontchooser, NULL);

  gtk_list_box_set_filter_func (GTK_LIST_BOX (priv->script_list), script_filter_func, fontchooser, NULL);
  gtk_list_box_set_sort_func (GTK_LIST_BOX (priv->script_list), script_sort_func, fontchooser, NULL);
#endif
}

static void
show_language_list (GtkFontChooserWidget *fontchooser)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;

  gtk_widget_hide (priv->language_filter_button);
  gtk_widget_show (priv->language_filter_revealer);
  gtk_revealer_set_reveal_child (GTK_REVEALER (priv->language_filter_revealer), TRUE);
}

static void
hide_language_list (GtkFontChooserWidget *fontchooser,
                    gboolean              animate)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;

  gtk_entry_set_text (GTK_ENTRY (priv->language_filter_entry), "");
  gtk_widget_show (priv->language_filter_button);
  if (!animate)
    gtk_revealer_set_transition_type (GTK_REVEALER (priv->language_filter_revealer),
                                      GTK_REVEALER_TRANSITION_TYPE_NONE);
  gtk_revealer_set_reveal_child (GTK_REVEALER (priv->language_filter_revealer), FALSE);
  if (!animate)
    gtk_revealer_set_transition_type (GTK_REVEALER (priv->language_filter_revealer),
                                      GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
}

static void
language_filter_button_clicked (GtkButton            *button,
                                GtkFontChooserWidget *fontchooser)
{
  hide_script_list (fontchooser, TRUE);
  show_language_list (fontchooser);
}

static void
language_filter_changed (GtkFontChooserWidget *fontchooser)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  const char *term;

  term = gtk_entry_get_text (GTK_ENTRY (priv->language_filter_entry));
  g_free (priv->language_term);
  priv->language_term = g_utf8_casefold (term, -1);
  gtk_list_box_invalidate_filter (GTK_LIST_BOX (priv->language_list));
}

static void
language_filter_stop (GtkFontChooserWidget *fontchooser)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;

  gtk_entry_set_text (GTK_ENTRY (priv->language_filter_entry), "");
}

static void
language_filter_activated (GtkFontChooserWidget *fontchooser)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  GList *children, *l;
  GtkWidget *row;

  row = NULL;
  children = gtk_container_get_children (GTK_CONTAINER (priv->language_list));
  for (l = children; l; l = l->next)
    {
      GtkWidget *r = l->data;

      if (!language_filter_func (GTK_LIST_BOX_ROW (r), fontchooser))
        continue;

      if (row != NULL)
        {
          /* more than one match, don't activate */
          g_list_free (children);
          return;
        }

      row = r;
    }
  g_list_free (children);

  gtk_widget_activate (row);
}

static void
popover_notify (GObject              *object,
                GParamSpec           *pspec,
                GtkFontChooserWidget *fontchooser)
{
  hide_script_list (fontchooser, FALSE);
  hide_language_list (fontchooser, FALSE);
}

static gboolean
is_alias_family (PangoFontFamily *family)
{
  const char *family_name;

  family_name = pango_font_family_get_name (family);

  switch (family_name[0])
    {
    case 'm':
    case 'M':
      return (g_ascii_strcasecmp (family_name, "monospace") == 0);
    case 's':
    case 'S':
      return (g_ascii_strcasecmp (family_name, "sans") == 0 ||
              g_ascii_strcasecmp (family_name, "serif") == 0);
    default:
      return FALSE;
    }

  return FALSE;
}

static gboolean
visible_func (GtkTreeModel *model,
              GtkTreeIter  *iter,
              gpointer      user_data)
{
  GtkFontChooserWidget *fontchooser = user_data;
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  gboolean result = TRUE;
  const gchar *search_text;
  gchar **split_terms;
  gchar *font_name, *font_name_casefold;
  guint i;

  if (priv->script != 0 || priv->language != NULL)
    {
      PangoFontFamily *family;
      PangoFontFace *face;
      PangoLanguage **languages;
      int n_languages;
      int i;
      gboolean is_alias;

      gtk_tree_model_get (priv->model, iter,
                          FAMILY_COLUMN, &family,
                          FACE_COLUMN, &face,
                          -1);

      pango_font_face_get_languages (face, &languages, &n_languages);
      is_alias = is_alias_family (family);

      g_object_unref (face);
      g_object_unref (family);

      if (!is_alias)
        {
          gboolean script_found;
          gboolean language_found;

          if (n_languages == 0)
            return FALSE;

          script_found = priv->script == 0;
          language_found = priv->language == NULL;

          for (i = 0; i < n_languages; i++)
            {
              if (priv->script != 0 &&
                  pango_language_includes_script (languages[i], priv->script))
                script_found = TRUE;
              if (priv->language != NULL && priv->language == languages[i])
                language_found = TRUE;
            }

          if (!script_found || !language_found)
            return FALSE;
        }
    }

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->monospace_filter_check)))
    {
      PangoFontFamily *family;

      gtk_tree_model_get (priv->model, iter,
                          FAMILY_COLUMN, &family,
                          -1);

      if (!pango_font_family_is_monospace (family))
        result = FALSE;

      g_object_unref (family);

      if (!result)
        return FALSE;
    }

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
  GtkStyleContext *context;
  double dpi, font_size;

  context = gtk_widget_get_style_context (treeview);
  dpi = _gtk_css_number_value_get (_gtk_style_context_peek_property (context,
                                                                     GTK_CSS_PROPERTY_DPI),
                                   100);
  gtk_style_context_get (context,
                         "font-size", &font_size,
                         NULL);

  return (dpi < 0.0 ? 96.0 : dpi) / 72.0 * PANGO_SCALE_X_LARGE * font_size * PANGO_SCALE;
}

static PangoAttrList *
gtk_font_chooser_widget_get_preview_attributes (GtkFontChooserWidget       *fontchooser,
                                                const PangoFontDescription *font_desc)
{
  PangoAttribute *attribute;
  PangoAttrList *attrs;

  attrs = pango_attr_list_new ();

  if (font_desc)
    {
      attribute = pango_attr_font_desc_new (font_desc);
      pango_attr_list_insert (attrs, attribute);
    }

  attribute = pango_attr_size_new_absolute (gtk_font_chooser_widget_get_preview_text_height (fontchooser));
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
  GtkDelayedFontDescription *desc;
  PangoAttrList *attrs;
  char *preview_title;

  gtk_tree_model_get (tree_model, iter,
                      PREVIEW_TITLE_COLUMN, &preview_title,
                      FONT_DESC_COLUMN, &desc,
                      -1);

  attrs = gtk_font_chooser_widget_get_preview_attributes (fontchooser,
              gtk_delayed_font_description_get (desc));

  g_object_set (cell,
                "xpad", 20,
                "ypad", 10,
                "attributes", attrs,
                "text", preview_title,
                NULL);

  gtk_delayed_font_description_unref (desc);
  pango_attr_list_unref (attrs);
  g_free (preview_title);
}

static void
gtk_font_chooser_widget_set_cell_size (GtkFontChooserWidget *fontchooser)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  PangoAttrList *attrs;
  GtkRequisition size;

  gtk_cell_renderer_set_fixed_size (priv->family_face_cell, -1, -1);

  attrs = gtk_font_chooser_widget_get_preview_attributes (fontchooser, NULL);
  
  g_object_set (priv->family_face_cell,
                "xpad", 20,
                "ypad", 10,
                "attributes", attrs,
                "text", "x",
                NULL);

  pango_attr_list_unref (attrs);

  gtk_cell_renderer_get_preferred_size (priv->family_face_cell,
                                        priv->family_face_list,
                                        &size,
                                        NULL);
  gtk_cell_renderer_set_fixed_size (priv->family_face_cell, size.width, size.height);
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

  g_free (priv->preview_text);

  g_clear_object (&priv->font_map);

  if (priv->axes)
    g_hash_table_unref (priv->axes);

  g_free (priv->script_term);
  g_free (priv->language_term);

  g_list_free_full (priv->feature_items, g_free);

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
  gboolean valid;

  if (pango_font_description_get_family (font_desc) == NULL)
    return FALSE;

  for (valid = gtk_tree_model_get_iter_first (priv->model, iter);
       valid;
       valid = gtk_tree_model_iter_next (priv->model, iter))
    {
      GtkDelayedFontDescription *desc;
      PangoFontDescription *merged;
      PangoFontFamily *family;

      gtk_tree_model_get (priv->model, iter,
                          FAMILY_COLUMN, &family,
                          FONT_DESC_COLUMN, &desc,
                          -1);

      if (!my_pango_font_family_equal (pango_font_description_get_family (font_desc),
                                       pango_font_family_get_name (family)))
        {
          gtk_delayed_font_description_unref (desc);
          g_object_unref (family);
          continue;
        }

      merged = pango_font_description_copy_static (gtk_delayed_font_description_get (desc));

      pango_font_description_merge_static (merged, font_desc, FALSE);
      if (pango_font_description_equal (merged, font_desc))
        {
          gtk_delayed_font_description_unref (desc);
          pango_font_description_free (merged);
          g_object_unref (family);
          break;
        }

      gtk_delayed_font_description_unref (desc);
      pango_font_description_free (merged);
      g_object_unref (family);
    }
  
  return valid;
}

static void
fontconfig_changed (GtkFontChooserWidget *fontchooser)
{
  gtk_font_chooser_widget_load_fonts (fontchooser, TRUE);
}

static void
gtk_font_chooser_widget_display_changed (GtkWidget  *widget,
                                         GdkDisplay *previous_display)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (widget);
  GtkSettings *settings;

  if (GTK_WIDGET_CLASS (gtk_font_chooser_widget_parent_class)->display_changed)
    GTK_WIDGET_CLASS (gtk_font_chooser_widget_parent_class)->display_changed (widget, previous_display);

  if (previous_display)
    {
      settings = gtk_settings_get_for_display (previous_display);
      g_signal_handlers_disconnect_by_func (settings, fontconfig_changed, widget);
    }
  settings = gtk_widget_get_settings (widget);
  g_signal_connect_object (settings, "notify::gtk-fontconfig-timestamp",
                           G_CALLBACK (fontconfig_changed), widget, G_CONNECT_SWAPPED);

  if (previous_display == NULL)
    previous_display = gdk_display_get_default ();

  if (previous_display == gtk_widget_get_display (widget))
    return;

  gtk_font_chooser_widget_load_fonts (fontchooser, FALSE);
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

#if defined(HAVE_HARFBUZZ) && defined(HAVE_PANGOFT)

#define FixedToFloat(f) (((float)(f))/65536.0)

static void
add_font_variations (GtkFontChooserWidget *fontchooser,
                     GString              *s)
{
  GHashTableIter iter;
  Axis *axis;
  const char *sep = "";
  char buf[G_ASCII_DTOSTR_BUF_SIZE];

  g_hash_table_iter_init (&iter, fontchooser->priv->axes);
  while (g_hash_table_iter_next (&iter, (gpointer *)NULL, (gpointer *)&axis))
    {
      char tag[5];
      double value;

      tag[0] = (axis->tag >> 24) & 0xff;
      tag[1] = (axis->tag >> 16) & 0xff;
      tag[2] = (axis->tag >> 8) & 0xff;
      tag[3] = (axis->tag >> 0) & 0xff;
      tag[4] = '\0';
      value = gtk_adjustment_get_value (axis->adjustment);
      g_string_append_printf (s, "%s%s=%s", sep, tag, g_ascii_dtostr (buf, sizeof(buf), value));
      sep = ",";
    }
}

static void
adjustment_changed (GtkAdjustment *adjustment,
                    Axis          *axis)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (axis->fontchooser);
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  PangoFontDescription *font_desc;
  GString *s;

  priv->updating_variations = TRUE;

  s = g_string_new ("");
  add_font_variations (fontchooser, s);

  if (s->len > 0)
    {
      font_desc = pango_font_description_new ();
      pango_font_description_set_variations (font_desc, s->str);
      gtk_font_chooser_widget_take_font_desc (fontchooser, font_desc);
    }

  g_string_free (s, TRUE);

  priv->updating_variations = FALSE;
}

static gboolean
should_show_axis (FT_Var_Axis *ax)
{
  /* FIXME use FT_Get_Var_Axis_Flags */
  if (ax->tag == FT_MAKE_TAG ('o', 'p', 's', 'z'))
    return FALSE;

  return TRUE;
}

static gboolean
is_named_instance (FT_Face face)
{
  return (face->face_index >> 16) > 0;
}

static struct {
  guint32 tag;
  const char *name;
} axis_names[] = {
  { FT_MAKE_TAG ('w', 'd', 't', 'h'), N_("Width") },
  { FT_MAKE_TAG ('w', 'g', 'h', 't'), N_("Weight") },
  { FT_MAKE_TAG ('i', 't', 'a', 'l'), N_("Italic") },
  { FT_MAKE_TAG ('s', 'l', 'n', 't'), N_("Slant") },
  { FT_MAKE_TAG ('o', 'p', 's', 'z'), N_("Optical Size") },
};

static void
add_axis (GtkFontChooserWidget *fontchooser,
          FT_Face               face,
          FT_Var_Axis          *ax,
          FT_Fixed              value,
          int                   row)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  Axis *axis;
  const char *name;
  int i;

  axis = g_new (Axis, 1);
  axis->tag = ax->tag;
  axis->fontchooser = GTK_WIDGET (fontchooser);

  name = ax->name;
  for (i = 0; i < G_N_ELEMENTS (axis_names); i++)
    {
      if (axis_names[i].tag == ax->tag)
        {
          name = _(axis_names[i].name);
          break;
        }
    }
  axis->label = gtk_label_new (name);
  gtk_widget_set_halign (axis->label, GTK_ALIGN_START);
  gtk_widget_set_valign (axis->label, GTK_ALIGN_BASELINE);
  gtk_grid_attach (GTK_GRID (priv->axis_grid), axis->label, 0, row, 1, 1);
  axis->adjustment = gtk_adjustment_new ((double)FixedToFloat(value),
                                         (double)FixedToFloat(ax->minimum),
                                         (double)FixedToFloat(ax->maximum),
                                         1.0, 10.0, 0.0);
  axis->scale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, axis->adjustment);
  gtk_scale_add_mark (GTK_SCALE (axis->scale), (double)FixedToFloat(ax->def), GTK_POS_TOP, NULL);
  gtk_widget_set_valign (axis->scale, GTK_ALIGN_BASELINE);
  gtk_widget_set_hexpand (axis->scale, TRUE);
  gtk_widget_set_size_request (axis->scale, 100, -1);
  gtk_scale_set_draw_value (GTK_SCALE (axis->scale), FALSE);
  gtk_grid_attach (GTK_GRID (priv->axis_grid), axis->scale, 1, row, 1, 1);
  axis->spin = gtk_spin_button_new (axis->adjustment, 0, 0);
  g_signal_connect (axis->spin, "output", G_CALLBACK (output_cb), fontchooser);
  gtk_widget_set_valign (axis->spin, GTK_ALIGN_BASELINE);
  gtk_grid_attach (GTK_GRID (priv->axis_grid), axis->spin, 2, row, 1, 1);

  g_hash_table_add (priv->axes, axis);

  adjustment_changed (axis->adjustment, axis);
  g_signal_connect (axis->adjustment, "value-changed", G_CALLBACK (adjustment_changed), axis);
  if (is_named_instance (face) || !should_show_axis (ax))
    {
      gtk_widget_hide (axis->label);
      gtk_widget_hide (axis->scale);
      gtk_widget_hide (axis->spin);
    }
}

static void
gtk_font_chooser_widget_update_font_variations (GtkFontChooserWidget *fontchooser)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  PangoFont *pango_font;
  FT_Face ft_face;
  FT_MM_Var *ft_mm_var;
  FT_Error ret;

  if (priv->updating_variations)
    return;

  g_hash_table_foreach (priv->axes, axis_remove, NULL);
  g_hash_table_remove_all (priv->axes);

  pango_font = pango_context_load_font (gtk_widget_get_pango_context (GTK_WIDGET (fontchooser)),
                                        priv->font_desc);
  ft_face = pango_fc_font_lock_face (PANGO_FC_FONT (pango_font));

  ret = FT_Get_MM_Var (ft_face, &ft_mm_var);
  if (ret == 0)
    {
      int i;
      FT_Fixed *coords;

      coords = g_new (FT_Fixed, ft_mm_var->num_axis);
      for (i = 0; i < ft_mm_var->num_axis; i++)
        coords[i] = ft_mm_var->axis[i].def;

      if (ft_face->face_index > 0)
        {
          int instance_id = ft_face->face_index >> 16;
          if (instance_id && instance_id <= ft_mm_var->num_namedstyles)
            {
              FT_Var_Named_Style *instance = &ft_mm_var->namedstyle[instance_id - 1];
              memcpy (coords, instance->coords, ft_mm_var->num_axis * sizeof (*coords));
            }
        }

      for (i = 0; i < ft_mm_var->num_axis; i++)
        add_axis (fontchooser, ft_face, &ft_mm_var->axis[i], coords[i], i + 1);

      g_free (coords);
      free (ft_mm_var);
    }

  pango_fc_font_unlock_face (PANGO_FC_FONT (pango_font));
  g_object_unref (pango_font);
}

static void
add_script (GtkListStore *store,
            guint         script_index,
            hb_tag_t      script,
            guint         lang_index,
            hb_tag_t      lang)
{
  char scriptbuf[5];
  char langbuf[5];
  const char *scriptname;
  const char *langname;
  char *name;

  hb_tag_to_string (script, scriptbuf);
  scriptbuf[4] = 0;

  hb_tag_to_string (lang, langbuf);
  langbuf[4] = 0;

  if (script == HB_OT_TAG_DEFAULT_SCRIPT)
    scriptname = "Default";
  else if (script == HB_TAG ('m','a','t','h'))
    scriptname = "Math";
  else
    scriptname = get_script_name_for_tag (script);

  if (!scriptname)
    scriptname = scriptbuf;

  langname = get_language_name_for_tag (lang);
  if (!langname)
    langname = langbuf;

  if (lang == HB_OT_TAG_DEFAULT_LANGUAGE)
    name = g_strdup_printf ("%s Script", scriptname);
  else
    name = g_strdup (langname);

  gtk_list_store_insert_with_values (store, NULL, -1,
                                     0, name,
                                     1, script_index,
                                     2, lang_index,
                                     3, lang,
                                     -1);
  g_free (name);
}

static int
feature_language_sort_func (GtkTreeModel *model,
                            GtkTreeIter  *a,
                            GtkTreeIter  *b,
                            gpointer      user_data)
{
  char *sa, *sb;
  int ret;

  gtk_tree_model_get (model, a, 0, &sa, -1);
  gtk_tree_model_get (model, b, 0, &sb, -1);

  ret = strcmp (sa, sb);

  g_free (sa);
  g_free (sb);

  return ret;
}

typedef struct {
  hb_tag_t script_tag;
  hb_tag_t lang_tag;
  unsigned int script_index;
  unsigned int lang_index;
} TagPair;

static guint
tag_pair_hash (gconstpointer data)
{
  const TagPair *pair = data;

  return pair->script_tag + pair->lang_tag;
}

static gboolean
tag_pair_equal (gconstpointer a, gconstpointer b)
{
  const TagPair *pair_a = a;
  const TagPair *pair_b = b;

  return pair_a->script_tag == pair_b->script_tag && pair_a->lang_tag == pair_b->lang_tag;
}

static void
update_language_combo (GtkFontChooserWidget *fontchooser,
                       hb_face_t            *hb_face)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  GtkListStore *store;
  gint i, j, k;
  hb_tag_t scripts[80];
  unsigned int n_scripts;
  unsigned int count;
  hb_tag_t table[2] = { HB_OT_TAG_GSUB, HB_OT_TAG_GPOS };
  GHashTable *tags;
  GHashTableIter iter;
  TagPair *pair;

  tags = g_hash_table_new_full (tag_pair_hash, tag_pair_equal, g_free, NULL);

  pair = g_new (TagPair, 1);
  pair->script_tag = HB_OT_TAG_DEFAULT_SCRIPT;
  pair->lang_tag = HB_OT_TAG_DEFAULT_LANGUAGE;
  g_hash_table_add (tags, pair);

  n_scripts = 0;
  for (i = 0; i < 2; i++)
    {
      count = G_N_ELEMENTS (scripts);
      hb_ot_layout_table_get_script_tags (hb_face, table[i], n_scripts, &count, scripts);
      g_print ("found %d scripts in %s\n", count, i == 0 ? "GSUB" : "GPOS");
      n_scripts += count;
    }

  for (j = 0; j < n_scripts; j++)
    {
      hb_tag_t languages[80];
      unsigned int n_languages;

      n_languages = 0;
      for (i = 0; i < 2; i++)
        {
          count = G_N_ELEMENTS (languages);
          hb_ot_layout_script_get_language_tags (hb_face, table[i], j, n_languages, &count, languages);
          g_print ("found %d languages for script %d in %s\n", count, j, i == 0 ? "GSUB" : "GPOS");
          n_languages += count;
        }

      pair = g_new (TagPair, 1);
      pair->script_tag = scripts[j];
      pair->lang_tag = HB_OT_TAG_DEFAULT_LANGUAGE;
      pair->script_index = j;
      pair->lang_index = HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX;
      g_hash_table_add (tags, pair);

      for (k = 0; k < n_languages; k++)
        {
          pair = g_new (TagPair, 1);
          pair->script_tag = scripts[j];
          pair->lang_tag = languages[k];
          pair->script_index = j;
          pair->lang_index = k;
          g_hash_table_add (tags, pair);
        }
    }

  store = gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT);

  g_hash_table_iter_init (&iter, tags);
  while (g_hash_table_iter_next (&iter, (gpointer *)&pair, NULL))
    add_script (store, pair->script_index, pair->script_tag, pair->lang_index, pair->lang_tag);

  g_hash_table_unref (tags);

  gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (store),
                                           feature_language_sort_func, NULL, NULL);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
                                        GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
                                        GTK_SORT_ASCENDING);
  gtk_combo_box_set_model (GTK_COMBO_BOX (priv->feature_language_combo), GTK_TREE_MODEL (store));
  gtk_combo_box_set_active (GTK_COMBO_BOX (priv->feature_language_combo), 0);
}

typedef struct {
  hb_tag_t tag;
  const char *name;
  GtkWidget *feat;
} FeatureItem;

static const char *
get_feature_display_name (hb_tag_t tag)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (open_type_layout_features); i++)
    {
      if (tag == open_type_layout_features[i].tag)
        return g_dpgettext2 (NULL, "OpenType layout", open_type_layout_features[i].name);
    }

  return NULL;
}

static void
set_inconsistent (GtkCheckButton *button,
                  gboolean        inconsistent)
{
  gtk_check_button_set_inconsistent (GTK_CHECK_BUTTON (button), inconsistent);
  gtk_widget_set_opacity (gtk_widget_get_first_child (GTK_WIDGET (button)), inconsistent ? 0.0 : 1.0);
}

static void
feat_clicked (GtkWidget *feat,
              gpointer   data)
{
  g_signal_handlers_block_by_func (feat, feat_clicked, NULL);

  if (gtk_check_button_get_inconsistent (GTK_CHECK_BUTTON (feat)))
    {
      set_inconsistent (GTK_CHECK_BUTTON (feat), FALSE);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (feat), TRUE);
    }
  else if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (feat)))
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (feat), FALSE);
    }
  else
    {
      set_inconsistent (GTK_CHECK_BUTTON (feat), TRUE);
    }

  g_signal_handlers_unblock_by_func (feat, feat_clicked, NULL);
}

static void
add_check_group (GtkFontChooserWidget *fontchooser,
                 const char  *title,
                 const char **tags)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  GtkWidget *label;
  GtkWidget *group;
  PangoAttrList *attrs;
  int i;

  group = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_halign (group, GTK_ALIGN_START);

  label = gtk_label_new (title);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  g_object_set (label, "margin-top", 10, "margin-bottom", 10, NULL);
  attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
  gtk_label_set_attributes (GTK_LABEL (label), attrs);
  pango_attr_list_unref (attrs);
  gtk_container_add (GTK_CONTAINER (group), label);

  for (i = 0; tags[i]; i++)
    {
      hb_tag_t tag;
      GtkWidget *feat;
      FeatureItem *item;

      tag = hb_tag_from_string (tags[i], -1);

      feat = gtk_check_button_new_with_label (get_feature_display_name (tag));
      set_inconsistent (GTK_CHECK_BUTTON (feat), TRUE);

      g_signal_connect_swapped (feat, "notify::active", G_CALLBACK (gtk_font_chooser_widget_update_preview_attributes), fontchooser);
      g_signal_connect_swapped (feat, "notify::inconsistent", G_CALLBACK (gtk_font_chooser_widget_update_preview_attributes), fontchooser);
      g_signal_connect (feat, "clicked", G_CALLBACK (feat_clicked), NULL);

      gtk_container_add (GTK_CONTAINER (group), feat);

      item = g_new (FeatureItem, 1);
      item->name = tags[i];
      item->tag = tag;
      item->feat = feat;

      priv->feature_items = g_list_prepend (priv->feature_items, item);
    }

  gtk_container_add (GTK_CONTAINER (priv->feature_box), group);
}

static void
add_radio_group (GtkFontChooserWidget *fontchooser,
                 const char  *title,
                 const char **tags)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  GtkWidget *label;
  GtkWidget *group;
  int i;
  GtkWidget *group_button = NULL;
  PangoAttrList *attrs;

  group = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_halign (group, GTK_ALIGN_START);

  label = gtk_label_new (title);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  g_object_set (label, "margin-top", 10, "margin-bottom", 10, NULL);
  attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
  gtk_label_set_attributes (GTK_LABEL (label), attrs);
  pango_attr_list_unref (attrs);
  gtk_container_add (GTK_CONTAINER (group), label);

  for (i = 0; tags[i]; i++)
    {
      hb_tag_t tag;
      GtkWidget *feat;
      FeatureItem *item;
      const char *name;

      tag = hb_tag_from_string (tags[i], -1);
      name = get_feature_display_name (tag);

      feat = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (group_button),
                                                          name ? name : _("Default"));
      if (group_button == NULL)
        group_button = feat;

      g_signal_connect_swapped (feat, "notify::active", G_CALLBACK (gtk_font_chooser_widget_update_preview_attributes), fontchooser);
      g_object_set_data (G_OBJECT (feat), "default", group_button);

      gtk_container_add (GTK_CONTAINER (group), feat);

      item = g_new (FeatureItem, 1);
      item->name = tags[i];
      item->tag = tag;
      item->feat = feat;

      priv->feature_items = g_list_prepend (priv->feature_items, item);
    }

  gtk_container_add (GTK_CONTAINER (priv->feature_box), group);
}

static void
gtk_font_chooser_widget_populate_features (GtkFontChooserWidget *fontchooser)
{
  const char *ligatures[] = { "liga", "dlig", "hlig", "clig", NULL };
  const char *letter_case[] = { "smcp", "c2sc", "pcap", "c2pc", "unic", "cpsp", "case", NULL };
  const char *number_case[] = { "xxxx", "lnum", "onum", NULL };
  const char *number_spacing[] = { "xxxx", "pnum", "tnum", NULL };
  const char *number_formatting[] = { "zero", "nalt", NULL };
  const char *char_variants[] = { "swsh", "cswh", "calt", "falt", "hist", "salt", "jalt", "titl", "rand", NULL };

  add_check_group (fontchooser, _("Ligatures"), ligatures);
  add_check_group (fontchooser, _("Letter Case"), letter_case);
  add_radio_group (fontchooser, _("Number Case"), number_case);
  add_radio_group (fontchooser, _("Number Spacing"), number_spacing);
  add_check_group (fontchooser, _("Number Formatting"), number_formatting);
  add_check_group (fontchooser, _("Character Variants"), char_variants);
}

static void
gtk_font_chooser_widget_update_font_features (GtkFontChooserWidget *fontchooser)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  PangoFont *pango_font;
  FT_Face ft_face;
  hb_font_t *hb_font;
  hb_tag_t script_tag = FT_MAKE_TAG('l','a','t','n');
  hb_tag_t lang_tag = FT_MAKE_TAG('D','E','U',' ');
  guint script_index = 0;
  guint lang_index = 0;
  int i, j;
  GList *l;

  for (l = priv->feature_items; l; l = l->next)
    {
      FeatureItem *item = l->data;
      gtk_widget_hide (item->feat);
      gtk_widget_hide (gtk_widget_get_parent (item->feat));
    }

  pango_font = pango_context_load_font (gtk_widget_get_pango_context (GTK_WIDGET (fontchooser)),
                                        priv->font_desc);
  ft_face = pango_fc_font_lock_face (PANGO_FC_FONT (pango_font)),
  hb_font = hb_ft_font_create (ft_face, NULL);

  if (hb_font)
    {
      hb_tag_t table[2] = { HB_OT_TAG_GSUB, HB_OT_TAG_GPOS };
      hb_face_t *hb_face;
      hb_tag_t features[80];
      unsigned int count;
      unsigned int n_features;

      hb_face = hb_font_get_face (hb_font);

      update_language_combo (fontchooser, hb_face);

      n_features = 0;
      for (i = 0; i < 2; i++)
        {
          hb_ot_layout_table_find_script (hb_face, table[i], script_tag, &script_index);
          hb_ot_layout_script_find_language (hb_face, table[i], script_index, lang_tag, &lang_index);
          count = G_N_ELEMENTS (features);
          hb_ot_layout_language_get_feature_tags (hb_face,
                                                  table[i],
                                                  script_index,
                                                  lang_index,
                                                  n_features,
                                                  &count,
                                                  features);
          n_features += count;
        }

      for (j = 0; j < n_features; j++)
        {
          for (l = priv->feature_items; l; l = l->next)
            {
              FeatureItem *item = l->data;
              if (item->tag != features[j])
                continue;

              gtk_widget_show (item->feat);
              gtk_widget_show (gtk_widget_get_parent (item->feat));

              if (GTK_IS_RADIO_BUTTON (item->feat))
                {
                  GtkWidget *def = GTK_WIDGET (g_object_get_data (G_OBJECT (item->feat), "default"));
                  gtk_widget_show (def);
                }
              else if (GTK_IS_CHECK_BUTTON (item->feat))
                {
                  set_inconsistent (GTK_CHECK_BUTTON (item->feat), TRUE);
                }
            }
        }

      hb_face_destroy (hb_face);
    }

  pango_fc_font_unlock_face (PANGO_FC_FONT (pango_font));
  g_object_unref (pango_font);
}

static void
collect_font_features (GtkFontChooserWidget *fontchooser,
                       PangoAttrList *attrs)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  GString *s;
  GList *l;
  GtkTreeIter iter;

  s = g_string_new ("kern 1, curs 1, lfbd 1, rfbd 1, mark 1, mkmk 1, mset 1, ccmp 1, rlig 1, rclt 1, rvrn 1");

  for (l = priv->feature_items; l; l = l->next)
    {
      FeatureItem *item = l->data;

      if (!gtk_widget_is_sensitive (item->feat))
        continue;

      if (GTK_IS_RADIO_BUTTON (item->feat))
        {
          if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (item->feat)) &&
              strcmp (item->name, "xxxx") != 0)
            {
              g_string_append (s, ", ");
              g_string_append (s, item->name);
              g_string_append (s, " 1");
            }
        }
      else if (GTK_IS_CHECK_BUTTON (item->feat))
        {
          if (gtk_check_button_get_inconsistent (GTK_CHECK_BUTTON (item->feat)))
            continue;

          g_string_append (s, ", ");
          g_string_append (s, item->name);
          if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (item->feat)))
            g_string_append (s, " 1");
          else
            g_string_append (s, " 0");
        }
    }

  pango_attr_list_insert (attrs, pango_attr_font_features_new (s->str));
  g_string_free (s, TRUE);

  if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (priv->feature_language_combo), &iter))
    {
      GtkTreeModel *model;
      const char *lang;
      hb_tag_t lang_tag;

      model = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->feature_language_combo));
      gtk_tree_model_get (model, &iter,
                          3, &lang_tag,
                          -1);
      lang = hb_language_to_string (hb_ot_tag_to_language (lang_tag));
      pango_attr_list_insert (attrs, pango_attr_language_new (pango_language_from_string (lang)));
    }
}

#endif

static void
gtk_font_chooser_widget_merge_font_desc (GtkFontChooserWidget       *fontchooser,
                                         const PangoFontDescription *font_desc,
                                         GtkTreeIter                *iter)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  PangoFontMask mask;

  g_assert (font_desc != NULL);
  /* iter may be NULL if the font doesn't exist on the list */

  mask = pango_font_description_get_set_fields (font_desc);

  /* sucky test, because we can't restrict the comparison to 
   * only the parts that actually do get merged */
  if (pango_font_description_equal (font_desc, priv->font_desc))
    return;

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

#if defined(HAVE_HARFBUZZ) && defined(HAVE_PANGOFT)
      gtk_font_chooser_widget_update_font_features (fontchooser);
      gtk_font_chooser_widget_update_font_variations (fontchooser);
#endif
    }

  gtk_font_chooser_widget_update_preview_attributes (fontchooser);

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

      if (gtk_font_chooser_widget_find_font (fontchooser, font_desc, &iter))
        gtk_font_chooser_widget_merge_font_desc (fontchooser, font_desc, &iter);
      else
        gtk_font_chooser_widget_merge_font_desc (fontchooser, font_desc, NULL);
    }
  else
    {
      gtk_font_chooser_widget_merge_font_desc (fontchooser, font_desc, &priv->font_iter);
    }

  pango_font_description_free (font_desc);
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
gtk_font_chooser_widget_set_font_map (GtkFontChooser *chooser,
                                      PangoFontMap   *fontmap)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (chooser);
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;

  if (g_set_object (&priv->font_map, fontmap))
    {
      PangoContext *context;

      if (!fontmap)
        fontmap = pango_cairo_font_map_get_default ();

      context = gtk_widget_get_pango_context (priv->family_face_list);
      pango_context_set_font_map (context, fontmap);

      context = gtk_widget_get_pango_context (priv->preview);
      pango_context_set_font_map (context, fontmap);

      gtk_font_chooser_widget_load_fonts (fontchooser, TRUE);
    }
}

static PangoFontMap *
gtk_font_chooser_widget_get_font_map (GtkFontChooser *chooser)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (chooser);
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;

  return priv->font_map;
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
gtk_font_chooser_widget_set_level (GtkFontChooserWidget *fontchooser,
                                   GtkFontChooserLevel   level)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;

  if (priv->level == level)
    return;

  priv->level = level;

  if (level == GTK_FONT_CHOOSER_LEVEL_FONT)
    {
      gtk_widget_show (priv->size_label);
      gtk_widget_show (priv->size_slider);
      gtk_widget_show (priv->size_spin);
    }
  else
   {
      gtk_widget_hide (priv->size_label);
      gtk_widget_hide (priv->size_slider);
      gtk_widget_hide (priv->size_spin);
   }

  gtk_font_chooser_widget_load_fonts (fontchooser, TRUE);

  g_object_notify (G_OBJECT (fontchooser), "level");
}

static GtkFontChooserLevel
gtk_font_chooser_widget_get_level (GtkFontChooserWidget *fontchooser)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;

  return priv->level;
}

static void
gtk_font_chooser_widget_iface_init (GtkFontChooserIface *iface)
{
  iface->get_font_family = gtk_font_chooser_widget_get_family;
  iface->get_font_face = gtk_font_chooser_widget_get_face;
  iface->get_font_size = gtk_font_chooser_widget_get_size;
  iface->set_filter_func = gtk_font_chooser_widget_set_filter_func;
  iface->set_font_map = gtk_font_chooser_widget_set_font_map;
  iface->get_font_map = gtk_font_chooser_widget_get_font_map;
}

gboolean
gtk_font_chooser_widget_handle_event (GtkWidget   *widget,
                                      GdkEventKey *key_event)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (widget);
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  GdkEvent *event = (GdkEvent *)key_event;

  if (gdk_event_get_event_type (event) == GDK_KEY_PRESS)
    {
      GdkModifierType state;
      guint keyval;

      gdk_event_get_state (event, &state);
      gdk_event_get_keyval (event, &keyval);

      if (gtk_widget_is_visible (priv->filter_popover))
        {
          if (gtk_revealer_get_child_revealed (GTK_REVEALER (priv->script_filter_revealer)))
            {
              gtk_widget_grab_focus (priv->script_filter_entry);
              return gtk_widget_event (priv->script_filter_entry, event);
            }

          if (gtk_revealer_get_child_revealed (GTK_REVEALER (priv->language_filter_revealer)))
            {
              gtk_widget_grab_focus (priv->language_filter_entry);
              return gtk_widget_event (priv->language_filter_entry, event);
            }

          if (keyval == GDK_KEY_Escape)
            {
              gtk_popover_popdown (GTK_POPOVER (priv->filter_popover));
              return GDK_EVENT_STOP;
            }
        }
      else
        {
          if ((state & GDK_MOD1_MASK) > 0 && keyval == GDK_KEY_Down)
            {
              gtk_popover_popup (GTK_POPOVER (priv->filter_popover));
              return GDK_EVENT_STOP;
            }
        }
    }

  return gtk_search_entry_handle_event (GTK_SEARCH_ENTRY (priv->search_entry), event);
}

void
gtk_font_chooser_widget_tweak_font (GtkWidget *widget,
                                    gboolean   tweak)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (widget);

  if (tweak)
    {
      gtk_entry_grab_focus_without_selecting (GTK_ENTRY (fontchooser->priv->preview2));
      gtk_stack_set_visible_child_name (GTK_STACK (fontchooser->priv->stack), "tweaks");
    }
  else
    {
      gtk_entry_grab_focus_without_selecting (GTK_ENTRY (fontchooser->priv->search_entry));
      gtk_stack_set_visible_child_name (GTK_STACK (fontchooser->priv->stack), "list");
    }
}
