#ifndef __GDK_COLOR_PROFILE_PRIVATE_H__
#define __GDK_COLOR_PROFILE_PRIVATE_H__

#include "gdkcolorprofile.h"
#include "gdkmemorytexture.h"

#include <lcms2.h>

G_BEGIN_DECLS

GdkColorProfile *            gdk_color_profile_get_srgb_linear            (void) G_GNUC_CONST;

GdkColorProfile *            gdk_color_profile_new_from_lcms_profile      (cmsHPROFILE           lcms_profile,
                                                                           GError              **error);

cmsHPROFILE *                gdk_color_profile_get_lcms_profile           (GdkColorProfile      *self);

gboolean                     gdk_color_profile_supports_memory_format     (GdkColorProfile      *profile,
                                                                           GdkMemoryFormat       format);

G_END_DECLS

#endif /* __GDK_COLOR_PROFILE_PRIVATE_H__ */
