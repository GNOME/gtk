#ifndef __GDK_ICC_PROFILE_PRIVATE_H__
#define __GDK_ICC_PROFILE_PRIVATE_H__

#include "gdkiccprofile.h"
#include "gdkmemorytexture.h"

#include <lcms2.h>

G_BEGIN_DECLS


GdkICCProfile *              gdk_icc_profile_new_from_lcms_profile      (cmsHPROFILE         lcms_profile,
                                                                         GError            **error);

cmsHPROFILE *                gdk_icc_profile_get_lcms_profile           (GdkICCProfile      *self);

cmsHTRANSFORM *              gdk_icc_profile_lookup_transform           (GdkICCProfile      *source,
                                                                         guint               source_type,
                                                                         GdkICCProfile      *dest,
                                                                         guint               dest_type);

G_END_DECLS

#endif /* __GDK_COLOR_PROFILE_PRIVATE_H__ */
