/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <X11/Xlocale.h>	/* so we get the right setlocale */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>		/* For getuid() and friends */
#include <gmodule.h>
#include "gtkbutton.h"
#include "gtkdnd.h"
#include "gtkfeatures.h"
#include "gtkhscrollbar.h"
#include "gtkhseparator.h"
#include "gtkmain.h"
#include "gtkpreview.h"
#include "gtkrc.h"
#include "gtkscrolledwindow.h"
#include "gtkselection.h"
#include "gtksignal.h"
#include "gtktable.h"
#include "gtktext.h"
#include "gtkvbox.h"
#include "gtkvscrollbar.h"
#include "gtkwidget.h"
#include "gtkwindow.h"
#include "gtkprivate.h"
#include "gdk/gdki18n.h"
#include "config.h"
#include "gtkdebug.h"
#include "gtkintl.h"

/* Private type definitions
 */
typedef struct _GtkInitFunction		 GtkInitFunction;
typedef struct _GtkQuitFunction		 GtkQuitFunction;
typedef struct _GtkClosure	         GtkClosure;
typedef struct _GtkKeySnooperData	 GtkKeySnooperData;

struct _GtkInitFunction
{
  GtkFunction function;
  gpointer data;
};

struct _GtkQuitFunction
{
  guint id;
  guint main_level;
  GtkCallbackMarshal marshal;
  GtkFunction function;
  gpointer data;
  GtkDestroyNotify destroy;
};

struct _GtkClosure
{
  GtkCallbackMarshal marshal;
  gpointer data;
  GtkDestroyNotify destroy;
};

struct _GtkKeySnooperData
{
  GtkKeySnoopFunc func;
  gpointer func_data;
  guint id;
};

static void  gtk_exit_func		 (void);
static gint  gtk_quit_invoke_function	 (GtkQuitFunction    *quitf);
static void  gtk_quit_destroy		 (GtkQuitFunction    *quitf);
static gint  gtk_invoke_key_snoopers	 (GtkWidget	     *grab_widget,
					  GdkEvent	     *event);

static void     gtk_destroy_closure      (gpointer            data);
static gboolean gtk_invoke_idle_timeout  (gpointer            data);
static void     gtk_invoke_input         (gpointer            data,
					  gint                source,
					  GdkInputCondition   condition);

#if 0
static void  gtk_error			 (gchar		     *str);
static void  gtk_warning		 (gchar		     *str);
static void  gtk_message		 (gchar		     *str);
static void  gtk_print			 (gchar		     *str);
#endif

const guint gtk_major_version = GTK_MAJOR_VERSION;
const guint gtk_minor_version = GTK_MINOR_VERSION;
const guint gtk_micro_version = GTK_MICRO_VERSION;
const guint gtk_binary_age = GTK_BINARY_AGE;
const guint gtk_interface_age = GTK_INTERFACE_AGE;

static guint gtk_main_loop_level = 0;
static gint gtk_initialized = FALSE;
static GList *current_events = NULL;

static GSList *main_loops = NULL;      /* stack of currently executing main loops */

static GSList *grabs = NULL;		   /* A stack of unique grabs. The grabbing
					    *  widget is the first one on the list.
					    */
static GList *init_functions = NULL;	   /* A list of init functions.
					    */
static GList *quit_functions = NULL;	   /* A list of quit functions.
					    */
static GMemChunk *quit_mem_chunk = NULL;

static GSList *key_snoopers = NULL;

static GdkVisual *gtk_visual;		   /* The visual to be used in creating new
					    *  widgets.
					    */
static GdkColormap *gtk_colormap;	   /* The colormap to be used in creating new
					    *  widgets.
					    */

guint gtk_debug_flags = 0;		   /* Global GTK debug flag */

#ifdef G_ENABLE_DEBUG
static const GDebugKey gtk_debug_keys[] = {
  {"objects", GTK_DEBUG_OBJECTS},
  {"misc", GTK_DEBUG_MISC},
  {"signals", GTK_DEBUG_SIGNALS},
  {"dnd", GTK_DEBUG_DND},
  {"plugsocket", GTK_DEBUG_PLUGSOCKET}
};

static const guint gtk_ndebug_keys = sizeof (gtk_debug_keys) / sizeof (GDebugKey);

#endif /* G_ENABLE_DEBUG */

gchar*
gtk_check_version (guint required_major,
		   guint required_minor,
		   guint required_micro)
{
  if (required_major > GTK_MAJOR_VERSION)
    return "Gtk+ version too old (major mismatch)";
  if (required_major < GTK_MAJOR_VERSION)
    return "Gtk+ version too new (major mismatch)";
  if (required_minor > GTK_MINOR_VERSION)
    return "Gtk+ version too old (minor mismatch)";
  if (required_minor < GTK_MINOR_VERSION)
    return "Gtk+ version too new (minor mismatch)";
  if (required_micro < GTK_MICRO_VERSION - GTK_BINARY_AGE)
    return "Gtk+ version too new (micro mismatch)";
  if (required_micro > GTK_MICRO_VERSION)
    return "Gtk+ version too old (micro mismatch)";
  return NULL;
}

/* This checks to see if the process is running suid or sgid
 * at the current time. If so, we don't allow GTK+ to be initialized.
 * This is meant to be a mild check - we only error out if we
 * can prove the programmer is doing something wrong, not if
 * they could be doing something wrong. For this reason, we
 * don't use issetugid() on BSD or prctl (PR_GET_DUMPABLE).
 */
static gboolean
check_setugid (void)
{
  uid_t ruid, euid, suid; /* Real, effective and saved user ID's */
  gid_t rgid, egid, sgid; /* Real, effective and saved group ID's */
  
#ifdef HAVE_GETRESUID
  /* These aren't in the header files, so we prototype them here.
   */
  int getresuid(uid_t *ruid, uid_t *euid, uid_t *suid);
  int getresgid(gid_t *rgid, gid_t *egid, gid_t *sgid);

  if (getresuid (&ruid, &euid, &suid) != 0 ||
      getresgid (&rgid, &egid, &sgid) != 0)
#endif /* HAVE_GETRESUID */
    {
      suid = ruid = getuid ();
      sgid = rgid = getgid ();
      euid = geteuid ();
      egid = getegid ();
    }

  if (ruid != euid || ruid != suid ||
      rgid != egid || rgid != sgid)
    {
      g_warning ("This process is currently running setuid or setgid.\n"
		 "This is not a supported use of GTK+. You must create a helper\n"
		 "program instead. For further details, see:\n\n"
		 "    http://www.gtk.org/setuid.html\n\n"
		 "Refusing to initialize GTK+.");
      exit (1);
    }

  return TRUE;
}

gboolean
gtk_init_check (int	 *argc,
		char   ***argv)
{
  extern void gtk_object_post_arg_parsing_init (void);
  GSList *gtk_modules = NULL;
  GSList *slist;
  gchar *env_string = NULL;

  if (gtk_initialized)
    return TRUE;

  if (!check_setugid ())
    return FALSE;
  
#if	0
  g_set_error_handler (gtk_error);
  g_set_warning_handler (gtk_warning);
  g_set_message_handler (gtk_message);
  g_set_print_handler (gtk_print);
#endif
  
  /* Initialize "gdk". We pass along the 'argc' and 'argv'
   *  parameters as they contain information that GDK uses
   */
  if (!gdk_init_check (argc, argv))
    return FALSE;

  gdk_event_handler_set ((GdkEventFunc)gtk_main_do_event, NULL, NULL);
  
#ifdef G_ENABLE_DEBUG
  env_string = getenv ("GTK_DEBUG");
  if (env_string != NULL)
    {
      gtk_debug_flags = g_parse_debug_string (env_string,
					      gtk_debug_keys,
					      gtk_ndebug_keys);
      env_string = NULL;
    }
#endif	/* G_ENABLE_DEBUG */

  env_string = getenv ("GTK_MODULES");
  if (env_string)
    {
      gchar **modules, **as;

      modules = g_strsplit (env_string, ":", -1);
      for (as = modules; *as; as++)
	{
	  if (**as)
	    gtk_modules = g_slist_prepend (gtk_modules, *as);
	  else
	    g_free (*as);
	}
      g_free (modules);
      env_string = NULL;
    }

  if (argc && argv)
    {
      gint i, j, k;
      
      for (i = 1; i < *argc;)
	{
	  if (strcmp ("--gtk-module", (*argv)[i]) == 0 ||
	      strncmp ("--gtk-module=", (*argv)[i], 13) == 0)
	    {
	      gchar *module_name = (*argv)[i] + 12;
	      
	      if (*module_name == '=')
		module_name++;
	      else if (i + 1 < *argc)
		{
		  (*argv)[i] = NULL;
		  i += 1;
		  module_name = (*argv)[i];
		}
	      (*argv)[i] = NULL;

	      if (module_name && *module_name)
		gtk_modules = g_slist_prepend (gtk_modules, g_strdup (module_name));
	    }
	  else if (strcmp ("--g-fatal-warnings", (*argv)[i]) == 0)
	    {
	      GLogLevelFlags fatal_mask;
	      
	      fatal_mask = g_log_set_always_fatal (G_LOG_FATAL_MASK);
	      fatal_mask |= G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL;
              g_log_set_always_fatal (fatal_mask);
	      (*argv)[i] = NULL;
	    }
#ifdef G_ENABLE_DEBUG
	  else if ((strcmp ("--gtk-debug", (*argv)[i]) == 0) ||
		   (strncmp ("--gtk-debug=", (*argv)[i], 12) == 0))
	    {
	      gchar *equal_pos = strchr ((*argv)[i], '=');
	      
	      if (equal_pos != NULL)
		{
		  gtk_debug_flags |= g_parse_debug_string (equal_pos+1,
							   gtk_debug_keys,
							   gtk_ndebug_keys);
		}
	      else if ((i + 1) < *argc && (*argv)[i + 1])
		{
		  gtk_debug_flags |= g_parse_debug_string ((*argv)[i+1],
							   gtk_debug_keys,
							   gtk_ndebug_keys);
		  (*argv)[i] = NULL;
		  i += 1;
		}
	      (*argv)[i] = NULL;
	    }
	  else if ((strcmp ("--gtk-no-debug", (*argv)[i]) == 0) ||
		   (strncmp ("--gtk-no-debug=", (*argv)[i], 15) == 0))
	    {
	      gchar *equal_pos = strchr ((*argv)[i], '=');
	      
	      if (equal_pos != NULL)
		{
		  gtk_debug_flags &= ~g_parse_debug_string (equal_pos+1,
							    gtk_debug_keys,
							    gtk_ndebug_keys);
		}
	      else if ((i + 1) < *argc && (*argv)[i + 1])
		{
		  gtk_debug_flags &= ~g_parse_debug_string ((*argv)[i+1],
							    gtk_debug_keys,
							    gtk_ndebug_keys);
		  (*argv)[i] = NULL;
		  i += 1;
		}
	      (*argv)[i] = NULL;
	    }
#endif /* G_ENABLE_DEBUG */
	  i += 1;
	}
      
      for (i = 1; i < *argc; i++)
	{
	  for (k = i; k < *argc; k++)
	    if ((*argv)[k] != NULL)
	      break;
	  
	  if (k > i)
	    {
	      k -= i;
	      for (j = i + k; j < *argc; j++)
		(*argv)[j-k] = (*argv)[j];
	      *argc -= k;
	    }
	}
    }
  
  /* load gtk modules */
  gtk_modules = g_slist_reverse (gtk_modules);
  for (slist = gtk_modules; slist; slist = slist->next)
    {
      gchar *module_name;
      GModule *module = NULL;
      GtkModuleInitFunc modinit_func = NULL;
      
      module_name = slist->data;
      slist->data = NULL;
      if (!(module_name[0] == '/' ||
	    (module_name[0] == 'l' &&
	     module_name[1] == 'i' &&
	     module_name[2] == 'b')))
	{
	  gchar *old = module_name;
	  
	  module_name = g_strconcat ("lib", module_name, ".so", NULL);
	  g_free (old);
	}
      if (g_module_supported ())
	{
	  module = g_module_open (module_name, G_MODULE_BIND_LAZY);
	  if (module &&
	      g_module_symbol (module, "gtk_module_init", (gpointer*) &modinit_func) &&
	      modinit_func)
	    {
	      if (!g_slist_find (gtk_modules, (void *)modinit_func))
		{
		  g_module_make_resident (module);
		  slist->data = (void *)modinit_func;
		}
	      else
		{
		  g_module_close (module);
		  module = NULL;
		}
	    }
	}
      if (!modinit_func)
	{
	  g_warning ("Failed to load module \"%s\": %s",
		     module ? g_module_name (module) : module_name,
		     g_module_error ());
	  if (module)
	    g_module_close (module);
	}
      g_free (module_name);
    }

#ifdef ENABLE_NLS
  bindtextdomain("gtk+", GTK_LOCALEDIR);
#endif  

  /* Initialize the default visual and colormap to be
   *  used in creating widgets. (We want to use the system
   *  defaults so as to be nice to the colormap).
   */
  gtk_visual = gdk_visual_get_system ();
  gtk_colormap = gdk_colormap_get_system ();

  gtk_type_init ();
  gtk_object_post_arg_parsing_init ();
  gtk_signal_init ();
  gtk_rc_init ();
  
  
  /* Register an exit function to make sure we are able to cleanup.
   */
  g_atexit (gtk_exit_func);
  
  /* Set the 'initialized' flag.
   */
  gtk_initialized = TRUE;

  /* initialize gtk modules
   */
  for (slist = gtk_modules; slist; slist = slist->next)
    {
      if (slist->data)
	{
	  GtkModuleInitFunc modinit;
	  
	  modinit = (GtkModuleInitFunc)slist->data;
	  modinit (argc, argv);
	}
    }
  g_slist_free (gtk_modules);

  return TRUE;
}

void
gtk_init (int *argc, char ***argv)
{
  if (!gtk_init_check (argc, argv))
    {
      g_warning ("cannot open display: %s", gdk_get_display ());
      exit(1);
    }
}

void
gtk_exit (int errorcode)
{
  /* Only if "gtk" has been initialized should we de-initialize.
   */
  /* de-initialisation is done by the gtk_exit_funct(),
   * no need to do this here (Alex J.)
   */
  gdk_exit(errorcode);
}

gchar*
gtk_set_locale (void)
{
  return gdk_set_locale ();
}

void
gtk_main (void)
{
  GList *tmp_list;
  GList *functions;
  GtkInitFunction *init;
  GMainLoop *loop;

  gtk_main_loop_level++;
  
  loop = g_main_new (TRUE);
  main_loops = g_slist_prepend (main_loops, loop);

  tmp_list = functions = init_functions;
  init_functions = NULL;
  
  while (tmp_list)
    {
      init = tmp_list->data;
      tmp_list = tmp_list->next;
      
      (* init->function) (init->data);
      g_free (init);
    }
  g_list_free (functions);

  if (g_main_is_running (main_loops->data))
    {
      GDK_THREADS_LEAVE ();
      g_main_run (loop);
      GDK_THREADS_ENTER ();
      gdk_flush ();
    }

  if (quit_functions)
    {
      GList *reinvoke_list = NULL;
      GtkQuitFunction *quitf;

      while (quit_functions)
	{
	  quitf = quit_functions->data;

	  tmp_list = quit_functions;
	  quit_functions = g_list_remove_link (quit_functions, quit_functions);
	  g_list_free_1 (tmp_list);

	  if ((quitf->main_level && quitf->main_level != gtk_main_loop_level) ||
	      gtk_quit_invoke_function (quitf))
	    {
	      reinvoke_list = g_list_prepend (reinvoke_list, quitf);
	    }
	  else
	    {
	      gtk_quit_destroy (quitf);
	    }
	}
      if (reinvoke_list)
	{
	  GList *work;
	  
	  work = g_list_last (reinvoke_list);
	  if (quit_functions)
	    quit_functions->prev = work;
	  work->next = quit_functions;
	  quit_functions = work;
	}

      gdk_flush ();
    }
	      
  main_loops = g_slist_remove (main_loops, loop);

  g_main_destroy (loop);

  gtk_main_loop_level--;
}

guint
gtk_main_level (void)
{
  return gtk_main_loop_level;
}

void
gtk_main_quit (void)
{
  g_return_if_fail (main_loops != NULL);

  g_main_quit (main_loops->data);
}

gint
gtk_events_pending (void)
{
  gboolean result;
  
  GDK_THREADS_LEAVE ();  
  result = g_main_pending();
  GDK_THREADS_ENTER ();

  return result;
}

gint 
gtk_main_iteration (void)
{
  GDK_THREADS_LEAVE ();
  g_main_iteration (TRUE);
  GDK_THREADS_ENTER ();

  if (main_loops)
    return !g_main_is_running (main_loops->data);
  else
    return TRUE;
}

gint 
gtk_main_iteration_do (gboolean blocking)
{
  GDK_THREADS_LEAVE ();
  g_main_iteration (blocking);
  GDK_THREADS_ENTER ();

  if (main_loops)
    return !g_main_is_running (main_loops->data);
  else
    return TRUE;
}

void 
gtk_main_do_event (GdkEvent *event)
{
  GtkWidget *event_widget;
  GtkWidget *grab_widget;
  GdkEvent *next_event;
  GList *tmp_list;

  /* If there are any events pending then get the next one.
   */
  next_event = gdk_event_peek ();
  
  /* Try to compress enter/leave notify events. These event
   *  pairs occur when the mouse is dragged quickly across
   *  a window with many buttons (or through a menu). Instead
   *  of highlighting and de-highlighting each widget that
   *  is crossed it is better to simply de-highlight the widget
   *  which contained the mouse initially and highlight the
   *  widget which ends up containing the mouse.
   */
  if (next_event)
    if (((event->type == GDK_ENTER_NOTIFY) ||
	 (event->type == GDK_LEAVE_NOTIFY)) &&
	((next_event->type == GDK_ENTER_NOTIFY) ||
	 (next_event->type == GDK_LEAVE_NOTIFY)) &&
	(next_event->type != event->type) &&
	(next_event->any.window == event->any.window))
      {
	/* Throw both the peeked copy and the queued copy away 
	 */
	gdk_event_free (next_event);
	next_event = gdk_event_get ();
	gdk_event_free (next_event);
	
	return;
      }

  if (next_event)
    gdk_event_free (next_event);

  /* Find the widget which got the event. We store the widget
   *  in the user_data field of GdkWindow's.
   *  Ignore the event if we don't have a widget for it, except
   *  for GDK_PROPERTY_NOTIFY events which are handled specialy.
   *  Though this happens rarely, bogus events can occour
   *  for e.g. destroyed GdkWindows. 
   */
  event_widget = gtk_get_event_widget (event);
  if (!event_widget)
    {
      /* To handle selection INCR transactions, we select
       * PropertyNotify events on the requestor window and create
       * a corresponding (fake) GdkWindow so that events get
       * here. There won't be a widget though, so we have to handle
	   * them specially
	   */
      if (event->type == GDK_PROPERTY_NOTIFY)
	gtk_selection_incr_event (event->any.window,
				  &event->property);
      
      return;
    }
  
  /* Push the event onto a stack of current events for
   * gtk_current_event_get().
   */
  current_events = g_list_prepend (current_events, event);
  
  /* If there is a grab in effect...
   */
  if (grabs)
    {
      grab_widget = grabs->data;
      
      /* If the grab widget is an ancestor of the event widget
       *  then we send the event to the original event widget.
       *  This is the key to implementing modality.
       */
      if (GTK_WIDGET_IS_SENSITIVE (event_widget) &&
	  gtk_widget_is_ancestor (event_widget, grab_widget))
	grab_widget = event_widget;
    }
  else
    {
      grab_widget = event_widget;
    }

  /* Not all events get sent to the grabbing widget.
   * The delete, destroy, expose, focus change and resize
   *  events still get sent to the event widget because
   *  1) these events have no meaning for the grabbing widget
   *  and 2) redirecting these events to the grabbing widget
   *  could cause the display to be messed up.
   * 
   * Drag events are also not redirected, since it isn't
   *  clear what the semantics of that would be.
   */
  switch (event->type)
    {
    case GDK_NOTHING:
      break;
      
    case GDK_DELETE:
      gtk_widget_ref (event_widget);
      if ((!grabs || gtk_widget_get_toplevel (grabs->data) == event_widget) &&
	  !gtk_widget_event (event_widget, event))
	gtk_widget_destroy (event_widget);
      gtk_widget_unref (event_widget);
      break;
      
    case GDK_DESTROY:
      /* Unexpected GDK_DESTROY from the outside, ignore for
       * child windows, handle like a GDK_DELETE for toplevels
       */
      if (!event_widget->parent)
	{
	  gtk_widget_ref (event_widget);
	  if (!gtk_widget_event (event_widget, event) &&
	      !GTK_OBJECT_DESTROYED (event_widget))
	    gtk_widget_destroy (event_widget);
	  gtk_widget_unref (event_widget);
	}
      break;
      
    case GDK_PROPERTY_NOTIFY:
    case GDK_EXPOSE:
    case GDK_NO_EXPOSE:
    case GDK_FOCUS_CHANGE:
    case GDK_CONFIGURE:
    case GDK_MAP:
    case GDK_UNMAP:
    case GDK_SELECTION_CLEAR:
    case GDK_SELECTION_REQUEST:
    case GDK_SELECTION_NOTIFY:
    case GDK_CLIENT_EVENT:
    case GDK_VISIBILITY_NOTIFY:
      gtk_widget_event (event_widget, event);
      break;

    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
    /* We treat button 4-5 specially, assume we have
     * a MS-style scrollwheel mouse, and try to find
     * a plausible widget to scroll. We also trap
     * button 4-5 double and triple clicks here, since
     * they will be generated if the user scrolls quickly.
     */
      if ((grab_widget == event_widget) &&
	  (event->button.button == 4 || event->button.button == 5))
	{
	  GtkWidget *range = NULL;
	  GtkWidget *scrollwin;
	  
	  if (GTK_IS_RANGE (event_widget))
	    range = event_widget;
	  else
	    {
	      scrollwin = gtk_widget_get_ancestor (event_widget,
						   GTK_TYPE_SCROLLED_WINDOW);
	      if (scrollwin)
		range = GTK_SCROLLED_WINDOW (scrollwin)->vscrollbar;
	    }
	  
	  if (range && GTK_WIDGET_VISIBLE (range))
	    {
	      if (event->type == GDK_BUTTON_PRESS)
		{
		  GtkAdjustment *adj = GTK_RANGE (range)->adjustment;
		  gfloat new_value = adj->value + ((event->button.button == 4) ? 
						   -adj->page_increment / 2: 
						    adj->page_increment / 2);
		  new_value = CLAMP (new_value, adj->lower, adj->upper - adj->page_size);
		  gtk_adjustment_set_value (adj, new_value);
		}
	      break;
	    }
	}
      gtk_propagate_event (grab_widget, event);
      break;

    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      if (key_snoopers)
	{
	  if (gtk_invoke_key_snoopers (grab_widget, event))
	    break;
	}
      /* else fall through */
    case GDK_MOTION_NOTIFY:
    case GDK_BUTTON_RELEASE:
    case GDK_PROXIMITY_IN:
    case GDK_PROXIMITY_OUT:
      gtk_propagate_event (grab_widget, event);
      break;
      
    case GDK_ENTER_NOTIFY:
      if (GTK_WIDGET_IS_SENSITIVE (grab_widget))
	{
	  gtk_widget_event (grab_widget, event);
	  if (event_widget == grab_widget)
	    GTK_PRIVATE_SET_FLAG (event_widget, GTK_LEAVE_PENDING);
	}
      break;
      
    case GDK_LEAVE_NOTIFY:
      if (GTK_WIDGET_LEAVE_PENDING (event_widget))
	{
	  GTK_PRIVATE_UNSET_FLAG (event_widget, GTK_LEAVE_PENDING);
	  gtk_widget_event (event_widget, event);
	}
      else if (GTK_WIDGET_IS_SENSITIVE (grab_widget))
	gtk_widget_event (grab_widget, event);
      break;
      
    case GDK_DRAG_STATUS:
    case GDK_DROP_FINISHED:
      gtk_drag_source_handle_event (event_widget, event);
      break;
    case GDK_DRAG_ENTER:
    case GDK_DRAG_LEAVE:
    case GDK_DRAG_MOTION:
    case GDK_DROP_START:
      gtk_drag_dest_handle_event (event_widget, event);
      break;
    }
  
  tmp_list = current_events;
  current_events = g_list_remove_link (current_events, tmp_list);
  g_list_free_1 (tmp_list);
}

gint
gtk_true (void)
{
  return TRUE;
}

gint
gtk_false (void)
{
  return FALSE;
}

void
gtk_grab_add (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  
  if (!GTK_WIDGET_HAS_GRAB (widget) && GTK_WIDGET_IS_SENSITIVE (widget))
    {
      GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_GRAB);
      
      grabs = g_slist_prepend (grabs, widget);
      gtk_widget_ref (widget);
    }
}

GtkWidget*
gtk_grab_get_current (void)
{
  if (grabs)
    return GTK_WIDGET (grabs->data);
  return NULL;
}

void
gtk_grab_remove (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  
  if (GTK_WIDGET_HAS_GRAB (widget))
    {
      GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_GRAB);
      
      grabs = g_slist_remove (grabs, widget);
      gtk_widget_unref (widget);
    }
}

void
gtk_init_add (GtkFunction function,
	      gpointer	  data)
{
  GtkInitFunction *init;
  
  init = g_new (GtkInitFunction, 1);
  init->function = function;
  init->data = data;
  
  init_functions = g_list_prepend (init_functions, init);
}

guint
gtk_key_snooper_install (GtkKeySnoopFunc snooper,
			 gpointer	 func_data)
{
  GtkKeySnooperData *data;
  static guint snooper_id = 1;

  g_return_val_if_fail (snooper != NULL, 0);

  data = g_new (GtkKeySnooperData, 1);
  data->func = snooper;
  data->func_data = func_data;
  data->id = snooper_id++;
  key_snoopers = g_slist_prepend (key_snoopers, data);

  return data->id;
}

void
gtk_key_snooper_remove (guint		 snooper_id)
{
  GtkKeySnooperData *data = NULL;
  GSList *slist;

  slist = key_snoopers;
  while (slist)
    {
      data = slist->data;
      if (data->id == snooper_id)
	break;

      slist = slist->next;
      data = NULL;
    }
  if (data)
    key_snoopers = g_slist_remove (key_snoopers, data);
}

static gint
gtk_invoke_key_snoopers (GtkWidget *grab_widget,
			 GdkEvent  *event)
{
  GSList *slist;
  gint return_val = FALSE;

  slist = key_snoopers;
  while (slist && !return_val)
    {
      GtkKeySnooperData *data;

      data = slist->data;
      slist = slist->next;
      return_val = (*data->func) (grab_widget, (GdkEventKey*) event, data->func_data);
    }

  return return_val;
}

guint
gtk_quit_add_full (guint		main_level,
		   GtkFunction		function,
		   GtkCallbackMarshal	marshal,
		   gpointer		data,
		   GtkDestroyNotify	destroy)
{
  static guint quit_id = 1;
  GtkQuitFunction *quitf;
  
  g_return_val_if_fail ((function != NULL) || (marshal != NULL), 0);

  if (!quit_mem_chunk)
    quit_mem_chunk = g_mem_chunk_new ("quit mem chunk", sizeof (GtkQuitFunction),
				      512, G_ALLOC_AND_FREE);
  
  quitf = g_chunk_new (GtkQuitFunction, quit_mem_chunk);
  
  quitf->id = quit_id++;
  quitf->main_level = main_level;
  quitf->function = function;
  quitf->marshal = marshal;
  quitf->data = data;
  quitf->destroy = destroy;

  quit_functions = g_list_prepend (quit_functions, quitf);
  
  return quitf->id;
}

static void
gtk_quit_destroy (GtkQuitFunction *quitf)
{
  if (quitf->destroy)
    quitf->destroy (quitf->data);
  g_mem_chunk_free (quit_mem_chunk, quitf);
}

static gint
gtk_quit_destructor (GtkObject **object_p)
{
  if (*object_p)
    gtk_object_destroy (*object_p);
  g_free (object_p);

  return FALSE;
}

void
gtk_quit_add_destroy (guint              main_level,
		      GtkObject         *object)
{
  GtkObject **object_p;

  g_return_if_fail (main_level > 0);
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));

  object_p = g_new (GtkObject*, 1);
  *object_p = object;
  gtk_signal_connect (object,
		      "destroy",
		      GTK_SIGNAL_FUNC (gtk_widget_destroyed),
		      object_p);
  gtk_quit_add (main_level, (GtkFunction) gtk_quit_destructor, object_p);
}

guint
gtk_quit_add (guint	  main_level,
	      GtkFunction function,
	      gpointer	  data)
{
  return gtk_quit_add_full (main_level, function, NULL, data, NULL);
}

void
gtk_quit_remove (guint id)
{
  GtkQuitFunction *quitf;
  GList *tmp_list;
  
  tmp_list = quit_functions;
  while (tmp_list)
    {
      quitf = tmp_list->data;
      
      if (quitf->id == id)
	{
	  quit_functions = g_list_remove_link (quit_functions, tmp_list);
	  g_list_free (tmp_list);
	  gtk_quit_destroy (quitf);
	  
	  return;
	}
      
      tmp_list = tmp_list->next;
    }
}

void
gtk_quit_remove_by_data (gpointer data)
{
  GtkQuitFunction *quitf;
  GList *tmp_list;
  
  tmp_list = quit_functions;
  while (tmp_list)
    {
      quitf = tmp_list->data;
      
      if (quitf->data == data)
	{
	  quit_functions = g_list_remove_link (quit_functions, tmp_list);
	  g_list_free (tmp_list);
	  gtk_quit_destroy (quitf);

	  return;
	}
      
      tmp_list = tmp_list->next;
    }
}

guint
gtk_timeout_add_full (guint32		 interval,
		      GtkFunction	 function,
		      GtkCallbackMarshal marshal,
		      gpointer		 data,
		      GtkDestroyNotify	 destroy)
{
  if (marshal)
    {
      GtkClosure *closure;

      closure = g_new (GtkClosure, 1);
      closure->marshal = marshal;
      closure->data = data;
      closure->destroy = destroy;

      return g_timeout_add_full (0, interval, 
				 gtk_invoke_idle_timeout,
				 closure,
				 gtk_destroy_closure);
    }
  else
    return g_timeout_add_full (0, interval, function, data, destroy);
}

guint
gtk_timeout_add (guint32     interval,
		 GtkFunction function,
		 gpointer    data)
{
  return g_timeout_add_full (0, interval, function, data, NULL);
}

void
gtk_timeout_remove (guint tag)
{
  g_source_remove (tag);
}

guint
gtk_idle_add_full (gint			priority,
		   GtkFunction		function,
		   GtkCallbackMarshal	marshal,
		   gpointer		data,
		   GtkDestroyNotify	destroy)
{
  if (marshal)
    {
      GtkClosure *closure;

      closure = g_new (GtkClosure, 1);
      closure->marshal = marshal;
      closure->data = data;
      closure->destroy = destroy;

      return g_idle_add_full (priority,
			      gtk_invoke_idle_timeout,
			      closure,
			      gtk_destroy_closure);
    }
  else
    return g_idle_add_full (priority, function, data, destroy);
}

guint
gtk_idle_add (GtkFunction function,
	      gpointer	  data)
{
  return g_idle_add_full (GTK_PRIORITY_DEFAULT, function, data, NULL);
}

guint	    
gtk_idle_add_priority	(gint		    priority,
			 GtkFunction	    function,
			 gpointer	    data)
{
  return g_idle_add_full (priority, function, data, NULL);
}

void
gtk_idle_remove (guint tag)
{
  g_source_remove (tag);
}

void
gtk_idle_remove_by_data (gpointer data)
{
  if (!g_idle_remove_by_data (data))
    g_warning ("gtk_idle_remove_by_data(%p): no such idle", data);
}

guint
gtk_input_add_full (gint		source,
		    GdkInputCondition	condition,
		    GdkInputFunction	function,
		    GtkCallbackMarshal	marshal,
		    gpointer		data,
		    GtkDestroyNotify	destroy)
{
  if (marshal)
    {
      GtkClosure *closure;

      closure = g_new (GtkClosure, 1);
      closure->marshal = marshal;
      closure->data = data;
      closure->destroy = destroy;

      return gdk_input_add_full (source,
				 condition,
				 (GdkInputFunction) gtk_invoke_input,
				 closure,
				 (GdkDestroyNotify) gtk_destroy_closure);
    }
  else
    return gdk_input_add_full (source, condition, function, data, destroy);
}

void
gtk_input_remove (guint tag)
{
  g_source_remove (tag);
}

static void
gtk_destroy_closure (gpointer data)
{
  GtkClosure *closure = data;

  if (closure->destroy)
    (closure->destroy) (closure->data);
  g_free (closure);
}

static gboolean
gtk_invoke_idle_timeout (gpointer data)
{
  GtkClosure *closure = data;

  GtkArg args[1];
  gint ret_val = FALSE;
  args[0].name = NULL;
  args[0].type = GTK_TYPE_BOOL;
  args[0].d.pointer_data = &ret_val;
  closure->marshal (NULL, closure->data,  0, args);
  return ret_val;
}

static void
gtk_invoke_input (gpointer	    data,
		  gint		    source,
		  GdkInputCondition condition)
{
  GtkClosure *closure = data;

  GtkArg args[3];
  args[0].type = GTK_TYPE_INT;
  args[0].name = NULL;
  GTK_VALUE_INT(args[0]) = source;
  args[1].type = GTK_TYPE_GDK_INPUT_CONDITION;
  args[1].name = NULL;
  GTK_VALUE_FLAGS(args[1]) = condition;
  args[2].type = GTK_TYPE_NONE;
  args[2].name = NULL;

  closure->marshal (NULL, closure->data, 2, args);
}

GdkEvent*
gtk_get_current_event (void)
{
  if (current_events)
    return gdk_event_copy ((GdkEvent *) current_events->data);
  else
    return NULL;
}

GtkWidget*
gtk_get_event_widget (GdkEvent *event)
{
  GtkWidget *widget;

  widget = NULL;
  if (event && event->any.window)
    gdk_window_get_user_data (event->any.window, (void**) &widget);
  
  return widget;
}

static void
gtk_exit_func (void)
{
  if (gtk_initialized)
    {
      gtk_initialized = FALSE;
      gtk_preview_uninit ();
    }
}


static gint
gtk_quit_invoke_function (GtkQuitFunction *quitf)
{
  if (!quitf->marshal)
    return quitf->function (quitf->data);
  else
    {
      GtkArg args[1];
      gint ret_val = FALSE;

      args[0].name = NULL;
      args[0].type = GTK_TYPE_BOOL;
      args[0].d.pointer_data = &ret_val;
      ((GtkCallbackMarshal) quitf->marshal) (NULL,
					     quitf->data,
					     0, args);
      return ret_val;
    }
}

void
gtk_propagate_event (GtkWidget *widget,
		     GdkEvent  *event)
{
  gint handled_event;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (event != NULL);
  
  handled_event = FALSE;

  gtk_widget_ref (widget);
      
  if ((event->type == GDK_KEY_PRESS) ||
      (event->type == GDK_KEY_RELEASE))
    {
      /* Only send key events within Window widgets to the Window
       *  The Window widget will in turn pass the
       *  key event on to the currently focused widget
       *  for that window.
       */
      GtkWidget *window;

      window = gtk_widget_get_ancestor (widget, GTK_TYPE_WINDOW);

      if (window)
	{
	  /* If there is a grab within the window, give the grab widget
	   * a first crack at the key event
	   */
	  if (widget != window && GTK_WIDGET_HAS_GRAB (widget))
	    handled_event = gtk_widget_event (widget, event);
	  
	  if (!handled_event)
	    {
	      window = gtk_widget_get_ancestor (widget, GTK_TYPE_WINDOW);
	      if (window)
		{
		  if (GTK_WIDGET_IS_SENSITIVE (window))
		    gtk_widget_event (window, event);
		}
	    }
		  
	  handled_event = TRUE; /* don't send to widget */
	}
    }
  
  /* Other events get propagated up the widget tree
   *  so that parents can see the button and motion
   *  events of the children.
   */
  if (!handled_event)
    {
      while (TRUE)
	{
	  GtkWidget *tmp;
	  
	  handled_event = !GTK_WIDGET_IS_SENSITIVE (widget) || gtk_widget_event (widget, event);
	  tmp = widget->parent;
	  gtk_widget_unref (widget);

	  widget = tmp;
	  
	  if (!handled_event && widget)
	    gtk_widget_ref (widget);
	  else
	    break;
	}
    }
  else
    gtk_widget_unref (widget);
}

#if 0
static void
gtk_error (gchar *str)
{
  gtk_print (str);
}

static void
gtk_warning (gchar *str)
{
  gtk_print (str);
}

static void
gtk_message (gchar *str)
{
  gtk_print (str);
}

static void
gtk_print (gchar *str)
{
  static GtkWidget *window = NULL;
  static GtkWidget *text;
  static int level = 0;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *table;
  GtkWidget *hscrollbar;
  GtkWidget *vscrollbar;
  GtkWidget *separator;
  GtkWidget *button;
  
  if (level > 0)
    {
      fputs (str, stdout);
      fflush (stdout);
      return;
    }
  
  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      
      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  (GtkSignalFunc) gtk_widget_destroyed,
			  &window);
      
      gtk_window_set_title (GTK_WINDOW (window), "Messages");
      
      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);
      
      
      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
      gtk_widget_show (box2);
      
      
      table = gtk_table_new (2, 2, FALSE);
      gtk_table_set_row_spacing (GTK_TABLE (table), 0, 2);
      gtk_table_set_col_spacing (GTK_TABLE (table), 0, 2);
      gtk_box_pack_start (GTK_BOX (box2), table, TRUE, TRUE, 0);
      gtk_widget_show (table);
      
      text = gtk_text_new (NULL, NULL);
      gtk_text_set_editable (GTK_TEXT (text), FALSE);
      gtk_table_attach_defaults (GTK_TABLE (table), text, 0, 1, 0, 1);
      gtk_widget_show (text);
      gtk_widget_realize (text);
      
      hscrollbar = gtk_hscrollbar_new (GTK_TEXT (text)->hadj);
      gtk_table_attach (GTK_TABLE (table), hscrollbar, 0, 1, 1, 2,
			GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
      gtk_widget_show (hscrollbar);
      
      vscrollbar = gtk_vscrollbar_new (GTK_TEXT (text)->vadj);
      gtk_table_attach (GTK_TABLE (table), vscrollbar, 1, 2, 0, 1,
			GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
      gtk_widget_show (vscrollbar);
      
      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);
      
      
      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
      gtk_widget_show (box2);
      
      
      button = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 (GtkSignalFunc) gtk_widget_hide,
				 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);
    }
  
  level += 1;
  gtk_text_insert (GTK_TEXT (text), NULL, NULL, NULL, str, -1);
  level -= 1;
  
  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
}
#endif
