#include "gdk.h"
#include "gdkprivate-nanox.h"

GdkAtom
gdk_atom_intern (const gchar *atom_name,
		 gboolean     only_if_exists)
{
		g_message("unimplemented %s", __FUNCTION__);
  return 0;
}

gchar*
gdk_atom_name (GdkAtom atom)
{
		g_message("unimplemented %s", __FUNCTION__);
  return NULL;
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
		g_message("unimplemented %s", __FUNCTION__);
  return 0;
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
		g_message("unimplemented %s", __FUNCTION__);
  return;
}

void
gdk_property_delete (GdkWindow *window,
		     GdkAtom    property)
{
		g_message("unimplemented %s", __FUNCTION__);
  return;
}

