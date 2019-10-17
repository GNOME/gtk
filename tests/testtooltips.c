/* testtooltips.c: Test application for GTK+ >= 2.12 tooltips code
 *
 * Copyright (C) 2006-2007  Imendio AB
 * Contact: Kristian Rietveld <kris@imendio.com>
 *
 * This work is provided "as is"; redistribution and modification
 * in whole or in part, in any medium, physical or electronic is
 * permitted without restriction.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * In no event shall the authors or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 */

#include <gtk/gtk.h>

typedef struct _MyTooltip MyTooltip;
typedef struct _MyTooltipClass MyTooltipClass;


struct _MyTooltip
{
  GtkBin parent_instance;
};

struct _MyTooltipClass
{
  GtkBinClass parent_class;
};

G_DEFINE_TYPE (MyTooltip, my_tooltip, GTK_TYPE_BIN)

static void
my_tooltip_init (MyTooltip *tt)
{
  GtkWidget *label = gtk_label_new ("Some text in a tooltip");

  gtk_container_add (GTK_CONTAINER (tt), label);

  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (tt)), "background");
}

static void
my_tooltip_class_init (MyTooltipClass *tt_class)
{
  gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (tt_class), "tooltip");
}

static gboolean
query_tooltip_cb (GtkWidget  *widget,
		  gint        x,
		  gint        y,
		  gboolean    keyboard_tip,
		  GtkTooltip *tooltip,
		  gpointer    data)
{
  gtk_tooltip_set_markup (tooltip, gtk_button_get_label (GTK_BUTTON (widget)));
  gtk_tooltip_set_icon_from_icon_name (tooltip, "edit-delete");

  return TRUE;
}

static gboolean
query_tooltip_custom_cb (GtkWidget  *widget,
			 gint        x,
			 gint        y,
			 gboolean    keyboard_tip,
			 GtkTooltip *tooltip,
			 gpointer    data)
{
  return TRUE;
}

static gboolean
query_tooltip_text_view_cb (GtkWidget  *widget,
			    gint        x,
			    gint        y,
			    gboolean    keyboard_tip,
			    GtkTooltip *tooltip,
			    gpointer    data)
{
  GtkTextTag *tag = data;
  GtkTextIter iter;
  GtkTextView *text_view = GTK_TEXT_VIEW (widget);
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (text_view);

  if (keyboard_tip)
    {
      gint offset;

      g_object_get (buffer, "cursor-position", &offset, NULL);
      gtk_text_buffer_get_iter_at_offset (buffer, &iter, offset);
    }
  else
    {
      gint bx, by, trailing;

      gtk_text_view_window_to_buffer_coords (text_view, GTK_TEXT_WINDOW_TEXT,
					     x, y, &bx, &by);
      gtk_text_view_get_iter_at_position (text_view, &iter, &trailing, bx, by);
    }

  if (gtk_text_iter_has_tag (&iter, tag))
    gtk_tooltip_set_text (tooltip, "Tooltip on text tag");
  else
   return FALSE;

  return TRUE;
}

static gboolean
query_tooltip_tree_view_cb (GtkWidget  *widget,
			    gint        x,
			    gint        y,
			    gboolean    keyboard_tip,
			    GtkTooltip *tooltip,
			    gpointer    data)
{
  GtkTreeIter iter;
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
  GtkTreeModel *model = gtk_tree_view_get_model (tree_view);
  GtkTreePath *path = NULL;
  gchar *tmp;
  gchar *pathstring;

  char buffer[512];

  if (!gtk_tree_view_get_tooltip_context (tree_view, &x, &y,
					  keyboard_tip,
					  &model, &path, &iter))
    return FALSE;

  gtk_tree_model_get (model, &iter, 0, &tmp, -1);
  pathstring = gtk_tree_path_to_string (path);

  g_snprintf (buffer, 511, "<b>Path %s:</b> %s", pathstring, tmp);
  gtk_tooltip_set_markup (tooltip, buffer);

  gtk_tree_view_set_tooltip_row (tree_view, tooltip, path);

  gtk_tree_path_free (path);
  g_free (pathstring);
  g_free (tmp);

  return TRUE;
}

static GtkTreeModel *
create_model (void)
{
  GtkTreeStore *store;
  GtkTreeIter iter;

  store = gtk_tree_store_new (1, G_TYPE_STRING);

  /* A tree store with some random words ... */
  gtk_tree_store_insert_with_values (store, &iter, NULL, 0,
				     0, "File Manager", -1);
  gtk_tree_store_insert_with_values (store, &iter, NULL, 0,
				     0, "Gossip", -1);
  gtk_tree_store_insert_with_values (store, &iter, NULL, 0,
				     0, "System Settings", -1);
  gtk_tree_store_insert_with_values (store, &iter, NULL, 0,
				     0, "The GIMP", -1);
  gtk_tree_store_insert_with_values (store, &iter, NULL, 0,
				     0, "Terminal", -1);
  gtk_tree_store_insert_with_values (store, &iter, NULL, 0,
				     0, "Word Processor", -1);

  return GTK_TREE_MODEL (store);
}

static void
selection_changed_cb (GtkTreeSelection *selection,
		      GtkWidget        *tree_view)
{
  gtk_widget_trigger_tooltip_query (tree_view);
}

static struct Rectangle
{
  gint x;
  gint y;
  gfloat r;
  gfloat g;
  gfloat b;
  const char *tooltip;
}
rectangles[] =
{
  { 10, 10, 0.0, 0.0, 0.9, "Blue box!" },
  { 200, 170, 1.0, 0.0, 0.0, "Red thing" },
  { 100, 50, 0.8, 0.8, 0.0, "Yellow thing" }
};

static gboolean
query_tooltip_drawing_area_cb (GtkWidget  *widget,
			       gint        x,
			       gint        y,
			       gboolean    keyboard_tip,
			       GtkTooltip *tooltip,
			       gpointer    data)
{
  gint i;

  if (keyboard_tip)
    return FALSE;

  for (i = 0; i < G_N_ELEMENTS (rectangles); i++)
    {
      struct Rectangle *r = &rectangles[i];

      if (r->x < x && x < r->x + 50
	  && r->y < y && y < r->y + 50)
        {
	  gtk_tooltip_set_markup (tooltip, r->tooltip);
	  return TRUE;
	}
    }

  return FALSE;
}

static void
drawing_area_draw (GtkDrawingArea *drawing_area,
		   cairo_t        *cr,
                   int             width,
                   int             height,
		   gpointer        data)
{
  gint i;

  cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
  cairo_paint (cr);

  for (i = 0; i < G_N_ELEMENTS (rectangles); i++)
    {
      struct Rectangle *r = &rectangles[i];

      cairo_rectangle (cr, r->x, r->y, 50, 50);
      cairo_set_source_rgb (cr, r->r, r->g, r->b);
      cairo_stroke (cr);

      cairo_rectangle (cr, r->x, r->y, 50, 50);
      cairo_set_source_rgba (cr, r->r, r->g, r->b, 0.5);
      cairo_fill (cr);
    }
}

static gboolean
query_tooltip_label_cb (GtkWidget  *widget,
			gint        x,
			gint        y,
			gboolean    keyboard_tip,
			GtkTooltip *tooltip,
			gpointer    data)
{
  GtkWidget *custom = data;

  gtk_tooltip_set_custom (tooltip, custom);

  return TRUE;
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *drawing_area;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *tooltip;

  GtkWidget *tooltip_window;
  GtkWidget *tooltip_button;

  GtkWidget *tree_view;
  GtkTreeViewColumn *column;

  GtkWidget *text_view;
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextTag *tag;

  gchar *text, *markup;

  gtk_init ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Tooltips test");
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_container_add (GTK_CONTAINER (window), box);

  tooltip = g_object_new (my_tooltip_get_type (), NULL);
  gtk_widget_set_margin_top (tooltip, 20);
  gtk_widget_set_margin_bottom (tooltip, 20);
  gtk_widget_set_halign (tooltip, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (box), tooltip);


  /* A check button using the tooltip-markup property */
  button = gtk_check_button_new_with_label ("This one uses the tooltip-markup property");
  gtk_widget_set_tooltip_text (button, "Hello, I am a static tooltip.");
  gtk_container_add (GTK_CONTAINER (box), button);

  text = gtk_widget_get_tooltip_text (button);
  markup = gtk_widget_get_tooltip_markup (button);
  g_assert (g_str_equal ("Hello, I am a static tooltip.", text));
  g_assert (g_str_equal ("Hello, I am a static tooltip.", markup));
  g_free (text); g_free (markup);

  /* A check button using the query-tooltip signal */
  button = gtk_check_button_new_with_label ("I use the query-tooltip signal");
  g_object_set (button, "has-tooltip", TRUE, NULL);
  g_signal_connect (button, "query-tooltip",
		    G_CALLBACK (query_tooltip_cb), NULL);
  gtk_container_add (GTK_CONTAINER (box), button);

  /* A label */
  button = gtk_label_new ("I am just a label");
  gtk_label_set_selectable (GTK_LABEL (button), FALSE);
  gtk_widget_set_tooltip_text (button, "Label & and tooltip");
  gtk_container_add (GTK_CONTAINER (box), button);

  text = gtk_widget_get_tooltip_text (button);
  markup = gtk_widget_get_tooltip_markup (button);
  g_assert (g_str_equal ("Label & and tooltip", text));
  g_assert (g_str_equal ("Label &amp; and tooltip", markup));
  g_free (text); g_free (markup);

  /* A selectable label */
  button = gtk_label_new ("I am a selectable label");
  gtk_label_set_selectable (GTK_LABEL (button), TRUE);
  gtk_widget_set_tooltip_markup (button, "<b>Another</b> Label tooltip");
  gtk_container_add (GTK_CONTAINER (box), button);

  text = gtk_widget_get_tooltip_text (button);
  markup = gtk_widget_get_tooltip_markup (button);
  g_assert (g_str_equal ("Another Label tooltip", text));
  g_assert (g_str_equal ("<b>Another</b> Label tooltip", markup));
  g_free (text); g_free (markup);

  /* Another one, with a custom tooltip window */
  button = gtk_check_button_new_with_label ("This one has a custom tooltip window!");
  gtk_container_add (GTK_CONTAINER (box), button);

  tooltip_window = gtk_window_new (GTK_WINDOW_POPUP);
  tooltip_button = gtk_label_new ("blaat!");
  gtk_container_add (GTK_CONTAINER (tooltip_window), tooltip_button);
  gtk_widget_show (tooltip_button);

  gtk_widget_set_tooltip_window (button, GTK_WINDOW (tooltip_window));
  gtk_window_set_type_hint (GTK_WINDOW (tooltip_window),
                            GDK_SURFACE_TYPE_HINT_TOOLTIP);
  gtk_window_set_transient_for (GTK_WINDOW (tooltip_window),
                                GTK_WINDOW (window));

  g_signal_connect (button, "query-tooltip",
		    G_CALLBACK (query_tooltip_custom_cb), NULL);
  g_object_set (button, "has-tooltip", TRUE, NULL);

  /* An insensitive button */
  button = gtk_button_new_with_label ("This one is insensitive");
  gtk_widget_set_sensitive (button, FALSE);
  g_object_set (button, "tooltip-text", "Insensitive!", NULL);
  gtk_container_add (GTK_CONTAINER (box), button);

  /* Testcases from Kris without a tree view don't exist. */
  tree_view = gtk_tree_view_new_with_model (create_model ());
  gtk_widget_set_size_request (tree_view, 200, 240);

  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
					       0, "Test",
					       gtk_cell_renderer_text_new (),
					       "text", 0,
					       NULL);

  g_object_set (tree_view, "has-tooltip", TRUE, NULL);
  g_signal_connect (tree_view, "query-tooltip",
		    G_CALLBACK (query_tooltip_tree_view_cb), NULL);
  g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view)),
		    "changed", G_CALLBACK (selection_changed_cb), tree_view);

  /* Set a tooltip on the column */
  column = gtk_tree_view_get_column (GTK_TREE_VIEW (tree_view), 0);
  gtk_tree_view_column_set_clickable (column, TRUE);
  g_object_set (gtk_tree_view_column_get_button (column), "tooltip-text", "Header", NULL);

  gtk_container_add (GTK_CONTAINER (box), tree_view);

  /* And a text view for Matthias */
  buffer = gtk_text_buffer_new (NULL);

  gtk_text_buffer_get_end_iter (buffer, &iter);
  gtk_text_buffer_insert (buffer, &iter, "Hello, the text ", -1);

  tag = gtk_text_buffer_create_tag (buffer, "bold", NULL);
  g_object_set (tag, "weight", PANGO_WEIGHT_BOLD, NULL);

  gtk_text_buffer_get_end_iter (buffer, &iter);
  gtk_text_buffer_insert_with_tags (buffer, &iter, "in bold", -1, tag, NULL);

  gtk_text_buffer_get_end_iter (buffer, &iter);
  gtk_text_buffer_insert (buffer, &iter, " has a tooltip!", -1);

  text_view = gtk_text_view_new_with_buffer (buffer);
  gtk_widget_set_size_request (text_view, 200, 50);

  g_object_set (text_view, "has-tooltip", TRUE, NULL);
  g_signal_connect (text_view, "query-tooltip",
		    G_CALLBACK (query_tooltip_text_view_cb), tag);

  gtk_container_add (GTK_CONTAINER (box), text_view);

  /* Drawing area */
  drawing_area = gtk_drawing_area_new ();
  gtk_drawing_area_set_content_width (GTK_DRAWING_AREA (drawing_area), 320);
  gtk_drawing_area_set_content_height (GTK_DRAWING_AREA (drawing_area), 240);
  gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (drawing_area),
                                  drawing_area_draw, NULL, NULL);
  g_object_set (drawing_area, "has-tooltip", TRUE, NULL);
  g_signal_connect (drawing_area, "query-tooltip",
		    G_CALLBACK (query_tooltip_drawing_area_cb), NULL);
  gtk_container_add (GTK_CONTAINER (box), drawing_area);

  button = gtk_label_new ("Custom tooltip I");
  label = gtk_label_new ("See, custom");
  g_object_ref_sink (label);
  g_object_set (button, "has-tooltip", TRUE, NULL);
  g_signal_connect (button, "query-tooltip",
		    G_CALLBACK (query_tooltip_label_cb), label);
  gtk_container_add (GTK_CONTAINER (box), button);

  button = gtk_label_new ("Custom tooltip II");
  label = gtk_label_new ("See, custom, too");
  g_object_ref_sink (label);
  g_object_set (button, "has-tooltip", TRUE, NULL);
  gtk_container_add (GTK_CONTAINER (box), button);
  g_signal_connect (button, "query-tooltip",
		    G_CALLBACK (query_tooltip_label_cb), label);

  /* Done! */
  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
