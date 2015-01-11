/* GTK - The GIMP Toolkit
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
 *
 * Author:  Theppitak Karoonboonyanan <thep@linux.thai.net>
 *
 */

#include <string.h>

#include <gdk/gdkkeysyms.h>
#include "gtkimcontextthai.h"
#include "thai-charprop.h"

static void     gtk_im_context_thai_class_init          (GtkIMContextThaiClass *class);
static void     gtk_im_context_thai_init                (GtkIMContextThai      *im_context_thai);
static gboolean gtk_im_context_thai_filter_keypress     (GtkIMContext          *context,
						         GdkEventKey           *key);

#ifndef GTK_IM_CONTEXT_THAI_NO_FALLBACK
static void     forget_previous_chars (GtkIMContextThai *context_thai);
static void     remember_previous_char (GtkIMContextThai *context_thai,
                                        gunichar new_char);
#endif /* !GTK_IM_CONTEXT_THAI_NO_FALLBACK */

static GObjectClass *parent_class;

GType gtk_type_im_context_thai = 0;

void
gtk_im_context_thai_register_type (GTypeModule *type_module)
{
  const GTypeInfo im_context_thai_info =
  {
    sizeof (GtkIMContextThaiClass),
    (GBaseInitFunc) NULL,
    (GBaseFinalizeFunc) NULL,
    (GClassInitFunc) gtk_im_context_thai_class_init,
    NULL,           /* class_finalize */    
    NULL,           /* class_data */
    sizeof (GtkIMContextThai),
    0,
    (GInstanceInitFunc) gtk_im_context_thai_init,
  };

  gtk_type_im_context_thai = 
    g_type_module_register_type (type_module,
                                 GTK_TYPE_IM_CONTEXT,
                                 "GtkIMContextThai",
                                 &im_context_thai_info, 0);
}

static void
gtk_im_context_thai_class_init (GtkIMContextThaiClass *class)
{
  GtkIMContextClass *im_context_class = GTK_IM_CONTEXT_CLASS (class);

  parent_class = g_type_class_peek_parent (class);

  im_context_class->filter_keypress = gtk_im_context_thai_filter_keypress;
}

static void
gtk_im_context_thai_init (GtkIMContextThai *context_thai)
{
#ifndef GTK_IM_CONTEXT_THAI_NO_FALLBACK
  forget_previous_chars (context_thai);
#endif /* !GTK_IM_CONTEXT_THAI_NO_FALLBACK */
  context_thai->isc_mode = ISC_BASICCHECK;
}

GtkIMContext *
gtk_im_context_thai_new (void)
{
  GtkIMContextThai *result;

  result = GTK_IM_CONTEXT_THAI (g_object_new (GTK_TYPE_IM_CONTEXT_THAI, NULL));

  return GTK_IM_CONTEXT (result);
}

GtkIMContextThaiISCMode
gtk_im_context_thai_get_isc_mode (GtkIMContextThai *context_thai)
{
  return context_thai->isc_mode;
}

GtkIMContextThaiISCMode
gtk_im_context_thai_set_isc_mode (GtkIMContextThai *context_thai,
                                  GtkIMContextThaiISCMode mode)
{
  GtkIMContextThaiISCMode prev_mode = context_thai->isc_mode;
  context_thai->isc_mode = mode;
  return prev_mode;
}

static gboolean
is_context_lost_key(guint keyval)
{
  return ((keyval & 0xFF00) == 0xFF00) &&
         (keyval == GDK_KEY_BackSpace ||
          keyval == GDK_KEY_Tab ||
          keyval == GDK_KEY_Linefeed ||
          keyval == GDK_KEY_Clear ||
          keyval == GDK_KEY_Return ||
          keyval == GDK_KEY_Pause ||
          keyval == GDK_KEY_Scroll_Lock ||
          keyval == GDK_KEY_Sys_Req ||
          keyval == GDK_KEY_Escape ||
          keyval == GDK_KEY_Delete ||
          (GDK_KEY_Home <= keyval && keyval <= GDK_KEY_Begin) || /* IsCursorkey */
          (GDK_KEY_KP_Space <= keyval && keyval <= GDK_KEY_KP_Delete) || /* IsKeypadKey, non-chars only */
          (GDK_KEY_Select <= keyval && keyval <= GDK_KEY_Break) || /* IsMiscFunctionKey */
          (GDK_KEY_F1 <= keyval && keyval <= GDK_KEY_F35)); /* IsFunctionKey */
}

static gboolean
is_context_intact_key(guint keyval)
{
  return (((keyval & 0xFF00) == 0xFF00) &&
           ((GDK_KEY_Shift_L <= keyval && keyval <= GDK_KEY_Hyper_R) || /* IsModifierKey */
            (keyval == GDK_KEY_Mode_switch) ||
            (keyval == GDK_KEY_Num_Lock))) ||
         (((keyval & 0xFE00) == 0xFE00) &&
          (GDK_KEY_ISO_Lock <= keyval && keyval <= GDK_KEY_ISO_Last_Group_Lock));
}

static gboolean
thai_is_accept (gunichar new_char, gunichar prev_char, gint isc_mode)
{
  switch (isc_mode)
    {
    case ISC_PASSTHROUGH:
      return TRUE;

    case ISC_BASICCHECK:
      return TAC_compose_input (prev_char, new_char) != 'R';

    case ISC_STRICT:
      {
        int op = TAC_compose_input (prev_char, new_char);
        return op != 'R' && op != 'S';
      }

    default:
      return FALSE;
    }
}

#define thai_is_composible(n,p)  (TAC_compose_input ((p), (n)) == 'C')

#ifndef GTK_IM_CONTEXT_THAI_NO_FALLBACK
static void
forget_previous_chars (GtkIMContextThai *context_thai)
{
  memset (context_thai->char_buff, 0, sizeof (context_thai->char_buff));
}

static void
remember_previous_char (GtkIMContextThai *context_thai, gunichar new_char)
{
  memmove (context_thai->char_buff + 1, context_thai->char_buff,
           (GTK_IM_CONTEXT_THAI_BUFF_SIZE - 1) * sizeof (context_thai->char_buff[0]));
  context_thai->char_buff[0] = new_char;
}
#endif /* !GTK_IM_CONTEXT_THAI_NO_FALLBACK */

static gunichar
get_previous_char (GtkIMContextThai *context_thai, gint offset)
{
  gchar *surrounding;
  gint  cursor_index;

  if (gtk_im_context_get_surrounding ((GtkIMContext *)context_thai,
                                      &surrounding, &cursor_index))
    {
      gunichar prev_char;
      gchar *p, *q;

      prev_char = 0;
      p = surrounding + cursor_index;
      for (q = p; offset < 0 && q > surrounding; ++offset)
        q = g_utf8_prev_char (q);
      if (offset == 0)
        {
          prev_char = g_utf8_get_char_validated (q, p - q);
          if (prev_char == (gunichar)-1 || prev_char == (gunichar)-2)
            prev_char = 0;
        }
      g_free (surrounding);
      return prev_char;
    }
#ifndef GTK_IM_CONTEXT_THAI_NO_FALLBACK
  else
    {
      offset = -offset - 1;
      if (0 <= offset && offset < GTK_IM_CONTEXT_THAI_BUFF_SIZE)
        return context_thai->char_buff[offset];
    }
#endif /* !GTK_IM_CONTEXT_THAI_NO_FALLBACK */

    return 0;
}

static gboolean
gtk_im_context_thai_commit_chars (GtkIMContextThai *context_thai,
                                  gunichar *s, gsize len)
{
  gchar *utf8;

  utf8 = g_ucs4_to_utf8 (s, len, NULL, NULL, NULL);
  if (!utf8)
    return FALSE;

  g_signal_emit_by_name (context_thai, "commit", utf8);

  g_free (utf8);
  return TRUE;
}

static gboolean
accept_input (GtkIMContextThai *context_thai, gunichar new_char)
{
#ifndef GTK_IM_CONTEXT_THAI_NO_FALLBACK
  remember_previous_char (context_thai, new_char);
#endif /* !GTK_IM_CONTEXT_THAI_NO_FALLBACK */

  return gtk_im_context_thai_commit_chars (context_thai, &new_char, 1);
}

static gboolean
reorder_input (GtkIMContextThai *context_thai,
               gunichar prev_char, gunichar new_char)
{
  gunichar buf[2];

  if (!gtk_im_context_delete_surrounding (GTK_IM_CONTEXT (context_thai), -1, 1))
    return FALSE;

#ifndef GTK_IM_CONTEXT_THAI_NO_FALLBACK
  forget_previous_chars (context_thai);
  remember_previous_char (context_thai, new_char);
  remember_previous_char (context_thai, prev_char);
#endif /* !GTK_IM_CONTEXT_THAI_NO_FALLBACK */

  buf[0] = new_char;
  buf[1] = prev_char;
  return gtk_im_context_thai_commit_chars (context_thai, buf, 2);
}

static gboolean
replace_input (GtkIMContextThai *context_thai, gunichar new_char)
{
  if (!gtk_im_context_delete_surrounding (GTK_IM_CONTEXT (context_thai), -1, 1))
    return FALSE;

#ifndef GTK_IM_CONTEXT_THAI_NO_FALLBACK
  forget_previous_chars (context_thai);
  remember_previous_char (context_thai, new_char);
#endif /* !GTK_IM_CONTEXT_THAI_NO_FALLBACK */

  return gtk_im_context_thai_commit_chars (context_thai, &new_char, 1);
}

static gboolean
gtk_im_context_thai_filter_keypress (GtkIMContext *context,
                                     GdkEventKey  *event)
{
  GtkIMContextThai *context_thai = GTK_IM_CONTEXT_THAI (context);
  gunichar prev_char, new_char;
  gboolean is_reject;
  GtkIMContextThaiISCMode isc_mode;

  if (event->type != GDK_KEY_PRESS)
    return FALSE;

  if (event->state & (GDK_MODIFIER_MASK
                      & ~(GDK_SHIFT_MASK | GDK_LOCK_MASK | GDK_MOD2_MASK)) ||
      is_context_lost_key (event->keyval))
    {
#ifndef GTK_IM_CONTEXT_THAI_NO_FALLBACK
      forget_previous_chars (context_thai);
#endif /* !GTK_IM_CONTEXT_THAI_NO_FALLBACK */
      return FALSE;
    }
  if (event->keyval == 0 || is_context_intact_key (event->keyval))
    {
      return FALSE;
    }

  prev_char = get_previous_char (context_thai, -1);
  if (!prev_char)
    prev_char = ' ';
  new_char = gdk_keyval_to_unicode (event->keyval);
  is_reject = TRUE;
  isc_mode = gtk_im_context_thai_get_isc_mode (context_thai);
  if (thai_is_accept (new_char, prev_char, isc_mode))
    {
      accept_input (context_thai, new_char);
      is_reject = FALSE;
    }
  else
    {
      gunichar context_char;

      /* rejected, trying to correct */
      context_char = get_previous_char (context_thai, -2);
      if (context_char)
        {
          if (thai_is_composible (new_char, context_char))
            {
              if (thai_is_composible (prev_char, new_char))
                is_reject = !reorder_input (context_thai, prev_char, new_char);
              else if (thai_is_composible (prev_char, context_char))
                is_reject = !replace_input (context_thai, new_char);
              else if ((TAC_char_class (prev_char) == FV1
                        || TAC_char_class (prev_char) == AM)
                       && TAC_char_class (new_char) == TONE)
                is_reject = !reorder_input (context_thai, prev_char, new_char);
            }
	  else if (thai_is_accept (new_char, context_char, isc_mode))
            is_reject = !replace_input (context_thai, new_char);
        }
    }
  if (is_reject)
    {
      /* reject character */
      gdk_beep ();
    }
  return TRUE;
}

