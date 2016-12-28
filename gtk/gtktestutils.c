/* Gtk+ testing utilities
 * Copyright (C) 2007 Imendio AB
 * Authors: Tim Janik
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
 */


#include "config.h"

#include "gtkspinbutton.h"
#include "gtkmain.h"
#include "gtkbox.h"
#include "gtklabel.h"
#include "gtkbutton.h"
#include "gtktextview.h"
#include "gtkrange.h"

#include <locale.h>
#include <string.h>
#include <math.h>

/* This is a hack.
 * We want to include the same headers as gtktypefuncs.c but we are not
 * allowed to include gtkx.h directly during GTK compilation.
 * So....
 */
#undef GTK_COMPILATION
#include <gtk/gtk.h>
#define GTK_COMPILATION

/**
 * SECTION:gtktesting
 * @Short_description: Utilities for testing GTK+ applications
 * @Title: Testing
 */

/**
 * gtk_test_init:
 * @argcp: Address of the `argc` parameter of the
 *        main() function. Changed if any arguments were handled.
 * @argvp: (inout) (array length=argcp): Address of the 
 *        `argv` parameter of main().
 *        Any parameters understood by g_test_init() or gtk_init() are
 *        stripped before return.
 * @...: currently unused
 *
 * This function is used to initialize a GTK+ test program.
 *
 * It will in turn call g_test_init() and gtk_init() to properly
 * initialize the testing framework and graphical toolkit. It’ll 
 * also set the program’s locale to “C” and prevent loading of rc 
 * files and Gtk+ modules. This is done to make tets program
 * environments as deterministic as possible.
 *
 * Like gtk_init() and g_test_init(), any known arguments will be
 * processed and stripped from @argc and @argv.
 *
 * Since: 2.14
 **/
void
gtk_test_init (int    *argcp,
               char ***argvp,
               ...)
{
  g_test_init (argcp, argvp, NULL);
  /* - enter C locale
   * - call g_test_init();
   * - call gtk_init();
   * - prevent RC files from loading;
   * - prevent Gtk modules from loading;
   * - supply mock object for GtkSettings
   * FUTURE TODO:
   * - this function could install a mock object around GtkSettings
   */
  g_setenv ("GTK_MODULES", "", TRUE);
  gtk_disable_setlocale();
  setlocale (LC_ALL, "C");
  g_test_bug_base ("http://bugzilla.gnome.org/show_bug.cgi?id=%s");

  /* XSendEvent() doesn't work yet on XI2 events.
   * So at the moment gdk_test_simulate_* can only
   * send events that GTK+ understands if XI2 is
   * disabled, bummer.
   */
  gdk_disable_multidevice ();

  gtk_init ();
}

static gboolean
quit_main_loop_callback (GtkWidget     *widget,
                         GdkFrameClock *frame_clock,
                         gpointer       user_data)
{
  gtk_main_quit ();

  return G_SOURCE_REMOVE;
}

/**
 * gtk_test_widget_wait_for_draw:
 * @widget: the widget to wait for
 *
 * Enters the main loop and waits for @widget to be “drawn”. In this
 * context that means it waits for the frame clock of @widget to have
 * run a full styling, layout and drawing cycle.
 *
 * This function is intended to be used for syncing with actions that
 * depend on @widget relayouting or on interaction with the display
 * server.
 *
 * Since: 3.10
 **/
void
gtk_test_widget_wait_for_draw (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  /* We can do this here because the whole tick procedure does not
   * reenter the main loop. Otherwise we'd need to manually get the
   * frame clock and connect to the after-paint signal.
   */
  gtk_widget_add_tick_callback (widget,
                                quit_main_loop_callback,
                                NULL,
                                NULL);

  gtk_main ();
}

static GType *all_registered_types = NULL;
static guint  n_all_registered_types = 0;

/**
 * gtk_test_list_all_types:
 * @n_types: location to store number of types
 *
 * Return the type ids that have been registered after
 * calling gtk_test_register_all_types().
 *
 * Returns: (array length=n_types zero-terminated=1) (transfer none):
 *    0-terminated array of type ids
 *
 * Since: 2.14
 */
const GType*
gtk_test_list_all_types (guint *n_types)
{
  if (n_types)
    *n_types = n_all_registered_types;
  return all_registered_types;
}

/**
 * gtk_test_register_all_types:
 *
 * Force registration of all core Gtk+ and Gdk object types.
 * This allowes to refer to any of those object types via
 * g_type_from_name() after calling this function.
 *
 * Since: 2.14
 **/
void
gtk_test_register_all_types (void)
{
  if (!all_registered_types)
    {
      const guint max_gtk_types = 999;
      GType *tp;
      all_registered_types = g_new0 (GType, max_gtk_types);
      tp = all_registered_types;
#include "gtktypefuncs.c"
      n_all_registered_types = tp - all_registered_types;
      g_assert (n_all_registered_types + 1 < max_gtk_types);
      *tp = 0;
    }
}
