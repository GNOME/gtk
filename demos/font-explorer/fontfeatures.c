#include "fontfeatures.h"
#include <gtk/gtk.h>
#include <hb-ot.h>
#include <glib/gi18n.h>

#include "open-type-layout.h"
#include "language-names.h"


enum {
  PROP_FONT_DESC = 1,
  PROP_LANGUAGE,
  PROP_FEATURES,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

typedef struct {
  unsigned int tag;
  const char *name;
  GtkWidget *feat;
} FeatureItem;

struct _FontFeatures
{
  GtkWidget parent;

  GtkGrid *label;
  GtkGrid *grid;
  Pango2FontDescription *font_desc;
  GSimpleAction *reset_action;
  Pango2Language *lang;
  GList *feature_items;

  Pango2FontMap *map;
};

struct _FontFeaturesClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE(FontFeatures, font_features, GTK_TYPE_WIDGET);

static Pango2Font *
get_font (FontFeatures *self)
{
  Pango2Context *context;
  Pango2Font *font;

  context = pango2_context_new ();
  if (self->map)
    pango2_context_set_font_map (context, self->map);

  font = pango2_context_load_font (context, self->font_desc);
  g_object_unref (context);

  return font;
}

static gboolean
is_ssNN (const char *buf)
{
  return g_str_has_prefix (buf, "ss") && g_ascii_isdigit (buf[2]) && g_ascii_isdigit (buf[3]);
}

static gboolean
is_cvNN (const char *buf)
{
  return g_str_has_prefix (buf, "cv") && g_ascii_isdigit (buf[2]) && g_ascii_isdigit (buf[3]);
}

static char *
get_feature_display_name (unsigned int tag)
{
  int i;
  static char buf[5] = { 0, };

  if (tag == HB_TAG ('x', 'x', 'x', 'x'))
    return g_strdup (_("Default"));

  hb_tag_to_string (tag, buf);
  if (is_ssNN (buf))
    {
      int num = (buf[2] - '0') * 10 + (buf[3] - '0');
      return g_strdup_printf (g_dpgettext2 (NULL, "OpenType layout", "Stylistic Set %d"), num);
    }
  else if (is_cvNN (buf))
    {
      int num = (buf[2] - '0') * 10 + (buf[3] - '0');
      return g_strdup_printf (g_dpgettext2 (NULL, "OpenType layout", "Character Variant %d"), num);
    }

  for (i = 0; i < G_N_ELEMENTS (open_type_layout_features); i++)
    {
      if (tag == open_type_layout_features[i].tag)
        return g_strdup (g_dpgettext2 (NULL, "OpenType layout", open_type_layout_features[i].name));
    }

  g_warning ("unknown OpenType layout feature tag: %s", buf);

  return g_strdup (buf);
}

static void
update_feature_label (FontFeatures *self,
                      FeatureItem  *item,
                      hb_font_t    *hb_font,
                      hb_tag_t      script_tag,
                      hb_tag_t      lang_tag)
{
  hb_face_t *hb_face;
  unsigned int script_index, lang_index, feature_index;
  hb_ot_name_id_t id;
  unsigned int len;
  char *label;
  char name[5] = { 0, };

  hb_face = hb_font_get_face (hb_font);

  hb_tag_to_string (item->tag, name);
  if (!is_ssNN (name) && !is_cvNN (name))
    return;

  hb_ot_layout_table_find_script (hb_face, HB_OT_TAG_GSUB, script_tag, &script_index);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  hb_ot_layout_script_find_language (hb_face, HB_OT_TAG_GSUB, script_index, lang_tag, &lang_index);
  G_GNUC_END_IGNORE_DEPRECATIONS

  if (hb_ot_layout_language_find_feature (hb_face, HB_OT_TAG_GSUB, script_index, lang_index, item->tag, &feature_index) &&
      hb_ot_layout_feature_get_name_ids (hb_face, HB_OT_TAG_GSUB, feature_index, &id, NULL, NULL, NULL, NULL))
    {
      len = hb_ot_name_get_utf8 (hb_face, id, HB_LANGUAGE_INVALID, NULL, NULL);
      len++;
      label = g_new (char, len);
      hb_ot_name_get_utf8 (hb_face, id, HB_LANGUAGE_INVALID, &len, label);

      gtk_check_button_set_label (GTK_CHECK_BUTTON (item->feat), label);

      g_free (label);
    }
  else
    {
      label = get_feature_display_name (item->tag);
      gtk_check_button_set_label (GTK_CHECK_BUTTON (item->feat), label);
      g_free (label);
    }
}

static void
set_inconsistent (GtkCheckButton *button,
                  gboolean        inconsistent)
{
  gtk_check_button_set_inconsistent (GTK_CHECK_BUTTON (button), inconsistent);
  gtk_widget_set_opacity (gtk_widget_get_first_child (GTK_WIDGET (button)), inconsistent ? 0.0 : 1.0);
}

static void
find_language_and_script (FontFeatures *self,
                          hb_face_t    *hb_face,
                          hb_tag_t     *lang_tag,
                          hb_tag_t     *script_tag)
{
  int i, j, k;
  hb_tag_t scripts[80];
  unsigned int n_scripts;
  unsigned int count;
  hb_tag_t table[2] = { HB_OT_TAG_GSUB, HB_OT_TAG_GPOS };
  hb_language_t lang;
  const char *langname, *p;

  langname = pango2_language_to_string (self->lang);

  p = strchr (langname, '-');
  lang = hb_language_from_string (langname, p ? p - langname : -1);

  n_scripts = 0;
  for (i = 0; i < 2; i++)
    {
      count = G_N_ELEMENTS (scripts);
      hb_ot_layout_table_get_script_tags (hb_face, table[i], n_scripts, &count, scripts);
      n_scripts += count;
    }

  for (j = 0; j < n_scripts; j++)
    {
      hb_tag_t languages[80];
      unsigned int n_languages;

      n_languages = 0;
      for (i = 0; i < 2; i++)
        {
          count = G_N_ELEMENTS (languages);
          hb_ot_layout_script_get_language_tags (hb_face, table[i], j, n_languages, &count, languages);
          n_languages += count;
        }

      for (k = 0; k < n_languages; k++)
        {
          if (lang == hb_ot_tag_to_language (languages[k]))
            {
              *script_tag = scripts[j];
              *lang_tag = languages[k];
              return;
            }
        }
    }

  *lang_tag = HB_OT_TAG_DEFAULT_LANGUAGE;
  *script_tag = HB_OT_TAG_DEFAULT_SCRIPT;
}

static void
hide_feature_maybe (FeatureItem *item,
                    gboolean     has_feature)
{
  gtk_widget_set_visible (item->feat, has_feature);
  if (has_feature)
    gtk_widget_set_visible (gtk_widget_get_parent (item->feat), TRUE);
}

/* Make features insensitive if the font/langsys does not have them,
 * and reset all others to their initial value
 */
static void
update_features (FontFeatures *self)
{
  guint script_index, lang_index;
  hb_tag_t lang_tag;
  hb_tag_t script_tag;
  Pango2Font *font;
  hb_font_t *hb_font;
  hb_face_t *hb_face;

  font = get_font (self);
  hb_font = pango2_font_get_hb_font (font);
  hb_face = hb_font_get_face (hb_font);

    {
      hb_tag_t table[2] = { HB_OT_TAG_GSUB, HB_OT_TAG_GPOS };
      hb_tag_t features[256];
      unsigned int count;
      unsigned int n_features = 0;

      find_language_and_script (self, hb_face, &lang_tag, &script_tag);

      /* Collect all features */
      for (int i = 0; i < 2; i++)
        {
          hb_ot_layout_table_find_script (hb_face,
                                          table[i],
                                          script_tag,
                                          &script_index);
          hb_ot_layout_script_select_language (hb_face,
                                               table[i],
                                               script_index,
                                               1,
                                               &lang_tag,
                                               &lang_index);

          count = G_N_ELEMENTS (features);
          hb_ot_layout_language_get_feature_tags (hb_face,
                                                  table[i],
                                                  script_index,
                                                  lang_index,
                                                  n_features,
                                                  &count,
                                                  features);
          n_features += count;
        }

      /* Update all the features */
      for (GList *l = self->feature_items; l; l = l->next)
        {
          FeatureItem *item = l->data;
          gboolean has_feature = FALSE;

          for (int j = 0; j < n_features; j++)
            {
              if (item->tag == features[j])
                {
                  has_feature = TRUE;
                  break;
                }
            }

          update_feature_label (self, item, hb_font, script_tag, lang_tag);

          hide_feature_maybe (item, has_feature);

          if (GTK_IS_CHECK_BUTTON (item->feat))
            {
              GtkWidget *def = GTK_WIDGET (g_object_get_data (G_OBJECT (item->feat), "default"));
              if (def)
                {
                  gtk_widget_show (def);
                  gtk_widget_show (gtk_widget_get_parent (def));
                  gtk_check_button_set_active (GTK_CHECK_BUTTON (def), TRUE);
                }
              else
                set_inconsistent (GTK_CHECK_BUTTON (item->feat), TRUE);
            }
        }
    }

  /* Hide empty groups */
  for (GList *l = self->feature_items; l; l = l->next)
    {
      FeatureItem *item = l->data;
      GtkWidget *box;

      box = gtk_widget_get_parent (item->feat);
      if (gtk_widget_get_visible (box))
        {
          GtkWidget *c;
          int count;

          count = 0;
          for (c = gtk_widget_get_first_child (box); c; c = gtk_widget_get_next_sibling (c))
            {
              if (gtk_widget_get_visible (c))
                count++;
            }

          if (count == 1)
            gtk_widget_hide (box);
          else if (count == 2 &&
                   item->tag == HB_TAG ('x', 'x', 'x', 'x'))
            gtk_widget_hide (box);
        }
    }
}

static void
script_changed (GtkComboBox  *combo,
                FontFeatures *self)
{
  update_features (self);
}

static char *
get_features (FontFeatures *self)
{
  GString *s;
  char buf[128];

  s = g_string_new ("");

  for (GList *l = self->feature_items; l; l = l->next)
    {
      FeatureItem *item = l->data;

      if (!gtk_widget_is_sensitive (item->feat))
        continue;

      if (GTK_IS_CHECK_BUTTON (item->feat) && g_object_get_data (G_OBJECT (item->feat), "default"))
        {
          if (gtk_check_button_get_active (GTK_CHECK_BUTTON (item->feat)) &&
              item->tag != HB_TAG ('x', 'x', 'x', 'x'))
            {
              hb_feature_to_string (&(hb_feature_t) { item->tag, 1, 0, -1 }, buf, sizeof (buf));
              if (s->len > 0)
                g_string_append_c (s, ',');
              g_string_append (s, buf);
            }
        }
      else if (GTK_IS_CHECK_BUTTON (item->feat))
        {
          guint32 value;

          if (gtk_check_button_get_inconsistent (GTK_CHECK_BUTTON (item->feat)))
            continue;

          value = gtk_check_button_get_active (GTK_CHECK_BUTTON (item->feat));
          hb_feature_to_string (&(hb_feature_t) { item->tag, value, 0, -1 }, buf, sizeof (buf));
          if (s->len > 0)
            g_string_append_c (s, ',');
          g_string_append (s, buf);
        }
    }

  return g_string_free (s, FALSE);
}

static void
update_display (FontFeatures *self)
{
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FEATURES]);
  g_simple_action_set_enabled (self->reset_action, TRUE);
}

static GtkWidget *
make_title_label (const char *title)
{
  GtkWidget *label;

  label = gtk_label_new (title);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  g_object_set (label, "margin-top", 10, "margin-bottom", 10, NULL);
  gtk_widget_add_css_class (label, "heading");

  return label;
}

static void
feat_toggled_cb (GtkCheckButton *check_button,
                 gpointer        data)
{
  set_inconsistent (check_button, FALSE);
}

static void
feat_pressed (GtkGestureClick *gesture,
              int              n_press,
              double           x,
              double           y,
              GtkWidget       *feat)
{
  const guint button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

  if (button == GDK_BUTTON_PRIMARY)
    {
      g_signal_handlers_block_by_func (feat, feat_pressed, NULL);

      if (gtk_check_button_get_inconsistent (GTK_CHECK_BUTTON (feat)))
        {
          set_inconsistent (GTK_CHECK_BUTTON (feat), FALSE);
          gtk_check_button_set_active (GTK_CHECK_BUTTON (feat), TRUE);
        }

      g_signal_handlers_unblock_by_func (feat, feat_pressed, NULL);
    }
  else if (button == GDK_BUTTON_SECONDARY)
    {
      gboolean inconsistent = gtk_check_button_get_inconsistent (GTK_CHECK_BUTTON (feat));
      set_inconsistent (GTK_CHECK_BUTTON (feat), !inconsistent);
    }
}

static void
add_check_group (FontFeatures  *self,
                 const char    *title,
                 const char   **tags,
                 unsigned int   n_tags,
                 int            row)
{
  GtkWidget *group;
  int i;

  group = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_halign (group, GTK_ALIGN_START);

  gtk_box_append (GTK_BOX (group), make_title_label (title));

  for (i = 0; i < n_tags; i++)
    {
      unsigned int tag;
      GtkWidget *feat;
      FeatureItem *item;
      GtkGesture *gesture;
      char *name;

      tag = hb_tag_from_string (tags[i], -1);

      name = get_feature_display_name (tag);
      feat = gtk_check_button_new_with_label (name);
      g_free (name);
      set_inconsistent (GTK_CHECK_BUTTON (feat), TRUE);
      g_signal_connect_swapped (feat, "notify::active", G_CALLBACK (update_display), self);
      g_signal_connect_swapped (feat, "notify::inconsistent", G_CALLBACK (update_display), self);
      g_signal_connect (feat, "toggled", G_CALLBACK (feat_toggled_cb), NULL);

      gesture = gtk_gesture_click_new ();
      gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_SECONDARY);
      g_signal_connect (gesture, "pressed", G_CALLBACK (feat_pressed), feat);
      gtk_widget_add_controller (feat, GTK_EVENT_CONTROLLER (gesture));

      gtk_box_append (GTK_BOX (group), feat);

      item = g_new (FeatureItem, 1);
      item->name = tags[i];
      item->tag = tag;
      item->feat = feat;

      self->feature_items = g_list_prepend (self->feature_items, item);
    }

  gtk_grid_attach (self->grid, group, 0, row, 2, 1);
}

static void
add_radio_group (FontFeatures  *self,
                 const char    *title,
                 const char   **tags,
                 unsigned int   n_tags,
                 int            row)
{
  GtkWidget *group;
  int i;
  GtkWidget *group_button = NULL;

  group = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_halign (group, GTK_ALIGN_START);

  gtk_box_append (GTK_BOX (group), make_title_label (title));

  for (i = 0; i < n_tags; i++)
    {
      unsigned int tag;
      GtkWidget *feat;
      FeatureItem *item;
      char *name;

      tag = hb_tag_from_string (tags[i], -1);
      name = get_feature_display_name (tag);
      feat = gtk_check_button_new_with_label (name ? name : _("Default"));
      g_free (name);
      if (group_button == NULL)
        group_button = feat;
      else
        gtk_check_button_set_group (GTK_CHECK_BUTTON (feat), GTK_CHECK_BUTTON (group_button));
      g_signal_connect_swapped (feat, "notify::active", G_CALLBACK (update_display), self);
      g_object_set_data (G_OBJECT (feat), "default", group_button);

      gtk_box_append (GTK_BOX (group), feat);

      item = g_new (FeatureItem, 1);
      item->name = tags[i];
      item->tag = tag;
      item->feat = feat;

      self->feature_items = g_list_prepend (self->feature_items, item);
    }

  gtk_grid_attach (self->grid, group, 0, row, 2, 1);
}

static void
setup_features (FontFeatures *self)
{
  const char *kerning[] = { "kern" };
  const char *ligatures[] = { "liga", "dlig", "hlig", "clig", "rlig" };
  const char *letter_case[] = {
    "smcp", "c2sc", "pcap", "c2pc", "unic", "cpsp", "case"
  };
  const char *number_case[] = { "xxxx", "lnum", "onum" };
  const char *number_spacing[] = { "xxxx", "pnum", "tnum" };
  const char *fractions[] = { "xxxx", "frac", "afrc" };
  const char *num_extras[] = { "zero", "nalt", "sinf" };
  const char *char_alt[] = {
    "swsh", "cswh", "locl", "calt", "falt", "hist",
    "salt", "jalt", "titl", "rand", "subs", "sups",
    "ordn", "ltra", "ltrm", "rtla", "rtlm", "rclt"
  };
  const char *pos_alt[] = {
    "init", "medi", "med2", "fina", "fin2", "fin3", "isol"
  };
  const char *width_var[] = {
    "fwid", "hwid", "halt", "pwid", "palt", "twid", "qwid"
  };
  const char *style_alt[] = {
    "ss01", "ss02", "ss03", "ss04", "ss05", "ss06",
    "ss07", "ss08", "ss09", "ss10", "ss11", "ss12",
    "ss13", "ss14", "ss15", "ss16", "ss17", "ss18",
    "ss19", "ss20"
  };
  const char *char_var[] = {
    "cv01", "cv02", "cv03", "cv04", "cv05", "cv06",
    "cv07", "cv08", "cv09", "cv10", "cv11", "cv12",
    "cv13", "cv14", "cv15", "cv16", "cv17", "cv18",
    "cv19", "cv20"
  };
  const char *math[] = { "dtls", "flac", "mgrk", "ssty" };
  const char *bounds[] = { "opbd", "lfbd", "rtbd" };
  int row = 0;

  add_check_group (self, _("Kerning"), kerning, G_N_ELEMENTS (kerning), row++);
  add_check_group (self, _("Ligatures"), ligatures, G_N_ELEMENTS (ligatures), row++);
  add_check_group (self, _("Letter Case"), letter_case, G_N_ELEMENTS (letter_case), row++);
  add_radio_group (self, _("Number Case"), number_case, G_N_ELEMENTS (number_case), row++);
  add_radio_group (self, _("Number Spacing"), number_spacing, G_N_ELEMENTS (number_spacing), row++);
  add_radio_group (self, _("Fractions"), fractions, G_N_ELEMENTS (fractions), row++);
  add_check_group (self, _("Numeric Extras"), num_extras, G_N_ELEMENTS (num_extras), row++);
  add_check_group (self, _("Character Alternatives"), char_alt, G_N_ELEMENTS (char_alt), row++);
  add_check_group (self, _("Positional Alternatives"), pos_alt, G_N_ELEMENTS (pos_alt), row++);
  add_check_group (self, _("Width Variants"), width_var, G_N_ELEMENTS (width_var), row++);
  add_check_group (self, _("Alternative Stylistic Sets"), style_alt, G_N_ELEMENTS (style_alt), row++);
  add_check_group (self, _("Character Variants"), char_var, G_N_ELEMENTS (char_var), row++);
  add_check_group (self, _("Mathematical"), math, G_N_ELEMENTS (math), row++);
  add_check_group (self, _("Optical Bounds"), bounds, G_N_ELEMENTS (bounds), row++);

  self->feature_items = g_list_reverse (self->feature_items);
}

static void
reset (GSimpleAction *action,
       GVariant      *parameter,
       FontFeatures   *self)
{
  update_features (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FEATURES]);
  g_simple_action_set_enabled (self->reset_action, FALSE);
}

static void
font_features_init (FontFeatures *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->font_desc = pango2_font_description_from_string ("sans 12");
  self->lang = pango2_language_get_default ();

  setup_features (self);

  self->reset_action = g_simple_action_new ("reset", NULL);
  g_simple_action_set_enabled (self->reset_action, FALSE);
  g_signal_connect (self->reset_action, "activate", G_CALLBACK (reset), self);
}

static void
font_features_dispose (GObject *object)
{
  FontFeatures *self = FONT_FEATURES (object);

  gtk_widget_clear_template (GTK_WIDGET (object), FONT_FEATURES_TYPE);

  g_list_free_full (self->feature_items, g_free);

  G_OBJECT_CLASS (font_features_parent_class)->dispose (object);
}

static void
font_features_finalize (GObject *object)
{
  FontFeatures *self = FONT_FEATURES (object);

  g_clear_pointer (&self->font_desc, pango2_font_description_free);
  g_clear_object (&self->map);

  G_OBJECT_CLASS (font_features_parent_class)->finalize (object);
}

static void
font_features_set_property (GObject      *object,
                            unsigned int  prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  FontFeatures *self = FONT_FEATURES (object);

  switch (prop_id)
    {
    case PROP_FONT_DESC:
      pango2_font_description_free (self->font_desc);
      self->font_desc = pango2_font_description_copy (g_value_get_boxed (value));
      update_features (self);
      break;

    case PROP_LANGUAGE:
      self->lang = pango2_language_from_string (g_value_get_string (value));
      update_features (self);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
font_features_get_property (GObject      *object,
                              unsigned int  prop_id,
                              GValue       *value,
                              GParamSpec   *pspec)
{
  FontFeatures *self = FONT_FEATURES (object);

  switch (prop_id)
    {
    case PROP_FEATURES:
      g_value_take_string (value, get_features (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
font_features_class_init (FontFeaturesClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = font_features_dispose;
  object_class->finalize = font_features_finalize;
  object_class->get_property = font_features_get_property;
  object_class->set_property = font_features_set_property;

  properties[PROP_FONT_DESC] =
      g_param_spec_boxed ("font-desc", "", "",
                          PANGO2_TYPE_FONT_DESCRIPTION,
                          G_PARAM_WRITABLE);

  properties[PROP_LANGUAGE] =
      g_param_spec_string ("language", "", "",
                           "en",
                           G_PARAM_WRITABLE);

  properties[PROP_FEATURES] =
      g_param_spec_string ("features", "", "",
                           "",
                           G_PARAM_READABLE);

  g_object_class_install_properties (G_OBJECT_CLASS (class), NUM_PROPERTIES, properties);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (class),
                                               "/org/gtk/fontexplorer/fontfeatures.ui");
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontFeatures, grid);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontFeatures, label);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), script_changed);

  gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (class), "fontfeatures");
}

FontFeatures *
font_features_new (void)
{
  return g_object_new (FONT_FEATURES_TYPE, NULL);
}

GAction *
font_features_get_reset_action (FontFeatures *self)
{
  return G_ACTION (self->reset_action);
}

void
font_features_set_font_map (FontFeatures  *self,
                            Pango2FontMap *map)
{
  g_set_object (&self->map, map);
  update_features (self);
}
