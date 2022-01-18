#include <gtk/gtk.h>

void
print_attribute (Pango2Attribute *attr, GString *string)
{
  GEnumClass *class;
  GEnumValue *value;
  guint start, end;

  pango2_attribute_get_range (attr, &start, &end);
  g_string_append_printf (string, "[%u,%u]", start, end);

  class = g_type_class_ref (pango2_attr_type_get_type ());
  value = g_enum_get_value (class, pango2_attribute_type (attr));
  g_string_append_printf (string, "%s=", value->value_nick);
  g_type_class_unref (class);

  switch (PANGO2_ATTR_VALUE_TYPE (attr))
    {
    case PANGO2_ATTR_VALUE_STRING:
      g_string_append (string, pango2_attribute_get_string (attr));
      break;
    case PANGO2_ATTR_VALUE_BOOLEAN:
      g_string_append_printf (string, "%s", pango2_attribute_get_boolean (attr) ? "true" : "false");
      break;
    case PANGO2_ATTR_VALUE_LANGUAGE:
      g_string_append (string, pango2_language_to_string (pango2_attribute_get_language (attr)));
      break;
    case PANGO2_ATTR_VALUE_INT:
      g_string_append_printf (string, "%d", pango2_attribute_get_int (attr));
      break;
    case PANGO2_ATTR_VALUE_FLOAT:
      {
        char val[20];

        g_ascii_formatd (val, 20, "%f", pango2_attribute_get_float (attr));
        g_string_append (string, val);
      }
      break;
    case PANGO2_ATTR_VALUE_FONT_DESC:
      {
        char *text = pango2_font_description_to_string (pango2_attribute_get_font_desc (attr));
        g_string_append (string, text);
        g_free (text);
      }
      break;
    case PANGO2_ATTR_VALUE_COLOR:
      {
        char *text = pango2_color_to_string (pango2_attribute_get_color (attr));
        g_string_append (string, text);
        g_free (text);
      }
      break;
    case PANGO2_ATTR_VALUE_POINTER:
    default:
      g_assert_not_reached ();
    }
}

void
print_attr_list (Pango2AttrList *attrs, GString *string)
{
  Pango2AttrIterator *iter;

  if (!attrs)
    return;

  iter = pango2_attr_list_get_iterator (attrs);
  do {
    int start, end;
    GSList *list, *l;

    pango2_attr_iterator_range (iter, &start, &end);
    g_string_append_printf (string, "range %d %d\n", start, end);
    list = pango2_attr_iterator_get_attrs (iter);
    for (l = list; l; l = l->next)
      {
        Pango2Attribute *attr = l->data;
        print_attribute (attr, string);
        g_string_append (string, "\n");
      }
    g_slist_free_full (list, (GDestroyNotify)pango2_attribute_destroy);
  } while (pango2_attr_iterator_next (iter));

  pango2_attr_iterator_destroy (iter);
}

static void
test_label_markup (void)
{
  GtkWidget *window;
  GtkWidget *label;
  Pango2AttrList *attrs;
  GString *str;
  const char *text;

  window = gtk_window_new ();
  label = gtk_label_new ("");

  gtk_window_set_child (GTK_WINDOW (window), label);
  gtk_window_set_mnemonics_visible (GTK_WINDOW (window), TRUE);

  gtk_label_set_use_underline (GTK_LABEL (label), TRUE);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_label_set_label (GTK_LABEL (label), "<a href=\"test\"><span font_style=\"italic\">abc</span> _def</a>");

  g_assert_cmpuint (gtk_label_get_mnemonic_keyval (GTK_LABEL (label)), ==, 'd');

  text = pango2_layout_get_text (gtk_label_get_layout (GTK_LABEL (label)));
  g_assert_cmpstr (text, ==, "abc def");

  attrs = pango2_layout_get_attributes (gtk_label_get_layout (GTK_LABEL (label)));
  str = g_string_new ("");
  print_attr_list (attrs, str);

  g_assert_cmpstr (str->str, ==,
    "range 0 3\n"
    "[0,4]underline=1\n"
    "[0,8]foreground=#1b1b6a6acbcb\n"
    "[0,3]style=2\n"
    "range 3 4\n"
    "[0,4]underline=1\n"
    "[0,8]foreground=#1b1b6a6acbcb\n"
    "range 4 5\n"
    "[0,8]foreground=#1b1b6a6acbcb\n"
    "[4,5]underline=3\n"
    "range 5 8\n"
    "[0,8]foreground=#1b1b6a6acbcb\n"
    "[5,8]underline=1\n"
    "range 8 2147483647\n");


  gtk_window_set_mnemonics_visible (GTK_WINDOW (window), FALSE);

  text = pango2_layout_get_text (gtk_label_get_layout (GTK_LABEL (label)));
  g_assert_cmpstr (text, ==, "abc def");

  attrs = pango2_layout_get_attributes (gtk_label_get_layout (GTK_LABEL (label)));
  g_string_set_size (str, 0);
  print_attr_list (attrs, str);

  g_assert_cmpstr (str->str, ==,
    "range 0 3\n"
    "[0,7]underline=1\n"
    "[0,7]foreground=#1b1b6a6acbcb\n"
    "[0,3]style=2\n"
    "range 3 7\n"
    "[0,7]underline=1\n"
    "[0,7]foreground=#1b1b6a6acbcb\n"
    "range 7 2147483647\n");

  gtk_window_set_mnemonics_visible (GTK_WINDOW (window), TRUE);
  gtk_label_set_use_underline (GTK_LABEL (label), FALSE);

  text = pango2_layout_get_text (gtk_label_get_layout (GTK_LABEL (label)));
  g_assert_cmpstr (text, ==, "abc _def");

  attrs = pango2_layout_get_attributes (gtk_label_get_layout (GTK_LABEL (label)));
  g_string_set_size (str, 0);
  print_attr_list (attrs, str);

  g_assert_cmpstr (str->str, ==,
    "range 0 3\n"
    "[0,8]underline=1\n"
    "[0,8]foreground=#1b1b6a6acbcb\n"
    "[0,3]style=2\n"
    "range 3 8\n"
    "[0,8]underline=1\n"
    "[0,8]foreground=#1b1b6a6acbcb\n"
    "range 8 2147483647\n");

  g_string_free (str, TRUE);

  gtk_window_destroy (GTK_WINDOW (window));
}

static void
test_label_underline (void)
{
  GtkWidget *window;
  GtkWidget *label;

  window = gtk_window_new ();

  label = gtk_label_new ("");

  gtk_window_set_child (GTK_WINDOW (window), label);
  gtk_window_set_mnemonics_visible (GTK_WINDOW (window), TRUE);

  gtk_label_set_use_markup (GTK_LABEL (label), FALSE);
  gtk_label_set_use_underline (GTK_LABEL (label), TRUE);
  gtk_label_set_label (GTK_LABEL (label), "tes_t & no markup <<");

  g_assert_cmpint (gtk_label_get_mnemonic_keyval (GTK_LABEL (label)), ==, GDK_KEY_t);

  gtk_window_destroy (GTK_WINDOW (window));
}

static void
test_label_parse_more (void)
{
  struct {
    const char *input;
    gboolean use_underline;
    gboolean use_markup;
    const char *text;
    guint accel;
  } tests[] = {
    { "tes_t m__e mo_re", TRUE, FALSE, "test m_e more", GDK_KEY_t },
    { "test m__e mo_re", TRUE, FALSE, "test m_e more", GDK_KEY_r },
    { "tes_t m__e mo_re", FALSE, FALSE, "tes_t m__e mo_re", GDK_KEY_VoidSymbol },
    { "<span font='test_font'>test <a href='bla'>w_ith</a> bla</span>", TRUE, TRUE, "test with bla", GDK_KEY_i },
    { "<span font='test_font'>test <a href='bla'>w_ith</a> bla</span>", FALSE, TRUE, "test w_ith bla", GDK_KEY_VoidSymbol },
  };
  GtkWidget *label;

  label = gtk_label_new ("");

  for (int i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      gtk_label_set_use_underline (GTK_LABEL (label), tests[i].use_underline);
      gtk_label_set_use_markup (GTK_LABEL (label), tests[i].use_markup);
      gtk_label_set_label (GTK_LABEL (label), tests[i].input);

      g_assert_cmpstr (gtk_label_get_label (GTK_LABEL (label)), ==, tests[i].input);
      g_assert_cmpstr (gtk_label_get_text (GTK_LABEL (label)), ==, tests[i].text);
      g_assert_cmpuint (gtk_label_get_mnemonic_keyval (GTK_LABEL (label)), ==, tests[i].accel);
    }

  g_object_ref_sink (label);
  g_object_unref (label);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/label/markup-parse", test_label_markup);
  g_test_add_func ("/label/underline-parse", test_label_underline);
  g_test_add_func ("/label/parse-more", test_label_parse_more);

  return g_test_run ();
}
