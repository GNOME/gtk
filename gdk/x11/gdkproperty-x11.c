/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <string.h>

#include "gdkx.h"
#include "gdkproperty.h"
#include "gdkprivate.h"
#include "gdkinternals.h"
#include "gdkdisplay-x11.h"
#include "gdkscreen-x11.h"

typedef struct
{
   GdkDisplay *display;
   GSList     *atom_list;
}
GdkAtomDisplayList;

typedef struct
{
    GdkAtom virtual_atom;
    GdkAtom x_atom;
}
GdkAtomMap;

static GSList *display_atom_lists = NULL;


GdkAtom 
gdk_x11_get_real_atom (GdkDisplay *display, GdkAtom virtual_atom, gboolean only_if_exists)
{
  GdkAtomMap *atom_map;
  GdkAtomDisplayList *atom_display_list;
  GSList *tmp_atom_display_list;
  GSList *tmp_atom_map_list;
  gboolean found = FALSE;
  
  if (virtual_atom == 0)
  {
/*    g_print ("return real  = 0\n");*/
    return 0;
  }
  
  if (display_atom_lists == NULL)
  {
    atom_map = g_new0 (GdkAtomMap, 1);
    atom_map->virtual_atom = virtual_atom;
    atom_map->x_atom = XInternAtom (GDK_DISPLAY_XDISPLAY (display),
				    g_quark_to_string (virtual_atom), 
				    only_if_exists);
    if (!atom_map->x_atom)
    {
      g_free (atom_map);
/*      g_print ("Fail XInternAtom\n");*/
      return 0;
    }
    atom_display_list = g_new0 (GdkAtomDisplayList, 1);
    atom_display_list->display = display;
    atom_display_list->atom_list = g_slist_append (atom_display_list->atom_list,
					           atom_map);
    display_atom_lists = g_slist_append (display_atom_lists, atom_display_list);
/*    g_print ("first Get REAL(%d) (%s) return X Atom is %d\n", virtual_atom, g_quark_to_string (virtual_atom), atom_map->x_atom);*/
    return atom_map->x_atom;
  }
  /* find atom map list for display */
  tmp_atom_display_list = display_atom_lists;
  while (tmp_atom_display_list && !found)
  {
    if (((GdkAtomDisplayList*)tmp_atom_display_list->data)->display == display)
      found = TRUE;
    else
      tmp_atom_display_list = tmp_atom_display_list->next;
  }
  if (found) 
  {
    /* check is atom is already cached */
    tmp_atom_map_list = ((GdkAtomDisplayList *)tmp_atom_display_list->data)->atom_list;
    atom_display_list = ((GdkAtomDisplayList *)tmp_atom_display_list->data);
    while (tmp_atom_map_list)
    {
      if (((GdkAtomMap *)tmp_atom_map_list->data)->virtual_atom == virtual_atom)
      {
        return ((GdkAtomMap *)tmp_atom_map_list->data)->x_atom;
      }
      tmp_atom_map_list = tmp_atom_map_list->next;
    }
    /* not found get and add mapping */
    atom_map = g_new0 (GdkAtomMap, 1);
    atom_map->virtual_atom = virtual_atom;
    atom_map->x_atom = XInternAtom (GDK_DISPLAY_XDISPLAY (display),
				    g_quark_to_string (virtual_atom), 
				    only_if_exists);
    if (!atom_map->x_atom)
    {
      g_free (atom_map);
      return None;
    }
    atom_display_list->atom_list = g_slist_append (atom_display_list->atom_list,
					           atom_map);    

/*    g_print ("Get REAL(%d) (%s) return X Atom is %d\n", virtual_atom, g_quark_to_string (virtual_atom), atom_map->x_atom);*/
    return atom_map->x_atom;
  }
    /* new display list and new atom */
  atom_map = g_new0 (GdkAtomMap, 1);
  atom_map->virtual_atom = virtual_atom;
  atom_map->x_atom = XInternAtom (GDK_DISPLAY_XDISPLAY (display),
      			    g_quark_to_string (virtual_atom), 
      			    only_if_exists);
  if (!atom_map->x_atom)
    {
      g_free (atom_map);
      return 0;
  }
  atom_display_list = g_new0 (GdkAtomDisplayList, 1);
  atom_display_list->display = display;
  atom_display_list->atom_list = g_slist_append (atom_display_list->atom_list,
      				           atom_map);
  display_atom_lists = g_slist_append (display_atom_lists, atom_display_list);
/*  g_print ("!New Display! Get REAL(%d) (%s) return X Atom is %d\n", virtual_atom, g_quark_to_string (virtual_atom), atom_map->x_atom);*/
  return atom_map->x_atom;    
}

GdkAtom
gdk_x11_get_virtual_atom (GdkDisplay *display, GdkAtom xatom)
{
  GdkAtomMap *atom_map;
  GdkAtomDisplayList *atom_display_list;
  GSList *tmp_atom_display_list;
  GSList *tmp_atom_map_list;
  gchar *xatom_string = NULL;
  gboolean found = FALSE;

  if (xatom == 0){
    /*g_print ("Get VIRTUAL (%d) return virtual = ZERO\n", xatom);*/
    return 0;
  }

  if (display_atom_lists == NULL)
  {
    atom_map = g_new0 (GdkAtomMap, 1);
    atom_map->x_atom = xatom;
    xatom_string = XGetAtomName (GDK_DISPLAY_XDISPLAY (display), xatom);
    atom_map->virtual_atom = g_quark_from_string (xatom_string);
    XFree (xatom_string);
    atom_display_list = g_new0 (GdkAtomDisplayList, 1);
    atom_display_list->display = display;
    atom_display_list->atom_list = g_slist_append (atom_display_list->atom_list,
					           atom_map);
    display_atom_lists = g_slist_append (display_atom_lists, atom_display_list);
    return atom_map->virtual_atom;
  }
  /* find atom map list for display */
  tmp_atom_display_list = display_atom_lists;
  while (tmp_atom_display_list && !found)
  {
    if (((GdkAtomDisplayList*)tmp_atom_display_list->data)->display == display)
      found = TRUE;
    else
      tmp_atom_display_list = tmp_atom_display_list->next;
  }
  if (found) 
  {
    /* check is atom is already cached */
    tmp_atom_map_list = ((GdkAtomDisplayList *)tmp_atom_display_list->data)->atom_list;
    atom_display_list = ((GdkAtomDisplayList *)tmp_atom_display_list->data);
    while (tmp_atom_map_list)
    {
      if (((GdkAtomMap *)tmp_atom_map_list->data)->x_atom == xatom)
      {
        return ((GdkAtomMap *)tmp_atom_map_list->data)->virtual_atom;
      }
      tmp_atom_map_list = tmp_atom_map_list->next;
    }
    /* not found get and add mapping */
    atom_map = g_new0 (GdkAtomMap, 1);
    atom_map->x_atom = xatom;
    xatom_string = XGetAtomName (GDK_DISPLAY_XDISPLAY (display), xatom);
    atom_map->virtual_atom = g_quark_from_string (xatom_string);
/*    g_print ("\nGet VIRTUAL (%d) string (%s) return virtual = %d\n", xatom, xatom_string, atom_map->virtual_atom);*/
    XFree (xatom_string);
    atom_display_list->atom_list = g_slist_append (atom_display_list->atom_list,
					           atom_map);    
    return atom_map->virtual_atom;
  }
    /* new display list and new atom */
  atom_map = g_new0 (GdkAtomMap, 1);
  atom_map->x_atom = xatom;
  xatom_string = XGetAtomName (GDK_DISPLAY_XDISPLAY (display), xatom);
  atom_map->virtual_atom = g_quark_from_string (xatom_string);
/*g_print ("\n!New Display! Get VIRTUAL (%d) string (%s) return virtual = %d\n", xatom, xatom_string, atom_map->virtual_atom);*/
  XFree (xatom_string);  
  atom_display_list = g_new0 (GdkAtomDisplayList, 1);
  atom_display_list->display = display;
  atom_display_list->atom_list = g_slist_append (atom_display_list->atom_list,
      				           atom_map);
  display_atom_lists = g_slist_append (display_atom_lists, atom_display_list);
  return atom_map->virtual_atom;    
}

GdkAtom
gdk_x11_get_real_atom_by_name (GdkDisplay *display, const gchar *atom_name)
{
/*  g_print("Get Real Atom by Name %s\n",atom_name);*/
  return  gdk_x11_get_real_atom (display, gdk_atom_intern (atom_name, FALSE), FALSE);
}

gchar*
gdk_x11_get_real_atom_name (GdkDisplay *display, GdkAtom xatom)
{
  gchar *name, *tmp;
  tmp = XGetAtomName (GDK_DISPLAY_XDISPLAY (display), xatom);
  name = g_strdup (tmp);
  if (tmp)
    XFree (tmp);
  return name;
}

GdkAtom
gdk_atom_intern (const gchar *atom_name, gboolean only_if_exists)
{
  g_return_val_if_fail (atom_name != NULL, GDK_NONE);
  return g_quark_from_string (atom_name);
}

gchar*
gdk_atom_name (GdkAtom atom)
{
  return g_strdup (g_quark_to_string (atom));
}

gboolean
gdk_property_get (GdkWindow   *window,
		  GdkAtom      property,
		  GdkAtom      type,
		  gulong       offset,
		  gulong       length,
		  gint         pdelete,
		  GdkAtom     *actual_property_type,
		  gint        *actual_format_type,
		  gint        *actual_length,
		  guchar     **data)
{
  Display *xdisplay;
  Window xwindow;
  Atom ret_prop_type;
  gint ret_format;
  gulong ret_nitems;
  gulong ret_bytes_after;
  gulong ret_length;
  guchar *ret_data;

  g_return_val_if_fail (!window || GDK_IS_WINDOW (window), FALSE);

  if (window)
    {
      if (GDK_WINDOW_DESTROYED (window))
	return FALSE;

      xdisplay = GDK_WINDOW_XDISPLAY (window);
      xwindow = GDK_WINDOW_XID (window);
    }
  else
    {
        g_warning("gdk_property_get no GdkWindow defined\n");
	return FALSE;
    }

  ret_data = NULL;
  XGetWindowProperty (xdisplay, xwindow, property,
		      offset, (length + 3) / 4, pdelete,
		      type, &ret_prop_type, &ret_format,
		      &ret_nitems, &ret_bytes_after,
		      &ret_data);

  if ((ret_prop_type == None) && (ret_format == 0)) {
    return FALSE;
  }

  if (actual_property_type)
    *actual_property_type = ret_prop_type;
  if (actual_format_type)
    *actual_format_type = ret_format;

  if ((type != AnyPropertyType) && (ret_prop_type != type))
    {
      gchar *rn, *pn;

      XFree (ret_data);
      rn = gdk_x11_get_real_atom_name (GDK_WINDOW_DISPLAY(window), ret_prop_type);

      pn = gdk_x11_get_real_atom_name (GDK_WINDOW_DISPLAY(window), type);

      g_warning ("Couldn't match property type %s to %s\n", rn, pn);
      g_free (rn); g_free (pn);
      return FALSE;
    }

  /* FIXME: ignoring bytes_after could have very bad effects */

  if (data)
    {
      switch (ret_format)
	{
	case 8:
	  ret_length = ret_nitems;
	  break;
	case 16:
	  ret_length = sizeof(short) * ret_nitems;
	  break;
	case 32:
	  ret_length = sizeof(long) * ret_nitems;
	  break;
	default:
	  g_warning ("unknown property return format: %d", ret_format);
	  XFree (ret_data);
	  return FALSE;
	}

      *data = g_new (guchar, ret_length);
      memcpy (*data, ret_data, ret_length);
      if (actual_length)
	*actual_length = ret_length;
    }

  XFree (ret_data);

  return TRUE;
}

void
gdk_property_change (GdkWindow    *window,
		     GdkAtom       property,
		     GdkAtom       type,
		     gint          format,
		     GdkPropMode   mode,
		     const guchar *data,
		     gint          nelements)
{
  Display *xdisplay;
  Window xwindow;

  g_return_if_fail (!window || GDK_IS_WINDOW (window));

  if (window)
    {
      if (GDK_WINDOW_DESTROYED (window))
	return;

      xdisplay = GDK_WINDOW_XDISPLAY (window);
      xwindow = GDK_WINDOW_XID (window);
    }
  else
    {
      g_warning("gdk_property_change no GdkWindow defined\n");
      return;
    }

  XChangeProperty (xdisplay, xwindow, property, type,
		   format, mode, (guchar *)data, nelements);
}

void
gdk_property_delete (GdkWindow *window,
		     GdkAtom    property)
{
  Display *xdisplay;
  Window xwindow;

  g_return_if_fail (!window || GDK_IS_WINDOW (window));

  if (window)
    {
      if (GDK_WINDOW_DESTROYED (window))
	return;

      xdisplay = GDK_WINDOW_XDISPLAY (window);
      xwindow = GDK_WINDOW_XID (window);
      XDeleteProperty (xdisplay, xwindow, property);

    }
/*  removed for multihead support,
 *  use root_window from a GdkScreen to delete a prop
 *  from the specific root window.
    else
    {
      xdisplay = gdk_display;
      xwindow = gdk_root_window;
    }
*/
 }
