#include <gtk/gtk.h>

#include "filter_paintable.h"

struct _GtkFilterPaintable
{
  GObject parent_instance;

  GdkTexture *texture;

  float brightness;
  float contrast;
  float saturation;
  float sepia;
  float invert;
  float rotate;
  float blur;
  GskComponentTransfer *red_transfer;
  GskComponentTransfer *green_transfer;
  GskComponentTransfer *blue_transfer;
  GskComponentTransfer *alpha_transfer;
};

struct _GtkFilterPaintableClass
{
  GObjectClass parent_class;
};

enum {
  PROP_TEXTURE = 1,
  PROP_BRIGHTNESS,
  PROP_CONTRAST,
  PROP_SATURATION,
  PROP_SEPIA,
  PROP_INVERT,
  PROP_ROTATE,
  PROP_BLUR,
  PROP_RED_TRANSFER,
  PROP_GREEN_TRANSFER,
  PROP_BLUE_TRANSFER,
  PROP_ALPHA_TRANSFER,
  NUM_PROPERTIES
};

static GParamSpec *props[NUM_PROPERTIES] = { 0, };

#define R 0.2126
#define G 0.7152
#define B 0.0722

static void
gtk_filter_paintable_snapshot (GdkPaintable *paintable,
                               GdkSnapshot  *snapshot,
                               double        width,
                               double        height)
{
  GtkFilterPaintable *self = GTK_FILTER_PAINTABLE (paintable);
  GskComponentTransfer *identity;

  identity = gsk_component_transfer_new_identity ();

  if (self->brightness != 1)
    {
      graphene_matrix_t matrix;

      graphene_matrix_init_scale (&matrix, self->brightness, self->brightness, self->brightness);
      gtk_snapshot_push_color_matrix (snapshot, &matrix, graphene_vec4_zero ());
    }

  if (self->contrast != 1)
    {
      graphene_matrix_t matrix;
      graphene_vec4_t offset;

      graphene_matrix_init_scale (&matrix, self->contrast, self->contrast, self->contrast);

      graphene_vec4_init (&offset, 0.5 - 0.5 * self->contrast, 0.5 - 0.5 * self->contrast, 0.5 - 0.5 * self->contrast, 0.0);

      gtk_snapshot_push_color_matrix (snapshot, &matrix, &offset);
    }

  if (self->saturation != 1)
    {
      float value = self->saturation;
      graphene_matrix_t matrix;

      graphene_matrix_init_from_float (&matrix, (float[16]) {
                                       R + (1.0 - R) * value, R - R * value, R - R * value, 0.0,
                                       G - G * value, G + (1.0 - G) * value, G - G * value, 0.0,
                                       B - B * value, B - B * value, B + (1.0 - B) * value, 0.0,
                                       0.0, 0.0, 0.0, 1.0
                                       });
      gtk_snapshot_push_color_matrix (snapshot, &matrix, graphene_vec4_zero ());
    }

  if (self->sepia != 0)
    {
      float value = self->sepia;
      graphene_matrix_t matrix;

      graphene_matrix_init_from_float (&matrix, (float[16]) {
                                       1.0 - 0.607 * value, 0.349 * value, 0.272 * value, 0.0,
                                       0.769 * value, 1.0 - 0.314 * value, 0.534 * value, 0.0,
                                       0.189 * value, 0.168 * value, 1.0 - 0.869 * value, 0.0,
                                       0.0, 0.0, 0.0, 1.0
                                       });
      gtk_snapshot_push_color_matrix (snapshot, &matrix, graphene_vec4_zero ());
    }

  if (self->invert != 0)
    {
      graphene_matrix_t matrix;
      graphene_vec4_t offset;

      graphene_matrix_init_scale (&matrix, 1 - 2 * self->invert, 1 - 2 * self->invert, 1 - 2 * self->invert);

      graphene_vec4_init (&offset, self->invert, self->invert, self->invert, 0);

      gtk_snapshot_push_color_matrix (snapshot, &matrix, &offset);
    }

  if (self->rotate != 0)
    {
      graphene_matrix_t matrix;
      float c = cosf (self->rotate * G_PI / 180);
      float s = sinf (self->rotate * G_PI / 180);

      graphene_matrix_init_from_float (&matrix, (float[16]) {
                                       0.213 + 0.787 * c - 0.213 * s,
                                       0.213 - 0.213 * c + 0.143 * s,
                                       0.213 - 0.213 * c - 0.787 * s,
                                       0,
                                       0.715 - 0.715 * c - 0.715 * s,
                                       0.715 + 0.285 * c + 0.140 * s,
                                       0.715 - 0.715 * c + 0.715 * s,
                                       0,
                                       0.072 - 0.072 * c + 0.928 * s,
                                       0.072 - 0.072 * c - 0.283 * s,
                                       0.072 + 0.928 * c + 0.072 * s,
                                       0,
                                       0, 0, 0, 1
                                       });

      gtk_snapshot_push_color_matrix (snapshot, &matrix, graphene_vec4_zero ());
    }

  if (!gsk_component_transfer_equal (self->red_transfer, identity) ||
      !gsk_component_transfer_equal (self->green_transfer, identity) ||
      !gsk_component_transfer_equal (self->blue_transfer, identity) ||
      !gsk_component_transfer_equal (self->alpha_transfer, identity))
    gtk_snapshot_push_component_transfer (snapshot,
                                          self->red_transfer,
                                          self->green_transfer,
                                          self->blue_transfer,
                                          self->alpha_transfer);

  if (self->blur != 0)
    gtk_snapshot_push_blur (snapshot, self->blur);

  gtk_snapshot_append_texture (snapshot, self->texture,
                               &GRAPHENE_RECT_INIT (0, 0,
                                                    gdk_texture_get_width (self->texture),
                                                    gdk_texture_get_height (self->texture)));

  if (self->blur != 0)
    gtk_snapshot_pop (snapshot);

  if (!gsk_component_transfer_equal (self->red_transfer, identity) ||
      !gsk_component_transfer_equal (self->green_transfer, identity) ||
      !gsk_component_transfer_equal (self->blue_transfer, identity) ||
      !gsk_component_transfer_equal (self->alpha_transfer, identity))
    gtk_snapshot_pop (snapshot);

  if (self->saturation != 1)
    gtk_snapshot_pop (snapshot);

  if (self->sepia != 0)
    gtk_snapshot_pop (snapshot);

  if (self->contrast != 1)
    gtk_snapshot_pop (snapshot);

  if (self->brightness != 1)
    gtk_snapshot_pop (snapshot);

  if (self->invert != 0)
    gtk_snapshot_pop (snapshot);

  if (self->rotate != 0)
    gtk_snapshot_pop (snapshot);

  gsk_component_transfer_free (identity);
}

static int
gtk_filter_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  GtkFilterPaintable *self = GTK_FILTER_PAINTABLE (paintable);

  return gdk_texture_get_width (self->texture);
}

static int
gtk_filter_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  GtkFilterPaintable *self = GTK_FILTER_PAINTABLE (paintable);

  return gdk_texture_get_height (self->texture);
}

static void
gtk_filter_paintable_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = gtk_filter_paintable_snapshot;
  iface->get_intrinsic_width = gtk_filter_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = gtk_filter_paintable_get_intrinsic_height;
}

G_DEFINE_TYPE_WITH_CODE (GtkFilterPaintable, gtk_filter_paintable, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                gtk_filter_paintable_paintable_init))

static void
gtk_filter_paintable_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GtkFilterPaintable *self = GTK_FILTER_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_TEXTURE:
      g_value_set_object (value, self->texture);
      break;
    case PROP_BRIGHTNESS:
      g_value_set_float (value, self->brightness);
      break;
    case PROP_CONTRAST:
      g_value_set_float (value, self->contrast);
      break;
    case PROP_SATURATION:
      g_value_set_float (value, self->saturation);
      break;
    case PROP_SEPIA:
      g_value_set_float (value, self->sepia);
      break;
    case PROP_INVERT:
      g_value_set_float (value, self->invert);
      break;
    case PROP_ROTATE:
      g_value_set_float (value, self->rotate);
      break;
    case PROP_BLUR:
      g_value_set_float (value, self->blur);
      break;
    case PROP_RED_TRANSFER:
      g_value_set_boxed (value, self->red_transfer);
      break;
    case PROP_GREEN_TRANSFER:
      g_value_set_boxed (value, self->green_transfer);
      break;
    case PROP_BLUE_TRANSFER:
      g_value_set_boxed (value, self->blue_transfer);
      break;
    case PROP_ALPHA_TRANSFER:
      g_value_set_boxed (value, self->alpha_transfer);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_filter_paintable_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GtkFilterPaintable *self = GTK_FILTER_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_TEXTURE:
      g_set_object (&self->texture, g_value_get_object (value));
      break;
    case PROP_BRIGHTNESS:
      self->brightness = g_value_get_float (value);
      break;
    case PROP_CONTRAST:
      self->contrast = g_value_get_float (value);
      break;
    case PROP_SATURATION:
      self->saturation = g_value_get_float (value);
      break;
    case PROP_SEPIA:
      self->sepia = g_value_get_float (value);
      break;
    case PROP_INVERT:
      self->invert = g_value_get_float (value);
      break;
    case PROP_ROTATE:
      self->rotate = g_value_get_float (value);
      break;
    case PROP_BLUR:
      self->blur = g_value_get_float (value);
      break;
    case PROP_RED_TRANSFER:
      gsk_component_transfer_free (self->red_transfer);
      self->red_transfer = gsk_component_transfer_copy (g_value_get_boxed (value));
      break;
    case PROP_GREEN_TRANSFER:
      gsk_component_transfer_free (self->green_transfer);
      self->green_transfer = gsk_component_transfer_copy (g_value_get_boxed (value));
      break;
    case PROP_BLUE_TRANSFER:
      gsk_component_transfer_free (self->blue_transfer);
      self->blue_transfer = gsk_component_transfer_copy (g_value_get_boxed (value));
      break;
    case PROP_ALPHA_TRANSFER:
      gsk_component_transfer_free (self->alpha_transfer);
      self->alpha_transfer = gsk_component_transfer_copy (g_value_get_boxed (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
}

static void
gtk_filter_paintable_finalize (GObject *object)
{
  GtkFilterPaintable *self = GTK_FILTER_PAINTABLE (object);

  g_object_unref (self->texture);
  gsk_component_transfer_free (self->red_transfer);
  gsk_component_transfer_free (self->green_transfer);
  gsk_component_transfer_free (self->blue_transfer);
  gsk_component_transfer_free (self->alpha_transfer);

  G_OBJECT_CLASS (gtk_filter_paintable_parent_class)->finalize (object);
}

static void
gtk_filter_paintable_class_init (GtkFilterPaintableClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_filter_paintable_finalize;
  object_class->set_property = gtk_filter_paintable_set_property;
  object_class->get_property = gtk_filter_paintable_get_property;

  props[PROP_TEXTURE]    = g_param_spec_object ("texture", NULL, NULL,
                                                GDK_TYPE_TEXTURE,
                                                G_PARAM_READWRITE);
  props[PROP_BRIGHTNESS] = g_param_spec_float ("brightness", NULL, NULL,
                                               0, 2, 1,
                                               G_PARAM_READWRITE);
  props[PROP_CONTRAST]   = g_param_spec_float ("contrast", NULL, NULL,
                                               0, 2, 1,
                                               G_PARAM_READWRITE);
  props[PROP_SATURATION] = g_param_spec_float ("saturation", NULL, NULL,
                                               0, 2, 1,
                                               G_PARAM_READWRITE);
  props[PROP_SEPIA]      = g_param_spec_float ("sepia", NULL, NULL,
                                               0, 1, 1,
                                               G_PARAM_READWRITE);
  props[PROP_INVERT]     = g_param_spec_float ("invert", NULL, NULL,
                                               0, 1, 0,
                                               G_PARAM_READWRITE);
  props[PROP_ROTATE]     = g_param_spec_float ("rotate", NULL, NULL,
                                               0, 360, 0,
                                               G_PARAM_READWRITE);
  props[PROP_BLUR]       = g_param_spec_float ("blur", NULL, NULL,
                                               0, 50, 0,
                                               G_PARAM_READWRITE);
  props[PROP_RED_TRANSFER] = g_param_spec_boxed ("red-transfer", NULL, NULL,
                                                 GSK_TYPE_COMPONENT_TRANSFER,
                                                 G_PARAM_READWRITE);
  props[PROP_GREEN_TRANSFER] = g_param_spec_boxed ("green-transfer", NULL, NULL,
                                                   GSK_TYPE_COMPONENT_TRANSFER,
                                                   G_PARAM_READWRITE);
  props[PROP_BLUE_TRANSFER] = g_param_spec_boxed ("blue-transfer", NULL, NULL,
                                                  GSK_TYPE_COMPONENT_TRANSFER,
                                                  G_PARAM_READWRITE);
  props[PROP_ALPHA_TRANSFER] = g_param_spec_boxed ("alpha-transfer", NULL, NULL,
                                                   GSK_TYPE_COMPONENT_TRANSFER,
                                                   G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, props);
}

static void
gtk_filter_paintable_init (GtkFilterPaintable *image)
{
  image->texture = gdk_texture_new_from_resource ("/image_filtering/portland-rose.jpg");
  image->brightness = 1;
  image->contrast = 1;
  image->saturation = 1;
  image->invert = 0;
  image->rotate = 0;

  image->red_transfer = gsk_component_transfer_new_identity ();
  image->green_transfer = gsk_component_transfer_new_identity ();
  image->blue_transfer = gsk_component_transfer_new_identity ();
  image->alpha_transfer = gsk_component_transfer_new_identity ();
}
