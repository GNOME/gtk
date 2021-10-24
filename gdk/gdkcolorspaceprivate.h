#ifndef __GDK_COLOR_SPACE_PRIVATE_H__
#define __GDK_COLOR_SPACE_PRIVATE_H__

#include "gdkcolorspace.h"

G_BEGIN_DECLS

struct _GdkColorSpace
{
  GObject parent_instance;

  gsize n_components;
};

struct _GdkColorSpaceClass
{
  GObjectClass parent_class;

  gboolean              (* supports_format)                     (GdkColorSpace        *self,
                                                                 GdkMemoryFormat       format);
  GBytes *              (* save_to_icc_profile)                 (GdkColorSpace        *self,
                                                                 GError              **error);

  void                  (* convert_color)                       (GdkColorSpace        *self,
                                                                 float                *components,
                                                                 const GdkColor       *source);
};

static inline gsize
gdk_color_space_get_n_components (GdkColorSpace *self)
{
  return self->n_components;
}

G_END_DECLS

#endif /* __GDK_COLOR_SPACE_PRIVATE_H__ */
