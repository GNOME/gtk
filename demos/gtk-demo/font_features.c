/* Pango/Font Explorer
 *
 * This example demonstrates support for OpenType font features with
 * Pango attributes. The attributes can be used manually or via Pango
 * markup.
 *
 * It can also be used to explore available features in OpenType fonts
 * and their effect.
 *
 * If the selected font supports OpenType font variations, then the
 * axes are also offered for customization.
 */

#include <gtk/gtk.h>
#include <pango/pangofc-font.h>
#include <hb.h>
#include <hb-ot.h>
#include <hb-ft.h>
#include <freetype/ftmm.h>
#include <freetype/ftsnames.h>
#include <freetype/ttnameid.h>
#include <glib/gi18n.h>

#include "open-type-layout.h"
#include "fontplane.h"
#include "script-names.h"
#include "language-names.h"


#define MAKE_TAG(a,b,c,d) (unsigned int)(((a) << 24) | ((b) << 16) | ((c) <<  8) | (d))

static GtkWidget *label;
static GtkWidget *settings;
static GtkWidget *description;
static GtkWidget *font;
static GtkWidget *script_lang;
static GtkWidget *resetbutton;
static GtkWidget *stack;
static GtkWidget *entry;
static GtkWidget *variations_heading;
static GtkWidget *variations_grid;
static GtkWidget *instance_combo;
static GtkWidget *edit_toggle;

typedef struct {
  unsigned int tag;
  const char *name;
  GtkWidget *icon;
  GtkWidget *dflt;
  GtkWidget *feat;
} FeatureItem;

static GList *feature_items;

typedef struct {
  unsigned int start;
  unsigned int end;
  PangoFontDescription *desc;
  char *features;
  PangoLanguage *language;
} Range;

static GList *ranges;

static void add_font_variations (GString *s);

static void
free_range (gpointer data)
{
  Range *range = data;

  if (range->desc)
    pango_font_description_free (range->desc);
  g_free (range->features);
  g_free (range);
}

static int
compare_range (gconstpointer a, gconstpointer b)
{
  const Range *ra = a;
  const Range *rb = b;

  if (ra->start < rb->start)
    return -1;
  else if (ra->start > rb->start)
    return 1;
  else if (ra->end < rb->end)
    return 1;
  else if (ra->end > rb->end)
    return -1;

  return 0;
}

static void
ensure_range (unsigned int          start,
              unsigned int          end,
              PangoFontDescription *desc,
              const char           *features,
              PangoLanguage        *language)
{
  GList *l;
  Range *range;

  for (l = ranges; l; l = l->next)
    {
      Range *r = l->data;

      if (r->start == start && r->end == end)
        {
          range = r;
          goto set;
        }
    }

  range = g_new0 (Range, 1);
  range->start = start;
  range->end = end;

  ranges = g_list_insert_sorted (ranges, range, compare_range);

set:
  if (range->desc)
    pango_font_description_free (range->desc);
  if (desc)
    range->desc = pango_font_description_copy (desc);
  g_free (range->features);
  range->features = g_strdup (features);
  range->language = language;
}

static const char *
get_feature_display_name (unsigned int tag)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (open_type_layout_features); i++)
    {
      if (tag == open_type_layout_features[i].tag)
        return g_dpgettext2 (NULL, "OpenType layout", open_type_layout_features[i].name);
    }

  return NULL;
}

static void update_display (void);

static void
set_inconsistent (GtkCheckButton *button,
                  gboolean        inconsistent)
{
  gtk_check_button_set_inconsistent (GTK_CHECK_BUTTON (button), inconsistent);
  gtk_widget_set_opacity (gtk_widget_get_first_child (GTK_WIDGET (button)), inconsistent ? 0.0 : 1.0);
}

static void
feat_clicked (GtkWidget *feat,
              gpointer   data)
{
  g_signal_handlers_block_by_func (feat, feat_clicked, NULL);

  if (gtk_check_button_get_inconsistent (GTK_CHECK_BUTTON (feat)))
    {
      set_inconsistent (GTK_CHECK_BUTTON (feat), FALSE);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (feat), TRUE);
    }
  else if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (feat)))
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (feat), FALSE);
    }
  else
    {
      set_inconsistent (GTK_CHECK_BUTTON (feat), TRUE);
    }

  g_signal_handlers_unblock_by_func (feat, feat_clicked, NULL);
}

static void
add_check_group (GtkWidget   *box,
                 const char  *title,
                 const char **tags)
{
  GtkWidget *label;
  GtkWidget *group;
  PangoAttrList *attrs;
  int i;

  group = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_halign (group, GTK_ALIGN_START);

  label = gtk_label_new (title);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  g_object_set (label, "margin-top", 10, "margin-bottom", 10, NULL);
  attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
  gtk_label_set_attributes (GTK_LABEL (label), attrs);
  pango_attr_list_unref (attrs);
  gtk_container_add (GTK_CONTAINER (group), label);

  for (i = 0; tags[i]; i++)
    {
      unsigned int tag;
      GtkWidget *feat;
      FeatureItem *item;

      tag = hb_tag_from_string (tags[i], -1);

      feat = gtk_check_button_new_with_label (get_feature_display_name (tag));
      set_inconsistent (GTK_CHECK_BUTTON (feat), TRUE);

      g_signal_connect (feat, "notify::active", G_CALLBACK (update_display), NULL);
      g_signal_connect (feat, "notify::inconsistent", G_CALLBACK (update_display), NULL);
      g_signal_connect (feat, "clicked", G_CALLBACK (feat_clicked), NULL);

      gtk_container_add (GTK_CONTAINER (group), feat);

      item = g_new (FeatureItem, 1);
      item->name = tags[i];
      item->tag = tag;
      item->icon = NULL;
      item->dflt = NULL;
      item->feat = feat;

      feature_items = g_list_prepend (feature_items, item);
    }

  gtk_container_add (GTK_CONTAINER (box), group);
}

static void
add_radio_group (GtkWidget *box,
                 const char  *title,
                 const char **tags)
{
  GtkWidget *label;
  GtkWidget *group;
  int i;
  GtkWidget *group_button = NULL;
  PangoAttrList *attrs;

  group = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_halign (group, GTK_ALIGN_START);

  label = gtk_label_new (title);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  g_object_set (label, "margin-top", 10, "margin-bottom", 10, NULL);
  attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
  gtk_label_set_attributes (GTK_LABEL (label), attrs);
  pango_attr_list_unref (attrs);
  gtk_container_add (GTK_CONTAINER (group), label);

  for (i = 0; tags[i]; i++)
    {
      unsigned int tag;
      GtkWidget *feat;
      FeatureItem *item;
      const char *name;

      tag = hb_tag_from_string (tags[i], -1);
      name = get_feature_display_name (tag);

      feat = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (group_button),
                                                          name ? name : _("Default"));
      if (group_button == NULL)
        group_button = feat;

      g_signal_connect (feat, "notify::active", G_CALLBACK (update_display), NULL);
      g_object_set_data (G_OBJECT (feat), "default", group_button);

      gtk_container_add (GTK_CONTAINER (group), feat);

      item = g_new (FeatureItem, 1);
      item->name = tags[i];
      item->tag = tag;
      item->icon = NULL;
      item->dflt = NULL;
      item->feat = feat;

      feature_items = g_list_prepend (feature_items, item);
    }

  gtk_container_add (GTK_CONTAINER (box), group);
}

static void
update_display (void)
{
  GString *s;
  const char *text;
  gboolean has_feature;
  GtkTreeIter iter;
  GtkTreeModel *model;
  PangoFontDescription *desc;
  GList *l;
  PangoAttrList *attrs;
  PangoAttribute *attr;
  gint ins, bound;
  guint start, end;
  PangoLanguage *lang;
  char *font_desc;
  char *features;

  text = gtk_entry_get_text (GTK_ENTRY (entry));

  if (gtk_label_get_selection_bounds (GTK_LABEL (label), &ins, &bound))
    {
      start = g_utf8_offset_to_pointer (text, ins) - text;
      end = g_utf8_offset_to_pointer (text, bound) - text;
    }
  else
    {
      start = PANGO_ATTR_INDEX_FROM_TEXT_BEGINNING;
      end = PANGO_ATTR_INDEX_TO_TEXT_END;
    }

  desc = gtk_font_chooser_get_font_desc (GTK_FONT_CHOOSER (font));

  s = g_string_new ("");
  add_font_variations (s);
  if (s->len > 0)
    {
      pango_font_description_set_variations (desc, s->str);
      g_string_free (s, TRUE);
    }

  font_desc = pango_font_description_to_string (desc);

  s = g_string_new ("");

  has_feature = FALSE;
  for (l = feature_items; l; l = l->next)
    {
      FeatureItem *item = l->data;

      if (!gtk_widget_is_sensitive (item->feat))
        continue;

      if (GTK_IS_RADIO_BUTTON (item->feat))
        {
          if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (item->feat)) &&
              strcmp (item->name, "xxxx") != 0)
            {
              if (has_feature)
                g_string_append (s, ", ");
              g_string_append (s, item->name);
              g_string_append (s, " 1");
              has_feature = TRUE;
            }
        }
      else if (GTK_IS_CHECK_BUTTON (item->feat))
        {
          if (gtk_check_button_get_inconsistent (GTK_CHECK_BUTTON (item->feat)))
            continue;

          if (has_feature)
            g_string_append (s, ", ");
          g_string_append (s, item->name);
          if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (item->feat)))
            g_string_append (s, " 1");
          else
            g_string_append (s, " 0");
          has_feature = TRUE;
        }
    }

  features = g_string_free (s, FALSE);

  if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (script_lang), &iter))
    {
      hb_tag_t lang_tag;

      model = gtk_combo_box_get_model (GTK_COMBO_BOX (script_lang));
      gtk_tree_model_get (model, &iter,
                          3, &lang_tag,
                          -1);

      lang = pango_language_from_string (hb_language_to_string (hb_ot_tag_to_language (lang_tag)));
    }
  else
    lang = NULL;

  ensure_range (start, end, desc, features, lang);

  attrs = pango_attr_list_new ();

  for (l = ranges; l; l = l->next)
    {
      Range *range = l->data;

      attr = pango_attr_font_desc_new (range->desc);
      attr->start_index = range->start;
      attr->end_index = range->end;
      pango_attr_list_insert (attrs, attr);

      attr = pango_attr_font_features_new (range->features);
      attr->start_index = range->start;
      attr->end_index = range->end;
      pango_attr_list_insert (attrs, attr);

      if (range->language)
        {
          attr = pango_attr_language_new (range->language);
          attr->start_index = range->start;
          attr->end_index = range->end;
          pango_attr_list_insert (attrs, attr);
        }
    }

  gtk_label_set_text (GTK_LABEL (description), font_desc);
  gtk_label_set_text (GTK_LABEL (settings), features);
  gtk_label_set_text (GTK_LABEL (label), text);
  gtk_label_set_attributes (GTK_LABEL (label), attrs);

  g_free (font_desc);
  pango_font_description_free (desc);
  g_free (features);
  pango_attr_list_unref (attrs);
}

static PangoFont *
get_pango_font (void)
{
  PangoFontDescription *desc;
  PangoContext *context;

  desc = gtk_font_chooser_get_font_desc (GTK_FONT_CHOOSER (font));
  context = gtk_widget_get_pango_context (font);

  return pango_context_load_font (context, desc);
}

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

static int
script_sort_func (GtkTreeModel *model,
                  GtkTreeIter  *a,
                  GtkTreeIter  *b,
                  gpointer      user_data)
{
  char *sa, *sb;
  int ret;

  gtk_tree_model_get (model, a, 0, &sa, -1);
  gtk_tree_model_get (model, b, 0, &sb, -1);

  ret = strcmp (sa, sb);

  g_free (sa);
  g_free (sb);

  return ret;
}

static void
update_script_combo (void)
{
  GtkListStore *store;
  hb_font_t *hb_font;
  gint i, j, k;
  FT_Face ft_face;
  PangoFont *pango_font;
  GHashTable *tags;
  GHashTableIter iter;
  TagPair *pair;
  char *lang;
  hb_tag_t active;
  GtkTreeIter active_iter;
  gboolean have_active = FALSE;

  lang = gtk_font_chooser_get_language (GTK_FONT_CHOOSER (font));
  active = hb_ot_tag_from_language (hb_language_from_string (lang, -1));
  g_free (lang);

  store = gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT);

  pango_font = get_pango_font ();
  ft_face = pango_fc_font_lock_face (PANGO_FC_FONT (pango_font)),
  hb_font = hb_ft_font_create (ft_face, NULL);

  tags = g_hash_table_new_full (tag_pair_hash, tag_pair_equal, g_free, NULL);

  pair = g_new (TagPair, 1);
  pair->script_tag = HB_OT_TAG_DEFAULT_SCRIPT;
  pair->lang_tag = HB_OT_TAG_DEFAULT_LANGUAGE;
  g_hash_table_add (tags, pair);

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

      hb_face_destroy (hb_face);
    }

  pango_fc_font_unlock_face (PANGO_FC_FONT (pango_font));
  g_object_unref (pango_font);

  g_hash_table_iter_init (&iter, tags);
  while (g_hash_table_iter_next (&iter, (gpointer *)&pair, NULL))
    {
      const char *langname;
      char langbuf[5];
      GtkTreeIter iter;

      if (pair->lang_tag == HB_OT_TAG_DEFAULT_LANGUAGE)
        langname = NC_("Language", "Default");
      else
        {
          langname = get_language_name_for_tag (pair->lang_tag);
          if (!langname)
            {
              hb_tag_to_string (pair->lang_tag, langbuf);
              langbuf[4] = 0;
              langname = langbuf;
            }
        }

      gtk_list_store_insert_with_values (store, &iter, -1,
                                         0, langname,
                                         1, pair->script_index,
                                         2, pair->lang_index,
                                         3, pair->lang_tag,
                                         -1);
      if (pair->lang_tag == active)
        {
          have_active = TRUE;
          active_iter = iter;
        }
    }

  g_hash_table_destroy (tags);

  gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (store),
                                           script_sort_func, NULL, NULL);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
                                        GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
                                        GTK_SORT_ASCENDING);
  gtk_combo_box_set_model (GTK_COMBO_BOX (script_lang), GTK_TREE_MODEL (store));
  if (have_active)
    gtk_combo_box_set_active_iter (GTK_COMBO_BOX (script_lang), &active_iter);
  else
    gtk_combo_box_set_active_iter (GTK_COMBO_BOX (script_lang), 0);
}

static void
update_features (void)
{
  gint i, j;
  GtkTreeModel *model;
  GtkTreeIter iter;
  guint script_index, lang_index;
  PangoFont *pango_font;
  FT_Face ft_face;
  hb_font_t *hb_font;
  GList *l;

  for (l = feature_items; l; l = l->next)
    {
      FeatureItem *item = l->data;
      gtk_widget_hide (item->feat);
      gtk_widget_hide (gtk_widget_get_parent (item->feat));
      if (strcmp (item->name, "xxxx") == 0)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item->feat), TRUE);
    }

  /* set feature presence checks from the font features */

  if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (script_lang), &iter))
    return;

  model = gtk_combo_box_get_model (GTK_COMBO_BOX (script_lang));
  gtk_tree_model_get (model, &iter,
                      1, &script_index,
                      2, &lang_index,
                      -1);

  pango_font = get_pango_font ();
  ft_face = pango_fc_font_lock_face (PANGO_FC_FONT (pango_font)),
  hb_font = hb_ft_font_create (ft_face, NULL);

  if (hb_font)
    {
      hb_tag_t tables[2] = { HB_OT_TAG_GSUB, HB_OT_TAG_GPOS };
      hb_face_t *hb_face;
      char *feat;

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
#if 0
              char buf[5];
              hb_tag_to_string (features[j], buf);
              buf[4] = 0;
              g_print ("%s present in %s\n", buf, i == 0 ? "GSUB" : "GPOS");
#endif
              for (l = feature_items; l; l = l->next)
                {
                  FeatureItem *item = l->data;

                  if (item->tag == features[j])
                    {
                      gtk_widget_show (item->feat);
                      gtk_widget_show (gtk_widget_get_parent (item->feat));
                      if (GTK_IS_RADIO_BUTTON (item->feat))
                        {
                          GtkWidget *def = GTK_WIDGET (g_object_get_data (G_OBJECT (item->feat), "default"));
                          gtk_widget_show (def);
                        }
                      else if (GTK_IS_CHECK_BUTTON (item->feat))
                        {
                          set_inconsistent (GTK_CHECK_BUTTON (item->feat), TRUE);
                        }
                    }
                }
            }
        }

      feat = gtk_font_chooser_get_font_features (GTK_FONT_CHOOSER (font));
      if (feat)
        {
          for (l = feature_items; l; l = l->next)
            {
              FeatureItem *item = l->data;
              char buf[5];
              char *p;

              hb_tag_to_string (item->tag, buf);
              buf[4] = 0;

              p = strstr (feat, buf);
              if (p)
                {
                  if (GTK_IS_RADIO_BUTTON (item->feat))
                    {
                      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item->feat), p[6] == '1');
                    }
                  else if (GTK_IS_CHECK_BUTTON (item->feat))
                    {
                      set_inconsistent (GTK_CHECK_BUTTON (item->feat), FALSE);
                      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item->feat), p[6] == '1');
                    }
                }
            }

          g_free (feat);
        }

      hb_face_destroy (hb_face);
    }

  pango_fc_font_unlock_face (PANGO_FC_FONT (pango_font));
  g_object_unref (pango_font);
}

#define FixedToFloat(f) (((float)(f))/65536.0)

static void
adjustment_changed (GtkAdjustment *adjustment,
                    GtkEntry      *entry)
{
  char *str;

  str = g_strdup_printf ("%g", gtk_adjustment_get_value (adjustment));
  gtk_entry_set_text (GTK_ENTRY (entry), str);
  g_free (str);

  update_display ();
}

static void
entry_activated (GtkEntry *entry,
                 GtkAdjustment *adjustment)
{
  gdouble value;
  gchar *err = NULL;

  value = g_strtod (gtk_entry_get_text (entry), &err);
  if (err != NULL)
    gtk_adjustment_set_value (adjustment, value);
}

static void unset_instance (GtkAdjustment *adjustment);

typedef struct {
  guint32 tag;
  GtkAdjustment *adjustment;
} Axis;

static GHashTable *axes;

static void
add_font_variations (GString *s)
{
  GHashTableIter iter;
  Axis *axis;
  char buf[G_ASCII_DTOSTR_BUF_SIZE];
  char *sep = "";

  g_hash_table_iter_init (&iter, axes);
  while (g_hash_table_iter_next (&iter, (gpointer *)NULL, (gpointer *)&axis))
    {
      char tag[5];
      double value;

      hb_tag_to_string (axis->tag, tag);
      tag[4] = '\0';
      value = gtk_adjustment_get_value (axis->adjustment);

      g_string_append_printf (s, "%s%s=%s", sep, tag, g_ascii_dtostr (buf, sizeof (buf), value));
      sep = ",";
    }
}

static guint
axes_hash (gconstpointer v)
{
  const Axis *p = v;

  return p->tag;
}

static gboolean
axes_equal (gconstpointer v1, gconstpointer v2)
{
  const Axis *p1 = v1;
  const Axis *p2 = v2;

  return p1->tag == p2->tag;
}

static void
add_axis (FT_Var_Axis *ax, FT_Fixed value, int i)
{
  GtkWidget *axis_label;
  GtkWidget *axis_entry;
  GtkWidget *axis_scale;
  GtkAdjustment *adjustment;
  Axis *axis;

  axis_label = gtk_label_new (ax->name);
  gtk_widget_set_halign (axis_label, GTK_ALIGN_START);
  gtk_widget_set_valign (axis_label, GTK_ALIGN_BASELINE);
  gtk_grid_attach (GTK_GRID (variations_grid), axis_label, 0, i, 1, 1);
  adjustment = gtk_adjustment_new ((double)FixedToFloat(value),
                                   (double)FixedToFloat(ax->minimum),
                                   (double)FixedToFloat(ax->maximum),
                                   1.0, 10.0, 0.0);
  axis_scale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, adjustment);
  gtk_scale_add_mark (GTK_SCALE (axis_scale), (double)FixedToFloat(ax->def), GTK_POS_TOP, NULL);
  gtk_widget_set_valign (axis_scale, GTK_ALIGN_BASELINE);
  gtk_widget_set_hexpand (axis_scale, TRUE);
  gtk_widget_set_size_request (axis_scale, 100, -1);
  gtk_scale_set_draw_value (GTK_SCALE (axis_scale), FALSE);
  gtk_grid_attach (GTK_GRID (variations_grid), axis_scale, 1, i, 1, 1);
  axis_entry = gtk_entry_new ();
  gtk_widget_set_valign (axis_entry, GTK_ALIGN_BASELINE);
  gtk_entry_set_width_chars (GTK_ENTRY (axis_entry), 4);
  gtk_grid_attach (GTK_GRID (variations_grid), axis_entry, 2, i, 1, 1);

  axis = g_new (Axis, 1);
  axis->tag = ax->tag;
  axis->adjustment = adjustment;
  g_hash_table_add (axes, axis);

  adjustment_changed (adjustment, GTK_ENTRY (axis_entry));

  g_signal_connect (adjustment, "value-changed", G_CALLBACK (adjustment_changed), axis_entry);
  g_signal_connect (adjustment, "value-changed", G_CALLBACK (unset_instance), NULL);
  g_signal_connect (axis_entry, "activate", G_CALLBACK (entry_activated), adjustment);
}

typedef struct {
  char *name;
  int n_axes;
  guint32 *axes;
  float   *coords;
} Instance;

static guint
instance_hash (gconstpointer v)
{
  const Instance *p = v;

  return g_str_hash (p->name);
}

static gboolean
instance_equal (gconstpointer v1, gconstpointer v2)
{
  const Instance *p1 = v1;
  const Instance *p2 = v2;

  return g_str_equal (p1->name, p2->name);
}

static void
free_instance (gpointer data)
{
  Instance *instance = data;

  g_free (instance->name);
  g_free (instance->axes);
  g_free (instance->coords);
  g_free (instance);
}

static GHashTable *instances;

typedef struct {
    const FT_UShort     platform_id;
    const FT_UShort     encoding_id;
    const char  fromcode[12];
} FtEncoding;

#define TT_ENCODING_DONT_CARE   0xffff

static const FtEncoding   ftEncoding[] = {
 {  TT_PLATFORM_APPLE_UNICODE,  TT_ENCODING_DONT_CARE,  "UTF-16BE" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_ID_ROMAN,        "MACINTOSH" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_ID_JAPANESE,     "SJIS" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_ID_SYMBOL_CS,     "UTF-16BE" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_ID_UNICODE_CS,    "UTF-16BE" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_ID_SJIS,          "SJIS-WIN" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_ID_GB2312,        "GB2312" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_ID_BIG_5,         "BIG-5" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_ID_WANSUNG,       "Wansung" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_ID_JOHAB,         "Johab" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_ID_UCS_4,         "UTF-16BE" },
 {  TT_PLATFORM_ISO,            TT_ISO_ID_7BIT_ASCII,   "ASCII" },
 {  TT_PLATFORM_ISO,            TT_ISO_ID_10646,        "UTF-16BE" },
 {  TT_PLATFORM_ISO,            TT_ISO_ID_8859_1,       "ISO-8859-1" },
};

typedef struct {
    const FT_UShort     platform_id;
    const FT_UShort     language_id;
    const char  lang[8];
} FtLanguage;

#define TT_LANGUAGE_DONT_CARE   0xffff

static const FtLanguage   ftLanguage[] = {
 {  TT_PLATFORM_APPLE_UNICODE,  TT_LANGUAGE_DONT_CARE,              "" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_ENGLISH,              "en" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_FRENCH,               "fr" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_GERMAN,               "de" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_ITALIAN,              "it" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_DUTCH,                "nl" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_SWEDISH,              "sv" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_SPANISH,              "es" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_DANISH,               "da" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_PORTUGUESE,           "pt" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_NORWEGIAN,            "no" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_HEBREW,               "he" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_JAPANESE,             "ja" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_ARABIC,               "ar" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_FINNISH,              "fi" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_GREEK,                "el" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_ICELANDIC,            "is" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_MALTESE,              "mt" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_TURKISH,              "tr" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_CROATIAN,             "hr" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_CHINESE_TRADITIONAL,  "zh-tw" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_URDU,                 "ur" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_HINDI,                "hi" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_THAI,                 "th" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_KOREAN,               "ko" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_LITHUANIAN,           "lt" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_POLISH,               "pl" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_HUNGARIAN,            "hu" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_ESTONIAN,             "et" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_LETTISH,              "lv" },
/* {  TT_PLATFORM_MACINTOSH,    TT_MAC_LANGID_SAAMISK, ??? */
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_FAEROESE,             "fo" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_FARSI,                "fa" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_RUSSIAN,              "ru" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_CHINESE_SIMPLIFIED,   "zh-cn" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_FLEMISH,              "nl" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_IRISH,                "ga" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_ALBANIAN,             "sq" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_ROMANIAN,             "ro" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_CZECH,                "cs" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_SLOVAK,               "sk" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_SLOVENIAN,            "sl" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_YIDDISH,              "yi" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_SERBIAN,              "sr" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_MACEDONIAN,           "mk" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_BULGARIAN,            "bg" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_UKRAINIAN,            "uk" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_BYELORUSSIAN,         "be" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_UZBEK,                "uz" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_KAZAKH,               "kk" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_AZERBAIJANI,          "az" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_AZERBAIJANI_CYRILLIC_SCRIPT, "az" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_AZERBAIJANI_ARABIC_SCRIPT,    "ar" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_ARMENIAN,             "hy" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_GEORGIAN,             "ka" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_MOLDAVIAN,            "mo" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_KIRGHIZ,              "ky" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_TAJIKI,               "tg" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_TURKMEN,              "tk" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_MONGOLIAN,            "mo" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_MONGOLIAN_MONGOLIAN_SCRIPT,"mo" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_MONGOLIAN_CYRILLIC_SCRIPT, "mo" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_PASHTO,               "ps" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_KURDISH,              "ku" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_KASHMIRI,             "ks" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_SINDHI,               "sd" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_TIBETAN,              "bo" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_NEPALI,               "ne" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_SANSKRIT,             "sa" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_MARATHI,              "mr" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_BENGALI,              "bn" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_ASSAMESE,             "as" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_GUJARATI,             "gu" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_PUNJABI,              "pa" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_ORIYA,                "or" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_MALAYALAM,            "ml" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_KANNADA,              "kn" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_TAMIL,                "ta" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_TELUGU,               "te" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_SINHALESE,            "si" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_BURMESE,              "my" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_KHMER,                "km" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_LAO,                  "lo" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_VIETNAMESE,           "vi" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_INDONESIAN,           "id" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_TAGALOG,              "tl" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_MALAY_ROMAN_SCRIPT,   "ms" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_MALAY_ARABIC_SCRIPT,  "ms" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_AMHARIC,              "am" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_TIGRINYA,             "ti" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_GALLA,                "om" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_SOMALI,               "so" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_SWAHILI,              "sw" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_RUANDA,               "rw" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_RUNDI,                "rn" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_CHEWA,                "ny" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_MALAGASY,             "mg" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_ESPERANTO,            "eo" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_WELSH,                "cy" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_BASQUE,               "eu" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_CATALAN,              "ca" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_LATIN,                "la" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_QUECHUA,              "qu" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_GUARANI,              "gn" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_AYMARA,               "ay" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_TATAR,                "tt" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_UIGHUR,               "ug" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_DZONGKHA,             "dz" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_JAVANESE,             "jw" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_SUNDANESE,            "su" },

#if 0  /* these seem to be errors that have been dropped */

 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_SCOTTISH_GAELIC },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_IRISH_GAELIC },

#endif

  /* The following codes are new as of 2000-03-10 */
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_GALICIAN,             "gl" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_AFRIKAANS,            "af" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_BRETON,               "br" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_INUKTITUT,            "iu" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_SCOTTISH_GAELIC,      "gd" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_MANX_GAELIC,          "gv" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_IRISH_GAELIC,         "ga" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_TONGAN,               "to" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_GREEK_POLYTONIC,      "el" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_GREELANDIC,           "ik" },
 {  TT_PLATFORM_MACINTOSH,      TT_MAC_LANGID_AZERBAIJANI_ROMAN_SCRIPT,"az" },

 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ARABIC_SAUDI_ARABIA,       "ar" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ARABIC_IRAQ,               "ar" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ARABIC_EGYPT,              "ar" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ARABIC_LIBYA,              "ar" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ARABIC_ALGERIA,            "ar" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ARABIC_MOROCCO,            "ar" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ARABIC_TUNISIA,            "ar" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ARABIC_OMAN,               "ar" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ARABIC_YEMEN,              "ar" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ARABIC_SYRIA,              "ar" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ARABIC_JORDAN,             "ar" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ARABIC_LEBANON,            "ar" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ARABIC_KUWAIT,             "ar" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ARABIC_UAE,                "ar" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ARABIC_BAHRAIN,            "ar" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ARABIC_QATAR,              "ar" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_BULGARIAN_BULGARIA,        "bg" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_CATALAN_SPAIN,             "ca" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_CHINESE_TAIWAN,            "zh-tw" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_CHINESE_PRC,               "zh-cn" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_CHINESE_HONG_KONG,         "zh-hk" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_CHINESE_SINGAPORE,         "zh-sg" },

 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_CHINESE_MACAU,             "zh-mo" },

 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_CZECH_CZECH_REPUBLIC,      "cs" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_DANISH_DENMARK,            "da" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_GERMAN_GERMANY,            "de" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_GERMAN_SWITZERLAND,        "de" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_GERMAN_AUSTRIA,            "de" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_GERMAN_LUXEMBOURG,         "de" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_GERMAN_LIECHTENSTEI,       "de" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_GREEK_GREECE,              "el" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ENGLISH_UNITED_STATES,     "en" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ENGLISH_UNITED_KINGDOM,    "en" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ENGLISH_AUSTRALIA,         "en" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ENGLISH_CANADA,            "en" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ENGLISH_NEW_ZEALAND,       "en" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ENGLISH_IRELAND,           "en" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ENGLISH_SOUTH_AFRICA,      "en" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ENGLISH_JAMAICA,           "en" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ENGLISH_CARIBBEAN,         "en" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ENGLISH_BELIZE,            "en" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ENGLISH_TRINIDAD,          "en" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ENGLISH_ZIMBABWE,          "en" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ENGLISH_PHILIPPINES,       "en" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SPANISH_SPAIN_TRADITIONAL_SORT,"es" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SPANISH_MEXICO,            "es" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SPANISH_SPAIN_INTERNATIONAL_SORT,"es" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SPANISH_GUATEMALA,         "es" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SPANISH_COSTA_RICA,        "es" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SPANISH_PANAMA,            "es" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SPANISH_DOMINICAN_REPUBLIC,"es" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SPANISH_VENEZUELA,         "es" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SPANISH_COLOMBIA,          "es" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SPANISH_PERU,              "es" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SPANISH_ARGENTINA,         "es" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SPANISH_ECUADOR,           "es" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SPANISH_CHILE,             "es" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SPANISH_URUGUAY,           "es" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SPANISH_PARAGUAY,          "es" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SPANISH_BOLIVIA,           "es" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SPANISH_EL_SALVADOR,       "es" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SPANISH_HONDURAS,          "es" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SPANISH_NICARAGUA,         "es" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SPANISH_PUERTO_RICO,       "es" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_FINNISH_FINLAND,           "fi" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_FRENCH_FRANCE,             "fr" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_FRENCH_BELGIUM,            "fr" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_FRENCH_CANADA,             "fr" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_FRENCH_SWITZERLAND,        "fr" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_FRENCH_LUXEMBOURG,         "fr" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_FRENCH_MONACO,             "fr" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_HEBREW_ISRAEL,             "he" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_HUNGARIAN_HUNGARY,         "hu" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ICELANDIC_ICELAND,         "is" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ITALIAN_ITALY,             "it" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ITALIAN_SWITZERLAND,       "it" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_JAPANESE_JAPAN,            "ja" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_KOREAN_EXTENDED_WANSUNG_KOREA,"ko" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_KOREAN_JOHAB_KOREA,        "ko" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_DUTCH_NETHERLANDS,         "nl" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_DUTCH_BELGIUM,             "nl" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_NORWEGIAN_NORWAY_BOKMAL,   "no" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_NORWEGIAN_NORWAY_NYNORSK,  "nn" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_POLISH_POLAND,             "pl" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_PORTUGUESE_BRAZIL,         "pt" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_PORTUGUESE_PORTUGAL,       "pt" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_RHAETO_ROMANIC_SWITZERLAND,"rm" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ROMANIAN_ROMANIA,          "ro" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_MOLDAVIAN_MOLDAVIA,        "mo" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_RUSSIAN_RUSSIA,            "ru" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_RUSSIAN_MOLDAVIA,          "ru" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_CROATIAN_CROATIA,          "hr" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SERBIAN_SERBIA_LATIN,      "sr" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SERBIAN_SERBIA_CYRILLIC,   "sr" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SLOVAK_SLOVAKIA,           "sk" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ALBANIAN_ALBANIA,          "sq" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SWEDISH_SWEDEN,            "sv" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SWEDISH_FINLAND,           "sv" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_THAI_THAILAND,             "th" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_TURKISH_TURKEY,            "tr" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_URDU_PAKISTAN,             "ur" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_INDONESIAN_INDONESIA,      "id" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_UKRAINIAN_UKRAINE,         "uk" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_BELARUSIAN_BELARUS,        "be" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SLOVENE_SLOVENIA,          "sl" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ESTONIAN_ESTONIA,          "et" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_LATVIAN_LATVIA,            "lv" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_LITHUANIAN_LITHUANIA,      "lt" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_CLASSIC_LITHUANIAN_LITHUANIA,"lt" },

#ifdef TT_MS_LANGID_MAORI_NEW_ZELAND
    /* this seems to be an error that have been dropped */
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_MAORI_NEW_ZEALAND,         "mi" },
#endif

 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_FARSI_IRAN,                "fa" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_VIETNAMESE_VIET_NAM,       "vi" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ARMENIAN_ARMENIA,          "hy" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_AZERI_AZERBAIJAN_LATIN,    "az" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_AZERI_AZERBAIJAN_CYRILLIC, "az" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_BASQUE_SPAIN,              "eu" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SORBIAN_GERMANY,           "wen" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_MACEDONIAN_MACEDONIA,      "mk" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SUTU_SOUTH_AFRICA,         "st" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_TSONGA_SOUTH_AFRICA,       "ts" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_TSWANA_SOUTH_AFRICA,       "tn" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_VENDA_SOUTH_AFRICA,        "ven" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_XHOSA_SOUTH_AFRICA,        "xh" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ZULU_SOUTH_AFRICA,         "zu" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_AFRIKAANS_SOUTH_AFRICA,    "af" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_GEORGIAN_GEORGIA,          "ka" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_FAEROESE_FAEROE_ISLANDS,   "fo" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_HINDI_INDIA,               "hi" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_MALTESE_MALTA,             "mt" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SAAMI_LAPONIA,             "se" },

 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SCOTTISH_GAELIC_UNITED_KINGDOM,"gd" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_IRISH_GAELIC_IRELAND,      "ga" },

 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_MALAY_MALAYSIA,            "ms" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_MALAY_BRUNEI_DARUSSALAM,   "ms" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_KAZAK_KAZAKSTAN,           "kk" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SWAHILI_KENYA,             "sw" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_UZBEK_UZBEKISTAN_LATIN,    "uz" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_UZBEK_UZBEKISTAN_CYRILLIC, "uz" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_TATAR_TATARSTAN,           "tt" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_BENGALI_INDIA,             "bn" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_PUNJABI_INDIA,             "pa" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_GUJARATI_INDIA,            "gu" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ORIYA_INDIA,               "or" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_TAMIL_INDIA,               "ta" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_TELUGU_INDIA,              "te" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_KANNADA_INDIA,             "kn" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_MALAYALAM_INDIA,           "ml" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ASSAMESE_INDIA,            "as" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_MARATHI_INDIA,             "mr" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SANSKRIT_INDIA,            "sa" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_KONKANI_INDIA,             "kok" },

  /* new as of 2001-01-01 */
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ARABIC_GENERAL,            "ar" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_CHINESE_GENERAL,           "zh" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ENGLISH_GENERAL,           "en" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_FRENCH_WEST_INDIES,        "fr" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_FRENCH_REUNION,            "fr" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_FRENCH_CONGO,              "fr" },

 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_FRENCH_SENEGAL,            "fr" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_FRENCH_CAMEROON,           "fr" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_FRENCH_COTE_D_IVOIRE,      "fr" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_FRENCH_MALI,               "fr" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_BOSNIAN_BOSNIA_HERZEGOVINA,"bs" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_URDU_INDIA,                "ur" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_TAJIK_TAJIKISTAN,          "tg" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_YIDDISH_GERMANY,           "yi" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_KIRGHIZ_KIRGHIZSTAN,       "ky" },

 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_TURKMEN_TURKMENISTAN,      "tk" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_MONGOLIAN_MONGOLIA,        "mn" },

  /* the following seems to be inconsistent;
     here is the current "official" way: */
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_TIBETAN_BHUTAN,            "bo" },
  /* and here is what is used by Passport SDK */
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_TIBETAN_CHINA,             "bo" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_DZONGHKA_BHUTAN,           "dz" },
  /* end of inconsistency */

 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_WELSH_WALES,               "cy" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_KHMER_CAMBODIA,            "km" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_LAO_LAOS,                  "lo" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_BURMESE_MYANMAR,           "my" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_GALICIAN_SPAIN,            "gl" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_MANIPURI_INDIA,            "mni" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SINDHI_INDIA,              "sd" },
  /* the following one is only encountered in Microsoft RTF specification */
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_KASHMIRI_PAKISTAN,         "ks" },
  /* the following one is not in the Passport list, looks like an omission */
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_KASHMIRI_INDIA,            "ks" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_NEPALI_NEPAL,              "ne" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_NEPALI_INDIA,              "ne" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_FRISIAN_NETHERLANDS,       "fy" },

  /* new as of 2001-03-01 (from Office Xp) */
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ENGLISH_HONG_KONG,         "en" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ENGLISH_INDIA,             "en" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ENGLISH_MALAYSIA,          "en" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_ENGLISH_SINGAPORE,         "en" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SYRIAC_SYRIA,              "syr" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SINHALESE_SRI_LANKA,       "si" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_CHEROKEE_UNITED_STATES,    "chr" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_INUKTITUT_CANADA,          "iu" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_AMHARIC_ETHIOPIA,          "am" },
#if 0
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_TAMAZIGHT_MOROCCO },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_TAMAZIGHT_MOROCCO_LATIN },
#endif
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_PASHTO_AFGHANISTAN,        "ps" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_FILIPINO_PHILIPPINES,      "phi" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_DHIVEHI_MALDIVES,          "div" },

 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_OROMO_ETHIOPIA,            "om" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_TIGRIGNA_ETHIOPIA,         "ti" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_TIGRIGNA_ERYTHREA,         "ti" },

  /* New additions from Windows Xp/Passport SDK 2001-11-10. */

  /* don't ask what this one means... It is commented out currently. */
#if 0
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_GREEK_GREECE2 },
#endif

 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SPANISH_UNITED_STATES,     "es" },
  /* The following two IDs blatantly violate MS specs by using a */
  /* sublanguage >,.                                         */
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SPANISH_LATIN_AMERICA,     "es" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_FRENCH_NORTH_AFRICA,       "fr" },

 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_FRENCH_MOROCCO,            "fr" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_FRENCH_HAITI,              "fr" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_BENGALI_BANGLADESH,        "bn" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_PUNJABI_ARABIC_PAKISTAN,   "ar" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_MONGOLIAN_MONGOLIA_MONGOLIAN,"mn" },
#if 0
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_EDO_NIGERIA },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_FULFULDE_NIGERIA },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_IBIBIO_NIGERIA },
#endif
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_HAUSA_NIGERIA,             "ha" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_YORUBA_NIGERIA,            "yo" },
  /* language codes from, to, are (still) unknown. */
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_IGBO_NIGERIA,              "ibo" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_KANURI_NIGERIA,            "kau" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_GUARANI_PARAGUAY,          "gn" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_HAWAIIAN_UNITED_STATES,    "haw" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_LATIN,                     "la" },
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_SOMALI_SOMALIA,            "so" },
#if 0
  /* Note: Yi does not have a (proper) ISO 639-2 code, since it is mostly */
  /*       not written (but OTOH the peculiar writing system is worth     */
  /*       studying).                                                     */
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_YI_CHINA },
#endif
 {  TT_PLATFORM_MICROSOFT,      TT_MS_LANGID_PAPIAMENTU_NETHERLANDS_ANTILLES,"pap" },
};

static const char *
FcSfntNameLanguage (FT_SfntName *sname)
{
    int i;
    FT_UShort platform_id = sname->platform_id;
    FT_UShort language_id = sname->language_id;

    for (i = 0; i < G_N_ELEMENTS (ftLanguage); i++)
        if (ftLanguage[i].platform_id == platform_id &&
            (ftLanguage[i].language_id == TT_LANGUAGE_DONT_CARE ||
             ftLanguage[i].language_id == language_id))
        {
            if (ftLanguage[i].lang[0] == '\0')
              return NULL;
            else
              return ftLanguage[i].lang;
        }
    return NULL;
}

static char *
FcSfntNameTranscode (FT_SfntName *name)
{
    int        i;
    const char *fromcode;

    for (i = 0; i < G_N_ELEMENTS (ftEncoding); i++)
        if (ftEncoding[i].platform_id == name->platform_id &&
            (ftEncoding[i].encoding_id == TT_ENCODING_DONT_CARE ||
             ftEncoding[i].encoding_id == name->encoding_id))
            break;
    if (i == G_N_ELEMENTS (ftEncoding))
        return  NULL;
    fromcode = ftEncoding[i].fromcode;

    return g_convert ((const char *)name->string, name->string_len, "UTF-8", fromcode, NULL, NULL, NULL);
}

static char *
get_sfnt_name (FT_Face ft_face,
               guint   nameid)
{
  guint count;
  guint i, j;
  const char * const *langs = g_get_language_names ();
  char *res = NULL;
  guint pos = G_MAXUINT;

  count = FT_Get_Sfnt_Name_Count (ft_face);
  for (i = 0; i < count; i++)
    {
      FT_SfntName name;
      const char *lang;

      if (FT_Get_Sfnt_Name (ft_face, i, &name) != 0)
        continue;

      if (name.name_id != nameid)
        continue;

      lang = FcSfntNameLanguage (&name);
      for (j = 0; j < pos && langs[j]; j++)
        {
          if (strcmp (lang, langs[j]) == 0)
            {
              pos = j;
              g_free (res);
              res = FcSfntNameTranscode (&name);
            }
        }

      if (pos == 0)
        break;
    }

  return res;
}

static gboolean
is_valid_subfamily_id (guint id)
{
  return id == 2 || id == 17 || (255 < id && id < 32768);
}

static void
add_instance (FT_Face             ft_face,
              FT_MM_Var          *ft_mm_var,
              FT_Var_Named_Style *ns,
              GtkWidget          *combo,
              int                 pos)
{
  Instance *instance;
  int i;

  instance = g_new0 (Instance, 1);

  if (is_valid_subfamily_id (ns->strid))
    instance->name = get_sfnt_name (ft_face, ns->strid);
  if (!instance->name)
    instance->name = g_strdup_printf ("Instance %d", pos);

  g_hash_table_add (instances, instance);
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), instance->name);

  instance->n_axes = ft_mm_var->num_axis;
  instance->axes = g_new (guint32, ft_mm_var->num_axis);
  instance->coords = g_new (float, ft_mm_var->num_axis);

  for (i = 0; i < ft_mm_var->num_axis; i++)
    {
      instance->axes[i] = ft_mm_var->axis[i].tag;
      instance->coords[i] = FixedToFloat(ns->coords[i]);
    }
}

static void
unset_instance (GtkAdjustment *adjustment)
{
  if (instance_combo)
    gtk_combo_box_set_active (GTK_COMBO_BOX (instance_combo), 0);
}

static void
instance_changed (GtkComboBox *combo)
{
  char *text;
  Instance *instance;
  Instance ikey;
  int i;

  text = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (combo));
  if (text[0] == '\0')
    goto out;

  ikey.name = text;
  instance = g_hash_table_lookup (instances, &ikey);
  if (!instance)
    {
      g_print ("did not find instance %s\n", text);
      goto out;
    }

  for (i = 0; i < instance->n_axes; i++)
    {
      Axis *axis;
      Axis akey;
      guint32 tag;
      gdouble value;

      tag = instance->axes[i];
      value = instance->coords[i];

      akey.tag = tag;
      axis = g_hash_table_lookup (axes, &akey);
      if (axis)
        {
          g_signal_handlers_block_by_func (axis->adjustment, unset_instance, NULL);
          gtk_adjustment_set_value (axis->adjustment, value);
          g_signal_handlers_unblock_by_func (axis->adjustment, unset_instance, NULL);
        }
    }

out:
  g_free (text);
}

static gboolean
matches_instance (FT_Var_Named_Style *instance,
                  FT_Fixed           *coords,
                  FT_UInt             num_coords)
{
  FT_UInt i;

  for (i = 0; i < num_coords; i++)
    if (coords[i] != instance->coords[i])
      return FALSE;

  return TRUE;
}

static void
add_font_plane (int i)
{
  GtkWidget *plane;
  Axis *weight_axis;
  Axis *width_axis;

  Axis key;

  key.tag = MAKE_TAG('w','g','h','t');
  weight_axis = g_hash_table_lookup (axes, &key);
  key.tag = MAKE_TAG('w','d','t','h');
  width_axis = g_hash_table_lookup (axes, &key);

  if (weight_axis && width_axis)
    {
      plane = gtk_font_plane_new (weight_axis->adjustment,
                                  width_axis->adjustment);

      gtk_widget_set_size_request (plane, 300, 300);
      gtk_widget_set_halign (plane, GTK_ALIGN_CENTER);
      gtk_grid_attach (GTK_GRID (variations_grid), plane, 0, i, 3, 1);
    }
}

static void
update_font_variations (void)
{
  GtkWidget *child, *next;
  PangoFont *pango_font;
  FT_Face ft_face;
  FT_MM_Var *ft_mm_var;
  FT_Error ret;

  child = gtk_widget_get_first_child (variations_grid);
  while (child != NULL)
    {
      next = gtk_widget_get_next_sibling (child);
      gtk_widget_destroy (child);
      child = next;
    }

  instance_combo = NULL;

  g_hash_table_remove_all (axes);
  g_hash_table_remove_all (instances);

  pango_font = get_pango_font ();
  ft_face = pango_fc_font_lock_face (PANGO_FC_FONT (pango_font)),

  ret = FT_Get_MM_Var (ft_face, &ft_mm_var);
  if (ret == 0)
    {
      unsigned int i;
      FT_Fixed *coords;

      coords = g_new (FT_Fixed, ft_mm_var->num_axis);
      ret = FT_Get_Var_Design_Coordinates (ft_face, ft_mm_var->num_axis, coords);

      if (ft_mm_var->num_namedstyles > 0)
        {
           GtkWidget *label;
           GtkWidget *combo;

           label = gtk_label_new ("Instance");
           gtk_label_set_xalign (GTK_LABEL (label), 0);
           gtk_widget_set_halign (label, GTK_ALIGN_START);
           gtk_widget_set_valign (label, GTK_ALIGN_BASELINE);
           gtk_grid_attach (GTK_GRID (variations_grid), label, 0, -1, 2, 1);

           combo = gtk_combo_box_text_new ();
           gtk_widget_set_valign (combo, GTK_ALIGN_BASELINE);
           g_signal_connect (combo, "changed", G_CALLBACK (instance_changed), NULL);
           gtk_grid_attach (GTK_GRID (variations_grid), combo, 1, -1, 2, 1);

           gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "");

           for (i = 0; i < ft_mm_var->num_namedstyles; i++)
             add_instance (ft_face, ft_mm_var, &ft_mm_var->namedstyle[i], combo, i);
           for (i = 0; i < ft_mm_var->num_namedstyles; i++)
             {
               if (matches_instance (&ft_mm_var->namedstyle[i], coords, ft_mm_var->num_axis))
                 {
                   gtk_combo_box_set_active (GTK_COMBO_BOX (combo), i + 1);
                   break;
                 }
             }

           instance_combo = combo;
        }

      if (ret == 0)
        {
          for (i = 0; i < ft_mm_var->num_axis; i++)
            add_axis (&ft_mm_var->axis[i], coords[i], i);

          add_font_plane (ft_mm_var->num_axis);
        }
      g_free (coords);
      free (ft_mm_var);
    }

  pango_fc_font_unlock_face (PANGO_FC_FONT (pango_font));
  g_object_unref (pango_font);
}

static void
font_changed (void)
{
  update_script_combo ();
  update_features ();
  update_font_variations ();
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
  GList *l;

  gtk_label_select_region (GTK_LABEL (label), 0, 0);

  g_list_free_full (ranges, free_range);
  ranges = NULL;

  for (l = feature_items; l; l = l->next)
    {
      FeatureItem *item = l->data;

      if (GTK_IS_RADIO_BUTTON (item->feat))
        {
          if (strcmp (item->name, "xxxx") == 0)
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item->feat), TRUE);
        }
      else if (GTK_IS_CHECK_BUTTON (item->feat))
        {
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item->feat), FALSE);
          set_inconsistent (GTK_CHECK_BUTTON (item->feat), TRUE);
        }
    }
}

static char *text;

static void
switch_to_entry (void)
{
  text = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
  gtk_stack_set_visible_child_name (GTK_STACK (stack), "entry");
  gtk_widget_grab_focus (entry);
}

static void
switch_to_label (void)
{
  g_free (text);
  text = NULL;
  gtk_stack_set_visible_child_name (GTK_STACK (stack), "label");
  update_display ();
}

static void
toggle_edit (void)
{
  if (strcmp (gtk_stack_get_visible_child_name (GTK_STACK (stack)), "label") == 0)
    switch_to_entry ();
  else
    switch_to_label ();
}

static void
stop_edit (void)
{
  gtk_button_clicked (GTK_BUTTON (edit_toggle));
}

static gboolean
entry_key_press (GtkEntry *entry, GdkEventKey *event)
{
  guint keyval;

  gdk_event_get_keyval ((GdkEvent *)event, &keyval);

  if (keyval == GDK_KEY_Escape)
    {
      gtk_entry_set_text (GTK_ENTRY (entry), text);
      stop_edit ();
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
      GtkWidget *feature_list;

      builder = gtk_builder_new_from_resource ("/font_features/font-features.ui");

      gtk_builder_add_callback_symbol (builder, "update_display", update_display);
      gtk_builder_add_callback_symbol (builder, "font_changed", font_changed);
      gtk_builder_add_callback_symbol (builder, "script_changed", script_changed);
      gtk_builder_add_callback_symbol (builder, "reset", reset_features);
      gtk_builder_add_callback_symbol (builder, "stop_edit", G_CALLBACK (stop_edit));
      gtk_builder_add_callback_symbol (builder, "toggle_edit", G_CALLBACK (toggle_edit));
      gtk_builder_add_callback_symbol (builder, "entry_key_press", G_CALLBACK (entry_key_press));
      gtk_builder_connect_signals (builder, NULL);

      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      feature_list = GTK_WIDGET (gtk_builder_get_object (builder, "feature_list"));
      label = GTK_WIDGET (gtk_builder_get_object (builder, "label"));
      settings = GTK_WIDGET (gtk_builder_get_object (builder, "settings"));
      description = GTK_WIDGET (gtk_builder_get_object (builder, "description"));
      resetbutton = GTK_WIDGET (gtk_builder_get_object (builder, "reset"));
      font = GTK_WIDGET (gtk_builder_get_object (builder, "font"));
      script_lang = GTK_WIDGET (gtk_builder_get_object (builder, "script_lang"));
      stack = GTK_WIDGET (gtk_builder_get_object (builder, "stack"));
      entry = GTK_WIDGET (gtk_builder_get_object (builder, "entry"));
      edit_toggle = GTK_WIDGET (gtk_builder_get_object (builder, "edit_toggle"));

      add_check_group (feature_list, _("Kerning"), (const char *[]){ "kern", NULL });
      add_check_group (feature_list, _("Ligatures"), (const char *[]){ "liga",
                                                                       "dlig",
                                                                       "hlig",
                                                                       "clig",
                                                                       "rlig", NULL });
      add_check_group (feature_list, _("Letter Case"), (const char *[]){ "smcp",
                                                                         "c2sc",
                                                                         "pcap",
                                                                         "c2pc",
                                                                         "unic",
                                                                         "cpsp",
                                                                         "case",NULL });
      add_radio_group (feature_list, _("Number Case"), (const char *[]){ "xxxx",
                                                                         "lnum",
                                                                         "onum", NULL });
      add_radio_group (feature_list, _("Number Spacing"), (const char *[]){ "xxxx",
                                                                            "pnum",
                                                                            "tnum", NULL });
      add_radio_group (feature_list, _("Fractions"), (const char *[]){ "xxxx",
                                                                       "frac",
                                                                       "afrc", NULL });
      add_check_group (feature_list, _("Numeric Extras"), (const char *[]){ "zero",
                                                                            "nalt",
                                                                            "sinf", NULL });
      add_check_group (feature_list, _("Character Alternatives"), (const char *[]){ "swsh",
                                                                                    "cswh",
                                                                                    "locl",
                                                                                    "calt",
                                                                                    "falt",
                                                                                    "hist",
                                                                                    "salt",
                                                                                    "jalt",
                                                                                    "titl",
                                                                                    "rand",
                                                                                    "subs",
                                                                                    "sups",
                                                                                    "ordn",
                                                                                    "ltra",
                                                                                    "ltrm",
                                                                                    "rtla",
                                                                                    "rtlm",
                                                                                    "rclt", NULL });
      add_check_group (feature_list, _("Positional Alternatives"), (const char *[]){ "init",
                                                                                     "medi",
                                                                                     "med2",
                                                                                     "fina",
                                                                                     "fin2",
                                                                                     "fin3",
                                                                                     "isol", NULL });
      add_check_group (feature_list, _("Width Variants"), (const char *[]){ "fwid",
                                                                            "hwid",
                                                                            "halt",
                                                                            "pwid",
                                                                            "palt",
                                                                            "twid",
                                                                            "qwid", NULL });
      add_check_group (feature_list, _("Alternative Stylistic Sets"), (const char *[]){ "ss00",
                                                                                        "ss01",
                                                                                        "ss02",
                                                                                        "ss03",
                                                                                        "ss04",
                                                                                        "ss05",
                                                                                        "ss06",
                                                                                        "ss07",
                                                                                        "ss08",
                                                                                        "ss09",
                                                                                        "ss10",
                                                                                        "ss11",
                                                                                        "ss12",
                                                                                        "ss13",
                                                                                        "ss14",
                                                                                        "ss15",
                                                                                        "ss16",
                                                                                        "ss17",
                                                                                        "ss18",
                                                                                        "ss19",
                                                                                        "ss20", NULL });
      add_check_group (feature_list, _("Mathematical"), (const char *[]){ "dtls",
                                                                          "flac",
                                                                          "mgrk",
                                                                          "ssty", NULL });
      add_check_group (feature_list, _("Optical Bounds"), (const char *[]){ "opbd",
                                                                            "lfbd",
                                                                            "rtbd", NULL });
      feature_items = g_list_reverse (feature_items);

      variations_heading = GTK_WIDGET (gtk_builder_get_object (builder, "variations_heading"));
      variations_grid = GTK_WIDGET (gtk_builder_get_object (builder, "variations_grid"));
      if (instances == NULL)
        instances = g_hash_table_new_full (instance_hash, instance_equal, NULL, free_instance);
      else
        g_hash_table_remove_all (instances);

      if (axes == NULL)
        axes = g_hash_table_new_full (axes_hash, axes_equal, NULL, g_free);
      else
        g_hash_table_remove_all (axes);

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
