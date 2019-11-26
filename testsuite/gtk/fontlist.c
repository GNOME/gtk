/*
 * Copyright © 2019 Benjamin Otte
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

#include <locale.h>

#include <gtk/gtk.h>

static GQuark changes_quark;

#if PANGO_VERSION < G_ENCODE_VERSION (1, 46)
#define pango_font_face_get_family(_face) g_object_get_data (G_OBJECT (_face), "-gtk-font-family")
#endif

static char *
model_to_string (GListModel *model)
{
  GString *string = g_string_new (NULL);
  guint i;

  for (i = 0; i < g_list_model_get_n_items (model); i++)
    {
      PangoFontFace *face = g_list_model_get_item (model, i);
      if (i > 0)
        g_string_append (string, ", ");
      g_string_append_printf (string, "%s-%s",
                              pango_font_family_get_name (pango_font_face_get_family (face)),
                              pango_font_face_get_face_name (face));
      g_object_unref (face);
    }

  return g_string_free (string, FALSE);
}

#define assert_model(model1, model2) G_STMT_START{ \
  char *s1 = model_to_string (G_LIST_MODEL (model1)); \
  char *s2 = model_to_string (G_LIST_MODEL (model2)); \
  if (!g_str_equal (s1, s2)) \
     g_assertion_message_cmpstr (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
         #model1 " == " #model2, s1, "==", s2); \
  g_free (s1); \
  g_free (s2); \
}G_STMT_END

#define assert_changes(model, expected) G_STMT_START{ \
  GString *changes = g_object_get_qdata (G_OBJECT (model), changes_quark); \
  if (!g_str_equal (changes->str, expected)) \
     g_assertion_message_cmpstr (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
         #model " == " #expected, changes->str, "==", expected); \
  g_string_set_size (changes, 0); \
}G_STMT_END

#define assert_no_changes(model) assert_changes(model, "")
#define assert_some_changes(model) G_STMT_START { \
  GString *changes = g_object_get_qdata (G_OBJECT (model), changes_quark); \
  if (changes->len == 0) \
     g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
         "changes to model were expected, but none happened."); \
  g_string_set_size (changes, 0); \
}G_STMT_END

static void
items_changed (GListModel *model,
               guint       position,
               guint       removed,
               guint       added,
               GString    *changes)
{
  g_assert (removed != 0 || added != 0);

  if (changes->len)
    g_string_append (changes, ", ");

  if (removed == 1 && added == 0)
    {
      g_string_append_printf (changes, "-%u", position);
    }
  else if (removed == 0 && added == 1)
    {
      g_string_append_printf (changes, "+%u", position);
    }
  else
    {
      g_string_append_printf (changes, "%u", position);
      if (removed > 0)
        g_string_append_printf (changes, "-%u", removed);
      if (added > 0)
        g_string_append_printf (changes, "+%u", added);
    }
}

static void
free_changes (gpointer data)
{
  GString *changes = data;

  /* all changes must have been checked via assert_changes() before */
  g_assert_cmpstr (changes->str, ==, "");

  g_string_free (changes, TRUE);
}

static GtkFontList *
new_font_list (void)
{
  GtkFontList *result;
  GString *changes;

  result = gtk_font_list_new ();
  changes = g_string_new ("");
  g_object_set_qdata_full (G_OBJECT(result), changes_quark, changes, free_changes);
  g_signal_connect (result, "items-changed", G_CALLBACK (items_changed), changes);

  return result;
}

static void
test_create (void)
{
  GtkFontList *list;
  
  list = new_font_list ();
  assert_changes (list, "");
  g_object_unref (list);
}

static void
test_set_display (void)
{
  GtkFontList *list;
  
  list = new_font_list ();
  assert_changes (list, "");
  gtk_font_list_set_display (list, gdk_display_get_default ());
  assert_changes (list, "");
  gtk_font_list_set_display (list, NULL);
  assert_changes (list, "");
  g_object_unref (list);
}

static void
test_set_font_map (void)
{
  GtkFontList *list1, *list2;
  
  list1 = new_font_list ();
  assert_changes (list1, "");
  list2 = new_font_list ();
  gtk_font_list_set_font_map (list2, pango_cairo_font_map_get_default ());
  assert_changes (list2, "");
  assert_model (list1, list2);
  g_object_unref (list2);
  g_object_unref (list1);
}

static void
test_set_families_only (void)
{
  GtkFontList *list1, *list2;
  
  list1 = new_font_list ();
  assert_changes (list1, "");
  list2 = new_font_list ();
  gtk_font_list_set_families_only (list2, TRUE);
  assert_some_changes (list2);
  gtk_font_list_set_families_only (list2, FALSE);
  assert_some_changes (list2);
  assert_model (list1, list2);
  g_object_unref (list2);
  g_object_unref (list1);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  changes_quark = g_quark_from_static_string ("What did I see? Can I believe what I saw?");

  g_test_add_func ("/fontlistmodel/create", test_create);
  g_test_add_func ("/fontlistmodel/set-display", test_set_display);
  g_test_add_func ("/fontlistmodel/set-font-map", test_set_font_map);
  g_test_add_func ("/fontlistmodel/set-families-only", test_set_families_only);

  return g_test_run ();
}
