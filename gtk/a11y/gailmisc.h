/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001 Sun Microsystems Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GAIL_MISC_H__
#define __GAIL_MISC_H__

#include <atk/atk.h>

G_BEGIN_DECLS

#define GAIL_TYPE_MISC                           (_gail_misc_get_type ())
#define GAIL_MISC(obj)                           (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAIL_TYPE_MISC, GailMisc))
#define GAIL_MISC_CLASS(klass)                   (G_TYPE_CHECK_CLASS_CAST ((klass), GAIL_TYPE_MISC, GailMiscClass))
#define GAIL_IS_MISC(obj)                        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAIL_TYPE_MISC))
#define GAIL_IS_MISC_CLASS(klass)                (G_TYPE_CHECK_CLASS_TYPE ((klass), GAIL_TYPE_MISC))
#define GAIL_MISC_GET_CLASS(obj)                 (G_TYPE_INSTANCE_GET_CLASS ((obj), GAIL_TYPE_MISC, GailMiscClass))

typedef struct _GailMisc      GailMisc;
typedef struct _GailMiscClass GailMiscClass;

struct _GailMisc
{
  AtkMisc parent;
};

struct _GailMiscClass
{
  AtkMiscClass parent_class;
};

GType _gail_misc_get_type (void);

G_END_DECLS

#endif /* __GAIL_MISC_H__ */
