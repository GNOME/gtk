#ifndef __GDK_COLOR_PROFILE_PRIVATE_H__
#define __GDK_COLOR_PROFILE_PRIVATE_H__

#include "gdkcolorprofile.h"
#include "gdkmemorytexture.h"

#include <lcms2.h>

G_BEGIN_DECLS

struct _GdkColorProfile
{
  GObject parent_instance;
};

struct _GdkColorProfileClass
{
  GObjectClass parent_class;

  gboolean (* is_linear)        (GdkColorProfile *profile);
  gsize    (* get_n_components) (GdkColorProfile *profile);
  gboolean (* equal)            (gconstpointer    profile1,
                                 gconstpointer    profile2);
};

GdkColorProfile *            gdk_color_profile_get_srgb_linear            (void) G_GNUC_CONST;
GdkColorProfile *            gdk_color_profile_get_hsl                    (void) G_GNUC_CONST;
GdkColorProfile *            gdk_color_profile_get_hwb                    (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __GDK_COLOR_PROFILE_PRIVATE_H__ */
