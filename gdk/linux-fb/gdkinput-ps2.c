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

#include <gdk/gdk.h>
#include <gdk/gdkinternals.h>
#include "gdkinputprivate.h"
#include "gdkkeysyms.h"
#include "gdkprivate-fb.h"
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/vt.h>
#include <sys/time.h>
#include <sys/kd.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <linux/fb.h>

/* Two minutes */
#define BLANKING_TIMEOUT 120*1000

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

typedef struct {
  gint fd, fd_tag, consfd;

  int vtnum, prev_vtnum;
  guint modifier_state;
  gboolean caps_lock : 1;
} Keyboard;

static guint blanking_timer = 0;

static Keyboard * tty_keyboard_open(void);

static Keyboard *keyboard = NULL;

#ifndef VESA_NO_BLANKING
#define VESA_NO_BLANKING        0
#define VESA_VSYNC_SUSPEND      1
#define VESA_HSYNC_SUSPEND      2
#define VESA_POWERDOWN          3
#endif

#if 0
static gboolean
input_activity_timeout(gpointer p)
{
  blanking_timer = 0;
  ioctl(gdk_display->fd, FBIOBLANK, VESA_POWERDOWN);
  return FALSE;
}
#endif

/* This is all very broken :( */
static void
input_activity (void)
{
#if 0
  if (blanking_timer)
    g_source_remove (blanking_timer);
  else
    gdk_fb_redraw_all ();

  blanking_timer = g_timeout_add (BLANKING_TIMEOUT, input_activity_timeout, NULL);
#endif
}

void
gdk_input_init (void)
{
  gdk_input_devices = g_list_append (NULL, gdk_core_pointer);

  gdk_input_ignore_core = FALSE;

  gdk_fb_mouse_open ();
}

GdkWindow *
gdk_window_find_focus (void)
{
  if (_gdk_fb_keyboard_grab_window)
    return _gdk_fb_keyboard_grab_window;
  else if (GDK_WINDOW_P (gdk_parent_root)->children)
    {
      GList *item;
      for (item = GDK_WINDOW_P (gdk_parent_root)->children; item; item = item->next)
	{
	  GdkWindowObject *priv = item->data;

	  if (priv->mapped)
	    return item->data;
	}
    }

  return gdk_parent_root;
}

/* Bogus implementation */
gboolean
gdk_keymap_get_entries_for_keycode (GdkKeymap     *keymap,
                                    guint          hardware_keycode,
                                    GdkKeymapKey **keys,
                                    guint        **keyvals,
                                    gint          *n_entries)
{
  return FALSE;
}

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

#define TRANS_TABLE_SIZE (sizeof(trans_table)/sizeof(trans_table[0]))

static gboolean
handle_mediumraw_keyboard_input (GIOChannel *gioc,
				 GIOCondition cond,
				 gpointer data)
{
  guchar buf[128];
  int i, n;
  Keyboard *k = data;
  guint32 now;

  n = read (k->fd, buf, sizeof(buf));
  if (n <= 0)
    g_error("Nothing from keyboard!");

  /* Now turn this into a keyboard event */
  now = gdk_fb_get_time ();

  for (i = 0; i < n; i++)
    {
      guchar keycode;
      gboolean key_up;
      GdkEvent *event;
      GdkWindow *win;
      char dummy[2];
      int mod;
      guint keyval;

      keycode = buf[i] & 0x7F;
      key_up = buf[i] & 0x80;

      if (keycode > TRANS_TABLE_SIZE)
	{
	  g_warning ("Unknown keycode\n");
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
	      ioctl (k->consfd, VT_ACTIVATE, vtnum);
	      ioctl (k->consfd, VT_WAITACTIVE, k->vtnum);
	      gdk_fb_redraw_all ();
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

      /* handle some magic keys */
      if (k->modifier_state & (GDK_CONTROL_MASK|GDK_MOD1_MASK))
	{
	  if (key_up)
	    {
	      if (keyval == GDK_BackSpace)
		exit (1);

	      if (keyval == GDK_Return)
		gdk_fb_redraw_all ();
	    }

	  keyval = 0;
	}

      if (!keyval)
	continue;

      win = gdk_window_find_focus ();
      event = gdk_event_make (win,
			      key_up ? GDK_KEY_RELEASE : GDK_KEY_PRESS,
			      TRUE);
      if (event)
	{
	  /* Find focused window */
	  event->key.time = now;
	  event->key.state = k->modifier_state;
	  event->key.keyval = keyval;
	  event->key.length = isprint (event->key.keyval) ? 1 : 0;
	  dummy[0] = event->key.keyval;
	  dummy[1] = 0;
	  event->key.string = event->key.length ? g_strdup(dummy) : NULL;
	}
    }

  input_activity ();

  return TRUE;
}

static gboolean
handle_xlate_keyboard_input (GIOChannel *gioc,
			     GIOCondition cond,
			     gpointer data)
{
  guchar buf[128];
  int i, n;
  Keyboard *k = data;
  guint32 now;

  n = read (k->fd, buf, sizeof(buf));
  if (n <= 0)
    g_error ("Nothing from keyboard!");

  /* Now turn this into a keyboard event */
  now = gdk_fb_get_time ();

  for (i = 0; i < n; i++)
    {
      GdkEvent *event;
      GdkWindow *win;
      char dummy[2];
      guint keyval;

      keyval = buf[i];

      switch (keyval) {
      case '\n':
	keyval = GDK_Return;
	break;
      case '\t':
	keyval = GDK_Tab;
	break;
      case 127:
	keyval = GDK_BackSpace;
	break;
      case 27:
	keyval = GDK_Escape;
	break;
      }

      win = gdk_window_find_focus ();
      
      /* Send key down: */
      event = gdk_event_make (win, GDK_KEY_PRESS, TRUE);
      if (event)
	{
	  /* Find focused window */
	  event->key.time = now;
	  event->key.state = k->modifier_state;
	  event->key.keyval = keyval;
	  event->key.length = isprint (event->key.keyval) ? 1 : 0;
	  dummy[0] = event->key.keyval;
	  dummy[1] = 0;
	  event->key.string = event->key.length ? g_strdup(dummy) : NULL;
	}

      /* Send key up: */
      event = gdk_event_make (win, GDK_KEY_RELEASE, TRUE);
      if (event)
	{
	  /* Find focused window */
	  event->key.time = now;
	  event->key.state = k->modifier_state;
	  event->key.keyval = keyval;
	  event->key.length = isprint (event->key.keyval) ? 1 : 0;
	  dummy[0] = event->key.keyval;
	  dummy[1] = 0;
	  event->key.string = event->key.length ? g_strdup(dummy) : NULL;
	}
    }

  input_activity ();

  return TRUE;
}


static Keyboard *
tty_keyboard_open (void)
{
  Keyboard *retval = g_new0 (Keyboard, 1);
  GIOChannel *gioc;
  const char cursoroff_str[] = "\033[?1;0;0c";
  int n;
  struct vt_stat vs;
  char buf[32];
  struct termios ts;
  gboolean raw_keyboard;

  retval->modifier_state = 0;
  retval->caps_lock = 0;
  
  setsid();
  retval->consfd = open ("/dev/console", O_RDWR);
  ioctl (retval->consfd, VT_GETSTATE, &vs);
  retval->prev_vtnum = vs.v_active;
  g_snprintf (buf, sizeof(buf), "/dev/tty%d", retval->prev_vtnum);
  ioctl (retval->consfd, KDSKBMODE, K_XLATE);

  n = ioctl (retval->consfd, VT_OPENQRY, &retval->vtnum);
  if (n < 0 || retval->vtnum == -1)
    g_error("Cannot allocate VT");

  ioctl (retval->consfd, VT_ACTIVATE, retval->vtnum);
  ioctl (retval->consfd, VT_WAITACTIVE, retval->vtnum);

#if 0
  close (0);
  close (1);
  close (2);
#endif
  
  g_snprintf (buf, sizeof(buf), "/dev/tty%d", retval->vtnum);
  retval->fd = open (buf, O_RDWR|O_NONBLOCK);
  if (retval->fd < 0)
    return NULL;
  raw_keyboard = TRUE;
  if (ioctl (retval->fd, KDSKBMODE, K_MEDIUMRAW) < 0)
    {
      raw_keyboard = FALSE;
      g_warning ("K_MEDIUMRAW failed, using broken XLATE keyboard driver");
    }

  /* Disable normal text on the console */
  ioctl (retval->fd, KDSETMODE, KD_GRAPHICS);

  /* Set controlling tty */
  ioctl (0, TIOCNOTTY, 0);
  ioctl (retval->fd, TIOCSCTTY, 0);
  tcgetattr (retval->fd, &ts);
  ts.c_cc[VTIME] = 0;
  ts.c_cc[VMIN] = 1;
  ts.c_lflag &= ~(ICANON|ECHO|ISIG);
  ts.c_iflag = 0;
  tcsetattr (retval->fd, TCSAFLUSH, &ts);

  tcsetpgrp (retval->fd, getpgrp());

  write (retval->fd, cursoroff_str, strlen (cursoroff_str));

#if 0
  if (retval->fd != 0)
    dup2 (retval->fd, 0);
  if (retval->fd != 1)
    dup2 (retval->fd, 1);
  if (retval->fd != 2)
    dup2 (retval->fd, 2);
#endif

  gioc = g_io_channel_unix_new (retval->fd);
  retval->fd_tag = g_io_add_watch (gioc,
				   G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL,
				   (raw_keyboard) ? handle_mediumraw_keyboard_input : handle_xlate_keyboard_input,
				   retval);

  return retval;
}

void
gdk_beep (void)
{
  static int pitch = 600, duration = 100;
  gulong arg;

  if (!keyboard)
    return;

  /* Thank you XFree86 */
  arg = ((1193190 / pitch) & 0xffff) |
    (((unsigned long)duration) << 16);

  ioctl (keyboard->fd, KDMKTONE, arg);
}

void
keyboard_init (void)
{
  keyboard = tty_keyboard_open ();
}

void
keyboard_shutdown (void)
{
  int tmpfd;

  ioctl (keyboard->fd, KDSETMODE, KD_TEXT);
  ioctl (keyboard->fd, KDSKBMODE, K_XLATE);
  close (keyboard->fd);
  g_source_remove (keyboard->fd_tag);

  tmpfd = keyboard->consfd;
  ioctl (tmpfd, VT_ACTIVATE, keyboard->prev_vtnum);
  ioctl (tmpfd, VT_WAITACTIVE, keyboard->prev_vtnum);
  ioctl (tmpfd, VT_DISALLOCATE, keyboard->vtnum);
  close (tmpfd);

  g_free (keyboard);
  keyboard = NULL;
}


