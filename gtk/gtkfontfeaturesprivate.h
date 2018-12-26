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

#include "gtkfontchooserwidget.h"
#include "gtkfontchooserwidgetprivate.h"

#include "gtkadjustment.h"
#include "gtkfontchooser.h"
#include "gtkwidget.h"
#include "gtkspinbutton.h"
#include "gtktreeview.h"

struct _GtkFontChooserWidgetPrivate
{
  GtkWidget    *stack;
  GtkWidget    *search_entry;
  GtkWidget    *family_face_list;
  GtkTreeViewColumn *family_face_column;
  GtkCellRenderer *family_face_cell;
  GtkWidget    *list_scrolled_window;
  GtkWidget    *list_stack;
  GtkTreeModel *model;
  GtkTreeModel *filter_model;

  GtkWidget       *preview;
  GtkWidget       *preview2;
  GtkWidget       *font_name_label;
  gchar           *preview_text;
  gboolean         show_preview_entry;

  GtkWidget *size_spin;
  GtkWidget *size_slider;
  GtkWidget *size_slider2;

  GtkWidget *axis_grid;
  GtkWidget       *feature_box;

  PangoFontMap         *font_map;

  PangoFontDescription *font_desc;
  char                 *font_features;
  PangoLanguage        *language;
  GtkTreeIter           font_iter;      /* invalid if font not available or pointer into model
                                           (not filter_model) to the row containing font */
  GtkFontFilterFunc filter_func;
  gpointer          filter_data;
  GDestroyNotify    filter_data_destroy;

  guint last_fontconfig_timestamp;

  GtkFontChooserLevel level;

  GHashTable *axes;
  gboolean updating_variations;

  GList *feature_items;

  GAction *tweak_action;

  gpointer ft_ext_items;
};

typedef struct {
  guint32 tag;
  GtkAdjustment *adjustment;
  GtkWidget *label;
  GtkWidget *scale;
  GtkWidget *spin;
  GtkWidget *fontchooser;
} Axis;

void        gtk_font_chooser_widget_populate_features         (GtkFontChooserWidget *fontchooser);
void        gtk_font_chooser_widget_take_font_desc            (GtkFontChooserWidget *fontchooser,
                                                               PangoFontDescription *font_desc);
gboolean    gtk_font_chooser_widget_update_font_features      (GtkFontChooserWidget *fontchooser);
gboolean    gtk_font_chooser_widget_update_font_variations    (GtkFontChooserWidget *fontchooser);
void        gtk_font_chooser_widget_update_preview_attributes (GtkFontChooserWidget *fontchooser);
void        gtk_font_chooser_widget_release_extra_ft_items    (GtkFontChooserWidget *fontchooser);

#if defined (GDK_WINDOWING_WIN32) && (HAVE_HARFBUZZ)
#include <ft2build.h>
#include FT_FREETYPE_H

_GDK_EXTERN
FT_Face     gtk_font_chooser_widget_win32_acquire_ftface      (GtkFontChooserWidget *fontchooser,
                                                               PangoFont            *font);

_GDK_EXTERN
void        gtk_font_chooser_widget_win32_release_ftface      (GtkFontChooserWidget *fontchooser);
#endif

gboolean    output_cb              (GtkSpinButton *spin,
                                    gpointer       data);
