/*
 * gtkimcontextime.c
 * Copyright (C) 2003 Takuro Ashie
 * Copyright (C) 2003-2004 Kazuki IWAMOTO
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*
 *  Please see the following site for the detail of Windows IME API.
 *  http://msdn.microsoft.com/library/default.asp?url=/library/en-us/appendix/hh/appendix/imeimes2_35ph.asp
 */

#ifdef GTK_DISABLE_DEPRECATED
#undef GTK_DISABLE_DEPRECATED
#endif

#include "gtkimcontextime.h"

#include "imm-extra.h"

#include "gdk/gdkkeysyms-compat.h"
#include "gdk/win32/gdkwin32.h"
#include "gdk/gdkkeysyms.h"

#include <pango/pango.h>

/* avoid warning */
#ifdef STRICT
# undef STRICT
# include <pango/pangowin32.h>
# ifndef STRICT
#   define STRICT 1
# endif
#else /* STRICT */
#   include <pango/pangowin32.h>
#endif /* STRICT */

/* #define BUFSIZE 4096 */

#define IS_DEAD_KEY(k) \
    ((k) >= GDK_dead_grave && (k) <= (GDK_dead_dasia+1))

#define FREE_PREEDIT_BUFFER(ctx) \
{                                \
  g_free((ctx)->priv->comp_str); \
  g_free((ctx)->priv->read_str); \
  (ctx)->priv->comp_str = NULL;  \
  (ctx)->priv->read_str = NULL;  \
  (ctx)->priv->comp_str_len = 0; \
  (ctx)->priv->read_str_len = 0; \
}


struct _GtkIMContextIMEPrivate
{
  /* save IME context when the client window is focused out */
  DWORD conversion_mode;
  DWORD sentence_mode;

  LPVOID comp_str;
  DWORD comp_str_len;
  LPVOID read_str;
  DWORD read_str_len;

  guint32 dead_key_keyval;
};


/* GObject class methods */
static void gtk_im_context_ime_class_init (GtkIMContextIMEClass *class);
static void gtk_im_context_ime_init       (GtkIMContextIME      *context_ime);
static void gtk_im_context_ime_dispose    (GObject              *obj);
static void gtk_im_context_ime_finalize   (GObject              *obj);

static void gtk_im_context_ime_set_property (GObject      *object,
                                             guint         prop_id,
                                             const GValue *value,
                                             GParamSpec   *pspec);
static void gtk_im_context_ime_get_property (GObject      *object,
                                             guint         prop_id,
                                             GValue       *value,
                                             GParamSpec   *pspec);

/* GtkIMContext's virtual functions */
static void gtk_im_context_ime_set_client_window   (GtkIMContext *context,
                                                    GdkWindow    *client_window);
static gboolean gtk_im_context_ime_filter_keypress (GtkIMContext   *context,
                                                    GdkEventKey    *event);
static void gtk_im_context_ime_reset               (GtkIMContext   *context);
static void gtk_im_context_ime_get_preedit_string  (GtkIMContext   *context,
                                                    gchar         **str,
                                                    PangoAttrList **attrs,
                                                    gint           *cursor_pos);
static void gtk_im_context_ime_focus_in            (GtkIMContext   *context);
static void gtk_im_context_ime_focus_out           (GtkIMContext   *context);
static void gtk_im_context_ime_set_cursor_location (GtkIMContext   *context,
                                                    GdkRectangle   *area);
static void gtk_im_context_ime_set_use_preedit     (GtkIMContext   *context,
                                                    gboolean        use_preedit);

/* GtkIMContextIME's private functions */
static void gtk_im_context_ime_set_preedit_font (GtkIMContext    *context);

static GdkFilterReturn
gtk_im_context_ime_message_filter               (GdkXEvent       *xevent,
                                                 GdkEvent        *event,
                                                 gpointer         data);
static void get_window_position                 (GdkWindow       *win,
                                                 gint            *x,
                                                 gint            *y);
static void cb_client_widget_hierarchy_changed  (GtkWidget       *widget,
                                                 GtkWidget       *widget2,
                                                 GtkIMContextIME *context_ime);

GType gtk_type_im_context_ime = 0;
static GObjectClass *parent_class;


void
gtk_im_context_ime_register_type (GTypeModule *type_module)
{
  const GTypeInfo im_context_ime_info = {
    sizeof (GtkIMContextIMEClass),
    (GBaseInitFunc) NULL,
    (GBaseFinalizeFunc) NULL,
    (GClassInitFunc) gtk_im_context_ime_class_init,
    NULL,                       /* class_finalize */
    NULL,                       /* class_data */
    sizeof (GtkIMContextIME),
    0,
    (GInstanceInitFunc) gtk_im_context_ime_init,
  };

  gtk_type_im_context_ime =
    g_type_module_register_type (type_module,
                                 GTK_TYPE_IM_CONTEXT,
                                 "GtkIMContextIME", &im_context_ime_info, 0);
}

static void
gtk_im_context_ime_class_init (GtkIMContextIMEClass *class)
{
  GtkIMContextClass *im_context_class = GTK_IM_CONTEXT_CLASS (class);
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  parent_class = g_type_class_peek_parent (class);

  gobject_class->finalize     = gtk_im_context_ime_finalize;
  gobject_class->dispose      = gtk_im_context_ime_dispose;
  gobject_class->set_property = gtk_im_context_ime_set_property;
  gobject_class->get_property = gtk_im_context_ime_get_property;

  im_context_class->set_client_window   = gtk_im_context_ime_set_client_window;
  im_context_class->filter_keypress     = gtk_im_context_ime_filter_keypress;
  im_context_class->reset               = gtk_im_context_ime_reset;
  im_context_class->get_preedit_string  = gtk_im_context_ime_get_preedit_string;
  im_context_class->focus_in            = gtk_im_context_ime_focus_in;
  im_context_class->focus_out           = gtk_im_context_ime_focus_out;
  im_context_class->set_cursor_location = gtk_im_context_ime_set_cursor_location;
  im_context_class->set_use_preedit     = gtk_im_context_ime_set_use_preedit;
}


static void
gtk_im_context_ime_init (GtkIMContextIME *context_ime)
{
  context_ime->client_window          = NULL;
  context_ime->toplevel               = NULL;
  context_ime->use_preedit            = TRUE;
  context_ime->preediting             = FALSE;
  context_ime->opened                 = FALSE;
  context_ime->focus                  = FALSE;
  context_ime->cursor_location.x      = 0;
  context_ime->cursor_location.y      = 0;
  context_ime->cursor_location.width  = 0;
  context_ime->cursor_location.height = 0;

  context_ime->priv = g_malloc0 (sizeof (GtkIMContextIMEPrivate));
  context_ime->priv->conversion_mode  = 0;
  context_ime->priv->sentence_mode    = 0;
  context_ime->priv->comp_str         = NULL;
  context_ime->priv->comp_str_len     = 0;
  context_ime->priv->read_str         = NULL;
  context_ime->priv->read_str_len     = 0;
}


static void
gtk_im_context_ime_dispose (GObject *obj)
{
  GtkIMContext *context = GTK_IM_CONTEXT (obj);
  GtkIMContextIME *context_ime = GTK_IM_CONTEXT_IME (obj);

  if (context_ime->client_window)
    gtk_im_context_ime_set_client_window (context, NULL);

  FREE_PREEDIT_BUFFER (context_ime);

  if (G_OBJECT_CLASS (parent_class)->dispose)
    G_OBJECT_CLASS (parent_class)->dispose (obj);
}


static void
gtk_im_context_ime_finalize (GObject *obj)
{
  /* GtkIMContext *context = GTK_IM_CONTEXT (obj); */
  GtkIMContextIME *context_ime = GTK_IM_CONTEXT_IME (obj);

  g_free (context_ime->priv);
  context_ime->priv = NULL;

  if (G_OBJECT_CLASS (parent_class)->finalize)
    G_OBJECT_CLASS (parent_class)->finalize (obj);
}


static void
gtk_im_context_ime_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GtkIMContextIME *context_ime = GTK_IM_CONTEXT_IME (object);

  g_return_if_fail (GTK_IS_IM_CONTEXT_IME (context_ime));

  switch (prop_id)
    {
    default:
      break;
    }
}


static void
gtk_im_context_ime_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GtkIMContextIME *context_ime = GTK_IM_CONTEXT_IME (object);

  g_return_if_fail (GTK_IS_IM_CONTEXT_IME (context_ime));

  switch (prop_id)
    {
    default:
      break;
    }
}


GtkIMContext *
gtk_im_context_ime_new (void)
{
  return g_object_new (GTK_TYPE_IM_CONTEXT_IME, NULL);
}


static void
gtk_im_context_ime_set_client_window (GtkIMContext *context,
                                      GdkWindow    *client_window)
{
  GtkIMContextIME *context_ime;

  g_return_if_fail (GTK_IS_IM_CONTEXT_IME (context));
  context_ime = GTK_IM_CONTEXT_IME (context);

  if (client_window)
    {
      HIMC himc;
      HWND hwnd;

      hwnd = gdk_win32_window_get_impl_hwnd (client_window);
      himc = ImmGetContext (hwnd);
      if (himc)
	{
	  context_ime->opened = ImmGetOpenStatus (himc);
	  ImmGetConversionStatus (himc,
				  &context_ime->priv->conversion_mode,
				  &context_ime->priv->sentence_mode);
	  ImmReleaseContext (hwnd, himc);
	}
    }
  else if (context_ime->focus)
    {
      gtk_im_context_ime_focus_out (context);
    }

  context_ime->client_window = client_window;
}

static gunichar
_gtk_im_context_ime_dead_key_unichar (guint    keyval,
                                      gboolean spacing)
{
  switch (keyval)
    {
#define CASE(keysym, unicode, spacing_unicode) \
      case GDK_dead_##keysym: return (spacing) ? spacing_unicode : unicode;

      CASE (grave, 0x0300, 0x0060);
      CASE (acute, 0x0301, 0x00b4);
      CASE (circumflex, 0x0302, 0x005e);
      CASE (tilde, 0x0303, 0x007e);	/* Also used with perispomeni, 0x342. */
      CASE (macron, 0x0304, 0x00af);
      CASE (breve, 0x0306, 0x02d8);
      CASE (abovedot, 0x0307, 0x02d9);
      CASE (diaeresis, 0x0308, 0x00a8);
      CASE (hook, 0x0309, 0);
      CASE (abovering, 0x030A, 0x02da);
      CASE (doubleacute, 0x030B, 0x2dd);
      CASE (caron, 0x030C, 0x02c7);
      CASE (abovecomma, 0x0313, 0);         /* Equivalent to psili */
      CASE (abovereversedcomma, 0x0314, 0); /* Equivalent to dasia */
      CASE (horn, 0x031B, 0);	/* Legacy use for psili, 0x313 (or 0x343). */
      CASE (belowdot, 0x0323, 0);
      CASE (cedilla, 0x0327, 0x00b8);
      CASE (ogonek, 0x0328, 0);	/* Legacy use for dasia, 0x314.*/
      CASE (iota, 0x0345, 0);

#undef CASE
    default:
      return 0;
    }
}

static void
_gtk_im_context_ime_commit_unichar (GtkIMContextIME *context_ime,
                                    gunichar         c)
{
  gchar utf8[10];
  int len;

  if (context_ime->priv->dead_key_keyval != 0)
    {
      gunichar combining;

      combining =
        _gtk_im_context_ime_dead_key_unichar (context_ime->priv->dead_key_keyval,
                                              FALSE);
      g_unichar_compose (c, combining, &c);
    }

  len = g_unichar_to_utf8 (c, utf8);
  utf8[len] = 0;

  g_signal_emit_by_name (context_ime, "commit", utf8);
  context_ime->priv->dead_key_keyval = 0;
}

static gboolean
gtk_im_context_ime_filter_keypress (GtkIMContext *context,
                                    GdkEventKey  *event)
{
  GtkIMContextIME *context_ime;
  gboolean retval = FALSE;
  guint32 c;

  g_return_val_if_fail (GTK_IS_IM_CONTEXT_IME (context), FALSE);
  g_return_val_if_fail (event, FALSE);

  if (event->type == GDK_KEY_RELEASE)
    return FALSE;

  if (event->state & GDK_CONTROL_MASK)
    return FALSE;

  context_ime = GTK_IM_CONTEXT_IME (context);

  if (!context_ime->focus)
    return FALSE;

  if (!GDK_IS_WINDOW (context_ime->client_window))
    return FALSE;

  if (event->keyval == GDK_space &&
      context_ime->priv->dead_key_keyval != 0)
    {
      c = _gtk_im_context_ime_dead_key_unichar (context_ime->priv->dead_key_keyval, TRUE);
      context_ime->priv->dead_key_keyval = 0;
      _gtk_im_context_ime_commit_unichar (context_ime, c);
      return TRUE;
    }

  c = gdk_keyval_to_unicode (event->keyval);

  if (c)
    {
      _gtk_im_context_ime_commit_unichar (context_ime, c);
      retval = TRUE;
    }
  else if (IS_DEAD_KEY (event->keyval))
    {
      gunichar dead_key;

      dead_key = _gtk_im_context_ime_dead_key_unichar (event->keyval, FALSE);

      /* Emulate double input of dead keys */
      if (dead_key && event->keyval == context_ime->priv->dead_key_keyval)
        {
          c = _gtk_im_context_ime_dead_key_unichar (context_ime->priv->dead_key_keyval, TRUE);
          context_ime->priv->dead_key_keyval = 0;
          _gtk_im_context_ime_commit_unichar (context_ime, c);
          _gtk_im_context_ime_commit_unichar (context_ime, c);
        }
      else
        context_ime->priv->dead_key_keyval = event->keyval;
    }

  return retval;
}


static void
gtk_im_context_ime_reset (GtkIMContext *context)
{
  GtkIMContextIME *context_ime = GTK_IM_CONTEXT_IME (context);
  HWND hwnd;
  HIMC himc;

  if (!context_ime->client_window)
    return;

  hwnd = gdk_win32_window_get_impl_hwnd (context_ime->client_window);
  himc = ImmGetContext (hwnd);
  if (!himc)
    return;

  if (context_ime->preediting)
    {
      if (ImmGetOpenStatus (himc))
        ImmNotifyIME (himc, NI_COMPOSITIONSTR, CPS_CANCEL, 0);

      context_ime->preediting = FALSE;
      g_signal_emit_by_name (context, "preedit-changed");
    }

  ImmReleaseContext (hwnd, himc);
}


static gchar *
get_utf8_preedit_string (GtkIMContextIME *context_ime, gint *pos_ret)
{
  gchar *utf8str = NULL;
  HWND hwnd;
  HIMC himc;
  gint pos = 0;

  if (pos_ret)
    *pos_ret = 0;

  if (!context_ime->client_window)
    return g_strdup ("");
  hwnd = gdk_win32_window_get_impl_hwnd (context_ime->client_window);
  himc = ImmGetContext (hwnd);
  if (!himc)
    return g_strdup ("");

  if (context_ime->preediting)
    {
      glong len;

      len = ImmGetCompositionStringW (himc, GCS_COMPSTR, NULL, 0);
      if (len > 0)
	{
	  GError *error = NULL;
	  gpointer buf = g_alloca (len);

	  ImmGetCompositionStringW (himc, GCS_COMPSTR, buf, len);
	  len /= 2;
	  utf8str = g_utf16_to_utf8 (buf, len, NULL, NULL, &error);
	  if (error)
	    {
	      g_warning ("%s", error->message);
	      g_error_free (error);
	    }
	  
	  if (pos_ret)
	    {
	      pos = ImmGetCompositionStringW (himc, GCS_CURSORPOS, NULL, 0);
	      if (pos < 0 || len < pos)
		{
		  g_warning ("ImmGetCompositionString: "
			     "Invalid cursor position!");
		  pos = 0;
		}
	    }
	}
    }

  if (!utf8str)
    {
      utf8str = g_strdup ("");
      pos = 0;
    }

  if (pos_ret)
    *pos_ret = pos;

  ImmReleaseContext (hwnd, himc);

  return utf8str;
}


static PangoAttrList *
get_pango_attr_list (GtkIMContextIME *context_ime, const gchar *utf8str)
{
  PangoAttrList *attrs = pango_attr_list_new ();
  HWND hwnd;
  HIMC himc;

  if (!context_ime->client_window)
    return attrs;
  hwnd = gdk_win32_window_get_impl_hwnd (context_ime->client_window);
  himc = ImmGetContext (hwnd);
  if (!himc)
    return attrs;

  if (context_ime->preediting)
    {
      const gchar *schr = utf8str, *echr;
      guint8 *buf;
      guint16 f_red, f_green, f_blue, b_red, b_green, b_blue;
      glong len, spos = 0, epos, sidx = 0, eidx;
      PangoAttribute *attr;

      /*
       *  get attributes list of IME.
       */
      len = ImmGetCompositionStringW (himc, GCS_COMPATTR, NULL, 0);
      buf = g_alloca (len);
      ImmGetCompositionStringW (himc, GCS_COMPATTR, buf, len);

      /*
       *  schr and echr are pointer in utf8str.
       */
      for (echr = g_utf8_next_char (utf8str); *schr != '\0';
           echr = g_utf8_next_char (echr))
        {
          /*
           *  spos and epos are buf(attributes list of IME) position by
           *  bytes.
           *  Using the wide-char API, this value is same with UTF-8 offset.
           */
	  epos = g_utf8_pointer_to_offset (utf8str, echr);

          /*
           *  sidx and eidx are positions in utf8str by bytes.
           */
          eidx = echr - utf8str;

          /*
           *  convert attributes list to PangoAttriute.
           */
          if (*echr == '\0' || buf[spos] != buf[epos])
            {
              switch (buf[spos])
                {
                case ATTR_TARGET_CONVERTED:
                  attr = pango_attr_underline_new (PANGO_UNDERLINE_DOUBLE);
                  attr->start_index = sidx;
                  attr->end_index = eidx;
                  pango_attr_list_change (attrs, attr);
                  f_red = f_green = f_blue = 0;
                  b_red = b_green = b_blue = 0xffff;
                  break;
                case ATTR_TARGET_NOTCONVERTED:
                  f_red = f_green = f_blue = 0xffff;
                  b_red = b_green = b_blue = 0;
                  break;
                case ATTR_INPUT_ERROR:
                  f_red = f_green = f_blue = 0;
                  b_red = b_green = b_blue = 0x7fff;
                  break;
                default:        /* ATTR_INPUT,ATTR_CONVERTED,ATTR_FIXEDCONVERTED */
                  attr = pango_attr_underline_new (PANGO_UNDERLINE_SINGLE);
                  attr->start_index = sidx;
                  attr->end_index = eidx;
                  pango_attr_list_change (attrs, attr);
                  f_red = f_green = f_blue = 0;
                  b_red = b_green = b_blue = 0xffff;
                }
              attr = pango_attr_foreground_new (f_red, f_green, f_blue);
              attr->start_index = sidx;
              attr->end_index = eidx;
              pango_attr_list_change (attrs, attr);
              attr = pango_attr_background_new (b_red, b_green, b_blue);
              attr->start_index = sidx;
              attr->end_index = eidx;
              pango_attr_list_change (attrs, attr);

              schr = echr;
              spos = epos;
              sidx = eidx;
            }
        }
    }

  ImmReleaseContext (hwnd, himc);

  return attrs;
}


static void
gtk_im_context_ime_get_preedit_string (GtkIMContext   *context,
                                       gchar         **str,
                                       PangoAttrList **attrs,
                                       gint           *cursor_pos)
{
  gchar *utf8str = NULL;
  gint pos = 0;
  GtkIMContextIME *context_ime;

  context_ime = GTK_IM_CONTEXT_IME (context);

  utf8str = get_utf8_preedit_string (context_ime, &pos);

  if (attrs)
    *attrs = get_pango_attr_list (context_ime, utf8str);

  if (str)
    {
      *str = utf8str;
    }
  else
    {
      g_free (utf8str);
      utf8str = NULL;
    }

  if (cursor_pos)
    *cursor_pos = pos;
}


static void
gtk_im_context_ime_focus_in (GtkIMContext *context)
{
  GtkIMContextIME *context_ime = GTK_IM_CONTEXT_IME (context);
  GdkWindow *toplevel;
  GtkWidget *widget = NULL;
  HWND hwnd, top_hwnd;
  HIMC himc;

  if (!GDK_IS_WINDOW (context_ime->client_window))
    return;

  /* swtich current context */
  context_ime->focus = TRUE;

  hwnd = gdk_win32_window_get_impl_hwnd (context_ime->client_window);
  himc = ImmGetContext (hwnd);
  if (!himc)
    return;

  toplevel = gdk_window_get_toplevel (context_ime->client_window);
  if (GDK_IS_WINDOW (toplevel))
    {
      gdk_window_add_filter (toplevel,
                             gtk_im_context_ime_message_filter, context_ime);
      top_hwnd = gdk_win32_window_get_impl_hwnd (toplevel);

      context_ime->toplevel = toplevel;
    }
  else
    {
      g_warning ("gtk_im_context_ime_focus_in(): "
                 "cannot find toplevel window.");
      return;
    }

  /* trace reparenting (probably no need) */
  gdk_window_get_user_data (context_ime->client_window, (gpointer) & widget);
  if (GTK_IS_WIDGET (widget))
    {
      g_signal_connect (widget, "hierarchy-changed",
                        G_CALLBACK (cb_client_widget_hierarchy_changed),
                        context_ime);
    }
  else
    {
      /* warning? */
    }

  /* restore preedit context */
  ImmSetConversionStatus (himc,
                          context_ime->priv->conversion_mode,
                          context_ime->priv->sentence_mode);

  if (context_ime->opened)
    {
      if (!ImmGetOpenStatus (himc))
        ImmSetOpenStatus (himc, TRUE);
      if (context_ime->preediting)
        {
          ImmSetCompositionStringW (himc,
				    SCS_SETSTR,
				    context_ime->priv->comp_str,
				    context_ime->priv->comp_str_len, NULL, 0);
          FREE_PREEDIT_BUFFER (context_ime);
        }
    }

  /* clean */
  ImmReleaseContext (hwnd, himc);
}


static void
gtk_im_context_ime_focus_out (GtkIMContext *context)
{
  GtkIMContextIME *context_ime = GTK_IM_CONTEXT_IME (context);
  GdkWindow *toplevel;
  GtkWidget *widget = NULL;
  HWND hwnd, top_hwnd;
  HIMC himc;

  if (!GDK_IS_WINDOW (context_ime->client_window))
    return;

  /* swtich current context */
  context_ime->focus = FALSE;

  hwnd = gdk_win32_window_get_impl_hwnd (context_ime->client_window);
  himc = ImmGetContext (hwnd);
  if (!himc)
    return;

  /* save preedit context */
  ImmGetConversionStatus (himc,
                          &context_ime->priv->conversion_mode,
                          &context_ime->priv->sentence_mode);

  if (ImmGetOpenStatus (himc))
    {
      gboolean preediting = context_ime->preediting;

      if (preediting)
        {
          FREE_PREEDIT_BUFFER (context_ime);

          context_ime->priv->comp_str_len
            = ImmGetCompositionStringW (himc, GCS_COMPSTR, NULL, 0);
          context_ime->priv->comp_str
            = g_malloc (context_ime->priv->comp_str_len);
          ImmGetCompositionStringW (himc, GCS_COMPSTR,
				    context_ime->priv->comp_str,
				    context_ime->priv->comp_str_len);

          context_ime->priv->read_str_len
            = ImmGetCompositionStringW (himc, GCS_COMPREADSTR, NULL, 0);
          context_ime->priv->read_str
            = g_malloc (context_ime->priv->read_str_len);
          ImmGetCompositionStringW (himc, GCS_COMPREADSTR,
				    context_ime->priv->read_str,
				    context_ime->priv->read_str_len);
        }

      ImmSetOpenStatus (himc, FALSE);

      context_ime->opened = TRUE;
      context_ime->preediting = preediting;
    }
  else
    {
      context_ime->opened = FALSE;
      context_ime->preediting = FALSE;
    }

  /* remove signal handler */
  gdk_window_get_user_data (context_ime->client_window, (gpointer) & widget);
  if (GTK_IS_WIDGET (widget))
    {
      g_signal_handlers_disconnect_by_func
        (G_OBJECT (widget),
         G_CALLBACK (cb_client_widget_hierarchy_changed), context_ime);
    }

  /* remove event fileter */
  toplevel = gdk_window_get_toplevel (context_ime->client_window);
  if (GDK_IS_WINDOW (toplevel))
    {
      gdk_window_remove_filter (toplevel,
                                gtk_im_context_ime_message_filter,
                                context_ime);
      top_hwnd = gdk_win32_window_get_impl_hwnd (toplevel);

      context_ime->toplevel = NULL;
    }
  else
    {
      g_warning ("gtk_im_context_ime_focus_out(): "
                 "cannot find toplevel window.");
    }

  /* clean */
  ImmReleaseContext (hwnd, himc);
}


static void
gtk_im_context_ime_set_cursor_location (GtkIMContext *context,
                                        GdkRectangle *area)
{
  gint wx = 0, wy = 0;
  GtkIMContextIME *context_ime;
  COMPOSITIONFORM cf;
  HWND hwnd;
  HIMC himc;

  g_return_if_fail (GTK_IS_IM_CONTEXT_IME (context));

  context_ime = GTK_IM_CONTEXT_IME (context);
  if (area)
    context_ime->cursor_location = *area;

  if (!context_ime->client_window)
    return;

  hwnd = gdk_win32_window_get_impl_hwnd (context_ime->client_window);
  himc = ImmGetContext (hwnd);
  if (!himc)
    return;

  get_window_position (context_ime->client_window, &wx, &wy);
  cf.dwStyle = CFS_POINT;
  cf.ptCurrentPos.x = wx + context_ime->cursor_location.x;
  cf.ptCurrentPos.y = wy + context_ime->cursor_location.y;
  ImmSetCompositionWindow (himc, &cf);

  ImmReleaseContext (hwnd, himc);
}


static void
gtk_im_context_ime_set_use_preedit (GtkIMContext *context,
                                    gboolean      use_preedit)
{
  GtkIMContextIME *context_ime;

  g_return_if_fail (GTK_IS_IM_CONTEXT_IME (context));
  context_ime = GTK_IM_CONTEXT_IME (context);

  context_ime->use_preedit = use_preedit;
  if (context_ime->preediting)
    {
      HWND hwnd;
      HIMC himc;

      hwnd = gdk_win32_window_get_impl_hwnd (context_ime->client_window);
      himc = ImmGetContext (hwnd);
      if (!himc)
        return;

      /* FIXME: What to do? */

      ImmReleaseContext (hwnd, himc);
    }
}


static void
gtk_im_context_ime_set_preedit_font (GtkIMContext *context)
{
  GtkIMContextIME *context_ime;
  GtkWidget *widget = NULL;
  HWND hwnd;
  HIMC himc;
  HKL ime = GetKeyboardLayout (0);
  const gchar *lang;
  gunichar wc;
  PangoContext *pango_context;
  PangoFont *font;
  LOGFONT *logfont;
  GtkStyleContext *style;
  PangoFontDescription *font_desc;

  g_return_if_fail (GTK_IS_IM_CONTEXT_IME (context));

  context_ime = GTK_IM_CONTEXT_IME (context);
  if (!context_ime->client_window)
    return;

  gdk_window_get_user_data (context_ime->client_window, (gpointer) &widget);
  if (!GTK_IS_WIDGET (widget))
    return;

  hwnd = gdk_win32_window_get_impl_hwnd (context_ime->client_window);
  himc = ImmGetContext (hwnd);
  if (!himc)
    return;

  /* set font */
  pango_context = gtk_widget_get_pango_context (widget);
  if (!pango_context)
    goto ERROR_OUT;

  /* Try to make sure we use a font that actually can show the
   * language in question.
   */ 

  switch (PRIMARYLANGID (LOWORD (ime)))
    {
    case LANG_JAPANESE:
      lang = "ja"; break;
    case LANG_KOREAN:
      lang = "ko"; break;
    case LANG_CHINESE:
      switch (SUBLANGID (LOWORD (ime)))
	{
	case SUBLANG_CHINESE_TRADITIONAL:
	  lang = "zh_TW"; break;
	case SUBLANG_CHINESE_SIMPLIFIED:
	  lang = "zh_CN"; break;
	case SUBLANG_CHINESE_HONGKONG:
	  lang = "zh_HK"; break;
	case SUBLANG_CHINESE_SINGAPORE:
	  lang = "zh_SG"; break;
	case SUBLANG_CHINESE_MACAU:
	  lang = "zh_MO"; break;
	default:
	  lang = "zh"; break;
	}
      break;
    default:
      lang = ""; break;
    }

  style = gtk_widget_get_style_context (widget);
  gtk_style_context_get (style, GTK_STATE_FLAG_NORMAL, "font", &font_desc, NULL);
  
  if (lang[0])
    {
      /* We know what language it is. Look for a character, any
       * character, that language needs.
       */
      PangoLanguage *pango_lang = pango_language_from_string (lang);
      PangoFontset *fontset =
        pango_context_load_fontset (pango_context,
				                            font_desc,
				                            pango_lang);
      gunichar *sample =
	g_utf8_to_ucs4 (pango_language_get_sample_string (pango_lang),
			-1, NULL, NULL, NULL);
      wc = 0x4E00;		/* In all CJK languages? */
      if (sample != NULL)
	{
	  int i;

	  for (i = 0; sample[i]; i++)
	    if (g_unichar_iswide (sample[i]))
	      {
		wc = sample[i];
		break;
	      }
	  g_free (sample);
	}
      font = pango_fontset_get_font (fontset, wc);
      g_object_unref (fontset);
    }
  else
    font = pango_context_load_font (pango_context, font_desc);

  if (!font)
    goto ERROR_OUT;

  logfont = pango_win32_font_logfont (font);
  if (logfont)
    ImmSetCompositionFont (himc, logfont);

  g_object_unref (font);

ERROR_OUT:
  /* clean */
  ImmReleaseContext (hwnd, himc);
}


static GdkFilterReturn
gtk_im_context_ime_message_filter (GdkXEvent *xevent,
                                   GdkEvent  *event,
                                   gpointer   data)
{
  GtkIMContext *context;
  GtkIMContextIME *context_ime;
  HWND hwnd;
  HIMC himc;
  MSG *msg = (MSG *) xevent;
  GdkFilterReturn retval = GDK_FILTER_CONTINUE;

  g_return_val_if_fail (GTK_IS_IM_CONTEXT_IME (data), retval);

  context = GTK_IM_CONTEXT (data);
  context_ime = GTK_IM_CONTEXT_IME (data);
  if (!context_ime->focus)
    return retval;

  hwnd = gdk_win32_window_get_impl_hwnd (context_ime->client_window);
  himc = ImmGetContext (hwnd);
  if (!himc)
    return retval;

  switch (msg->message)
    {
    case WM_IME_COMPOSITION:
      {
        gint wx = 0, wy = 0;
        CANDIDATEFORM cf;

        get_window_position (context_ime->client_window, &wx, &wy);
        /* FIXME! */
        {
          HWND hwnd_top;
          POINT pt;
          RECT rc;

          hwnd_top =
            gdk_win32_window_get_impl_hwnd (gdk_window_get_toplevel
                                            (context_ime->client_window));
          GetWindowRect (hwnd_top, &rc);
          pt.x = wx;
          pt.y = wy;
          ClientToScreen (hwnd_top, &pt);
          wx = pt.x - rc.left;
          wy = pt.y - rc.top;
        }
        cf.dwIndex = 0;
        cf.dwStyle = CFS_CANDIDATEPOS;
        cf.ptCurrentPos.x = wx + context_ime->cursor_location.x;
        cf.ptCurrentPos.y = wy + context_ime->cursor_location.y
          + context_ime->cursor_location.height;
        ImmSetCandidateWindow (himc, &cf);

        if ((msg->lParam & GCS_COMPSTR))
          g_signal_emit_by_name (context, "preedit-changed");

        if (msg->lParam & GCS_RESULTSTR)
          {
            gsize len;
            gchar *utf8str = NULL;
            GError *error = NULL;

	    len = ImmGetCompositionStringW (himc, GCS_RESULTSTR, NULL, 0);

            if (len > 0)
              {
		gpointer buf = g_alloca (len);
		ImmGetCompositionStringW (himc, GCS_RESULTSTR, buf, len);
		len /= 2;
		utf8str = g_utf16_to_utf8 (buf, len, NULL, NULL, &error);
                if (error)
                  {
                    g_warning ("%s", error->message);
                    g_error_free (error);
                  }
              }

            if (utf8str)
              {
                g_signal_emit_by_name (context, "commit", utf8str);
                g_free (utf8str);
		retval = TRUE;
              }
          }

        if (context_ime->use_preedit)
          retval = TRUE;
        break;
      }

    case WM_IME_STARTCOMPOSITION:
      context_ime->preediting = TRUE;
      gtk_im_context_ime_set_cursor_location (context, NULL);
      g_signal_emit_by_name (context, "preedit-start");
      if (context_ime->use_preedit)
        retval = TRUE;
      break;

    case WM_IME_ENDCOMPOSITION:
      context_ime->preediting = FALSE;
      g_signal_emit_by_name (context, "preedit-changed");
      g_signal_emit_by_name (context, "preedit-end");
      if (context_ime->use_preedit)
        retval = TRUE;
      break;

    case WM_IME_NOTIFY:
      switch (msg->wParam)
        {
        case IMN_SETOPENSTATUS:
          context_ime->opened = ImmGetOpenStatus (himc);
          gtk_im_context_ime_set_preedit_font (context);
          break;

        default:
          break;
        }

    default:
      break;
    }

  ImmReleaseContext (hwnd, himc);
  return retval;
}


/*
 * x and y must be initialized to 0.
 */
static void
get_window_position (GdkWindow *win, gint *x, gint *y)
{
  GdkWindow *parent, *toplevel;
  gint wx, wy;

  g_return_if_fail (GDK_IS_WINDOW (win));
  g_return_if_fail (x && y);

  gdk_window_get_position (win, &wx, &wy);
  *x += wx;
  *y += wy;
  parent = gdk_window_get_parent (win);
  toplevel = gdk_window_get_toplevel (win);

  if (parent && parent != toplevel)
    get_window_position (parent, x, y);
}


/*
 *  probably, this handler isn't needed.
 */
static void
cb_client_widget_hierarchy_changed (GtkWidget       *widget,
                                    GtkWidget       *widget2,
                                    GtkIMContextIME *context_ime)
{
  GdkWindow *new_toplevel;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_IM_CONTEXT_IME (context_ime));

  if (!context_ime->client_window)
    return;
  if (!context_ime->focus)
    return;

  new_toplevel = gdk_window_get_toplevel (context_ime->client_window);
  if (context_ime->toplevel == new_toplevel)
    return;

  /* remove filter from old toplevel */
  if (GDK_IS_WINDOW (context_ime->toplevel))
    {
      gdk_window_remove_filter (context_ime->toplevel,
                                gtk_im_context_ime_message_filter,
                                context_ime);
    }
  else
    {
    }

  /* add filter to new toplevel */
  if (GDK_IS_WINDOW (new_toplevel))
    {
      gdk_window_add_filter (new_toplevel,
                             gtk_im_context_ime_message_filter, context_ime);
    }
  else
    {
    }

  context_ime->toplevel = new_toplevel;
}
