/* GTK - The GIMP Toolkit
 * Copyright (C) 2017 Red Hat, Inc.
 * Copyright (C) 2018 Purism SPC
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

#include <string.h>
#include <wayland-client-protocol.h>

#include <gtk/gtk.h>
#include "gtk/gtkintl.h"
#include "gtk/gtkimmodule.h"

#include "gdk/wayland/gdkwayland.h"
#include "text-input-unstable-v3-client-protocol.h"

typedef struct _GtkIMContextWaylandGlobal GtkIMContextWaylandGlobal;
typedef struct _GtkIMContextWayland GtkIMContextWayland;
typedef struct _GtkIMContextWaylandClass GtkIMContextWaylandClass;

struct _GtkIMContextWaylandGlobal
{
  struct wl_display *display;
  struct wl_registry *registry;
  uint32_t text_input_manager_wl_id;
  struct zwp_text_input_manager_v3 *text_input_manager;
  struct zwp_text_input_v3 *text_input;

  GtkIMContext *current;

  /* The input-method.enter event may happen before or after GTK focus-in,
   * so the context may not exist at the time. Same for leave and focus-out. */
  gboolean focused;

  guint serial;
  guint done_serial;
};

struct _GtkIMContextWaylandClass
{
  GtkIMContextSimpleClass parent_class;
};

struct preedit {
  gchar *text;
  gint cursor_begin;
  gint cursor_end;
};

struct surrounding_delete {
  guint before_length;
  guint after_length;
};

struct _GtkIMContextWayland
{
  GtkIMContextSimple parent_instance;
  GdkWindow *window;
  GtkWidget *widget;

  GtkGesture *gesture;
  gdouble press_x;
  gdouble press_y;

  struct {
    gchar *text;
    gint cursor_idx;
    gint anchor_idx;
  } surrounding;

  enum zwp_text_input_v3_change_cause surrounding_change;

  struct surrounding_delete pending_surrounding_delete;

  struct preedit current_preedit;
  struct preedit pending_preedit;

  gchar *pending_commit;

  cairo_rectangle_int_t cursor_rect;
  guint use_preedit : 1;
  guint enabled : 1;
};

GType type_wayland = 0;
static GObjectClass *parent_class;
static GtkIMContextWaylandGlobal *global = NULL;

static const GtkIMContextInfo imwayland_info =
{
  "wayland",      /* ID */
  NC_("input method menu", "Wayland"),      /* Human readable name */
  GETTEXT_PACKAGE, /* Translation domain */
  GTK_LOCALEDIR,   /* Dir for bindtextdomain (not strictly needed for "gtk+") */
  "",              /* Languages for which this module is the default */
};

static const GtkIMContextInfo *info_list[] =
{
  &imwayland_info,
};

static void gtk_im_context_wayland_focus_out (GtkIMContext *context);

static void commit_state (GtkIMContextWayland *context);
static void notify_surrounding_text (GtkIMContextWayland *context);
static void notify_cursor_location (GtkIMContextWayland *context);
static void notify_content_type (GtkIMContextWayland *context);

#define GTK_IM_CONTEXT_WAYLAND(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), type_wayland, GtkIMContextWayland))

#ifndef INCLUDE_IM_wayland
#define MODULE_ENTRY(type,function) G_MODULE_EXPORT type im_module_ ## function
#else
#define MODULE_ENTRY(type, function) type _gtk_immodule_wayland_ ## function
#endif

static void
notify_im_change (GtkIMContextWayland                 *context,
                  enum zwp_text_input_v3_change_cause  cause)
{
  gboolean result;

  if (!global->current)
    return;

  if (!context->enabled)
    return;

  context->surrounding_change = cause;
  g_signal_emit_by_name (global->current, "retrieve-surrounding", &result);
  notify_surrounding_text (context);
  notify_content_type (context);
  notify_cursor_location (context);
  commit_state (context);
}

static void
text_input_preedit (void                     *data,
                    struct zwp_text_input_v3 *text_input,
                    const char               *text,
                    gint                      cursor_begin,
                    gint                      cursor_end)
{
  GtkIMContextWayland *context;
  GtkIMContextWaylandGlobal *global = data;

  if (!global->current)
    return;

  context = GTK_IM_CONTEXT_WAYLAND (global->current);
      
  g_free (context->pending_preedit.text);
  context->pending_preedit.text = g_strdup (text);
  context->pending_preedit.cursor_begin = cursor_begin;
  context->pending_preedit.cursor_end = cursor_end;
}

static void
text_input_preedit_apply (GtkIMContextWaylandGlobal *global)
{
  GtkIMContextWayland *context;
  gboolean state_change;
  struct preedit defaults = {0};

  if (!global->current)
    return;

  context = GTK_IM_CONTEXT_WAYLAND (global->current);
  if (context->pending_preedit.text == NULL &&
      context->current_preedit.text == NULL)
    return;

  state_change = ((context->pending_preedit.text == NULL)
                 != (context->current_preedit.text == NULL));

  if (state_change && !context->current_preedit.text)
    g_signal_emit_by_name (context, "preedit-start");

  g_free (context->current_preedit.text);
  context->current_preedit = context->pending_preedit;
  context->pending_preedit = defaults;

  g_signal_emit_by_name (context, "preedit-changed");

  if (state_change && !context->current_preedit.text)
    g_signal_emit_by_name (context, "preedit-end");
}

static void
text_input_commit (void                     *data,
                   struct zwp_text_input_v3 *text_input,
                   const char               *text)
{
  GtkIMContextWaylandGlobal *global = data;
  GtkIMContextWayland *context;

  if (!global->current)
      return;
  
  context = GTK_IM_CONTEXT_WAYLAND (global->current);

  g_free (context->pending_commit);
  context->pending_commit = g_strdup (text);
}

static void
text_input_commit_apply (GtkIMContextWaylandGlobal *global)
{
  GtkIMContextWayland *context;
  context = GTK_IM_CONTEXT_WAYLAND (global->current);
  if (context->pending_commit)
    g_signal_emit_by_name (global->current, "commit", context->pending_commit);
  g_free (context->pending_commit);
  context->pending_commit = NULL;
}

static void
text_input_delete_surrounding_text (void                     *data,
                                    struct zwp_text_input_v3 *text_input,
                                    uint32_t                  before_length,
                                    uint32_t                  after_length)
{
  GtkIMContextWaylandGlobal *global = data;
  GtkIMContextWayland *context;
  char *cursor_pointer;
  uint32_t char_before_length;
  uint32_t char_after_length;

  if (!global->current)
      return;
  
  context = GTK_IM_CONTEXT_WAYLAND (global->current);

  /* We already got byte lengths from text_input_v3, but GTK uses char lengths
   * for delete_surrounding, So convert it here.
   */
  cursor_pointer = context->surrounding.text + context->surrounding.cursor_idx;
  char_before_length = g_utf8_pointer_to_offset (cursor_pointer - before_length, cursor_pointer);
  char_after_length = g_utf8_pointer_to_offset (cursor_pointer, cursor_pointer + after_length);

  context->pending_surrounding_delete.before_length = char_before_length;
  context->pending_surrounding_delete.after_length = char_after_length;
}

static void
text_input_delete_surrounding_text_apply (GtkIMContextWaylandGlobal *global)
{
  GtkIMContextWayland *context;
  gboolean retval;
  gint len;
  struct surrounding_delete defaults = {0};

  context = GTK_IM_CONTEXT_WAYLAND (global->current);

  len = context->pending_surrounding_delete.after_length
      + context->pending_surrounding_delete.before_length;
  if (len > 0)
    {
      g_signal_emit_by_name (global->current, "delete-surrounding",
                             -context->pending_surrounding_delete.before_length,
                             len, &retval);
      notify_im_change (GTK_IM_CONTEXT_WAYLAND (context),
                        ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_INPUT_METHOD);
    }

  context->pending_surrounding_delete = defaults;
}

static void
text_input_done (void                     *data,
                 struct zwp_text_input_v3 *text_input,
                 uint32_t                  serial)
{
  GtkIMContextWaylandGlobal *global = data;
  GtkIMContextWayland *context;
  gboolean update_im;

  global->done_serial = serial;

  if (!global->current)
    return;

  context = GTK_IM_CONTEXT_WAYLAND (global->current);
  update_im = (context->pending_commit != NULL ||
               g_strcmp0 (context->pending_preedit.text,
                          context->current_preedit.text) != 0);

  text_input_delete_surrounding_text_apply (global);
  text_input_commit_apply (global);
  text_input_preedit_apply (global);

  if (update_im && global->serial == serial)
    notify_im_change (context, ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_INPUT_METHOD);
}

static void
notify_surrounding_text (GtkIMContextWayland *context)
{
#define MAX_LEN 4000
  const gchar *start, *end;
  int len, cursor, anchor;
  char *str = NULL;

  if (!global || !global->text_input)
    return;
  if (global->current != GTK_IM_CONTEXT (context))
    return;
  if (!context->enabled || !context->surrounding.text)
    return;

  len = strlen (context->surrounding.text);
  cursor = context->surrounding.cursor_idx;
  anchor = context->surrounding.anchor_idx;

  /* The protocol specifies a maximum length of 4KiB on transfers,
   * mangle the surrounding text if it's bigger than that, and relocate
   * cursor/anchor locations as per the string being sent.
   */
  if (len > MAX_LEN)
    {
      if (context->surrounding.cursor_idx < MAX_LEN &&
          context->surrounding.anchor_idx < MAX_LEN)
        {
          start = context->surrounding.text;
          end = &context->surrounding.text[MAX_LEN];
        }
      else if (context->surrounding.cursor_idx > len - MAX_LEN &&
               context->surrounding.anchor_idx > len - MAX_LEN)
        {
          start = &context->surrounding.text[len - MAX_LEN];
          end = &context->surrounding.text[len];
        }
      else
        {
          int mid, a, b;
          int cursor_len = ABS (context->surrounding.cursor_idx -
                                context->surrounding.anchor_idx);

          if (cursor_len > MAX_LEN)
            {
              g_warn_if_reached ();
              return;
            }

          mid = MIN (context->surrounding.cursor_idx,
                     context->surrounding.anchor_idx) + (cursor_len / 2);
          a = MAX (0, mid - (MAX_LEN / 2));
          b = MIN (len, mid + (MAX_LEN / 2));

          start = &context->surrounding.text[a];
          end = &context->surrounding.text[b];
        }

      if (start != context->surrounding.text)
        start = g_utf8_next_char (start);
      if (end != &context->surrounding.text[len])
        end = g_utf8_find_prev_char (context->surrounding.text, end);

      cursor -= start - context->surrounding.text;
      anchor -= start - context->surrounding.text;

      str = g_strndup (start, end - start);
    }

  zwp_text_input_v3_set_surrounding_text (global->text_input,
                                          str ? str : context->surrounding.text,
                                          cursor, anchor);
  zwp_text_input_v3_set_text_change_cause (global->text_input,
                                           context->surrounding_change);
  g_free (str);
#undef MAX_LEN
}

static void
notify_cursor_location (GtkIMContextWayland *context)
{
  cairo_rectangle_int_t rect;

  if (!global || !global->text_input)
    return;
  if (global->current != GTK_IM_CONTEXT (context))
    return;
  if (!context->enabled || !context->window)
    return;

  rect = context->cursor_rect;
  gdk_window_get_root_coords (context->window, rect.x, rect.y,
                              &rect.x, &rect.y);

  zwp_text_input_v3_set_cursor_rectangle (global->text_input,
                                          rect.x, rect.y,
                                          rect.width, rect.height);
}

static uint32_t
translate_hints (GtkInputHints   input_hints,
                 GtkInputPurpose purpose)
{
  uint32_t hints = 0;

  if (input_hints & GTK_INPUT_HINT_SPELLCHECK)
    hints |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_SPELLCHECK;
  if (input_hints & GTK_INPUT_HINT_WORD_COMPLETION)
    hints |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_COMPLETION;
  if (input_hints & GTK_INPUT_HINT_LOWERCASE)
    hints |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_LOWERCASE;
  if (input_hints & GTK_INPUT_HINT_UPPERCASE_CHARS)
    hints |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_UPPERCASE;
  if (input_hints & GTK_INPUT_HINT_UPPERCASE_WORDS)
    hints |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_TITLECASE;
  if (input_hints & GTK_INPUT_HINT_UPPERCASE_SENTENCES)
    hints |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_AUTO_CAPITALIZATION;

  if (purpose == GTK_INPUT_PURPOSE_PIN ||
      purpose == GTK_INPUT_PURPOSE_PASSWORD)
    {
      hints |= (ZWP_TEXT_INPUT_V3_CONTENT_HINT_HIDDEN_TEXT |
                ZWP_TEXT_INPUT_V3_CONTENT_HINT_SENSITIVE_DATA);
    }

  return hints;
}

static uint32_t
translate_purpose (GtkInputPurpose purpose)
{
  switch (purpose)
    {
    case GTK_INPUT_PURPOSE_FREE_FORM:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL;
    case GTK_INPUT_PURPOSE_ALPHA:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_ALPHA;
    case GTK_INPUT_PURPOSE_DIGITS:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_DIGITS;
    case GTK_INPUT_PURPOSE_NUMBER:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NUMBER;
    case GTK_INPUT_PURPOSE_PHONE:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_PHONE;
    case GTK_INPUT_PURPOSE_URL:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_URL;
    case GTK_INPUT_PURPOSE_EMAIL:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_EMAIL;
    case GTK_INPUT_PURPOSE_NAME:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NAME;
    case GTK_INPUT_PURPOSE_PASSWORD:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_PASSWORD;
    case GTK_INPUT_PURPOSE_PIN:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_PIN;
    case GTK_INPUT_PURPOSE_TERMINAL:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_TERMINAL;
    }

  return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL;
}

static void
notify_content_type (GtkIMContextWayland *context)
{
  GtkInputHints hints;
  GtkInputPurpose purpose;

  if (global->current != GTK_IM_CONTEXT (context))
    return;

  if (!context->enabled)
    return;

  g_object_get (context,
                "input-hints", &hints,
                "input-purpose", &purpose,
                NULL);

  zwp_text_input_v3_set_content_type (global->text_input,
                                      translate_hints (hints, purpose),
                                      translate_purpose (purpose));
}

static void
commit_state (GtkIMContextWayland *context)
{
  if (global->current != GTK_IM_CONTEXT (context))
    return;
  if (!context->enabled)
    return;
  global->serial++;
  zwp_text_input_v3_commit (global->text_input);
  context->surrounding_change = ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_INPUT_METHOD;
}

static void
gtk_im_context_wayland_finalize (GObject *object)
{
  GtkIMContextWayland *context = GTK_IM_CONTEXT_WAYLAND (object);

  gtk_im_context_wayland_focus_out (GTK_IM_CONTEXT (context));

  g_clear_object (&context->window);
  g_clear_object (&context->gesture);
  g_free (context->surrounding.text);
  g_free (context->current_preedit.text);
  g_free (context->pending_preedit.text);
  g_free (context->pending_commit);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/* We want a unified experience between GtkIMContextSimple and IBus / Wayland
 * when it comes to Compose sequences. IBus initial implementation of preedit
 * for Compose sequences shows U+2384, which has been described as 'distracting'.
 * This function tries to detect this case, and tweaks the text to match what
 * GtkIMContextSimple produces.
 */
static char *
tweak_preedit (const char *text)
{
  GString *s;
  guint len;
  const char *p;

  s = g_string_new ("");

  len = g_utf8_strlen (text, -1);

  for (p = text; *p; p = g_utf8_next_char (p))
    {
      gunichar ch = g_utf8_get_char (p);

      if (ch == 0x2384)
        {
          if (len == 1 || p > text)
            g_string_append (s, "·");
        }
      else
        g_string_append_unichar (s, ch);
    }

  return g_string_free (s, FALSE);
}


static void
gtk_im_context_wayland_get_preedit_string (GtkIMContext   *context,
                                           gchar         **str,
                                           PangoAttrList **attrs,
                                           gint           *cursor_pos)
{
  GtkIMContextWayland *context_wayland = GTK_IM_CONTEXT_WAYLAND (context);
  char *preedit_str;

  if (attrs)
    *attrs = NULL;

  GTK_IM_CONTEXT_CLASS (parent_class)->get_preedit_string (context,
                                                           str, attrs,
                                                           cursor_pos);

  /* If the parent implementation returns a len>0 string, go with it */
  if (str && *str)
    {
      if (**str)
        return;

      g_free (*str);
    }

  preedit_str =
    tweak_preedit (context_wayland->current_preedit.text ? context_wayland->current_preedit.text : "");

  if (cursor_pos)
    *cursor_pos = g_utf8_strlen (preedit_str,
                                 context_wayland->current_preedit.cursor_begin);

  if (attrs)
    {
      if (!*attrs)
        *attrs = pango_attr_list_new ();
      pango_attr_list_insert (*attrs,
                              pango_attr_underline_new (PANGO_UNDERLINE_SINGLE));
      if (context_wayland->current_preedit.cursor_begin
          != context_wayland->current_preedit.cursor_end)
        {
          /* FIXME: Oh noes, how to highlight while taking into account user preferences? */
          PangoAttribute *cursor = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
          cursor->start_index = context_wayland->current_preedit.cursor_begin;
          cursor->end_index = context_wayland->current_preedit.cursor_end;
          pango_attr_list_insert (*attrs, cursor);
        }
    }

  if (str)
    *str = preedit_str;
  else
    g_free (preedit_str);
}

static gboolean
gtk_im_context_wayland_filter_keypress (GtkIMContext *context,
                                        GdkEventKey  *key)
{
  /* This is done by the compositor */
  return GTK_IM_CONTEXT_CLASS (parent_class)->filter_keypress (context, key);
}

static void
enable (GtkIMContextWayland *context_wayland)
{
  /* Technically, text input isn't enabled until after the commit.
   * In reality, enable can't fail, and notify functions need to know
   * that they are free to send requests. */
  context_wayland->enabled = TRUE;
  zwp_text_input_v3_enable (global->text_input);
  notify_im_change (context_wayland,
                    ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_OTHER);
}

static void
disable (GtkIMContextWayland *context_wayland)
{
  zwp_text_input_v3_disable (global->text_input);
  commit_state (context_wayland);
  context_wayland->enabled = FALSE;

  /* The commit above will still count in the .done event accounting,
   * we should account for it, lest the serial gets out of sync after
   * a future focus_in/enable.
   */
  global->done_serial++;

  /* after disable, incoming state changes won't take effect anyway */
  if (context_wayland->current_preedit.text)
    {
      text_input_preedit (global, global->text_input, NULL, 0, 0);
      text_input_preedit_apply (global);
    }
}


static void
pressed_cb (GtkGestureMultiPress *gesture,
            gint                  n_press,
            gdouble               x,
            gdouble               y,
            GtkIMContextWayland  *context)
{
  if (n_press == 1)
    {
      context->press_x = x;
      context->press_y = y;
    }
}

static void
released_cb (GtkGestureMultiPress *gesture,
             gint                  n_press,
             gdouble               x,
             gdouble               y,
             GtkIMContextWayland  *context)
{
  GtkInputHints hints;

  if (!global->current)
    return;

  g_object_get (context, "input-hints", &hints, NULL);

  if (global->focused &&
      n_press == 1 &&
      (hints & GTK_INPUT_HINT_INHIBIT_OSK) == 0 &&
      !gtk_drag_check_threshold (context->widget,
                                 context->press_x,
                                 context->press_y,
                                 x, y))
    enable (context);
}

static void
gtk_im_context_wayland_set_client_window (GtkIMContext *context,
                                          GdkWindow    *window)
{
  GtkIMContextWayland *context_wayland = GTK_IM_CONTEXT_WAYLAND (context);
  GtkWidget *widget = NULL;

  if (window == context_wayland->window)
    return;

  if (window)
    gdk_window_get_user_data (window, (gpointer*) &widget);

  if (context_wayland->widget && context_wayland->widget != widget)
    g_clear_object (&context_wayland->gesture);

  g_set_object (&context_wayland->window, window);

  if (context_wayland->widget != widget)
    {
      context_wayland->widget = widget;

      if (widget)
        {
          GtkGesture *gesture;

          gesture = gtk_gesture_multi_press_new (widget);
          gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture),
                                                      GTK_PHASE_CAPTURE);
          g_signal_connect (gesture, "pressed",
                            G_CALLBACK (pressed_cb), context);
          g_signal_connect (gesture, "released",
                            G_CALLBACK (released_cb), context);
          context_wayland->gesture = gesture;
        }
    }

  GTK_IM_CONTEXT_CLASS (parent_class)->set_client_window (context, window);
}

static void
text_input_enter (void                     *data,
                  struct zwp_text_input_v3 *text_input,
                  struct wl_surface        *surface)
{
  global->focused = TRUE;

  if (global->current)
    enable (GTK_IM_CONTEXT_WAYLAND (global->current));
}

static void
text_input_leave (void                     *data,
                  struct zwp_text_input_v3 *text_input,
                  struct wl_surface        *surface)
{
  global->focused = FALSE;

  if (global->current)
    disable (GTK_IM_CONTEXT_WAYLAND (global->current));
}


static const struct zwp_text_input_v3_listener text_input_listener = {
  text_input_enter,
  text_input_leave,
  text_input_preedit,
  text_input_commit,
  text_input_delete_surrounding_text,
  text_input_done,
};

static void
registry_handle_global (void               *data,
                        struct wl_registry *registry,
                        uint32_t            id,
                        const char         *interface,
                        uint32_t            version)
{
  GtkIMContextWaylandGlobal *global = data;
  GdkSeat *seat = gdk_display_get_default_seat (gdk_display_get_default ());

  if (strcmp (interface, "zwp_text_input_manager_v3") == 0)
    {
      global->text_input_manager_wl_id = id;
      global->text_input_manager =
        wl_registry_bind (global->registry, global->text_input_manager_wl_id,
                          &zwp_text_input_manager_v3_interface, 1);
      global->text_input =
        zwp_text_input_manager_v3_get_text_input (global->text_input_manager,
                                                  gdk_wayland_seat_get_wl_seat (seat));
      global->serial = 0;
      zwp_text_input_v3_add_listener (global->text_input,
                                      &text_input_listener, global);
    }
}

static void
registry_handle_global_remove (void               *data,
                               struct wl_registry *registry,
                               uint32_t            id)
{
  GtkIMContextWaylandGlobal *global = data;

  if (id != global->text_input_manager_wl_id)
    return;

  g_clear_pointer (&global->text_input, zwp_text_input_v3_destroy);
  g_clear_pointer (&global->text_input_manager, zwp_text_input_manager_v3_destroy);
}

static const struct wl_registry_listener registry_listener = {
    registry_handle_global,
    registry_handle_global_remove
};

static void
gtk_im_context_wayland_global_init (GdkDisplay *display)
{
  g_return_if_fail (global == NULL);

  global = g_new0 (GtkIMContextWaylandGlobal, 1);
  global->display = gdk_wayland_display_get_wl_display (display);
  global->registry = wl_display_get_registry (global->display);

  wl_registry_add_listener (global->registry, &registry_listener, global);
}

static void
gtk_im_context_wayland_focus_in (GtkIMContext *context)
{
  GtkIMContextWayland *context_wayland = GTK_IM_CONTEXT_WAYLAND (context);

  if (global->current == context)
    return;
  if (!global->text_input)
    return;

  if (context_wayland->gesture)
    gtk_event_controller_reset (GTK_EVENT_CONTROLLER (context_wayland->gesture));
  global->current = context;

  if (global->focused)
    enable (context_wayland);
}

static void
gtk_im_context_wayland_focus_out (GtkIMContext *context)
{
  GtkIMContextWayland *context_wayland = GTK_IM_CONTEXT_WAYLAND (context);

  if (global->current != context)
    return;

  if (global->focused)
    disable (context_wayland);

  global->current = NULL;
}

static void
gtk_im_context_wayland_reset (GtkIMContext *context)
{
  notify_im_change (GTK_IM_CONTEXT_WAYLAND (context),
                    ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_OTHER);

  GTK_IM_CONTEXT_CLASS (parent_class)->reset (context);
}

static void
gtk_im_context_wayland_set_cursor_location (GtkIMContext *context,
                                            GdkRectangle *rect)
{
  GtkIMContextWayland *context_wayland;
  int side;

  context_wayland = GTK_IM_CONTEXT_WAYLAND (context);

  if (context_wayland->cursor_rect.x == rect->x &&
      context_wayland->cursor_rect.y == rect->y &&
      context_wayland->cursor_rect.width == rect->width &&
      context_wayland->cursor_rect.height == rect->height)
    return;

  /* Reset the gesture if the cursor changes too far (eg. clicking
   * between disjoint positions in the text).
   *
   * Still Allow some jittering (a square almost double the cursor rect height
   * on either side) as clicking on the exact same position between characters
   * is hard.
   */
  side = context_wayland->cursor_rect.height;

  if (context_wayland->gesture &&
      (ABS (rect->x - context_wayland->cursor_rect.x) >= side ||
       ABS (rect->y - context_wayland->cursor_rect.y) >= side))
    gtk_event_controller_reset (GTK_EVENT_CONTROLLER (context_wayland->gesture));

  context_wayland->cursor_rect = *rect;
}

static void
gtk_im_context_wayland_set_use_preedit (GtkIMContext *context,
                                        gboolean      use_preedit)
{
  GtkIMContextWayland *context_wayland = GTK_IM_CONTEXT_WAYLAND (context);

  context_wayland->use_preedit = !!use_preedit;
}

static void
gtk_im_context_wayland_set_surrounding (GtkIMContext *context,
                                        const gchar  *text,
                                        gint          len,
                                        gint          cursor_index)
{
  GtkIMContextWayland *context_wayland;

  context_wayland = GTK_IM_CONTEXT_WAYLAND (context);

  if (context_wayland->surrounding.text && text &&
      (len < 0 || len == strlen (context_wayland->surrounding.text)) &&
      strncmp (context_wayland->surrounding.text, text, len) == 0 &&
      context_wayland->surrounding.cursor_idx == cursor_index &&
      context_wayland->surrounding.anchor_idx == cursor_index)
    return;

  g_free (context_wayland->surrounding.text);
  context_wayland->surrounding.text = g_strndup (text, len);
  context_wayland->surrounding.cursor_idx = cursor_index;
  /* Anchor is not exposed via the set_surrounding interface, emulating. */
  context_wayland->surrounding.anchor_idx = cursor_index;

  notify_surrounding_text (context_wayland);
}

static gboolean
gtk_im_context_wayland_get_surrounding (GtkIMContext  *context,
                                        gchar        **text,
                                        gint          *cursor_index)
{
  GtkIMContextWayland *context_wayland;

  context_wayland = GTK_IM_CONTEXT_WAYLAND (context);

  if (!context_wayland->surrounding.text)
    return FALSE;

  *text = context_wayland->surrounding.text;
  *cursor_index = context_wayland->surrounding.cursor_idx;
  return TRUE;
}

static void
gtk_im_context_wayland_commit (GtkIMContext *context,
                               const gchar  *str)
{
  if (GTK_IM_CONTEXT_CLASS (parent_class)->commit)
    GTK_IM_CONTEXT_CLASS (parent_class)->commit (context, str);

  notify_im_change (GTK_IM_CONTEXT_WAYLAND (context),
                    ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_INPUT_METHOD);
}

static void
gtk_im_context_wayland_class_init (GtkIMContextWaylandClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkIMContextClass *im_context_class = GTK_IM_CONTEXT_CLASS (klass);

  object_class->finalize = gtk_im_context_wayland_finalize;

  im_context_class->set_client_window = gtk_im_context_wayland_set_client_window;
  im_context_class->get_preedit_string = gtk_im_context_wayland_get_preedit_string;
  im_context_class->filter_keypress = gtk_im_context_wayland_filter_keypress;
  im_context_class->focus_in = gtk_im_context_wayland_focus_in;
  im_context_class->focus_out = gtk_im_context_wayland_focus_out;
  im_context_class->reset = gtk_im_context_wayland_reset;
  im_context_class->set_cursor_location = gtk_im_context_wayland_set_cursor_location;
  im_context_class->set_use_preedit = gtk_im_context_wayland_set_use_preedit;
  im_context_class->set_surrounding = gtk_im_context_wayland_set_surrounding;
  im_context_class->get_surrounding = gtk_im_context_wayland_get_surrounding;
  im_context_class->commit = gtk_im_context_wayland_commit;

  parent_class = g_type_class_peek_parent (klass);
}

static void
on_content_type_changed (GtkIMContextWayland *context)
{
  notify_content_type (context);
  commit_state (context);
}

static void
gtk_im_context_wayland_init (GtkIMContextWayland *context)
{
  context->use_preedit = TRUE;
  g_signal_connect_swapped (context, "notify::input-purpose",
                            G_CALLBACK (on_content_type_changed), context);
  g_signal_connect_swapped (context, "notify::input-hints",
                            G_CALLBACK (on_content_type_changed), context);
}

static void
gtk_im_context_wayland_register_type (GTypeModule *module)
{
  const GTypeInfo object_info = {
    sizeof (GtkIMContextWaylandClass),
    NULL, NULL,
    (GClassInitFunc) gtk_im_context_wayland_class_init,
    NULL, NULL,
    sizeof (GtkIMContextWayland),
    0,
    (GInstanceInitFunc) gtk_im_context_wayland_init,
  };

  type_wayland = g_type_module_register_type (module,
                                              GTK_TYPE_IM_CONTEXT_SIMPLE,
                                              "GtkIMContextWayland",
                                              &object_info, 0);
}

MODULE_ENTRY (void, init) (GTypeModule * module)
{
  gtk_im_context_wayland_register_type (module);
  gtk_im_context_wayland_global_init (gdk_display_get_default ());
}

MODULE_ENTRY (void, exit) (void)
{
}

MODULE_ENTRY (void, list) (const GtkIMContextInfo *** contexts, int *n_contexts)
{
  *contexts = info_list;
  *n_contexts = G_N_ELEMENTS (info_list);
}

MODULE_ENTRY (GtkIMContext *, create) (const gchar * context_id)
{
  if (strcmp (context_id, "wayland") == 0)
    return g_object_new (type_wayland, NULL);
  else
    return NULL;
}
