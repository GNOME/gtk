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

  text = gtk_editable_get_text (GTK_EDITABLE (entry));

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
  PangoFont *pango_font;
  GHashTable *tags;
  GHashTableIter iter;
  TagPair *pair;
  char *lang;
  hb_tag_t active;
  GtkTreeIter active_iter;
  gboolean have_active = FALSE;

  lang = gtk_font_chooser_get_language (GTK_FONT_CHOOSER (font));

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  active = hb_ot_tag_from_language (hb_language_from_string (lang, -1));
  G_GNUC_END_IGNORE_DEPRECATIONS

  g_free (lang);

  store = gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT);

  pango_font = get_pango_font ();
  hb_font = pango_font_get_hb_font (pango_font);

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
    }

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
  hb_font = pango_font_get_hb_font (pango_font);

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
    }

  g_object_unref (pango_font);
}

#define FixedToFloat(f) (((float)(f))/65536.0)

static void
adjustment_changed (GtkAdjustment *adjustment,
                    GtkEntry      *entry)
{
  char *str;

  str = g_strdup_printf ("%g", gtk_adjustment_get_value (adjustment));
  gtk_editable_set_text (GTK_EDITABLE (entry), str);
  g_free (str);

  update_display ();
}

static void
entry_activated (GtkEntry *entry,
                 GtkAdjustment *adjustment)
{
  gdouble value;
  gchar *err = NULL;

  value = g_strtod (gtk_editable_get_text (GTK_EDITABLE (entry)), &err);
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
add_axis (hb_face_t             *hb_face,
          hb_ot_var_axis_info_t *ax,
          float                  value,
          int                    i)
{
  GtkWidget *axis_label;
  GtkWidget *axis_entry;
  GtkWidget *axis_scale;
  GtkAdjustment *adjustment;
  Axis *axis;
  char name[20];
  unsigned int name_len = 20;

  hb_ot_name_get_utf8 (hb_face, ax->name_id, HB_LANGUAGE_INVALID, &name_len, name);

  axis_label = gtk_label_new (name);
  gtk_widget_set_halign (axis_label, GTK_ALIGN_START);
  gtk_widget_set_valign (axis_label, GTK_ALIGN_BASELINE);
  gtk_grid_attach (GTK_GRID (variations_grid), axis_label, 0, i, 1, 1);
  adjustment = gtk_adjustment_new (value, ax->min_value, ax->max_value,
                                   1.0, 10.0, 0.0);
  axis_scale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, adjustment);
  gtk_scale_add_mark (GTK_SCALE (axis_scale), ax->default_value, GTK_POS_TOP, NULL);
  gtk_widget_set_valign (axis_scale, GTK_ALIGN_BASELINE);
  gtk_widget_set_hexpand (axis_scale, TRUE);
  gtk_widget_set_size_request (axis_scale, 100, -1);
  gtk_scale_set_draw_value (GTK_SCALE (axis_scale), FALSE);
  gtk_grid_attach (GTK_GRID (variations_grid), axis_scale, 1, i, 1, 1);
  axis_entry = gtk_entry_new ();
  gtk_widget_set_valign (axis_entry, GTK_ALIGN_BASELINE);
  gtk_editable_set_width_chars (GTK_EDITABLE (axis_entry), 4);
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
  unsigned int index;
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
  g_free (instance);
}

static GHashTable *instances;

static void
add_instance (hb_face_t    *face,
              unsigned int  index,
              GtkWidget    *combo,
              int           pos)
{
  Instance *instance;
  hb_ot_name_id_t name_id;
  char name[20];
  unsigned int name_len = 20;

  instance = g_new0 (Instance, 1);

  name_id = hb_ot_var_named_instance_get_subfamily_name_id (face, index);
  hb_ot_name_get_utf8 (face, name_id, HB_LANGUAGE_INVALID, &name_len, name);

  instance->name = g_strdup_printf (name);
  instance->index = index;

  g_hash_table_add (instances, instance);
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), instance->name);
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
  unsigned int coords_length;
  float *coords = NULL;
  hb_ot_var_axis_info_t *ai = NULL;
  unsigned int n_axes;
  PangoFont *pango_font = NULL;
  hb_font_t *hb_font;
  hb_face_t *hb_face;

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

  pango_font = get_pango_font ();
  hb_font = pango_font_get_hb_font (pango_font);
  hb_face = hb_font_get_face (hb_font);

  n_axes = hb_ot_var_get_axis_infos (hb_face, 0, NULL, NULL);
  ai = g_new (hb_ot_var_axis_info_t, n_axes);
  hb_ot_var_get_axis_infos (hb_face, 0, &n_axes, ai);

  coords = g_new (float, n_axes);
  hb_ot_var_named_instance_get_design_coords (hb_face,
                                              instance->index,
                                              &coords_length,
                                              coords);

  for (i = 0; i < n_axes; i++)
    {
      Axis *axis;
      Axis akey;
      gdouble value;

      value = coords[ai[i].axis_index];

      akey.tag = ai[i].tag;
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
  g_clear_object (&pango_font);
  g_free (ai);
  g_free (coords);
}

static gboolean
matches_instance (hb_face_t             *hb_face,
                  unsigned int           index,
                  unsigned int           n_axes,
                  float                 *coords)
{
  float *instance_coords;
  unsigned int coords_length;
  int i;

  instance_coords = g_new (float, n_axes);
  coords_length = n_axes;

  hb_ot_var_named_instance_get_design_coords (hb_face,
                                              index,
                                              &coords_length,
                                              instance_coords);

  for (i = 0; i < n_axes; i++)
    if (instance_coords[i] != coords[i])
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

/* FIXME: This doesn't work if the font has an avar table */
static float
denorm_coord (hb_ot_var_axis_info_t *axis, int coord)
{
  float r = coord / 16384.0;

  if (coord < 0)
    return axis->default_value + r * (axis->default_value - axis->min_value);
  else
    return axis->default_value + r * (axis->max_value - axis->default_value);
}

static void
update_font_variations (void)
{
  GtkWidget *child, *next;
  PangoFont *pango_font = NULL;
  hb_font_t *hb_font;
  hb_face_t *hb_face;
  unsigned int n_axes;
  hb_ot_var_axis_info_t *ai = NULL;
  float *design_coords = NULL;
  const int *coords;
  unsigned int length;
  int i;

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
  hb_font = pango_font_get_hb_font (pango_font);
  hb_face = hb_font_get_face (hb_font);

  n_axes = hb_ot_var_get_axis_infos (hb_face, 0, NULL, NULL);
  if (n_axes == 0)
    goto done;

  ai = g_new0 (hb_ot_var_axis_info_t, n_axes);
  design_coords = g_new (float, n_axes);

  hb_ot_var_get_axis_infos (hb_face, 0, &n_axes, ai);
  coords = hb_font_get_var_coords_normalized (hb_font, &length);
  for (i = 0; i < length; i++)
    design_coords[i] = denorm_coord (&ai[i], coords[i]);

  if (hb_ot_var_get_named_instance_count (hb_face) > 0)
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

      gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "");

      for (i = 0; i < hb_ot_var_get_named_instance_count (hb_face); i++)
        add_instance (hb_face, i, combo, i);

      for (i = 0; i < hb_ot_var_get_named_instance_count (hb_face); i++)
        {
          if (matches_instance (hb_face, i, n_axes, design_coords))
            {
              gtk_combo_box_set_active (GTK_COMBO_BOX (combo), i + 1);
              break;
            }
        }

      gtk_grid_attach (GTK_GRID (variations_grid), combo, 1, -1, 2, 1);
      g_signal_connect (combo, "changed", G_CALLBACK (instance_changed), NULL);
      instance_combo = combo;
   }

  for (i = 0; i < n_axes; i++)
    add_axis (hb_face, &ai[i], design_coords[i], i);

  add_font_plane (n_axes);

done:
  g_clear_object (&pango_font);
  g_free (ai);
  g_free (design_coords);
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
  text = g_strdup (gtk_editable_get_text (GTK_EDITABLE (entry)));
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
  g_signal_emit_by_name (edit_toggle, "clicked");
}

static gboolean
entry_key_press (GtkEventController *controller,
                 guint               keyval,
                 guint               keycode,
                 GdkModifierType     modifiers,
                 GtkEntry           *entry)
{
  if (keyval == GDK_KEY_Escape)
    {
      gtk_editable_set_text (GTK_EDITABLE (entry), text);
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
      GtkEventController *controller;

      builder = gtk_builder_new_from_resource ("/font_features/font-features.ui");

      gtk_builder_add_callback_symbol (builder, "update_display", update_display);
      gtk_builder_add_callback_symbol (builder, "font_changed", font_changed);
      gtk_builder_add_callback_symbol (builder, "script_changed", script_changed);
      gtk_builder_add_callback_symbol (builder, "reset", reset_features);
      gtk_builder_add_callback_symbol (builder, "stop_edit", G_CALLBACK (stop_edit));
      gtk_builder_add_callback_symbol (builder, "toggle_edit", G_CALLBACK (toggle_edit));
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

      controller = gtk_event_controller_key_new ();
      g_object_set_data_full (G_OBJECT (entry), "controller", controller, g_object_unref);
      g_signal_connect (controller, "key-pressed", G_CALLBACK (entry_key_press), entry);
      gtk_widget_add_controller (entry, controller);

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
