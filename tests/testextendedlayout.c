/* testextendedlayout.c
 * Copyright (C) 2007 Mathias Hasselmann <mathias.hasselmann@gmx.de>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <gtk/gtk.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef GDK_WINDOWING_X11
#include "x11/gdkx.h"
#endif

#define IS_VALID_BASELINE(Baseline) ((Baseline) >= 0)

typedef enum _GuideType GuideType;
typedef enum _TestResult TestResult;

typedef struct _Guide Guide;

typedef struct _TestCase TestCase;
typedef struct _TestSuite TestSuite;

enum _GuideFlags
{
  GUIDE_FLAGS_HORIZONTAL = (1 << 0),
  GUIDE_FLAGS_VERTICAL = (1 << 1)
};

enum _GuideType
{
  GUIDE_BASELINE,

  GUIDE_INTERIOUR_VERTICAL,
  GUIDE_INTERIOUR_HORIZONTAL,
  GUIDE_INTERIOUR_BOTH,

  GUIDE_EXTERIOUR_VERTICAL,
  GUIDE_EXTERIOUR_HORIZONTAL,
  GUIDE_EXTERIOUR_BOTH
};

enum _TestResult
{
  TEST_RESULT_NONE,
  TEST_RESULT_SUCCESS,
  TEST_RESULT_FAILURE
};

enum
{
  RESULT_COLUMN_MESSAGE,
  RESULT_COLUMN_WEIGHT,
  RESULT_COLUMN_ICON,
  RESULT_COLUMN_RESULT,
  RESULT_COLUNN_COUNT
};

enum
{
  TEST_COLUMN_LABEL,
  TEST_COLUMN_SELECTED,
  TEST_COLUMN_TEST_CASE,
  TEST_COLUMN_HAS_TEST_CASE,
  TEST_COLUMN_PAGE_INDEX,
  TEST_COLUMN_COUNT
};

struct _Guide
{
  GtkWidget *widget;
  GuideType type;
  gint group;
};

struct _TestCase
{
  TestSuite *suite;
  const gchar *name;
  const gchar *detail;
  GtkWidget *widget;
  GList *guides;
  guint idle;
};

struct _TestSuite
{
  GtkTreeSelection *selection;
  GtkWidget *test_current_button;
  GtkListStore *tests;

  GtkWidget *window;
  GtkWidget *notebook;
  GtkWidget *baselines;
  GtkWidget *interiour;
  GtkWidget *exteriour;
  GtkWidget *statusbar;

  GtkTreeStore *results;
  GtkWidget *results_view;
  gint n_test_cases;
  gint level;

  GdkPixmap *tile;
  GtkWidget *current;
  GtkWidget *hover;
  gint timestamp;

  GtkTreeIter parent;
};

static const gchar lorem_ipsum[] =
  "<span weight=\"bold\" size=\"xx-large\">"
  "Lorem ipsum</span> dolor sit amet, consectetuer "
  "adipiscing elit. Aliquam sed erat. Proin lectus "
  "orci, venenatis pharetra, egestas id, tincidunt "
  "vel, eros. Integer fringilla. Aenean justo ipsum, "        
  "luctus ut, volutpat laoreet, vehicula in, libero.";

const gchar *captions[] =
  { 
    "<span size='xx-small'>xx-Small</span>",
    "<span weight='bold'>Bold</span>",
    "<span size='large'>Large</span>",
    "<span size='xx-large'>xx-Large</span>",
    NULL
  };

static char * mask_xpm[] = 
  {
    "20 20 2 1",
    " 	c #000000",
    "#	c #FFFFFF",
    " # # # # # # # # # #",
    "# # # # # # # # # # ",
    " # # # # # # # # # #",
    "# # # # # # # # # # ",
    " # # # # # # # # # #",
    "# # # # # # # # # # ",
    " # # # # # # # # # #",
    "# # # # # # # # # # ",
    " # # # # # # # # # #",
    "# # # # # # # # # # ",
    " # # # # # # # # # #",
    "# # # # # # # # # # ",
    " # # # # # # # # # #",
    "# # # # # # # # # # ",
    " # # # # # # # # # #",
    "# # # # # # # # # # ",
    " # # # # # # # # # #",
    "# # # # # # # # # # ",
    " # # # # # # # # # #",
    "# # # # # # # # # # "
  };

static gint8 dashes[] = { 1, 5 };

static void
set_widget_name (GtkWidget   *widget,
                 const gchar *format,
                 ...)
{
  gchar *name, *dash;
  va_list args;

  va_start (args, format);
  name = g_strdup_vprintf (format, args);
  va_end (args);

  for(dash = name; NULL != (dash = strchr (dash, ' ')); )
    *dash = '-';

  gtk_widget_set_name (widget, name);
  g_free (name);
}

static Guide*
guide_new (GtkWidget   *widget,
           GuideType    type,
           gint         group)
{
  Guide* self = g_new0 (Guide, 1);

  self->widget = widget;
  self->type = type;
  self->group = group;

  return self;
}

static TestCase*
test_case_new (TestSuite   *suite,
               const gchar *name,
               const gchar *detail,
               GtkWidget   *widget)
{
  TestCase* self = g_new0 (TestCase, 1);

  self->suite = suite;
  self->name = name;
  self->detail = detail;
  self->widget = widget;

  return self;
}

static void
update_status (TestSuite *suite,
               GtkWidget *child)
{
  const gchar *widget_name = gtk_widget_get_name (child);
  const gchar *type_name = G_OBJECT_TYPE_NAME (child);
  GString *status = g_string_new (type_name);

  if (strcmp (widget_name, type_name))
    g_string_append_printf (status, " (%s)", widget_name);

  g_string_append_printf (status,
                          "@%p:\nposition=%dx%d; size=%dx%d; requisition=%dx%d",
                          child,
                          child->allocation.x,
                          child->allocation.y,
                          child->allocation.width,
                          child->allocation.height,
                          child->requisition.width,
                          child->requisition.height);

  if (GTK_IS_EXTENDED_LAYOUT (child))
    {
      GtkExtendedLayout *layout = (GtkExtendedLayout*) child;
      GtkRequisition min_size, nat_size;
      gint min_height, nat_height;
      gint min_width, nat_width;

      gtk_extended_layout_get_desired_size (layout, FALSE, &min_size, &nat_size);
      g_string_append_printf (status, "; minimal-size: %dx%d, natural-size: %dx%d",
                              min_size.width, min_size.height,
                              nat_size.width, nat_size.height);

      gtk_extended_layout_get_height_for_width (layout,
                                                child->allocation.width,
                                                &min_height, &nat_height);
      g_string_append_printf (status, "; height-for-%d: minimal: %d, natural: %d",
                              child->allocation.width, min_height, nat_height);

      gtk_extended_layout_get_width_for_height (layout,
                                                child->allocation.height,
                                                &min_width, &nat_width);
      g_string_append_printf (status, "; width-for-%d: minimal: %d, natural: %d",
                              child->allocation.height, min_width, nat_width);

    }

  gtk_label_set_text (GTK_LABEL (suite->statusbar), status->str);
  g_string_free (status, TRUE);
}

static void
item_activate_cb (GtkWidget *item,
                  gpointer   data)
{
  GtkWidget *widget = data;
  TestCase *test;

  test = g_object_get_data (G_OBJECT (widget), "test-case");
  update_status (test->suite, widget);
  test->suite->current = widget;

  gtk_widget_queue_draw (test->widget);
}

static void
test_case_append_guide (TestCase  *self,
                        GtkWidget *widget,
                        GuideType  type,
                        gint       group)
{
  const gchar *widget_name;
  const gchar *type_name;
  gchar *item_label;
  GtkWidget *popup;
  GtkWidget *item;
  Guide *guide;

  guide = guide_new (widget, type, group);
  self->guides = g_list_append (self->guides, guide);
  g_object_set_data (G_OBJECT (widget), "test-case", self);

  widget_name = gtk_widget_get_name (widget);
  type_name = G_OBJECT_TYPE_NAME (widget);

  item_label = g_strconcat (type_name,
                            strcmp (widget_name, type_name) ? " (" : NULL,
                            widget_name, ")", NULL);

  item = gtk_menu_item_new_with_label (item_label);
  popup = g_object_get_data (G_OBJECT (self->widget), "popup");

  if (!popup)
    {
      popup = gtk_menu_new ();
      g_object_set_data (G_OBJECT (self->widget), "popup", popup);
    }

  g_signal_connect (item, "activate", G_CALLBACK (item_activate_cb), widget);
  gtk_menu_shell_append (GTK_MENU_SHELL (popup), item);
  gtk_widget_show (item);

  g_free (item_label);
}

static void
append_natural_size_box (TestCase           *test,
                         GtkWidget          *parent,
                         gboolean            vertical,
                         gboolean            table,
                         gboolean            ellipses)
{
  GtkWidget *container = NULL;
  GtkWidget *button, *label;

  PangoEllipsizeMode ellipsize_mode;
  gint i, j, k, l;

  for (i = 0; i < (table ? 9 : 6); ++i)
    {
      ellipsize_mode = ellipses ?
        PANGO_ELLIPSIZE_START + i/(table ? 3 : 2) : 
        PANGO_ELLIPSIZE_NONE;

      if (!i || (ellipses && 0 == i % (table ? 3 : 2)))
        {
          label = gtk_label_new (NULL);

          switch(ellipsize_mode)
            {
              case PANGO_ELLIPSIZE_NONE:
                gtk_label_set_markup (GTK_LABEL (label), "<b>No ellipses</b>");
                break;
              case PANGO_ELLIPSIZE_START:
                gtk_label_set_markup (GTK_LABEL (label), "<b>Ellipses at start</b>");
                break;
              case PANGO_ELLIPSIZE_MIDDLE:
                gtk_label_set_markup (GTK_LABEL (label), "<b>Ellipses in the middle</b>");
                break;
              case PANGO_ELLIPSIZE_END:
                gtk_label_set_markup (GTK_LABEL (label), "<b>Ellipses at end</b>");
                break;
            }

          if (vertical)
            {
              gtk_misc_set_alignment (GTK_MISC (label), 0.0, 1.0);
              gtk_label_set_angle (GTK_LABEL (label), 90);
            }
          else
            {
              gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
              gtk_label_set_angle (GTK_LABEL (label), 0);
            }

          gtk_box_pack_start (GTK_BOX (parent), label, FALSE, TRUE, 0);
        }

      if (table)
        {
          k = 1 + i / 3 + i % 3;

          if (i < 7)
            {
              if (i < 6)
                container = gtk_table_new (vertical ? k : 1,
                                           vertical ? 1 : k, FALSE);
              else
                container = gtk_table_new (vertical ? k : 3,
                                           vertical ? 3 : k, FALSE);

              gtk_table_set_col_spacings (GTK_TABLE (container), 4);
              gtk_table_set_row_spacings (GTK_TABLE (container), 4);
            }
        }
      else if (vertical)
        container = gtk_vbox_new (FALSE, 4);
      else
        container = gtk_hbox_new (FALSE, 4);

      if (!gtk_widget_get_parent (container))
        gtk_box_pack_start (GTK_BOX (parent), container, FALSE, TRUE, 0);

      for (j = 0, l = i < 6 ? i / 3 : i - 6; j <= l; ++j)
        {
          label = gtk_label_new (NULL);
          gtk_label_set_markup (GTK_LABEL (label), "<small>Small Button</small>");
          gtk_label_set_angle (GTK_LABEL (label), vertical ? 90 : 0);
          gtk_label_set_ellipsize (GTK_LABEL (label), ellipsize_mode);

          button = gtk_button_new ();
          set_widget_name (button, "small-%d-%d-%d", ellipses, i, j);
          gtk_container_add (GTK_CONTAINER (button), label);

          if (!table)
            gtk_box_pack_start (GTK_BOX (container), button, FALSE, TRUE, 0);
          else if (i < 6)
            gtk_table_attach (GTK_TABLE (container), button,
                              vertical ? 0 : j, vertical ? 1 : j + 1,
                              vertical ? j : 0, vertical ? j + 1 : 1,
                              GTK_FILL, GTK_FILL, 0, 0);
          else
            gtk_table_attach (GTK_TABLE (container), button,
                              vertical ? i - 6 : j,
                              vertical ? i - 5 : j < l ? j + 1 : 3,
                              vertical ? j : i - 6, 
                              vertical ? (j < l ? j + 1 : 3) : i - 5,
                              GTK_FILL, GTK_FILL, 0, 0);

          test_case_append_guide (test, button,
                                  vertical ? GUIDE_EXTERIOUR_HORIZONTAL 
                                           : GUIDE_EXTERIOUR_VERTICAL,
                                  6 == i ? 3 : 7 == i && j ? 4 : j);
        }

      for (j = 0, l = (i < 6 ? i % 3 : 1); j < l; ++j)
        {
          label = gtk_label_new (NULL);
          gtk_label_set_markup (GTK_LABEL (label), "<small>Large Button</small>");
          gtk_label_set_angle (GTK_LABEL (label), vertical ? 90 : 0);

          button = gtk_button_new ();
          set_widget_name (button, "large-%d-%d-%d", ellipses, i, j);
          gtk_container_add (GTK_CONTAINER (button), label);

          if (table)
            gtk_table_attach (GTK_TABLE (container), button,
                              vertical ? MAX (0, i - 6) : i/3 + j + 1, 
                              vertical ? MAX (1, i - 5) : i/3 + j + 2,
                              vertical ? i/3 + j + 1 : MAX (0, i - 6), 
                              vertical ? i/3 + j + 2 : MAX (1, i - 5),
                              vertical ? GTK_FILL : GTK_FILL | GTK_EXPAND,
                              vertical ? GTK_FILL | GTK_EXPAND : GTK_FILL,
                              0, 0);
          else
            gtk_box_pack_start (GTK_BOX (container), button, TRUE, TRUE, 0);

          test_case_append_guide (test, button, 
                                  vertical ? GUIDE_EXTERIOUR_HORIZONTAL 
                                           : GUIDE_EXTERIOUR_VERTICAL,
                                  i < 6 ? 5 + i + j : 12);
        }
    }
}

static gboolean
restore_paned (gpointer data)
{
  GtkPaned *paned;
  GtkWidget *hint;
  gint pos;


  paned = GTK_PANED (data);
  hint = gtk_paned_get_child2 (paned);
  gtk_widget_set_sensitive (hint, TRUE);

  pos = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (paned), "initial-position"));
  gtk_paned_set_position (paned, pos);

  return FALSE;
}

static gboolean
shrink_paned_timeout (gpointer data)
{
  GtkPaned *paned;
  gint pos;

  paned = GTK_PANED (data);
  pos = gtk_paned_get_position (paned);

  if (pos < 20)
    {
      g_timeout_add (1000, restore_paned, paned);
      return FALSE;
    }

  gtk_paned_set_position (paned, pos - 5);
  return TRUE;
}

static void
shrink_paned (GtkWidget *button,
              gpointer   data)
{
  GtkPaned *paned;
  GtkWidget *hint;

  paned = GTK_PANED (data);
  hint = gtk_paned_get_child2 (paned);
  gtk_widget_set_sensitive (hint, FALSE);

  g_object_set_data (G_OBJECT (paned), "initial-position",
                     GINT_TO_POINTER (gtk_paned_get_position (paned)));
  g_timeout_add (50, shrink_paned_timeout, paned);
}

static TestCase*
natural_size_test_new (TestSuite *suite,
                       gboolean   vertical,
                       gboolean   table)
{
  GtkWidget *box, *paned, *hint, *button;
  const gchar *detail;
  TestCase *test;

  if (vertical)
    {
      detail = table ? "GtkTable, vertical" : "GtkVBox";
      hint = gtk_alignment_new (0.5, 1.0, 1.0, 0.0);
      box = gtk_hbox_new (FALSE, 6);
      paned = gtk_vpaned_new ();
    }
  else
    {
      detail = table ? "GtkTable, horizontal" : "GtkHBox";
      hint = gtk_alignment_new (1.0, 0.5, 0.0, 1.0);
      box = gtk_vbox_new (FALSE, 6);
      paned = gtk_hpaned_new ();
    }

  test = test_case_new (suite, "Natural Size", detail, paned);
  gtk_container_set_border_width (GTK_CONTAINER (test->widget), 6);

  gtk_container_set_border_width (GTK_CONTAINER (box), 6);
  gtk_paned_pack1 (GTK_PANED (test->widget), box, TRUE, TRUE);

  append_natural_size_box (test, box, vertical, table, FALSE);
  append_natural_size_box (test, box, vertical, table, TRUE);

  button = gtk_button_new_with_label ("Shrink to check ellipsing");
  g_signal_connect (button, "clicked", G_CALLBACK (shrink_paned), test->widget);

  if (!vertical) 
    gtk_label_set_angle (GTK_LABEL (GTK_BIN (button)->child), -90);

  gtk_container_set_border_width (GTK_CONTAINER (hint), 6);
  gtk_container_add (GTK_CONTAINER (hint), button);
  gtk_paned_pack2 (GTK_PANED (test->widget), hint, FALSE, FALSE);

  return test;
}

static void
on_socket_realized (GtkWidget *widget,
                    gpointer   data)
{
  gtk_socket_add_id (GTK_SOCKET (widget), GPOINTER_TO_INT (data)); 
}

static void
on_xembed_socket_realized (GtkWidget *widget,
                           gpointer   data)
{
  GdkNativeWindow plug_id = 0;
  GError *error = NULL;
  gchar **argv = data;
  gint child_stdout;

  if (g_spawn_async_with_pipes (NULL, argv, NULL, 0,
                                NULL, NULL, NULL,
                                NULL, &child_stdout, NULL,
                                &error))
    {
      gchar *plug_str;
      char buffer[32];
      gint len;

      len = read (child_stdout, buffer, sizeof (buffer) - 1);
      close (child_stdout);

      if (len > 0)
        {
          buffer[len] = '\0';
          plug_id = atoi (buffer);
        }

      plug_str = g_strdup_printf ("plug-id=%d", plug_id);
      g_print ("%s: %s\n", gtk_widget_get_name (widget), plug_str);
      gtk_widget_set_tooltip_text (widget, plug_str);
      g_free (plug_str);
    }
  else
    {
      GtkWidget *plug, *label;
      gchar *error_message;

      error_message = g_strdup_printf (
        "Failed to create external plug:\n%s",
        error ? error->message : "No details available.");

      label = gtk_label_new (error_message);
      g_warning (error_message);

      g_free (error_message);
      g_clear_error (&error);

      if (argv[2] && g_str_equal (argv[2], "--vertical"))
        gtk_label_set_angle (GTK_LABEL (label), 90);

      plug = gtk_plug_new (0);
      gtk_container_add (GTK_CONTAINER (plug), label);
      gtk_widget_show_all (plug);

      plug_id = gtk_plug_get_id (GTK_PLUG (plug));
    }

  gtk_socket_add_id (GTK_SOCKET (widget), plug_id); 
}

static void
natural_size_test_misc_create_child (TestCase  *test,
                                     GtkWidget *box,
                                     gchar     *arg0,
                                     gint       orientation,
                                     gint       type)
{
  const gchar *type_names[] =
  {
    "align", "socket-gtkplug", "socket-xembed",
    "cell-view", "tree-view", "tree-view-scrolled",
  };

  const gchar *numbers[] = 
  { 
    "First", "Second", "Third", "Fourth", "Fifth",
    "Sixth", "Seventh", "Eighth", "Nineth", NULL
  };

  GtkWidget *label, *child, *view, *align, *plug;
  GdkNativeWindow plug_id;

  GtkListStore *store = NULL;
  GtkTreeViewColumn *column;
  GtkCellRenderer *cell;
  GtkTreePath *path;

  gchar **argv = NULL;
  gint i, argc;

  GdkColor color;

  if (type >= 3)
    {
      store = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

      for (i = 0; numbers[i] && (type > 4 || i < 2); ++i)
        {
          gchar *small = g_strdup_printf ("%s Small Cell", numbers[i]);
          gchar *large = g_strdup_printf ("%s Large Cell", numbers[i]);

          GtkTreeIter iter;

          gtk_list_store_append (store, &iter);
          gtk_list_store_set (store, &iter, 0, GTK_STOCK_ABOUT,
                                            1, small, 2, large, -1);

          g_free (large);
          g_free (small);
        }
    }

  for (i = 0; i < 2; ++i)
    {
      label = gtk_label_new ("Hello World");

      gtk_label_set_ellipsize (GTK_LABEL (label),
                               i ? PANGO_ELLIPSIZE_END : 
                                   PANGO_ELLIPSIZE_NONE);

      if (!orientation)
        gtk_label_set_angle (GTK_LABEL (label), 90);

      switch (type)
        {
          case 0:
            child = label;
            break;

          case 1:
            plug = gtk_plug_new (0);
            plug_id = gtk_plug_get_id (GTK_PLUG (plug));
            gtk_container_add (GTK_CONTAINER (plug), label);
            gtk_widget_show_all (plug);

            child = gtk_socket_new ();

            g_signal_connect (child, "realize",
                              G_CALLBACK (on_socket_realized),
                              GINT_TO_POINTER (plug_id));
            break;

          case 2:
            child = gtk_socket_new ();

            argv = g_new0 (gchar*, 5);
            argc = 0;

            argv[argc++] = arg0;
            argv[argc++] = "--action=create-plug";

            if (!orientation)
              argv[argc++] = "--vertical";
            if (i)
              argv[argc++] = "--ellipsize";

            g_signal_connect_data (child, "realize",
                                   G_CALLBACK (on_xembed_socket_realized),
                                   argv, (GClosureNotify) g_free, 0);
            break;

          case 3:
            child = gtk_cell_view_new ();

            if (gdk_color_parse ("#ffc", &color))
              gtk_cell_view_set_background_color (GTK_CELL_VIEW (child), &color);

            cell = gtk_cell_renderer_pixbuf_new ();
            gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (child), cell, FALSE);
            gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (child), cell,
                                            "stock-id", 0, NULL);

            cell = gtk_cell_renderer_text_new ();
            gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (child), cell, FALSE);
            gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (child), cell,
                                            "text", 1, NULL);

            if (i)
              g_object_set (cell, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

            cell = gtk_cell_renderer_text_new ();
            gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (child), cell, TRUE);
            gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (child), cell,
                                            "text", 2, NULL);

            gtk_cell_view_set_model (GTK_CELL_VIEW (child),
                                     GTK_TREE_MODEL (store));

            path = gtk_tree_path_new_from_indices (0, -1);
            gtk_cell_view_set_displayed_row (GTK_CELL_VIEW (child), path);
            gtk_tree_path_free (path);

            break;

          case 4:
          case 5:
            view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
            gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (view), FALSE);

            cell = gtk_cell_renderer_pixbuf_new ();
            column = gtk_tree_view_column_new_with_attributes (NULL, cell, "stock-id", 0, NULL);
            gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

            cell = gtk_cell_renderer_text_new ();
            column = gtk_tree_view_column_new_with_attributes ("Bar", cell, "text", 1, NULL);
            gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

            if (i)
              g_object_set (cell, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

            cell = gtk_cell_renderer_text_new ();
            column = gtk_tree_view_column_new_with_attributes ("Foo", cell, "text", 2, NULL);
            gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

            if (type > 4)
              {
                child = gtk_scrolled_window_new (NULL, NULL);
                gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (child),
                                                     GTK_SHADOW_IN);
                gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (child),
                                                GTK_POLICY_NEVER,
                                                GTK_POLICY_ALWAYS);
                gtk_container_add (GTK_CONTAINER (child), view);
              }
            else
              child = view;

            break;

          default:
            continue;
        }

      align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
      gtk_box_pack_start (GTK_BOX (box), align, FALSE, TRUE, 0);
      gtk_container_add (GTK_CONTAINER (align), child);

      set_widget_name (child, "%s-%s-ellipsize-%s",
                       orientation ? "horizontal" : "vertical",
                       type_names[type], i ? "end" : "none");

      test_case_append_guide (test, child, 
                              orientation ? GUIDE_EXTERIOUR_VERTICAL
                                          : GUIDE_EXTERIOUR_HORIZONTAL,
                              type < 3 ? orientation : type - 1);
    }
}

static TestCase*
natural_size_test_misc_new (TestSuite *suite,
                            gchar     *arg0)
{
  const gchar *captions[] = 
  {
    "<b>GtkAligment</b>",
    "<b>GtkSocket with GtkPlug</b>",
    "<b>GtkSocket with XEMBED</b>",
    "<b>GtkCellView</b>",
    "<b>GtkTreeView</b>",
    "<b>GtkTreeView within GtkScrolledWindow</b>"
  };

  GtkWidget *box, *hpaned, *vpaned, *label;
  gint orientation, type;
  TestCase *test;

  test = test_case_new (suite,  "Natural Size", "Various Widgets", 
                        gtk_vbox_new (FALSE, 6));

  gtk_container_set_border_width (GTK_CONTAINER (test->widget), 6);

  vpaned = NULL; /* silence the gcc */

  for (orientation = 0; orientation < 2; ++orientation)
    {
      label = gtk_label_new ("Move the handle to test\n"
                             "natural size allocation");
      gtk_misc_set_padding (GTK_MISC (label), 6, 6);

      if (orientation)
        {
          gtk_label_set_angle (GTK_LABEL (label), 90);
          gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

          hpaned = gtk_hpaned_new ();
          box = gtk_hbox_new (FALSE, 6);

          gtk_box_pack_start (GTK_BOX (box), label, FALSE, TRUE, 0);
          gtk_box_pack_start (GTK_BOX (box), gtk_vseparator_new (), FALSE, TRUE, 0);
          gtk_box_pack_start (GTK_BOX (box), vpaned, TRUE, TRUE, 0);

          gtk_paned_pack2 (GTK_PANED (hpaned), box, TRUE, FALSE);

          box = gtk_vbox_new (FALSE, 6);

          gtk_paned_pack1 (GTK_PANED (hpaned), box, TRUE, TRUE);
          gtk_box_pack_start (GTK_BOX (test->widget), hpaned, TRUE, TRUE, 0);
        }
      else
        {
          gtk_misc_set_alignment (GTK_MISC (label), 0.5, 1.0);

          vpaned = gtk_vpaned_new ();
          box = gtk_hbox_new (FALSE, 6);

          gtk_paned_pack1 (GTK_PANED (vpaned), box, TRUE, TRUE);
          gtk_paned_pack2 (GTK_PANED (vpaned), label, FALSE, FALSE);
        }

      for (type = 0; type < (orientation ? 6 : 3); ++type)
        {
          label = gtk_label_new (NULL);
          gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
          gtk_label_set_markup (GTK_LABEL (label), captions[type]);
          gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
          gtk_box_pack_start (GTK_BOX (box), label, FALSE, TRUE, 0);

          if (!orientation)
            gtk_label_set_angle (GTK_LABEL (label), 90);

          natural_size_test_misc_create_child (test, box, arg0, orientation, type);
        }
    }

  return test;
}

static TestCase*
size_for_allocation_test_new (TestSuite *suite,
                              gboolean   vertical,
                              gboolean   table)
{
  GtkWidget *container, *child;
  TestCase *test;
  int i;

  if (vertical)
    {
      test = test_case_new (suite,
                            "Size for Allocation", 
                            table ? "Height for Width, GtkTable"
                                  : "Height for Width, GtkVBox",
                            gtk_hpaned_new ());

      if (table)
        {
          container = gtk_table_new (4, 1, FALSE);
          gtk_orientable_set_orientation (GTK_ORIENTABLE (container), 
                                          GTK_ORIENTATION_VERTICAL);
        }
      else
        container = gtk_vbox_new (FALSE, 6);

      child = gtk_label_new ("Move the handle to test\n"
                             "height-for-width requests");

      gtk_label_set_angle (GTK_LABEL (child), 90);
    }
  else
    {
      test = test_case_new (suite,
                            "Size for Allocation", 
                            table ? "Width for Height, GtkTable"
                                  : "Width for Height, GtkHBox",
                            gtk_vpaned_new ());

      if (table)
        {
          container = gtk_table_new (1, 4, FALSE);
          gtk_orientable_set_orientation (GTK_ORIENTABLE (container), 
                                     GTK_ORIENTATION_HORIZONTAL);
        }
      else
        container = gtk_hbox_new (FALSE, 6);

      child = gtk_label_new ("Move the handle to test\n"
                             "width-for-height requests");
    }

  gtk_container_set_border_width (GTK_CONTAINER (test->widget), 6);
  gtk_container_set_border_width (GTK_CONTAINER (container), 6);
  gtk_misc_set_padding (GTK_MISC (child), 6, 6);

  gtk_paned_pack1 (GTK_PANED (test->widget), container, TRUE, FALSE);
  gtk_paned_pack2 (GTK_PANED (test->widget), child, FALSE, FALSE);

  for (i = 0; i < 4; ++i)
    {
      if (2 != i)
        {       
          child = gtk_label_new (lorem_ipsum);
          gtk_label_set_line_wrap (GTK_LABEL (child), TRUE);
          gtk_label_set_use_markup (GTK_LABEL (child), TRUE);
          test_case_append_guide (test, child, GUIDE_EXTERIOUR_BOTH, -1);
          test_case_append_guide (test, child, GUIDE_INTERIOUR_BOTH, -1);

          if (!table)
            gtk_box_pack_start (GTK_BOX (container), child, FALSE, TRUE, 0);
          else if (vertical)
            gtk_table_attach (GTK_TABLE (container), child, 0, 1, i, i + 1,
                              GTK_EXPAND|GTK_FILL, GTK_FILL, 0, 0);
          else
            gtk_table_attach (GTK_TABLE (container), child, i, i + 1, 0, 1, 
                              GTK_FILL, GTK_EXPAND|GTK_FILL, 0, 0);

/*           if (i > 0) */
/*             gtk_label_set_full_size (GTK_LABEL (child), TRUE); */

          if (i > 2)
            gtk_label_set_angle (GTK_LABEL (child), vertical ? 180 : 270);
          else if (!vertical)
            gtk_label_set_angle (GTK_LABEL (child), 90);

          set_widget_name (child, "%s-label-and-%g-degree",
                           i > 0 ? "full-size" : "regular",
                           gtk_label_get_angle (GTK_LABEL (child)));
        }
      else
        {       
          child = gtk_button_new ();
          set_widget_name (child, "the-button");
          gtk_container_add (GTK_CONTAINER (child),
                             gtk_image_new_from_stock (GTK_STOCK_DIALOG_INFO,
                                                       GTK_ICON_SIZE_DIALOG));

          if (!table)
            gtk_box_pack_start (GTK_BOX (container), child, TRUE, TRUE, 0);
          else if (vertical)
            gtk_table_attach (GTK_TABLE (container), child, 0, 1, i, i + 1,
                              GTK_EXPAND|GTK_FILL, GTK_EXPAND|GTK_FILL, 0, 0);
          else
            gtk_table_attach (GTK_TABLE (container), child, i, i + 1, 0, 1,
                              GTK_EXPAND|GTK_FILL, GTK_EXPAND|GTK_FILL, 0, 0);
        }
    }

  return test;
}

static TestCase*
baseline_test_new (TestSuite *suite)
{
  GtkWidget *child;
  GtkWidget *view;
  GtkWidget *label;

  TestCase *test = test_case_new (suite, 
                                  "Baseline Alignment", "Real-World Example",
                                  gtk_table_new (3, 3, FALSE));

  gtk_container_set_border_width (GTK_CONTAINER (test->widget), 12);
  gtk_table_set_col_spacings (GTK_TABLE (test->widget), 6);
  gtk_table_set_row_spacings (GTK_TABLE (test->widget), 6);

  child = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (child), "Test...");
  test_case_append_guide (test, child, GUIDE_BASELINE, 0);
  gtk_table_attach (GTK_TABLE (test->widget), child, 1, 2, 0, 1, 
                    GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);

  label = gtk_label_new_with_mnemonic ("_Title:");
  test_case_append_guide (test, label, GUIDE_BASELINE, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_label_set_mnemonic_widget  (GTK_LABEL (label), child);
  gtk_table_attach (GTK_TABLE (test->widget), label, 0, 1, 0, 1, 
                    GTK_FILL, GTK_FILL, 0, 0);

  label = gtk_label_new_with_mnemonic ("Notice on\ntwo rows.");
  test_case_append_guide (test, label, GUIDE_BASELINE, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
  gtk_table_attach (GTK_TABLE (test->widget), label, 2, 3, 0, 2, 
                    GTK_FILL, GTK_FILL, 0, 0);

  child = gtk_font_button_new ();
  test_case_append_guide (test, child, GUIDE_BASELINE, 1);
  gtk_table_attach (GTK_TABLE (test->widget), child, 1, 2, 1, 2, 
                    GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);

  label = gtk_label_new_with_mnemonic ("_Font:");
  test_case_append_guide (test, label, GUIDE_BASELINE, 1);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_label_set_mnemonic_widget  (GTK_LABEL (label), child);
  gtk_table_attach (GTK_TABLE (test->widget), label, 0, 1, 1, 2, 
                    GTK_FILL, GTK_FILL, 0, 0);

  view = gtk_text_view_new ();
  gtk_widget_set_size_request (view, 200, -1);
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (view),
                               GTK_WRAP_WORD);
  test_case_append_guide (test, view, GUIDE_BASELINE, 2);
  gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)),
                            lorem_ipsum, -1);

  child = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (child),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (child),
                                       GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (child), view);

  gtk_table_attach (GTK_TABLE (test->widget), child, 1, 3, 2, 3, 
                    GTK_FILL | GTK_EXPAND,
                    GTK_FILL | GTK_EXPAND, 
                    0, 0);

  label = gtk_label_new_with_mnemonic ("_Comment:");
  test_case_append_guide (test, label, GUIDE_BASELINE, 2);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
  gtk_label_set_mnemonic_widget  (GTK_LABEL (label), child);
  gtk_table_attach (GTK_TABLE (test->widget), label, 0, 1, 2, 3,
                    GTK_FILL, GTK_FILL, 0, 0);

  return test;
}

static TestCase*
baseline_test_bin_new (TestSuite *suite)
{
  GtkWidget *bin;
  GtkWidget *label;
  GtkWidget *table;

  int i, j;

  const GType types[] = 
    { 
      GTK_TYPE_ALIGNMENT, GTK_TYPE_BUTTON, 
      GTK_TYPE_EVENT_BOX, GTK_TYPE_FRAME, 
      G_TYPE_INVALID
    };

  TestCase *test = test_case_new (suite, 
                                  "Baseline Alignment", "Various GtkBins",
                                  gtk_alignment_new (0.5, 0.5, 0.0, 0.0));

  table = gtk_table_new (G_N_ELEMENTS (types) - 1, 
                         G_N_ELEMENTS (captions),
                         FALSE);

  gtk_container_set_border_width (GTK_CONTAINER (table), 12);
  gtk_table_set_col_spacings (GTK_TABLE (table), 6);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_container_add (GTK_CONTAINER (test->widget), table);

  for (i = 0; types[i]; ++i)
    {
      label = gtk_label_new (g_type_name (types[i]));
      gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

      gtk_table_attach (GTK_TABLE (table), label, 0, 1,
                        i, i + 1, GTK_FILL, GTK_FILL, 0, 0);

      for (j = 0; captions[j]; ++j)
        {
          bin = g_object_new (types[i], NULL);
          label = gtk_label_new (NULL);

          gtk_label_set_markup (GTK_LABEL (label), captions[j]);
          gtk_container_add (GTK_CONTAINER (bin), label);

          test_case_append_guide (test, bin, GUIDE_BASELINE, i);
          gtk_table_attach (GTK_TABLE (table), bin, j + 1, j + 2,
                            i, i + 1, GTK_FILL, GTK_FILL, 0, 0);
        }
    }

  return test;
}

#if 0
static TestCase*
baseline_test_hbox_new (TestSuite *suite,
                        gboolean   buttons)
{
  GtkWidget *bin;
  GtkWidget *child;
  GtkWidget *table;
  GtkWidget *hbox;

  int i, j;

  const gchar *names[] = 
    {
      "default", "baseline", "baseline and bottom-padding", 
      "baseline and top-padding", "baseline and border-width",
      NULL
    };

  TestCase *test = test_case_new (suite, "Baseline Alignment",
                                  buttons ? "GtkHBox and Buttons" 
                                          : "GtkHBox and Labels",
                                  gtk_alignment_new (0.5, 0.5, 0.0, 0.0));

  table = gtk_table_new (G_N_ELEMENTS (names) - 1, 
                         G_N_ELEMENTS (captions),
                         FALSE);

  gtk_container_set_border_width (GTK_CONTAINER (table), 12);
  gtk_table_set_col_spacings (GTK_TABLE (table), 6);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_container_add (GTK_CONTAINER (test->widget), table);

  for (i = 0; names[i]; ++i)
    {
      child = gtk_label_new (names[i]);
      gtk_misc_set_alignment (GTK_MISC (child), 0.0, 0.5);
  
      hbox = gtk_hbox_new (FALSE, 6);
      test_case_append_guide (test, hbox, GUIDE_EXTERIOUR_BOTH, -1);
      set_widget_name (hbox, "hbox-%s", names[i]);

      if (i > 0)
        gtk_hbox_set_baseline_policy (GTK_HBOX (hbox), GTK_BASELINE_FIRST);

      gtk_table_attach (GTK_TABLE (table), child,
                        0, 1, i, i + 1,
                        GTK_FILL, GTK_FILL, 0, 0);
      gtk_table_attach (GTK_TABLE (table), hbox,
                        1, G_N_ELEMENTS (captions), i, i + 1,
                        GTK_FILL, GTK_FILL, 0, 0);

      for (j = i ? -3 : 0; captions[MAX (0, j)]; ++j)
        {
          child = gtk_label_new (NULL);
          gtk_label_set_markup (GTK_LABEL (child), captions[MAX (0, j)]);

          if (buttons)
            {
              bin = gtk_button_new ();
              gtk_container_add (GTK_CONTAINER (bin), child);
              child = bin;
            }

          test_case_append_guide (test, child, GUIDE_BASELINE, i);

          if (j < 0 && i > 1)
            {
              bin = gtk_alignment_new (0.5, 0.5, 0.0, (j + 3) * 0.5);

              set_widget_name (bin, "align-%s-%s-%d",
                               buttons ? "button" : "label",
                               names[i], (j + 3) * 50);

              switch (i) 
                {
                  case 2:
                    gtk_alignment_set_padding (GTK_ALIGNMENT (bin), 0, 25, 0, 0);
                    break;

                  case 3:
                    gtk_alignment_set_padding (GTK_ALIGNMENT (bin), 25, 0, 0, 0);
                    break;

                  case 4:
                    gtk_container_set_border_width (GTK_CONTAINER (bin), 12);
                    break;
                }

              gtk_container_add (GTK_CONTAINER (bin), child);
              test_case_append_guide (test, bin, GUIDE_BASELINE, i);
              child = bin;
            }

          gtk_box_pack_start (GTK_BOX (hbox), child, FALSE, TRUE, 0);
        }
    }

  return test;
}
#endif

static gboolean
get_extends (GtkWidget    *widget,
             GtkWidget    *toplevel,
             GdkRectangle *extends)
{
  *extends = widget->allocation;

  return
    gtk_widget_get_visible (widget) &&
    gtk_widget_translate_coordinates (widget, toplevel, 0, 0, 
                                      &extends->x, &extends->y);
}

static gboolean
get_interiour (GtkWidget    *widget,
                GtkWidget    *toplevel,
               GdkRectangle *extends)
{
  if (GTK_IS_LABEL (widget))
    {
      PangoLayout *layout;
      PangoRectangle log;
      GtkLabel *label;

      label = GTK_LABEL (widget);
      layout = gtk_label_get_layout (label);
      pango_layout_get_pixel_extents (layout, NULL, &log);
      gtk_label_get_layout_offsets (label, &log.x, &log.y);

      log.x -= toplevel->allocation.x;
      log.y -= toplevel->allocation.y;

      g_assert (sizeof log == sizeof *extends);
      memcpy (extends, &log, sizeof *extends);
    }

  return FALSE;
}

static gint
get_baseline_of_layout (PangoLayout *layout)
{
  PangoLayoutLine *line;
  PangoRectangle log;

  line = pango_layout_get_line_readonly (layout, 0);
  pango_layout_line_get_pixel_extents (line, NULL, &log);
  return PANGO_ASCENT (log);
}

static gint
get_baselines_of_text_view (GtkTextView *view, gint **baselines)
{
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (view);
  GtkTextAttributes *attrs = gtk_text_view_get_default_attributes (view);
  PangoContext *context = gtk_widget_get_pango_context (GTK_WIDGET (view));
  PangoLayout *layout = pango_layout_new (context);

  GtkTextIter start, end;
  GdkRectangle bounds;
  gchar *text;

  gtk_text_buffer_get_start_iter (buffer, &start);
  gtk_text_iter_get_attributes (&start, attrs);

  end = start;
  gtk_text_iter_forward_to_line_end (&end);
  text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
  gtk_text_view_get_iter_location (view, &start, &bounds);

  pango_layout_set_width (layout, PANGO_SCALE *
                          GTK_WIDGET (view)->allocation.width);
  pango_layout_set_font_description (layout, attrs->font);
  pango_layout_set_wrap (layout, PANGO_WRAP_WORD);
  pango_layout_set_text (layout, text, -1);

  gtk_text_view_buffer_to_window_coords (view, GTK_TEXT_WINDOW_TEXT,
                                         0, bounds.y, NULL, &bounds.y);
  bounds.y += get_baseline_of_layout (layout);

  gtk_text_attributes_unref (attrs);
  g_object_unref (layout);
  g_free (text);

  *baselines = g_new(gint, 1);
  *baselines[0] = bounds.y;

  return 1;
}

static gint
get_baselines (GtkWidget *widget, gint **baselines)
{
#if 0
  if (GTK_IS_EXTENDED_LAYOUT (widget) &&
      GTK_EXTENDED_LAYOUT_HAS_BASELINES (widget))
    return gtk_extended_layout_get_baselines (GTK_EXTENDED_LAYOUT (widget), baselines);
#endif
  if (GTK_IS_TEXT_VIEW (widget))
    return get_baselines_of_text_view (GTK_TEXT_VIEW (widget), baselines);

  return -1;
}

static void
draw_baselines (GdkDrawable  *drawable,
                GdkGC        *gc,
                GtkWidget    *toplevel,
                GdkRectangle *extends,
                gint          baseline)
{
  const gint x0 = toplevel->allocation.x;
  const gint y0 = toplevel->allocation.y;
  const gint cx = toplevel->allocation.width;

  const gint xa = x0 + extends->x;
  const gint xe = xa + extends->width - 1;
  const gint ya = y0 + extends->y + baseline;

  gdk_draw_line (drawable, gc, xa, ya - 5, xa, ya + 2);
  gdk_draw_line (drawable, gc, xa - 5, ya, xe + 5, ya);
  gdk_draw_line (drawable, gc, xe, ya - 5, xe, ya + 2);

  gdk_gc_set_line_attributes (gc, 1, GDK_LINE_ON_OFF_DASH,
                              GDK_CAP_NOT_LAST, GDK_JOIN_MITER);

  gdk_gc_set_dashes (gc, x0 % (dashes[0] + dashes[1]), dashes, 2);
  gdk_draw_line (drawable, gc, x0, ya, xa - 5, ya);

  gdk_gc_set_dashes (gc, (xe + 2) % (dashes[0] + dashes[1]), dashes, 2);
  gdk_draw_line (drawable, gc, xe + 5, ya, x0 + cx - 1, ya);

  gdk_gc_set_line_attributes (gc, 1, GDK_LINE_SOLID,
                              GDK_CAP_NOT_LAST, GDK_JOIN_MITER);
}

static void
draw_extends (GdkDrawable  *drawable,
              GdkGC        *gc,
              GtkWidget    *toplevel,
              GdkRectangle *extends)
{
  const gint x0 = toplevel->allocation.x;
  const gint y0 = toplevel->allocation.y;
  const gint cx = toplevel->allocation.width;
  const gint cy = toplevel->allocation.height;

  const gint xa = x0 + extends->x;
  const gint xe = xa + extends->width - 1;

  const gint ya = y0 + extends->y;
  const gint ye = ya + extends->height - 1;

  gdk_draw_line (drawable, gc, xa, y0, xa, y0 + cy - 1);
  gdk_draw_line (drawable, gc, xe, y0, xe, y0 + cy - 1);
  gdk_draw_line (drawable, gc, x0, ya, x0 + cx - 1, ya);
  gdk_draw_line (drawable, gc, x0, ye, x0 + cx - 1, ye);
}

static gint
test_case_eval_guide (const TestCase  *self,
                      const Guide     *guide,
                      GdkRectangle    *extends,
                      gint           **baselines)
{
  gint num_baselines = -1;

  if (get_extends (guide->widget, self->widget, extends))
    {
      *baselines = NULL;

      switch (guide->type)
        {
          case GUIDE_BASELINE:
            num_baselines = get_baselines (guide->widget, baselines);
            break;

          case GUIDE_INTERIOUR_BOTH:
          case GUIDE_INTERIOUR_VERTICAL:
          case GUIDE_INTERIOUR_HORIZONTAL:
            get_interiour (guide->widget, self->widget, extends);
            num_baselines = 0;
            break;

          case GUIDE_EXTERIOUR_BOTH:
          case GUIDE_EXTERIOUR_VERTICAL:
          case GUIDE_EXTERIOUR_HORIZONTAL:
            num_baselines = 0;
            break;
        }
    }

  return num_baselines;
}

static gboolean
guide_is_compatible (const Guide    *self,
                     const Guide    *other)
{
  switch (self->type)
    {
      case GUIDE_BASELINE:
        return
          GUIDE_BASELINE == other->type;

      case GUIDE_INTERIOUR_BOTH:
      case GUIDE_EXTERIOUR_BOTH:
        return 
          GUIDE_INTERIOUR_BOTH == other->type ||
          GUIDE_EXTERIOUR_BOTH == other->type;

      case GUIDE_INTERIOUR_VERTICAL:
      case GUIDE_EXTERIOUR_VERTICAL:
        return 
          GUIDE_INTERIOUR_VERTICAL == other->type ||
          GUIDE_EXTERIOUR_VERTICAL == other->type;

      case GUIDE_INTERIOUR_HORIZONTAL:
      case GUIDE_EXTERIOUR_HORIZONTAL:
        return 
          GUIDE_INTERIOUR_HORIZONTAL == other->type ||
          GUIDE_EXTERIOUR_HORIZONTAL == other->type;
    }

  g_return_val_if_reached (FALSE);
}

static gboolean
test_case_compare_guides (const TestCase *self,
                          const Guide    *guide1,
                          const Guide    *guide2)
{
  gint *baselines1 = NULL, *baselines2 = NULL;
  GdkRectangle extends1, extends2;
  gboolean equal = FALSE;

  if (guide_is_compatible (guide1, guide2) &&
      test_case_eval_guide (self, guide1, &extends1, &baselines1) >= 0 &&
      test_case_eval_guide (self, guide2, &extends2, &baselines2) >= 0)
    {
      switch (guide1->type)
        {
          case GUIDE_BASELINE:
            equal =
              IS_VALID_BASELINE (*baselines1) &&
              IS_VALID_BASELINE (*baselines2) &&
              extends1.y + *baselines1 == extends2.y + *baselines2;
            break;

          case GUIDE_INTERIOUR_HORIZONTAL:
          case GUIDE_EXTERIOUR_HORIZONTAL:
            equal =
              extends1.height == extends2.height &&
              extends1.y == extends2.y;
            break;

          case GUIDE_INTERIOUR_VERTICAL:
          case GUIDE_EXTERIOUR_VERTICAL:
            equal =
              extends1.width == extends2.width &&
              extends1.x == extends2.x;
            break;

          case GUIDE_INTERIOUR_BOTH:
          case GUIDE_EXTERIOUR_BOTH:
            equal = !memcpy (&extends1, &extends2, sizeof extends1);
            break;

        }
    }

  g_free (baselines1);
  g_free (baselines2);

  return equal;
}

static const gchar*
guide_type_get_color (GuideType type,
                      gboolean  is_current)
{
  switch (type) 
    {
      case GUIDE_BASELINE: return is_current ? "#f00" : "#00f";
      default: return is_current ? "#000" : "#fff";
    }
}

static gboolean
draw_guides (gpointer data)
{
  TestCase *test = data;
  GdkDrawable *drawable;
  const GList *iter;
  gint iteration;

  GdkGCValues values;
  GdkGC *gc;

  gboolean show_baselines;
  gboolean show_interiour;
  gboolean show_exteriour;

  values.subwindow_mode = GDK_INCLUDE_INFERIORS;
  drawable = test->widget->window;

  gc = gdk_gc_new_with_values (drawable, &values, 
                               GDK_GC_SUBWINDOW);

  gdk_gc_set_tile (gc, test->suite->tile);

  show_baselines =
    test->suite->baselines && gtk_toggle_button_get_active (
    GTK_TOGGLE_BUTTON (test->suite->baselines));
  show_interiour =
    test->suite->interiour && gtk_toggle_button_get_active (
    GTK_TOGGLE_BUTTON (test->suite->interiour));;
  show_exteriour = 
    test->suite->exteriour && gtk_toggle_button_get_active (
    GTK_TOGGLE_BUTTON (test->suite->exteriour));;

  for (iteration = 0; iteration < 3; ++iteration)
    {
      for (iter = test->guides; iter; iter = iter->next)
        {
          const Guide *guide = iter->data;
          gboolean is_current = (guide->widget == test->suite->current);
          GdkRectangle extends;
          gint num_baselines;
          gint *baselines;
          gint i;

          if (!is_current != !iteration)
            continue;

          if (is_current)
            {
              if (test->suite->timestamp >= 3)
                continue;

              if (1 == iteration)
                {
                  gdk_gc_set_fill (gc, GDK_TILED);
                  gdk_gc_set_function (gc, GDK_OR);

                  gdk_draw_rectangle (drawable, gc, TRUE, 
                                      guide->widget->allocation.x,
                                      guide->widget->allocation.y,
                                      guide->widget->allocation.width,
                                      guide->widget->allocation.height);

                  gdk_gc_set_function (gc, GDK_COPY);
                  gdk_gc_set_fill (gc, GDK_SOLID);

                  continue;
                }
            }

          gdk_color_parse (guide_type_get_color (guide->type, is_current),
                           &values.foreground);

          gdk_gc_set_rgb_fg_color (gc, &values.foreground);

          num_baselines = test_case_eval_guide (test, guide, &extends, &baselines);

          if (num_baselines > 0)
            {
              g_assert (NULL != baselines);

              if (show_baselines)
                for (i = 0; i < num_baselines; ++i)
                  draw_baselines (drawable, gc, test->widget, &extends, baselines[i]);
            }
          else if (num_baselines > -1)
            {
              if ((show_interiour && (
                   guide->type == GUIDE_INTERIOUR_VERTICAL ||
                   guide->type == GUIDE_INTERIOUR_HORIZONTAL ||
                   guide->type == GUIDE_INTERIOUR_BOTH)) ||
                  (show_exteriour && (
                   guide->type == GUIDE_EXTERIOUR_VERTICAL ||
                   guide->type == GUIDE_EXTERIOUR_HORIZONTAL ||
                   guide->type == GUIDE_EXTERIOUR_BOTH)))
                draw_extends (drawable, gc, test->widget, &extends);
            }

          g_free (baselines);
        }
    }

  g_object_unref (gc);
  test->idle = 0;

  return FALSE;
}

static gboolean           
expose_cb (GtkWidget      *widget,
           GdkEventExpose *event,
           gpointer        data)
{
  TestCase *test = data;

  if (0 == test->idle)
    {
      if (widget != test->widget)
        gtk_widget_queue_draw (test->widget);

      test->idle = g_idle_add (draw_guides, test);
    }

  return FALSE;
}

static void
realize_cb (GtkWidget *widget,
            gpointer   data)
{
  TestCase *test = data;

  if (widget->window != test->widget->window)
    g_signal_connect_after (widget, "expose-event",
                            G_CALLBACK (expose_cb), test);
}

static void
attach_sub_windows (GtkWidget *widget,
                    gpointer   data)
{
  g_signal_connect_after (widget, "realize", G_CALLBACK (realize_cb), data);

  if (GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget), attach_sub_windows, data);
}

static void
test_suite_insert_page (TestSuite   *self, 
                        TestCase    *test,
                        GtkWidget   *widget,
                        const gchar *label)
{
  GtkTreeModel *model = GTK_TREE_MODEL (self->tests);
  TestCase *prev = NULL;
  GtkTreeIter iter;
  gint i, n_rows;

  if (!widget && test)
    widget = test->widget;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  n_rows = gtk_tree_model_iter_n_children (model, NULL);

  gtk_notebook_insert_page (GTK_NOTEBOOK (self->notebook), 
                            widget, NULL, self->n_test_cases);

  gtk_list_store_insert (self->tests, &iter, n_rows);

  gtk_list_store_set (self->tests, &iter,
                      TEST_COLUMN_LABEL, label, 
                      TEST_COLUMN_SELECTED, NULL != test, 
                      TEST_COLUMN_HAS_TEST_CASE, NULL != test, 
                      TEST_COLUMN_PAGE_INDEX, self->n_test_cases,
                      TEST_COLUMN_TEST_CASE, test,
                      -1);

  for (i = n_rows - 1; i >= 0 && NULL == prev &&
       gtk_tree_model_iter_nth_child (model, &iter, NULL, i);
       --i)
    gtk_tree_model_get (GTK_TREE_MODEL (self->tests), &iter,
                        TEST_COLUMN_TEST_CASE, &prev, 
                        -1);

  if (NULL == test || (prev && strcmp (test->name, prev->name)))
    {
      gtk_list_store_insert (self->tests, &iter, n_rows);
      gtk_list_store_set (self->tests, &iter,
                          TEST_COLUMN_HAS_TEST_CASE, FALSE,
                          TEST_COLUMN_PAGE_INDEX, -1,
                          -1);
    }

  if (test)
    ++self->n_test_cases;
}

static void
test_suite_append (TestSuite *self,
                   TestCase  *test)
{
  GString *markup = g_string_new (test->name);

  g_string_printf (markup, "<b>%s</b>", test->name);

  if (test->detail)
    g_string_append_printf (markup, "\n<small>%s</small>", test->detail);

  test_suite_insert_page (self, test, NULL, markup->str);
  g_string_free (markup, TRUE);

  g_signal_connect_after (test->widget, "expose-event",
                          G_CALLBACK (expose_cb), test);
  g_signal_connect_after (test->widget, "realize",
                          G_CALLBACK (realize_cb), test);
  g_object_set_data_full (G_OBJECT(test->widget), 
                          "test-case", test, g_free);

  gtk_container_forall (GTK_CONTAINER (test->widget),
                        attach_sub_windows, test);
}

static void 
realize_notebook_cb (GtkWidget *widget,
                     gpointer   data)
{
  TestSuite *suite = data;

  suite->tile =
    gdk_pixmap_colormap_create_from_xpm_d (
    suite->notebook->window, NULL, NULL, NULL,
    mask_xpm);
}

static void
test_suite_free (TestSuite* self)
{       
  g_object_unref (self->tile);
  g_free (self);
}

static void
test_suite_start (TestSuite *self)
{
  if (0 == self->level++)
    {
      g_print ("\033[1mStarting test suite.\033[0m\n");
      gtk_tree_store_clear (self->results);
    }
}

static void
test_suite_stop (TestSuite *self)
{
  if (0 == --self->level)
    {
      gtk_notebook_set_current_page (GTK_NOTEBOOK (self->notebook), 
                                     self->n_test_cases);
      g_print ("\033[1mTest suite stopped.\033[0m\n");
    }
}

static const gchar*
test_result_to_string (TestResult result)
{
  switch (result)
  {
    case TEST_RESULT_NONE: return NULL;
    case TEST_RESULT_SUCCESS: return "SUCCESS";
    case TEST_RESULT_FAILURE: return "FAILURE";
  }

  g_return_val_if_reached (NULL);
}

static const gchar*
test_result_to_icon (TestResult result)
{
  switch (result)
  {
    case TEST_RESULT_NONE: return GTK_STOCK_EXECUTE;
    case TEST_RESULT_SUCCESS: return GTK_STOCK_OK;
    case TEST_RESULT_FAILURE: return GTK_STOCK_DIALOG_ERROR;
  }

  g_return_val_if_reached (NULL);
}

static void
test_suite_report (TestSuite   *self,
                   const gchar *message,
                   gint         group,
                   TestResult   result)
{
  const gchar *text = test_result_to_string (result);
  const gchar *icon = test_result_to_icon (result);

  GtkTreeIter iter;

  if (message)
    {
      PangoWeight weight = PANGO_WEIGHT_NORMAL;
      GtkTreePath *path;

      if (TEST_RESULT_NONE != result)
        {
          g_print ("   - %s: %s\n", message, text);
          gtk_tree_store_append (self->results, &iter, &self->parent);
        }
      else if (group < 0)
        {
          g_print ("\033[1mTesting: %s\033[0m\n", message);
          gtk_tree_store_append (self->results, &self->parent, NULL);
          weight = PANGO_WEIGHT_BOLD;
          iter = self->parent;

        }
      else
        {
          if (gtk_tree_store_iter_depth (self->results, &self->parent) < 1 ||
              !gtk_tree_model_iter_parent (GTK_TREE_MODEL (self->results), &iter, &self->parent))
              iter = self->parent;

          g_print (" * %s\n", message);
          gtk_tree_store_append (self->results, &self->parent, &iter);
          iter = self->parent;
        }

      gtk_tree_store_set (self->results, &iter, 
                          RESULT_COLUMN_MESSAGE, message, 
                          RESULT_COLUMN_WEIGHT, weight, 
                          RESULT_COLUMN_RESULT, text, 
                          RESULT_COLUMN_ICON, icon,
                          -1);

      if (TEST_RESULT_FAILURE == result)
        {
          path = gtk_tree_model_get_path (GTK_TREE_MODEL (self->results), &iter);
          gtk_tree_view_expand_to_path (GTK_TREE_VIEW (self->results_view), path);
          gtk_tree_path_free (path);
        }
    }
  else
    {
      if (-1 == group && gtk_tree_model_iter_parent (
          GTK_TREE_MODEL (self->results), &iter, &self->parent))
        self->parent = iter;

      gtk_tree_store_set (self->results, &self->parent,
                          RESULT_COLUMN_RESULT, text, 
                          RESULT_COLUMN_ICON, icon,
                          -1);
    }
}

static void
test_suite_run (TestSuite *self,
                gint       index)
{
  GtkNotebook *notebook;
  GtkWidget *page;
  TestCase *test;

  notebook = GTK_NOTEBOOK (self->notebook);

  if (-1 == index)
    index = gtk_notebook_get_current_page (notebook);

  page = gtk_notebook_get_nth_page (notebook, index);
  test = g_object_get_data (G_OBJECT (page), "test-case");

  if (NULL != test)
    {
      TestResult test_result = TEST_RESULT_SUCCESS;
      gint last_group = -1;
      gchar *message;
      GList *oiter;
      gint o, group;

      message = test->detail ?
        g_strdup_printf ("%s (%s)", test->name, test->detail) :
        g_strdup (test->name);

      test_suite_start (self);
      test_suite_report (self, message, -1, TEST_RESULT_NONE);

      g_free (message);

      for (oiter = test->guides; oiter; oiter = oiter->next)
        last_group = MAX (last_group, ((const Guide*)oiter->data)->group);

      for (group = 0; group <= last_group; ++group)
        {
          const Guide *oguide;

          for (o = 0, oiter = test->guides; oiter; ++o, oiter = oiter->next)
            {
              oguide = oiter->data;

              if (oguide->group == group)
                break;
            }

          if (oiter)
            {
              TestResult group_result = TEST_RESULT_SUCCESS;
              const gchar *widget_name;
              const gchar *type_name;

              GList *iiter;
              gint i;

              widget_name = gtk_widget_get_name (oguide->widget);
              type_name = G_OBJECT_TYPE_NAME (oguide->widget);

              message = g_strdup_printf (
                "Group %d, Guide %d (%s%s%s)",
                oguide->group, o, type_name,
                strcmp (type_name, widget_name) ? ": " : "",
                strcmp (type_name, widget_name) ? widget_name : "");

              test_suite_report (self, message, oguide->group, TEST_RESULT_NONE);
              g_free (message);

              for(i = 0, iiter = test->guides; iiter; ++i, iiter = iiter->next)
                {
                  const Guide *iguide = iiter->data;

                  if (iguide->group == oguide->group)
                    {
                      widget_name = gtk_widget_get_name (iguide->widget);
                      type_name = G_OBJECT_TYPE_NAME (iguide->widget);

                      message = g_strdup_printf (
                        "Guide %d (%s%s%s)", i, type_name,
                        strcmp (type_name, widget_name) ? ": " : "",
                        strcmp (type_name, widget_name) ? widget_name : "");

                      if (test_case_compare_guides (test, oguide, iguide))
                        {
                          test_suite_report (self, message, oguide->group, TEST_RESULT_SUCCESS);
                        }
                      else
                        {
                          test_suite_report (self, message, oguide->group, TEST_RESULT_FAILURE);
                          group_result = TEST_RESULT_FAILURE;
                          test_result = TEST_RESULT_FAILURE;
                        }

                      g_free (message);
                    }
                } 

              test_suite_report (self, NULL, oguide->group, group_result);
            }
        }

      test_suite_report (self, NULL, -1, test_result);
      test_suite_stop (self);
    }
}

static void
test_current_cb (GtkWidget *widget,
                 gpointer  data)
{
  TestSuite *suite = data;
  test_suite_run (suite, -1);
}

static void
test_suite_show_and_run_test (TestSuite *self,
                              gint       page)
{
  GTimer *timer = g_timer_new ();

  gtk_notebook_set_current_page (GTK_NOTEBOOK (self->notebook), page);
  g_timer_start (timer);

  while (g_timer_elapsed (timer, NULL) < 0.3 &&
         !gtk_main_iteration_do (FALSE))
    {
      if (!gtk_events_pending ())
        g_usleep (500);
    }

  test_suite_run (self, -1);
  g_timer_destroy (timer);
}

static void
test_selected_cb (GtkWidget *widget,
                  gpointer   data)
{
  TestSuite *suite = data;
  GtkTreeModel *model;
  GtkTreeIter iter;

  model = GTK_TREE_MODEL (suite->tests);
  test_suite_start (suite);

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
        {
          gboolean selected = FALSE;
          gint page_index = -1;

          gtk_tree_model_get (model, &iter, 
                              TEST_COLUMN_SELECTED, &selected,
                              TEST_COLUMN_PAGE_INDEX, &page_index,
                              -1);

          if (page_index >= 0 && selected)
            test_suite_show_and_run_test (suite, page_index);
        }
      while (gtk_tree_model_iter_next (model, &iter));
    }

  test_suite_stop (suite);
}

static void
test_all_cb (GtkWidget *widget,
             gpointer  data)
{
  TestSuite *suite = data;
  gint i;

  test_suite_start (suite);

  for (i = 0; i < suite->n_test_cases; ++i)
    test_suite_show_and_run_test (suite, i);

  test_suite_stop (suite);
}

static void
switch_page_cb (GtkNotebook     *notebook,
                GtkNotebookPage *page,
                gint             index,
                gpointer         data)
{
  TestSuite *suite = data;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gint page_index;

  gtk_widget_set_sensitive (suite->test_current_button,
                            index < suite->n_test_cases);

  model = GTK_TREE_MODEL (suite->tests);

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
        {
          gtk_tree_model_get (model, &iter, 
                              TEST_COLUMN_PAGE_INDEX, &page_index,
                              -1);

          if (page_index == index)
            {
              gtk_tree_selection_select_iter (suite->selection, &iter);
              break;
            }
        }
      while (gtk_tree_model_iter_next (model, &iter));
    }
}

static GtkWidget*
find_widget_at_position (GtkWidget *widget,
                         gint       x,
                         gint       y)
{
  if (x < 0 || x >= widget->allocation.width ||
      y < 0 || y >= widget->allocation.height)
    return NULL;

  if (GTK_IS_CONTAINER (widget))
    {
      GtkWidget *child;
      GList *children;
      GList *iter;

      gint rx, ry;

      children = gtk_container_get_children (GTK_CONTAINER (widget));

      for (iter = children; iter; iter = iter->next)
        {
          gtk_widget_translate_coordinates (widget, iter->data, x, y, &rx, &ry);
          child = find_widget_at_position (iter->data, rx, ry);

          if (child)
            {
              widget = child;
              break;
            }
        }

      g_list_free (children);
    }

  return widget;
}

static void
queue_redraw (GtkWidget *page, 
              GtkWidget *child)
{
  gint x, y;

  gtk_widget_translate_coordinates (child, page, 0, 0, &x, &y);

  gtk_widget_queue_draw_area (page,
                              page->allocation.x,
                              page->allocation.y + y,
                              page->allocation.width,
                              child->allocation.height);
  gtk_widget_queue_draw_area (page,
                              page->allocation.x + x,
                              page->allocation.y,
                              child->allocation.width,
                              page->allocation.height);
}

static gboolean           
watch_pointer_cb (gpointer data)
{
  TestSuite *suite = data;
  GtkWidget *prev = suite->current;
  TestCase *test = NULL;

  gboolean dirty;
  GtkWidget *page;
  GtkWidget *child;
  gint i, x, y;

  i = gtk_notebook_get_current_page (GTK_NOTEBOOK (suite->notebook));
  page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (suite->notebook), i);

  gtk_widget_get_pointer (page, &x, &y);
  child = find_widget_at_position (page, x, y);

  while (child)
    {
      test = g_object_get_data (G_OBJECT(child), "test-case");

      if (test)
        break;

      child = gtk_widget_get_parent (child);
    }

  dirty = suite->current && !(suite->timestamp % 3);
  suite->timestamp = (suite->timestamp + 1) % 6;

  if (!test)
    {
      dirty = (NULL != suite->current);

      if (suite->current)
        gtk_label_set_text (GTK_LABEL (suite->statusbar),
                            "No widget selected.\n");

      suite->current = NULL;
    }
  else if (child != suite->hover)
    {
      if (child != suite->current)
        update_status (suite, child);

      suite->current = child;
      suite->hover = child;
      dirty = TRUE;
    }

  if (dirty)
    {
      if (suite->current)
        {
          if (prev && prev != suite->current)
            queue_redraw (page, prev);

          queue_redraw (page, suite->current);
        }
      else
        {
          gtk_widget_queue_draw (page);
        }
    }

  return TRUE;
}

static gboolean
button_press_event_cb (GtkWidget      *widget,
                       GdkEventButton *event,
                       gpointer        data)
{
  TestSuite *suite = data;
  GtkWidget *popup = NULL;
  GtkWidget *page;
  gint i;

  if (3 != event->button)
    return FALSE;

  i = gtk_notebook_get_current_page (GTK_NOTEBOOK (suite->notebook));
  page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (suite->notebook), i);
  popup = g_object_get_data (G_OBJECT (page), "popup");

  gtk_menu_popup (GTK_MENU (popup),
                  NULL, NULL, NULL, NULL,
                  event->button, event->time);

  return TRUE;
}

static void
test_suite_setup_results_page (TestSuite *self)
{
  GtkTreeViewColumn *column;
  GtkCellRenderer *cell;
  GtkWidget *scroller;

  self->results = gtk_tree_store_new (RESULT_COLUNN_COUNT,
                                      G_TYPE_STRING, PANGO_TYPE_WEIGHT,
                                      G_TYPE_STRING, G_TYPE_STRING);

  self->results_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (self->results));
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (self->results_view), FALSE);

  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_expand (column, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (self->results_view), column);

  cell = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (column, cell, FALSE);
  gtk_tree_view_column_set_attributes (column, cell, 
                                       "icon-name", RESULT_COLUMN_ICON, NULL);

  cell = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, cell, TRUE);
  gtk_tree_view_column_set_attributes (column, cell, 
                                       "text", RESULT_COLUMN_MESSAGE,
                                       "weight", RESULT_COLUMN_WEIGHT, NULL);

  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_expand (column, FALSE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (self->results_view), column);

  cell = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, cell, TRUE);
  gtk_tree_view_column_set_attributes (column, cell, 
                                       "text", RESULT_COLUMN_RESULT, NULL);

  scroller = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroller),
                                       GTK_SHADOW_IN);
  gtk_container_set_border_width (GTK_CONTAINER (scroller), 12);
  gtk_container_add (GTK_CONTAINER (scroller), self->results_view);

  test_suite_insert_page (self, NULL, scroller, "<b>Test Results</b>");

  g_signal_connect (self->notebook, "realize",
                    G_CALLBACK (realize_notebook_cb), self);
}

static gboolean
tests_is_separator (GtkTreeModel *model,
                    GtkTreeIter  *iter,
                    gpointer      data)
{
  gchar *label;

  gtk_tree_model_get (model, iter, TEST_COLUMN_LABEL, &label, -1);
  g_free (label);

  return (NULL == label);
}

static void
test_case_toggled (GtkCellRendererToggle *cell,
                   gchar                 *path,
                   gpointer               data)
{
  GtkTreeIter iter;

  if (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (data), &iter, path))
    gtk_list_store_set (GTK_LIST_STORE (data), &iter, TEST_COLUMN_SELECTED,
                        !gtk_cell_renderer_toggle_get_active (cell),
                        -1);
}

static void
selection_changed (GtkTreeSelection *selection,
                   gpointer          data)
{
  TestSuite *suite = data;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gint page_index;

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gtk_tree_model_get (model, &iter, 
                          TEST_COLUMN_PAGE_INDEX, &page_index, 
                          -1);

      if (page_index >= 0)
        gtk_notebook_set_current_page (GTK_NOTEBOOK (suite->notebook),
                                       page_index);
    }
}

static void
test_suite_setup_ui (TestSuite *self)
{
  GtkWidget *table, *actions, *button, *align;
  GtkWidget *view, *scrolled;

  GtkTreeViewColumn *column;
  GtkCellRenderer *cell;

  self->tests = gtk_list_store_new (TEST_COLUMN_COUNT, 
                                    G_TYPE_STRING, G_TYPE_BOOLEAN,
                                    G_TYPE_POINTER, G_TYPE_BOOLEAN, 
                                    G_TYPE_INT);

  view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (self->tests));
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (view), FALSE);
  gtk_tree_view_set_row_separator_func (GTK_TREE_VIEW (view), tests_is_separator, NULL, NULL);

  self->selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
  gtk_tree_selection_set_mode (self->selection, GTK_SELECTION_BROWSE);

  g_signal_connect (self->selection, "changed", G_CALLBACK (selection_changed), self);

  column = gtk_tree_view_column_new ();
  cell = gtk_cell_renderer_toggle_new ();
  gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), cell, FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (column), cell,
                                  "active", TEST_COLUMN_SELECTED, 
                                  "activatable", TEST_COLUMN_HAS_TEST_CASE,
                                  "visible", TEST_COLUMN_HAS_TEST_CASE,
                                  NULL);

  g_signal_connect (cell, "toggled", G_CALLBACK (test_case_toggled), self->tests);

  column = gtk_tree_view_column_new ();
  cell = gtk_cell_renderer_text_new ();
  gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), cell, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (column), cell,
                                  "markup", TEST_COLUMN_LABEL, NULL);

  scrolled = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled),
                                       GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_container_add (GTK_CONTAINER (scrolled), view);

  self->notebook = gtk_notebook_new ();
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (self->notebook), FALSE);

  actions = gtk_hbox_new (TRUE, 12);

  align = gtk_alignment_new (1.0, 0.5, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (align), actions);

  button = gtk_button_new_with_mnemonic ("Test _Current Page");
  g_signal_connect (button, "clicked", G_CALLBACK (test_current_cb), self);
  gtk_box_pack_start (GTK_BOX (actions), button, FALSE, TRUE, 0);

  self->test_current_button = button;
  g_signal_connect (self->notebook, "switch-page", G_CALLBACK (switch_page_cb), self);

  button = gtk_button_new_with_mnemonic ("Test _Selected Pages");
  g_signal_connect (button, "clicked", G_CALLBACK (test_selected_cb), self);
  gtk_box_pack_start (GTK_BOX (actions), button, FALSE, TRUE, 0);

  button = gtk_button_new_with_mnemonic ("Test _All Pages");
  g_signal_connect (button, "clicked", G_CALLBACK (test_all_cb), self);
  gtk_box_pack_start (GTK_BOX (actions), button, FALSE, TRUE, 0);

  actions = gtk_hbox_new (FALSE, 12);
  gtk_box_pack_end (GTK_BOX (actions), align, TRUE, TRUE, 0);

  gtk_box_pack_start (GTK_BOX (actions),
                      gtk_label_new ("Guides:"),
                      FALSE, TRUE, 0);

  self->baselines = gtk_check_button_new_with_mnemonic ("_Baselines");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->baselines), TRUE);
  gtk_box_pack_start (GTK_BOX (actions), self->baselines, FALSE, TRUE, 0);

  g_signal_connect_swapped (self->baselines, "toggled", 
                            G_CALLBACK (gtk_widget_queue_draw),
                            self->notebook);

  self->interiour = gtk_check_button_new_with_mnemonic ("_Interiours");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->interiour), TRUE);
  gtk_box_pack_start (GTK_BOX (actions), self->interiour, FALSE, TRUE, 0);

  g_signal_connect_swapped (self->interiour, "toggled", 
                            G_CALLBACK (gtk_widget_queue_draw),
                            self->notebook);
  
  self->exteriour = gtk_check_button_new_with_mnemonic ("_Exteriours");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->exteriour), TRUE);
  gtk_box_pack_start (GTK_BOX (actions), self->exteriour, FALSE, TRUE, 0);
  
  g_signal_connect_swapped (self->exteriour, "toggled", 
                            G_CALLBACK (gtk_widget_queue_draw),
                            self->notebook);

  self->statusbar = gtk_label_new ("No widget selected.\n");
  gtk_misc_set_alignment (GTK_MISC (self->statusbar), 0.0, 0.5);
  gtk_label_set_ellipsize (GTK_LABEL (self->statusbar),
                           PANGO_ELLIPSIZE_END);

  table = gtk_table_new (3, 2, FALSE);

  gtk_table_set_col_spacings (GTK_TABLE (table), 6);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_container_set_border_width (GTK_CONTAINER (table), 6);

  gtk_table_attach (GTK_TABLE (table), actions, 0, 2, 0, 1, GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);
  gtk_table_attach (GTK_TABLE (table), scrolled, 0, 1, 1, 2, GTK_FILL, GTK_FILL | GTK_EXPAND, 0, 0);
  gtk_table_attach (GTK_TABLE (table), self->notebook, 1, 2, 1, 2, GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
  gtk_table_attach (GTK_TABLE (table), self->statusbar, 0, 2, 2, 3, GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);

  self->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  g_object_connect (self->window, 
    "signal::button-press-event", button_press_event_cb, self,
    "signal::destroy", gtk_main_quit, NULL,
    NULL);

  g_timeout_add (200, watch_pointer_cb, self);

  gtk_window_set_title (GTK_WINDOW (self->window), "Testing GtkExtendedLayout");
  gtk_widget_add_events (self->window, GDK_BUTTON_PRESS_MASK);
  gtk_container_add (GTK_CONTAINER (self->window), table);
  gtk_widget_grab_focus (view);
}

static TestSuite*
test_suite_new (gchar *arg0)
{       
  TestSuite* self = g_new0 (TestSuite, 1);

  test_suite_setup_ui (self);

  test_suite_append (self, natural_size_test_new (self, FALSE, FALSE));
  test_suite_append (self, natural_size_test_new (self, TRUE, FALSE));
  test_suite_append (self, natural_size_test_new (self, FALSE, TRUE));
  test_suite_append (self, natural_size_test_new (self, TRUE, TRUE));
  test_suite_append (self, natural_size_test_misc_new (self, arg0));
  test_suite_append (self, size_for_allocation_test_new (self, TRUE, FALSE));
  test_suite_append (self, size_for_allocation_test_new (self, FALSE, FALSE));
  test_suite_append (self, size_for_allocation_test_new (self, TRUE, TRUE));
  test_suite_append (self, size_for_allocation_test_new (self, FALSE, TRUE));
  test_suite_append (self, baseline_test_new (self));
  test_suite_append (self, baseline_test_bin_new (self));
#if 0
  test_suite_append (self, baseline_test_hbox_new (self, FALSE));
  test_suite_append (self, baseline_test_hbox_new (self, TRUE));
#endif

  test_suite_setup_results_page (self);

  return self;
}

static gboolean
on_embedding_timeout (gpointer data)
{
  GdkNativeWindow plug_id = GPOINTER_TO_INT (data);
  g_printerr ("Embedding timeout expired for plug %d. Aborting.\n", plug_id);
  gtk_main_quit ();
  return FALSE;
}

static void
on_embedded (GtkWidget *widget, 
             gpointer   data)
{
  g_source_remove (GPOINTER_TO_INT (data));
}

static void
create_plug (gboolean ellipsize, 
             gboolean vertical)
{
  GtkWidget *plug, *label;
  GdkNativeWindow plug_id;
  guint timeout;


  label = gtk_label_new ("Hello World");
  
  if (ellipsize)
    gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  if (vertical)
    gtk_label_set_angle (GTK_LABEL (label), 90);

  plug = gtk_plug_new (0);
  gtk_container_add (GTK_CONTAINER (plug), label);
  gtk_widget_show_all (plug);

  plug_id = gtk_plug_get_id (GTK_PLUG (plug));
  timeout = g_timeout_add (5 * 1000, on_embedding_timeout,
                           GINT_TO_POINTER (plug_id));

  g_signal_connect (plug, "embedded", 
                    G_CALLBACK (on_embedded),
                    GINT_TO_POINTER (timeout));
  g_signal_connect (plug, "delete-event",
                    G_CALLBACK (gtk_main_quit),
                    NULL);

  g_print ("%d\n", plug_id);
}

int
main (int argc, char *argv[])
{
  GOptionContext *options;
  TestSuite *suite = NULL;
  GError *error = NULL;

  gboolean ellipsize = FALSE;
  gboolean vertical = FALSE;
  gint initial_page = 0;
  gchar *action = NULL;
  
  GOptionEntry entries[] = 
  {
    { "action", 'a', 0, G_OPTION_ARG_STRING, &action, "Action to perform", NULL },
    { "initial-page", 'p', 0, G_OPTION_ARG_INT, &initial_page, "Initial page of the test suite", NULL },
    { "ellipsize", 0, 0, G_OPTION_ARG_NONE, &ellipsize, "Add ellipses to labels", NULL },
    { "vertical", 0, 0, G_OPTION_ARG_NONE, &vertical, "Render vertical layout", NULL },
    { NULL }
  };

  gtk_init (&argc, &argv);

  options = g_option_context_new (NULL);
  g_option_context_add_main_entries (options, entries, NULL);
  g_option_context_add_group (options, gtk_get_option_group (TRUE));
  g_option_context_parse (options, &argc, &argv, &error);

  if (error)
    {
      g_print ("Usage Error: %s\n", error->message);
      return 2;
    }

  if (action && g_str_equal (action, "create-plug"))
    {
      create_plug (ellipsize, vertical);
    }
  else
    {
      suite = test_suite_new (argv[0]);
      gtk_widget_show_all (suite->window);

      gtk_notebook_set_current_page (GTK_NOTEBOOK (suite->notebook),
                                     initial_page);
    }

  gtk_main ();

  if (suite)
    test_suite_free (suite);

  return 0;
}

/* vim: set sw=2 sta et: */
