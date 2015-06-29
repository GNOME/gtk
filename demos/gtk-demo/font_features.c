/* Pango/Font Features
 *
 * This demonstrates support for OpenType font features with
 * Pango attributes. The attributes can be used manually or
 * via Pango markup.
 */

#include <gtk/gtk.h>

static GtkWidget *label;
static GtkWidget *settings;
static GtkWidget *font;
static GtkWidget *resetbutton;
static GtkWidget *numcasedefault;
static GtkWidget *numspacedefault;
static GtkWidget *fractiondefault;
static GtkWidget *stack;
static GtkWidget *entry;

static GtkWidget *toggle[24];

static void
update (void)
{
  GString *s;
  char *font_desc;
  char *font_settings;
  const char *text;
  gboolean has_feature;
  int i;

  text = gtk_entry_get_text (GTK_ENTRY (entry));

  font_desc = gtk_font_chooser_get_font (GTK_FONT_CHOOSER (font));

  s = g_string_new ("");

  has_feature = FALSE;
  for (i = 0; i < 24; i++)
    {
      if (!gtk_widget_is_sensitive (toggle[i]))
        continue;

      if (GTK_IS_RADIO_BUTTON (toggle[i]))
        {
          if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle[i])))
            {
              if (has_feature)
                g_string_append (s, ", ");
              g_string_append (s, gtk_buildable_get_name (GTK_BUILDABLE (toggle[i])));
              g_string_append (s, " 1");
              has_feature = TRUE;
            }
        }
      else
        {
          if (has_feature)
            g_string_append (s, ", ");
          g_string_append (s, gtk_buildable_get_name (GTK_BUILDABLE (toggle[i])));
          if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle[i])))
            g_string_append (s, " 1");
          else
            g_string_append (s, " 0");
          has_feature = TRUE;
        }
    }

  font_settings = g_string_free (s, FALSE);

  gtk_label_set_text (GTK_LABEL (settings), font_settings);

  s = g_string_new ("");
  g_string_append_printf (s, "<span font_desc='%s' font_features='%s'>%s</span>", font_desc, font_settings, text);

  gtk_label_set_markup (GTK_LABEL (label), s->str);

  g_string_free (s, TRUE);

  g_free (font_desc);
  g_free (font_settings);
}

static void
reset (void)
{
  int i;

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (numcasedefault), TRUE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (numspacedefault), TRUE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (fractiondefault), TRUE);
  for (i = 0; i < 24; i++)
    {
      if (!GTK_IS_RADIO_BUTTON (toggle[i]))
        {
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle[i]), FALSE);
          gtk_widget_set_sensitive (toggle[i], FALSE);
        }
    }
}

static char *text;

static void
switch_to_entry (void)
{
  text = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
  gtk_stack_set_visible_child_name (GTK_STACK (stack), "entry");
}

static void
switch_to_label (void)
{
  g_free (text);
  text = NULL;
  gtk_stack_set_visible_child_name (GTK_STACK (stack), "label");
  update ();
}

static gboolean
entry_key_press (GtkEntry *entry, GdkEventKey *event)
{
  if (event->keyval == GDK_KEY_Escape)
    {
      gtk_entry_set_text (GTK_ENTRY (entry), text);
      switch_to_label ();
      return GDK_EVENT_STOP;
    }

  return GDK_EVENT_PROPAGATE;
}

GtkWidget *
do_font_features (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkBuilder *builder;
      int i;

      builder = gtk_builder_new_from_resource ("/font-features/font-features.ui");

      gtk_builder_add_callback_symbol (builder, "update", update);
      gtk_builder_add_callback_symbol (builder, "reset", reset);
      gtk_builder_add_callback_symbol (builder, "switch_to_entry", switch_to_entry);
      gtk_builder_add_callback_symbol (builder, "switch_to_label", switch_to_label);
      gtk_builder_add_callback_symbol (builder, "entry_key_press", G_CALLBACK (entry_key_press));
      gtk_builder_connect_signals (builder, NULL);

      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      label = GTK_WIDGET (gtk_builder_get_object (builder, "label"));
      settings = GTK_WIDGET (gtk_builder_get_object (builder, "settings"));
      resetbutton = GTK_WIDGET (gtk_builder_get_object (builder, "reset"));
      font = GTK_WIDGET (gtk_builder_get_object (builder, "font"));
      numcasedefault = GTK_WIDGET (gtk_builder_get_object (builder, "numcasedefault"));
      numspacedefault = GTK_WIDGET (gtk_builder_get_object (builder, "numspacedefault"));
      fractiondefault = GTK_WIDGET (gtk_builder_get_object (builder, "fractiondefault"));
      stack = GTK_WIDGET (gtk_builder_get_object (builder, "stack"));
      entry = GTK_WIDGET (gtk_builder_get_object (builder, "entry"));

      i = 0;
      toggle[i++] = GTK_WIDGET (gtk_builder_get_object (builder, "kern"));
      toggle[i++] = GTK_WIDGET (gtk_builder_get_object (builder, "liga"));
      toggle[i++] = GTK_WIDGET (gtk_builder_get_object (builder, "dlig"));
      toggle[i++] = GTK_WIDGET (gtk_builder_get_object (builder, "hlig"));
      toggle[i++] = GTK_WIDGET (gtk_builder_get_object (builder, "clig"));
      toggle[i++] = GTK_WIDGET (gtk_builder_get_object (builder, "smcp"));
      toggle[i++] = GTK_WIDGET (gtk_builder_get_object (builder, "c2sc"));
      toggle[i++] = GTK_WIDGET (gtk_builder_get_object (builder, "lnum"));
      toggle[i++] = GTK_WIDGET (gtk_builder_get_object (builder, "onum"));
      toggle[i++] = GTK_WIDGET (gtk_builder_get_object (builder, "pnum"));
      toggle[i++] = GTK_WIDGET (gtk_builder_get_object (builder, "tnum"));
      toggle[i++] = GTK_WIDGET (gtk_builder_get_object (builder, "frac"));
      toggle[i++] = GTK_WIDGET (gtk_builder_get_object (builder, "afrc"));
      toggle[i++] = GTK_WIDGET (gtk_builder_get_object (builder, "zero"));
      toggle[i++] = GTK_WIDGET (gtk_builder_get_object (builder, "nalt"));
      toggle[i++] = GTK_WIDGET (gtk_builder_get_object (builder, "swsh"));
      toggle[i++] = GTK_WIDGET (gtk_builder_get_object (builder, "calt"));
      toggle[i++] = GTK_WIDGET (gtk_builder_get_object (builder, "hist"));
      toggle[i++] = GTK_WIDGET (gtk_builder_get_object (builder, "salt"));
      toggle[i++] = GTK_WIDGET (gtk_builder_get_object (builder, "ss01"));
      toggle[i++] = GTK_WIDGET (gtk_builder_get_object (builder, "ss02"));
      toggle[i++] = GTK_WIDGET (gtk_builder_get_object (builder, "ss03"));
      toggle[i++] = GTK_WIDGET (gtk_builder_get_object (builder, "ss04"));
      toggle[i++] = GTK_WIDGET (gtk_builder_get_object (builder, "ss05"));

      update ();

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      g_object_unref (builder);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_widget_destroy (window);

  return window;
}
