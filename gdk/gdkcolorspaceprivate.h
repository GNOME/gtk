#ifndef __GDK_COLOR_SPACE_PRIVATE_H__
#define __GDK_COLOR_SPACE_PRIVATE_H__

#include "gdkcolorspace.h"

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
};


G_END_DECLS

#endif /* __GDK_COLOR_SPACE_PRIVATE_H__ */
