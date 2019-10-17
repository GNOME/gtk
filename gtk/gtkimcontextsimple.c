/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc.
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

#include <gdk/gdk.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif
#ifdef GDK_WINDOWING_WAYLAND
#include <wayland/gdkwayland.h>
#endif
#ifdef GDK_WINDOWING_WIN32
#include <win32/gdkwin32.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "gtkprivate.h"
#include "gtkaccelgroup.h"
#include "gtkimcontextsimple.h"
#include "gtksettings.h"
#include "gtkwidget.h"
#include "gtkdebug.h"
#include "gtkintl.h"
#include "gtkcomposetable.h"
#include "gtkimmoduleprivate.h"

#include "gtkimcontextsimpleprivate.h"
#include "gtkimcontextsimpleseqs.h"

/**
 * SECTION:gtkimcontextsimple
 * @Short_description: An input method context supporting table-based input methods
 * @Title: GtkIMContextSimple
 *
 * GtkIMContextSimple is a simple input method context supporting table-based
 * input methods. It has a built-in table of compose sequences that is derived
 * from the X11 Compose files.
 *
 * GtkIMContextSimple reads additional compose sequences from the first of the
 * following files that is found: ~/.config/gtk-4.0/Compose, ~/.XCompose,
 * /usr/share/X11/locale/$locale/Compose (for locales that have a nontrivial
 * Compose file). The syntax of these files is described in the Compose(5)
 * manual page.
 *
 * ## Unicode characters
 *
 * GtkIMContextSimple also supports numeric entry of Unicode characters
 * by typing Ctrl-Shift-u, followed by a hexadecimal Unicode codepoint.
 * For example, Ctrl-Shift-u 1 2 3 Enter yields U+0123 LATIN SMALL LETTER
 * G WITH CEDILLA, i.e. ģ.
 */

struct _GtkIMContextSimplePrivate
{
  guint16        compose_buffer[GTK_MAX_COMPOSE_LEN + 1];
  gunichar       tentative_match;
  gint           tentative_match_len;

  guint          in_hex_sequence : 1;
  guint          modifiers_dropped : 1;
};

/* From the values below, the value 30 means the number of different first keysyms
 * that exist in the Compose file (from Xorg). When running compose-parse.py without
 * parameters, you get the count that you can put here. Needed when updating the
 * gtkimcontextsimpleseqs.h header file (contains the compose sequences).
 */
const GtkComposeTableCompact gtk_compose_table_compact = {
  gtk_compose_seqs_compact,
  5,
  30,
  6
};

G_LOCK_DEFINE_STATIC (global_tables);
static GSList *global_tables;

static const guint16 gtk_compose_ignore[] = {
  GDK_KEY_Shift_L,
  GDK_KEY_Shift_R,
  GDK_KEY_Control_L,
  GDK_KEY_Control_R,
  GDK_KEY_Caps_Lock,
  GDK_KEY_Shift_Lock,
  GDK_KEY_Meta_L,
  GDK_KEY_Meta_R,
  GDK_KEY_Alt_L,
  GDK_KEY_Alt_R,
  GDK_KEY_Super_L,
  GDK_KEY_Super_R,
  GDK_KEY_Hyper_L,
  GDK_KEY_Hyper_R,
  GDK_KEY_Mode_switch,
  GDK_KEY_ISO_Level3_Shift
};

static void     gtk_im_context_simple_finalize           (GObject                  *obj);
static gboolean gtk_im_context_simple_filter_keypress    (GtkIMContext             *context,
							  GdkEventKey              *key);
static void     gtk_im_context_simple_reset              (GtkIMContext             *context);
static void     gtk_im_context_simple_get_preedit_string (GtkIMContext             *context,
							  gchar                   **str,
							  PangoAttrList           **attrs,
							  gint                     *cursor_pos);
static void     gtk_im_context_simple_set_client_widget  (GtkIMContext             *context,
                                                          GtkWidget                *widget);

G_DEFINE_TYPE_WITH_CODE (GtkIMContextSimple, gtk_im_context_simple, GTK_TYPE_IM_CONTEXT,
                         G_ADD_PRIVATE (GtkIMContextSimple)
                         gtk_im_module_ensure_extension_point ();
                         g_io_extension_point_implement (GTK_IM_MODULE_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "gtk-im-context-simple",
                                                         G_MININT))

static void
gtk_im_context_simple_class_init (GtkIMContextSimpleClass *class)
{
  GtkIMContextClass *im_context_class = GTK_IM_CONTEXT_CLASS (class);
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  im_context_class->filter_keypress = gtk_im_context_simple_filter_keypress;
  im_context_class->reset = gtk_im_context_simple_reset;
  im_context_class->get_preedit_string = gtk_im_context_simple_get_preedit_string;
  im_context_class->set_client_widget = gtk_im_context_simple_set_client_widget;
  gobject_class->finalize = gtk_im_context_simple_finalize;
}

static gchar*
get_x11_compose_file_dir (void)
{
  gchar* compose_file_dir;

#if defined (GDK_WINDOWING_X11)
  compose_file_dir = g_strdup (X11_DATA_PREFIX "/share/X11/locale");
#else
  compose_file_dir = g_build_filename (_gtk_get_datadir (), "X11", "locale", NULL);
#endif

  return compose_file_dir;
}

static void
gtk_im_context_simple_init_compose_table (GtkIMContextSimple *im_context_simple)
{
  gchar *path = NULL;
  const gchar *home;
  const gchar *locale;
  gchar **langs = NULL;
  gchar **lang = NULL;
  const char * const sys_langs[] = { "el_gr", "fi_fi", "pt_br", NULL };
  const char * const *sys_lang = NULL;
  gchar *x11_compose_file_dir = get_x11_compose_file_dir ();

  path = g_build_filename (g_get_user_config_dir (), "gtk-4.0", "Compose", NULL);
  if (g_file_test (path, G_FILE_TEST_EXISTS))
    {
      gtk_im_context_simple_add_compose_file (im_context_simple, path);
      g_free (path);
      return;
    }
  g_free (path);
  path = NULL;

  home = g_get_home_dir ();
  if (home == NULL)
    return;

  path = g_build_filename (home, ".XCompose", NULL);
  if (g_file_test (path, G_FILE_TEST_EXISTS))
    {
      gtk_im_context_simple_add_compose_file (im_context_simple, path);
      g_free (path);
      return;
    }
  g_free (path);
  path = NULL;

  locale = g_getenv ("LC_CTYPE");
  if (locale == NULL)
    locale = g_getenv ("LANG");
  if (locale == NULL)
    locale = "C";

  /* FIXME: https://bugzilla.gnome.org/show_bug.cgi?id=751826 */
  langs = g_get_locale_variants (locale);

  for (lang = langs; *lang; lang++)
    {
      if (g_str_has_prefix (*lang, "en_US"))
        break;
      if (**lang == 'C')
        break;

      /* Other languages just include en_us compose table. */
      for (sys_lang = sys_langs; *sys_lang; sys_lang++)
        {
          if (g_ascii_strncasecmp (*lang, *sys_lang, strlen (*sys_lang)) == 0)
            {
              path = g_build_filename (x11_compose_file_dir, *lang, "Compose", NULL);
              break;
            }
        }

      if (path == NULL)
        continue;

      if (g_file_test (path, G_FILE_TEST_EXISTS))
        break;
      g_free (path);
      path = NULL;
    }

  g_free (x11_compose_file_dir);
  g_strfreev (langs);

  if (path != NULL)
    gtk_im_context_simple_add_compose_file (im_context_simple, path);
  g_free (path);
  path = NULL;
}

static void
init_compose_table_thread_cb (GTask            *task,
                              gpointer          source_object,
                              gpointer          task_data,
                              GCancellable     *cancellable)
{
  if (g_task_return_error_if_cancelled (task))
    return;

  g_return_if_fail (GTK_IS_IM_CONTEXT_SIMPLE (task_data));

  gtk_im_context_simple_init_compose_table (GTK_IM_CONTEXT_SIMPLE (task_data));
}

static void
init_compose_table_async (GtkIMContextSimple   *im_context_simple,
                          GCancellable         *cancellable,
                          GAsyncReadyCallback   callback,
                          gpointer              user_data)
{
  GTask *task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_source_tag (task, init_compose_table_async);
  g_task_set_task_data (task, g_object_ref (im_context_simple), g_object_unref);
  g_task_run_in_thread (task, init_compose_table_thread_cb);
  g_object_unref (task);
}

static void
gtk_im_context_simple_init (GtkIMContextSimple *im_context_simple)
{
  im_context_simple->priv = gtk_im_context_simple_get_instance_private (im_context_simple); 
}

static void
gtk_im_context_simple_finalize (GObject *obj)
{
  G_OBJECT_CLASS (gtk_im_context_simple_parent_class)->finalize (obj);
}

/**
 * gtk_im_context_simple_new:
 * 
 * Creates a new #GtkIMContextSimple.
 *
 * Returns: a new #GtkIMContextSimple.
 **/
GtkIMContext *
gtk_im_context_simple_new (void)
{
  return g_object_new (GTK_TYPE_IM_CONTEXT_SIMPLE, NULL);
}

static void
gtk_im_context_simple_commit_char (GtkIMContext *context,
				   gunichar ch)
{
  GtkIMContextSimple *context_simple = GTK_IM_CONTEXT_SIMPLE (context);
  GtkIMContextSimplePrivate *priv = context_simple->priv;
  gchar buf[10];
  gint len;

  g_return_if_fail (g_unichar_validate (ch));

  len = g_unichar_to_utf8 (ch, buf);
  buf[len] = '\0';

  if (priv->tentative_match || priv->in_hex_sequence)
    {
      priv->in_hex_sequence = FALSE;
      priv->tentative_match = 0;
      priv->tentative_match_len = 0;
      g_signal_emit_by_name (context_simple, "preedit-changed");
      g_signal_emit_by_name (context_simple, "preedit-end");
    }

  g_signal_emit_by_name (context, "commit", &buf);
}

static int
compare_seq_index (const void *key, const void *value)
{
  const guint16 *keysyms = key;
  const guint16 *seq = value;

  if (keysyms[0] < seq[0])
    return -1;
  else if (keysyms[0] > seq[0])
    return 1;

  return 0;
}

static int
compare_seq (const void *key, const void *value)
{
  int i = 0;
  const guint16 *keysyms = key;
  const guint16 *seq = value;

  while (keysyms[i])
    {
      if (keysyms[i] < seq[i])
	return -1;
      else if (keysyms[i] > seq[i])
	return 1;

      i++;
    }

  return 0;
}

static gboolean
check_table (GtkIMContextSimple    *context_simple,
	     const GtkComposeTable *table,
	     gint                   n_compose)
{
  GtkIMContextSimplePrivate *priv = context_simple->priv;
  gint row_stride = table->max_seq_len + 2; 
  guint16 *seq; 
  
  /* Will never match, if the sequence in the compose buffer is longer
   * than the sequences in the table.  Further, compare_seq (key, val)
   * will overrun val if key is longer than val. */
  if (n_compose > table->max_seq_len)
    return FALSE;
  
  seq = bsearch (priv->compose_buffer,
		 table->data, table->n_seqs,
		 sizeof (guint16) *  row_stride, 
		 compare_seq);

  if (seq)
    {
      guint16 *prev_seq;

      /* Back up to the first sequence that matches to make sure
       * we find the exact match if there is one.
       */
      while (seq > table->data)
	{
	  prev_seq = seq - row_stride;
	  if (compare_seq (priv->compose_buffer, prev_seq) != 0)
	    break;
	  seq = prev_seq;
	}
      
      if (n_compose == table->max_seq_len ||
	  seq[n_compose] == 0) /* complete sequence */
	{
	  guint16 *next_seq;
	  gunichar value = 
	    0x10000 * seq[table->max_seq_len] + seq[table->max_seq_len + 1];

	  /* We found a tentative match. See if there are any longer
	   * sequences containing this subsequence
	   */
	  next_seq = seq + row_stride;
	  if (next_seq < table->data + row_stride * table->n_seqs)
	    {
	      if (compare_seq (priv->compose_buffer, next_seq) == 0)
		{
		  priv->tentative_match = value;
		  priv->tentative_match_len = n_compose;

		  g_signal_emit_by_name (context_simple, "preedit-changed");

		  return TRUE;
		}
	    }

	  gtk_im_context_simple_commit_char (GTK_IM_CONTEXT (context_simple), value);
	  priv->compose_buffer[0] = 0;
	}
      
      return TRUE;
    }

  return FALSE;
}

/* Checks if a keysym is a dead key. Dead key keysym values are defined in
 * ../gdk/gdkkeysyms.h and the first is GDK_KEY_dead_grave. As X.Org is updated,
 * more dead keys are added and we need to update the upper limit.
 * Currently, the upper limit is GDK_KEY_dead_dasia+1. The +1 has to do with
 * a temporary issue in the X.Org header files.
 * In future versions it will be just the keysym (no +1).
 */
#define IS_DEAD_KEY(k) \
    ((k) >= GDK_KEY_dead_grave && (k) <= (GDK_KEY_dead_dasia+1))

#ifdef GDK_WINDOWING_WIN32

/* On Windows, user expectation is that typing a dead accent followed
 * by space will input the corresponding spacing character. The X
 * compose tables are different for dead acute and diaeresis, which
 * when followed by space produce a plain ASCII apostrophe and double
 * quote respectively. So special-case those.
 */

static gboolean
check_win32_special_cases (GtkIMContextSimple    *context_simple,
			   gint                   n_compose)
{
  GtkIMContextSimplePrivate *priv = context_simple->priv;
  if (n_compose == 2 &&
      priv->compose_buffer[1] == GDK_KEY_space)
    {
      gunichar value = 0;

      switch (priv->compose_buffer[0])
	{
	case GDK_KEY_dead_acute:
	  value = 0x00B4; break;
	case GDK_KEY_dead_diaeresis:
	  value = 0x00A8; break;
        default:
          break;
	}
      if (value > 0)
	{
	  gtk_im_context_simple_commit_char (GTK_IM_CONTEXT (context_simple), value);
	  priv->compose_buffer[0] = 0;

	  return TRUE;
	}
    }
  return FALSE;
}

static void
check_win32_special_case_after_compact_match (GtkIMContextSimple    *context_simple,
					      gint                   n_compose,
					      guint                  value)
{
  GtkIMContextSimplePrivate *priv = context_simple->priv;

  /* On Windows user expectation is that typing two dead accents will input
   * two corresponding spacing accents.
   */
  if (n_compose == 2 &&
      priv->compose_buffer[0] == priv->compose_buffer[1] &&
      IS_DEAD_KEY (priv->compose_buffer[0]))
    {
      gtk_im_context_simple_commit_char (GTK_IM_CONTEXT (context_simple), value);
    }
}

#endif

#ifdef GDK_WINDOWING_QUARTZ

static gboolean
check_quartz_special_cases (GtkIMContextSimple *context_simple,
                            gint                n_compose)
{
  GtkIMContextSimplePrivate *priv = context_simple->priv;
  guint value = 0;

  if (n_compose == 2)
    {
      switch (priv->compose_buffer[0])
        {
        case GDK_KEY_dead_doubleacute:
          switch (priv->compose_buffer[1])
            {
            case GDK_KEY_dead_doubleacute:
            case GDK_KEY_space:
              value = GDK_KEY_quotedbl; break;

            case 'a': value = GDK_KEY_adiaeresis; break;
            case 'A': value = GDK_KEY_Adiaeresis; break;
            case 'e': value = GDK_KEY_ediaeresis; break;
            case 'E': value = GDK_KEY_Ediaeresis; break;
            case 'i': value = GDK_KEY_idiaeresis; break;
            case 'I': value = GDK_KEY_Idiaeresis; break;
            case 'o': value = GDK_KEY_odiaeresis; break;
            case 'O': value = GDK_KEY_Odiaeresis; break;
            case 'u': value = GDK_KEY_udiaeresis; break;
            case 'U': value = GDK_KEY_Udiaeresis; break;
            case 'y': value = GDK_KEY_ydiaeresis; break;
            case 'Y': value = GDK_KEY_Ydiaeresis; break;
            }
          break;

        case GDK_KEY_dead_acute:
          switch (priv->compose_buffer[1])
            {
            case 'c': value = GDK_KEY_ccedilla; break;
            case 'C': value = GDK_KEY_Ccedilla; break;
            }
          break;
        }
    }

  if (value > 0)
    {
      gtk_im_context_simple_commit_char (GTK_IM_CONTEXT (context_simple),
                                         gdk_keyval_to_unicode (value));
      priv->compose_buffer[0] = 0;

      return TRUE;
    }

  return FALSE;
}

#endif

gboolean
gtk_check_compact_table (const GtkComposeTableCompact  *table,
                         guint16                       *compose_buffer,
                         gint                           n_compose,
                         gboolean                      *compose_finish,
                         gboolean                      *compose_match,
                         gunichar                      *output_char)
{
  gint row_stride;
  guint16 *seq_index;
  guint16 *seq;
  gint i;
  gboolean match;
  gunichar value;

  if (compose_finish)
    *compose_finish = FALSE;
  if (compose_match)
    *compose_match = FALSE;
  if (output_char)
    *output_char = 0;

  /* Will never match, if the sequence in the compose buffer is longer
   * than the sequences in the table.  Further, compare_seq (key, val)
   * will overrun val if key is longer than val.
   */
  if (n_compose > table->max_seq_len)
    return FALSE;

  seq_index = bsearch (compose_buffer,
                       table->data,
                       table->n_index_size,
                       sizeof (guint16) * table->n_index_stride,
                       compare_seq_index);

  if (!seq_index)
    return FALSE;

  if (seq_index && n_compose == 1)
    return TRUE;

  seq = NULL;
  match = FALSE;
  value = 0;

  for (i = n_compose - 1; i < table->max_seq_len; i++)
    {
      row_stride = i + 1;

      if (seq_index[i + 1] - seq_index[i] > 0)
        {
          seq = bsearch (compose_buffer + 1,
                         table->data + seq_index[i],
                         (seq_index[i + 1] - seq_index[i]) / row_stride,
                         sizeof (guint16) *  row_stride,
                         compare_seq);

          if (seq)
            {
              if (i == n_compose - 1)
                {
                  value = seq[row_stride - 1];
                  match = TRUE;
                }
              else
                {
                  if (output_char)
                    *output_char = value;
                  if (match)
                    {
                      if (compose_match)
                        *compose_match = TRUE;
                    }

                  return TRUE;
                }
            }
        }
    }

  if (match)
    {
      if (compose_match)
        *compose_match = TRUE;
      if (compose_finish)
        *compose_finish = TRUE;
      if (output_char)
        *output_char = value;

      return TRUE;
    }

  return FALSE;
}

/* This function receives a sequence of Unicode characters and tries to
 * normalize it (NFC). We check for the case where the resulting string
 * has length 1 (single character).
 * NFC normalisation normally rearranges diacritic marks, unless these
 * belong to the same Canonical Combining Class.
 * If they belong to the same canonical combining class, we produce all
 * permutations of the diacritic marks, then attempt to normalize.
 */
static gboolean
check_normalize_nfc (gunichar* combination_buffer, gint n_compose)
{
  gunichar combination_buffer_temp[GTK_MAX_COMPOSE_LEN];
  gchar *combination_utf8_temp = NULL;
  gchar *nfc_temp = NULL;
  gint n_combinations;
  gunichar temp_swap;
  gint i;

  n_combinations = 1;

  for (i = 1; i < n_compose; i++ )
     n_combinations *= i;

  /* Xorg reuses dead_tilde for the perispomeni diacritic mark.
   * We check if base character belongs to Greek Unicode block,
   * and if so, we replace tilde with perispomeni.
   */
  if (combination_buffer[0] >= 0x390 && combination_buffer[0] <= 0x3FF)
    {
      for (i = 1; i < n_compose; i++ )
        if (combination_buffer[i] == 0x303)
          combination_buffer[i] = 0x342;
    }

  memcpy (combination_buffer_temp, combination_buffer, GTK_MAX_COMPOSE_LEN * sizeof (gunichar) );

  for (i = 0; i < n_combinations; i++ )
    {
      g_unicode_canonical_ordering (combination_buffer_temp, n_compose);
      combination_utf8_temp = g_ucs4_to_utf8 (combination_buffer_temp, -1, NULL, NULL, NULL);
      nfc_temp = g_utf8_normalize (combination_utf8_temp, -1, G_NORMALIZE_NFC);

      if (g_utf8_strlen (nfc_temp, -1) == 1)
        {
          memcpy (combination_buffer, combination_buffer_temp, GTK_MAX_COMPOSE_LEN * sizeof (gunichar) );

          g_free (combination_utf8_temp);
          g_free (nfc_temp);

          return TRUE;
        }

      g_free (combination_utf8_temp);
      g_free (nfc_temp);

      if (n_compose > 2)
        {
          temp_swap = combination_buffer_temp[i % (n_compose - 1) + 1];
          combination_buffer_temp[i % (n_compose - 1) + 1] = combination_buffer_temp[(i+1) % (n_compose - 1) + 1];
          combination_buffer_temp[(i+1) % (n_compose - 1) + 1] = temp_swap;
        }
      else
        break;
    }

  return FALSE;
}

gboolean
gtk_check_algorithmically (const guint16       *compose_buffer,
                           gint                 n_compose,
                           gunichar            *output_char)

{
  gint i;
  gunichar combination_buffer[GTK_MAX_COMPOSE_LEN];
  gchar *combination_utf8, *nfc;

  if (output_char)
    *output_char = 0;

  if (n_compose >= GTK_MAX_COMPOSE_LEN)
    return FALSE;

  for (i = 0; i < n_compose && IS_DEAD_KEY (compose_buffer[i]); i++)
    ;
  if (i == n_compose)
    return TRUE;

  if (i > 0 && i == n_compose - 1)
    {
      combination_buffer[0] = gdk_keyval_to_unicode (compose_buffer[i]);
      combination_buffer[n_compose] = 0;
      i--;
      while (i >= 0)
	{
	  switch (compose_buffer[i])
	    {
#define CASE(keysym, unicode) \
	    case GDK_KEY_dead_##keysym: combination_buffer[i+1] = unicode; break

	    CASE (grave, 0x0300);
	    CASE (acute, 0x0301);
	    CASE (circumflex, 0x0302);
	    CASE (tilde, 0x0303);	/* Also used with perispomeni, 0x342. */
	    CASE (macron, 0x0304);
	    CASE (breve, 0x0306);
	    CASE (abovedot, 0x0307);
	    CASE (diaeresis, 0x0308);
	    CASE (hook, 0x0309);
	    CASE (abovering, 0x030A);
	    CASE (doubleacute, 0x030B);
	    CASE (caron, 0x030C);
	    CASE (abovecomma, 0x0313);         /* Equivalent to psili */
	    CASE (abovereversedcomma, 0x0314); /* Equivalent to dasia */
	    CASE (horn, 0x031B);	/* Legacy use for psili, 0x313 (or 0x343). */
	    CASE (belowdot, 0x0323);
	    CASE (cedilla, 0x0327);
	    CASE (ogonek, 0x0328);	/* Legacy use for dasia, 0x314.*/
	    CASE (iota, 0x0345);
	    CASE (voiced_sound, 0x3099);	/* Per Markus Kuhn keysyms.txt file. */
	    CASE (semivoiced_sound, 0x309A);	/* Per Markus Kuhn keysyms.txt file. */

	    /* The following cases are to be removed once xkeyboard-config,
 	     * xorg are fully updated.
 	     */
            /* Workaround for typo in 1.4.x xserver-xorg */
	    case 0xfe66: combination_buffer[i+1] = 0x314; break;
	    /* CASE (dasia, 0x314); */
	    /* CASE (perispomeni, 0x342); */
	    /* CASE (psili, 0x343); */
#undef CASE
	    default:
	      combination_buffer[i+1] = gdk_keyval_to_unicode (compose_buffer[i]);
	    }
	  i--;
	}
      
      /* If the buffer normalizes to a single character, then modify the order
       * of combination_buffer accordingly, if necessary, and return TRUE.
       */
      if (check_normalize_nfc (combination_buffer, n_compose))
        {
      	  combination_utf8 = g_ucs4_to_utf8 (combination_buffer, -1, NULL, NULL, NULL);
          nfc = g_utf8_normalize (combination_utf8, -1, G_NORMALIZE_NFC);

          if (output_char)
            *output_char = g_utf8_get_char (nfc);

          g_free (combination_utf8);
          g_free (nfc);

          return TRUE;
        }
    }

  return FALSE;
}

/* In addition to the table-driven sequences, we allow Unicode hex
 * codes to be entered. The method chosen here is similar to the
 * one recommended in ISO 14755, but not exactly the same, since we
 * don’t want to steal 16 valuable key combinations.
 *
 * A hex Unicode sequence must be started with Ctrl-Shift-U, followed
 * by a sequence of hex digits entered with Ctrl-Shift still held.
 * Releasing one of the modifiers or pressing space while the modifiers
 * are still held commits the character. It is possible to erase
 * digits using backspace.
 *
 * As an extension to the above, we also allow to start the sequence
 * with Ctrl-Shift-U, then release the modifiers before typing any
 * digits, and enter the digits without modifiers.
 */

static gboolean
check_hex (GtkIMContextSimple *context_simple,
           gint                n_compose)
{
  GtkIMContextSimplePrivate *priv = context_simple->priv;
  /* See if this is a hex sequence, return TRUE if so */
  gint i;
  GString *str;
  gulong n;
  gchar *nptr = NULL;
  gchar buf[7];

  priv->tentative_match = 0;
  priv->tentative_match_len = 0;

  str = g_string_new (NULL);
  
  i = 0;
  while (i < n_compose)
    {
      gunichar ch;
      
      ch = gdk_keyval_to_unicode (priv->compose_buffer[i]);
      
      if (ch == 0)
        return FALSE;

      if (!g_unichar_isxdigit (ch))
        return FALSE;

      buf[g_unichar_to_utf8 (ch, buf)] = '\0';

      g_string_append (str, buf);
      
      ++i;
    }

  n = strtoul (str->str, &nptr, 16);

  /* If strtoul fails it probably means non-latin digits were used;
   * we should in principle handle that, but we probably don't.
   */
  if (nptr - str->str < str->len)
    {
      g_string_free (str, TRUE);
      return FALSE;
    }
  else
    g_string_free (str, TRUE);

  if (g_unichar_validate (n))
    {
      priv->tentative_match = n;
      priv->tentative_match_len = n_compose;
    }
  
  return TRUE;
}

static void
beep_surface (GdkSurface *surface)
{
  GdkDisplay *display = gdk_surface_get_display (surface);
  gboolean   beep;

  g_object_get (gtk_settings_get_for_display (display),
                "gtk-error-bell", &beep,
                NULL);

  if (beep)
    gdk_surface_beep (surface);
}

static gboolean
no_sequence_matches (GtkIMContextSimple *context_simple,
                     gint                n_compose,
                     GdkEventKey        *event)
{
  GtkIMContextSimplePrivate *priv = context_simple->priv;
  GtkIMContext *context;
  gunichar ch;
  guint keyval;

  context = GTK_IM_CONTEXT (context_simple);
  
  /* No compose sequences found, check first if we have a partial
   * match pending.
   */
  if (priv->tentative_match)
    {
      gint len = priv->tentative_match_len;
      int i;
      
      gtk_im_context_simple_commit_char (context, priv->tentative_match);
      priv->compose_buffer[0] = 0;
      
      for (i=0; i < n_compose - len - 1; i++)
	{
	  GdkEvent *tmp_event = gdk_event_copy ((GdkEvent *)event);
          gdk_event_set_keyval (tmp_event, priv->compose_buffer[len + i]);
	  
	  gtk_im_context_filter_keypress (context, (GdkEventKey *)tmp_event);
	  g_object_unref (tmp_event);
	}

      return gtk_im_context_filter_keypress (context, event);
    }
  else if (gdk_event_get_keyval ((GdkEvent *) event, &keyval))
    {
      priv->compose_buffer[0] = 0;
      if (n_compose > 1)		/* Invalid sequence */
	{
	  beep_surface (gdk_event_get_surface ((GdkEvent *) event));
	  return TRUE;
	}
  
      ch = gdk_keyval_to_unicode (keyval);
      if (ch != 0 && !g_unichar_iscntrl (ch))
	{
	  gtk_im_context_simple_commit_char (context, ch);
	  return TRUE;
	}
      else
	return FALSE;
    }
  else
    return FALSE;
}

static gboolean
is_hex_keyval (guint keyval)
{
  gunichar ch = gdk_keyval_to_unicode (keyval);

  return g_unichar_isxdigit (ch);
}

static guint
canonical_hex_keyval (GdkEventKey *event)
{
  GdkSurface *surface = gdk_event_get_surface ((GdkEvent *) event);
  GdkKeymap *keymap = gdk_display_get_keymap (gdk_surface_get_display (surface));
  guint keyval, event_keyval;
  guint *keyvals = NULL;
  gint n_vals = 0;
  gint i;

  if (!gdk_event_get_keyval ((GdkEvent *) event, &event_keyval))
    return 0;

  /* See if the keyval is already a hex digit */
  if (is_hex_keyval (event_keyval))
    return event_keyval;

  /* See if this key would have generated a hex keyval in
   * any other state, and return that hex keyval if so
   */
  gdk_keymap_get_entries_for_keycode (keymap,
                                      gdk_event_get_scancode ((GdkEvent *) event),
				      NULL,
				      &keyvals, &n_vals);

  keyval = 0;
  i = 0;
  while (i < n_vals)
    {
      if (is_hex_keyval (keyvals[i]))
        {
          keyval = keyvals[i];
          break;
        }

      ++i;
    }

  g_free (keyvals);
  
  if (keyval)
    return keyval;
  else
    /* No way to make it a hex digit
     */
    return 0;
}

static gboolean
gtk_im_context_simple_filter_keypress (GtkIMContext *context,
				       GdkEventKey  *event)
{
  GtkIMContextSimple *context_simple = GTK_IM_CONTEXT_SIMPLE (context);
  GtkIMContextSimplePrivate *priv = context_simple->priv;
  GdkSurface *surface = gdk_event_get_surface ((GdkEvent *) event);
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkKeymap *keymap = gdk_display_get_keymap (display);
  GSList *tmp_list;
  int n_compose = 0;
  GdkModifierType hex_mod_mask;
  gboolean have_hex_mods;
  gboolean is_hex_start;
  gboolean is_hex_end;
  gboolean is_backspace;
  gboolean is_escape;
  guint hex_keyval;
  int i;
  gboolean compose_finish;
  gboolean compose_match;
  gunichar output_char;
  guint keyval, state;

  while (priv->compose_buffer[n_compose] != 0)
    n_compose++;

  if (!gdk_event_get_keyval ((GdkEvent *) event, &keyval) ||
      !gdk_event_get_state ((GdkEvent *) event, &state))
    return GDK_EVENT_PROPAGATE;

  if (gdk_event_get_event_type ((GdkEvent *) event) == GDK_KEY_RELEASE)
    {
      if (priv->in_hex_sequence &&
          (keyval == GDK_KEY_Control_L || keyval == GDK_KEY_Control_R ||
	   keyval == GDK_KEY_Shift_L || keyval == GDK_KEY_Shift_R))
	{
          if (priv->tentative_match &&
	      g_unichar_validate (priv->tentative_match))
	    {
	      gtk_im_context_simple_commit_char (context, priv->tentative_match);
	      priv->compose_buffer[0] = 0;

	      return TRUE;
	    }
	  else if (n_compose == 0)
	    {
	      priv->modifiers_dropped = TRUE;

	      return TRUE;
	    }
	  else if (priv->in_hex_sequence)
	    {
	      /* invalid hex sequence */
	      beep_surface (surface);

	      priv->tentative_match = 0;
	      priv->in_hex_sequence = FALSE;
	      priv->compose_buffer[0] = 0;

	      g_signal_emit_by_name (context_simple, "preedit-changed");
	      g_signal_emit_by_name (context_simple, "preedit-end");

	      return TRUE;
	    }
	}

      return FALSE;
    }

  /* Ignore modifier key presses */
  for (i = 0; i < G_N_ELEMENTS (gtk_compose_ignore); i++)
    if (keyval == gtk_compose_ignore[i])
      return FALSE;

  hex_mod_mask = gdk_keymap_get_modifier_mask (keymap, GDK_MODIFIER_INTENT_PRIMARY_ACCELERATOR);
  hex_mod_mask |= GDK_SHIFT_MASK;

  if (priv->in_hex_sequence && priv->modifiers_dropped)
    have_hex_mods = TRUE;
  else
    have_hex_mods = (state & (hex_mod_mask)) == hex_mod_mask;
  is_hex_start = keyval == GDK_KEY_U;
  is_hex_end = (keyval == GDK_KEY_space ||
                keyval == GDK_KEY_KP_Space ||
                keyval == GDK_KEY_Return ||
                keyval == GDK_KEY_ISO_Enter ||
                keyval == GDK_KEY_KP_Enter);
  is_backspace = keyval == GDK_KEY_BackSpace;
  is_escape = keyval == GDK_KEY_Escape;
  hex_keyval = canonical_hex_keyval (event);

  /* If we are already in a non-hex sequence, or
   * this keystroke is not hex modifiers + hex digit, don't filter
   * key events with accelerator modifiers held down. We only treat
   * Control and Alt as accel modifiers here, since Super, Hyper and
   * Meta are often co-located with Mode_Switch, Multi_Key or
   * ISO_Level3_Switch.
   */
  if (!have_hex_mods ||
      (n_compose > 0 && !priv->in_hex_sequence) ||
      (n_compose == 0 && !priv->in_hex_sequence && !is_hex_start) ||
      (priv->in_hex_sequence && !hex_keyval &&
       !is_hex_start && !is_hex_end && !is_escape && !is_backspace))
    {
      GdkModifierType no_text_input_mask;

      no_text_input_mask = gdk_keymap_get_modifier_mask (keymap, GDK_MODIFIER_INTENT_NO_TEXT_INPUT);

      if (state & no_text_input_mask ||
	  (priv->in_hex_sequence && priv->modifiers_dropped &&
	   (keyval == GDK_KEY_Return ||
	    keyval == GDK_KEY_ISO_Enter ||
	    keyval == GDK_KEY_KP_Enter)))
	{
	  return FALSE;
	}
    }
  
  /* Handle backspace */
  if (priv->in_hex_sequence && have_hex_mods && is_backspace)
    {
      if (n_compose > 0)
	{
	  n_compose--;
	  priv->compose_buffer[n_compose] = 0;
          check_hex (context_simple, n_compose);
	}
      else
	{
	  priv->in_hex_sequence = FALSE;
	}

      g_signal_emit_by_name (context_simple, "preedit-changed");

      if (!priv->in_hex_sequence)
        g_signal_emit_by_name (context_simple, "preedit-end");
      
      return TRUE;
    }

  /* Check for hex sequence restart */
  if (priv->in_hex_sequence && have_hex_mods && is_hex_start)
    {
      if (priv->tentative_match &&
	  g_unichar_validate (priv->tentative_match))
	{
	  gtk_im_context_simple_commit_char (context, priv->tentative_match);
	  priv->compose_buffer[0] = 0;
	}
      else
	{
	  /* invalid hex sequence */
	  if (n_compose > 0)
	    beep_surface (surface);

	  priv->tentative_match = 0;
	  priv->in_hex_sequence = FALSE;
	  priv->compose_buffer[0] = 0;
	}
    }
  
  /* Check for hex sequence start */
  if (!priv->in_hex_sequence && have_hex_mods && is_hex_start)
    {
      priv->compose_buffer[0] = 0;
      priv->in_hex_sequence = TRUE;
      priv->modifiers_dropped = FALSE;
      priv->tentative_match = 0;

      g_signal_emit_by_name (context_simple, "preedit-start");
      g_signal_emit_by_name (context_simple, "preedit-changed");
  
      return TRUE;
    }
  
  /* Then, check for compose sequences */
  if (priv->in_hex_sequence)
    {
      if (hex_keyval)
	priv->compose_buffer[n_compose++] = hex_keyval;
      else if (is_escape)
	{
	  gtk_im_context_simple_reset (context);
	  return TRUE;
	}
      else if (!is_hex_end)
	{
	  /* non-hex character in hex sequence */
	  beep_surface (surface);
	  return TRUE;
	}
    }
  else
    priv->compose_buffer[n_compose++] = keyval;

  priv->compose_buffer[n_compose] = 0;

  if (priv->in_hex_sequence)
    {
      /* If the modifiers are still held down, consider the sequence again */
      if (have_hex_mods)
        {
          /* space or return ends the sequence, and we eat the key */
          if (n_compose > 0 && is_hex_end)
            {
	      if (priv->tentative_match &&
		  g_unichar_validate (priv->tentative_match))
		{
		  gtk_im_context_simple_commit_char (context, priv->tentative_match);
		  priv->compose_buffer[0] = 0;
		}
	      else
		{
		  /* invalid hex sequence */
		  beep_surface (surface);

		  priv->tentative_match = 0;
		  priv->in_hex_sequence = FALSE;
		  priv->compose_buffer[0] = 0;
		}
            }
          else if (!check_hex (context_simple, n_compose))
	    beep_surface (surface);

	  g_signal_emit_by_name (context_simple, "preedit-changed");

	  if (!priv->in_hex_sequence)
	    g_signal_emit_by_name (context_simple, "preedit-end");

	  return TRUE;
        }
    }
  else
    {
      gboolean success = FALSE;

#ifdef GDK_WINDOWING_WIN32
      if (GDK_IS_WIN32_DISPLAY (display))
        {
          guint16  output[2];
          gsize    output_size = 2;

          switch (gdk_win32_keymap_check_compose (GDK_WIN32_KEYMAP (keymap),
                                                  priv->compose_buffer,
                                                  n_compose,
                                                  output, &output_size))
            {
            case GDK_WIN32_KEYMAP_MATCH_NONE:
              break;
            case GDK_WIN32_KEYMAP_MATCH_EXACT:
            case GDK_WIN32_KEYMAP_MATCH_PARTIAL:
              for (i = 0; i < output_size; i++)
                {
                  output_char = gdk_keyval_to_unicode (output[i]);
                  gtk_im_context_simple_commit_char (GTK_IM_CONTEXT (context_simple),
                                                     output_char);
                }
              priv->compose_buffer[0] = 0;
              return TRUE;
            case GDK_WIN32_KEYMAP_MATCH_INCOMPLETE:
              return TRUE;
            default:
              g_assert_not_reached ();
              break;
            }
        }
#endif

      G_LOCK (global_tables);

      tmp_list = global_tables;
      while (tmp_list)
        {
          if (check_table (context_simple, tmp_list->data, n_compose))
            {
              success = TRUE;
              break;
            }
          tmp_list = tmp_list->next;
        }

      G_UNLOCK (global_tables);

      if (success)
        return TRUE;

#ifdef GDK_WINDOWING_WIN32
      if (check_win32_special_cases (context_simple, n_compose))
	return TRUE;
#endif

#ifdef GDK_WINDOWING_QUARTZ
      if (check_quartz_special_cases (context_simple, n_compose))
        return TRUE;
#endif

      if (gtk_check_compact_table (&gtk_compose_table_compact,
                                   priv->compose_buffer,
                                   n_compose, &compose_finish,
                                   &compose_match, &output_char))
        {
          if (compose_finish)
            {
              if (compose_match)
                {
                  gtk_im_context_simple_commit_char (GTK_IM_CONTEXT (context_simple),
                                                     output_char);
#ifdef G_OS_WIN32
                  check_win32_special_case_after_compact_match (context_simple,
                                                                n_compose,
                                                                output_char);
#endif
                  priv->compose_buffer[0] = 0;
                }
            }
          else
            {
              if (compose_match)
                {
                  priv->tentative_match = output_char;
                  priv->tentative_match_len = n_compose;
                }
              if (output_char)
                g_signal_emit_by_name (context_simple, "preedit-changed");
            }

          return TRUE;
        }
  
      if (gtk_check_algorithmically (priv->compose_buffer, n_compose, &output_char))
        {
          if (output_char)
            {
              gtk_im_context_simple_commit_char (GTK_IM_CONTEXT (context_simple),
                                                 output_char);
              priv->compose_buffer[0] = 0;
            }
	  return TRUE;
        }
    }
  
  /* The current compose_buffer doesn't match anything */
  return no_sequence_matches (context_simple, n_compose, event);
}

static void
gtk_im_context_simple_reset (GtkIMContext *context)
{
  GtkIMContextSimple *context_simple = GTK_IM_CONTEXT_SIMPLE (context);
  GtkIMContextSimplePrivate *priv = context_simple->priv;

  priv->compose_buffer[0] = 0;

  if (priv->tentative_match || priv->in_hex_sequence)
    {
      priv->in_hex_sequence = FALSE;
      priv->tentative_match = 0;
      priv->tentative_match_len = 0;
      g_signal_emit_by_name (context_simple, "preedit-changed");
      g_signal_emit_by_name (context_simple, "preedit-end");
    }
}

static void     
gtk_im_context_simple_get_preedit_string (GtkIMContext   *context,
					  gchar         **str,
					  PangoAttrList **attrs,
					  gint           *cursor_pos)
{
  GtkIMContextSimple *context_simple = GTK_IM_CONTEXT_SIMPLE (context);
  GtkIMContextSimplePrivate *priv = context_simple->priv;
  char outbuf[37]; /* up to 6 hex digits */
  int len = 0;

  if (priv->in_hex_sequence)
    {
      int hexchars = 0;
         
      outbuf[0] = 'u';
      len = 1;

      while (priv->compose_buffer[hexchars] != 0)
	{
	  len += g_unichar_to_utf8 (gdk_keyval_to_unicode (priv->compose_buffer[hexchars]),
				    outbuf + len);
	  ++hexchars;
	}

      g_assert (len < 25);
    }
  else if (priv->tentative_match)
    len = g_unichar_to_utf8 (priv->tentative_match, outbuf);

  outbuf[len] = '\0';

  if (str)
    *str = g_strdup (outbuf);

  if (attrs)
    {
      *attrs = pango_attr_list_new ();
      
      if (len)
	{
	  PangoAttribute *attr = pango_attr_underline_new (PANGO_UNDERLINE_SINGLE);
	  attr->start_index = 0;
          attr->end_index = len;
	  pango_attr_list_insert (*attrs, attr);
	}
    }

  if (cursor_pos)
    *cursor_pos = len;
}

static void
gtk_im_context_simple_set_client_widget  (GtkIMContext *context,
                                          GtkWidget    *widget)
{
  GtkIMContextSimple *im_context_simple = GTK_IM_CONTEXT_SIMPLE (context);
  gboolean run_compose_table = FALSE;

  if (!widget)
    return;

  /* Load compose table for X11 or Wayland. */
#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (gtk_widget_get_display (widget)))
    run_compose_table = TRUE;
#endif
#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gtk_widget_get_display (widget)))
    run_compose_table = TRUE;
#endif

  if (run_compose_table)
    init_compose_table_async (im_context_simple, NULL, NULL, NULL);
}

/**
 * gtk_im_context_simple_add_table: (skip)
 * @context_simple: A #GtkIMContextSimple
 * @data: (array): the table
 * @max_seq_len: Maximum length of a sequence in the table
 *               (cannot be greater than #GTK_MAX_COMPOSE_LEN)
 * @n_seqs: number of sequences in the table
 *
 * Adds an additional table to search to the input context.
 * Each row of the table consists of @max_seq_len key symbols
 * followed by two #guint16 interpreted as the high and low
 * words of a #gunicode value. Tables are searched starting
 * from the last added.
 *
 * The table must be sorted in dictionary order on the
 * numeric value of the key symbol fields. (Values beyond
 * the length of the sequence should be zero.)
 **/
void
gtk_im_context_simple_add_table (GtkIMContextSimple *context_simple,
				 guint16            *data,
				 gint                max_seq_len,
				 gint                n_seqs)
{
  g_return_if_fail (GTK_IS_IM_CONTEXT_SIMPLE (context_simple));

  G_LOCK (global_tables);

  global_tables = gtk_compose_table_list_add_array (global_tables,
                                                    data, max_seq_len, n_seqs);

  G_UNLOCK (global_tables);
}

/**
 * gtk_im_context_simple_add_compose_file:
 * @context_simple: A #GtkIMContextSimple
 * @compose_file: The path of compose file
 *
 * Adds an additional table from the X11 compose file.
 */
void
gtk_im_context_simple_add_compose_file (GtkIMContextSimple *context_simple,
                                        const gchar        *compose_file)
{
  g_return_if_fail (GTK_IS_IM_CONTEXT_SIMPLE (context_simple));

  G_LOCK (global_tables);

  global_tables = gtk_compose_table_list_add_file (global_tables,
                                                   compose_file);

  G_UNLOCK (global_tables);
}
