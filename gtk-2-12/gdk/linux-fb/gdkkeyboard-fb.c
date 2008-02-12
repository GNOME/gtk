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

#include <config.h>
#include <gdk/gdk.h>
#include <gdk/gdkinternals.h>
#include "gdkkeysyms.h"
#include "gdkprivate-fb.h"
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/kd.h>
#include <sys/vt.h>

typedef struct _GdkFBKeyboard GdkFBKeyboard;
typedef struct _GdkFBKeyboardDevice GdkFBKeyboardDevice;

struct _GdkFBKeyboard {
  gint fd;
  GIOChannel *io;
  gint io_tag;

  guint modifier_state;
  guint caps_lock : 1;

  gint group;
  gint level;
  
  GdkFBKeyboardDevice *dev;
};

struct _GdkFBKeyboardDevice {
  char *name;
  gboolean (*open)(GdkFBKeyboard *kb);
  void (*close)(GdkFBKeyboard *kb);

  guint    (*lookup_key)               (GdkFBKeyboard       *kb,
					const GdkKeymapKey  *key);
  gboolean (*translate_keyboard_state) (GdkFBKeyboard       *kb,
					guint                hardware_keycode,
					GdkModifierType      state,
					gint                 group,
					guint               *keyval,
					gint                *effective_group,
					gint                *level,
					GdkModifierType     *consumed_modifiers);
  gboolean (*get_entries_for_keyval)   (GdkFBKeyboard       *kb,
					guint                keyval,
					GdkKeymapKey       **keys,
					gint                *n_keys);
  gboolean (*get_entries_for_keycode)  (GdkFBKeyboard       *kb,
					guint                hardware_keycode,
					GdkKeymapKey       **keys,
					guint              **keyvals,
					gint                *n_entries);

  gpointer driver_data;
};

static GdkFBKeyboard *gdk_fb_keyboard = NULL;
static GdkKeymap *default_keymap = NULL;

static gboolean xlate_open            (GdkFBKeyboard       *kb);
static void     xlate_close           (GdkFBKeyboard       *kb);
static guint    xlate_lookup          (GdkFBKeyboard       *kb,
				       const GdkKeymapKey  *key);
static gboolean xlate_translate       (GdkFBKeyboard       *kb,
				       guint                hardware_keycode,
				       GdkModifierType      state,
				       gint                 group,
				       guint               *keyval,
				       gint                *effective_group,
				       gint                *level,
				       GdkModifierType     *consumed_modifiers);
static gboolean xlate_get_for_keyval  (GdkFBKeyboard       *kb,
				       guint                keyval,
				       GdkKeymapKey       **keys,
				       gint                *n_keys);
static gboolean xlate_get_for_keycode (GdkFBKeyboard       *kb,
				       guint                hardware_keycode,
				       GdkKeymapKey       **keys,
				       guint              **keyvals,
				       gint                *n_entries);

static gboolean raw_open            (GdkFBKeyboard       *kb);
static void     raw_close           (GdkFBKeyboard       *kb);
static guint    raw_lookup          (GdkFBKeyboard       *kb,
				     const GdkKeymapKey  *key);
static gboolean raw_translate       (GdkFBKeyboard       *kb,
				     guint                hardware_keycode,
				     GdkModifierType      state,
				     gint                 group,
				     guint               *keyval,
				     gint                *effective_group,
				     gint                *level,
				     GdkModifierType     *consumed_modifiers);
static gboolean raw_get_for_keyval  (GdkFBKeyboard       *kb,
				     guint                keyval,
				     GdkKeymapKey       **keys,
				     gint                *n_keys);
static gboolean raw_get_for_keycode (GdkFBKeyboard       *kb,
				     guint                hardware_keycode,
				     GdkKeymapKey       **keys,
				     guint              **keyvals,
				     gint                *n_entries);


static GdkFBKeyboardDevice keyb_devs[] =
{
  {
    "xlate",
    xlate_open,
    xlate_close,
    xlate_lookup,
    xlate_translate,
    xlate_get_for_keyval,
    xlate_get_for_keycode
  },
  {
    "raw",
    raw_open,
    raw_close,
    raw_lookup,
    raw_translate,
    raw_get_for_keyval,
    raw_get_for_keycode
  },
};

GdkKeymap*
gdk_keymap_get_default_for_display (GdkDisplay *display)
{
  if (default_keymap == NULL)
    default_keymap = g_object_new (gdk_keymap_get_type (), NULL);

  return default_keymap;
}

GdkKeymap*
gdk_keymap_get_for_display (GdkDisplay *display)
{
  return gdk_keymap_get_default_for_display (display);
}

PangoDirection
gdk_keymap_get_direction (GdkKeymap *keymap)
{
  /* FIXME: Only supports LTR keymaps at the moment */
  return PANGO_DIRECTION_LTR;
}

guint
gdk_fb_keyboard_modifiers (void)
{
  return gdk_fb_keyboard->modifier_state;
}

gboolean
gdk_fb_keyboard_init (gboolean open_dev)
{
  GdkFBKeyboard *keyb;
  char *keyb_type;
  int i;

  gdk_fb_keyboard = g_new0 (GdkFBKeyboard, 1);
  keyb = gdk_fb_keyboard;
  keyb->fd = -1;
  
  keyb_type = getenv ("GDK_KEYBOARD_TYPE");
  
  if (!keyb_type)
    keyb_type = "xlate";

  for (i = 0; i < G_N_ELEMENTS(keyb_devs); i++)
    {
      if (g_ascii_strcasecmp(keyb_type, keyb_devs[i].name)==0)
	break;
    }
  
  if (i == G_N_ELEMENTS(keyb_devs))
    {
      g_warning ("No keyboard driver of type %s found", keyb_type);
      return FALSE;
    }

  keyb->dev = &keyb_devs[i];

  if (open_dev)
    return gdk_fb_keyboard_open ();
  else
    return TRUE;
}

gboolean
gdk_fb_keyboard_open (void)
{
  GdkFBKeyboard *keyb;
  GdkFBKeyboardDevice *device;

  keyb = gdk_fb_keyboard;
  device = keyb->dev;

  if (!device->open(keyb))
    {
      g_warning ("Keyboard driver open failed");
      return FALSE;
    }

  return TRUE;
}

void 
gdk_fb_keyboard_close (void)
{
  gdk_fb_keyboard->dev->close(gdk_fb_keyboard);
}


gboolean
gdk_keymap_get_entries_for_keyval (GdkKeymap     *keymap,
                                   guint          keyval,
                                   GdkKeymapKey **keys,
                                   gint          *n_keys)
{
  g_return_val_if_fail (keymap == NULL || GDK_IS_KEYMAP (keymap), FALSE);
  g_return_val_if_fail (keys != NULL, FALSE);
  g_return_val_if_fail (n_keys != NULL, FALSE);
  g_return_val_if_fail (keyval != 0, FALSE);

  return gdk_fb_keyboard->dev->get_entries_for_keyval (gdk_fb_keyboard,
						       keyval,
						       keys,
						       n_keys);
}

gboolean
gdk_keymap_get_entries_for_keycode (GdkKeymap     *keymap,
                                    guint          hardware_keycode,
                                    GdkKeymapKey **keys,
                                    guint        **keyvals,
                                    gint          *n_entries)
{
  g_return_val_if_fail (keymap == NULL || GDK_IS_KEYMAP (keymap), FALSE);
  g_return_val_if_fail (n_entries != NULL, FALSE);
  
  return gdk_fb_keyboard->dev->get_entries_for_keycode (gdk_fb_keyboard,
							hardware_keycode,
							keys,
							keyvals,
							n_entries);
}


guint
gdk_keymap_lookup_key (GdkKeymap          *keymap,
                       const GdkKeymapKey *key)
{
  g_return_val_if_fail (keymap == NULL || GDK_IS_KEYMAP (keymap), 0);
  g_return_val_if_fail (key != NULL, 0);
  g_return_val_if_fail (key->group < 4, 0);

  return gdk_fb_keyboard->dev->lookup_key (gdk_fb_keyboard,  key);
}


gboolean
gdk_keymap_translate_keyboard_state (GdkKeymap       *keymap,
                                     guint            hardware_keycode,
                                     GdkModifierType  state,
                                     gint             group,
                                     guint           *keyval,
                                     gint            *effective_group,
                                     gint            *level,
                                     GdkModifierType *consumed_modifiers)
{
  g_return_val_if_fail (keymap == NULL || GDK_IS_KEYMAP (keymap), FALSE);
  g_return_val_if_fail (group < 4, FALSE);

  return gdk_fb_keyboard->dev->translate_keyboard_state (gdk_fb_keyboard,
							 hardware_keycode,
							 state,
							 group,
							 keyval,
							 effective_group,
							 level,
							 consumed_modifiers);
}

static void
gdk_fb_handle_key (guint hw_keycode,
		   guint keyval,
		   guint modifier_state,
		   guint8 group,
		   gchar *string,
		   gint string_length,
		   gboolean key_up)
{
  GdkWindow *win;
  GdkEvent *event;

  /* handle some magic keys */
  if (key_up &&
      (modifier_state & GDK_CONTROL_MASK) &&
      (modifier_state & GDK_MOD1_MASK))
    {
      if (keyval == GDK_BackSpace)
	{
	  ioctl (gdk_display->tty_fd, KDSKBMODE, K_XLATE);
	  exit (1);
	}
	  
      if (keyval == GDK_Return)
	gdk_fb_redraw_all ();
    }

  win = gdk_fb_keyboard_event_window (gdk_fb_window_find_focus (),
				      key_up ? GDK_KEY_RELEASE : GDK_KEY_PRESS);
  if (win)
    {
      event = gdk_event_make (win,
			      key_up ? GDK_KEY_RELEASE : GDK_KEY_PRESS,
			      TRUE);
    
      event->key.state = modifier_state;
      event->key.keyval = keyval;
      event->key.string = string;
      event->key.length = string_length;
      event->key.hardware_keycode = hw_keycode;
      event->key.group = group;
    }
  else
    g_free (string);
}

/******************************************************
 ********* Device specific keyboard code **************
 ******************************************************/


/* XLATE keyboard driver */

struct {
  char *str;
  guint code;
  guint modifier;
} xlate_codes[] =
{
  {"\x5b\x41", GDK_F1},
  {"\x5b\x42", GDK_F2},
  {"\x5b\x43", GDK_F3},
  {"\x5b\x44", GDK_F4},
  {"\x5b\x45", GDK_F5},
  {"\x31\x37\x7e", GDK_F6},
  {"\x31\x38\x7e", GDK_F7},
  {"\x31\x39\x7e", GDK_F8},
  {"\x32\x30\x7e", GDK_F9},
  {"\x32\x31\x7e", GDK_F10},
  {"\x32\x33\x7e", GDK_F11},
  {"\x32\x34\x7e", GDK_F12},
  
  {"\x32\x35\x7e", GDK_F1, GDK_SHIFT_MASK},
  {"\x32\x36\x7e", GDK_F2, GDK_SHIFT_MASK},
  {"\x32\x38\x7e", GDK_F3, GDK_SHIFT_MASK},
  {"\x32\x39\x7e", GDK_F4, GDK_SHIFT_MASK},
  {"\x33\x31\x7e", GDK_F5, GDK_SHIFT_MASK},
  {"\x33\x32\x7e", GDK_F6, GDK_SHIFT_MASK},
  {"\x33\x33\x7e", GDK_F7, GDK_SHIFT_MASK},
  {"\x33\x34\x7e", GDK_F8, GDK_SHIFT_MASK},

  {"\x31\x7e", GDK_Home},
  {"\x35\x7e", GDK_Page_Up},
  {"\x34\x7e", GDK_End},
  {"\x36\x7e", GDK_Page_Down},
  {"\x32\x7e", GDK_Insert},
  {"\x33\x7e", GDK_Delete},

  {"\x41", GDK_Up},
  {"\x44", GDK_Left},
  {"\x42", GDK_Down},
  {"\x43", GDK_Right},
  {"\x50", GDK_Break},
};

struct {
  guint code;
  guint modifier;
} xlate_chars[] =
{
  /* 0x00 */
  {'@', GDK_CONTROL_MASK},
  {'a', GDK_CONTROL_MASK},
  {'b', GDK_CONTROL_MASK},
  {'c', GDK_CONTROL_MASK},
  {'d', GDK_CONTROL_MASK},
  {'e', GDK_CONTROL_MASK},
  {'f', GDK_CONTROL_MASK},
  {'g', GDK_CONTROL_MASK},
  {'h', GDK_CONTROL_MASK},
  {GDK_Tab, 0},
  {'j', GDK_CONTROL_MASK},
  {'k', GDK_CONTROL_MASK},
  {'l', GDK_CONTROL_MASK},
  {GDK_Return, 0},
  {'n', GDK_CONTROL_MASK},
  {'o', GDK_CONTROL_MASK},
  /* 0x10 */
  {'p', GDK_CONTROL_MASK},
  {'q', GDK_CONTROL_MASK},
  {'r', GDK_CONTROL_MASK},
  {'s', GDK_CONTROL_MASK},
  {'t', GDK_CONTROL_MASK},
  {'u', GDK_CONTROL_MASK},
  {'v', GDK_CONTROL_MASK},
  {'w', GDK_CONTROL_MASK},
  {'x', GDK_CONTROL_MASK},
  {'y', GDK_CONTROL_MASK},
  {'z', GDK_CONTROL_MASK},
  {GDK_Escape, 0},
  {'\\', GDK_CONTROL_MASK},
  {']', GDK_CONTROL_MASK},
  {'^', GDK_CONTROL_MASK},
  {'_', GDK_CONTROL_MASK},
  /* 0x20 */
  {GDK_space, 0},
  {'!', 0},
  {'"', 0},
  {'#', 0},
  {'$', 0},
  {'%', 0},
  {'&', 0},
  {'\'', 0},
  {'(', 0},
  {')', 0},
  {'*', 0},
  {'+', 0},
  {',', 0},
  {'-', 0},
  {'.', 0},
  {'/', 0},
  /* 0x30 */
  {'0', 0},
  {'1', 0},
  {'2', 0},
  {'3', 0},
  {'4', 0},
  {'5', 0},
  {'6', 0},
  {'7', 0},
  {'8', 0},
  {'9', 0},
  {':', 0},
  {';', 0},
  {'<', 0},
  {'=', 0},
  {'>', 0},
  {'?', 0},
  /* 0x40 */
  {'@', 0},
  {'A', 0},
  {'B', 0},
  {'C', 0},
  {'D', 0},
  {'E', 0},
  {'F', 0},
  {'G', 0},
  {'H', 0},
  {'I', 0},
  {'J', 0},
  {'K', 0},
  {'L', 0},
  {'M', 0},
  {'N', 0},
  {'O', 0},
  /* 0x50 */
  {'P', 0},
  {'Q', 0},
  {'R', 0},
  {'S', 0},
  {'T', 0},
  {'U', 0},
  {'V', 0},
  {'W', 0},
  {'X', 0},
  {'Y', 0},
  {'Z', 0},
  {'[', 0},
  {'\\', 0},
  {']', 0},
  {'^', 0},
  {'_', 0},
  /* 0x60 */
  {'`', 0},
  {'a', 0},
  {'b', 0},
  {'c', 0},
  {'d', 0},
  {'e', 0},
  {'f', 0},
  {'g', 0},
  {'h', 0},
  {'i', 0},
  {'j', 0},
  {'k', 0},
  {'l', 0},
  {'m', 0},
  {'n', 0},
  {'o', 0},
  /* 0x70 */
  {'p', 0},
  {'q', 0},
  {'r', 0},
  {'s', 0},
  {'t', 0},
  {'u', 0},
  {'v', 0},
  {'w', 0},
  {'x', 0},
  {'y', 0},
  {'z', 0},
  {'{', 0},
  {'|', 0},
  {'}', 0},
  {'~', 0},
  {GDK_BackSpace, 0},
  /* 0x80 */
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  /* 0x90 */
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  /* 0xA0 */
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  /* 0xB0 */
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  /* 0xC0 */
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  /* 0xD0 */
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  /* 0xE0 */
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  /* 0xF0 */
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
};

static gboolean
iscode (char *code, char *str, int str_max)
{
  int i;

  for (i = 0; code[i] && (i < str_max); i++)
    {
      if (code[i] != str[i])
	return FALSE;
    }
  return (code[i] == 0);
}

static gboolean
xlate_io (GIOChannel *gioc,
	  GIOCondition cond,
	  gpointer data)
{
  GdkFBKeyboard *keyb = (GdkFBKeyboard *)data;
  guchar buf[128];
  guint keycode;
  guint modifier;
  gboolean handled;
  int i, n, j;
  int left;
  
  n = read (keyb->fd, buf, sizeof(buf));
  if (n <= 0)
    g_error ("Nothing from keyboard!");

  for (i = 0; i < n; )
    {
      handled = FALSE;
      modifier = 0;
      if ((buf[i] == 27) && (i+1 != n)) /* Escape */
	{
	  /* Esc is not the last char in buffer, interpret as code sequence */
	  if (buf[i+1] == '[')
	    {
	      i += 2;
	      left = n-i;
	      if (left <= 0)
		return TRUE;

	      for (j=0;j<G_N_ELEMENTS (xlate_codes);j++)
		{
		  if (iscode (xlate_codes[j].str, &buf[i], left))
		    {
		      /* Ctrl-Alt Return can't be pressed in the XLATE driver,
		       * use Shift F1 instead */
		      if ((xlate_codes[j].code == GDK_F1) &&
			  (xlate_codes[j].modifier & GDK_SHIFT_MASK))
			gdk_fb_redraw_all ();

		      if ((xlate_codes[j].code == GDK_F2) &&
			  (xlate_codes[j].modifier & GDK_SHIFT_MASK))
			{
			  static gint deg = 0;
			  deg = (deg + 1) % 4;
			    
			  gdk_fb_set_rotation (deg);
			}

		      if ((xlate_codes[j].code == GDK_F8) &&
			  (xlate_codes[j].modifier & GDK_SHIFT_MASK))
			exit (1);

		      
		      gdk_fb_handle_key (xlate_codes[j].code,
					 xlate_codes[j].code,
					 xlate_codes[j].modifier,
					 0,
					 NULL,
					 0,
					 FALSE);
		      gdk_fb_handle_key (xlate_codes[j].code,
					 xlate_codes[j].code,
					 xlate_codes[j].modifier,
					 0,
					 NULL,
					 0,
					 TRUE);
		      i += strlen (xlate_codes[j].str);
		      handled = TRUE;
		      break;
		    }
		}
	    }
	  else
	    {
	      /* Alt-key */
	      modifier |= GDK_MOD1_MASK;
	      i++;
	    }
	}
      if (!handled)
	{
	  char dummy[2];
	  gint len;
	  
	  keycode = xlate_chars[buf[i]].code;
	  if (keycode == 0)
	    keycode = buf[i];
	  modifier |= xlate_chars[buf[i]].modifier;
	  
	  dummy[0] = keycode;
	  dummy[1] = 0;

	  len = ((keycode < 255) && isprint (keycode)) ? 1 : 0;
	  gdk_fb_handle_key (keycode,
			     keycode,
			     modifier,
			     0,
			     (len)?g_strdup(dummy) : NULL,
			     len,
			     FALSE);
	  gdk_fb_handle_key (keycode,
			     keycode,
			     modifier,
			     0,
			     (len)?g_strdup(dummy) : NULL,
			     len,
			     TRUE);
	  i++;
	}
    }
  return TRUE;
}

static gboolean
write_string (gint         fd,
	      const gchar *str)
{
  gsize to_write = strlen (str);

  while (to_write > 0)
    {
      gssize count = write (fd, str, to_write);
      if (count < 0)
	{
	  if (errno != EINTR)
	    return FALSE;
	}
      else
	{
	  to_write -= count;
	  str += count;
	}
    }

  return TRUE;
}

static gboolean
xlate_open (GdkFBKeyboard *kb)
{
  const char cursoroff_str[] = "\033[?1;0;0c";
  struct termios ts;
  
  tcgetattr (gdk_display->tty_fd, &ts);
  ts.c_cc[VTIME] = 0;
  ts.c_cc[VMIN] = 1;
  ts.c_lflag &= ~(ICANON|ECHO|ISIG);
  ts.c_iflag = 0;
  tcsetattr (gdk_display->tty_fd, TCSAFLUSH, &ts);

  tcsetpgrp (gdk_display->tty_fd, getpgrp());

  write_string (gdk_display->tty_fd, cursoroff_str);
  
  ioctl (gdk_display->tty_fd, KDSKBMODE, K_XLATE);

  kb->fd = gdk_display->tty_fd;
  kb->io = g_io_channel_unix_new (kb->fd);
  kb->io_tag = g_io_add_watch (kb->io,
			       G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL,
			       xlate_io,
			       kb);
  
  return TRUE;
}

static void
xlate_close (GdkFBKeyboard *kb)
{
  struct termios ts;
  const char cursoron_str[] = "\033c\033[3g\033]R";

  write_string (gdk_display->tty_fd, cursoron_str);

  tcgetattr (gdk_display->tty_fd, &ts);
  ts.c_lflag |= (ICANON|ECHO|ISIG);
  tcsetattr (gdk_display->tty_fd, TCSAFLUSH, &ts);
  
  g_source_remove (kb->io_tag);
  g_io_channel_unref (kb->io);
  kb->fd = -1;
  /* don't close kb->fd, it is the tty from gdk_display */
}

static guint
xlate_lookup (GdkFBKeyboard       *kb,
	      const GdkKeymapKey  *key)
{
  if (key->group != 0)
    return 0;
  if (key->level != 0)
    return 0;
  return key->keycode;
}

static gboolean
xlate_translate (GdkFBKeyboard       *kb,
		 guint                hardware_keycode,
		 GdkModifierType      state,
		 gint                 group,
		 guint               *keyval,
		 gint                *effective_group,
		 gint                *level,
		 GdkModifierType     *consumed_modifiers)
{
  if (keyval)
    *keyval = hardware_keycode;
  if (effective_group)
    *effective_group = 0;
  if (level)
    *level = 0;
  if (consumed_modifiers)
    *consumed_modifiers = 0;
  return TRUE;
}

static gboolean
xlate_get_for_keyval (GdkFBKeyboard       *kb,
		      guint                keyval,
		      GdkKeymapKey       **keys,
		      gint                *n_keys)
{
  *n_keys = 1;
  *keys = g_new (GdkKeymapKey, 1);
  (*keys)->keycode = keyval;
  (*keys)->group = 0;
  (*keys)->level = 0;
  return TRUE;
}

static gboolean
xlate_get_for_keycode (GdkFBKeyboard       *kb,
		       guint                hardware_keycode,
		       GdkKeymapKey       **keys,
		       guint              **keyvals,
		       gint                *n_entries)
{
  if (keys)
    {
      *keys = g_new (GdkKeymapKey, 1);
      (*keys)->keycode = hardware_keycode;
      (*keys)->level = 0;
      (*keys)->group = 0;
    }
  if (keyvals)
    {
      *keyvals = g_new (guint, 1);
      **keyvals = hardware_keycode;
    }
  if (n_entries)
    *n_entries = 1;

  return TRUE;
}

/* Raw keyboard support */

static const guint trans_table[256][3] = {
  /* 0x00 */
  {0, 0, 0},
  {GDK_Escape, 0, 0},
  {'1', '!', 0},
  {'2', '@', 0},
  {'3', '#', 0},
  {'4', '$', 0},
  {'5', '%', 0},
  {'6', '^', 0},
  {'7', '&', 0},
  {'8', '*', 0},
  {'9', '(', 0},
  {'0', ')', 0},
  {'-', '_', 0},
  {'=', '+', 0},
  {GDK_BackSpace, 0, 0},
  {GDK_Tab, 0, 0},

  /* 0x10 */
  {'q', 'Q', 0},
  {'w', 'W', 0},
  {'e', 'E', 0},
  {'r', 'R', 0},
  {'t', 'T', 0},
  {'y', 'Y', 0},
  {'u', 'U', 0},
  {'i', 'I', 0},
  {'o', 'O', 0},
  {'p', 'P', 0},
  {'[', '{', 0},
  {']', '}', 0},
  {GDK_Return, 0, 0},
  {GDK_Control_L, 0, 0}, /* mod */
  {'a', 'A', 0},
  {'s', 'S', 0},

	/* 0x20 */
  {'d', 'D', 0},
  {'f', 'F', 0},
  {'g', 'G', 0},
  {'h', 'H', 0},
  {'j', 'J', 0},
  {'k', 'K', 0},
  {'l', 'L', 0},
  {';', ':', 0},
  {'\'', '"', 0},
  {'`', '~', 0},
  {GDK_Shift_L, 0, 0}, /* mod */
  {'\\', 0, 0},
  {'z', 0, 0},
  {'x', 0, 0},
  {'c', 0, 0},

  {'v', 'V', 0},

	/* 0x30 */
  {'b', 'B', 0},
  {'n', 'N', 0},
  {'m', 'M', 0},
  {',', 0, 0},
  {'.', 0, 0},
  {'/', 0, 0},
  {GDK_Shift_R, 0, 0}, /* mod */
  {GDK_KP_Multiply, 0, 0},
  {0, 0, 0},
  {GDK_space, 0, 0},
  {0, 0, 0},
  {GDK_F1, 0, 0},
  {GDK_F2, 0, 0},
  {GDK_F3, 0, 0},
  {GDK_F4, 0, 0},
  {GDK_F5, 0, 0},

	/* 0x40 */
  {GDK_F6, 0, 0},
  {GDK_F7, 0, 0},
  {GDK_F8, 0, 0},
  {GDK_F9, 0, 0},
  {GDK_F10, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {'7', 0, 0},
  {'8', 0, 0},
  {'9', 0, 0},
  {'-', 0, 0},
  {'4', 0, 0},
  {'5', 0, 0},
  {'6', 0, 0},
  {'+', 0, 0},
  {'1', 0, 0},

	/* 0x50 */
  {'2', 0, 0},
  {'3', 0, 0},
  {'0', 0, 0},
  {'.', 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {GDK_F11, 0, 0},
  {GDK_F12, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},

	/* 0x60 */
  {GDK_Return, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {GDK_Home, 0, 0},
  {GDK_Up, 0, 0},
  {GDK_Page_Up, 0, 0},
  {GDK_Left, 0, 0},
  {GDK_Right, 0, 0},
  {GDK_End, 0, 0},
  {GDK_Down, 0, 0},
  {GDK_Page_Down, 0, 0},
  {GDK_Insert, 0, 0},
  {GDK_Delete, 0, 0},

	/* 0x70 */
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},

	/* 0x80 */
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},

	/* 0x90 */
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},

	/* 0xA0 */
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},

	/* 0xB0 */
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},

	/* 0xC0 */
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {GDK_Up, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {GDK_Left, 0, 0},
  {0, 0, 0},
  {GDK_Right, 0, 0},
  {0, 0, 0},
  {0, 0, 0},

	/* 0xD0 */
  {GDK_Down, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},

	/* 0xE0 */
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},

	/* 0xF0 */
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
};

static gboolean
raw_io (GIOChannel *gioc,
	GIOCondition cond,
	gpointer data)
{
  GdkFBKeyboard *k = data;
  guchar buf[128];
  int i, n;

  n = read (k->fd, buf, sizeof(buf));
  if (n <= 0)
    g_error("Nothing from keyboard!");

  for (i = 0; i < n; i++)
    {
      guchar keycode;
      gboolean key_up;
      char dummy[2];
      int len;
      int mod;
      guint keyval;

      keycode = buf[i] & 0x7F;
      key_up = buf[i] & 0x80;

      if (keycode > G_N_ELEMENTS (trans_table))
	{
	  g_warning ("Unknown keycode");
	  continue;
	}

      if ( (keycode == 0x1D) /* left Ctrl */
	   || (keycode == 0x9D) /* right Ctrl */
	   || (keycode == 0x38) /* left Alt */
	   || (keycode == 0xB8) /* right Alt */
	   || (keycode == 0x2A) /* left Shift */
	   || (keycode == 0x36) /* right Shift */)
	{
	  switch (keycode)
	    {
	    case 0x1D: /* Left Ctrl */
	    case 0x9D: /* Right Ctrl */
	      if (key_up)
		k->modifier_state &= ~GDK_CONTROL_MASK;
	      else
		k->modifier_state |= GDK_CONTROL_MASK;
	      break;
	    case 0x38: /* Left Alt */
	    case 0xB8: /* Right Alt */
	      if (key_up)
		k->modifier_state &= ~GDK_MOD1_MASK;
	      else
		k->modifier_state |= GDK_MOD1_MASK;
	      break;
	    case 0x2A: /* Left Shift */
	    case 0x36: /* Right Shift */
	      if (key_up)
		k->modifier_state &= ~GDK_SHIFT_MASK;
	      else
		k->modifier_state |= GDK_SHIFT_MASK;
	      break;
	    }
	  continue; /* Don't generate events for modifiers */
	}

      if (keycode == 0x3A /* Caps lock */)
	{
	  if (!key_up)
	    k->caps_lock = !k->caps_lock;
	  
	  ioctl (k->fd, KDSETLED, k->caps_lock ? LED_CAP : 0);
	  continue;
	}

      if (trans_table[keycode][0] >= GDK_F1 &&
	  trans_table[keycode][0] <= GDK_F35 &&
	  (k->modifier_state & GDK_MOD1_MASK))
	{
	  if (key_up) /* Only switch on release */
	    {
	      gint vtnum = trans_table[keycode][0] - GDK_F1 + 1;

	      /* Do the whole funky VT switch thing */
	      ioctl (gdk_display->console_fd, VT_ACTIVATE, vtnum);
	    }

	  continue;
	}

      keyval = 0;
      mod = 0;
      if (k->modifier_state & GDK_CONTROL_MASK)
	mod = 2;
      else if (k->modifier_state & GDK_SHIFT_MASK)
	mod = 1;
      do {
	keyval = trans_table[keycode][mod--];
      } while (!keyval && (mod >= 0));

      if (k->caps_lock && (keyval >= 'a') && (keyval <= 'z'))
	keyval = toupper (keyval);

      if (!keyval)
	continue;

      len = isprint (keyval) ? 1 : 0;
      dummy[0] = keyval;
      dummy[1] = 0;

      gdk_fb_handle_key (keycode,
			 keyval,
			 k->modifier_state,
			 0,
			 (len)?g_strdup(dummy):NULL,
			 len,
			 key_up);
    }

  return TRUE;
}

static gboolean
raw_open (GdkFBKeyboard *kb)
{
  const char cursoroff_str[] = "\033[?1;0;0c";
  struct termios ts;
  
  tcgetattr (gdk_display->tty_fd, &ts);
  ts.c_cc[VTIME] = 0;
  ts.c_cc[VMIN] = 1;
  ts.c_lflag &= ~(ICANON|ECHO|ISIG);
  ts.c_iflag = 0;
  tcsetattr (gdk_display->tty_fd, TCSAFLUSH, &ts);

  tcsetpgrp (gdk_display->tty_fd, getpgrp());

  write_string (gdk_display->tty_fd, cursoroff_str);
  
  if (ioctl (gdk_display->tty_fd, KDSKBMODE, K_MEDIUMRAW) < 0)
    {
      g_warning ("setting tty to K_MEDIUMRAW failed (are you root?)");
      return FALSE;
    }

  kb->fd = gdk_display->tty_fd;
  kb->io = g_io_channel_unix_new (kb->fd);
  kb->io_tag = g_io_add_watch (kb->io,
			       G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL,
			       raw_io,
			       kb);
  
  return TRUE;
}

static void
raw_close (GdkFBKeyboard *kb)
{
  struct termios ts;
  const char cursoron_str[] = "\033c";

  write_string (gdk_display->tty_fd, cursoron_str);

  tcgetattr (gdk_display->tty_fd, &ts);
  ts.c_lflag |= (ICANON|ECHO|ISIG);
  tcsetattr (gdk_display->tty_fd, TCSAFLUSH, &ts);
  
  ioctl (gdk_display->tty_fd, KDSKBMODE, K_XLATE);

  g_source_remove (kb->io_tag);
  g_io_channel_unref (kb->io);
  /* don't close kb->fd, it is the tty from gdk_display */
}

static guint
raw_lookup (GdkFBKeyboard       *kb,
	    const GdkKeymapKey  *key)
{
  if (key->group != 0)
    return 0;
  if ((key->keycode < 0) || (key->keycode >= 256))
    return 0;
  if ((key->level < 0) || (key->level >= 3))
    return 0;
  return trans_table[key->keycode][key->level];
}

static gboolean
raw_translate (GdkFBKeyboard       *kb,
	       guint                hardware_keycode,
	       GdkModifierType      state,
	       gint                 group,
	       guint               *keyval,
	       gint                *effective_group,
	       gint                *level,
	       GdkModifierType     *consumed_modifiers)
{
  guint tmp_keyval;
  gint tmp_level;

  if (keyval)
    *keyval = 0;
  if (effective_group)
    *effective_group = 0;
  if (level)
    *level = 0;
  if (consumed_modifiers)
    *consumed_modifiers = 0;

  if ((hardware_keycode < 0) || (hardware_keycode >= 256))
    return FALSE;

  if (group != 0)
    return FALSE;

  if (state & GDK_CONTROL_MASK)
    tmp_level = 2;
  else if (state & GDK_SHIFT_MASK)
    tmp_level = 1;
  else
    tmp_level = 0;

  do {
    tmp_keyval = trans_table[hardware_keycode][tmp_level --];
  } while (!tmp_keyval && (tmp_level >= 0));

  if (keyval)
    *keyval = tmp_keyval;
  if (level)
    *level = tmp_level;

  return TRUE;
}

static gboolean
raw_get_for_keyval (GdkFBKeyboard       *kb,
		    guint                keyval,
		    GdkKeymapKey       **keys,
		    gint                *n_keys)
{
  GArray *retval;
  int i, j;

  retval = g_array_new (FALSE, FALSE, sizeof (GdkKeymapKey));

  for (i = 0; i < 256; i++)
    for (j = 0; j < 3; j++)
      if (trans_table[i][j] == keyval)
	{
	  GdkKeymapKey key;

	  key.keycode = i;
	  key.group = 0;
	  key.level = j;

	  g_array_append_val (retval, key);
	}

  if (retval->len > 0)
    {
      *keys = (GdkKeymapKey*) retval->data;
      *n_keys = retval->len;
    }
  else
    {
      *keys = NULL;
      *n_keys = 0;
    }

  g_array_free (retval, retval->len > 0 ? FALSE : TRUE);

  return *n_keys > 0;
}

static gboolean
raw_get_for_keycode (GdkFBKeyboard       *kb,
		     guint                hardware_keycode,
		     GdkKeymapKey       **keys,
		     guint              **keyvals,
		     gint                *n_entries)
{
  GArray *key_array;
  GArray *keyval_array;
  int i;

  if (hardware_keycode <= 0 ||
      hardware_keycode >= 256)
    { 
      if (keys)
        *keys = NULL;
      if (keyvals)
        *keyvals = NULL;

      *n_entries = 0;
      return FALSE;
    }

  if (keys)
    key_array = g_array_new (FALSE, FALSE, sizeof (GdkKeymapKey));
  else
    key_array = NULL;

  if (keyvals)
    keyval_array = g_array_new (FALSE, FALSE, sizeof (guint));
  else
    keyval_array = NULL;

  for (i = 0; i < 3; i++)
    {
      if (key_array)
	{
	  GdkKeymapKey key;

	  key.keycode = hardware_keycode;
	  key.group = 0;
	  key.level = i;

	  g_array_append_val (key_array, key);
	}

      if (keyval_array)
	{
	  g_array_append_val (keyval_array, trans_table[hardware_keycode][i]);
	}
    }

  if ((key_array && key_array->len > 0) ||
      (keyval_array && keyval_array->len > 0))
    { 
      if (keys)
        *keys = (GdkKeymapKey*) key_array->data;

      if (keyvals)
        *keyvals = (guint*) keyval_array->data;

      if (key_array)
        *n_entries = key_array->len;
      else
        *n_entries = keyval_array->len;
    }
  else
    { 
      if (keys)
        *keys = NULL;

      if (keyvals)
        *keyvals = NULL;

      *n_entries = 0;
    }

  if (key_array)
    g_array_free (key_array, key_array->len > 0 ? FALSE : TRUE);
  if (keyval_array)
    g_array_free (keyval_array, keyval_array->len > 0 ? FALSE : TRUE);

  return *n_entries > 0;
}
