/* Lists/Characters
 *
 * This demo shows a multi-column representation of some parts
 * of the Unicode Character Database, or UCD.
 */

#include <gtk/gtk.h>
#include "script-names.h"


#define UCD_TYPE_ITEM (ucd_item_get_type ())
G_DECLARE_FINAL_TYPE (UcdItem, ucd_item, UCD, ITEM, GObject)

struct _UcdItem
{
  GObject parent_instance;
  gunichar codepoint;
  const char *name;
};

struct _UcdItemClass
{
  GObjectClass parent_class;
};

G_DEFINE_TYPE (UcdItem, ucd_item, G_TYPE_OBJECT)

static void
ucd_item_init (UcdItem *item)
{
}

static void
ucd_item_class_init (UcdItemClass *class)
{
}

static UcdItem *
ucd_item_new (gunichar    codepoint,
              const char *name)
{
  UcdItem *item;

  item = g_object_new (UCD_TYPE_ITEM, NULL);

  item->codepoint = codepoint;
  item->name = name;

  return item;
}

static gunichar
ucd_item_get_codepoint (UcdItem *item)
{
  return item->codepoint;
}

static const char *
ucd_item_get_name (UcdItem *item)
{
  return item->name;
}

static GListModel *
ucd_model_new (void)
{
  GBytes *bytes;
  GVariant *v;
  GVariantIter *iter;
  GListStore *store;
  guint u;
  char *name;

  bytes = g_resources_lookup_data ("/listview_ucd/ucdnames.data", 0, NULL);
  v = g_variant_ref_sink (g_variant_new_from_bytes (G_VARIANT_TYPE ("a(us)"), bytes, TRUE));

  iter = g_variant_iter_new (v);

  store = g_list_store_new (G_TYPE_OBJECT);
  while (g_variant_iter_next (iter, "(us)", &u, &name))
    {
      if (u == 0)
        continue;

      UcdItem *item = ucd_item_new (u, name);
      g_list_store_append (store, item);
      g_object_unref (item);
    }

  g_variant_iter_free (iter);
  g_variant_unref (v);
  g_bytes_unref (bytes);

  return G_LIST_MODEL (store);
}

static void
setup_centered_label (GtkSignalListItemFactory *factory,
                      GObject                  *listitem)
{
  GtkWidget *label;
  label = gtk_label_new ("");
  gtk_list_item_set_child (GTK_LIST_ITEM (listitem), label);
}

static void
setup_label (GtkSignalListItemFactory *factory,
             GObject                  *listitem)
{
  GtkWidget *label;
  label = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_list_item_set_child (GTK_LIST_ITEM (listitem), label);
}

static void
bind_codepoint (GtkSignalListItemFactory *factory,
                GObject                  *listitem)
{
  GtkWidget *label;
  GObject *item;
  gunichar codepoint;
  char buffer[16] = { 0, };

  label = gtk_list_item_get_child (GTK_LIST_ITEM (listitem));
  item = gtk_list_item_get_item (GTK_LIST_ITEM (listitem));
  codepoint = ucd_item_get_codepoint (UCD_ITEM (item));

  g_snprintf (buffer, 10, "%#06x", codepoint);
  gtk_label_set_label (GTK_LABEL (label), buffer);
}

static void
bind_char (GtkSignalListItemFactory *factory,
           GObject                  *listitem)
{
  GtkWidget *label;
  GObject *item;
  gunichar codepoint;
  char buffer[16] = { 0, };

  label = gtk_list_item_get_child (GTK_LIST_ITEM (listitem));
  item = gtk_list_item_get_item (GTK_LIST_ITEM (listitem));
  codepoint = ucd_item_get_codepoint (UCD_ITEM (item));

  if (g_unichar_isprint (codepoint))
    g_unichar_to_utf8 (codepoint, buffer);

  gtk_label_set_label (GTK_LABEL (label), buffer);
}

static void
bind_name (GtkSignalListItemFactory *factory,
           GObject                  *listitem)
{
  GtkWidget *label;
  GObject *item;
  const char *name;

  label = gtk_list_item_get_child (GTK_LIST_ITEM (listitem));
  item = gtk_list_item_get_item (GTK_LIST_ITEM (listitem));
  name = ucd_item_get_name (UCD_ITEM (item));

  gtk_label_set_label (GTK_LABEL (label), name);
}

static void
bind_type (GtkSignalListItemFactory *factory,
           GObject                  *listitem)
{
  GtkWidget *label;
  GObject *item;
  gunichar codepoint;
  const char *types[] = {
    "Other, Control",
    "Other, Format",
    "Other, Not Assigned",
    "Other, Private Use",
    "Letter, Lowercase",
    "Letter, Modifier",
    "Letter, Other",
    "Letter, Titlecase",
    "Letter, Uppercase",
    "Mark, Spacing",
    "Mark, Enclosing",
    "Mark, Nonspacing",
    "Number, Decimal Digit",
    "Number, Letter",
    "Number, Other",
    "Punctuation, Connector",
    "Punctuation, Dash",
    "Punctuation, Close",
    "Punctuation, Final quote",
    "Punctuation, Initial quote",
    "Punctuation, Other",
    "Punctuation, Open",
    "Symbol, Currency",
    "Symbol, Modifier",
    "Symbol, Math",
    "Symbol, Other",
    "Separator, Line",
    "Separator, Paragraph",
    "Separator, Space",
  };

  label = gtk_list_item_get_child (GTK_LIST_ITEM (listitem));
  item = gtk_list_item_get_item (GTK_LIST_ITEM (listitem));
  codepoint = ucd_item_get_codepoint (UCD_ITEM (item));

  gtk_label_set_label (GTK_LABEL (label), types[g_unichar_type (codepoint)]);
}

static void
bind_break_type (GtkSignalListItemFactory *factory,
                 GObject                  *listitem)
{
  GtkWidget *label;
  GObject *item;
  gunichar codepoint;
  const char *break_types[] = {
    "Mandatory Break",
    "Carriage Return",
    "Line Feed",
    "Attached Characters and Combining Marks",
    "Surrogates",
    "Zero Width Space",
    "Inseparable",
    "Non-breaking (\"Glue\")",
    "Contingent Break Opportunity",
    "Space",
    "Break Opportunity After",
    "Break Opportunity Before",
    "Break Opportunity Before and After",
    "Hyphen",
    "Nonstarter",
    "Opening Punctuation",
    "Closing Punctuation",
    "Ambiguous Quotation",
    "Exclamation/Interrogation",
    "Ideographic",
    "Numeric",
    "Infix Separator (Numeric)",
    "Symbols Allowing Break After",
    "Ordinary Alphabetic and Symbol Characters",
    "Prefix (Numeric)",
    "Postfix (Numeric)",
    "Complex Content Dependent (South East Asian)",
    "Ambiguous (Alphabetic or Ideographic)",
    "Unknown",
    "Next Line",
    "Word Joiner",
    "Hangul L Jamo",
    "Hangul V Jamo",
    "Hangul T Jamo",
    "Hangul LV Syllable",
    "Hangul LVT Syllable",
    "Closing Parenthesis",
    "Conditional Japanese Starter",
    "Hebrew Letter",
    "Regional Indicator",
    "Emoji Base",
    "Emoji Modifier",
    "Zero Width Joiner",
  };

  label = gtk_list_item_get_child (GTK_LIST_ITEM (listitem));
  item = gtk_list_item_get_item (GTK_LIST_ITEM (listitem));
  codepoint = ucd_item_get_codepoint (UCD_ITEM (item));

  gtk_label_set_label (GTK_LABEL (label), break_types[g_unichar_break_type (codepoint)]);
}

static void
bind_combining_class (GtkSignalListItemFactory *factory,
                      GObject                  *listitem)
{
  GtkWidget *label;
  GObject *item;
  gunichar codepoint;
  const char *classes[255] = { 0, };

  classes[0] = "Not Reordered";
  classes[1] = "Overlay";
  classes[7] = "Nukta";
  classes[8] = "Kana Voicing";
  classes[9] = "Virama";
  classes[10] = "CCC10 (Hebrew)";
  classes[11] = "CCC11 (Hebrew)";
  classes[12] = "CCC12 (Hebrew)";
  classes[13] = "CCC13 (Hebrew)";
  classes[14] = "CCC14 (Hebrew)";
  classes[15] = "CCC15 (Hebrew)";
  classes[16] = "CCC16 (Hebrew)";
  classes[17] = "CCC17 (Hebrew)";
  classes[18] = "CCC18 (Hebrew)";
  classes[19] = "CCC19 (Hebrew)";
  classes[20] = "CCC20 (Hebrew)";
  classes[21] = "CCC21 (Hebrew)";
  classes[22] = "CCC22 (Hebrew)";
  classes[23] = "CCC23 (Hebrew)";
  classes[24] = "CCC24 (Hebrew)";
  classes[25] = "CCC25 (Hebrew)";
  classes[26] = "CCC26 (Hebrew)";

  classes[27] = "CCC27 (Arabic)";
  classes[28] = "CCC28 (Arabic)";
  classes[29] = "CCC29 (Arabic)";
  classes[30] = "CCC30 (Arabic)";
  classes[31] = "CCC31 (Arabic)";
  classes[32] = "CCC32 (Arabic)";
  classes[33] = "CCC33 (Arabic)";
  classes[34] = "CCC34 (Arabic)";
  classes[35] = "CCC35 (Arabic)";

  classes[36] = "CCC36 (Syriac)";

  classes[84] = "CCC84 (Telugu)";
  classes[85] = "CCC85 (Telugu)";

  classes[103] = "CCC103 (Thai)";
  classes[107] = "CCC107 (Thai)";

  classes[118] = "CCC118 (Lao)";
  classes[122] = "CCC122 (Lao)";

  classes[129] = "CCC129 (Tibetan)";
  classes[130] = "CCC130 (Tibetan)";
  classes[133] = "CCC133 (Tibetan)";

  classes[200] = "Attached Below Left";
  classes[202] = "Attached Below";
  classes[214] = "Attached Above";
  classes[216] = "Attached Above Right";
  classes[218] = "Below Left";
  classes[220] = "Below";
  classes[222] = "Below Right";
  classes[224] = "Left";
  classes[226] = "Right";
  classes[228] = "Above Left";
  classes[230] = "Above";
  classes[232] = "Above Right";
  classes[233] = "Double Below";
  classes[234] = "Double Above";
  classes[240] = "Iota Subscript";
  classes[255] = "Invalid";

  label = gtk_list_item_get_child (GTK_LIST_ITEM (listitem));
  item = gtk_list_item_get_item (GTK_LIST_ITEM (listitem));
  codepoint = ucd_item_get_codepoint (UCD_ITEM (item));

  gtk_label_set_label (GTK_LABEL (label), classes[g_unichar_combining_class (codepoint)]);
}

static void
bind_script (GtkSignalListItemFactory *factory,
             GObject                  *listitem)
{
  GtkWidget *label;
  GObject *item;
  gunichar codepoint;
  GUnicodeScript script;

  label = gtk_list_item_get_child (GTK_LIST_ITEM (listitem));
  item = gtk_list_item_get_item (GTK_LIST_ITEM (listitem));
  codepoint = ucd_item_get_codepoint (UCD_ITEM (item));
  script = g_unichar_get_script (codepoint);

  gtk_label_set_label (GTK_LABEL (label), get_script_name (script));
}

static void
selection_changed (GObject    *object,
                   GParamSpec *pspec,
                   GtkWidget  *label)
{
  UcdItem *item;
  guint codepoint;
  char buffer[16] = { 0, };

  item = gtk_single_selection_get_selected_item (GTK_SINGLE_SELECTION (object));
  codepoint = ucd_item_get_codepoint (item);

  if (g_unichar_isprint (codepoint))
    g_unichar_to_utf8 (codepoint, buffer);

  gtk_label_set_label (GTK_LABEL (label), buffer);
}

static GtkWidget *
create_ucd_view (GtkWidget *label)
{
  GtkWidget *cv;
  GListModel *ucd_model;
  GtkSingleSelection *selection;
  GtkListItemFactory *factory;
  GtkColumnViewColumn *column;

  ucd_model = ucd_model_new ();

  selection = gtk_single_selection_new (ucd_model);
  gtk_single_selection_set_autoselect (selection, TRUE);
  gtk_single_selection_set_can_unselect (selection, FALSE);
  g_signal_connect (selection, "notify::selected", G_CALLBACK (selection_changed), label);

  cv = gtk_column_view_new (GTK_SELECTION_MODEL (selection));
  gtk_column_view_set_show_column_separators (GTK_COLUMN_VIEW (cv), TRUE);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_centered_label), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_codepoint), NULL);
  column = gtk_column_view_column_new ("Codepoint", factory);
  gtk_column_view_append_column (GTK_COLUMN_VIEW (cv), column);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_centered_label), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_char), NULL);
  column = gtk_column_view_column_new ("Char", factory);
  gtk_column_view_append_column (GTK_COLUMN_VIEW (cv), column);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_label), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_name), NULL);
  column = gtk_column_view_column_new ("Name", factory);
  gtk_column_view_column_set_resizable (column, TRUE);
  gtk_column_view_append_column (GTK_COLUMN_VIEW (cv), column);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_label), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_type), NULL);
  column = gtk_column_view_column_new ("Type", factory);
  gtk_column_view_column_set_resizable (column, TRUE);
  gtk_column_view_append_column (GTK_COLUMN_VIEW (cv), column);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_label), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_break_type), NULL);
  column = gtk_column_view_column_new ("Break Type", factory);
  gtk_column_view_column_set_resizable (column, TRUE);
  gtk_column_view_append_column (GTK_COLUMN_VIEW (cv), column);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_label), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_combining_class), NULL);
  column = gtk_column_view_column_new ("Combining Class", factory);
  gtk_column_view_column_set_resizable (column, TRUE);
  gtk_column_view_append_column (GTK_COLUMN_VIEW (cv), column);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_label), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_script), NULL);
  column = gtk_column_view_column_new ("Script", factory);
  gtk_column_view_column_set_resizable (column, TRUE);
  gtk_column_view_append_column (GTK_COLUMN_VIEW (cv), column);

  return cv;
}

static GtkWidget *window;

GtkWidget *
do_listview_ucd (GtkWidget *do_widget)
{
  if (window == NULL)
    {
      GtkWidget *listview, *sw;
      GtkWidget *box, *label;
      GtkCssProvider *provider;

      window = gtk_window_new ();
      gtk_window_set_default_size (GTK_WINDOW (window), 800, 400);
      gtk_window_set_title (GTK_WINDOW (window), "Characters");
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *) &window);

      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      label = gtk_label_new ("");
      gtk_label_set_width_chars (GTK_LABEL (label), 2);
      gtk_widget_add_css_class (label, "enormous");
      provider = gtk_css_provider_new ();
      gtk_css_provider_load_from_data (provider, "label.enormous { font-size: 80px; }", -1);
      gtk_style_context_add_provider (gtk_widget_get_style_context (label), GTK_STYLE_PROVIDER (provider), 800);
      gtk_widget_set_hexpand (label, TRUE);
      gtk_box_append (GTK_BOX (box), label);

      sw = gtk_scrolled_window_new ();
      gtk_scrolled_window_set_propagate_natural_width (GTK_SCROLLED_WINDOW (sw), TRUE);
      listview = create_ucd_view (label);
      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), listview);
      gtk_box_prepend (GTK_BOX (box), sw);
      gtk_window_set_child (GTK_WINDOW (window), box);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}

