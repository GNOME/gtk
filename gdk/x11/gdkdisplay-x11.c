#if HAVE_CONFIG_H
#include <config.h>
#endif
#include <glib.h>
#include "gdkx.h"
#include "gdkdisplay.h"
#include "gdkdisplay-x11.h"
#include "gdkscreen.h"
#include "gdkscreen-x11.h"
#include "gdkinternals.h"

void	gdk_x11_display_impl_class_init(GdkDisplayImplX11Class *class);

GType		gdk_x11_display_impl_get_type(){
 static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GdkDisplayImplX11Class),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_x11_display_impl_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkDisplayImplX11),
        0,              /* n_preallocs */
        (GInstanceInitFunc) NULL,
      };
      object_type = g_type_register_static (GDK_TYPE_DISPLAY,
                                            "GdkDisplayImplX11",
                                            &object_info, 0);
    }
  return object_type;
}

void gdk_x11_display_impl_class_init(GdkDisplayImplX11Class *class){
    GdkDisplayClass * dpy_class = GDK_DISPLAY_CLASS(class);
    dpy_class->new_display = gdk_x11_display_impl_display_new;
    dpy_class->get_display_name = gdk_x11_display_impl_get_display_name;
    dpy_class->get_n_screens = gdk_x11_display_impl_get_n_screens;
    dpy_class->get_screen = gdk_x11_display_impl_get_screen;
    dpy_class->get_default_screen = gdk_x11_display_impl_get_default_screen;
}




GdkDisplay *	gdk_x11_display_impl_display_new(gchar * display_name)
{
  GdkDisplay * dpy;
  GdkDisplayImplX11* dpy_impl;
  Screen *default_scr;
  GdkScreen *scr;
  GdkScreenImplX11* scr_impl;

  int i=0;
  int scr_num;

  dpy = g_object_new(gdk_x11_display_impl_get_type(),NULL);
  dpy_impl = GDK_DISPLAY_IMPL_X11(dpy);

  if ((dpy_impl->xdisplay = XOpenDisplay(display_name)) == NULL){
	return NULL;
  }

  scr_num = ScreenCount(dpy_impl->xdisplay);
  default_scr = DefaultScreenOfDisplay(dpy_impl->xdisplay);
  /* populate the screen list and set default */
  for(i=0;i<scr_num;i++){ 
      scr = g_object_new(gdk_X11_screen_impl_get_type(),NULL);
      scr_impl = GDK_SCREEN_IMPL_X11(scr);
      scr_impl->display = dpy;
      scr_impl->xdisplay = dpy_impl->xdisplay;
      scr_impl->xscreen = ScreenOfDisplay(dpy_impl->xdisplay, i);
      scr_impl->scr_num = i;
      scr_impl->root_window = (Window) RootWindow (dpy_impl->xdisplay, i);
      scr_impl->wmspec_check_window = None;
      scr_impl->leader_window =  XCreateSimpleWindow(dpy_impl->xdisplay,
						     scr_impl->root_window,
						     10, 10, 10, 10, 0, 0, 0);
      scr_impl->visual_initialised = FALSE;
      scr_impl->colormap_initialised = FALSE;
      if(scr_impl->xscreen == default_scr)
        dpy_impl->default_screen = scr;
      dpy_impl->screen_list = g_slist_append(dpy_impl->screen_list,scr);
  }
  dpy_impl->dnd_default_screen = dpy_impl->default_screen;
  return dpy;
}


gchar * gdk_x11_display_impl_get_display_name(GdkDisplay *dpy){
    GdkDisplayImplX11 *dpy_impl = GDK_DISPLAY_IMPL_X11(dpy);
    return (gchar*) DisplayString(dpy_impl->xdisplay);
}

gint gdk_x11_display_impl_get_n_screens(GdkDisplay *dpy){
    GdkDisplayImplX11 *dpy_impl = GDK_DISPLAY_IMPL_X11(dpy);
    return (gint) ScreenCount(dpy_impl->xdisplay);
}


GdkScreen *gdk_x11_display_impl_get_screen(GdkDisplay *dpy,gint screen_num){
  Screen * desired_scr;
  GSList * tmp;
  GdkDisplayImplX11 *dpy_impl = GDK_DISPLAY_IMPL_X11(dpy);
  g_assert(ScreenCount(dpy_impl->xdisplay) > screen_num);
  tmp = dpy_impl->screen_list;
  g_assert(tmp != NULL);
  desired_scr = ScreenOfDisplay(dpy_impl->xdisplay,screen_num);
  while(tmp){
      if((((GdkScreenImplX11*)tmp->data)->xscreen) == desired_scr)
      {
	GdkScreenImplX11 *desired_screen = (GdkScreenImplX11*) tmp->data; 
	if(!desired_screen->visual_initialised)
	  _gdk_visual_init((GdkScreen*) tmp->data);
	if(!desired_screen->colormap_initialised)
	  _gdk_windowing_window_init((GdkScreen*) tmp->data);
        return (GdkScreen*) tmp->data;
      }
      tmp = g_slist_next(tmp);
  }
  g_error("Internal screen list is corrupted"); 
  exit(1);
}


GdkScreen *gdk_x11_display_impl_get_default_screen(GdkDisplay *dpy){
    GdkDisplayImplX11 *dpy_impl = GDK_DISPLAY_IMPL_X11(dpy);
    return dpy_impl->default_screen;
}

gboolean gdk_x11_display_impl_is_root_window(GdkDisplay *dpy, Window root_window)
{
  GSList * tmp;
  GdkDisplayImplX11 *dpy_impl = GDK_DISPLAY_IMPL_X11(dpy);
  tmp = dpy_impl->screen_list;
  g_assert(tmp != NULL);
  while(tmp){
      if((((GdkScreenImplX11*)tmp->data)->root_window) == root_window)
          return TRUE;
      tmp = g_slist_next(tmp);
  }
  return FALSE;
}

void
gdk_display_use_xshm_set (GdkDisplay * display, gboolean use_xshm)
{
  GDK_DISPLAY_IMPL_X11 (display)->gdk_use_xshm = use_xshm;
}

gboolean
gdk_display_use_xshm_get (GdkDisplay * display)
{
  return GDK_DISPLAY_IMPL_X11 (display)->gdk_use_xshm;
}


void
gdk_display_pointer_ungrab (GdkDisplay * display, guint32 time)
{
  _gdk_input_ungrab_pointer (time);

  XUngrabPointer (GDK_DISPLAY_XDISPLAY (display), time);
  GDK_DISPLAY_IMPL_X11 (display)->gdk_xgrab_window = NULL;
}


gboolean
gdk_display_is_pointer_grabbed (GdkDisplay * display)
{
  return (GDK_DISPLAY_IMPL_X11 (display)->gdk_xgrab_window != NULL);
}

void
gdk_display_keyboard_ungrab (GdkDisplay * display, guint32 time)
{
  XUngrabKeyboard (GDK_DISPLAY_XDISPLAY (display), time);
}

void
gdk_display_beep (GdkDisplay * display)
{
  XBell (GDK_DISPLAY_XDISPLAY (display), 0);
}

/* helper function used for atom caching in gdk_display_atom */

typedef struct {
  GdkDisplay *display;
  GHashTable *hash_table;
} GdkAtomHash;

static GdkAtomHash * 
gdk_atom_hash_get(GdkDisplay *display, GSList * atom_hash_list)
{
  GSList *tmp = atom_hash_list;

  if(!atom_hash_list)
     return NULL;

  while(tmp)
  {
    if(((GdkAtomHash *)tmp->data)->display == display)
      return ((GdkAtomHash *)tmp->data);

    tmp = tmp->next;
  }
  return NULL;
}

GdkAtom
gdk_display_atom (GdkDisplay * dpy,
		  const gchar * atom_name,
		  gboolean only_if_exists)
{
  GdkAtom retval;
  static GSList *atom_hash_list = NULL;
  GdkAtomHash * atom_hash;

  g_return_val_if_fail (atom_name != NULL, GDK_NONE);

  atom_hash = gdk_atom_hash_get(dpy, atom_hash_list);
  if(!atom_hash)
  {
    /* list or hashtable doesn't exist for display */
    atom_hash = g_new(GdkAtomHash, 1);
    atom_hash->display = dpy;
    atom_hash->hash_table = g_hash_table_new (g_str_hash, g_str_equal);
    g_slist_append (atom_hash_list, atom_hash);
  }

  retval = GPOINTER_TO_UINT (g_hash_table_lookup (atom_hash->hash_table,
						  atom_name));
  if (!retval) {
    retval =
      XInternAtom (GDK_DISPLAY_XDISPLAY (dpy), atom_name, only_if_exists);

    if (retval != None)
      g_hash_table_insert (atom_hash->hash_table,
			   g_strdup (atom_name), GUINT_TO_POINTER (retval));
  }
  return retval;
}

