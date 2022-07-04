#include "samplechooser.h"
#include <gtk/gtk.h>
#include <hb-ot.h>

enum {
  PROP_SAMPLE_TEXT = 1,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

struct _SampleChooser
{
  GtkWidget parent;

  int sample;
  const char *sample_text;
};

struct _SampleChooserClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE(SampleChooser, sample_chooser, GTK_TYPE_WIDGET);

static const char *pangrams[] = {
  "The quick brown fox jumps over the lazy dog.",
  "Waltz, bad nymph, for quick jigs vex.",
  "Quick zephyrs blow, vexing daft Jim.",
  "Crazy Fredrick bought many very exquisite opal jewels.",
  "Jaded zombies acted quaintly but kept driving their oxen forward.",
};

static const char *paragraphs[] = {
  "Grumpy wizards make toxic brew for the evil Queen and Jack. A quick movement of the enemy will jeopardize six gunboats. The job of waxing linoleum frequently peeves chintzy kids. My girl wove six dozen plaid jackets before she quit. Twelve ziggurats quickly jumped a finch box.",
  "    Разъяренный чтец эгоистично бьёт пятью жердями шустрого фехтовальщика. Наш банк вчера же выплатил Ф.Я. Эйхгольду комиссию за ценные вещи. Эх, чужак, общий съём цен шляп (юфть) – вдрызг! В чащах юга жил бы цитрус? Да, но фальшивый экземпляр!",
  "Τάχιστη αλώπηξ βαφής ψημένη γη, δρασκελίζει υπέρ νωθρού κυνός",
};

static const char *alphabets[] = {
  "abcdefghijklmnopqrstuvwxyz",
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
  "0123456789",
  "!@#$%^&*()?",
};

static const char *titles[] = {
  "From My Cold Dead Hands",
  "From Afar Upon the Back of a Tiger",
  "Spontaneous Apple Creation",
  "Big Bizness (Screwed & Chopped)",
  "Pizza Shop Extended",
  "Good News & Bad News",
};

static void
next_pangram (GtkButton     *button,
              SampleChooser *self)
{
  self->sample++;
  self->sample_text = pangrams[self->sample % G_N_ELEMENTS (pangrams)];
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SAMPLE_TEXT]);
}

static void
next_paragraph (GtkButton     *button,
                SampleChooser *self)
{
  self->sample++;
  self->sample_text = paragraphs[self->sample % G_N_ELEMENTS (paragraphs)];
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SAMPLE_TEXT]);
}

static void
next_alphabet (GtkButton     *button,
               SampleChooser *self)
{
  self->sample++;
  self->sample_text = alphabets[self->sample % G_N_ELEMENTS (alphabets)];
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SAMPLE_TEXT]);
}

static void
next_title (GtkButton     *button,
            SampleChooser *self)
{
  self->sample++;
  self->sample_text = titles[self->sample % G_N_ELEMENTS (titles)];
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SAMPLE_TEXT]);
}

static void
sample_chooser_init (SampleChooser *self)
{
  self->sample_text = "Boring sample text";
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
sample_chooser_dispose (GObject *object)
{
  GtkWidget *child;

  gtk_widget_clear_template (GTK_WIDGET (object), SAMPLE_CHOOSER_TYPE);

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (object))) != NULL)
    gtk_widget_unparent (child);

  G_OBJECT_CLASS (sample_chooser_parent_class)->dispose (object);
}

static void
sample_chooser_get_property (GObject      *object,
                             unsigned int  prop_id,
                             GValue       *value,
                             GParamSpec   *pspec)
{
  SampleChooser *self = SAMPLE_CHOOSER (object);

  switch (prop_id)
    {
    case PROP_SAMPLE_TEXT:
      g_value_set_string (value, self->sample_text);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
sample_chooser_class_init (SampleChooserClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = sample_chooser_dispose;
  object_class->get_property = sample_chooser_get_property;

  properties[PROP_SAMPLE_TEXT] =
      g_param_spec_string ("sample-text", "", "",
                           "",
                           G_PARAM_READABLE);

  g_object_class_install_properties (G_OBJECT_CLASS (class), NUM_PROPERTIES, properties);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (class),
                                               "/org/gtk/fontexplorer/samplechooser.ui");

  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), next_pangram);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), next_paragraph);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), next_alphabet);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), next_title);

  gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (class), "samplechooser");
}

SampleChooser *
sample_chooser_new (void)
{
  return g_object_new (SAMPLE_CHOOSER_TYPE, NULL);
}
