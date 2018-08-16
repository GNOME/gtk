#include <gtk/gtk.h>


#define GTK_TYPE_FORM_ENTRY (gtk_form_entry_get_type ())
G_DECLARE_FINAL_TYPE (GtkFormEntry, gtk_form_entry, GTK, FORM_ENTRY, GtkWidget)

#define FINAL_SCALE 0.7
#define TRANSITION_DURATION (200 * 1000)

struct _GtkFormEntry
{
  GtkWidget parent_instance;

  GtkWidget *entry;
  GtkWidget *placeholder;

  double placeholder_scale;
};

G_DEFINE_TYPE (GtkFormEntry, gtk_form_entry, GTK_TYPE_WIDGET)


static void
gtk_form_entry_size_allocate (GtkWidget *widget,
                              int        width,
                              int        height,
                              int        baseline)
{
  GtkFormEntry *self = (GtkFormEntry *)widget;
  int placeholder_height;
  int top;

  gtk_widget_measure (self->placeholder, GTK_ORIENTATION_VERTICAL, -1,
                      &placeholder_height, NULL, NULL, NULL);
  top = placeholder_height * FINAL_SCALE;

  gtk_widget_size_allocate (self->entry,
                            &(GtkAllocation) {
                              0, top,
                              width, height - top
                            }, -1);

  /* Placeholder allocation depends on self->placeholder_scale.
   * If that's 1.0, we don't scale it and center it on the
   * GtkEntry. Otherwise, we move it up until y = 0. */
  {
    const int max_y = top + ((height - top) / 2) - (placeholder_height / 2);
    const double t = self->placeholder_scale; /* TODO: Interpolate */
    const int y = 0 + (t * max_y);
    int x;
    graphene_matrix_t m;

    /* Get 0 in entry coords so we can position the placeholder there. */
    gtk_widget_translate_coordinates (self->entry, widget, 0, 0, &x, NULL);

    x *= t;

    gtk_widget_size_allocate (self->placeholder,
                              &(GtkAllocation) {
                                x, y,
                                width,
                                placeholder_height
                              }, -1);

    graphene_matrix_init_translate (&m, &GRAPHENE_POINT3D_INIT (x, y, 0));

    graphene_matrix_scale (&m,
                           CLAMP (t, FINAL_SCALE, 1.0),
                           CLAMP (t, FINAL_SCALE, 1.0),
                           1);
    /*gtk_widget_set_transform (self->placeholder, &m);*/
    gtk_widget_allocate (self->placeholder,
                         width,
                         placeholder_height,
                         -1,
                         &m);
  }
}

static void
gtk_form_entry_measure (GtkWidget      *widget,
                        GtkOrientation  orientation,
                        int             for_size,
                        int            *minimum,
                        int            *natural,
                        int            *minimum_baseline,
                        int            *natural_baseline)
{
  GtkFormEntry *self = (GtkFormEntry *)widget;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      int min1, nat1;
      int min2, nat2;

      gtk_widget_measure (self->entry, orientation, for_size,
                          &min1, &nat1, NULL, NULL);

      gtk_widget_measure (self->entry, orientation, for_size,
                          &min2, &nat2, NULL, NULL);

      *minimum = MAX (min1, min2);
      *natural = MAX (nat1, nat2);
    }
  else /* VERTICAL */
    {
      int min, nat;
      int pmin, pnat;

      gtk_widget_measure (self->entry, orientation, -1,
                          &min, &nat, NULL, NULL);

      gtk_widget_measure (self->entry, orientation, -1,
                          &pmin, &pnat, NULL, NULL);

      *minimum = min + (pmin * FINAL_SCALE);
      *natural = nat + (pnat * FINAL_SCALE);
    }
}

static gboolean
tick_cb (GtkWidget     *widget,
         GdkFrameClock *frame_clock,
         gpointer       user_data)
{
  GtkFormEntry *self = user_data;

  self->placeholder_scale -= 0.02;

  gtk_widget_queue_allocate (GTK_WIDGET (self));
  gtk_widget_queue_draw (self->placeholder);

  if (self->placeholder_scale <= 0)
    {
      self->placeholder_scale = 0;
      return G_SOURCE_REMOVE;
    }


  return G_SOURCE_CONTINUE;
}

static void
gtk_form_entry_focused (GtkEventControllerKey *controller,
                        gpointer               user_data)
{
  GtkFormEntry *self = user_data;

  gtk_widget_add_tick_callback (GTK_WIDGET (self), tick_cb, self, NULL);
}

static void
gtk_form_entry_unfocused (GtkEventControllerKey *controller,
                          gpointer               user_data)
{
  GtkFormEntry *self = user_data;

  self->placeholder_scale = 1.0;
  gtk_widget_queue_allocate (GTK_WIDGET (self));
  gtk_widget_queue_draw (self->placeholder);
}


static void
gtk_form_entry_class_init (GtkFormEntryClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->measure = gtk_form_entry_measure;
  widget_class->size_allocate = gtk_form_entry_size_allocate;
}

static void
gtk_form_entry_init (GtkFormEntry *self)
{
  GtkEventController *key_controller;
  gtk_widget_set_has_surface (GTK_WIDGET (self), FALSE);

  self->entry = gtk_entry_new ();
  self->placeholder = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (self->placeholder), 0);
  self->placeholder_scale = 1.0;

  gtk_widget_set_parent (self->entry, GTK_WIDGET (self));
  gtk_widget_set_parent (self->placeholder, GTK_WIDGET (self));

  key_controller = gtk_event_controller_key_new ();
  g_signal_connect (key_controller, "focus-in", G_CALLBACK (gtk_form_entry_focused), self);
  g_signal_connect (key_controller, "focus-out", G_CALLBACK (gtk_form_entry_unfocused), self);
  gtk_widget_add_controller (self->entry, key_controller);
}

GtkWidget *
gtk_form_entry_new (const char *text)
{
   GtkWidget *w = g_object_new (GTK_TYPE_FORM_ENTRY, NULL);

   gtk_label_set_text (GTK_LABEL (((GtkFormEntry*)w)->placeholder), text);

   return w;
}

int
main (int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *form_entry1;
  GtkWidget *form_entry2;

  gtk_init ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  form_entry1 = gtk_form_entry_new ("First Name");
  form_entry2 = gtk_form_entry_new ("Last Name");

  gtk_container_add (GTK_CONTAINER (box), form_entry1);
  gtk_container_add (GTK_CONTAINER (box), form_entry2);


  gtk_widget_set_halign (box, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (box, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (window), box);



  gtk_window_set_default_size ((GtkWindow *)window, 200, 200);
  g_signal_connect (window, "close-request", G_CALLBACK (gtk_main_quit), NULL);
  gtk_widget_show (window);


  gtk_main ();

  return 0;
}
