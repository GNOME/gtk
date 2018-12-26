/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Alberto Ruiz <aruiz@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkfontchooserwidget.h"
#include "gtkfontchooserwidgetprivate.h"
#include "gtkfontfeaturesprivate.h"

#include "gtkadjustment.h"
#include "gtkcheckbutton.h"
#include "gtkgrid.h"
#include "gtkintl.h"
#include "gtklabel.h"
#include "gtkscale.h"
#include "gtkspinbutton.h"
#include "gtkwidget.h"

#include "gtkradiobutton.h"
#include "gtkgesturemultipress.h"

#if defined (HAVE_HARFBUZZ) && defined (HAVE_PANGOFT)
#include <pango/pangofc-font.h>
#include <hb.h>
#include <hb-ot.h>
#include <hb-ft.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MULTIPLE_MASTERS_H

#include "language-names.h"
#include "script-names.h"
#include "open-type-layout.h"

static void update_font_features (GtkFontChooserWidget *fontchooser);

guint
axis_hash (gconstpointer v)
{
  const Axis *a = v;

  return a->tag;
}

gboolean
axis_equal (gconstpointer v1, gconstpointer v2)
{
  const Axis *a1 = v1;
  const Axis *a2 = v2;

  return a1->tag == a2->tag;
}

void
axis_free (gpointer v)
{
  Axis *a = v;

  g_free (a);
}

static void
axis_remove (gpointer key,
             gpointer value,
             gpointer data)
{
  Axis *a = value;

  gtk_widget_destroy (a->label);
  gtk_widget_destroy (a->scale);
  gtk_widget_destroy (a->spin);
}

/* OpenType variations */

#define FixedToFloat(f) (((float)(f))/65536.0)

static void
add_font_variations (GtkFontChooserWidget *fontchooser,
                     GString              *s)
{
  GHashTableIter iter;
  Axis *axis;
  const char *sep = "";
  char buf[G_ASCII_DTOSTR_BUF_SIZE];

  g_hash_table_iter_init (&iter, fontchooser->priv->axes);
  while (g_hash_table_iter_next (&iter, (gpointer *)NULL, (gpointer *)&axis))
    {
      char tag[5];
      double value;

      tag[0] = (axis->tag >> 24) & 0xff;
      tag[1] = (axis->tag >> 16) & 0xff;
      tag[2] = (axis->tag >> 8) & 0xff;
      tag[3] = (axis->tag >> 0) & 0xff;
      tag[4] = '\0';
      value = gtk_adjustment_get_value (axis->adjustment);
      g_string_append_printf (s, "%s%s=%s", sep, tag, g_ascii_dtostr (buf, sizeof(buf), value));
      sep = ",";
    }
}

static void
adjustment_changed (GtkAdjustment *adjustment,
                    Axis          *axis)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (axis->fontchooser);
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  PangoFontDescription *font_desc;
  GString *s;

  priv->updating_variations = TRUE;

  s = g_string_new ("");
  add_font_variations (fontchooser, s);

  if (s->len > 0)
    {
      font_desc = pango_font_description_new ();
      pango_font_description_set_variations (font_desc, s->str);
      gtk_font_chooser_widget_take_font_desc (fontchooser, font_desc);
    }

  g_string_free (s, TRUE);

  priv->updating_variations = FALSE;
}

static gboolean
should_show_axis (FT_Var_Axis *ax)
{
  /* FIXME use FT_Get_Var_Axis_Flags */
  if (ax->tag == FT_MAKE_TAG ('o', 'p', 's', 'z'))
    return FALSE;

  return TRUE;
}

static gboolean
is_named_instance (FT_Face face)
{
  return (face->face_index >> 16) > 0;
}

static struct {
  guint32 tag;
  const char *name;
} axis_names[] = {
  { FT_MAKE_TAG ('w', 'd', 't', 'h'), N_("Width") },
  { FT_MAKE_TAG ('w', 'g', 'h', 't'), N_("Weight") },
  { FT_MAKE_TAG ('i', 't', 'a', 'l'), N_("Italic") },
  { FT_MAKE_TAG ('s', 'l', 'n', 't'), N_("Slant") },
  { FT_MAKE_TAG ('o', 'p', 's', 'z'), N_("Optical Size") },
};

static gboolean
add_axis (GtkFontChooserWidget *fontchooser,
          FT_Face               face,
          FT_Var_Axis          *ax,
          FT_Fixed              value,
          int                   row)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  Axis *axis;
  const char *name;
  int i;

  axis = g_new (Axis, 1);
  axis->tag = ax->tag;
  axis->fontchooser = GTK_WIDGET (fontchooser);

  name = ax->name;
  for (i = 0; i < G_N_ELEMENTS (axis_names); i++)
    {
      if (axis_names[i].tag == ax->tag)
        {
          name = _(axis_names[i].name);
          break;
        }
    }
  axis->label = gtk_label_new (name);
  gtk_widget_show (axis->label);
  gtk_widget_set_halign (axis->label, GTK_ALIGN_START);
  gtk_widget_set_valign (axis->label, GTK_ALIGN_BASELINE);
  gtk_grid_attach (GTK_GRID (priv->axis_grid), axis->label, 0, row, 1, 1);
  axis->adjustment = gtk_adjustment_new ((double)FixedToFloat(value),
                                         (double)FixedToFloat(ax->minimum),
                                         (double)FixedToFloat(ax->maximum),
                                         1.0, 10.0, 0.0);
  axis->scale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, axis->adjustment);
  gtk_widget_show (axis->scale);
  gtk_scale_add_mark (GTK_SCALE (axis->scale), (double)FixedToFloat(ax->def), GTK_POS_TOP, NULL);
  gtk_widget_set_valign (axis->scale, GTK_ALIGN_BASELINE);
  gtk_widget_set_hexpand (axis->scale, TRUE);
  gtk_widget_set_size_request (axis->scale, 100, -1);
  gtk_scale_set_draw_value (GTK_SCALE (axis->scale), FALSE);
  gtk_grid_attach (GTK_GRID (priv->axis_grid), axis->scale, 1, row, 1, 1);
  axis->spin = gtk_spin_button_new (axis->adjustment, 0, 0);
  gtk_widget_show (axis->spin);
  g_signal_connect (axis->spin, "output", G_CALLBACK (output_cb), fontchooser);
  gtk_widget_set_valign (axis->spin, GTK_ALIGN_BASELINE);
  gtk_grid_attach (GTK_GRID (priv->axis_grid), axis->spin, 2, row, 1, 1);

  g_hash_table_add (priv->axes, axis);

  adjustment_changed (axis->adjustment, axis);
  g_signal_connect (axis->adjustment, "value-changed", G_CALLBACK (adjustment_changed), axis);
  if (is_named_instance (face) || !should_show_axis (ax))
    {
      gtk_widget_hide (axis->label);
      gtk_widget_hide (axis->scale);
      gtk_widget_hide (axis->spin);

      return FALSE;
    }

  return TRUE;
}

gboolean
gtk_font_chooser_widget_update_font_variations (GtkFontChooserWidget *fontchooser)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  PangoFont *pango_font;
  FT_Face ft_face;
  FT_MM_Var *ft_mm_var;
  FT_Error ret;
  gboolean has_axis = FALSE;

  if (priv->updating_variations)
    return FALSE;

  g_hash_table_foreach (priv->axes, axis_remove, NULL);
  g_hash_table_remove_all (priv->axes);

  if ((priv->level & GTK_FONT_CHOOSER_LEVEL_VARIATIONS) == 0)
    return FALSE;

  pango_font = pango_context_load_font (gtk_widget_get_pango_context (GTK_WIDGET (fontchooser)),
                                        priv->font_desc);

  ft_face = pango_fc_font_lock_face (PANGO_FC_FONT (pango_font));

  ret = FT_Get_MM_Var (ft_face, &ft_mm_var);
  if (ret == 0)
    {
      int i;
      FT_Fixed *coords;

      coords = g_new (FT_Fixed, ft_mm_var->num_axis);
      for (i = 0; i < ft_mm_var->num_axis; i++)
        coords[i] = ft_mm_var->axis[i].def;

      if (ft_face->face_index > 0)
        {
          int instance_id = ft_face->face_index >> 16;
          if (instance_id && instance_id <= ft_mm_var->num_namedstyles)
            {
              FT_Var_Named_Style *instance = &ft_mm_var->namedstyle[instance_id - 1];
              memcpy (coords, instance->coords, ft_mm_var->num_axis * sizeof (*coords));
            }
        }

      for (i = 0; i < ft_mm_var->num_axis; i++)
        {
          if (add_axis (fontchooser, ft_face, &ft_mm_var->axis[i], coords[i], i + 4))
            has_axis = TRUE;
        }

      g_free (coords);
      free (ft_mm_var);
    }

  pango_fc_font_unlock_face (PANGO_FC_FONT (pango_font));

  g_object_unref (pango_font);

  return has_axis;
}

/* OpenType features */

/* look for a lang / script combination that matches the
 * language property and is supported by the hb_face. If
 * none is found, return the default lang / script tags.
 */
static void
find_language_and_script (GtkFontChooserWidget *fontchooser,
                          hb_face_t            *hb_face,
                          hb_tag_t             *lang_tag,
                          hb_tag_t             *script_tag)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  gint i, j, k;
  hb_tag_t scripts[80];
  unsigned int n_scripts;
  unsigned int count;
  hb_tag_t table[2] = { HB_OT_TAG_GSUB, HB_OT_TAG_GPOS };
  hb_language_t lang;
  const char *langname, *p;

  langname = pango_language_to_string (priv->language);
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

typedef struct {
  hb_tag_t tag;
  const char *name;
  GtkWidget *top;
  GtkWidget *feat;
  GtkWidget *example;
} FeatureItem;

static const char *
get_feature_display_name (hb_tag_t tag)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (open_type_layout_features); i++)
    {
      if (tag == open_type_layout_features[i].tag)
        return g_dpgettext2 (NULL, "OpenType layout", open_type_layout_features[i].name);
    }

  return NULL;
}

static void
set_inconsistent (GtkCheckButton *button,
                  gboolean        inconsistent)
{
  if (inconsistent)
    gtk_widget_set_state_flags (GTK_WIDGET (button), GTK_STATE_FLAG_INCONSISTENT, FALSE);
  else
    gtk_widget_unset_state_flags (GTK_WIDGET (button), GTK_STATE_FLAG_INCONSISTENT);
//  gtk_widget_set_opacity (gtk_widget_get_first_child (GTK_WIDGET (button)), inconsistent ? 0.0 : 1.0);
}

static void
feat_clicked (GtkWidget *feat,
              gpointer   data)
{
  g_signal_handlers_block_by_func (feat, feat_clicked, NULL);

  if (gtk_widget_get_state_flags (feat) & GTK_STATE_FLAG_INCONSISTENT)
    {
      set_inconsistent (GTK_CHECK_BUTTON (feat), FALSE);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (feat), TRUE);
    }

  g_signal_handlers_unblock_by_func (feat, feat_clicked, NULL);
}

static void
feat_pressed (GtkGesture *gesture,
              int         n_press,
              double      x,
              double      y,
              GtkWidget  *feat)
{
  gboolean inconsistent;

  inconsistent = (gtk_widget_get_state_flags (feat) & GTK_STATE_FLAG_INCONSISTENT) != 0;
  set_inconsistent (GTK_CHECK_BUTTON (feat), !inconsistent);
}

static char *
find_affected_text (hb_tag_t   feature_tag,
                    hb_face_t *hb_face,
                    hb_tag_t   script_tag,
                    hb_tag_t   lang_tag,
                    int        max_chars)
{
  unsigned int script_index = 0;
  unsigned int lang_index = 0;
  unsigned int feature_index = 0;
  GString *chars;

  chars = g_string_new ("");

  hb_ot_layout_table_find_script (hb_face, HB_OT_TAG_GSUB, script_tag, &script_index);
  hb_ot_layout_script_find_language (hb_face, HB_OT_TAG_GSUB, script_index, lang_tag, &lang_index);
  if (hb_ot_layout_language_find_feature (hb_face, HB_OT_TAG_GSUB, script_index, lang_index, feature_tag, &feature_index))
    {
      unsigned int lookup_indexes[32];
      unsigned int lookup_count = 32;
      int count;
      int n_chars = 0;

      count  = hb_ot_layout_feature_get_lookups (hb_face,
                                                 HB_OT_TAG_GSUB,
                                                 feature_index,
                                                 0,
                                                 &lookup_count,
                                                 lookup_indexes);
      if (count > 0)
        {
          hb_set_t* glyphs_before = NULL;
          hb_set_t* glyphs_input  = NULL;
          hb_set_t* glyphs_after  = NULL;
          hb_set_t* glyphs_output = NULL;
          hb_font_t *hb_font = NULL;
          hb_codepoint_t gid;

          glyphs_input  = hb_set_create ();

          // XXX For now, just look at first index
          hb_ot_layout_lookup_collect_glyphs (hb_face,
                                              HB_OT_TAG_GSUB,
                                              lookup_indexes[0],
                                              glyphs_before,
                                              glyphs_input,
                                              glyphs_after,
                                              glyphs_output);

          hb_font = hb_font_create (hb_face);
          hb_ft_font_set_funcs (hb_font);

          gid = -1;
          while (hb_set_next (glyphs_input, &gid)) {
            hb_codepoint_t ch;
            if (n_chars == max_chars)
              {
                g_string_append (chars, "…");
                break;
              }
            for (ch = 0; ch < 0xffff; ch++) {
              hb_codepoint_t glyph = 0;
              hb_font_get_nominal_glyph (hb_font, ch, &glyph);
              if (glyph == gid) {
                g_string_append_unichar (chars, (gunichar)ch);
                n_chars++;
                break;
              }
            }
          }
          hb_set_destroy (glyphs_input);
          hb_font_destroy (hb_font);
        }
    }

  return g_string_free (chars, FALSE);
}

static void
update_feature_example (FeatureItem          *item,
                        hb_face_t            *hb_face,
                        hb_tag_t              script_tag,
                        hb_tag_t              lang_tag,
                        PangoFontDescription *font_desc)
{
  const char *letter_case[] = { "smcp", "c2sc", "pcap", "c2pc", "unic", "cpsp", "case", NULL };
  const char *number_case[] = { "xxxx", "lnum", "onum", NULL };
  const char *number_spacing[] = { "xxxx", "pnum", "tnum", NULL };
  const char *number_formatting[] = { "zero", "nalt", NULL };
  const char *char_variants[] = {
    "swsh", "cswh", "calt", "falt", "hist", "salt", "jalt", "titl", "rand",
    "ss01", "ss02", "ss03", "ss04", "ss05", "ss06", "ss07", "ss08", "ss09", "ss10",
    "ss11", "ss12", "ss13", "ss14", "ss15", "ss16", "ss17", "ss18", "ss19", "ss20",
    NULL };

  if (g_strv_contains (number_case, item->name) ||
      g_strv_contains (number_spacing, item->name))
    {
      PangoAttrList *attrs;
      PangoAttribute *attr;
      PangoFontDescription *desc;
      char *str;

      attrs = pango_attr_list_new ();

      desc = pango_font_description_copy (font_desc);
      pango_font_description_unset_fields (desc, PANGO_FONT_MASK_SIZE);
      pango_attr_list_insert (attrs, pango_attr_font_desc_new (desc));
      pango_font_description_free (desc);
      str = g_strconcat (item->name, " 1", NULL);
      attr = pango_attr_font_features_new (str);
      pango_attr_list_insert (attrs, attr);

      gtk_label_set_text (GTK_LABEL (item->example), "0123456789");
      gtk_label_set_attributes (GTK_LABEL (item->example), attrs);

      pango_attr_list_unref (attrs);
    }
  else if (g_strv_contains (letter_case, item->name) ||
           g_strv_contains (number_formatting, item->name) ||
           g_strv_contains (char_variants, item->name))
    {
      char *input = NULL;
      char *text;

      if (strcmp (item->name, "case") == 0)
        input = g_strdup ("A-B[Cq]");
      else if (g_strv_contains (letter_case, item->name))
        input = g_strdup ("AaBbCc…");
      else if (strcmp (item->name, "zero") == 0)
        input = g_strdup ("0");
      else if (strcmp (item->name, "nalt") == 0)
        input = find_affected_text (item->tag, hb_face, script_tag, lang_tag, 3);
      else
        input = find_affected_text (item->tag, hb_face, script_tag, lang_tag, 10);

      if (input[0] != '\0')
        {
          PangoAttrList *attrs;
          PangoAttribute *attr;
          PangoFontDescription *desc;
          char *str;

          text = g_strconcat (input, " ⟶ ", input, NULL);

          attrs = pango_attr_list_new ();

          desc = pango_font_description_copy (font_desc);
          pango_font_description_unset_fields (desc, PANGO_FONT_MASK_SIZE);
          pango_attr_list_insert (attrs, pango_attr_font_desc_new (desc));
          pango_font_description_free (desc);
          str = g_strconcat (item->name, " 0", NULL);
          attr = pango_attr_font_features_new (str);
          attr->start_index = 0;
          attr->end_index = strlen (input);
          pango_attr_list_insert (attrs, attr);
          str = g_strconcat (item->name, " 1", NULL);
          attr = pango_attr_font_features_new (str);
          attr->start_index = strlen (input) + strlen (" ⟶ ");
          attr->end_index = attr->start_index + strlen (input);
          pango_attr_list_insert (attrs, attr);

          gtk_label_set_text (GTK_LABEL (item->example), text);
          gtk_label_set_attributes (GTK_LABEL (item->example), attrs);

          g_free (text);
          pango_attr_list_unref (attrs);
        }
      else
        gtk_label_set_markup (GTK_LABEL (item->example), "");
      g_free (input);
    }
}

static void
add_check_group (GtkFontChooserWidget *fontchooser,
                 const char  *title,
                 const char **tags)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  GtkWidget *label;
  GtkWidget *group;
  PangoAttrList *attrs;
  int i;

  group = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_show (group);
  gtk_widget_set_halign (group, GTK_ALIGN_FILL);

  label = gtk_label_new (title);
  gtk_widget_show (label);
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
      hb_tag_t tag;
      GtkWidget *feat;
      FeatureItem *item;
      GtkGesture *gesture;
      GtkWidget *box;
      GtkWidget *example;

      tag = hb_tag_from_string (tags[i], -1);

      feat = gtk_check_button_new_with_label (get_feature_display_name (tag));
      gtk_widget_show (feat);
      set_inconsistent (GTK_CHECK_BUTTON (feat), TRUE);
      g_signal_connect_swapped (feat, "notify::active", G_CALLBACK (update_font_features), fontchooser);
      g_signal_connect_swapped (feat, "notify::inconsistent", G_CALLBACK (update_font_features), fontchooser);
      g_signal_connect (feat, "clicked", G_CALLBACK (feat_clicked), NULL);

      gesture = gtk_gesture_multi_press_new (feat);
      g_object_set_data_full (G_OBJECT (feat), "press", gesture, g_object_unref);

      gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_SECONDARY);
      g_signal_connect (gesture, "pressed", G_CALLBACK (feat_pressed), feat);

      example = gtk_label_new ("");
      gtk_widget_show (example);
      gtk_label_set_selectable (GTK_LABEL (example), TRUE);
      gtk_widget_set_halign (example, GTK_ALIGN_START);

      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
      gtk_widget_show (box);
      gtk_box_set_homogeneous (GTK_BOX (box), TRUE);
      gtk_container_add (GTK_CONTAINER (box), feat);
      gtk_container_add (GTK_CONTAINER (box), example);
      gtk_container_add (GTK_CONTAINER (group), box);

      item = g_new (FeatureItem, 1);
      item->name = tags[i];
      item->tag = tag;
      item->top = box;
      item->feat = feat;
      item->example = example;

      priv->feature_items = g_list_prepend (priv->feature_items, item);
    }

  gtk_container_add (GTK_CONTAINER (priv->feature_box), group);
}

static void
add_radio_group (GtkFontChooserWidget *fontchooser,
                 const char  *title,
                 const char **tags)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  GtkWidget *label;
  GtkWidget *group;
  int i;
  GtkWidget *group_button = NULL;
  PangoAttrList *attrs;

  group = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_show (group);
  gtk_widget_set_halign (group, GTK_ALIGN_FILL);

  label = gtk_label_new (title);
  gtk_widget_show (label);
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
      hb_tag_t tag;
      GtkWidget *feat;
      FeatureItem *item;
      const char *name;
      GtkWidget *box;
      GtkWidget *example;

      tag = hb_tag_from_string (tags[i], -1);
      name = get_feature_display_name (tag);

      feat = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (group_button),
                                                          name ? name : _("Default"));
      gtk_widget_show (feat);
      if (group_button == NULL)
        group_button = feat;

      g_signal_connect_swapped (feat, "notify::active", G_CALLBACK (update_font_features), fontchooser);
      g_object_set_data (G_OBJECT (feat), "default", group_button);

      example = gtk_label_new ("");
      gtk_widget_show (example);
      gtk_label_set_selectable (GTK_LABEL (example), TRUE);
      gtk_widget_set_halign (example, GTK_ALIGN_START);

      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
      gtk_widget_show (box);
      gtk_box_set_homogeneous (GTK_BOX (box), TRUE);
      gtk_container_add (GTK_CONTAINER (box), feat);
      gtk_container_add (GTK_CONTAINER (box), example);
      gtk_container_add (GTK_CONTAINER (group), box);

      item = g_new (FeatureItem, 1);
      item->name = tags[i];
      item->tag = tag;
      item->top = box;
      item->feat = feat;
      item->example = example;

      priv->feature_items = g_list_prepend (priv->feature_items, item);
    }

  gtk_container_add (GTK_CONTAINER (priv->feature_box), group);
}

void
gtk_font_chooser_widget_populate_features (GtkFontChooserWidget *fontchooser)
{
  const char *ligatures[] = { "liga", "dlig", "hlig", "clig", NULL };
  const char *letter_case[] = { "smcp", "c2sc", "pcap", "c2pc", "unic", "cpsp", "case", NULL };
  const char *number_case[] = { "xxxx", "lnum", "onum", NULL };
  const char *number_spacing[] = { "xxxx", "pnum", "tnum", NULL };
  const char *number_formatting[] = { "zero", "nalt", NULL };
  const char *char_variants[] = {
    "swsh", "cswh", "calt", "falt", "hist", "salt", "jalt", "titl", "rand",
    "ss01", "ss02", "ss03", "ss04", "ss05", "ss06", "ss07", "ss08", "ss09", "ss10",
    "ss11", "ss12", "ss13", "ss14", "ss15", "ss16", "ss17", "ss18", "ss19", "ss20",
    NULL };

  add_check_group (fontchooser, _("Ligatures"), ligatures);
  add_check_group (fontchooser, _("Letter Case"), letter_case);
  add_radio_group (fontchooser, _("Number Case"), number_case);
  add_radio_group (fontchooser, _("Number Spacing"), number_spacing);
  add_check_group (fontchooser, _("Number Formatting"), number_formatting);
  add_check_group (fontchooser, _("Character Variants"), char_variants);

  update_font_features (fontchooser);
}

gboolean
gtk_font_chooser_widget_update_font_features (GtkFontChooserWidget *fontchooser)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  PangoFont *pango_font;
  FT_Face ft_face;
  hb_font_t *hb_font;
  hb_tag_t script_tag;
  hb_tag_t lang_tag;
  guint script_index = 0;
  guint lang_index = 0;
  int i, j;
  GList *l;
  gboolean has_feature = FALSE;

  for (l = priv->feature_items; l; l = l->next)
    {
      FeatureItem *item = l->data;
      gtk_widget_hide (item->top);
      gtk_widget_hide (gtk_widget_get_parent (item->top));
    }

  if ((priv->level & GTK_FONT_CHOOSER_LEVEL_FEATURES) == 0)
    return FALSE;

  pango_font = pango_context_load_font (gtk_widget_get_pango_context (GTK_WIDGET (fontchooser)),
                                        priv->font_desc);

  ft_face = pango_fc_font_lock_face (PANGO_FC_FONT (pango_font));

  hb_font = hb_ft_font_create (ft_face, NULL);

  if (hb_font)
    {
      hb_tag_t table[2] = { HB_OT_TAG_GSUB, HB_OT_TAG_GPOS };
      hb_face_t *hb_face;
      hb_tag_t features[80];
      unsigned int count;
      unsigned int n_features;

      hb_face = hb_font_get_face (hb_font);

      find_language_and_script (fontchooser, hb_face, &lang_tag, &script_tag);

      n_features = 0;
      for (i = 0; i < 2; i++)
        {
          hb_ot_layout_table_find_script (hb_face, table[i], script_tag, &script_index);
          hb_ot_layout_script_find_language (hb_face, table[i], script_index, lang_tag, &lang_index);
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

      for (j = 0; j < n_features; j++)
        {
          for (l = priv->feature_items; l; l = l->next)
            {
              FeatureItem *item = l->data;
              if (item->tag != features[j])
                continue;

              has_feature = TRUE;
              gtk_widget_show (item->top);
              gtk_widget_show (gtk_widget_get_parent (item->top));

              update_feature_example (item, hb_face, script_tag, lang_tag, priv->font_desc);

              if (GTK_IS_RADIO_BUTTON (item->feat))
                {
                  GtkWidget *def = GTK_WIDGET (g_object_get_data (G_OBJECT (item->feat), "default"));
                  gtk_widget_show (gtk_widget_get_parent (def));
                }
              else if (GTK_IS_CHECK_BUTTON (item->feat))
                {
                  set_inconsistent (GTK_CHECK_BUTTON (item->feat), TRUE);
                }
            }
        }

      hb_face_destroy (hb_face);
    }

  pango_fc_font_unlock_face (PANGO_FC_FONT (pango_font));

  g_object_unref (pango_font);

  return has_feature;
}

static void
update_font_features (GtkFontChooserWidget *fontchooser)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  GString *s;
  GList *l;

  s = g_string_new ("");

  for (l = priv->feature_items; l; l = l->next)
    {
      FeatureItem *item = l->data;

      if (!gtk_widget_is_sensitive (item->feat))
        continue;

      if (GTK_IS_RADIO_BUTTON (item->feat))
        {
          if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (item->feat)) &&
              strcmp (item->name, "xxxx") != 0)
            {
              g_string_append_printf (s, "%s\"%s\" %d", s->len > 0 ? ", " : "", item->name, 1);
            }
        }
      else if (GTK_IS_CHECK_BUTTON (item->feat))
        {
          if (gtk_widget_get_state_flags (item->feat) & GTK_STATE_FLAG_INCONSISTENT)
            continue;

          g_string_append_printf (s, "%s\"%s\" %d",
                                  s->len > 0 ? ", " : "", item->name,
                                  gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (item->feat)));
        }
    }

  if (g_strcmp0 (priv->font_features, s->str) != 0)
    {
      g_free (priv->font_features);
      priv->font_features = g_string_free (s, FALSE);
      g_object_notify (G_OBJECT (fontchooser), "font-features");
    }
  else
    g_string_free (s, TRUE);

  gtk_font_chooser_widget_update_preview_attributes (fontchooser);
}
#endif

gboolean
output_cb (GtkSpinButton *spin,
           gpointer       data)
{
  GtkAdjustment *adjustment;
  gchar *text;
  gdouble value;

  adjustment = gtk_spin_button_get_adjustment (spin);
  value = gtk_adjustment_get_value (adjustment);
  text = g_strdup_printf ("%2.4g", value);
  gtk_entry_set_text (GTK_ENTRY (spin), text);
  g_free (text);

  return TRUE;
}
