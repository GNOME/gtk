#include "gdk.h"
#include "gdkprivate-nanox.h"

gboolean
gdk_selection_owner_set (GdkWindow *owner,
			 GdkAtom    selection,
			 guint32    time,
			 gboolean   send_event)
{
		g_message("unimplemented %s", __FUNCTION__);
  return 0;
}


GdkWindow*
gdk_selection_owner_get (GdkAtom selection)
{
		g_message("unimplemented %s", __FUNCTION__);
  return NULL;
}


void
gdk_selection_convert (GdkWindow *requestor,
		       GdkAtom    selection,
		       GdkAtom    target,
		       guint32    time)
{
		g_message("unimplemented %s", __FUNCTION__);
}


gint
gdk_selection_property_get (GdkWindow  *requestor,
			    guchar    **data,
			    GdkAtom    *ret_type,
			    gint       *ret_format)
{
		g_message("unimplemented %s", __FUNCTION__);
  return 0;
}


void
gdk_selection_send_notify (guint32  requestor,
			   GdkAtom  selection,
			   GdkAtom  target,
			   GdkAtom  property,
			   guint32  time)
{
		g_message("unimplemented %s", __FUNCTION__);
}

gint
gdk_text_property_to_text_list (GdkAtom       encoding,
				gint          format, 
				const guchar *text,
				gint          length,
				gchar      ***list)
{
		g_message("unimplemented %s", __FUNCTION__);
  return 0;
}


void
gdk_free_text_list (gchar **list)
{
  g_return_if_fail (list != NULL);

}

gint
gdk_string_to_compound_text (const gchar *str,
			     GdkAtom     *encoding,
			     gint        *format,
			     guchar     **ctext,
			     gint        *length)
{
		g_message("unimplemented %s", __FUNCTION__);
  return 0;
}

void gdk_free_compound_text (guchar *ctext)
{
/*  if (ctext)
    g_free (ctext);*/
}

