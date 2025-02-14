/* gtk-im-context-preview.c
 *
 * Copyright 2025 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gtkimcontextpreview-private.h"

typedef enum
{
  CONTEXT_STATE_INITIAL              = 0,
  CONTEXT_STATE_PREVIEW_ONLY         = 1 << 0,
  CONTEXT_STATE_PREEDIT_ONLY         = 1 << 1,
  CONTEXT_STATE_PREEDIT_WITH_PREVIEW = CONTEXT_STATE_PREVIEW_ONLY | CONTEXT_STATE_PREEDIT_ONLY,
} ContextState;

struct _GtkIMContextPreview
{
  GtkIMContext  parent_instance;

  GtkIMContext *im_context;

  PangoAttrList *suffix_attrs;
  char *suffix;

  /* Our current state which we use to transition between our known
   * "preedit" values and any "preedit" coming from the sub-IMContext.
   */
  ContextState state;

  /* Track the the "preedit-start" / "preedit-end" count from the sub
   * IM Context so that we can calculate state properly.
   */
  int im_context_preedit_count;

  /* Make sure we avoid re-entrancy */
  int reentrant_check : 1;
};

enum {
  PROP_0,
  PROP_IM_CONTEXT,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GtkIMContextPreview, gtk_im_context_preview, GTK_TYPE_IM_CONTEXT)

static GParamSpec *properties[N_PROPS];

static void
gtk_im_context_preview_transition_out (GtkIMContextPreview *self)
{
  ContextState old_state;

  g_assert (GTK_IS_IM_CONTEXT_PREVIEW (self));

  old_state = self->state;
  self->state = CONTEXT_STATE_INITIAL;

  switch (old_state)
    {
    case CONTEXT_STATE_INITIAL:
      break;

    case CONTEXT_STATE_PREEDIT_ONLY:
    case CONTEXT_STATE_PREVIEW_ONLY:
    case CONTEXT_STATE_PREEDIT_WITH_PREVIEW:
      g_signal_emit_by_name (self, "preedit-end");
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
gtk_im_context_preview_transition_in (GtkIMContextPreview *self)
{
  g_assert (GTK_IS_IM_CONTEXT_PREVIEW (self));

  switch (self->state)
    {
    case CONTEXT_STATE_INITIAL:
      break;

    case CONTEXT_STATE_PREEDIT_ONLY:
    case CONTEXT_STATE_PREVIEW_ONLY:
    case CONTEXT_STATE_PREEDIT_WITH_PREVIEW:
      g_signal_emit_by_name (self, "preedit-start");
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
gtk_im_context_preview_transition (GtkIMContextPreview *self)
{
  ContextState new_state = CONTEXT_STATE_INITIAL;

  g_assert (GTK_IS_IM_CONTEXT_PREVIEW (self));

  if (self->reentrant_check)
    return;

  self->reentrant_check = TRUE;

  if (self->im_context_preedit_count > 0)
    new_state |= CONTEXT_STATE_PREEDIT_ONLY;

  if (self->suffix != NULL)
    new_state |= CONTEXT_STATE_PREVIEW_ONLY;

  /* Only skip operations when both old and new state are initial. Otherwise
   * we really need to update to ensure that our initial state is
   * propagated to the widget.
   */
  if (new_state || self->state)
    {
      gtk_im_context_preview_transition_out (self);
      self->state = new_state;
      gtk_im_context_preview_transition_in (self);
    }

  self->reentrant_check = FALSE;
}

static gboolean
gtk_im_context_preview_filter_keypress (GtkIMContext *context,
                                        GdkEvent     *event)
{
  GtkIMContextPreview *self = GTK_IM_CONTEXT_PREVIEW (context);

  if (self->state == CONTEXT_STATE_PREVIEW_ONLY)
    {
#if 0
      switch (gdk_key_event_get_keyval (event))
        {
        case GDK_KEY_Up:
        case GDK_KEY_Down:
          return FALSE;
        }
#endif
    }

  return gtk_im_context_filter_keypress (self->im_context, event);
}

static void
gtk_im_context_preview_reset (GtkIMContext *context)
{
  GtkIMContextPreview *self = GTK_IM_CONTEXT_PREVIEW (context);

  gtk_im_context_reset (self->im_context);

  gtk_im_context_preview_transition (self);
}

static void
gtk_im_context_preview_set_client_widget (GtkIMContext *context,
                                          GtkWidget    *widget)
{
  GtkIMContextPreview *self = GTK_IM_CONTEXT_PREVIEW (context);

  gtk_im_context_set_client_widget (self->im_context, widget);
}

static void
gtk_im_context_preview_focus_in (GtkIMContext *context)
{
  GtkIMContextPreview *self = GTK_IM_CONTEXT_PREVIEW (context);

  g_print ("FOCUS IN!\n");

  gtk_im_context_focus_in (self->im_context);

  gtk_im_context_preview_transition (self);
}

static void
gtk_im_context_preview_focus_out (GtkIMContext *context)
{
  GtkIMContextPreview *self = GTK_IM_CONTEXT_PREVIEW (context);

  gtk_im_context_focus_out (self->im_context);
}

static void
gtk_im_context_preview_set_cursor_location (GtkIMContext *context,
                                            GdkRectangle *area)
{
  GtkIMContextPreview *self = GTK_IM_CONTEXT_PREVIEW (context);

  gtk_im_context_set_cursor_location (self->im_context, area);
}

static void
gtk_im_context_preview_set_use_preedit (GtkIMContext *context,
                                        gboolean      use_preedit)
{
  GtkIMContextPreview *self = GTK_IM_CONTEXT_PREVIEW (context);

  gtk_im_context_set_use_preedit (self->im_context, use_preedit);
}

static void
gtk_im_context_preview_get_preedit_string (GtkIMContext   *context,
                                           char          **str,
                                           PangoAttrList **attrs,
                                           int            *cursor_pos)
{
  GtkIMContextPreview *self = GTK_IM_CONTEXT_PREVIEW (context);
  PangoAttrList *sub_attrs = NULL;
  char *sub_str = NULL;
  int sub_cursor_pos = 0;
  int suffix_offset = 0;

  if (self->state & CONTEXT_STATE_PREEDIT_ONLY)
    {
      gtk_im_context_get_preedit_string (self->im_context, &sub_str, &sub_attrs, &sub_cursor_pos);

      if (sub_str)
        suffix_offset = g_utf8_strlen (sub_str, -1);
    }

  if (self->state & CONTEXT_STATE_PREVIEW_ONLY)
    {
      char *tmp;

      tmp = g_steal_pointer (&sub_str);
      sub_str = g_strconcat (tmp ? tmp : "", self->suffix, NULL);
      g_free (tmp);

      if (self->suffix_attrs)
        {
          GSList *list = pango_attr_list_get_attributes (self->suffix_attrs);

          if (sub_attrs == NULL)
            sub_attrs = pango_attr_list_new ();

          for (const GSList *iter = list; iter; iter = iter->next)
            {
              PangoAttribute *copy = pango_attribute_copy (iter->data);

              copy->start_index += suffix_offset;

              if (copy->end_index > 0 && copy->end_index < G_MAXINT - copy->end_index)
                copy->end_index += suffix_offset;

              pango_attr_list_insert (sub_attrs, g_steal_pointer (&copy));
            }

          g_slist_free_full (list, (GDestroyNotify)pango_attribute_destroy);
        }
    }

  *str = g_steal_pointer (&sub_str);
  *attrs = g_steal_pointer (&sub_attrs);
  *cursor_pos = sub_cursor_pos;
}

static gboolean
gtk_im_context_preview_get_surrounding (GtkIMContext  *context,
                                        char         **text,
                                        int           *cursor_index)
{
  GtkIMContextPreview *self = GTK_IM_CONTEXT_PREVIEW (context);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  return gtk_im_context_get_surrounding (self->im_context, text, cursor_index);
  G_GNUC_END_IGNORE_DEPRECATIONS
}

static gboolean
gtk_im_context_preview_get_surrounding_with_selection (GtkIMContext  *context,
                                                       char         **text,
                                                       int           *cursor_index,
                                                       int           *anchor_index)
{
  GtkIMContextPreview *self = GTK_IM_CONTEXT_PREVIEW (context);
  char *preedit = NULL;
  int real_cursor_index = 0;
  int real_anchor_index = 0;
  gboolean ret = FALSE;

  if (self->state & CONTEXT_STATE_PREEDIT_ONLY)
    ret = gtk_im_context_get_surrounding_with_selection (self->im_context, &preedit, &real_cursor_index, &real_anchor_index);

  if (self->state & CONTEXT_STATE_PREVIEW_ONLY)
    {
      char *tmp = g_steal_pointer (&preedit);

      if (tmp)
        preedit = g_strconcat (preedit, self->suffix, NULL);
      else
        preedit = g_strdup (self->suffix);

      g_free (tmp);

      ret = TRUE;
    }

  if (cursor_index != NULL)
    *cursor_index = real_cursor_index;

  if (anchor_index != NULL)
    *anchor_index = real_anchor_index;

  return ret;
}

static void
gtk_im_context_preview_set_surrounding (GtkIMContext *context,
                                        const char   *text,
                                        int           len,
                                        int           cursor_index)
{
  GtkIMContextPreview *self = GTK_IM_CONTEXT_PREVIEW (context);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_im_context_set_surrounding (self->im_context, text, len, cursor_index);
  G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
gtk_im_context_preview_set_surrounding_with_selection (GtkIMContext *context,
                                                       const char   *text,
                                                       int           len,
                                                       int           cursor_index,
                                                       int           anchor_index)
{
  GtkIMContextPreview *self = GTK_IM_CONTEXT_PREVIEW (context);

  gtk_im_context_set_surrounding_with_selection (self->im_context, text, len, cursor_index, anchor_index);
}

static void
gtk_im_context_preview_activate_osk (GtkIMContext *context)
{
  GtkIMContextPreview *self = GTK_IM_CONTEXT_PREVIEW (context);

  gtk_im_context_activate_osk (self->im_context, NULL);
}

static gboolean
gtk_im_context_preview_activate_osk_with_event (GtkIMContext *context,
                                                GdkEvent     *event)
{
  GtkIMContextPreview *self = GTK_IM_CONTEXT_PREVIEW (context);

  return gtk_im_context_activate_osk (self->im_context, event);
}

static void
gtk_im_context_preview_preedit_start (GtkIMContext *context)
{
  GtkIMContextPreview *self = GTK_IM_CONTEXT_PREVIEW (context);

  self->im_context_preedit_count++;

  gtk_im_context_preview_transition (GTK_IM_CONTEXT_PREVIEW (context));
}

static void
gtk_im_context_preview_preedit_end (GtkIMContext *context)
{
  GtkIMContextPreview *self = GTK_IM_CONTEXT_PREVIEW (context);

  g_return_if_fail (self->im_context_preedit_count > 0);

  self->im_context_preedit_count--;

  gtk_im_context_preview_transition (GTK_IM_CONTEXT_PREVIEW (context));
}

static void
gtk_im_context_preview_preedit_changed (GtkIMContext *context)
{
  g_signal_emit_by_name (context, "preedit-changed");
}

static void
gtk_im_context_preview_commit (GtkIMContext *context,
                               const char   *str)
{
  g_signal_emit_by_name (context, "commit", str);
}

static gboolean
gtk_im_context_preview_retrieve_surrounding (GtkIMContext *context)
{
  gboolean ret = FALSE;
  g_signal_emit_by_name (context, "retrieve-surrounding", &ret);
  return ret;
}

static gboolean
gtk_im_context_preview_delete_surrounding (GtkIMContext *context,
                                           int           offset,
                                           int           n_chars)
{
  gboolean ret = FALSE;
  g_signal_emit_by_name (context, "delete-surrounding", offset, n_chars, &ret);
  return ret;
}

static void
gtk_im_context_preview_constructed (GObject *object)
{
  GtkIMContextPreview *self = (GtkIMContextPreview *)object;

  G_OBJECT_CLASS (gtk_im_context_preview_parent_class)->constructed (object);

  g_signal_connect_object (self->im_context,
                           "preedit-start",
                           G_CALLBACK (gtk_im_context_preview_preedit_start),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->im_context,
                           "preedit-end",
                           G_CALLBACK (gtk_im_context_preview_preedit_end),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->im_context,
                           "preedit-changed",
                           G_CALLBACK (gtk_im_context_preview_preedit_changed),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->im_context,
                           "commit",
                           G_CALLBACK (gtk_im_context_preview_commit),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->im_context,
                           "retrieve-surrounding",
                           G_CALLBACK (gtk_im_context_preview_retrieve_surrounding),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->im_context,
                           "delete-surrounding",
                           G_CALLBACK (gtk_im_context_preview_delete_surrounding),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
gtk_im_context_preview_dispose (GObject *object)
{
  GtkIMContextPreview *self = (GtkIMContextPreview *)object;

  g_clear_object (&self->im_context);
  g_clear_pointer (&self->suffix, g_free);
  g_clear_pointer (&self->suffix_attrs, pango_attr_list_unref);

  G_OBJECT_CLASS (gtk_im_context_preview_parent_class)->dispose (object);
}

static void
gtk_im_context_preview_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GtkIMContextPreview *self = GTK_IM_CONTEXT_PREVIEW (object);

  switch (prop_id)
    {
    case PROP_IM_CONTEXT:
      g_value_set_object (value, self->im_context);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_im_context_preview_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkIMContextPreview *self = GTK_IM_CONTEXT_PREVIEW (object);

  switch (prop_id)
    {
    case PROP_IM_CONTEXT:
      self->im_context = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_im_context_preview_class_init (GtkIMContextPreviewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkIMContextClass *im_context_class = GTK_IM_CONTEXT_CLASS (klass);

  object_class->constructed = gtk_im_context_preview_constructed;
  object_class->dispose = gtk_im_context_preview_dispose;
  object_class->get_property = gtk_im_context_preview_get_property;
  object_class->set_property = gtk_im_context_preview_set_property;

  im_context_class->activate_osk = gtk_im_context_preview_activate_osk;
  im_context_class->activate_osk_with_event = gtk_im_context_preview_activate_osk_with_event;
  im_context_class->filter_keypress = gtk_im_context_preview_filter_keypress;
  im_context_class->focus_in = gtk_im_context_preview_focus_in;
  im_context_class->focus_out = gtk_im_context_preview_focus_out;
  im_context_class->get_preedit_string = gtk_im_context_preview_get_preedit_string;
  im_context_class->get_surrounding = gtk_im_context_preview_get_surrounding;
  im_context_class->get_surrounding_with_selection = gtk_im_context_preview_get_surrounding_with_selection;
  im_context_class->reset = gtk_im_context_preview_reset;
  im_context_class->set_client_widget = gtk_im_context_preview_set_client_widget;
  im_context_class->set_cursor_location = gtk_im_context_preview_set_cursor_location;
  im_context_class->set_surrounding = gtk_im_context_preview_set_surrounding;
  im_context_class->set_surrounding_with_selection = gtk_im_context_preview_set_surrounding_with_selection;
  im_context_class->set_use_preedit = gtk_im_context_preview_set_use_preedit;

  properties[PROP_IM_CONTEXT] =
    g_param_spec_object ("im-context", NULL, NULL,
                         GTK_TYPE_IM_CONTEXT,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gtk_im_context_preview_init (GtkIMContextPreview *self)
{
}

GtkIMContext *
gtk_im_context_preview_new (GtkIMContext *im_context)
{
  g_return_val_if_fail (GTK_IS_IM_CONTEXT (im_context), NULL);

  return g_object_new (GTK_TYPE_IM_CONTEXT_PREVIEW,
                       "im-context", im_context,
                       NULL);
}

void
gtk_im_context_preview_set_suffix (GtkIMContextPreview *self,
                                   const char          *suffix,
                                   PangoAttrList       *suffix_attrs)
{
  g_return_if_fail (GTK_IS_IM_CONTEXT_PREVIEW (self));

  if (suffix != NULL && suffix[0] == 0)
    suffix = NULL;

  g_set_str (&self->suffix, suffix);
  g_clear_pointer (&self->suffix_attrs, pango_attr_list_unref);
  self->suffix_attrs = suffix_attrs;

  gtk_im_context_preview_transition (self);
}
