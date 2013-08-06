/*
 * Copyright (C) 2011 Red Hat Inc.
 *
 * Author:
 *      Benjamin Otte <otte@redhat.com>
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

#include "config.h"

#include <string.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#define DEPTH_INCREMENT 2

static char *
get_test_file (const char *test_file,
               const char *extension,
               gboolean    must_exist)
{
  GString *file = g_string_new (NULL);

  if (g_str_has_suffix (test_file, ".ui"))
    g_string_append_len (file, test_file, strlen (test_file) - strlen (".ui"));
  else
    g_string_append (file, test_file);
  
  g_string_append (file, extension);

  if (must_exist &&
      !g_file_test (file->str, G_FILE_TEST_EXISTS))
    {
      g_string_free (file, TRUE);
      return NULL;
    }

  return g_string_free (file, FALSE);
}

static char *
diff_with_file (const char  *file1,
                char        *text,
                gssize       len,
                GError     **error)
{
  const char *command[] = { "diff", "-u", file1, NULL, NULL };
  char *diff, *tmpfile;
  int fd;

  diff = NULL;

  if (len < 0)
    len = strlen (text);
  
  /* write the text buffer to a temporary file */
  fd = g_file_open_tmp (NULL, &tmpfile, error);
  if (fd < 0)
    return NULL;

  if (write (fd, text, len) != (int) len)
    {
      close (fd);
      g_set_error (error,
                   G_FILE_ERROR, G_FILE_ERROR_FAILED,
                   "Could not write data to temporary file '%s'", tmpfile);
      goto done;
    }
  close (fd);
  command[3] = tmpfile;

  /* run diff command */
  g_spawn_sync (NULL,
                (char **) command,
                NULL,
                G_SPAWN_SEARCH_PATH,
                NULL, NULL,
	        &diff,
                NULL, NULL,
                error);

done:
  g_unlink (tmpfile);
  g_free (tmpfile);

  return diff;
}

static int unnamed_object_count;

static void
setup_test (void)
{
  unnamed_object_count = 0;
}

static const char *
get_name (AtkObject *accessible)
{
  char *name;

  name = g_object_get_data (G_OBJECT (accessible), "gtk-accessibility-dump-name");
  if (name)
    return name;

  if (GTK_IS_ACCESSIBLE (accessible))
    {
      GtkWidget *widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));

      name = g_strdup (gtk_buildable_get_name (GTK_BUILDABLE (widget)));
    }

  if (name == NULL && ATK_IS_TEXT (accessible))
    {
      name = atk_text_get_text (ATK_TEXT (accessible), 0, -1);
    }

  if (name == NULL)
    {
      /* Generate a unique, repeatable name */
      name = g_strdup_printf ("unnamed-%s-%d", G_OBJECT_TYPE_NAME (accessible), unnamed_object_count++);
    }

  g_object_set_data_full (G_OBJECT (accessible), "gtk-accessibility-dump-name", name, g_free);
  return name;
}

static void
dump_relation (GString     *string,
               guint        depth,
               AtkRelation *relation)
{
  GPtrArray *targets;
  const char *name;
  guint i;

  targets = atk_relation_get_target (relation);
  if (targets == NULL || targets->len == 0)
    return;

  name = atk_relation_type_get_name (atk_relation_get_relation_type (relation));
  g_string_append_printf (string, "%*s%s: %s\n", depth, "", name, get_name (g_ptr_array_index (targets, 0)));
  depth += strlen (name) + 2;

  for (i = 1; i < targets->len; i++)
    {
      g_string_append_printf (string, "%*s%s\n", depth, "", get_name (g_ptr_array_index (targets, i)));
    }
}

static void
dump_relation_set (GString        *string,
                   guint           depth,
                   AtkRelationSet *set)
{
  guint i;

  if (set == NULL)
    return;

  for (i = 0; i < atk_relation_set_get_n_relations (set); i++)
    {
      AtkRelation *relation;
      
      relation = atk_relation_set_get_relation (set, i);

      dump_relation (string, depth, relation);
    }

  g_object_unref (set);
}

static void
dump_state_set (GString     *string,
                guint        depth,
                AtkStateSet *set)
{
  guint i;

  if (set == NULL)
    return;

  if (!atk_state_set_is_empty (set))
    {
      g_string_append_printf (string, "%*sstate:", depth, "");
      for (i = 0; i < ATK_STATE_LAST_DEFINED; i++)
        {
          if (atk_state_set_contains_state (set, i))
            g_string_append_printf (string, " %s", atk_state_type_get_name (i));
        }
      g_string_append_c (string, '\n');
    }

  g_object_unref (set);
}

static void
dump_attribute (GString      *string,
                guint         depth,
                AtkAttribute *attribute)
{
  g_string_append_printf (string, "%*s%s: %s\n", depth, "", attribute->name, attribute->value);
}

static void
dump_attribute_set (GString         *string,
                    guint            depth,
                    AtkAttributeSet *set)
{
  GSList *l;
  AtkAttribute *attribute;

  for (l = set; l; l = l->next)
    {
      attribute = l->data;

      dump_attribute (string, depth, attribute);
    }
}

static gint
compare_attr (gconstpointer a, gconstpointer b)
{
  const AtkAttribute *aattr = a;
  const AtkAttribute *battr = b;

  return strcmp (aattr->name, battr->name);
}

static void
dump_text_attributes (GString         *string,
                      gint             depth,
                      const gchar     *name,
                      AtkAttributeSet *attributes)
{
  GSList *l;
  AtkAttribute *attr;
  const gchar *value;

  if (attributes == NULL)
    return;

  attributes = g_slist_sort (attributes, compare_attr);

  for (l = attributes; l; l = l->next)
    {
      attr = l->data;
      /* don't dump values that depend on the environment or too
       * closely on the exact theme pixels.
       */
      if (strcmp (attr->name, "family-name") == 0 ||
          strcmp (attr->name, "size") == 0 ||
          strcmp (attr->name, "weight") == 0 ||
          strcmp (attr->name, "stretch") == 0 ||
          strcmp (attr->name, "variant") == 0 ||
          strcmp (attr->name, "style") == 0 ||
          strcmp (attr->name, "language") == 0 ||
          strcmp (attr->name, "fg-color") == 0 ||
          strcmp (attr->name, "bg-color") == 0 ||
          strcmp (attr->name, "direction") == 0)
        value = "<omitted>";
      else
        value = attr->value;

      if (name)
        {
          /* first time this loop is run */
          g_string_append_printf (string, "%*s%s: %s: %s\n", depth, "", name, attr->name, value);
          depth += strlen (name) + 2;
          name = NULL;
        }
      else
        {
          /* every other time */
          g_string_append_printf (string, "%*s%s: %s\n", depth, "", attr->name, value);
        }
    }

  atk_attribute_set_free (attributes);
}

extern GType atk_layer_get_type (void);

static const gchar *
layer_name (AtkLayer layer)
{
  GEnumClass *class;
  GEnumValue *value;

  class = g_type_class_ref (atk_layer_get_type ());
  value = g_enum_get_value (class, layer);
  g_type_class_unref (class);

  return value->value_nick;
}

static void
dump_atk_component (AtkComponent *atk_component,
                    guint         depth,
                    GString      *string)
{
  AtkLayer layer;

  g_string_append_printf (string, "%*s<AtkComponent>\n", depth, "");

  layer = atk_component_get_layer (atk_component);
  g_string_append_printf (string, "%*slayer: %s\n", depth, "", layer_name (layer));

  g_string_append_printf (string, "%*salpha: %g\n", depth, "", atk_component_get_alpha (atk_component));
}

static void
dump_atk_text (AtkText *atk_text,
               guint    depth,
               GString *string)
{
  gchar *text;
  gint i, start, end;

  g_string_append_printf (string, "%*s<AtkText>\n", depth, "");

  text = atk_text_get_text (atk_text, 0, -1);
  g_string_append_printf (string, "%*stext: %s\n", depth, "", text);
  g_free (text);

  g_string_append_printf (string, "%*scharacter count: %d\n", depth, "", atk_text_get_character_count (atk_text));

  g_string_append_printf (string, "%*scaret offset: %d\n", depth, "", atk_text_get_caret_offset (atk_text));

  for (i = 0; i < atk_text_get_n_selections (atk_text); i++)
    {
      text = atk_text_get_selection (atk_text, i, &start, &end);
      if (text)
        g_string_append_printf (string, "%*sselection %d: (%d, %d) %s\n", depth, "", i, start, end, text);
      g_free (text);
    }

  dump_text_attributes (string, depth, "default attributes", atk_text_get_default_attributes (atk_text));
}

static void
dump_atk_image (AtkImage *atk_image,
                guint     depth,
                GString  *string)
{
  gint width, height;

  g_string_append_printf (string, "%*s<AtkImage>\n", depth, "");

  atk_image_get_image_size (atk_image, &width, &height);
  g_string_append_printf (string, "%*simage size: %d x %d\n", depth, "", width, height);

  g_string_append_printf (string, "%*simage description: %s\n", depth, "", atk_image_get_image_description (atk_image));
}

static void
dump_atk_action (AtkAction *atk_action,
                 guint      depth,
                 GString   *string)
{
  gint i;

  g_string_append_printf (string, "%*s<AtkAction>\n", depth, "");

  for (i = 0; i < atk_action_get_n_actions (atk_action); i++)
    {
      if (atk_action_get_name (atk_action, i))
        g_string_append_printf (string, "%*saction %d name: %s\n", depth, "", i, atk_action_get_name (atk_action, i));
      if (atk_action_get_description (atk_action, i))
        g_string_append_printf (string, "%*saction %d description: %s\n", depth, "", i, atk_action_get_description (atk_action, i));
      if (atk_action_get_keybinding (atk_action, i))
        g_string_append_printf (string, "%*saction %d keybinding: %s\n", depth, "", i, atk_action_get_keybinding (atk_action, i));
    }
}

static void
dump_atk_selection (AtkSelection *atk_selection,
                    guint         depth,
                    GString      *string)
{
  guint n_selections, n_counted_selections;
  gint i;

  g_string_append_printf (string, "%*s<AtkSelection>\n", depth, "");

  n_selections = atk_selection_get_selection_count (atk_selection);

  n_counted_selections = 0;
  for (i = 0; i < atk_object_get_n_accessible_children (ATK_OBJECT (atk_selection)); i++)
    {
      if (atk_selection_is_child_selected (atk_selection, i))
        {
          AtkObject *object = atk_object_ref_accessible_child (ATK_OBJECT (atk_selection), i);
          
          g_assert (object);
          
          if (n_counted_selections == 0)
            {
              g_string_append_printf (string, "%*sselected children: %s\n", depth, "", get_name (object));
              depth += strlen ("selected children: ");
            }
          else
            g_string_append_printf (string, "%*s%s\n", depth, "", get_name (object));
          n_counted_selections++;
        }
    }
  
  g_assert_cmpint (n_selections, ==, n_counted_selections);
  g_assert_cmpint (n_selections, ==, atk_selection_get_selection_count (atk_selection));
}

static void
dump_atk_value (AtkValue *atk_value,
                guint     depth,
                GString  *string)
{
  GValue value = G_VALUE_INIT;
  GValue svalue = G_VALUE_INIT;

  g_string_append_printf (string, "%*s<AtkValue>\n", depth, "");

  g_value_init (&value, G_TYPE_DOUBLE);
  g_value_init (&svalue, G_TYPE_STRING);

  atk_value_get_minimum_value (atk_value, &value);
  if (g_value_transform (&value, &svalue))
    g_string_append_printf (string, "%*sminimum value: %s\n", depth, "", g_value_get_string (&svalue));
  else
    g_string_append_printf (string, "%*sminimum value: <%s>\n", depth, "", G_VALUE_TYPE_NAME (&value));

  g_value_reset (&value);
  g_value_reset (&svalue);

  atk_value_get_maximum_value (atk_value, &value);
  if (g_value_transform (&value, &svalue))
    g_string_append_printf (string, "%*smaximum value: %s\n", depth, "", g_value_get_string (&svalue));
  else
    g_string_append_printf (string, "%*smaximum value: <%s>\n", depth, "", G_VALUE_TYPE_NAME (&value));

  g_value_reset (&value);
  g_value_reset (&svalue);

  atk_value_get_current_value (atk_value, &value);
  if (g_value_transform (&value, &svalue))
    g_string_append_printf (string, "%*scurrent value: %s\n", depth, "", g_value_get_string (&svalue));
  else
    g_string_append_printf (string, "%*scurrent value: %s\n", depth, "", G_VALUE_TYPE_NAME (&value));

  g_value_reset (&value);
  g_value_reset (&svalue);

  /* Don't dump minimum increment; it changes too much in response to
   * theme changes.
   * https://bugzilla.gnome.org/show_bug.cgi?id=704747
   */
}

static void
dump_atk_hyperlink_impl (AtkHyperlinkImpl *impl,
                         guint             depth,
                         GString          *string)
{
  AtkHyperlink *atk_link;
  gint i;

  g_string_append_printf (string, "%*s<AtkHyperlinkImpl>\n", depth, "");

  atk_link = atk_hyperlink_impl_get_hyperlink (impl);

  g_string_append_printf (string, "%*sanchors:", depth, "");

  for (i = 0; i < atk_hyperlink_get_n_anchors (atk_link); i++)
    {
      gchar *uri;

      uri = atk_hyperlink_get_uri (atk_link, i);
      g_string_append_printf (string, " %s", uri);
      g_free (uri);
    }
  g_string_append_c (string, '\n');

  g_object_unref (atk_link);
}

static void
dump_atk_streamable_content (AtkStreamableContent *content,
                             guint                 depth,
                             GString              *string)
{
  gint i;

  g_string_append_printf (string, "%*s<AtkStreamableContent>\n", depth, "");

  g_string_append_printf (string, "%*smime types:", depth, "");
  for (i = 0; i < atk_streamable_content_get_n_mime_types (content); i++)
    g_string_append_printf (string, " %s", atk_streamable_content_get_mime_type (content, i));
  g_string_append_c (string, '\n');
}

static void
dump_atk_table (AtkTable *table,
                guint     depth,
                GString  *string)
{
  gint *selected;
  gint n_selected;
  gint i, j;
  AtkObject *obj;
  const gchar *desc;

  g_string_append_printf (string, "%*s<AtkTable>\n", depth, "");

  obj = atk_table_get_summary (table);
  if (obj)
    g_string_append_printf (string, "%*ssummary: %s\n", depth, "", get_name (obj));

  obj = atk_table_get_caption (table);
  if (obj)
    g_string_append_printf (string, "%*scaption: %s\n", depth, "", get_name (obj));

  g_string_append_printf (string, "%*srows: %d\n", depth, "", atk_table_get_n_rows (table));
  g_string_append_printf (string, "%*scolumns: %d\n", depth, "", atk_table_get_n_columns (table));

  selected = NULL;
  n_selected = atk_table_get_selected_rows (table, &selected);
  if (n_selected > 0)
    {
      g_string_append_printf (string, "%*sselected rows:", depth, "");
      for (i = 0; i < n_selected; i++)
        g_string_append_printf (string, " %d", selected[i]);
      g_string_append_c (string, '\n');
    }
  g_free (selected);

  selected = NULL;
  n_selected = atk_table_get_selected_columns (table, &selected);
  if (n_selected > 0)
    {
      g_string_append_printf (string, "%*sselected columns:", depth, "");
      for (i = 0; i < n_selected; i++)
        g_string_append_printf (string, " %d", selected[i]);
      g_string_append_c (string, '\n');
    }
  g_free (selected);

  
  for (i = 0; i < atk_table_get_n_columns (table); i++)
    {
      desc = atk_table_get_column_description (table, i);
      if (desc)
        g_string_append_printf (string, "%*scolumn %d description: %s\n", depth, "", i, desc);
      obj = atk_table_get_column_header (table, i);
      if (obj)
        g_string_append_printf (string, "%*scolumn %d header: %s\n", depth, "", i, get_name (obj));
    }

  for (i = 0; i < atk_table_get_n_rows (table); i++)
    {
      desc = atk_table_get_row_description (table, i);
      if (desc)
        g_string_append_printf (string, "%*srow %d description: %s\n", depth, "", i, desc);
      obj = atk_table_get_row_header (table, i);
      if (obj)
        g_string_append_printf (string, "%*srow %d header: %s\n", depth, "", i, get_name (obj));
    }

  g_string_append_printf (string, "%*stable indexes:\n", depth, "");
  for (i = 0; i < atk_table_get_n_rows (table); i++)
    {
      g_string_append_printf (string, "%*s", depth + DEPTH_INCREMENT, "");
      for (j = 0; j < atk_table_get_n_columns (table); j++)
        {
          int id = atk_table_get_index_at (table, i, j);

          obj = atk_object_ref_accessible_child (ATK_OBJECT (table), id);
          if (j > 0)
            g_string_append (string, " ");

          g_string_append_printf (string, "%s%s%s",
                                  atk_table_get_row_at_index (table, id) == i ? "✓" : "⚠",
                                  atk_table_get_column_at_index (table, id) == j ? "✓" : "⚠",
                                  get_name (obj));
        }
      g_string_append (string, "\n");
    }
}

static void
dump_accessible (AtkObject     *accessible,
                 guint          depth,
                 GString       *string)
{
  guint i;

  g_string_append_printf (string, "%*s%s\n", depth, "", get_name (accessible));
  depth += DEPTH_INCREMENT;

  g_string_append_printf (string, "%*s\"%s\"\n", depth, "", atk_role_get_name (atk_object_get_role (accessible)));
  if (GTK_IS_ACCESSIBLE (atk_object_get_parent (accessible)))
    g_string_append_printf (string, "%*sparent: %s\n", depth, "", get_name (atk_object_get_parent (accessible)));
  if (atk_object_get_index_in_parent (accessible) != -1)
    g_string_append_printf (string, "%*sindex: %d\n", depth, "", atk_object_get_index_in_parent (accessible));
  if (atk_object_get_name (accessible))
    g_string_append_printf (string, "%*sname: %s\n", depth, "", atk_object_get_name (accessible));
  if (atk_object_get_description (accessible))
    g_string_append_printf (string, "%*sdescription: %s\n", depth, "", atk_object_get_description (accessible));
  dump_relation_set (string, depth, atk_object_ref_relation_set (accessible));
  dump_state_set (string, depth, atk_object_ref_state_set (accessible));
  dump_attribute_set (string, depth, atk_object_get_attributes (accessible));

  if (ATK_IS_COMPONENT (accessible))
    dump_atk_component (ATK_COMPONENT (accessible), depth, string);

  if (ATK_IS_TEXT (accessible))
    dump_atk_text (ATK_TEXT (accessible), depth, string);

  if (ATK_IS_IMAGE (accessible))
    dump_atk_image (ATK_IMAGE (accessible), depth, string);

  if (ATK_IS_ACTION (accessible))
    dump_atk_action (ATK_ACTION (accessible), depth, string);

  if (ATK_IS_SELECTION (accessible))
    dump_atk_selection (ATK_SELECTION (accessible), depth, string);

  if (ATK_IS_VALUE (accessible))
    dump_atk_value (ATK_VALUE (accessible), depth, string);

  if (ATK_IS_HYPERLINK_IMPL (accessible))
    dump_atk_hyperlink_impl (ATK_HYPERLINK_IMPL (accessible), depth, string);

  if (ATK_IS_STREAMABLE_CONTENT (accessible))
    dump_atk_streamable_content (ATK_STREAMABLE_CONTENT (accessible), depth, string);

  if (ATK_IS_TABLE (accessible))
    dump_atk_table (ATK_TABLE (accessible), depth, string);

  for (i = 0; i < atk_object_get_n_accessible_children (accessible); i++)
    {
      AtkObject *child = atk_object_ref_accessible_child (accessible, i);
      dump_accessible (child, depth, string);
      g_object_unref (child);
    }
}

static GtkWidget *
builder_get_toplevel (GtkBuilder *builder)
{
  GSList *list, *walk;
  GtkWidget *window = NULL;

  list = gtk_builder_get_objects (builder);
  for (walk = list; walk; walk = walk->next)
    {
      if (GTK_IS_WINDOW (walk->data) &&
          gtk_widget_get_parent (walk->data) == NULL)
        {
          window = walk->data;
          break;
        }
    }
  
  g_slist_free (list);

  return window;
}

static void
dump_ui_file (const char *ui_file,
              GString *string)
{
  GtkWidget *window;
  GtkBuilder *builder;
  GError *error = NULL;

  builder = gtk_builder_new ();
  gtk_builder_add_from_file (builder, ui_file, &error);
  g_assert_no_error (error);
  window = builder_get_toplevel (builder);
  g_object_unref (builder);
  g_assert (window);

  gtk_widget_show (window);

  dump_accessible (gtk_widget_get_accessible (window), 0, string);
  gtk_widget_destroy (window);
}

static void
dump_to_stdout (GFile *file)
{
  char *ui_file;
  GString *dump;

  ui_file = g_file_get_path (file);
  dump = g_string_new ("");

  dump_ui_file (ui_file, dump);
  g_print ("%s", dump->str);

  g_string_free (dump, TRUE);
  g_free (ui_file);
}

static void
test_ui_file (GFile *file)
{
  char *ui_file, *a11y_file;
  GString *dump;
  GError *error = NULL;

  ui_file = g_file_get_path (file);
  a11y_file = get_test_file (ui_file, ".txt", TRUE);
  dump = g_string_new ("");

  dump_ui_file (ui_file, dump);

  if (a11y_file)
    {
      char *diff = diff_with_file (a11y_file, dump->str, dump->len, &error);
      g_assert_no_error (error);

      if (diff && diff[0])
        {
          g_printerr ("Contents don't match expected contents:\n%s", diff);
          g_test_fail ();
          g_free (diff);
        }
    }
  else if (dump->str[0])
    {
      g_test_message ("Expected a reference file:\n%s", dump->str);
      g_test_fail ();
    }

  g_string_free (dump, TRUE);
  g_free (a11y_file);
  g_free (ui_file);
}

static void
add_test_for_file (GFile *file)
{
  g_test_add_vtable (g_file_get_path (file),
                     0,
                     g_object_ref (file),
                     (GTestFixtureFunc) setup_test,
                     (GTestFixtureFunc) test_ui_file,
                     (GTestFixtureFunc) g_object_unref);
}

static int
compare_files (gconstpointer a, gconstpointer b)
{
  GFile *file1 = G_FILE (a);
  GFile *file2 = G_FILE (b);
  char *path1, *path2;
  int result;

  path1 = g_file_get_path (file1);
  path2 = g_file_get_path (file2);

  result = strcmp (path1, path2);

  g_free (path1);
  g_free (path2);

  return result;
}

static void
add_tests_for_files_in_directory (GFile *dir)
{
  GFileEnumerator *enumerator;
  GFileInfo *info;
  GList *files;
  GError *error = NULL;

  enumerator = g_file_enumerate_children (dir, G_FILE_ATTRIBUTE_STANDARD_NAME, 0, NULL, &error);
  g_assert_no_error (error);
  files = NULL;

  while ((info = g_file_enumerator_next_file (enumerator, NULL, &error)))
    {
      const char *filename;

      filename = g_file_info_get_name (info);

      if (!g_str_has_suffix (filename, ".ui"))
        {
          g_object_unref (info);
          continue;
        }

      files = g_list_prepend (files, g_file_get_child (dir, filename));

      g_object_unref (info);
    }
  
  g_assert_no_error (error);
  g_object_unref (enumerator);

  files = g_list_sort (files, compare_files);
  g_list_foreach (files, (GFunc) add_test_for_file, NULL);
  g_list_free_full (files, g_object_unref);
}

static char *arg_base_dir = NULL;

static const GOptionEntry test_args[] = {
  { "directory",        'd', 0, G_OPTION_ARG_FILENAME, &arg_base_dir,
    "Directory to run tests from", "DIR" },
  { NULL }
};

static gboolean
parse_command_line (int *argc, char ***argv)
{
  GError *error = NULL;
  GOptionContext *context;

  context = g_option_context_new ("- run GTK accessibility tests");
  g_option_context_add_main_entries (context, test_args, NULL);
  g_option_context_set_ignore_unknown_options (context, TRUE);

  if (!g_option_context_parse (context, argc, argv, &error))
    {
      g_print ("option parsing failed: %s\n", error->message);
      return FALSE;
    }

  gtk_test_init (argc, argv);

  return TRUE;
}

int
main (int argc, char **argv)
{
  if (!parse_command_line (&argc, &argv))
    return 1;

  if (argc < 2)
    {
      const char *basedir;
      GFile *dir;

      if (arg_base_dir)
        basedir = arg_base_dir;
      else
        basedir = g_test_get_dir (G_TEST_DIST);
      dir = g_file_new_for_path (basedir);
      add_tests_for_files_in_directory (dir);

      g_object_unref (dir);
    }
  else if (argc == 3 && strcmp (argv[1], "--generate") == 0)
    {
      GFile *file = g_file_new_for_commandline_arg (argv[2]);

      dump_to_stdout (file);

      g_object_unref (file);

      return 0;
    }
  else
    {
      guint i;

      for (i = 1; i < argc; i++)
        {
          GFile *file = g_file_new_for_commandline_arg (argv[i]);

          add_test_for_file (file);

          g_object_unref (file);
        }
    }

  return g_test_run ();
}

