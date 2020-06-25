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

struct _GtkEntryCompletion
{
  GObject parent_instance;

  GtkWidget *entry;

  GListModel *filter_model;
  GtkExpression *expression;
  GtkListItemFactory *factory;

  gint minimum_key_length;
  char *case_normalized_key;

  GtkEventController *entry_key_controller;
  GtkEventController *entry_focus_controller;

  GtkWidget *popup_window;
  GtkWidget *scrolled_window;
  GtkWidget *list_view;

  gulong completion_timeout;
  gulong changed_id;
  gulong insert_text_id;

  int current_selected;

  guint has_completion     : 1;
  guint inline_completion  : 1;
  guint popup_completion   : 1;
  guint popup_set_width    : 1;
  guint popup_single_match : 1;
  guint inline_selection   : 1;

  char *completion_prefix;

  GSource *check_completion_idle;
};

struct _GtkEntryCompletionClass
{
  GObjectClass parent_class;

  gboolean (* match_selected)   (GtkEntryCompletion *completion,
                                 guint               position);
  gboolean (* insert_prefix)    (GtkEntryCompletion *completion,
                                 const char         *prefix);
  gboolean (* cursor_on_match)  (GtkEntryCompletion *completion,
                                 guint               position);
  void     (* no_matches)       (GtkEntryCompletion *completion);
};

void     gtk_entry_completion_resize_popup (GtkEntryCompletion *completion);
void     gtk_entry_completion_popdown      (GtkEntryCompletion *completion);
void     gtk_entry_completion_connect      (GtkEntryCompletion *completion,
                                            GtkEntry           *entry);
void     gtk_entry_completion_disconnect   (GtkEntryCompletion *completion);

GtkIMContext * _gtk_entry_get_im_context    (GtkEntry  *entry);
GtkEventController * gtk_entry_get_key_controller (GtkEntry *entry);
GtkText *gtk_entry_get_text_widget (GtkEntry *entry);

G_END_DECLS

#endif /* __GTK_ENTRY_PRIVATE_H__ */
