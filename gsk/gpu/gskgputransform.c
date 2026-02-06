#include "config.h"

#include "gskgputransformprivate.h"

#include "gskrectprivate.h"

void
gsk_gpu_transform_init (GskGpuTransform        *self,
                        GdkDihedral             dihedral,
                        const graphene_size_t  *scale,
                        const graphene_point_t *offset)
{
  self->dihedral = dihedral;
  self->scale = *scale;
  self->offset = *offset;
}

/**
 * gsk_gpu_transform_transform:
 * @self: the GPU transform
 * @transform: the transform to apply
 *
 * Tries to apply the transform to. If the transform is
 * too complex, this will fail.
 *
 * On success, self will have been modified as if the two
 * transforms had been applied in order.
 *
 * Returns: true if applying the transform succeeded
 **/
gboolean
gsk_gpu_transform_transform (GskGpuTransform *self,
                             GskTransform    *transform)
{
  float dx, dy, scale_x, scale_y, xx, xy, yx, yy;
  GdkDihedral dihedral, inverted;

  switch (gsk_transform_get_fine_category (transform))
  {
    case GSK_FINE_TRANSFORM_CATEGORY_IDENTITY:
      return TRUE;

    case GSK_FINE_TRANSFORM_CATEGORY_2D_TRANSLATE:
      gsk_transform_to_translate (transform, &dx, &dy);
      self->offset.x += dx;
      self->offset.y += dy;
      return TRUE;

    case GSK_FINE_TRANSFORM_CATEGORY_2D_AFFINE:
      gsk_transform_to_affine (transform, &scale_x, &scale_y, &dx, &dy);
      self->offset.x = (self->offset.x + dx) / scale_x;
      self->offset.y = (self->offset.y + dy) / scale_y;
      self->scale.width *= scale_x;
      self->scale.height *= scale_y;
      return TRUE;

    case GSK_FINE_TRANSFORM_CATEGORY_2D_NEGATIVE_AFFINE:
    case GSK_FINE_TRANSFORM_CATEGORY_2D_DIHEDRAL:
      gsk_transform_to_dihedral (transform, &dihedral, &scale_x, &scale_y, &dx, &dy);
      inverted = gdk_dihedral_invert (dihedral);
      gdk_dihedral_get_mat2 (inverted, &xx, &xy, &yx, &yy);
      self->offset.x = (self->offset.x + dx) / scale_x;
      self->offset.y = (self->offset.y + dy) / scale_y;
      self->offset = GRAPHENE_POINT_INIT (xx * self->offset.x + xy * self->offset.y,
                                          yx * self->offset.x + yy * self->offset.y);
      self->scale = GRAPHENE_SIZE_INIT (fabs (scale_x * (self->scale.width * xx + self->scale.height * yx)),
                                        fabs (scale_y * (self->scale.width * xy + self->scale.height * yy)));
      self->dihedral = gdk_dihedral_combine (self->dihedral, dihedral);
      return TRUE;

    case GSK_FINE_TRANSFORM_CATEGORY_2D:
    case GSK_FINE_TRANSFORM_CATEGORY_3D:
    case GSK_FINE_TRANSFORM_CATEGORY_ANY:
    case GSK_FINE_TRANSFORM_CATEGORY_UNKNOWN:
      return FALSE;

    default:
      g_assert_not_reached ();
      return FALSE;
  }
}

void
gsk_gpu_transform_transform_rect (const GskGpuTransform *self,
                                  const graphene_rect_t *rect,
                                  graphene_rect_t       *result)
{
  graphene_rect_t tmp;

  tmp = GRAPHENE_RECT_INIT ((rect->origin.x + self->offset.x) * self->scale.width,
                            (rect->origin.y + self->offset.y) * self->scale.height,
                            rect->size.width * self->scale.width,
                            rect->size.height * self->scale.height);
  gsk_rect_dihedral (&tmp, gdk_dihedral_invert (self->dihedral), result);
}

void
gsk_gpu_transform_invert_rect (const GskGpuTransform *self,
                               const graphene_rect_t *rect,
                               graphene_rect_t       *result)
{
  graphene_rect_t tmp;

  gsk_rect_dihedral (rect, self->dihedral, &tmp);
  *result = GRAPHENE_RECT_INIT (tmp.origin.x / self->scale.width - self->offset.x,
                                tmp.origin.y / self->scale.height - self->offset.y,
                                tmp.size.width / self->scale.width,
                                tmp.size.height / self->scale.height);
}

static void
string_append_double (GString *string,
                      double   d)
{
  char buf[G_ASCII_DTOSTR_BUF_SIZE];

  g_ascii_formatd (buf, G_ASCII_DTOSTR_BUF_SIZE, "%g", d);
  g_string_append (string, buf);
}

void
gsk_gpu_transform_print (const GskGpuTransform *self,
                         GString               *str)
{
  int rotate = self->dihedral & 3;
  int flip = self->dihedral & 4;
  gboolean some = FALSE;

  if (rotate)
    {
      g_string_append (str, "rotate(90)");
      some = TRUE;
    }
  if (self->scale.width != 1.0 || self->scale.height != 1.0 || flip)
    {
      if (some)
        g_string_append_c (str, ' ');
      g_string_append (str, "scale(");
      string_append_double (str, ((flip && !rotate) ? -1.0 : 1.0) * self->scale.width);
      g_string_append (str, ", ");
      string_append_double (str, ((flip && rotate) ? -1.0 : 1.0) * self->scale.height);
      g_string_append (str, ")");
      some = TRUE;
    }

  if (self->offset.x != 0 || self->offset.y != 0)
    {
      if (some)
        g_string_append_c (str, ' ');
      g_string_append (str, "translate(");
      string_append_double (str, self->offset.x);
      g_string_append (str, ", ");
      string_append_double (str, self->offset.y);
      g_string_append (str, ")");
      some = TRUE;
    }
  if (!some)
    g_string_append (str, "none");
}

char *
gsk_gpu_transform_to_string (const GskGpuTransform *self)
{
  GString *str = g_string_new (NULL);
  gsk_gpu_transform_print (self, str);
  return g_string_free (str, FALSE);
}

