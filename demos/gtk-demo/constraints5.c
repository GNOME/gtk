/* Constraints/Words
 *
 * GtkConstraintLayout lets you define big grids.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#define WORDS_TYPE_BASE (words_base_get_type ())
#define WORDS_TYPE_GRID (words_grid_get_type ())
#define WORDS_TYPE_CONSTRAINT (words_constraint_get_type ())

typedef struct
{
  GtkWidget parent_instance;
} WordsBase;

typedef WordsBase WordsGrid;
typedef WordsBase WordsConstraint;

typedef GtkWidgetClass WordsBaseClass;
typedef GtkWidgetClass WordsGridClass;
typedef GtkWidgetClass WordsConstraintClass;

G_DEFINE_TYPE (WordsBase, words_base, GTK_TYPE_WIDGET)
G_DEFINE_TYPE (WordsGrid, words_grid, WORDS_TYPE_BASE)
G_DEFINE_TYPE (WordsConstraint, words_constraint, WORDS_TYPE_BASE)

static void
words_grid_init (WordsGrid *words)
{
}

static void
words_grid_class_init (WordsGridClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_GRID_LAYOUT);
}

static void
words_constraint_init (WordsGrid *words)
{
}

static void
words_constraint_class_init (WordsConstraintClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_CONSTRAINT_LAYOUT);
}

static void
word_base_dispose (GObject *object)
{
  GtkWidget *self = GTK_WIDGET (object);
  GtkWidget *child;

  while ((child = gtk_widget_get_first_child (self)) != NULL)
    gtk_widget_unparent (child);

  G_OBJECT_CLASS (words_base_parent_class)->dispose (object);
}

static void
words_base_class_init (WordsBaseClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = word_base_dispose;
}

static int num_words = 100;
static gboolean use_constraints = FALSE;

static void
read_words (WordsBase *self)
{
  GBytes *data;
  const char *words;
  int left, top;
  GtkWidget *child = NULL;
  GtkLayoutManager *layout = gtk_widget_get_layout_manager (GTK_WIDGET (self));
  GtkGridConstraint *grid;
  GtkConstraint *constraint;
  int count;
  int rightmost;
  GtkWidget *right_child = NULL;
  gboolean use_constraint = GTK_IS_CONSTRAINT_LAYOUT (layout);

  if (use_constraint)
    {
      grid = gtk_grid_constraint_new ();
      g_object_set (grid,
                    "row-homogeneous", TRUE,
                    "column-homogeneous", FALSE,
                    NULL);
    }
  else
    {
      gtk_grid_layout_set_row_homogeneous (GTK_GRID_LAYOUT (layout), TRUE);
      gtk_grid_layout_set_column_homogeneous (GTK_GRID_LAYOUT (layout), FALSE);
    }

  data = g_resources_lookup_data ("/constraints5/words", 0, NULL);
  words = g_bytes_get_data (data, NULL);
  count = 0;

  rightmost = 0;
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

      gtk_widget_set_parent (child, GTK_WIDGET (self));

      if (left + len > rightmost)
        {
          rightmost = left + len;
          right_child = child;
        }

      if (use_constraint)
        {
          gtk_grid_constraint_add (grid, child,
                                   left, left + len,
                                   top, top + 1);
          if (left == 0 && top == 0)
            {
              constraint = gtk_constraint_new (NULL,
                                               GTK_CONSTRAINT_ATTRIBUTE_TOP,
                                               GTK_CONSTRAINT_RELATION_EQ,
                                               child,
                                               GTK_CONSTRAINT_ATTRIBUTE_TOP,
                                               1.0,
                                               0.0,
                                               GTK_CONSTRAINT_STRENGTH_REQUIRED);
              gtk_constraint_layout_add_constraint (GTK_CONSTRAINT_LAYOUT (layout),
                                                    constraint);
              constraint = gtk_constraint_new (NULL,
                                               GTK_CONSTRAINT_ATTRIBUTE_LEFT,
                                               GTK_CONSTRAINT_RELATION_EQ,
                                               child,
                                               GTK_CONSTRAINT_ATTRIBUTE_LEFT,
                                               1.0,
                                               0.0,
                                               GTK_CONSTRAINT_STRENGTH_REQUIRED);
               gtk_constraint_layout_add_constraint (GTK_CONSTRAINT_LAYOUT (layout),
                                                     constraint);
            }
        }
      else
        {
          GtkGridLayoutChild *grid_child = GTK_GRID_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (layout, child));

          g_object_set (grid_child,
                        "left-attach", left,
                        "top-attach", top,
                        "column-span", len,
                        "row-span", 1,
                        NULL);
        }

      left = left + len;
      count++;

      if (count >= num_words)
        break;
    }

  if (use_constraint)
    {
      constraint = gtk_constraint_new (NULL,
                                       GTK_CONSTRAINT_ATTRIBUTE_RIGHT,
                                       GTK_CONSTRAINT_RELATION_EQ,
                                       right_child,
                                       GTK_CONSTRAINT_ATTRIBUTE_RIGHT,
                                       1.0,
                                       0.0,
                                       GTK_CONSTRAINT_STRENGTH_REQUIRED);
      gtk_constraint_layout_add_constraint (GTK_CONSTRAINT_LAYOUT (layout),
                                            constraint);
      constraint = gtk_constraint_new (NULL,
                                       GTK_CONSTRAINT_ATTRIBUTE_BOTTOM,
                                       GTK_CONSTRAINT_RELATION_EQ,
                                       child,
                                       GTK_CONSTRAINT_ATTRIBUTE_BOTTOM,
                                       1.0,
                                       0.0,
                                       GTK_CONSTRAINT_STRENGTH_REQUIRED);
      gtk_constraint_layout_add_constraint (GTK_CONSTRAINT_LAYOUT (layout),
                                            constraint);

      gtk_grid_constraint_attach (grid, GTK_CONSTRAINT_LAYOUT (layout));
    }

  g_bytes_unref (data);
}

static void
words_base_init (WordsBase *self)
{
  read_words (self);
}

static void
show_words (GtkWidget *parent)
{
  GtkWidget *window;
  GtkWidget *header, *box, *grid, *button;
  GtkWidget *swin;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_transient_for (GTK_WINDOW (window),
                                GTK_WINDOW (gtk_widget_get_root (parent)));
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);

  header = gtk_header_bar_new ();
  gtk_header_bar_set_title (GTK_HEADER_BAR (header), use_constraints ? "Constraints" : "Grid");
  gtk_header_bar_set_show_title_buttons (GTK_HEADER_BAR (header), FALSE);
  gtk_window_set_titlebar (GTK_WINDOW (window), header);
  gtk_window_set_resizable (GTK_WINDOW (window), TRUE);
  gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (window), box);

  swin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_propagate_natural_width (GTK_SCROLLED_WINDOW (swin), TRUE);
  gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (swin), TRUE);
  gtk_widget_set_hexpand (swin, TRUE);
  gtk_widget_set_vexpand (swin, TRUE);
  gtk_widget_set_halign (swin, GTK_ALIGN_FILL);
  gtk_widget_set_valign (swin, GTK_ALIGN_FILL);
  gtk_container_add (GTK_CONTAINER (box), swin);

  if (use_constraints)
    grid = g_object_new (WORDS_TYPE_CONSTRAINT, NULL);
  else
    grid = g_object_new (WORDS_TYPE_GRID, NULL);

  gtk_widget_set_halign (swin, GTK_ALIGN_START);
  gtk_widget_set_valign (swin, GTK_ALIGN_START);

  gtk_container_add (GTK_CONTAINER (swin), grid);

  button = gtk_button_new_with_label ("Close");
  gtk_container_add (GTK_CONTAINER (box), button);
  g_signal_connect_swapped (button, "clicked",
                            G_CALLBACK (gtk_widget_destroy), window);

  gtk_widget_show (window);
}

static void
use_constraints_cb (GtkButton *button)
{
  use_constraints = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
}

static void
word_count_cb (GtkSpinButton *button)
{
  num_words = gtk_spin_button_get_value_as_int (button);
}

GtkWidget *
do_constraints5 (GtkWidget *do_widget)
{
 static GtkWidget *window;

 if (!window)
   {
     GtkWidget *header, *grid, *button, *label;

     window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

     header = gtk_header_bar_new ();
     gtk_header_bar_set_title (GTK_HEADER_BAR (header), "Words");
     gtk_header_bar_set_show_title_buttons (GTK_HEADER_BAR (header), TRUE);
     gtk_window_set_titlebar (GTK_WINDOW (window), header);
     gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
     g_signal_connect (window, "destroy",
                       G_CALLBACK (gtk_widget_destroyed), &window);

     grid = gtk_grid_new ();
     g_object_set (grid,
                   "margin", 12,
                   "row-spacing", 12,
                   "column-spacing", 6,
                   "halign", GTK_ALIGN_FILL,
                   "valign", GTK_ALIGN_FILL,
                   "hexpand", TRUE,
                   "vexpand", TRUE,
                   NULL);
     gtk_container_add (GTK_CONTAINER (window), grid);

     label = gtk_label_new ("Constraints:");
     gtk_label_set_xalign (GTK_LABEL (label), 1.0);
     gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
     button = gtk_check_button_new ();
     g_signal_connect (button, "clicked", G_CALLBACK (use_constraints_cb), NULL);
     gtk_grid_attach (GTK_GRID (grid), button, 1, 0, 1, 1);
     label = gtk_label_new ("Words:");
     gtk_label_set_xalign (GTK_LABEL (label), 1.0);
     gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);
     button = gtk_spin_button_new_with_range (0, 1300, 1);
     g_signal_connect (button, "value-changed", G_CALLBACK (word_count_cb), NULL);
     gtk_spin_button_set_value (GTK_SPIN_BUTTON (button), 10);
     gtk_grid_attach (GTK_GRID (grid), button, 1, 1, 1, 1);

     button = gtk_button_new_with_label ("Show");
     gtk_widget_set_halign (button, GTK_ALIGN_END);
     gtk_widget_set_valign (button, GTK_ALIGN_END);
     g_signal_connect_swapped (button, "clicked",
                               G_CALLBACK (show_words), window);
     gtk_grid_attach (GTK_GRID (grid), button, 0, 2, 2, 1);
   }

 if (!gtk_widget_get_visible (window))
   gtk_widget_show (window);
 else
   gtk_widget_destroy (window);

 return window;
}
