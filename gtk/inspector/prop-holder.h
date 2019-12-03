#ifndef __PROP_HOLDER_H__
#define __PROP_HOLDER_H__

#include <gtk/gtk.h>

#define PROP_TYPE_HOLDER (prop_holder_get_type ())

G_DECLARE_FINAL_TYPE (PropHolder, prop_holder, PROP, HOLDER, GObject)

PropHolder * prop_holder_new         (GObject    *object,
                                      const char *property);

GObject    *prop_holder_get_object   (PropHolder *holder);
const char *prop_holder_get_property (PropHolder *holder);

#endif /* __PROP_HOLDER_H__ */
