#pragma once

#include <glib-object.h>
#include "gtksvgprivate.h"

G_BEGIN_DECLS

typedef struct _SvgComputeContext SvgComputeContext;
struct _SvgComputeContext
{
  GtkSvg *svg;
  const graphene_rect_t *viewport;
  Shape *parent; /* Can be different from the actual parent, for <use> */
  int64_t current_time;
  const GdkRGBA *colors;
  size_t n_colors;
  GdkColorState *interpolation;
};

typedef struct _SvgValueClass SvgValueClass;
struct _SvgValueClass
{
  const char *name;

  void       (* free)        (SvgValue              *value);
  gboolean   (* equal)       (const SvgValue        *value0,
                              const SvgValue        *value1);
  SvgValue * (* interpolate) (const SvgValue        *value0,
                              const SvgValue        *value1,
                              SvgComputeContext     *context,
                              double                 t);
  SvgValue * (* accumulate)  (const SvgValue        *value0,
                              const SvgValue        *value1,
                              SvgComputeContext     *context,
                              int                    n);
  void       (* print)       (const SvgValue        *value,
                              GString               *string);
  double     (* distance)    (const SvgValue        *value0,
                              const SvgValue        *value1);
  SvgValue * (* resolve)     (const SvgValue        *value,
                              ShapeAttr              attr,
                              unsigned int           idx,
                              Shape                 *shape,
                              SvgComputeContext     *context);
};

struct _SvgValue
{
  const SvgValueClass *class;
  int ref_count;
};

GType      svg_value_get_type     (void) G_GNUC_CONST;

SvgValue * svg_value_ref          (SvgValue       *value);
void       svg_value_unref        (SvgValue       *value);

gboolean   svg_value_equal        (const SvgValue *self,
                                   const SvgValue *other);
char *     svg_value_to_string    (const SvgValue *self);

SvgValue * svg_value_interpolate (const SvgValue    *value0,
                                  const SvgValue    *value1,
                                  SvgComputeContext *context,
                                  double             t);
SvgValue * svg_value_accumulate  (const SvgValue    *value0,
                                  const SvgValue    *value1,
                                  SvgComputeContext *context,
                                  int                n);
void       svg_value_print       (const SvgValue    *value,
                                  GString           *string);
double     svg_value_distance    (const SvgValue    *value0,
                                  const SvgValue    *value1);
SvgValue * svg_value_resolve     (const SvgValue    *value,
                                  ShapeAttr          attr,
                                  unsigned int       idx,
                                  Shape             *shape,
                                  SvgComputeContext *context);


SvgValue * svg_value_alloc            (const SvgValueClass *class,
                                       size_t                size);

gboolean   svg_value_is_immortal      (const SvgValue      *value);

void       svg_value_default_free     (SvgValue            *value);

double     svg_value_default_distance (const SvgValue      *value0,
                                       const SvgValue      *value1);

SvgValue * svg_value_default_resolve  (const SvgValue      *value,
                                       ShapeAttr            attr,
                                       unsigned int         idx,
                                       Shape               *shape,
                                       SvgComputeContext   *context);

G_END_DECLS
