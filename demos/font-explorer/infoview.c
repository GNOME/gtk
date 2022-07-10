#include "infoview.h"
#include <gtk/gtk.h>

#include <hb-ot.h>

enum {
  PROP_FONT_MAP = 1,
  PROP_FONT_DESC,
  PROP_SIZE,
  PROP_VARIATIONS,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

struct _InfoView
{
  GtkWidget parent;

  GtkGrid *info;

  Pango2FontMap *font_map;
  Pango2FontDescription *font_desc;
  float size;
  char *variations;
};

struct _InfoViewClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE(InfoView, info_view, GTK_TYPE_WIDGET);

static void
info_view_init (InfoView *self)
{
  self->font_map = g_object_ref (pango2_font_map_get_default ());
  self->font_desc = pango2_font_description_from_string ("sans 12");
  self->size = 12.;
  self->variations = g_strdup ("");

  gtk_widget_set_layout_manager (GTK_WIDGET (self),
                                 gtk_box_layout_new (GTK_ORIENTATION_VERTICAL));
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
info_view_dispose (GObject *object)
{
  gtk_widget_clear_template (GTK_WIDGET (object), INFO_VIEW_TYPE);

  G_OBJECT_CLASS (info_view_parent_class)->dispose (object);
}

static void
info_view_finalize (GObject *object)
{
  InfoView *self = INFO_VIEW (object);

  g_clear_object (&self->font_map);
  pango2_font_description_free (self->font_desc);
  g_free (self->variations);

  G_OBJECT_CLASS (info_view_parent_class)->finalize (object);
}

static Pango2Font *
get_font (InfoView *self,
          int       size)
{
  Pango2Context *context;
  Pango2FontDescription *desc;
  Pango2Font *font;

  context = pango2_context_new_with_font_map (self->font_map);
  desc = pango2_font_description_copy_static (self->font_desc);
  pango2_font_description_set_variations (desc, self->variations);
  pango2_font_description_set_size (desc, size);
  font = pango2_context_load_font (context, desc);
  pango2_font_description_free (desc);
  g_object_unref (context);

  return font;
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
add_misc_line (InfoView   *self,
               const char *title,
               const char *value,
               int         row)
{
  GtkWidget *label;

  label = gtk_label_new (title);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_START);
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_grid_attach (self->info, label, 0, row, 1, 1);

  label = gtk_label_new (value);
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  gtk_widget_set_valign (label, GTK_ALIGN_START);
  gtk_label_set_xalign (GTK_LABEL (label), 1);
  gtk_label_set_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_width_chars (GTK_LABEL (label), 40);
  gtk_label_set_max_width_chars (GTK_LABEL (label), 40);
  gtk_grid_attach (self->info, label, 1, row, 1, 1);
}

static void
add_info_line (InfoView        *self,
               hb_face_t       *face,
               hb_ot_name_id_t  name_id,
               const char      *title,
               int              row)
{
  char info[256];
  unsigned int len = sizeof (info);

  if (hb_ot_name_get_utf8 (face, name_id, HB_LANGUAGE_INVALID, &len, info) > 0)
    add_misc_line (self, title, info, row);
}

static void
add_metrics_line (InfoView            *self,
                  hb_font_t           *font,
                  hb_ot_metrics_tag_t  metrics_tag,
                  const char          *title,
                  int                  row)
{
  hb_position_t pos;

  if (hb_ot_metrics_get_position (font, metrics_tag, &pos))
    {
      char buf[128];

      g_snprintf (buf, sizeof (buf), "%d", pos);
      add_misc_line (self, title, buf, row);
    }
}

static void
add_style_line (InfoView       *self,
                hb_font_t      *font,
                hb_style_tag_t  style_tag,
                const char     *title,
                int             row)
{
  float value;
  char buf[16];

  value = hb_style_get_value (font, style_tag);
  g_snprintf (buf, sizeof (buf), "%.2f", value);
  add_misc_line (self, title, buf, row);
}

static void
update_info (InfoView *self)
{
  GtkWidget *child;
  int size = pango2_font_description_get_size (self->font_desc);
  Pango2Font *pango_font = get_font (self, MAX (size, 10 * PANGO2_SCALE));
  hb_font_t *font1 = pango2_font_get_hb_font (pango_font);
  hb_face_t *face = hb_font_get_face (font1);
  hb_font_t *font = hb_font_create_sub_font (font1);
  int row = 0;
  char buf[128];
  unsigned int count;
  GString *s;
  hb_tag_t *tables;

  hb_font_set_scale (font, hb_face_get_upem (face), hb_face_get_upem (face));

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self->info))) != NULL)
    gtk_widget_unparent (child);

  gtk_grid_attach (self->info, make_title_label ("General Info"), 0, row++, 2, 1);
  add_info_line (self, face, HB_OT_NAME_ID_FONT_FAMILY, "Font Family Name", row++);
  add_info_line (self, face, HB_OT_NAME_ID_FONT_SUBFAMILY, "Font Subfamily Name", row++);
  add_info_line (self, face, HB_OT_NAME_ID_UNIQUE_ID, "Unique Font Identifier", row++);
  add_info_line (self, face, HB_OT_NAME_ID_FULL_NAME, "Full Name", row++);
  add_info_line (self, face, HB_OT_NAME_ID_VERSION_STRING, "Version", row++);
  add_info_line (self, face, HB_OT_NAME_ID_POSTSCRIPT_NAME, "Postscript Name", row++);
  add_info_line (self, face, HB_OT_NAME_ID_TYPOGRAPHIC_FAMILY, "Typographic Family Name", row++);
  add_info_line (self, face, HB_OT_NAME_ID_TYPOGRAPHIC_SUBFAMILY, "Typographic Subfamily Name", row++);
  add_info_line (self, face, HB_OT_NAME_ID_MANUFACTURER, "Vendor ID", row++);
  add_info_line (self, face, HB_OT_NAME_ID_DESIGNER, "Designer", row++);
  add_info_line (self, face, HB_OT_NAME_ID_DESCRIPTION, "Description", row++);
  add_info_line (self, face, HB_OT_NAME_ID_COPYRIGHT, "Copyright", row++);

  gtk_grid_attach (self->info, make_title_label ("Metrics"), 0, row++, 2, 1);
  g_snprintf (buf, sizeof (buf), "%d", hb_face_get_upem (face));
  add_misc_line (self, "Units per Em", buf, row++);
  add_metrics_line (self, font, HB_OT_METRICS_TAG_HORIZONTAL_ASCENDER, "Ascender", row++);
  add_metrics_line (self, font, HB_OT_METRICS_TAG_HORIZONTAL_DESCENDER, "Descender", row++);
  add_metrics_line (self, font, HB_OT_METRICS_TAG_HORIZONTAL_LINE_GAP, "Line Gap", row++);
  add_metrics_line (self, font, HB_OT_METRICS_TAG_HORIZONTAL_CARET_RISE, "Caret Rise", row++);
  add_metrics_line (self, font, HB_OT_METRICS_TAG_HORIZONTAL_CARET_RUN, "Caret Run", row++);
  add_metrics_line (self, font, HB_OT_METRICS_TAG_HORIZONTAL_CARET_OFFSET, "Caret Offset", row++);
  add_metrics_line (self, font, HB_OT_METRICS_TAG_X_HEIGHT, "x Height", row++);
  add_metrics_line (self, font, HB_OT_METRICS_TAG_CAP_HEIGHT, "Cap Height", row++);

  add_metrics_line (self, font, HB_OT_METRICS_TAG_STRIKEOUT_SIZE, "Strikeout Size", row++);
  add_metrics_line (self, font, HB_OT_METRICS_TAG_STRIKEOUT_OFFSET, "Strikeout Offset", row++);
  add_metrics_line (self, font, HB_OT_METRICS_TAG_STRIKEOUT_SIZE, "Underline Size", row++);
  add_metrics_line (self, font, HB_OT_METRICS_TAG_STRIKEOUT_OFFSET, "Underline Offset", row++);

  gtk_grid_attach (self->info, make_title_label ("Style"), 0, row++, 2, 1);

  add_style_line (self, font, HB_STYLE_TAG_ITALIC, "Italic", row++);
  add_style_line (self, font, HB_STYLE_TAG_OPTICAL_SIZE, "Optical Size", row++);
  add_style_line (self, font, HB_STYLE_TAG_SLANT_ANGLE, "Slant Angle", row++);
  add_style_line (self, font, HB_STYLE_TAG_WIDTH, "Width", row++);
  add_style_line (self, font, HB_STYLE_TAG_WEIGHT, "Weight", row++);

  gtk_grid_attach (self->info, make_title_label ("Miscellaneous"), 0, row++, 2, 1);

  count = hb_face_get_glyph_count (face);
  g_snprintf (buf, sizeof (buf), "%d", count);
  add_misc_line (self, "Glyph Count", buf, row++);

  if (hb_ot_var_get_axis_count (face) > 0)
    {
      s = g_string_new ("");
      hb_ot_var_axis_info_t *axes;

      axes = g_newa (hb_ot_var_axis_info_t, hb_ot_var_get_axis_count (face));
      count = hb_ot_var_get_axis_count (face);
      hb_ot_var_get_axis_infos (face, 0, &count, axes);
      for (int i = 0; i < count; i++)
        {
          char name[256];
          unsigned int len;

          len = sizeof (buf);
          hb_ot_name_get_utf8 (face, axes[i].name_id, HB_LANGUAGE_INVALID, &len, name);
          if (s->len > 0)
            g_string_append (s, ", ");
          g_string_append (s, name);
        }
      add_misc_line (self, "Axes", s->str, row++);
      g_string_free (s, TRUE);
    }

  if (hb_ot_var_get_named_instance_count (face) > 0)
    {
      s = g_string_new ("");
      for (int i = 0; i < hb_ot_var_get_named_instance_count (face); i++)
        {
          hb_ot_name_id_t name_id;
          char name[256];
          unsigned int len;

          name_id = hb_ot_var_named_instance_get_subfamily_name_id (face, i);
          len = sizeof (buf);
          hb_ot_name_get_utf8 (face, name_id, HB_LANGUAGE_INVALID, &len, name);
          if (s->len > 0)
            g_string_append (s, ", ");
          g_string_append (s, name);
        }
      add_misc_line (self, "Named Instances", s->str, row++);
      g_string_free (s, TRUE);
    }

  s = g_string_new ("");
  count = hb_face_get_table_tags (face, 0, NULL, NULL);
  tables = g_newa (hb_tag_t, count);
  hb_face_get_table_tags (face, 0, &count, tables);
  for (int i = 0; i < count; i++)
    {
      memset (buf, 0, sizeof (buf));
      hb_tag_to_string (tables[i], buf);
      if (s->len > 0)
        g_string_append (s, ", ");
      g_string_append (s, buf);
    }
  add_misc_line (self, "Tables", s->str, row++);
  g_string_free (s, TRUE);

  s = g_string_new ("");
  if (hb_ot_color_has_palettes (face))
    g_string_append_printf (s, "%s", "Palettes");
  if (hb_ot_color_has_layers (face))
    g_string_append_printf (s, "%s%s", s->len > 0 ? ", " : "", "Layers");
  if (hb_ot_color_has_svg (face))
    g_string_append_printf (s, "%s%s", s->len > 0 ? ", " : "", "SVG");
  if (hb_ot_color_has_png (face))
    g_string_append_printf (s, "%s%s", s->len > 0 ? ", " : "", "PNG");
  if (s->len > 0)
    add_misc_line (self, "Color", s->str, row++);
  g_string_free (s, TRUE);

  hb_font_destroy (font);
}

static void
info_view_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  InfoView *self = INFO_VIEW (object);

  switch (prop_id)
    {
    case PROP_FONT_MAP:
      g_set_object (&self->font_map, g_value_get_object (value));
      break;

    case PROP_FONT_DESC:
      pango2_font_description_free (self->font_desc);
      self->font_desc = pango2_font_description_copy (g_value_get_boxed (value));
      break;

    case PROP_SIZE:
      self->size = g_value_get_float (value);
      break;

    case PROP_VARIATIONS:
      g_free (self->variations);
      self->variations = g_strdup (g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }

  update_info (self);
}

static void
info_view_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  InfoView *self = INFO_VIEW (object);

  switch (prop_id)
    {
    case PROP_FONT_MAP:
      g_value_set_object (value, self->font_map);
      break;

    case PROP_FONT_DESC:
      g_value_set_boxed (value, self->font_desc);
      break;

    case PROP_SIZE:
      g_value_set_float (value, self->size);
      break;

    case PROP_VARIATIONS:
      g_value_set_string (value, self->variations);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
info_view_class_init (InfoViewClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = info_view_dispose;
  object_class->finalize = info_view_finalize;
  object_class->get_property = info_view_get_property;
  object_class->set_property = info_view_set_property;

  properties[PROP_FONT_MAP] =
      g_param_spec_object ("font-map", "", "",
                           PANGO2_TYPE_FONT_MAP,
                           G_PARAM_READWRITE);

  properties[PROP_FONT_DESC] =
      g_param_spec_boxed ("font-desc", "", "",
                          PANGO2_TYPE_FONT_DESCRIPTION,
                          G_PARAM_READWRITE);

  properties[PROP_SIZE] =
      g_param_spec_float ("size", "", "",
                          0., 100., 12.,
                          G_PARAM_READWRITE);

  properties[PROP_VARIATIONS] =
      g_param_spec_string ("variations", "", "",
                           "",
                           G_PARAM_READWRITE);

  g_object_class_install_properties (G_OBJECT_CLASS (class), NUM_PROPERTIES, properties);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (class),
                                               "/org/gtk/fontexplorer/infoview.ui");

  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), InfoView, info);

  gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (class), "infoview");
}
