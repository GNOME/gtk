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

#include "gtkentry.h"

#include "gtkentrycompletion.h"
#include "gtkeventcontrollermotion.h"
#include "gtkliststore.h"
#include "gtktreemodelfilter.h"
#include "gtktreeviewcolumn.h"
#include "gtkeventcontrollerkey.h"
#include "gtktextprivate.h"

G_BEGIN_DECLS

typedef struct _GtkEntryCompletionClass       GtkEntryCompletionClass;
typedef struct _GtkEntryCompletionPrivate     GtkEntryCompletionPrivate;

struct _GtkEntryCompletion
{
  GObject parent_instance;

  /*< private >*/
  GtkEntryCompletionPrivate *priv;
};

struct _GtkEntryCompletionClass
{
  GObjectClass parent_class;

  gboolean (* match_selected)   (GtkEntryCompletion *completion,
                                 GtkTreeModel       *model,
                                 GtkTreeIter        *iter);
  void     (* action_activated) (GtkEntryCompletion *completion,
                                 gint                index_);
  gboolean (* insert_prefix)    (GtkEntryCompletion *completion,
                                 const gchar        *prefix);
  gboolean (* cursor_on_match)  (GtkEntryCompletion *completion,
                                 GtkTreeModel       *model,
                                 GtkTreeIter        *iter);
  void     (* no_matches)       (GtkEntryCompletion *completion);
};

struct _GtkEntryCompletionPrivate
{
  GtkWidget *entry;

  GtkWidget *tree_view;
  GtkTreeViewColumn *column;
  GtkTreeModelFilter *filter_model;
  GtkListStore *actions;
  GtkCellArea *cell_area;

  GtkEntryCompletionMatchFunc match_func;
  gpointer match_data;
  GDestroyNotify match_notify;

  gint minimum_key_length;
  gint text_column;

  gchar *case_normalized_key;

  GtkEventController *entry_key_controller;

  /* only used by GtkEntry when attached: */
  GtkWidget *popup_window;
  GtkWidget *vbox;
  GtkWidget *scrolled_window;
  GtkWidget *action_view;

  gulong completion_timeout;
  gulong changed_id;
  gulong insert_text_id;

  gint current_selected;

  guint first_sel_changed : 1;
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

void     _gtk_entry_completion_resize_popup (GtkEntryCompletion *completion);
void     _gtk_entry_completion_popdown      (GtkEntryCompletion *completion);
void     _gtk_entry_completion_connect      (GtkEntryCompletion *completion,
                                             GtkEntry           *entry);
void     _gtk_entry_completion_disconnect   (GtkEntryCompletion *completion);

GtkIMContext* _gtk_entry_get_im_context    (GtkEntry  *entry);
void     gtk_entry_enter_text              (GtkEntry   *entry,
                                            const char *text);

GtkEventController * gtk_entry_get_key_controller (GtkEntry *entry);
GtkText *gtk_entry_get_text_widget (GtkEntry *entry);

G_END_DECLS

#endif /* __GTK_ENTRY_PRIVATE_H__ */
