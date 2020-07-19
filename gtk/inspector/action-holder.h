
#ifndef __ACTION_HOLDER_H__
#define __ACTION_HOLDER_H__

#include <gtk/gtk.h>

#define ACTION_TYPE_HOLDER (action_holder_get_type ())

G_DECLARE_FINAL_TYPE (ActionHolder, action_holder, ACTION, HOLDER, GObject)

ActionHolder * action_holder_new     (GObject    *owner,
                                      const char *name);

GObject      *action_holder_get_owner (ActionHolder *holder);
const char   *action_holder_get_name  (ActionHolder *holder);

#endif /* __ACTION_HOLDER_H__ */
