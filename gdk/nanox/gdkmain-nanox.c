#include "gdk.h"
#include "gdkinternals.h"
#include "gdkprivate-nanox.h"

static GR_SCREEN_INFO screen_info;
static int gdk_use_xshm = 0; /* shm not supported */
guint gdk_selection_property = 0;
gchar* gdk_progclass = NULL;
GdkWindowPrivate* gdk_xgrab_window = NULL;

GdkArgDesc _gdk_windowing_args[] = {
  {NULL}
};

gboolean
_gdk_windowing_init_check (int argc, char **argv)
{
  int result = GrOpen();
  if (result < 0)
	return 0;
  GrGetScreenInfo(&screen_info);

  return 1;
}


gchar*
gdk_set_locale (void)
{
	return "";
}

void
gdk_set_use_xshm (gboolean use_xshm)
{
  gdk_use_xshm = 0; /* shm not supported */
}

gboolean
gdk_get_use_xshm (void)
{
  return gdk_use_xshm;
}

GdkGrabStatus
gdk_pointer_grab (GdkWindow *	  window,
		  gboolean	  owner_events,
		  GdkEventMask	  event_mask,
		  GdkWindow *	  confine_to,
		  GdkCursor *	  cursor,
		  guint32	  time)
{
		g_message("unimplemented %s", __FUNCTION__);
  return 0;
}


void
gdk_pointer_ungrab (guint32 time)
{
		g_message("unimplemented %s", __FUNCTION__);
}


gboolean
gdk_pointer_is_grabbed (void)
{
  return gdk_xgrab_window != NULL;
}

GdkGrabStatus
gdk_keyboard_grab (GdkWindow *	   window,
		   gboolean	   owner_events,
		   guint32	   time)
{
		g_message("unimplemented %s", __FUNCTION__);
  return 0;
}


void
gdk_keyboard_ungrab (guint32 time)
{
		g_message("unimplemented %s", __FUNCTION__);
}

gint
gdk_screen_width (void)
{
  return screen_info.cols;
}

gint
gdk_screen_height (void)
{
  return screen_info.rows;
}

gint
gdk_screen_width_mm (void)
{
  return screen_info.cols*10/screen_info.xdpcm; 
}

gint
gdk_screen_height_mm (void)
{
  return screen_info.rows*10/screen_info.ydpcm; 
}

void
gdk_set_sm_client_id (const gchar* sm_client_id)
{
		g_message("unimplemented %s", __FUNCTION__);
}


void
gdk_key_repeat_disable (void)
{
		g_message("unimplemented %s", __FUNCTION__);
}

void
gdk_key_repeat_restore (void)
{
		g_message("unimplemented %s", __FUNCTION__);
}

void
gdk_beep (void)
{
		g_message("unimplemented %s", __FUNCTION__);
}

void
gdk_windowing_exit (void)
{
  GrClose();
}

gchar *
gdk_get_display (void)
{
  return "nano-X";
}

gchar*
gdk_keyval_name (guint	      keyval)
{
	static gchar buf[64];
	g_snprintf(buf, 64, "%c", keyval);
	return buf;
}

guint
gdk_keyval_from_name (const gchar *keyval_name)
{
  return *keyval_name;
}

/*
void
gdk_keyval_convert_case (guint symbol,
			 guint *lower,
			 guint *upper)
{
}
*/

static guint gdk_xid_hash    (guint *xid);
static gint  gdk_xid_compare (guint *a,
			      guint *b);


static GHashTable *xid_ht = NULL;


void
gdk_xid_table_insert (guint      *xid,
		      gpointer  data)
{
  g_return_if_fail (xid != NULL);

  if (!xid_ht)
    xid_ht = g_hash_table_new ((GHashFunc) gdk_xid_hash,
			       (GCompareFunc) gdk_xid_compare);

  g_hash_table_insert (xid_ht, xid, data);
}

void
gdk_xid_table_remove (guint xid)
{
  if (!xid_ht)
    xid_ht = g_hash_table_new ((GHashFunc) gdk_xid_hash,
			       (GCompareFunc) gdk_xid_compare);

  g_hash_table_remove (xid_ht, &xid);
}

gpointer
gdk_xid_table_lookup (guint xid)
{
  gpointer data = NULL;

  if (xid_ht)
    data = g_hash_table_lookup (xid_ht, &xid);
  
  return data;
}


static guint
gdk_xid_hash (guint *xid)
{
  return *xid;
}

static gint
gdk_xid_compare (guint *a,
		 guint *b)
{
  return (*a == *b);
}

gchar *
gdk_wcstombs (const GdkWChar *src)
{
  gchar *mbstr;
  gint i, length = 0;

  while (src[length] != 0)
	length++;
  mbstr = g_new (gchar, length + 1);
  for (i=0; i <length+1;++i)
	mbstr[i] = src[i];
  return mbstr;
}

gint
gdk_mbstowcs (GdkWChar *dest, const gchar *src, gint dest_max)
{
  gint i;

  for (i=0; i < dest_max && src[i]; i++)
	dest[i] = src[i];
  return i;
}

