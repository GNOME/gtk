#ifndef __GDK_DISPLAY_X11__
#define __GDK_DISPLAY_X11__

#include "config.h"
#include "gdkdisplay.h"
#include <X11/X.h>
#include <X11/Xlib.h>
#include "gdkkeysyms.h"
#include "gdkwindow.h"
#include "xsettings-client.h"
#ifdef HAVE_XKB
#include <X11/XKBlib.h>
#endif
#ifdef __cplusplus
extern "C"
{
#endif				/* __cplusplus */

  typedef struct _GdkDisplayImplX11 GdkDisplayImplX11;
  typedef struct _GdkDisplayImplX11Class GdkDisplayImplX11Class;


#define GDK_TYPE_DISPLAY_IMPL_X11              (gdk_x11_display_impl_get_type())
#define GDK_DISPLAY_IMPL_X11(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_DISPLAY_IMPL_X11, GdkDisplayImplX11))
#define GDK_DISPLAY_IMPL_X11_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_DISPLAY_IMPL_X11, GdkDisplayImplX11Class))
#define GDK_IS_DISPLAY_IMPL_X11(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_DISPLAY_IMPL_X11))
#define GDK_IS_DISPLAY_IMPL_X11_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_DISPLAY_IMPL_X11))
#define GDK_DISPLAY_IMPL_X11_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_DISPLAY_IMPL_X11, GdkDisplayImplX11Class))

  typedef struct _Xdnd_Action Xdnd_Action;

  struct _Xdnd_Action
  {
    gchar *name;
    GdkAtom atom;
    GdkDragAction action;
  };

  struct _GdkDisplayImplX11
  {
    GdkDisplay parent_instance;
    Display *xdisplay;
    GdkScreen *default_screen;
    GSList *screen_list;

    /* Display wide Atoms */

    Atom gdk_wm_delete_window;
    Atom gdk_wm_take_focus;
    Atom gdk_wm_protocols;
    Atom gdk_wm_window_protocols[3];
    Atom gdk_selection_property;
    Atom wm_client_leader_atom;
    Atom wm_state_atom;
    Atom wm_desktop_atom;
    Atom wmspec_check_atom;
    Atom wmspec_supported_atom;
    Atom timestamp_prop_atom;

    /* Keyboard related information */

    gint autorepeat;
    gint min_keycode;
    gint max_keycode;
    guint _gdk_keymap_serial;
    gboolean _gdk_use_xkb;
    KeySym *keymap;
    gint keysyms_per_keycode;
    XModifierKeymap *mod_keymap;
    GdkModifierType group_switch_mask;

    /* Whether we were able to turn on detectable-autorepeat using
     * XkbSetDetectableAutorepeat. If FALSE, we'll fall back
     * to checking the next event with XPending(). */

    gboolean _gdk_have_xkb_autorepeat;
    guint keymap_current_serial;
#ifdef HAVE_XKB
    XkbDescPtr xkb_desc;
#endif
    gboolean gdk_use_xshm;

    /* Window that currently holds the x pointer grab */

    GdkWindowObject *gdk_xgrab_window;

    /* drag and drop information */
    /* I assume each display needs only info on a default  display */

    GdkScreen *dnd_default_screen;
    GdkDragContext *current_dest_drag;

    /* data needed for MOTIF DnD */

    Atom motif_drag_window_atom;
    Window motif_drag_window;
    GdkWindow *motif_drag_gdk_window;
    Atom motif_drag_targets_atom;
    GList **motif_target_lists;
    gint motif_n_target_lists;

    /* Misc Settings */

    XSettingsClient *xsettings_client;
  };

  struct _GdkDisplayImplX11Class
  {
    GdkDisplayClass parent_class;
  };

GdkDisplay *gdk_x11_display_impl_display_new        (gchar                  *display_name);
char *      gdk_x11_display_impl_get_display_name   (GdkDisplay             *dpy);
gint        gdk_x11_display_impl_get_n_screens      (GdkDisplay             *dpy);
GdkScreen * gdk_x11_display_impl_get_screen         (GdkDisplay             *dpy,
						     gint                    screen_num);
GdkScreen * gdk_x11_display_impl_get_default_screen (GdkDisplay             *dpy);
void        gdk_x11_display_impl_class_init         (GdkDisplayImplX11Class *class);
GType       gdk_x11_display_impl_get_type           (void);
gboolean    gdk_x11_display_impl_is_root_window     (GdkDisplay             *dpy,
						     Window                  xroot_window);

#define DEFAULT_X_DISPLAY   GDK_DISPLAY_IMPL_X11(gdk_get_default_display())->xdisplay
#define GDK_DISPLAY_XDISPLAY(dpy)  (GDK_DISPLAY_IMPL_X11(dpy)->xdisplay)

#ifdef __cplusplus
}
#endif				/* __cplusplus */

#endif				/* __GDK_DISPLAY_X11__ */
