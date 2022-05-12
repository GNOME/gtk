#ifndef __GDK_COLOR_SPACE_PRIVATE_H__
#define __GDK_COLOR_SPACE_PRIVATE_H__

#include "gdkcolorspace.h"
#include <lcms2.h>

G_BEGIN_DECLS

struct _GdkColorSpace
{
  GObject parent_instance;
};

struct _GdkColorSpaceClass
{
  GObjectClass parent_class;

  gboolean              (* supports_format)                     (GdkColorSpace        *self,
                                                                 GdkMemoryFormat       format);
  GBytes *              (* save_to_icc_profile)                 (GdkColorSpace        *self,
                                                                 GError              **error);

  int                   (* get_n_components)                    (GdkColorSpace        *self);

  gboolean              (* equal)                               (GdkColorSpace        *profile1,
                                                                 GdkColorSpace        *profile2);
};


GdkColorSpace *          gdk_color_space_get_srgb_linear            (void) G_GNUC_CONST;

cmsHTRANSFORM *          gdk_color_space_lookup_transform           (GdkColorSpace          *source,
                                                                     guint                   source_type,
                                                                     GdkColorSpace          *dest,
                                                                     guint                   dest_type);

G_END_DECLS

#endif /* __GDK_COLOR_SPACE_PRIVATE_H__ */
