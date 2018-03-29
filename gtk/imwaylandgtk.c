/* GTK - The GIMP Toolkit
 * Copyright (C) 2017 Red Hat, Inc.
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
#include "gtk-text-input-client-protocol.h"

typedef struct _GtkIMContextWaylandGlobal GtkIMContextWaylandGlobal;
typedef struct _GtkIMContextWayland GtkIMContextWayland;
typedef struct _GtkIMContextWaylandClass GtkIMContextWaylandClass;

struct _GtkIMContextWaylandGlobal
{
  struct wl_display *display;
  struct wl_registry *registry;
  uint32_t text_input_manager_wl_id;
  struct gtk_text_input_manager *text_input_manager;
  struct gtk_text_input *text_input;
  uint32_t enter_serial;

  GtkIMContext *current;
};

struct _GtkIMContextWaylandClass
{
  GtkIMContextSimpleClass parent_class;
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
  } surrounding;

  struct {
    gchar *text;
    gint cursor_idx;
  } preedit;

  cairo_rectangle_int_t cursor_rect;
  guint use_preedit : 1;
};

GType type_wayland = 0;
static GObjectClass *parent_class;
static GtkIMContextWaylandGlobal *global = NULL;

static const GtkIMContextInfo imwayland_info =
{
  "waylandgtk",      /* ID */
  NC_("input method menu", "Waylandgtk"),      /* Human readable name */
  GETTEXT_PACKAGE, /* Translation domain */
  GTK_LOCALEDIR,   /* Dir for bindtextdomain (not strictly needed for "gtk+") */
  "",              /* Languages for which this module is the default */
};

static const GtkIMContextInfo *info_list[] =
{
  &imwayland_info,
};

#define GTK_IM_CONTEXT_WAYLAND(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), type_wayland, GtkIMContextWayland))

#ifndef INCLUDE_IM_wayland
#define MODULE_ENTRY(type,function) G_MODULE_EXPORT type im_module_ ## function
#else
#define MODULE_ENTRY(type, function) type _gtk_immodule_wayland_ ## function
#endif

static void
reset_preedit (GtkIMContextWayland *context)
{
  if (context->preedit.text == NULL)
    return;

  g_clear_pointer (&context->preedit.text, g_free);
  context->preedit.cursor_idx = 0;
  g_signal_emit_by_name (context, "preedit-changed");
}

static void
text_input_enter (void                     *data,
                  struct gtk_text_input    *text_input,
                  uint32_t                  serial,
                  struct wl_surface        *surface)
{
  GtkIMContextWaylandGlobal *global = data;

  global->enter_serial = serial;
}

static void
text_input_leave (void                     *data,
                  struct gtk_text_input    *text_input,
                  uint32_t                  serial,
                  struct wl_surface        *surface)
{
  GtkIMContextWayland *context;

  if (!global->current)
    return;

  context = GTK_IM_CONTEXT_WAYLAND (global->current);
  reset_preedit (context);
}

static void
text_input_preedit (void                     *data,
                    struct gtk_text_input    *text_input,
                    const char               *text,
                    guint                     cursor)
{
  GtkIMContextWayland *context;
  gboolean state_change;

  if (!global->current)
    return;

  context = GTK_IM_CONTEXT_WAYLAND (global->current);
  if (!text && !context->preedit.text)
    return;

  state_change = ((text == NULL) != (context->preedit.text == NULL));

  if (state_change && !context->preedit.text)
    g_signal_emit_by_name (context, "preedit-start");

  g_free (context->preedit.text);
  context->preedit.text = g_strdup (text);
  context->preedit.cursor_idx = cursor;

  g_signal_emit_by_name (context, "preedit-changed");

  if (state_change && !context->preedit.text)
    g_signal_emit_by_name (context, "preedit-end");
}

static void
text_input_commit (void                     *data,
                   struct gtk_text_input    *text_input,
                   const char               *text)
{
  GtkIMContextWaylandGlobal *global = data;

  if (global->current && text)
    g_signal_emit_by_name (global->current, "commit", text);
}

static void
text_input_delete_surrounding_text (void                     *data,
                                    struct gtk_text_input    *text_input,
                                    uint32_t                  offset,
                                    uint32_t                  len)
{
  GtkIMContextWaylandGlobal *global = data;

  if (global->current)
    g_signal_emit_by_name (global->current, "delete-surrounding", offset, len);
}

static const struct gtk_text_input_listener text_input_listener = {
  text_input_enter,
  text_input_leave,
  text_input_preedit,
  text_input_commit,
  text_input_delete_surrounding_text
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

  if (strcmp (interface, "gtk_text_input_manager") == 0)
    {
      global->text_input_manager_wl_id = id;
      global->text_input_manager =
        wl_registry_bind (global->registry, global->text_input_manager_wl_id,
                          &gtk_text_input_manager_interface, 1);
      global->text_input =
        gtk_text_input_manager_get_text_input (global->text_input_manager,
                                               gdk_wayland_seat_get_wl_seat (seat));
      gtk_text_input_add_listener (global->text_input,
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

  g_clear_pointer(&global->text_input, gtk_text_input_destroy);
  g_clear_pointer(&global->text_input_manager, gtk_text_input_manager_destroy);
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
notify_surrounding_text (GtkIMContextWayland *context)
{
  if (!global || !global->text_input)
    return;
  if (global->current != GTK_IM_CONTEXT (context))
    return;
  if (!context->surrounding.text)
    return;

  gtk_text_input_set_surrounding_text (global->text_input,
                                       context->surrounding.text,
                                       context->surrounding.cursor_idx,
                                       context->surrounding.cursor_idx);
}

static void
notify_cursor_location (GtkIMContextWayland *context)
{
  cairo_rectangle_int_t rect;

  if (!global || !global->text_input)
    return;
  if (global->current != GTK_IM_CONTEXT (context))
    return;
  if (!context->window)
    return;

  rect = context->cursor_rect;
  gdk_window_get_root_coords (context->window, rect.x, rect.y,
                              &rect.x, &rect.y);

  gtk_text_input_set_cursor_rectangle (global->text_input,
                                       rect.x, rect.y,
                                       rect.width, rect.height);
}

static uint32_t
translate_hints (GtkInputHints   input_hints,
                 GtkInputPurpose purpose)
{
  uint32_t hints = 0;

  if (input_hints & GTK_INPUT_HINT_SPELLCHECK)
    hints |= GTK_TEXT_INPUT_CONTENT_HINT_SPELLCHECK;
  if (input_hints & GTK_INPUT_HINT_WORD_COMPLETION)
    hints |= GTK_TEXT_INPUT_CONTENT_HINT_COMPLETION;
  if (input_hints & GTK_INPUT_HINT_LOWERCASE)
    hints |= GTK_TEXT_INPUT_CONTENT_HINT_LOWERCASE;
  if (input_hints & GTK_INPUT_HINT_UPPERCASE_CHARS)
    hints |= GTK_TEXT_INPUT_CONTENT_HINT_UPPERCASE;
  if (input_hints & GTK_INPUT_HINT_UPPERCASE_WORDS)
    hints |= GTK_TEXT_INPUT_CONTENT_HINT_TITLECASE;
  if (input_hints & GTK_INPUT_HINT_UPPERCASE_SENTENCES)
    hints |= GTK_TEXT_INPUT_CONTENT_HINT_AUTO_CAPITALIZATION;

  if (purpose == GTK_INPUT_PURPOSE_PIN ||
      purpose == GTK_INPUT_PURPOSE_PASSWORD)
    {
      hints |= (GTK_TEXT_INPUT_CONTENT_HINT_HIDDEN_TEXT |
                GTK_TEXT_INPUT_CONTENT_HINT_SENSITIVE_DATA);
    }

  return hints;
}

static uint32_t
translate_purpose (GtkInputPurpose purpose)
{
  switch (purpose)
    {
    case GTK_INPUT_PURPOSE_FREE_FORM:
      return GTK_TEXT_INPUT_CONTENT_PURPOSE_NORMAL;
    case GTK_INPUT_PURPOSE_ALPHA:
      return GTK_TEXT_INPUT_CONTENT_PURPOSE_ALPHA;
    case GTK_INPUT_PURPOSE_DIGITS:
      return GTK_TEXT_INPUT_CONTENT_PURPOSE_DIGITS;
    case GTK_INPUT_PURPOSE_NUMBER:
      return GTK_TEXT_INPUT_CONTENT_PURPOSE_NUMBER;
    case GTK_INPUT_PURPOSE_PHONE:
      return GTK_TEXT_INPUT_CONTENT_PURPOSE_PHONE;
    case GTK_INPUT_PURPOSE_URL:
      return GTK_TEXT_INPUT_CONTENT_PURPOSE_URL;
    case GTK_INPUT_PURPOSE_EMAIL:
      return GTK_TEXT_INPUT_CONTENT_PURPOSE_EMAIL;
    case GTK_INPUT_PURPOSE_NAME:
      return GTK_TEXT_INPUT_CONTENT_PURPOSE_NAME;
    case GTK_INPUT_PURPOSE_PASSWORD:
      return GTK_TEXT_INPUT_CONTENT_PURPOSE_PASSWORD;
    case GTK_INPUT_PURPOSE_PIN:
      return GTK_TEXT_INPUT_CONTENT_PURPOSE_PIN;
    }

  return GTK_TEXT_INPUT_CONTENT_PURPOSE_NORMAL;
}

static void
notify_content_type (GtkIMContextWayland *context)
{
  GtkInputHints hints;
  GtkInputPurpose purpose;

  if (global->current != GTK_IM_CONTEXT (context))
    return;

  g_object_get (context,
                "input-hints", &hints,
                "input-purpose", &purpose,
                NULL);

  gtk_text_input_set_content_type (global->text_input,
                                   translate_hints (hints, purpose),
                                   translate_purpose (purpose));
}

static void
commit_state (GtkIMContextWayland *context)
{
  if (global->current != GTK_IM_CONTEXT (context))
    return;
  gtk_text_input_commit (global->text_input);
}

static void
enable_text_input (GtkIMContextWayland *context,
                   gboolean             toggle_panel)
{
  guint flags = 0;

  if (context->use_preedit)
    flags |= GTK_TEXT_INPUT_ENABLE_FLAGS_CAN_SHOW_PREEDIT;
  if (toggle_panel)
    flags |= GTK_TEXT_INPUT_ENABLE_FLAGS_TOGGLE_INPUT_PANEL;

  gtk_text_input_enable (global->text_input,
                         global->enter_serial,
                         flags);
}

static void
gtk_im_context_wayland_finalize (GObject *object)
{
  GtkIMContextWayland *context = GTK_IM_CONTEXT_WAYLAND (object);

  g_clear_object (&context->window);
  g_clear_object (&context->gesture);

  G_OBJECT_CLASS (parent_class)->finalize (object);
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

  if (n_press == 1 &&
      (hints & GTK_INPUT_HINT_INHIBIT_OSK) == 0 &&
      !gtk_drag_check_threshold (context->widget,
                                 context->press_x,
                                 context->press_y,
                                 x, y))
    {
      enable_text_input (GTK_IM_CONTEXT_WAYLAND (context), TRUE);
    }
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
}

static void
gtk_im_context_wayland_get_preedit_string (GtkIMContext   *context,
                                           gchar         **str,
                                           PangoAttrList **attrs,
                                           gint           *cursor_pos)
{
  GtkIMContextWayland *context_wayland = GTK_IM_CONTEXT_WAYLAND (context);
  gchar *preedit_str;

  if (attrs)
    *attrs = NULL;

  GTK_IM_CONTEXT_CLASS (parent_class)->get_preedit_string (context, str, attrs, cursor_pos);

  /* If the parent implementation returns a len>0 string, go with it */
  if (str && *str)
    {
      if (**str)
        return;

      g_free (*str);
    }

  preedit_str =
    context_wayland->preedit.text ? context_wayland->preedit.text : "";

  if (str)
    *str = g_strdup (preedit_str);
  if (cursor_pos)
    *cursor_pos = context_wayland->preedit.cursor_idx;

  if (attrs)
    {
      if (!*attrs)
        *attrs = pango_attr_list_new ();
      pango_attr_list_insert (*attrs,
                              pango_attr_underline_new (PANGO_UNDERLINE_SINGLE));
    }
}

static gboolean
gtk_im_context_wayland_filter_keypress (GtkIMContext *context,
                                        GdkEventKey  *key)
{
  /* This is done by the compositor */
  return GTK_IM_CONTEXT_CLASS (parent_class)->filter_keypress (context, key);
}

static void
gtk_im_context_wayland_focus_in (GtkIMContext *context)
{
  GtkIMContextWayland *context_wayland = GTK_IM_CONTEXT_WAYLAND (context);

  if (global->current == context)
    return;
  if (!global->text_input)
    return;

  global->current = context;
  enable_text_input (context_wayland, FALSE);
  notify_content_type (context_wayland);
  notify_surrounding_text (context_wayland);
  notify_cursor_location (context_wayland);
  commit_state (context_wayland);
}

static void
gtk_im_context_wayland_focus_out (GtkIMContext *context)
{
  if (global->current != context)
    return;

  gtk_text_input_disable (global->text_input);
  global->current = NULL;
}

static void
gtk_im_context_wayland_reset (GtkIMContext *context)
{
  reset_preedit (GTK_IM_CONTEXT_WAYLAND (context));

  GTK_IM_CONTEXT_CLASS (parent_class)->reset (context);
}

static void
gtk_im_context_wayland_set_cursor_location (GtkIMContext *context,
                                            GdkRectangle *rect)
{
  GtkIMContextWayland *context_wayland;

  context_wayland = GTK_IM_CONTEXT_WAYLAND (context);

  context_wayland->cursor_rect = *rect;
  notify_cursor_location (context_wayland);
  commit_state (context_wayland);
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

  g_free (context_wayland->surrounding.text);
  context_wayland->surrounding.text = g_strdup (text);
  context_wayland->surrounding.cursor_idx = cursor_index;

  notify_surrounding_text (context_wayland);
  commit_state (context_wayland);
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
  if (strcmp (context_id, "waylandgtk") == 0)
    return g_object_new (type_wayland, NULL);
  else
    return NULL;
}
