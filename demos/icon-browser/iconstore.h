#ifndef __ICON_STORE_H
#define __ICON_STORE_H

#include <gtk/gtk.h>


#define ICON_STORE_TYPE (icon_store_get_type ())
#define ICON_STORE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), ICON_STORE_TYPE, IconStore))


typedef struct _IconStore       IconStore;
typedef struct _IconStoreClass  IconStoreClass;

enum {
  ICON_STORE_NAME_COLUMN,
  ICON_STORE_SYMBOLIC_NAME_COLUMN,
  ICON_STORE_DESCRIPTION_COLUMN,
  ICON_STORE_CONTEXT_COLUMN
};

GType   icon_store_get_type     (void);

void    icon_store_set_text_column (IconStore *store,
                                    gint       column);

#endif /* __ICON_STORE_H */
