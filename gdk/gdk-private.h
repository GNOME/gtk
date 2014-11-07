#ifndef __GDK__PRIVATE_H__
#define __GDK__PRIVATE_H__

#include <gdk/gdk.h>

#define GDK_PRIVATE_CALL(symbol)        (gdk__private__ ()->symbol)

GdkDisplay *    gdk_display_open_default        (void);

gboolean        gdk_device_grab_info            (GdkDisplay  *display,
                                                 GdkDevice   *device,
                                                 GdkWindow  **grab_window,
                                                 gboolean    *owner_events);

void            gdk_add_option_entries          (GOptionGroup *group);

void            gdk_pre_parse                   (void);

typedef struct {
  /* add all private functions here, initialize them in gdk-private.c */
  gboolean (* gdk_device_grab_info) (GdkDisplay  *display,
                                     GdkDevice   *device,
                                     GdkWindow  **grab_window,
                                     gboolean    *owner_events);

  GdkDisplay *(* gdk_display_open_default) (void);

  void (* gdk_add_option_entries) (GOptionGroup *group);
  void (* gdk_pre_parse) (void);
} GdkPrivateVTable;

GDK_AVAILABLE_IN_ALL
GdkPrivateVTable *      gdk__private__  (void);

#endif /* __GDK__PRIVATE_H__ */
