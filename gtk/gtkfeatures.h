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
#ifndef __GTK_FEATURES_H__
#define __GTK_FEATURES_H__


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

/* compile time version
 */
#define GTK_MAJOR_VERSION				(1)
#define GTK_MINOR_VERSION				(1)
#define GTK_MICRO_VERSION				(0)

/* new functions gtk_container_set_focus_vadjustment() and
 * gtk_container_set_focus_hadjustment().
 */
#define GTK_HAVE_CONTAINER_FOCUS_ADJUSTMENTS		1-0-1

/* new macros for checking of class types
 */
#define	GTK_HAVE_IS_CLASS				1-1-0

/* export the gtk_signal_init() function
 * new gtk_signal_n_emissions*() functions
 * "signal-name" is now an alias for "signal_name"
 * new gtk_signal_emitv*() interface
 */
#define GTK_HAVE_SIGNAL_INIT				1-1-0
#define	GTK_HAVE_N_EMISSIONS				1-1-0
#define GTK_HAVE_SIGNAL_LISP_SYNTAX			1-1-0
#define GTK_HAVE_SIGNAL_EMITV				1-1-0
  


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_FEATURES_H__ */
