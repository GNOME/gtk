#include "glyphpicker.h"

#include <gtk/gtk.h>
#include <hb-ot.h>
#include <hb-gobject.h>

static void glyph_picker_set_face  (GlyphPicker  *self,
                                    hb_face_t    *face);
static void glyph_picker_set_glyph (GlyphPicker  *self,
                                    unsigned int  glyph);

enum {
  PROP_FACE = 1,
  PROP_GLYPH,
  PROP_GLYPH_NAME,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

struct _GlyphPicker
{
  GtkWidget parent;

  GtkLabel *label;
  GtkSpinButton *spin;
  GtkLabel *name;

  hb_face_t *face;
  hb_font_t *font;
};

struct _GlyphPickerClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (GlyphPicker, glyph_picker, GTK_TYPE_WIDGET);

static unsigned int
get_glyph (GlyphPicker *self)
{
  return (unsigned int) gtk_spin_button_get_value_as_int (self->spin);
}

static char *
get_glyph_name (GlyphPicker *self)
{
  char name[64];

  hb_font_get_glyph_name (self->font, get_glyph (self), name, sizeof (name));

  return g_strdup (name);
}

static void
value_changed (GlyphPicker *self)
{
  char *name;

  name = get_glyph_name (self);
  gtk_label_set_label (self->name, name);
  g_free (name);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_GLYPH]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_GLYPH_NAME]);
}

static void
glyph_picker_init (GlyphPicker *self)
{
  self->label = GTK_LABEL (gtk_label_new ("Glyph"));
  gtk_widget_set_parent (GTK_WIDGET (self->label), GTK_WIDGET (self));
  self->spin = GTK_SPIN_BUTTON (gtk_spin_button_new_with_range (0, 0, 1));
  gtk_widget_set_parent (GTK_WIDGET (self->spin), GTK_WIDGET (self));
  self->name = GTK_LABEL (gtk_label_new (""));
  gtk_widget_set_parent (GTK_WIDGET (self->name), GTK_WIDGET (self));

  g_signal_connect_swapped (self->spin, "value-changed", G_CALLBACK (value_changed), self);
}

static void
glyph_picker_dispose (GObject *object)
{
  GlyphPicker *self = GLYPH_PICKER (object);

  g_clear_pointer ((GtkWidget **)&self->spin, gtk_widget_unparent);
  g_clear_pointer ((GtkWidget **)&self->label, gtk_widget_unparent);

  G_OBJECT_CLASS (glyph_picker_parent_class)->dispose (object);
}

static void
glyph_picker_finalize (GObject *object)
{
  GlyphPicker *self = GLYPH_PICKER (object);

  g_clear_pointer (&self->face, hb_face_destroy);

  G_OBJECT_CLASS (glyph_picker_parent_class)->finalize (object);
}

static void
update_bounds (GlyphPicker *self)
{
  unsigned int glyph_count = 0;
  GtkAdjustment *adjustment;

  if (self->face)
    glyph_count = hb_face_get_glyph_count (self->face);

  adjustment = gtk_spin_button_get_adjustment (self->spin);
  gtk_adjustment_set_upper (adjustment, glyph_count - 1);
}

static void
update_font (GlyphPicker *self)
{
  g_clear_pointer (&self->font, hb_font_destroy);
  if (self->face)
    self->font = hb_font_create (self->face);
  if (self->font)
    value_changed (self);
}

static void
update_glyph (GlyphPicker *self)
{
  hb_codepoint_t glyph;

  if (hb_font_get_glyph_from_name (self->font, "icon0", -1, &glyph) ||
      hb_font_get_glyph_from_name (self->font, "A", -1, &glyph))
    {
      glyph_picker_set_glyph (self, glyph);
    }
}

static void
glyph_picker_set_property (GObject      *object,
                           unsigned int  prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GlyphPicker *self = GLYPH_PICKER (object);

  switch (prop_id)
    {
    case PROP_FACE:
      glyph_picker_set_face (self, (hb_face_t *) g_value_get_boxed (value));
      break;

    case PROP_GLYPH:
      glyph_picker_set_glyph (self, g_value_get_uint (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
glyph_picker_get_property (GObject      *object,
                           unsigned int  prop_id,
                           GValue       *value,
                           GParamSpec   *pspec)
{
  GlyphPicker *self = GLYPH_PICKER (object);

  switch (prop_id)
    {
    case PROP_FACE:
      g_value_set_boxed (value, self->face);
      break;

    case PROP_GLYPH:
      g_value_set_uint (value, get_glyph (self));
      break;

    case PROP_GLYPH_NAME:
      g_value_take_string (value, get_glyph_name (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
glyph_picker_class_init (GlyphPickerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = glyph_picker_dispose;
  object_class->finalize = glyph_picker_finalize;
  object_class->get_property = glyph_picker_get_property;
  object_class->set_property = glyph_picker_set_property;

  properties[PROP_FACE] =
      g_param_spec_boxed ("face", "", "",
                          HB_GOBJECT_TYPE_FACE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_GLYPH] =
      g_param_spec_uint ("glyph", "", "",
                         0, G_MAXUINT, 0,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_GLYPH_NAME] =
      g_param_spec_string ("glyph-name", "", "",
                           NULL,
                           G_PARAM_READABLE);

  g_object_class_install_properties (G_OBJECT_CLASS (class), NUM_PROPERTIES, properties);

  gtk_widget_class_set_layout_manager_type (GTK_WIDGET_CLASS (class), GTK_TYPE_BOX_LAYOUT);
  gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (class), "glyphpicker");
}

static void
glyph_picker_set_face (GlyphPicker *self,
                       hb_face_t   *face)
{
  if (self->face == face)
    return;

  if (self->face)
    hb_face_destroy (self->face);
  self->face = face;
  if (self->face)
    hb_face_reference (self->face);

  update_bounds (self);
  update_font (self);
  update_glyph (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FACE]);
}

static void
glyph_picker_set_glyph (GlyphPicker  *self,
                        unsigned int  glyph)
{
  if (get_glyph (self) == glyph)
    return;

  gtk_spin_button_set_value (self->spin, glyph);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_GLYPH]);
}
