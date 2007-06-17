/* Copyright (C) 2006 Openismus GmbH
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

#include "gtkimcontextmultipress.h"
#include <gtk/gtkimcontext.h>
#include <gtk/gtkimmodule.h>
#include <gtk/gtkimcontext.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gdk/gdkwindow.h>
#include <gdk/gdkkeysyms.h> /* For GDK_A, etc. */
#include <glib.h> /* For GKeyFile */
#include <glib-object.h>
#include <string.h> /* For memset() */

#define AUTOMATIC_COMPOSE_TIMEOUT 1 /* seconds */

/* Just the last part of the name, not the whole path: */
#define CONFIGURATION_FILENAME MULTIPRESS_CONFDIR G_DIR_SEPARATOR_S "im-multipress.conf"

#define MULTIPRESS_PASSTHROUGH_FLAG "multipress-passthrough-flag"

/** This contains rows of characters that can be inputed by pressing a particular key repeatedly.
 * Each row has one key (such as GDK_A), and an array of characters, such as 'a'.
 */
struct _KeySequence
{
  gunichar key_press; /* Such as 'a' (== GDK_a) */
  gchar** characters; /* Array of strings. */
  gsize characters_length; /* size of the array of strings. */
};


static void gtk_im_context_multipress_class_init (GtkImContextMultipressClass *klass);
static void gtk_im_context_multipress_init (GtkImContextMultipress *self);
static void gtk_im_context_multipress_finalize (GObject * obj);

static void gtk_im_context_multipress_load_config (GtkImContextMultipress *self);

static GObjectClass* gtk_im_context_multipress_parent_class = NULL;
static GType gtk_im_multi_press_im_context_type = 0;

/** Notice that we have a *_register_type(GTypeModule*) function instead of a *_get_type() function,
 * because we must use g_type_module_register_type(), providing the GTypeModule* that was provided to im_context_init().
 * That is also why we are not using G_DEFINE_TYPE().
 */
void 
gtk_im_context_multipress_register_type (GTypeModule* type_module)
{
  if (gtk_im_multi_press_im_context_type == 0)
  {
    static const GTypeInfo im_context_multipress_info =
    {
      sizeof(GtkImContextMultipressClass),
      (GBaseInitFunc) NULL,
      (GBaseFinalizeFunc) NULL,
      (GClassInitFunc) gtk_im_context_multipress_class_init,
      NULL,
      NULL,
      sizeof(GtkImContextMultipress),
      0,
      (GInstanceInitFunc) gtk_im_context_multipress_init,
      0,
    };

    gtk_im_multi_press_im_context_type =
      g_type_module_register_type (type_module,
                                   GTK_TYPE_IM_CONTEXT,
                                   "GtkImContextMultipress",
                                   &im_context_multipress_info, 0);
  }
}

GType 
gtk_im_context_multipress_get_type(void)
{
  g_assert (gtk_im_multi_press_im_context_type != 0);
  return gtk_im_multi_press_im_context_type;
}

/*
 * Returns TRUE if the passthrough flag is set on the currently focused
 * child of the widget that owns the GDK window.  In order to turn on
 * passthrough mode, call:
 * g_object_set_data(widget, "multipress-passthrough-flag", GINT_TO_POINTER(1));
 */
static gboolean 
passthrough_enabled_for_window (GdkWindow* window)
{
  gpointer event_widget = NULL;

  g_return_val_if_fail (window != NULL, FALSE);
  /*
   * For historical reasons, GTK+ assumes the user data attached to a GdkWindow
   * to point to the GtkWidget that owns the window.  It's even documented:
   * http://developer.gnome.org/doc/API/2.0/gdk/gdk-Windows.html#gdk-window-set-user-data
   * So we are really lucky here, as this allows us to attach IM state
   * information to a widget in a fairly straightforward manner.
   */
  gdk_window_get_user_data (window, &event_widget);

  if (event_widget && GTK_IS_WIDGET(event_widget))
  {
    GtkWidget* toplevel;
    GtkWidget* focus_widget;
    /*
     * The event window for key presses will usually belong to the toplevel
     * GtkWindow, but that might not be true for synthetic events.  In any
     * case we need to find the currently focused child widget.
     */
    toplevel = gtk_widget_get_toplevel ((GtkWidget*) event_widget);

    g_return_val_if_fail (toplevel != NULL && GTK_IS_WINDOW(toplevel), FALSE);

    focus_widget = gtk_window_get_focus ((GtkWindow*) toplevel);

    if (focus_widget)
    {
      static GQuark quark_passthrough_flag = 0;

      if (!quark_passthrough_flag)
        quark_passthrough_flag = g_quark_from_string (MULTIPRESS_PASSTHROUGH_FLAG);

      if (g_object_get_qdata (G_OBJECT(focus_widget), quark_passthrough_flag))
        return TRUE;
    }
  }

  return FALSE;
}

static gboolean vfunc_filter_keypress (GtkIMContext *context, GdkEventKey  *event);
static void vfunc_reset (GtkIMContext *context);
static void vfunc_get_preedit_string (GtkIMContext   *context,
					  gchar         **str,
					  PangoAttrList **attrs,
					  gint           *cursor_pos);

static void 
gtk_im_context_multipress_class_init (GtkImContextMultipressClass *klass)
{
  GtkIMContextClass* im_context_class;

  /* Set this so we can use it later: */
  gtk_im_context_multipress_parent_class = g_type_class_peek_parent(klass);

  /* Specify our vfunc implementations: */
  im_context_class = GTK_IM_CONTEXT_CLASS (klass);
  im_context_class->filter_keypress = vfunc_filter_keypress;
  im_context_class->reset = vfunc_reset;
  im_context_class->get_preedit_string = vfunc_get_preedit_string;

  G_OBJECT_CLASS(klass)->finalize = gtk_im_context_multipress_finalize;
}

static void 
gtk_im_context_multipress_init (GtkImContextMultipress *self)
{
  gtk_im_context_multipress_load_config (self);
}

static void 
gtk_im_context_multipress_finalize (GObject * obj)
{
  GtkImContextMultipress *self = gtk_im_context_multipress(obj);
   
  /* Release the configuration data: */

  /* Free each item: */
  gsize i = 0;
  for (i = 0; i < self->key_sequences_count; ++i)
  {
    KeySequence* item = self->key_sequences[i];

    /* Free the array of strings in the item: */
    /* This is only for null-terminated arrays: g_strfreev(item->characters); */
    gsize i = 0;
    for (i = 0; i < item->characters_length; ++i)
    {
      gchar* str = item->characters[i];
      g_free (str);
      item->characters[i] = NULL;
    }

    g_free(item->characters);
    item->characters = NULL;
    item->characters_length = 0;

    /* Free the item itself: */
    g_free (item);
  }

  /* Free the array of pointers: */
  g_free (self->key_sequences);

  self->key_sequences = NULL;
  self->key_sequences_count = 0;

    
  G_OBJECT_CLASS (gtk_im_context_multipress_parent_class)->finalize(obj);
}


GtkIMContext 
*gtk_im_context_multipress_new (void)
{
  return (GtkIMContext*)g_object_new (GTK_TYPE_IM_CONTEXT_MULTIPRESS, NULL);
}

/** Lookup a compose sequence for the key press, from the table.
 *  The result is an null-terminated array of gchar*. It should not be freed by the caller.
 */
static KeySequence* 
lookup_characters (GtkImContextMultipress *multipress_context, guint keypress)
{

  /* Find the matching KeySequence, so that the caller can look at the possible characters for this keypress: */
  gsize i = 0;
  for (i = 0; i < multipress_context->key_sequences_count; ++i)
  {
    KeySequence* item = multipress_context->key_sequences[i];

    /* Just compare the first item, to match the keyval: */
    if (keypress == item->key_press)
      return item;
  }

  return NULL;
}

static void 
cancel_automatic_timeout_commit (GtkImContextMultipress *multipress_context)
{
  /* printf("debug: cancelling timeout\n"); */

  if (multipress_context->timeout_id)
    g_source_remove (multipress_context->timeout_id); /* This function cancels timeouts, idle handlers, etc. */
 
  multipress_context->timeout_id = 0;
}


/** Clear the compose buffer, so we are ready to compose the next character.
 */
static void 
clear_compose_buffer (GtkImContextMultipress *multipress_context)
{
  multipress_context->key_last_entered = 0;
  multipress_context->compose_count = 0;

  multipress_context->tentative_match = NULL;
  cancel_automatic_timeout_commit(multipress_context);
}

/** Finish composing, provide the character, and clear our compose buffer.
 */
static void 
accept_character (GtkImContextMultipress *multipress_context, const gchar* characters)
{
  /* printf("debug: accepting character: %c\n", (char)character); */
 
  cancel_automatic_timeout_commit (multipress_context);

  /* Accept the character: */

  /* Clear the compose buffer, so we are ready to compose the next character. */
  clear_compose_buffer (multipress_context);

  /* We must also signal that the preedit has changed, or we will still see the old 
     preedit from the composing of the character that we just committed, hanging around after the cursor.
   */
  g_signal_emit_by_name (multipress_context, "preedit_changed");

  /* Provide a character to GTK+: */
  g_signal_emit_by_name (multipress_context, "commit", characters);

  /* Note that if we emit preedit_changed after commit, 
   * there's a segfault/invalid-write with GtkTextView in gtk_text_layout_free_line_display(), when destroying a PangoLayout
   * (this can also be avoided by not using any pango attributes in get_preedit_string().
   */
}

static gboolean 
on_timeout (gpointer data)
{
  GtkImContextMultipress* multipress_context;

  GDK_THREADS_ENTER();

  /* printf("debug: on_timeout\n"); */

  multipress_context = gtk_im_context_multipress(data);

  /* A certain amount of time has passed,
   * so we will assume that the user really wants the currently chosen character:
   */
  accept_character (multipress_context, multipress_context->tentative_match);

  multipress_context->timeout_id = 0;

  GDK_THREADS_LEAVE();

  return FALSE; /* Don't call this callback again. We only need to call it once. */
}

static gboolean 
vfunc_filter_keypress (GtkIMContext *context, GdkEventKey *event)
{
  GtkIMContextClass* parent;
  GtkImContextMultipress* multipress_context;

  /* printf("debug: vfunc_filter_keypress:\n"); */

  multipress_context = gtk_im_context_multipress (context);

  if (event->type == GDK_KEY_PRESS)
  {
    KeySequence* possible = NULL;

    /* printf("debug: multipress_context->compose_count=%d\n", multipress_context->compose_count); */

    /* Check whether the current key is the same as previously entered,
     *  because if it is not then we should accept the previous one, and start a new character.
     */
    if (multipress_context->compose_count > 0 && multipress_context->key_last_entered != event->keyval)
    {
      /* Accept the previously chosen character: */
      if (multipress_context->tentative_match)
      {
        /* This wipes the compose_count and key_last_entered. */
        accept_character (multipress_context, multipress_context->tentative_match);
      } 
    }

    /* Decide what character this key press would choose: */
    if (!passthrough_enabled_for_window (event->window))
      possible = lookup_characters (multipress_context, event->keyval); /* Not to be freed. */

    if (possible)
    {
      /* Check whether we are at the end of a compose sequence, with no more possible characters: */
      /* Cycle back to the start if necessary: */
      if (multipress_context->compose_count >= possible->characters_length)
      {
        clear_compose_buffer (multipress_context);
        return vfunc_filter_keypress (context, event);
      }

      /* Store the last key pressed in the compose sequence. */
      multipress_context->key_last_entered = event->keyval; 
      ++(multipress_context->compose_count);

      /* Get the possible match for this number of presses of the key: */ 
      multipress_context->tentative_match = possible->characters[multipress_context->compose_count -1]; /* compose_count starts at 1, so that 0 can mean not composing. */

      /* Indicate the current possible character.
       * This will cause our vfunc_get_preedit_string() vfunc to be called, 
       * which will provide the current possible character for the user to see.
       */
      g_signal_emit_by_name (multipress_context, "preedit_changed");
      
      /* Cancel any outstanding timeout, so we can start the timer again: */
      cancel_automatic_timeout_commit (multipress_context);

      /* Create a timeout that will cause the currently chosen character to be committed,
       * if nothing happens for a certain amount of time:
       */
      /* g_timeout_add_seconds is only available since glib 2.14: multipress_context->timeout_id = g_timeout_add_seconds(AUTOMATIC_COMPOSE_TIMEOUT, on_timeout, multipress_context); */
      multipress_context->timeout_id = g_timeout_add (AUTOMATIC_COMPOSE_TIMEOUT * 1000, on_timeout, multipress_context);

      return TRUE; /* TRUE means that the event was handled. */
    }
    else
    {
      guint32 keyval_uchar;

      /*
       * Just accept all other keypresses directly, but commit the
       * current preedit content first.
       */
      if (multipress_context->compose_count > 0 && multipress_context->tentative_match)
        accept_character (multipress_context, multipress_context->tentative_match);

      keyval_uchar = gdk_keyval_to_unicode (event->keyval);
      /*
       * Convert to a string, because that's what accept_character() needs.
       */
      if (keyval_uchar)
      {
        gchar keyval_utf8[7]; /* max length of UTF-8 sequence + 1 for NUL termination */
        gint  length;

        length = g_unichar_to_utf8 (keyval_uchar, keyval_utf8);
        keyval_utf8[length] = '\0';

        accept_character (multipress_context, keyval_utf8);

        return TRUE; /* key handled */
      }
    }
  }

  /* The default implementation just returns FALSE, 
   * but it is generally a good idea to call the base class implementation:
   */
  parent = (GtkIMContextClass *)gtk_im_context_multipress_parent_class;
  if (parent->filter_keypress)
    return parent->filter_keypress (context, event);
  else
    return FALSE;
}

static void 
vfunc_reset (GtkIMContext *context)
{
  GtkImContextMultipress *multipress_context = gtk_im_context_multipress (context);

  clear_compose_buffer (multipress_context);
}


static void 
vfunc_get_preedit_string (GtkIMContext   *context,
					  gchar         **str,
					  PangoAttrList **attrs,
					  gint           *cursor_pos)
{
  /* printf("debug: get_preedit_string:\n"); */

  GtkImContextMultipress *multipress_context = gtk_im_context_multipress (context);

  /* Show the user what character he will get if he accepts: */
  gsize len_bytes = 0;
  gsize len_utf8_chars = 0;
  if (str)
  {
    if (multipress_context->tentative_match)
    {
      /*
      printf("debug: vfunc_get_preedit_string(): tentative_match != NULL\n");
      printf("debug: vfunc_get_preedit_string(): tentative_match=%s\n", multipress_context->tentative_match);
      */
      *str = g_strdup (multipress_context->tentative_match);
    }
    else
    {
      /* *str can never be NULL - that crashes the caller, which doesn't check for it: */
      *str = g_strdup("");
    }

    if (*str)
    {
      len_utf8_chars = g_utf8_strlen (*str, -1); /* For the cursor pos, which seems need to be UTF-8 characters (GtkEntry clamps it.) */
      len_bytes = strlen (*str); /* The number of bytes, not the number of UTF-8 characters. For the PangoAttribute. */ 
    }
  }

  /* Underline it, to show the user that he is in compose mode: */
  if (attrs)
    {
      *attrs = pango_attr_list_new ();
      
      if (len_bytes)
	{
	  PangoAttribute *attr = pango_attr_underline_new (PANGO_UNDERLINE_SINGLE);
	  attr->start_index = 0;
          attr->end_index = len_bytes;
	  pango_attr_list_insert (*attrs, attr);
	}
    }

  if (cursor_pos)
    *cursor_pos = len_utf8_chars;
}

static void 
gtk_im_context_multipress_load_config (GtkImContextMultipress *self)
{
  /* Open the configuration file: */
  GKeyFile* key_file;
  GError* error = NULL;
  GArray* array;
  gboolean found;
  guint key_suffix_num = 0;
  gboolean keep_looking = TRUE;

  key_file = g_key_file_new ();
  found = g_key_file_load_from_file (key_file, CONFIGURATION_FILENAME, G_KEY_FILE_NONE, &error);
  if (!found || error)
  {
    if (error)
    {
      g_warning ("Error while trying to open the %s configuration file: %s", CONFIGURATION_FILENAME, error->message);
      g_error_free (error);
      error = NULL;

      /*debug_output_possible_data_dirs();*/
    }

    g_key_file_free (key_file);
    return;
  }

  /* Get data from the file:
   * Each KP_* key should have a value consiting of ;-separated values: */

  array = g_array_sized_new (FALSE /* Not zero_terminated */, TRUE /* clear */,
                            sizeof(KeySequence*), 10 /* reserved size */);

  /* Look at each KP_* key in sequence, until there are no more KP_* keys: */
  while (keep_looking)
  {
    gchar* key_name;
    gchar** values;
    gsize length_values = 0;

    key_name = g_strdup_printf ("KP_%d", key_suffix_num);
    values = g_key_file_get_string_list (key_file, "keys", key_name, &length_values, &error);
    if (error)
    {
      /* Only show the warning if this was the first key. It's OK to fail when trying to find subsequent keys: */
      if (key_suffix_num == 0)
      {
        g_warning ("Error while trying to read key values from the configuration file: %s", error->message);
      }

      g_error_free (error);
      error = NULL;
    }

    if (!values)
    {
       /* printf("debug: No values found for key %s\n", key_name); */
       keep_looking = FALSE; /* This must be the last in the array of keys. */
       /* debug_output_possible_config_keys(key_file); */
    }
    else
    {
      KeySequence* key_sequence;
      GArray* array_characters;
      gsize value_index = 0;

      key_sequence = g_new0 (KeySequence, 1);
      g_array_append_val (array, key_sequence);   

      array_characters = g_array_sized_new (FALSE /* Not zero_terminated */, TRUE /* clear */,
                                           sizeof(gchar*), 10 /* reserved size */);

      for (value_index = 0; value_index < length_values; ++value_index)
      {
        gchar* value;
        gchar* value_copy;

        value = values[value_index];

        if (value_index == 0)
        {
          g_assert (strlen(value) > 0);

          key_sequence->key_press = g_utf8_get_char (value);
        }

        value_copy = g_strdup (value);
        g_array_append_val (array_characters, value_copy);

        /* printf("debug: Key=%s, value=%s\n", key_name, value); */
      }

      g_strfreev (values);

      key_sequence->characters_length = array_characters->len;
      key_sequence->characters = (gchar**)g_array_free(array_characters, FALSE /* Don't free items - return a real array of them. */);
    }

    g_free (key_name);
    ++key_suffix_num;
  }

  g_key_file_free (key_file);

  self->key_sequences_count = array->len;
  self->key_sequences = (KeySequence**)g_array_free (array, FALSE /* Don't free items - return a real array of them. */);

  /* debug_output_key_sequences_array(self); */
}
