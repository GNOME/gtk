/* Pango/Font Features
 *
 * This example demonstrates support for OpenType font features with
 * Pango attributes. The attributes can be used manually or via Pango
 * markup.
 *
 * It can also be used to explore available features in OpenType fonts
 * and their effect.
 */

#include <gtk/gtk.h>

#if PANGO_VERSION_CHECK(1,44,0)
# if !HB_VERSION_ATLEAST(2,2,0)
#  define FONT_FEATURES_USE_PANGOFT2 1
# endif
#else
# define FONT_FEATURES_USE_PANGOFT2 1
#endif

#ifdef FONT_FEATURES_USE_PANGOFT2
#include <pango/pangofc-font.h>
#include <hb.h>
#include <hb-ot.h>
#include <hb-ft.h>
#else
#include <hb-ot.h>
#endif

static GtkWidget *label;
static GtkWidget *settings;
static GtkWidget *font;
static GtkWidget *script_lang;
static GtkWidget *resetbutton;
static GtkWidget *numcasedefault;
static GtkWidget *numspacedefault;
static GtkWidget *fractiondefault;
static GtkWidget *stack;
static GtkWidget *entry;

#define num_features 40

static GtkWidget *toggle[num_features];
static GtkWidget *icon[num_features];
static const char *feature_names[num_features] = {
  "kern", "liga", "dlig", "hlig", "clig", "smcp", "c2sc", "pcap", "c2pc", "unic",
  "cpsp", "case", "lnum", "onum", "pnum", "tnum", "frac", "afrc", "zero", "nalt",
  "sinf", "swsh", "cswh", "locl", "calt", "hist", "salt", "titl", "rand", "subs",
  "sups", "init", "medi", "fina", "isol", "ss01", "ss02", "ss03", "ss04", "ss05"
};

static void
update_display (void)
{
  GString *s;
  char *font_desc;
  char *font_settings;
  const char *text;
  gboolean has_feature;
  int i;
  hb_tag_t lang_tag;
  GtkTreeModel *model;
  GtkTreeIter iter;
  const char *lang;

  text = gtk_entry_get_text (GTK_ENTRY (entry));

  font_desc = gtk_font_chooser_get_font (GTK_FONT_CHOOSER (font));

  s = g_string_new ("");

  has_feature = FALSE;
  for (i = 0; i < num_features; i++)
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


  if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (script_lang), &iter))
    {
      model = gtk_combo_box_get_model (GTK_COMBO_BOX (script_lang));
      gtk_tree_model_get (model, &iter,
                          3, &lang_tag,
                          -1);

      lang = hb_language_to_string (hb_ot_tag_to_language (lang_tag));
    }
  else
    lang = NULL;

  s = g_string_new ("");
  g_string_append_printf (s, "<span font_desc='%s' font_features='%s'", font_desc, font_settings);
  if (lang)
    g_string_append_printf (s, " lang='%s'", lang);
  g_string_append_printf (s, ">%s</span>", text);

  gtk_label_set_markup (GTK_LABEL (label), s->str);

  g_string_free (s, TRUE);

  g_free (font_desc);
  g_free (font_settings);
}

static PangoFont *
get_pango_font (void)
{
  PangoFontDescription *desc;
  PangoContext *context;
  PangoFontMap *map;

  desc = gtk_font_chooser_get_font_desc (GTK_FONT_CHOOSER (font));
  context = gtk_widget_get_pango_context (font);
  map = pango_context_get_font_map (context);

  return pango_font_map_load_font (map, context, desc);
}

static struct { const char *name; hb_script_t script; } script_names[] = {
  { "Common", HB_SCRIPT_COMMON },
  { "Inherited", HB_SCRIPT_INHERITED },
  { "Unknown", HB_SCRIPT_UNKNOWN },
  { "Arabic", HB_SCRIPT_ARABIC },
  { "Armenian", HB_SCRIPT_ARMENIAN },
  { "Bengali", HB_SCRIPT_BENGALI },
  { "Cyrillic", HB_SCRIPT_CYRILLIC },
  { "Devanagari", HB_SCRIPT_DEVANAGARI },
  { "Georgian", HB_SCRIPT_GEORGIAN },
  { "Greek", HB_SCRIPT_GREEK },
  { "Gujarati", HB_SCRIPT_GUJARATI },
  { "Gurmukhi", HB_SCRIPT_GURMUKHI },
  { "Hangul", HB_SCRIPT_HANGUL },
  { "Han", HB_SCRIPT_HAN },
  { "Hebrew", HB_SCRIPT_HEBREW },
  { "Hiragana", HB_SCRIPT_HIRAGANA },
  { "Kannada", HB_SCRIPT_KANNADA },
  { "Katakana", HB_SCRIPT_KATAKANA },
  { "Lao", HB_SCRIPT_LAO },
  { "Latin", HB_SCRIPT_LATIN },
  { "Malayalam", HB_SCRIPT_MALAYALAM },
  { "Oriya", HB_SCRIPT_ORIYA },
  { "Tamil", HB_SCRIPT_TAMIL },
  { "Telugu", HB_SCRIPT_TELUGU },
  { "Thai", HB_SCRIPT_THAI },
  { "Tibetan", HB_SCRIPT_TIBETAN },
  { "Bopomofo", HB_SCRIPT_BOPOMOFO }
  /* FIXME: complete */
};

static struct { const char *name; hb_tag_t tag; } language_names[] = {
  { "Arabic", HB_TAG ('A','R','A',' ') },
  { "Romanian", HB_TAG ('R','O','M',' ') },
  { "Skolt Sami", HB_TAG ('S','K','S',' ') },
  { "Northern Sami", HB_TAG ('N','S','M',' ') },
  { "Kildin Sami", HB_TAG ('K','S','M',' ') },
  { "Moldavian", HB_TAG ('M','O','L',' ') },
  { "Turkish", HB_TAG ('T','R','K',' ') },
  { "Azerbaijani", HB_TAG ('A','Z','E',' ') },
  { "Crimean Tatar", HB_TAG ('C','R','T',' ') },
  { "Serbian", HB_TAG ('S','R','B',' ') },
  { "German", HB_TAG ('D','E','U',' ') }
  /* FIXME: complete */
};

typedef struct {
  hb_tag_t script_tag;
  hb_tag_t lang_tag;
  unsigned int script_index;
  unsigned int lang_index;
} TagPair;

static guint
tag_pair_hash (gconstpointer data)
{
  const TagPair *pair = data;

  return pair->script_tag + pair->lang_tag;
}

static gboolean
tag_pair_equal (gconstpointer a, gconstpointer b)
{
  const TagPair *pair_a = a;
  const TagPair *pair_b = b;

  return pair_a->script_tag == pair_b->script_tag && pair_a->lang_tag == pair_b->lang_tag;
}

static void
update_script_combo (void)
{
  GtkListStore *store;
  hb_font_t *hb_font = NULL;
  gint i, j, k, l;
  PangoFont *pango_font;
  GHashTable *tags;
  GHashTableIter iter;
  TagPair *pair;
  gboolean cleanup_hb_face = FALSE;

#ifdef FONT_FEATURES_USE_PANGOFT2
  FT_Face ft_face;
#endif

  store = gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT);

  pango_font = get_pango_font ();

#ifdef FONT_FEATURES_USE_PANGOFT2
  if (PANGO_IS_FC_FONT (pango_font))
    {
      ft_face = pango_fc_font_lock_face (PANGO_FC_FONT (pango_font)),
      hb_font = hb_ft_font_create (ft_face, NULL);
      cleanup_hb_face = TRUE;
    }
#else
  hb_font = pango_font_get_hb_font (pango_font);
#endif

  tags = g_hash_table_new_full (tag_pair_hash, tag_pair_equal, g_free, NULL);

  pair = g_new (TagPair, 1);
  pair->script_tag = HB_OT_TAG_DEFAULT_SCRIPT;
  pair->lang_tag = HB_OT_TAG_DEFAULT_LANGUAGE;
  g_hash_table_insert (tags, pair, g_strdup ("Default"));

  if (hb_font)
    {
      hb_tag_t tables[2] = { HB_OT_TAG_GSUB, HB_OT_TAG_GPOS };
      hb_face_t *hb_face;

      hb_face = hb_font_get_face (hb_font);

      for (i= 0; i < 2; i++)
        {
          hb_tag_t scripts[80];
          unsigned int script_count = G_N_ELEMENTS (scripts);

          hb_ot_layout_table_get_script_tags (hb_face, tables[i], 0, &script_count, scripts);
          for (j = 0; j < script_count; j++)
            {
              hb_tag_t languages[80];
              unsigned int language_count = G_N_ELEMENTS (languages);

              pair = g_new (TagPair, 1);
              pair->script_tag = scripts[j];
              pair->lang_tag = HB_OT_TAG_DEFAULT_LANGUAGE;
              pair->script_index = j;
              pair->lang_index = HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX;
              g_hash_table_add (tags, pair);

              hb_ot_layout_script_get_language_tags (hb_face, tables[i], j, 0, &language_count, languages);
              for (k = 0; k < language_count; k++)
                {
                  pair = g_new (TagPair, 1);
                  pair->script_tag = scripts[j];
                  pair->lang_tag = languages[k];
                  pair->script_index = j;
                  pair->lang_index = k;
                  g_hash_table_add (tags, pair);
                }
            }
        }

      if (cleanup_hb_face)
        hb_face_destroy (hb_face);
    }

#ifdef FONT_FEATURES_USE_PANGOFT2
  if (PANGO_IS_FC_FONT (pango_font))
    pango_fc_font_unlock_face (PANGO_FC_FONT (pango_font));
#endif

  g_object_unref (pango_font);

  g_hash_table_iter_init (&iter, tags);
  while (g_hash_table_iter_next (&iter, (gpointer *)&pair, NULL))
    {
      const char *scriptname;
      char scriptbuf[5];
      const char *langname;
      char langbuf[5];
      char *name;

      if (pair->script_tag == HB_OT_TAG_DEFAULT_SCRIPT)
        scriptname = "Default";
      else if (pair->script_tag == HB_TAG ('m','a','t','h'))
        scriptname = "Math";
      else
        {
          hb_script_t script;

          hb_tag_to_string (pair->script_tag, scriptbuf);
          scriptbuf[4] = 0;
          scriptname = scriptbuf;

          script = hb_script_from_iso15924_tag (pair->script_tag);
          for (k = 0; k < G_N_ELEMENTS (script_names); k++)
            {
              if (script == script_names[k].script)
                {
                  scriptname = script_names[k].name;
                  break;
                }
            }
        }

      if (pair->lang_tag == HB_OT_TAG_DEFAULT_LANGUAGE)
        langname = "Default";
      else
        {
          hb_tag_to_string (pair->lang_tag, langbuf);
          langbuf[4] = 0;
          langname = langbuf;

          for (l = 0; l < G_N_ELEMENTS (language_names); l++)
            {
              if (pair->lang_tag == language_names[l].tag)
                {
                  langname = language_names[l].name;
                  break;
                }
            }
        }

      name = g_strdup_printf ("%s - %s", scriptname, langname);

      gtk_list_store_insert_with_values (store, NULL, -1,
                                         0, name,
                                         1, pair->script_index,
                                         2, pair->lang_index,
                                         3, pair->lang_tag,
                                         -1);

      g_free (name);
    }

  g_hash_table_destroy (tags);

  gtk_combo_box_set_model (GTK_COMBO_BOX (script_lang), GTK_TREE_MODEL (store));
  gtk_combo_box_set_active (GTK_COMBO_BOX (script_lang), 0);
}

static void
update_features (void)
{
  gint i, j, k;
  GtkTreeModel *model;
  GtkTreeIter iter;
  guint script_index, lang_index;
  PangoFont *pango_font;
  hb_font_t *hb_font = NULL;
  gboolean cleanup_hb_face = FALSE;

#ifdef FONT_FEATURES_USE_PANGOFT2
  FT_Face ft_face;
#endif

  for (i = 0; i < num_features; i++)
    gtk_widget_set_opacity (icon[i], 0);

  /* set feature presence checks from the font features */

  if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (script_lang), &iter))
    return;

  model = gtk_combo_box_get_model (GTK_COMBO_BOX (script_lang));
  gtk_tree_model_get (model, &iter,
                      1, &script_index,
                      2, &lang_index,
                      -1);

  pango_font = get_pango_font ();

#ifdef FONT_FEATURES_USE_PANGOFT2
  if (PANGO_IS_FC_FONT (pango_font))
    {
      ft_face = pango_fc_font_lock_face (PANGO_FC_FONT (pango_font)),
      hb_font = hb_ft_font_create (ft_face, NULL);
      cleanup_hb_face = TRUE;
    }
#else
  hb_font = pango_font_get_hb_font (pango_font);
#endif

  if (hb_font)
    {
      hb_tag_t tables[2] = { HB_OT_TAG_GSUB, HB_OT_TAG_GPOS };
      hb_face_t *hb_face;

      hb_face = hb_font_get_face (hb_font);

      for (i = 0; i < 2; i++)
        {
          hb_tag_t features[80];
          unsigned int count = G_N_ELEMENTS(features);

          hb_ot_layout_language_get_feature_tags (hb_face,
                                                  tables[i],
                                                  script_index,
                                                  lang_index,
                                                  0,
                                                  &count,
                                                  features);

          for (j = 0; j < count; j++)
            {
              for (k = 0; k < num_features; k++)
                {
                  if (hb_tag_from_string (feature_names[k], -1) == features[j])
                    gtk_widget_set_opacity (icon[k], 0.5);
                }
            }
        }

      if (cleanup_hb_face)
        hb_face_destroy (hb_face);
    }

#ifdef FONT_FEATURES_USE_PANGOFT2
  if (PANGO_IS_FC_FONT (pango_font))
    pango_fc_font_unlock_face (PANGO_FC_FONT (pango_font));
#endif

  g_object_unref (pango_font);
}

static void
font_changed (void)
{
  update_script_combo ();
}

static void
script_changed (void)
{
  update_features ();
  update_display ();
}

static void
reset_features (void)
{
  int i;

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (numcasedefault), TRUE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (numspacedefault), TRUE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (fractiondefault), TRUE);
  for (i = 0; i < num_features; i++)
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
  update_display ();
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

      builder = gtk_builder_new_from_resource ("/font_features/font-features.ui");

      gtk_builder_add_callback_symbol (builder, "update_display", update_display);
      gtk_builder_add_callback_symbol (builder, "font_changed", font_changed);
      gtk_builder_add_callback_symbol (builder, "script_changed", script_changed);
      gtk_builder_add_callback_symbol (builder, "reset", reset_features);
      gtk_builder_add_callback_symbol (builder, "switch_to_entry", switch_to_entry);
      gtk_builder_add_callback_symbol (builder, "switch_to_label", switch_to_label);
      gtk_builder_add_callback_symbol (builder, "entry_key_press", G_CALLBACK (entry_key_press));
      gtk_builder_connect_signals (builder, NULL);

      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      label = GTK_WIDGET (gtk_builder_get_object (builder, "label"));
      settings = GTK_WIDGET (gtk_builder_get_object (builder, "settings"));
      resetbutton = GTK_WIDGET (gtk_builder_get_object (builder, "reset"));
      font = GTK_WIDGET (gtk_builder_get_object (builder, "font"));
      script_lang = GTK_WIDGET (gtk_builder_get_object (builder, "script_lang"));
      numcasedefault = GTK_WIDGET (gtk_builder_get_object (builder, "numcasedefault"));
      numspacedefault = GTK_WIDGET (gtk_builder_get_object (builder, "numspacedefault"));
      fractiondefault = GTK_WIDGET (gtk_builder_get_object (builder, "fractiondefault"));
      stack = GTK_WIDGET (gtk_builder_get_object (builder, "stack"));
      entry = GTK_WIDGET (gtk_builder_get_object (builder, "entry"));

      for (i = 0; i < num_features; i++)
        {
          char *iname;

          toggle[i] = GTK_WIDGET (gtk_builder_get_object (builder, feature_names[i]));
          iname = g_strconcat (feature_names[i], "_pres", NULL);
          icon[i] = GTK_WIDGET (gtk_builder_get_object (builder, iname));
          g_free (iname);
        }

      font_changed ();

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
