/* Constraints/Words
 *
 * GtkConstraintLayout lets you define big grids.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>


G_DECLARE_FINAL_TYPE (WordGrid, word_grid, WORD, GRID, GtkWidget)

struct _WordGrid
{
  GtkWidget parent_instance;

  GPtrArray *children;
};

G_DEFINE_TYPE (WordGrid, word_grid, GTK_TYPE_WIDGET)

static void
word_grid_destroy (GtkWidget *widget)
{
  WordGrid *self = WORD_GRID (widget);

  if (self->children)
    {
      g_ptr_array_free (self->children, TRUE);
      self->children = NULL;
    }

  GTK_WIDGET_CLASS (word_grid_parent_class)->destroy (widget);
}

static void
word_grid_class_init (WordGridClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->destroy = word_grid_destroy;

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_CONSTRAINT_LAYOUT);
}

static void
read_words (WordGrid *self)
{
  GBytes *data;
  const char *words;
  int left, top;
  GtkGridConstraint *grid;
  GtkConstraint *constraint;
  GtkConstraintLayout *layout;
  GtkWidget *child;
  GtkWidget *last_child;

  layout = GTK_CONSTRAINT_LAYOUT (gtk_widget_get_layout_manager (GTK_WIDGET (self)));

  grid = gtk_grid_constraint_new ();
  g_object_set (grid,
                "row-homogeneous", TRUE,
                "column-homogeneous", TRUE,
                NULL);

  data = g_resources_lookup_data ("/constraints3/words", 0, NULL);
  words = g_bytes_get_data (data, NULL);

  left = 0;
  top = 0;
  while (words && words[0])
    {
      char *p = strchr (words, '\n');
      char *word;
      int len;

      if (p)
        {
          word = strndup (words, p - words);
          words = p + 1;
        }
      else
        {
          word = strdup (words);
          words = NULL;
        }

      len = strlen (word);
      child = gtk_button_new_with_label (word);
      if (left + len > 50)
        {
          top++;
          left = 0;
        }

      last_child = child;
      gtk_widget_set_parent (child, GTK_WIDGET (self));
      gtk_grid_constraint_add (grid, child,
                               left, left + len,
                               top, top + 1);
      left = left + len;

      //g_print ("%d %d %d %d %s\n", left, left + len, top, top + 1, word);
      if (top == 0)
        {
          constraint = gtk_constraint_new (NULL,
                                      GTK_CONSTRAINT_ATTRIBUTE_TOP,
                                      GTK_CONSTRAINT_RELATION_EQ,
                                      child,
                                      GTK_CONSTRAINT_ATTRIBUTE_TOP,
                                      1.0,
                                      0.0,
                                      GTK_CONSTRAINT_STRENGTH_REQUIRED);
          gtk_constraint_layout_add_constraint (layout, constraint);
        }
      if (left == 0)
        {
          constraint = gtk_constraint_new (NULL,
                                      GTK_CONSTRAINT_ATTRIBUTE_RIGHT,
                                      GTK_CONSTRAINT_RELATION_EQ,
                                      last_child,
                                      GTK_CONSTRAINT_ATTRIBUTE_RIGHT,
                                      1.0,
                                      0.0,
                                      GTK_CONSTRAINT_STRENGTH_REQUIRED);
          gtk_constraint_layout_add_constraint (layout, constraint);
          constraint = gtk_constraint_new (NULL,
                                      GTK_CONSTRAINT_ATTRIBUTE_LEFT,
                                      GTK_CONSTRAINT_RELATION_EQ,
                                      child,
                                      GTK_CONSTRAINT_ATTRIBUTE_LEFT,
                                      1.0,
                                      0.0,
                                      GTK_CONSTRAINT_STRENGTH_REQUIRED);
          gtk_constraint_layout_add_constraint (layout, constraint);
        }
    }

  constraint = gtk_constraint_new (NULL,
                                  GTK_CONSTRAINT_ATTRIBUTE_BOTTOM,
                                  GTK_CONSTRAINT_RELATION_EQ,
                                  child,
                                  GTK_CONSTRAINT_ATTRIBUTE_BOTTOM,
                                  1.0,
                                  0.0,
                                  GTK_CONSTRAINT_STRENGTH_REQUIRED);
  gtk_constraint_layout_add_constraint (layout, constraint);

  gtk_constraint_layout_add_grid_constraint (layout, grid);

  g_bytes_unref (data);
}

static void
word_grid_init (WordGrid *self)
{
  self->children = g_ptr_array_new_with_free_func ((GDestroyNotify)gtk_widget_destroy);

  read_words (self);
}

GtkWidget *
do_constraints3 (GtkWidget *do_widget)
{
 static GtkWidget *window;

 if (!window)
   {
     GtkWidget *header, *box, *grid, *button;
     GtkWidget *swin;

     window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
     gtk_window_set_display (GTK_WINDOW (window), gtk_widget_get_display (do_widget));

     header = gtk_header_bar_new ();
     gtk_header_bar_set_title (GTK_HEADER_BAR (header), "Constraints");
     gtk_header_bar_set_show_title_buttons (GTK_HEADER_BAR (header), FALSE);
     gtk_window_set_titlebar (GTK_WINDOW (window), header);
     g_signal_connect (window, "destroy",
                       G_CALLBACK (gtk_widget_destroyed), &window);

     box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
     gtk_container_add (GTK_CONTAINER (window), box);

     swin = gtk_scrolled_window_new (NULL, NULL);
     gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
                                     GTK_POLICY_ALWAYS,
                                     GTK_POLICY_ALWAYS);
     gtk_widget_set_hexpand (swin, TRUE);
     gtk_widget_set_vexpand (swin, TRUE);
     gtk_widget_set_halign (swin, GTK_ALIGN_FILL);
     gtk_widget_set_valign (swin, GTK_ALIGN_FILL);
     gtk_container_add (GTK_CONTAINER (box), swin);

     grid = g_object_new (word_grid_get_type (), NULL);
     gtk_widget_set_hexpand (grid, TRUE);
     gtk_widget_set_vexpand (grid, TRUE);
     gtk_container_add (GTK_CONTAINER (swin), grid);

     button = gtk_button_new_with_label ("Close");
     gtk_container_add (GTK_CONTAINER (box), button);
     gtk_widget_set_hexpand (grid, TRUE);
     g_signal_connect_swapped (button, "clicked",
                               G_CALLBACK (gtk_widget_destroy), window);
   }

 if (!gtk_widget_get_visible (window))
   gtk_widget_show (window);
 else
   gtk_widget_destroy (window);

 return window;
}
