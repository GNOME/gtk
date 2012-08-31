/* gtkentryprivate.h
 * Copyright (C) 2003  Kristian Rietveld  <kris@gtk.org>
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

#ifndef __GTK_ENTRY_PRIVATE_H__
#define __GTK_ENTRY_PRIVATE_H__

#include <gtk/gtktreeviewcolumn.h>
#include <gtk/gtktreemodelfilter.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtkentrycompletion.h>
#include <gtk/gtkentry.h>

G_BEGIN_DECLS

struct _GtkEntryCompletionPrivate
{
  GtkWidget *entry;

  GtkWidget *tree_view;
  GtkTreeViewColumn *column;
  GtkTreeModelFilter *filter_model;
  GtkListStore *actions;
  gboolean first_sel_changed;
  GtkCellArea *cell_area;

  GtkEntryCompletionMatchFunc match_func;
  gpointer match_data;
  GDestroyNotify match_notify;

  gint minimum_key_length;
  gint text_column;
  gint current_selected;

  gchar *case_normalized_key;

  /* only used by GtkEntry when attached: */
  GtkWidget *popup_window;
  GtkWidget *vbox;
  GtkWidget *scrolled_window;
  GtkWidget *action_view;

  gulong completion_timeout;
  gulong changed_id;
  gulong insert_text_id;

  guint ignore_enter      : 1;
  guint has_completion    : 1;
  guint inline_completion : 1;
  guint popup_completion  : 1;
  guint popup_set_width   : 1;
  guint popup_single_match : 1;
  guint inline_selection   : 1;
  guint has_grab           : 1;

  gchar *completion_prefix;

  GSource *check_completion_idle;

  GdkDevice *device;
};

gboolean _gtk_entry_completion_resize_popup (GtkEntryCompletion *completion);
void     _gtk_entry_completion_popdown      (GtkEntryCompletion *completion);
void     _gtk_entry_completion_connect      (GtkEntryCompletion *completion,
                                             GtkEntry           *entry);
void     _gtk_entry_completion_disconnect   (GtkEntryCompletion *completion);

gchar*   _gtk_entry_get_display_text       (GtkEntry *entry,
                                            gint      start_pos,
                                            gint      end_pos);
void     _gtk_entry_get_borders            (GtkEntry  *entry,
                                            GtkBorder *borders);
GtkIMContext* _gtk_entry_get_im_context    (GtkEntry  *entry);
void     _gtk_entry_set_is_cell_renderer   (GtkEntry  *entry,
                                            gboolean   is_cell_renderer);


G_END_DECLS

#endif /* __GTK_ENTRY_PRIVATE_H__ */
