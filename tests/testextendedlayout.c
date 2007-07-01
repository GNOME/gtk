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
#include <string.h>
#include <stdarg.h>

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
  COLUMN_MESSAGE,
  COLUMN_WEIGHT,
  COLUMN_ICON,
  COLUMN_RESULT,
  COLUNN_COUNT
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
  GtkWidget *window;
  GtkWidget *notebook;
  GtkWidget *baselines;
  GtkWidget *interiour;
  GtkWidget *exteriour;
  GtkWidget *statusbar;

  GtkTreeStore *results;
  GtkWidget *results_view;
  GtkTreeIter parent;
  gint n_test_cases;
  gint level;

  GdkPixmap *tile;
  GtkWidget *current;
  gint timestamp;
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
test_case_append_guide (TestCase  *self,
                        GtkWidget *widget,
                        GuideType  type,
                        gint       group)
{
  Guide *guide = guide_new (widget, type, group);
  self->guides = g_list_append (self->guides, guide);
  g_object_set_data (G_OBJECT (widget), "test-case", self);
}

static void
append_natural_size_box (TestCase           *test,
                         const gchar        *caption,
                         PangoEllipsizeMode  ellipsize)
{
  GtkWidget *hbox;
  GtkWidget *button;
  GtkWidget *label;

  hbox = gtk_hbox_new (FALSE, 12);

  label = gtk_label_new ("The small Button");
  gtk_label_set_ellipsize (GTK_LABEL (label), ellipsize);

  button = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (button), label);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, TRUE, 0);
  test_case_append_guide (test, button, GUIDE_EXTERIOUR_VERTICAL, 0);

  button = gtk_button_new_with_label ("The large Button");
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
  test_case_append_guide (test, button, GUIDE_EXTERIOUR_VERTICAL, 1);

  label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label), caption); 
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

  gtk_box_pack_start (GTK_BOX (test->widget), label, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (test->widget), hbox, FALSE, TRUE, 0);

}

static TestCase*
create_natural_size_test (TestSuite *suite)
{
  TestCase *test = test_case_new (suite, "Natural Size", NULL,
                                  gtk_vbox_new (FALSE, 12));

  gtk_container_set_border_width (GTK_CONTAINER (test->widget), 12);

  append_natural_size_box (test,
                           "<b>No ellipsizing</b>",
                           PANGO_ELLIPSIZE_NONE);
  append_natural_size_box (test,
                           "<b>Ellipsizing at start</b>",
                           PANGO_ELLIPSIZE_START);
  append_natural_size_box (test,
                           "<b>Ellipsizing in the middle</b>",
                           PANGO_ELLIPSIZE_MIDDLE);
  append_natural_size_box (test,
                           "<b>Ellipsizing at end</b>",
                           PANGO_ELLIPSIZE_END);

  return test;
}

static TestCase*
create_height_for_width_test (TestSuite *suite)
{
  PangoLayout *layout;
  PangoRectangle log;
  GtkWidget *child;

  TestCase *test = test_case_new (suite, "Height for Width", NULL,
                                  gtk_hbox_new (FALSE, 12));

  gtk_container_set_border_width (GTK_CONTAINER (test->widget), 12);

  child = gtk_label_new (lorem_ipsum);
  gtk_label_set_line_wrap (GTK_LABEL (child), TRUE);
  gtk_label_set_use_markup (GTK_LABEL (child), TRUE);
  gtk_box_pack_start (GTK_BOX (test->widget), child, TRUE, TRUE, 0);
  layout = gtk_label_get_layout (GTK_LABEL (child));

  pango_layout_get_pixel_extents (layout, NULL, &log);
  gtk_widget_set_size_request (test->widget, log.width * 3 / 2, -1);

  test_case_append_guide (test, child, GUIDE_INTERIOUR_BOTH, 0);
  test_case_append_guide (test, child, GUIDE_EXTERIOUR_BOTH, 0);
  test_case_append_guide (test, child, GUIDE_BASELINE, 0);

  child = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (child),
                     gtk_image_new_from_stock (GTK_STOCK_DIALOG_INFO,
                                               GTK_ICON_SIZE_DIALOG));
  gtk_box_pack_start (GTK_BOX (test->widget), child, FALSE, TRUE, 0);

  return test;
}

static TestCase*
create_baseline_test (TestSuite *suite)
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
create_baseline_test_bin (TestSuite *suite)
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

  TestCase *test = test_case_new (suite, "Baseline Alignment", "GtkBin",
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

static TestCase*
create_baseline_test_hbox (TestSuite *suite)
{
  GtkWidget *bin;
  GtkWidget *label;
  GtkWidget *table;
  GtkWidget *hbox;

  int i, j;

  const gchar *names[] = 
    {
      "default", "baseline", "baseline and bottom-padding", 
      "baseline and top-padding", "baseline and border-width",
      NULL
    };

  TestCase *test = test_case_new (suite, "Baseline Alignment", "GtkHBox",
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
      label = gtk_label_new (names[i]);
      gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  
      hbox = gtk_hbox_new (FALSE, 6);
      test_case_append_guide (test, hbox, GUIDE_EXTERIOUR_BOTH, -1);
      g_object_set_data (G_OBJECT (hbox), "debug-wanted", GINT_TO_POINTER (TRUE));
      gtk_widget_set_name (hbox, names[i]);

      if (i > 0)
        gtk_hbox_set_baseline_policy (GTK_HBOX (hbox), GTK_BASELINE_FIRST);

      gtk_table_attach (GTK_TABLE (table), label,
                        0, 1, i, i + 1,
                        GTK_FILL, GTK_FILL, 0, 0);
      gtk_table_attach (GTK_TABLE (table), hbox,
                        1, G_N_ELEMENTS (captions), i, i + 1,
                        GTK_FILL, GTK_FILL, 0, 0);

      for (j = 0; captions[j]; ++j)
        {
          label = gtk_label_new (NULL);
          gtk_label_set_markup (GTK_LABEL (label), captions[j]);
          test_case_append_guide (test, label, GUIDE_BASELINE, i);

          if (0 == j && i > 1)
            {
              bin = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);

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

              gtk_container_add (GTK_CONTAINER (bin), label);
              gtk_box_pack_start (GTK_BOX (hbox), bin, FALSE, TRUE, 0);
              test_case_append_guide (test, bin, GUIDE_BASELINE, i);
            }
          else
            {
              gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
            }
        }
    }

  return test;
}

static gboolean
get_extends (GtkWidget    *widget,
             GtkWidget    *toplevel,
             GdkRectangle *extends)
{
  *extends = widget->allocation;

  return
    GTK_WIDGET_VISIBLE (widget) &&
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
  if (GTK_IS_EXTENDED_LAYOUT (widget) &&
      GTK_EXTENDED_LAYOUT_HAS_BASELINES (widget))
    return gtk_extended_layout_get_baselines (GTK_EXTENDED_LAYOUT (widget), baselines);
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
  gdk_draw_line (drawable, gc, xa + 2, ya, xe - 2, ya);
  gdk_draw_line (drawable, gc, xe, ya - 5, xe, ya + 2);

  gdk_gc_set_line_attributes (gc, 1, GDK_LINE_ON_OFF_DASH,
                              GDK_CAP_NOT_LAST, GDK_JOIN_MITER);

  gdk_draw_line (drawable, gc, x0, ya, xa - 2, ya);
  gdk_draw_line (drawable, gc, xe + 2, ya, x0 + cx - 1, ya);

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
guide_type_get_color (GuideType type)
{
  switch (type) 
    {
      case GUIDE_BASELINE: return "#f00";
      default: return "#ff0";
    }
}

static gboolean
draw_guides (gpointer data)
{
  TestCase *test = data;
  GdkDrawable *drawable;

  const GList *iter;

  gint8 dashes[] = { 3, 3 };
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
  gdk_gc_set_dashes (gc, 1, dashes, 2);

  show_baselines =
    test->suite->baselines && gtk_toggle_button_get_active (
    GTK_TOGGLE_BUTTON (test->suite->baselines));
  show_interiour =
    test->suite->interiour && gtk_toggle_button_get_active (
    GTK_TOGGLE_BUTTON (test->suite->interiour));;
  show_exteriour = 
    test->suite->exteriour && gtk_toggle_button_get_active (
    GTK_TOGGLE_BUTTON (test->suite->exteriour));;

  for (iter = test->guides; iter; iter = iter->next)
    {
      const Guide *guide = iter->data;
      GdkRectangle extends;
      gint num_baselines;
      gint *baselines;
      gint i;

      if (guide->widget == test->suite->current)
        {
          if (test->suite->timestamp < 3)
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
            }
          else
            {
              continue;
            }
        }

      gdk_color_parse (guide_type_get_color (guide->type), 
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
test_suite_append (TestSuite *self,
                   TestCase  *test)
{
  GtkWidget *label;
  GString *markup;

  markup = g_string_new (test->name);

  if (test->detail)
    {
      g_string_append (markup, "\n<small>(");
      g_string_append (markup, test->detail);
      g_string_append (markup, ")</small>");
    }

  label = gtk_label_new (markup->str);
  g_string_free (markup, TRUE);

  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);

  gtk_notebook_insert_page (GTK_NOTEBOOK (self->notebook), test->widget,
                            label, self->n_test_cases++);

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
                   TestResult   result)
{
  const gchar *text = test_result_to_string (result);
  const gchar *icon = test_result_to_icon (result);

  PangoWeight weight;
  GtkTreePath *path;
  GtkTreeIter iter;

  if (TEST_RESULT_NONE == result)
    {
      g_print ("\033[1mTesting: %s\033[0m\n", message);
      gtk_tree_store_append (self->results, &self->parent, NULL);
      weight = PANGO_WEIGHT_BOLD;
      iter = self->parent;
    }
  else
    {
      g_print (" * %s: %s\n", message, text);
      gtk_tree_store_append (self->results, &iter, &self->parent);
      weight = PANGO_WEIGHT_NORMAL;
    }

  gtk_tree_store_set (self->results, &iter, 
                      COLUMN_MESSAGE, message, 
                      COLUMN_WEIGHT, weight, 
                      COLUMN_RESULT, text, 
                      COLUMN_ICON, icon,
                      -1);

  path = gtk_tree_model_get_path (GTK_TREE_MODEL (self->results), &iter);
  gtk_tree_view_expand_to_path (GTK_TREE_VIEW (self->results_view), path);
  gtk_tree_path_free (path);
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
      gint last_group = -1;
      GList *oiter;
      gint o;

      test_suite_start (self);
      test_suite_report (self, test->name, TEST_RESULT_NONE);

      for(o = 0, oiter = test->guides; oiter; ++o, oiter = oiter->next)
        {
          const Guide *oguide = oiter->data;
        
          if (oguide->group > last_group)
            {
              GList *iiter;
              gint i;

              for(i = 0, iiter = test->guides; iiter; ++i, iiter = iiter->next)
                {
                  const Guide *iguide = iiter->data;

                  if (iguide->group == oguide->group)
                    {
                      gchar *message = g_strdup_printf (
                        "Group %d: Guide %d (%s) vs %d (%s)", oguide->group, 
                        o, G_OBJECT_TYPE_NAME (oguide->widget),
                        i, G_OBJECT_TYPE_NAME (iguide->widget));

                      if (test_case_compare_guides (test, oguide, iguide))
                        test_suite_report (self, message, TEST_RESULT_SUCCESS);
                      else
                        test_suite_report (self, message, TEST_RESULT_FAILURE);

                      g_free (message);
                    }
                } 

              last_group = oguide->group;
            }

        }

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
test_all_cb (GtkWidget *widget,
             gpointer  data)
{
  GTimer *timer = g_timer_new ();
  TestSuite *suite = data;
  gint i;

  test_suite_start (suite);

  for (i = 0; i < suite->n_test_cases; ++i)
    {
      gtk_notebook_set_current_page (GTK_NOTEBOOK (suite->notebook), i);
      g_timer_start (timer);

      while (g_timer_elapsed (timer, NULL) < 0.3 &&
             !gtk_main_iteration_do (FALSE))
        {
          if (!gtk_events_pending ())
            g_usleep (500);
        }

      test_current_cb (widget, suite);
      g_timer_stop (timer);
    }

  test_suite_stop (suite);
  g_timer_destroy (timer);
}

static void
switch_page_cb (GtkNotebook     *notebook,
                GtkNotebookPage *page,
                gint             index,
                gpointer         data)
{
  gpointer *bag = data;
  TestSuite *suite = bag[0];
  GtkWidget *button = bag[1];

  gtk_widget_set_sensitive (button, index < suite->n_test_cases);
}

static gpointer
pointer_bag_new (gpointer first, ...)
{
  gpointer *self;
  gint count = 0;
  gpointer ptr;
  va_list args;

  va_start (args, first);

  for (ptr = first, count = 0; ptr; ptr = va_arg (args, gpointer))
    ++count;

  va_end (args);

  self = g_new0 (gpointer, count + 1);

  va_start (args, first);

  for (ptr = first, count = 0; ptr; ptr = va_arg (args, gpointer))
    self[count++] = ptr;

  va_end (args);

  return self;
}

static void 
pointer_bag_free (gpointer self)
{
  g_free (self);
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
update_status (TestSuite *suite,
               GtkWidget *child)
{
  const gchar *widget_name = gtk_widget_get_name (child);
  const gchar *type_name = G_OBJECT_TYPE_NAME (child);
  GString *status = g_string_new (widget_name);

  if (strcmp (widget_name, type_name))
    g_string_append_printf (status, " (%s)", type_name);

  g_string_append_printf (status,
                          ": pos=%dx%d; size=%dx%d",
                          child->allocation.x,
                          child->allocation.y,
                          child->allocation.width,
                          child->allocation.height);

  if (GTK_IS_EXTENDED_LAYOUT (child))
    {
      if (GTK_EXTENDED_LAYOUT_HAS_BASELINES (child))
        {
          gint *baselines = NULL;
          gint num_baselines = 0;
          gint i;

          num_baselines =
            gtk_extended_layout_get_baselines (GTK_EXTENDED_LAYOUT (child), 
                                               &baselines);

          for (i = 0; i < num_baselines; ++i)
            {
              g_string_append_printf (status, "%s%d",
                                      i ? ", " : num_baselines > 1 ?
                                      "; baselines: " : "; baseline: ",
                                      baselines[i]);
            }

          g_free (baselines);
        }

      if (GTK_EXTENDED_LAYOUT_HAS_PADDING (child))
        {
          GtkBorder padding;

          gtk_extended_layout_get_padding (GTK_EXTENDED_LAYOUT (child),
                                           &padding);

          g_string_append_printf (status, "; padding: %d/%d/%d/%d",
                                  padding.top, padding.left,
                                  padding.right, padding.bottom);
        }
    }

  gtk_label_set_text (GTK_LABEL (suite->statusbar), status->str);
  g_string_free (status, TRUE);
}

static gboolean           
watch_pointer_cb (gpointer data)
{
  TestSuite *suite = data;
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

  if (test)
    {
      g_assert (child);

      if (child != suite->current)
        update_status (suite, child);

      suite->current = child;
      dirty = TRUE;
    }
  else
    {
      dirty = (NULL != suite->current);

      if (suite->current)
        gtk_label_set_text (GTK_LABEL (suite->statusbar),
                            "No widget selected.");

      suite->current = NULL;
    }

  if (dirty)
    {
      if (suite->current)
        {
          gtk_widget_translate_coordinates (suite->current, page, 0, 0, &x, &y);

          gtk_widget_queue_draw_area (page,
                                      page->allocation.x,
                                      page->allocation.y + y,
                                      page->allocation.width,
                                      suite->current->allocation.height);
          gtk_widget_queue_draw_area (page,
                                      page->allocation.x + x,
                                      page->allocation.y,
                                      suite->current->allocation.width,
                                      page->allocation.height);
        }
      else
        {
          gtk_widget_queue_draw (page);
        }
    }

  return TRUE;
}

static void
test_suite_setup_results_page (TestSuite *self)
{
  GtkTreeViewColumn *column;
  GtkCellRenderer *cell;
  GtkWidget *scroller;

  self->results = gtk_tree_store_new (COLUNN_COUNT,
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
                                       "icon-name", COLUMN_ICON, NULL);

  cell = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, cell, TRUE);
  gtk_tree_view_column_set_attributes (column, cell, 
                                       "text", COLUMN_MESSAGE,
                                       "weight", COLUMN_WEIGHT, NULL);

  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_expand (column, FALSE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (self->results_view), column);

  cell = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, cell, TRUE);
  gtk_tree_view_column_set_attributes (column, cell, 
                                       "text", COLUMN_RESULT, NULL);

  scroller = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroller),
                                       GTK_SHADOW_IN);
  gtk_container_set_border_width (GTK_CONTAINER (scroller), 12);
  gtk_container_add (GTK_CONTAINER (scroller), self->results_view);

  gtk_notebook_append_page (GTK_NOTEBOOK (self->notebook),
                            scroller, gtk_label_new ("Results"));

  g_signal_connect (self->notebook, "realize",
                    G_CALLBACK (realize_notebook_cb), self);
}

static void
test_suite_setup_ui (TestSuite *self)
{
  GtkWidget *actions;
  GtkWidget *button;
  GtkWidget *align;
  GtkWidget *vbox;

  self->notebook = gtk_notebook_new ();
  gtk_notebook_set_tab_pos (GTK_NOTEBOOK (self->notebook), GTK_POS_RIGHT);

  actions = gtk_hbox_new (FALSE, 12);

  align = gtk_alignment_new (1.0, 0.5, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (align), actions);

  button = gtk_button_new_with_mnemonic ("Test _Current Page");
  g_signal_connect (button, "clicked", G_CALLBACK (test_current_cb), self);
  gtk_box_pack_start (GTK_BOX (actions), button, FALSE, TRUE, 0);

  g_signal_connect_data (self->notebook, "switch-page",
                         G_CALLBACK (switch_page_cb),
                         pointer_bag_new (self, button, NULL),
                         (GClosureNotify) pointer_bag_free, 0);

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

  self->statusbar = gtk_label_new ("No widget selected.");
  gtk_label_set_ellipsize (GTK_LABEL (self->statusbar),
                           PANGO_ELLIPSIZE_END);

  vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
  gtk_box_pack_start (GTK_BOX (vbox), actions, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), self->notebook, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), self->statusbar, FALSE, TRUE, 0);

  self->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  g_signal_connect (self->window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  g_timeout_add (200, watch_pointer_cb, self);

  gtk_window_set_title (GTK_WINDOW (self->window), "Testing GtkExtendedLayout");
  gtk_container_add (GTK_CONTAINER (self->window), vbox);
  gtk_widget_grab_focus (self->notebook);
}

static TestSuite*
test_suite_new ()
{       
  TestSuite* self = g_new0 (TestSuite, 1);

  test_suite_setup_ui (self);
  test_suite_append (self, create_natural_size_test (self));
  test_suite_append (self, create_height_for_width_test (self));
  test_suite_append (self, create_baseline_test (self));
  test_suite_append (self, create_baseline_test_bin (self));
  test_suite_append (self, create_baseline_test_hbox (self));
  test_suite_setup_results_page (self);

  return self;
}

int
main (int argc, char *argv[])
{
  TestSuite *suite;

  gtk_init (&argc, &argv);

  suite = test_suite_new ();
  gtk_widget_show_all (suite->window);

  gtk_main ();

  test_suite_free (suite);

  return 0;
}

/* vim: set sw=2 sta et: */
