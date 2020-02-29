#ifndef __GDK_POPUP_PRIVATE_H__
#define __GDK_POPUP_PRIVATE_H__

#include "gdkpopup.h"

G_BEGIN_DECLS


struct _GdkPopupInterface
{
  GTypeInterface g_iface;

  gboolean      (* present)             (GdkPopup       *popup,
                                         int             width,
                                         int             height,
                                         GdkPopupLayout *layout);

  GdkGravity    (* get_surface_anchor)  (GdkPopup       *popup);
  GdkGravity    (* get_rect_anchor)     (GdkPopup       *popup);
  GdkSurface *  (* get_parent)          (GdkPopup       *popup);
  void          (* get_position)        (GdkPopup       *popup,
                                         int            *x,
                                         int            *y);
  gboolean      (* get_autohide)        (GdkPopup       *popup);
};

G_END_DECLS

#endif /* __GDK_POPUP_PRIVATE_H__ */
