#include "fontvariations.h"
#include "rangeedit.h"
#include <gtk/gtk.h>
#include <hb-ot.h>

enum {
  PROP_FONT_DESC = 1,
  PROP_VARIATIONS,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

struct _FontVariations
{
  GtkWidget parent;

  GtkGrid *label;
  GtkGrid *grid;
  Pango2FontDescription *font_desc;
  GSimpleAction *reset_action;
  gboolean has_variations;

  GtkWidget *instance_combo;
  GHashTable *axes;
  GHashTable *instances;

  Pango2FontMap *map;
};

struct _FontVariationsClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE(FontVariations, font_variations, GTK_TYPE_WIDGET);

static Pango2Font *
get_font (FontVariations *self)
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

typedef struct {
  guint32 tag;
  GtkAdjustment *adjustment;
  double default_value;
} Axis;

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
unset_instance (GtkAdjustment  *adjustment,
                FontVariations *self)
{
  if (self->instance_combo)
    gtk_combo_box_set_active (GTK_COMBO_BOX (self->instance_combo), 0);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VARIATIONS]);
  g_simple_action_set_enabled (self->reset_action, TRUE);
}

static void
add_axis (FontVariations        *self,
          hb_face_t             *hb_face,
          hb_ot_var_axis_info_t *ax,
          int                    i)
{
  GtkWidget *axis_label;
  GtkWidget *axis_scale;
  GtkAdjustment *adjustment;
  Axis *axis;
  char name[20];
  unsigned int name_len = 20;

  hb_ot_name_get_utf8 (hb_face, ax->name_id, HB_LANGUAGE_INVALID, &name_len, name);

  axis_label = gtk_label_new (name);
  gtk_widget_set_halign (axis_label, GTK_ALIGN_START);
  gtk_widget_set_valign (axis_label, GTK_ALIGN_BASELINE);
  gtk_grid_attach (self->grid, axis_label, 0, i, 1, 1);
  adjustment = gtk_adjustment_new (ax->default_value, ax->min_value, ax->max_value,
                                   1.0, 10.0, 0.0);
  axis_scale = g_object_new (RANGE_EDIT_TYPE,
                             "adjustment", adjustment,
                             "default-value", ax->default_value,
                             "n-chars", 5,
                             "hexpand", TRUE,
                             "halign", GTK_ALIGN_FILL,
                             "valign", GTK_ALIGN_BASELINE,
                             NULL);
  gtk_grid_attach (self->grid, axis_scale, 1, i, 1, 1);

  axis = g_new0 (Axis, 1);
  axis->tag = ax->tag;
  axis->adjustment = adjustment;
  axis->default_value = ax->default_value;
  g_hash_table_add (self->axes, axis);

  g_signal_connect (adjustment, "value-changed", G_CALLBACK (unset_instance), self);
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

static void
add_instance (FontVariations *self,
              hb_face_t      *face,
              unsigned int    index,
              GtkWidget      *combo,
              int             pos)
{
  Instance *instance;
  hb_ot_name_id_t name_id;
  char name[20];
  unsigned int name_len = 20;

  instance = g_new0 (Instance, 1);

  name_id = hb_ot_var_named_instance_get_subfamily_name_id (face, index);
  hb_ot_name_get_utf8 (face, name_id, HB_LANGUAGE_INVALID, &name_len, name);

  instance->name = g_strdup (name);
  instance->index = index;

  g_hash_table_add (self->instances, instance);
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), instance->name);
}

static void
instance_changed (GtkComboBox    *combo,
                  FontVariations *self)
{
  char *text;
  Instance *instance;
  Instance ikey;
  int i;
  unsigned int coords_length;
  float *coords = NULL;
  hb_ot_var_axis_info_t *ai = NULL;
  unsigned int n_axes;
  Pango2Font *font = NULL;
  hb_font_t *hb_font;
  hb_face_t *hb_face;

  text = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (combo));
  if (text[0] == '\0')
    goto out;

  ikey.name = text;
  instance = g_hash_table_lookup (self->instances, &ikey);
  if (!instance)
    {
      g_print ("did not find instance %s\n", text);
      goto out;
    }

  font = get_font (self);
  hb_font = pango2_font_get_hb_font (font);
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
      double value;

      value = coords[ai[i].axis_index];

      akey.tag = ai[i].tag;
      axis = g_hash_table_lookup (self->axes, &akey);
      if (axis)
        {
          g_signal_handlers_block_by_func (axis->adjustment, unset_instance, self);
          gtk_adjustment_set_value (axis->adjustment, value);
          g_signal_handlers_unblock_by_func (axis->adjustment, unset_instance, self);
        }
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VARIATIONS]);
  g_simple_action_set_enabled (self->reset_action, TRUE);

out:
  g_free (text);
  g_clear_object (&font);
  g_free (ai);
  g_free (coords);
}

static void
update_variations (FontVariations *self)
{
  GtkWidget *child;
  Pango2Font *font;
  hb_font_t *hb_font;
  hb_face_t *hb_face;
  unsigned int n_axes;
  hb_ot_var_axis_info_t *ai = NULL;
  int i;

  font = get_font (self);
  hb_font = pango2_font_get_hb_font (font);
  hb_face = hb_font_get_face (hb_font);

  g_object_ref (self->label);

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self->grid))))
    gtk_grid_remove (self->grid, child);

  gtk_grid_attach (self->grid, GTK_WIDGET (self->label), 0, -2, 2, 1);
  g_object_unref (self->label);

  self->instance_combo = NULL;
  g_hash_table_remove_all (self->axes);
  g_hash_table_remove_all (self->instances);

  n_axes = hb_ot_var_get_axis_infos (hb_face, 0, NULL, NULL);
  self->has_variations = n_axes > 0;
  gtk_widget_set_visible (GTK_WIDGET (self), self->has_variations);
  if (!self->has_variations)
    {
      g_simple_action_set_enabled (self->reset_action, FALSE);
      return;
    }

  if (hb_ot_var_get_named_instance_count (hb_face) > 0)
    {
      GtkWidget *label;
      GtkWidget *combo;

      label = gtk_label_new ("Instance");
      gtk_label_set_xalign (GTK_LABEL (label), 0);
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_BASELINE);
      gtk_grid_attach (self->grid, label, 0, -1, 1, 1);

      combo = gtk_combo_box_text_new ();
      gtk_widget_set_halign (combo, GTK_ALIGN_START);
      gtk_widget_set_valign (combo, GTK_ALIGN_BASELINE);
      gtk_widget_set_hexpand (combo, TRUE);

      gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "");

      for (i = 0; i < hb_ot_var_get_named_instance_count (hb_face); i++)
        add_instance (self, hb_face, i, combo, i);

      gtk_grid_attach (GTK_GRID (self->grid), combo, 1, -1, 1, 1);
      g_signal_connect (combo, "changed", G_CALLBACK (instance_changed), self);
      self->instance_combo = combo;
   }

  ai = g_new (hb_ot_var_axis_info_t, n_axes);
  hb_ot_var_get_axis_infos (hb_face, 0, &n_axes, ai);
  for (i = 0; i < n_axes; i++)
    add_axis (self, hb_face, &ai[i], i);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VARIATIONS]);

  g_clear_object (&font);
  g_free (ai);
}

static char *
get_variations (FontVariations *self)
{
  GHashTableIter iter;
  Axis *axis;
  char buf[G_ASCII_DTOSTR_BUF_SIZE];
  const char *sep = "";
  GString *s;

  if (!self->has_variations)
    return g_strdup ("");

  s = g_string_new ("");

  g_hash_table_iter_init (&iter, self->axes);
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

  return g_string_free (s, FALSE);
}

static void
reset (GSimpleAction *action,
       GVariant      *parameter,
       FontVariations   *self)
{
  GHashTableIter iter;
  Axis *axis;

  if (self->instance_combo)
    gtk_combo_box_set_active (GTK_COMBO_BOX (self->instance_combo), 0);

  g_hash_table_iter_init (&iter, self->axes);
  while (g_hash_table_iter_next (&iter, (gpointer *)NULL, (gpointer *)&axis))
    {
      g_signal_handlers_block_by_func (axis->adjustment, unset_instance, self);
      gtk_adjustment_set_value (axis->adjustment, axis->default_value);
      g_signal_handlers_unblock_by_func (axis->adjustment, unset_instance, self);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VARIATIONS]);
  g_simple_action_set_enabled (self->reset_action, FALSE);
}

static void
font_variations_init (FontVariations *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->reset_action = g_simple_action_new ("reset", NULL);
  g_simple_action_set_enabled (self->reset_action, FALSE);
  g_signal_connect (self->reset_action, "activate", G_CALLBACK (reset), self);

  self->instances = g_hash_table_new_full (instance_hash, instance_equal,
                                           NULL, free_instance);
  self->axes = g_hash_table_new_full (axes_hash, axes_equal,
                                      NULL, g_free);
}

static void
font_variations_dispose (GObject *object)
{
  FontVariations *self = FONT_VARIATIONS (object);

  gtk_widget_clear_template (GTK_WIDGET (object), FONT_VARIATIONS_TYPE);

  g_hash_table_unref (self->instances);
  g_hash_table_unref (self->axes);

  G_OBJECT_CLASS (font_variations_parent_class)->dispose (object);
}

static void
font_variations_finalize (GObject *object)
{
  FontVariations *self = FONT_VARIATIONS (object);

  g_clear_pointer (&self->font_desc, pango2_font_description_free);
  g_clear_object (&self->map);

  G_OBJECT_CLASS (font_variations_parent_class)->finalize (object);
}

static void
font_variations_set_property (GObject      *object,
                              unsigned int  prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  FontVariations *self = FONT_VARIATIONS (object);

  switch (prop_id)
    {
    case PROP_FONT_DESC:
      pango2_font_description_free (self->font_desc);
      self->font_desc = pango2_font_description_copy (g_value_get_boxed (value));
      update_variations (self);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
font_variations_get_property (GObject      *object,
                              unsigned int  prop_id,
                              GValue       *value,
                              GParamSpec   *pspec)
{
  FontVariations *self = FONT_VARIATIONS (object);

  switch (prop_id)
    {
    case PROP_VARIATIONS:
      g_value_take_string (value, get_variations (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
font_variations_class_init (FontVariationsClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = font_variations_dispose;
  object_class->finalize = font_variations_finalize;
  object_class->get_property = font_variations_get_property;
  object_class->set_property = font_variations_set_property;

  properties[PROP_FONT_DESC] =
      g_param_spec_boxed ("font-desc", "", "",
                          PANGO2_TYPE_FONT_DESCRIPTION,
                          G_PARAM_WRITABLE);

  properties[PROP_VARIATIONS] =
      g_param_spec_string ("variations", "", "",
                           "",
                           G_PARAM_READABLE);

  g_object_class_install_properties (G_OBJECT_CLASS (class), NUM_PROPERTIES, properties);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (class),
                                               "/org/gtk/fontexplorer/fontvariations.ui");
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontVariations, grid);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontVariations, label);

  gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (class), "fontvariations");
}

FontVariations *
font_variations_new (void)
{
  return g_object_new (FONT_VARIATIONS_TYPE, NULL);
}

GAction *
font_variations_get_reset_action (FontVariations *self)
{
  return G_ACTION (self->reset_action);
}

void
font_variations_set_font_map (FontVariations *self,
                              Pango2FontMap  *map)
{
  g_set_object (&self->map, map);
  update_variations (self);
}
