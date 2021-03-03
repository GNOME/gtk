#include <gtk/gtk.h>

void
print_attribute (PangoAttribute *attr, GString *string)
{
  GEnumClass *class;
  GEnumValue *value;

  g_string_append_printf (string, "[%d,%d]", attr->start_index, attr->end_index);

  class = g_type_class_ref (pango_attr_type_get_type ());
  value = g_enum_get_value (class, attr->klass->type);
  g_string_append_printf (string, "%s=", value->value_nick);
  g_type_class_unref (class);

  switch (attr->klass->type)
    {
    case PANGO_ATTR_LANGUAGE:
      g_string_append (string, pango_language_to_string (((PangoAttrLanguage *)attr)->value));
      break;
    case PANGO_ATTR_FAMILY:
    case PANGO_ATTR_FONT_FEATURES:
      g_string_append (string, ((PangoAttrString *)attr)->value);
      break;
    case PANGO_ATTR_STYLE:
    case PANGO_ATTR_WEIGHT:
    case PANGO_ATTR_VARIANT:
    case PANGO_ATTR_STRETCH:
    case PANGO_ATTR_SIZE:
    case PANGO_ATTR_ABSOLUTE_SIZE:
    case PANGO_ATTR_UNDERLINE:
    case PANGO_ATTR_OVERLINE:
    case PANGO_ATTR_STRIKETHROUGH:
    case PANGO_ATTR_RISE:
    case PANGO_ATTR_FALLBACK:
    case PANGO_ATTR_LETTER_SPACING:
    case PANGO_ATTR_GRAVITY:
    case PANGO_ATTR_GRAVITY_HINT:
    case PANGO_ATTR_FOREGROUND_ALPHA:
    case PANGO_ATTR_BACKGROUND_ALPHA:
    case PANGO_ATTR_ALLOW_BREAKS:
    case PANGO_ATTR_INSERT_HYPHENS:
    case PANGO_ATTR_SHOW:
      g_string_append_printf (string, "%d", ((PangoAttrInt *)attr)->value);
      break;
    case PANGO_ATTR_FONT_DESC:
      {
        char *text = pango_font_description_to_string (((PangoAttrFontDesc *)attr)->desc);
        g_string_append (string, text);
        g_free (text);
      }
      break;
    case PANGO_ATTR_FOREGROUND:
    case PANGO_ATTR_BACKGROUND:
    case PANGO_ATTR_UNDERLINE_COLOR:
    case PANGO_ATTR_OVERLINE_COLOR:
    case PANGO_ATTR_STRIKETHROUGH_COLOR:
      {
        char *text = pango_color_to_string (&((PangoAttrColor *)attr)->color);
        g_string_append (string, text);
        g_free (text);
      }
      break;
    case PANGO_ATTR_SHAPE:
      g_string_append_printf (string, "shape");
      break;
    case PANGO_ATTR_SCALE:
      {
        char val[20];

        g_ascii_formatd (val, 20, "%f", ((PangoAttrFloat *)attr)->value);
        g_string_append (string, val);
     }
      break;
    case PANGO_ATTR_INVALID:
    default:
      g_assert_not_reached ();
      break;
    }
}

void
print_attr_list (PangoAttrList *attrs, GString *string)
{
  PangoAttrIterator *iter;

  if (!attrs)
    return;

  iter = pango_attr_list_get_iterator (attrs);
  do {
    gint start, end;
    GSList *list, *l;

    pango_attr_iterator_range (iter, &start, &end);
    g_string_append_printf (string, "range %d %d\n", start, end);
    list = pango_attr_iterator_get_attrs (iter);
    for (l = list; l; l = l->next)
      {
        PangoAttribute *attr = l->data;
        print_attribute (attr, string);
        g_string_append (string, "\n");
      }
    g_slist_free_full (list, (GDestroyNotify)pango_attribute_destroy);
  } while (pango_attr_iterator_next (iter));

  pango_attr_iterator_destroy (iter);
}

static void
test_label_markup (void)
{
  GtkWidget *window;
  GtkWidget *label;
  PangoAttrList *attrs;
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

  text = pango_layout_get_text (gtk_label_get_layout (GTK_LABEL (label)));
  g_assert_cmpstr (text, ==, "abc def");

  attrs = pango_layout_get_attributes (gtk_label_get_layout (GTK_LABEL (label)));
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

  text = pango_layout_get_text (gtk_label_get_layout (GTK_LABEL (label)));
  g_assert_cmpstr (text, ==, "abc def");

  attrs = pango_layout_get_attributes (gtk_label_get_layout (GTK_LABEL (label)));
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

  text = pango_layout_get_text (gtk_label_get_layout (GTK_LABEL (label)));
  g_assert_cmpstr (text, ==, "abc _def");

  attrs = pango_layout_get_attributes (gtk_label_get_layout (GTK_LABEL (label)));
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

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/label/markup-parse", test_label_markup);

  return g_test_run ();
}
