/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include <locale.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/stat.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define GDK_DISABLE_DEPRECATION_WARNINGS

#include <glib.h>
#include <glib/gstdio.h>
#include "gdk/gdk.h"

#include "gtkversion.h"
#include "gtkrc.h"
#include "gtkstyle.h"
#include "gtkbindings.h"
#include "gtkintl.h"
#include "gtkiconfactory.h"
#include "gtkmain.h"
#include "gtkmodules.h"
#include "gtkmodulesprivate.h"
#include "gtkprivate.h"
#include "gtksettingsprivate.h"
#include "gtkwidgetpath.h"
#include "gtkwindow.h"

#ifdef G_OS_WIN32
#include <io.h>
#endif


/**
 * SECTION:gtkrc
 * @Short_description: Deprecated routines for handling resource files
 * @Title: Resource Files
 *
 * GTK+ provides resource file mechanism for configuring
 * various aspects of the operation of a GTK+ program
 * at runtime.
 *
 * <warning>
 * In GTK+ 3.0, resource files have been deprecated and replaced
 * by CSS-like style sheets, which are understood by #GtkCssProvider.
 * </warning>
 *
 * <refsect2>
 * <title>Default files</title>
 * <para>
 * An application can cause GTK+ to parse a specific RC
 * file by calling gtk_rc_parse(). In addition to this,
 * certain files will be read at the end of gtk_init().
 * Unless modified, the files looked for will be
 * <filename>&lt;SYSCONFDIR&gt;/gtk-2.0/gtkrc</filename>
 * and <filename>.gtkrc-3.0</filename> in the users home directory.
 * (<filename>&lt;SYSCONFDIR&gt;</filename> defaults to
 * <filename>/usr/local/etc</filename>. It can be changed with the
 * <option>--prefix</option> or <option>--sysconfdir</option> options when
 * configuring GTK+.)
 *
 * The set of these <firstterm>default</firstterm> files
 * can be retrieved with gtk_rc_get_default_files()
 * and modified with gtk_rc_add_default_file() and
 * gtk_rc_set_default_files().
 * Additionally, the <envar>GTK2_RC_FILES</envar> environment variable
 * can be set to a #G_SEARCHPATH_SEPARATOR_S-separated list of files
 * in order to overwrite the set of default files at runtime.
 * <para><anchor id="locale-specific-rc"/>
 * For each RC file, in addition to the file itself, GTK+ will look for
 * a locale-specific file that will be parsed after the main file.
 * For instance, if <envar>LANG</envar> is set to <literal>ja_JP.ujis</literal>,
 * when loading the default file <filename>~/.gtkrc</filename> then GTK+ looks
 * for <filename>~/.gtkrc.ja_JP</filename> and <filename>~/.gtkrc.ja</filename>,
 * and parses the first of those that exists.</para>
 * </para>
 * </refsect2>
 * <refsect2>
 * <title>Pathnames and patterns</title>
 * <anchor id="gtkrc-pathnames-and-patterns"/>
 * <para>
 * A resource file defines a number of styles and key bindings and
 * attaches them to particular widgets. The attachment is done
 * by the <literal>widget</literal>, <literal>widget_class</literal>,
 * and <literal>class</literal> declarations. As an example
 * of such a statement:
 *
 * <informalexample><programlisting>
 * widget "mywindow.*.GtkEntry" style "my-entry-class"
 * </programlisting></informalexample>
 *
 * attaches the style <literal>"my-entry-class"</literal> to all
 * widgets  whose <firstterm>widget path</firstterm> matches the
 * <firstterm>pattern</firstterm> <literal>"mywindow.*.GtkEntry"</literal>.
 * That is, all #GtkEntry widgets which are part of a #GtkWindow named
 * <literal>"mywindow"</literal>.
 *
 * The patterns here are given in the standard shell glob syntax.
 * The <literal>"?"</literal> wildcard matches any character, while
 * <literal>"*"</literal> matches zero or more of any character.
 * The three types of matching are against the widget path, the
 * <firstterm>class path</firstterm> and the class hierarchy. Both the
 * widget path and the class path consist of a <literal>"."</literal>
 * separated list of all the parents of the widget and the widget itself
 * from outermost to innermost. The difference is that in the widget path,
 * the name assigned by gtk_widget_set_name() is used if present, otherwise
 * the class name of the widget, while for the class path, the class name is
 * always used.
 *
 * Since GTK+ 2.10, <literal>widget_class</literal> paths can also contain
 * <literal>&lt;classname&gt;</literal> substrings, which are matching
 * the class with the given name and any derived classes. For instance,
 * <informalexample><programlisting>
 * widget_class "*&lt;GtkMenuItem&gt;.GtkLabel" style "my-style"
 * </programlisting></informalexample>
 * will match #GtkLabel widgets which are contained in any kind of menu item.
 *
 * So, if you have a #GtkEntry named <literal>"myentry"</literal>, inside of a
 * horizontal box in a window named <literal>"mywindow"</literal>, then the
 * widget path is: <literal>"mywindow.GtkHBox.myentry"</literal>
 * while the class path is: <literal>"GtkWindow.GtkHBox.GtkEntry"</literal>.
 *
 * Matching against class is a little different. The pattern match is done
 * against all class names in the widgets class hierarchy (not the layout
 * hierarchy) in sequence, so the pattern:
 * <informalexample><programlisting>
 * class "GtkButton" style "my-style"
 * </programlisting></informalexample>
 * will match not just #GtkButton widgets, but also #GtkToggleButton and
 * #GtkCheckButton widgets, since those classes derive from #GtkButton.
 *
 * Additionally, a priority can be specified for each pattern, and styles
 * override other styles first by priority, then by pattern type and then
 * by order of specification (later overrides earlier). The priorities
 * that can be specified are (highest to lowest):
 * <simplelist>
 * <member><literal>highest</literal></member>
 * <member><literal>rc</literal></member>
 * <member><literal>theme</literal></member>
 * <member><literal>application</literal></member>
 * <member><literal>gtk</literal></member>
 * <member><literal>lowest</literal></member>
 * </simplelist>
 * <literal>rc</literal> is the default for styles
 * read from an RC file, <literal>theme</literal>
 * is the default for styles read from theme RC files,
 * <literal>application</literal>
 * should be used for styles an application sets
 * up, and <literal>gtk</literal> is used for styles
 * that GTK+ creates internally.
 * </para>
 * </refsect2>
 * <refsect2>
 * <title>Theme gtkrc files</title>
 * <anchor id="theme-gtkrc-files"/>
 * <para>
 * Theme RC files are loaded first from under the <filename>~/.themes/</filename>,
 * then from the directory from gtk_rc_get_theme_dir(). The files looked at will
 * be <filename>gtk-3.0/gtkrc</filename>.
 *
 * When the application prefers dark themes
 * (see the #GtkSettings:gtk-application-prefer-dark-theme property for details),
 * <filename>gtk-3.0/gtkrc-dark</filename> will be loaded first, and if not present
 * <filename>gtk-3.0/gtkrc</filename> will be loaded.
 * </para>
 * </refsect2>
 * <refsect2>
 * <title>Optimizing RC Style Matches</title>
 * <anchor id="optimizing-rc-style-matches"/>
 * <para>
 * Everytime a widget is created and added to the layout hierarchy of a #GtkWindow
 * ("anchored" to be exact), a list of matching RC styles out of all RC styles read
 * in so far is composed.
 * For this, every RC style is matched against the widgets class path,
 * the widgets name path and widgets inheritance hierarchy.
 * As a consequence, significant slowdown can be caused by utilization of many
 * RC styles and by using RC style patterns that are slow or complicated to match
 * against a given widget.
 * The following ordered list provides a number of advices (prioritized by
 * effectiveness) to reduce the performance overhead associated with RC style
 * matches:
 *
 * <orderedlist>
 *   <listitem><para>
 *   Move RC styles for specific applications into RC files dedicated to those
 *   applications and parse application specific RC files only from
 *   applications that are affected by them.
 *   This reduces the overall amount of RC styles that have to be considered
 *   for a match across a group of applications.
 *   </para></listitem>
 *   <listitem><para>
 *   Merge multiple styles which use the same matching rule, for instance:
 *   <informalexample><programlisting>
 *      style "Foo" { foo_content }
 *      class "X" style "Foo"
 *      style "Bar" { bar_content }
 *      class "X" style "Bar"
 *   </programlisting></informalexample>
 *   is faster to match as:
 *   <informalexample><programlisting>
 *      style "FooBar" { foo_content bar_content }
 *      class "X" style "FooBar"
 *   </programlisting></informalexample>
 *   </para></listitem>
 *   <listitem><para>
 *   Use of wildcards should be avoided, this can reduce the individual RC style
 *   match to a single integer comparison in most cases.
 *   </para></listitem>
 *   <listitem><para>
 *   To avoid complex recursive matching, specification of full class names
 *   (for <literal>class</literal> matches) or full path names (for
 *   <literal>widget</literal> and <literal>widget_class</literal> matches)
 *   is to be preferred over shortened names
 *   containing <literal>"*"</literal> or <literal>"?"</literal>.
 *   </para></listitem>
 *   <listitem><para>
 *   If at all necessary, wildcards should only be used at the tail or head
 *   of a pattern. This reduces the match complexity to a string comparison
 *   per RC style.
 *   </para></listitem>
 *   <listitem><para>
 *   When using wildcards, use of <literal>"?"</literal> should be preferred
 *   over <literal>"*"</literal>. This can reduce the matching complexity from
 *   O(n^2) to O(n). For example <literal>"Gtk*Box"</literal> can be turned into
 *   <literal>"Gtk?Box"</literal> and will still match #GtkHBox and #GtkVBox.
 *   </para></listitem>
 *  <listitem><para>
 *   The use of <literal>"*"</literal> wildcards should be restricted as much
 *   as possible, because matching <literal>"A*B*C*RestString"</literal> can
 *   result in matching complexities of O(n^2) worst case.
 *   </para></listitem>
 * </orderedlist>
 * </para>
 * </refsect2>
 * <refsect2>
 * <title>Toplevel declarations</title>
 * <para>
 * An RC file is a text file which is composed of a sequence
 * of declarations. <literal>'#'</literal> characters delimit comments and
 * the portion of a line after a <literal>'#'</literal> is ignored when parsing
 * an RC file.
 *
 * The possible toplevel declarations are:
 *
 * <variablelist>
 *   <varlistentry>
 *     <term><literal>binding <replaceable>name</replaceable>
 *      { ... }</literal></term>
 *     <listitem>
 *       <para>Declares a binding set.</para>
 *     </listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term><literal>class <replaceable>pattern</replaceable>
 *           [ style | binding ][ : <replaceable>priority</replaceable> ]
 *           <replaceable>name</replaceable></literal></term>
 *     <listitem>
 *      <para>Specifies a style or binding set for a particular
 *      branch of the inheritance hierarchy.</para>
 *     </listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term><literal>include <replaceable>filename</replaceable></literal></term>
 *     <listitem>
 *       <para>Parses another file at this point. If
 *         <replaceable>filename</replaceable> is not an absolute filename,
 *         it is searched in the directories of the currently open RC files.</para>
 *       <para>GTK+ also tries to load a
 *         <link linkend="locale-specific-rc">locale-specific variant</link> of
 *         the included file.</para>
 *     </listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term><literal>module_path <replaceable>path</replaceable></literal></term>
 *     <listitem>
 *       <para>Sets a path (a list of directories separated
 *       by colons) that will be searched for theme engines referenced in
 *       RC files.</para>
 *     </listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term><literal>pixmap_path <replaceable>path</replaceable></literal></term>
 *     <listitem>
 *       <para>Sets a path (a list of directories separated
 *       by colons) that will be searched for pixmaps referenced in
 *       RC files.</para>
 *     </listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term><literal>im_module_file <replaceable>pathname</replaceable></literal></term>
 *     <listitem>
 *       <para>Sets the pathname for the IM modules file. Setting this from RC files
 *       is deprecated; you should use the environment variable <envar>GTK_IM_MODULE_FILE</envar>
 *       instead.</para>
 *     </listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term><literal>style <replaceable>name</replaceable> [ =
 *     <replaceable>parent</replaceable> ] { ... }</literal></term>
 *     <listitem>
 *       <para>Declares a style.</para>
 *     </listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term><literal>widget <replaceable>pattern</replaceable>
 *           [ style | binding ][ : <replaceable>priority</replaceable> ]
 *           <replaceable>name</replaceable></literal></term>
 *     <listitem>
 *      <para>Specifies a style or binding set for a particular
 *      group of widgets by matching on the widget pathname.</para>
 *     </listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term><literal>widget_class <replaceable>pattern</replaceable>
 *           [ style | binding ][ : <replaceable>priority</replaceable> ]
 *           <replaceable>name</replaceable></literal></term>
 *     <listitem>
 *      <para>Specifies a style or binding set for a particular
 *      group of widgets by matching on the class pathname.</para>
 *     </listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term><replaceable>setting</replaceable> = <replaceable>value</replaceable></term>
 *     <listitem>
 *       <para>Specifies a value for a <link linkend="GtkSettings">setting</link>.
 *         Note that settings in RC files are overwritten by system-wide settings
 *         (which are managed by an XSettings manager on X11).</para>
 *     </listitem>
 *   </varlistentry>
 * </variablelist>
 * </para>
 * </refsect2>
 * <refsect2>
 * <title>Styles</title>
 * <para>
 * A RC style is specified by a <literal>style</literal>
 * declaration in a RC file, and then bound to widgets
 * with a <literal>widget</literal>, <literal>widget_class</literal>,
 * or <literal>class</literal> declaration. All styles
 * applying to a particular widget are composited together
 * with <literal>widget</literal> declarations overriding
 * <literal>widget_class</literal> declarations which, in
 * turn, override <literal>class</literal> declarations.
 * Within each type of declaration, later declarations override
 * earlier ones.
 *
 * Within a <literal>style</literal> declaration, the possible
 * elements are:
 *
 * <variablelist>
 *   <varlistentry>
 *     <term><literal>bg[<replaceable>state</replaceable>] =
 *       <replaceable>color</replaceable></literal></term>
 *      <listitem>
 *          Sets the color used for the background of most widgets.
 *      </listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term><literal>fg[<replaceable>state</replaceable>] =
 *       <replaceable>color</replaceable></literal></term>
 *      <listitem>
 *          Sets the color used for the foreground of most widgets.
 *      </listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term><literal>base[<replaceable>state</replaceable>] =
 *       <replaceable>color</replaceable></literal></term>
 *      <listitem>
 *          Sets the color used for the background of widgets displaying
 *          editable text. This color is used for the background
 *          of, among others, #GtkText, #GtkEntry, #GtkList, and #GtkCList.
 *      </listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term><literal>text[<replaceable>state</replaceable>] =
 *       <replaceable>color</replaceable></literal></term>
 *      <listitem>
 *          Sets the color used for foreground of widgets using
 *          <literal>base</literal> for the background color.
 *      </listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term><literal>xthickness =
 *       <replaceable>number</replaceable></literal></term>
 *      <listitem>
 *          Sets the xthickness, which is used for various horizontal padding
 *          values in GTK+.
 *      </listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term><literal>ythickness =
 *       <replaceable>number</replaceable></literal></term>
 *      <listitem>
 *          Sets the ythickness, which is used for various vertical padding
 *          values in GTK+.
 *      </listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term><literal>bg_pixmap[<replaceable>state</replaceable>] =
 *       <replaceable>pixmap</replaceable></literal></term>
 *      <listitem>
 *          Sets a background pixmap to be used in place of
 *          the <literal>bg</literal> color (or for #GtkText,
 *          in place of the <literal>base</literal> color. The special
 *          value <literal>"&lt;parent&gt;"</literal> may be used to indicate that the widget should
 *          use the same background pixmap as its parent. The special value
 *          <literal>"&lt;none&gt;"</literal> may be used to indicate no background pixmap.
 *      </listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term><literal>font = <replaceable>font</replaceable></literal></term>
 *      <listitem>
 *          Starting with GTK+ 2.0, the "font" and "fontset"
 *          declarations are ignored; use "font_name" declarations instead.
 *      </listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term><literal>fontset = <replaceable>font</replaceable></literal></term>
 *      <listitem>
 *          Starting with GTK+ 2.0, the "font" and "fontset"
 *          declarations are ignored; use "font_name" declarations instead.
 *      </listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term><literal>font_name = <replaceable>font</replaceable></literal></term>
 *      <listitem>
 *          Sets the font for a widget. <replaceable>font</replaceable> must be
 *          a Pango font name, e.g. <literal>"Sans Italic 10"</literal>.
 *          For details about Pango font names, see
 *          pango_font_description_from_string().
 *      </listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term><literal>stock[<replaceable>"stock-id"</replaceable>] = { <replaceable>icon source specifications</replaceable> }</literal></term>
 *      <listitem>
 *         Defines the icon for a stock item.
 *      </listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term><literal>color[<replaceable>"color-name"</replaceable>] = <replaceable>color specification</replaceable></literal></term>
 *      <listitem>
 *         Since 2.10, this element can be used to defines symbolic colors. See below for
 *         the syntax of color specifications.
 *      </listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term><literal>engine <replaceable>"engine"</replaceable> { <replaceable>engine-specific
 * settings</replaceable> }</literal></term>
 *      <listitem>
 *         Defines the engine to be used when drawing with this style.
 *      </listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term><literal><replaceable>class</replaceable>::<replaceable>property</replaceable> = <replaceable>value</replaceable></literal></term>
 *      <listitem>
 *         Sets a <link linkend="style-properties">style property</link> for a widget class.
 *      </listitem>
 *   </varlistentry>
 * </variablelist>
 *
 * The colors and background pixmaps are specified as a function of the
 * state of the widget. The states are:
 *
 * <variablelist>
 *   <varlistentry>
 *     <term><literal>NORMAL</literal></term>
 *     <listitem>
 *         A color used for a widget in its normal state.
 *     </listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term><literal>ACTIVE</literal></term>
 *     <listitem>
 *         A variant of the <literal>NORMAL</literal> color used when the
 *         widget is in the %GTK_STATE_ACTIVE state, and also for
 *         the trough of a ScrollBar, tabs of a NoteBook
 *         other than the current tab and similar areas.
 *         Frequently, this should be a darker variant
 *         of the <literal>NORMAL</literal> color.
 *     </listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term><literal>PRELIGHT</literal></term>
 *     <listitem>
 *         A color used for widgets in the %GTK_STATE_PRELIGHT state. This
 *         state is the used for Buttons and MenuItems
 *         that have the mouse cursor over them, and for
 *         their children.
 *     </listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term><literal>SELECTED</literal></term>
 *     <listitem>
 *         A color used to highlight data selected by the user.
 *         for instance, the selected items in a list widget, and the
 *         selection in an editable widget.
 *     </listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term><literal>INSENSITIVE</literal></term>
 *     <listitem>
 *         A color used for the background of widgets that have
 *         been set insensitive with gtk_widget_set_sensitive().
 *     </listitem>
 *   </varlistentry>
 * </variablelist>
 *
 * <anchor id="color-format"/>
 * Colors can be specified as a string containing a color name (GTK+ knows
 * all names from the X color database <filename>/usr/lib/X11/rgb.txt</filename>),
 * in one of the hexadecimal forms <literal>#rrrrggggbbbb</literal>,
 * <literal>#rrrgggbbb</literal>, <literal>#rrggbb</literal>,
 * or <literal>#rgb</literal>, where <literal>r</literal>,
 * <literal>g</literal> and <literal>b</literal> are
 * hex digits, or they can be specified as a triplet
 * <literal>{ <replaceable>r</replaceable>, <replaceable>g</replaceable>,
 * <replaceable>b</replaceable>}</literal>, where <literal>r</literal>,
 * <literal>g</literal> and <literal>b</literal> are either integers in
 * the range 0-65535 or floats in the range 0.0-1.0.
 *
 * Since 2.10, colors can also be specified by refering to a symbolic color, as
 * follows: <literal>@<!-- -->color-name</literal>, or by using expressions to combine
 * colors. The following expressions are currently supported:
 *   <variablelist>
 *     <varlistentry>
 *       <term>mix (<replaceable>factor</replaceable>, <replaceable>color1</replaceable>, <replaceable>color2</replaceable>)</term>
 *       <listitem><para>
 *         Computes a new color by mixing <replaceable>color1</replaceable> and
 *         <replaceable>color2</replaceable>. The <replaceable>factor</replaceable>
 *         determines how close the new color is to <replaceable>color1</replaceable>.
 *         A factor of 1.0 gives pure <replaceable>color1</replaceable>, a factor of
 *         0.0 gives pure <replaceable>color2</replaceable>.
 *       </para></listitem>
 *     </varlistentry>
 *     <varlistentry>
 *       <term>shade (<replaceable>factor</replaceable>, <replaceable>color</replaceable>)</term>
 *       <listitem><para>
 *         Computes a lighter or darker variant of <replaceable>color</replaceable>.
 *         A <replaceable>factor</replaceable> of 1.0 leaves the color unchanged, smaller
 *         factors yield darker colors, larger factors yield lighter colors.
 *       </para></listitem>
 *     </varlistentry>
 *     <varlistentry>
 *       <term>lighter (<replaceable>color</replaceable>)</term>
 *       <listitem><para>
 *         This is an abbreviation for
 *         <literal>shade (1.3, <replaceable>color</replaceable>)</literal>.
 *       </para></listitem>
 *     </varlistentry>
 *     <varlistentry>
 *       <term>darker (<replaceable>color</replaceable>)</term>
 *       <listitem><para>
 *         This is an abbreviation for
 *         <literal>shade (0.7, <replaceable>color</replaceable>)</literal>.
 *       </para></listitem>
 *     </varlistentry>
 *   </variablelist>
 *
 * Here are some examples of color expressions:
 *
 * <informalexample><programlisting>
 *  mix (0.5, "red", "blue")
 *  shade (1.5, mix (0.3, "#0abbc0", { 0.3, 0.5, 0.9 }))
 *  lighter (@<!-- -->foreground)
 * </programlisting></informalexample>
 *
 * In a <literal>stock</literal> definition, icon sources are specified as a
 * 4-tuple of image filename or icon name, text direction, widget state, and size, in that
 * order.  Each icon source specifies an image filename or icon name to use with a given
 * direction, state, and size. Filenames are specified as a string such
 * as <literal>"itemltr.png"</literal>, while icon names (looked up
 * in the current icon theme), are specified with a leading
 * <literal>@</literal>, such as <literal>@"item-ltr"</literal>.
 * The <literal>*</literal> character can be used as a
 * wildcard, and if direction/state/size are omitted they default to
 * <literal>*</literal>. So for example, the following specifies different icons to
 * use for left-to-right and right-to-left languages:
 *
 * <informalexample><programlisting>
 * stock["my-stock-item"] =
 * {
 *   { "itemltr.png", LTR, *, * },
 *   { "itemrtl.png", RTL, *, * }
 * }
 * </programlisting></informalexample>
 *
 * This could be abbreviated as follows:
 *
 * <informalexample><programlisting>
 * stock["my-stock-item"] =
 * {
 *   { "itemltr.png", LTR },
 *   { "itemrtl.png", RTL }
 * }
 * </programlisting></informalexample>
 *
 * You can specify custom icons for specific sizes, as follows:
 *
 * <informalexample><programlisting>
 * stock["my-stock-item"] =
 * {
 *   { "itemmenusize.png", *, *, "gtk-menu" },
 *   { "itemtoolbarsize.png", *, *, "gtk-large-toolbar" }
 *   { "itemgeneric.png" } // implicit *, *, * as a fallback
 * }
 * </programlisting></informalexample>
 *
 * The sizes that come with GTK+ itself are <literal>"gtk-menu"</literal>,
 * <literal>"gtk-small-toolbar"</literal>, <literal>"gtk-large-toolbar"</literal>,
 * <literal>"gtk-button"</literal>, <literal>"gtk-dialog"</literal>. Applications
 * can define other sizes.
 *
 * It's also possible to use custom icons for a given state, for example:
 *
 * <informalexample><programlisting>
 * stock["my-stock-item"] =
 * {
 *   { "itemprelight.png", *, PRELIGHT },
 *   { "iteminsensitive.png", *, INSENSITIVE },
 *   { "itemgeneric.png" } // implicit *, *, * as a fallback
 * }
 * </programlisting></informalexample>
 *
 * When selecting an icon source to use, GTK+ will consider text direction most
 * important, state second, and size third. It will select the best match based on
 * those criteria. If an attribute matches exactly (e.g. you specified
 * <literal>PRELIGHT</literal> or specified the size), GTK+ won't modify the image;
 * if the attribute matches with a wildcard, GTK+ will scale or modify the image to
 * match the state and size the user requested.
 * </para>
 * </refsect2>
 * <refsect2>
 * <title>Key bindings</title>
 * <para>
 * Key bindings allow the user to specify actions to be
 * taken on particular key presses. The form of a binding
 * set declaration is:
 *
 * <informalexample><programlisting>
 * binding <replaceable>name</replaceable> {
 *   bind <replaceable>key</replaceable> {
 *     <replaceable>signalname</replaceable> (<replaceable>param</replaceable>, ...)
 *     ...
 *   }
 *   ...
 * }
 * </programlisting></informalexample>
 *
 * <replaceable>key</replaceable> is a string consisting of a
 * series of modifiers followed by the name of a key. The
 * modifiers can be:
 * <simplelist>
 * <member><literal>&lt;alt&gt;</literal></member>
 * <member><literal>&lt;ctl&gt;</literal></member>
 * <member><literal>&lt;control&gt;</literal></member>
 * <member><literal>&lt;meta&gt;</literal></member>
 * <member><literal>&lt;hyper&gt;</literal></member>
 * <member><literal>&lt;super&gt;</literal></member>
 * <member><literal>&lt;mod1&gt;</literal></member>
 * <member><literal>&lt;mod2&gt;</literal></member>
 * <member><literal>&lt;mod3&gt;</literal></member>
 * <member><literal>&lt;mod4&gt;</literal></member>
 * <member><literal>&lt;mod5&gt;</literal></member>
 * <member><literal>&lt;release&gt;</literal></member>
 * <member><literal>&lt;shft&gt;</literal></member>
 * <member><literal>&lt;shift&gt;</literal></member>
 * </simplelist>
 * <literal>&lt;shft&gt;</literal> is an alias for
 * <literal>&lt;shift&gt;</literal>,
 * <literal>&lt;ctl&gt;</literal> is an alias for
 * <literal>&lt;control&gt;</literal>,
 *  and
 * <literal>&lt;alt&gt;</literal> is an alias for
 * <literal>&lt;mod1&gt;</literal>.
 *
 * The action that is bound to the key is a sequence
 * of signal names (strings) followed by parameters for
 * each signal. The signals must be action signals.
 * (See g_signal_new()). Each parameter can be
 * a float, integer, string, or unquoted string
 * representing an enumeration value. The types of
 * the parameters specified must match the types of the
 * parameters of the signal.
 *
 * Binding sets are connected to widgets in the same manner as styles,
 * with one difference: Binding sets override other binding sets first
 * by pattern type, then by priority and then by order of specification.
 * The priorities that can be specified and their default values are the
 * same as for styles.
 * </para>
 * </refsect2>
 */


enum 
{
  PATH_ELT_PSPEC,
  PATH_ELT_UNRESOLVED,
  PATH_ELT_TYPE
};

typedef struct
{
  gint type;
  union 
  {
    GType         class_type;
    gchar        *class_name;
    GPatternSpec *pspec;
  } elt;
} PathElt;

typedef struct {
  GSList *color_hashes;
} GtkRcStylePrivate;

#define GTK_RC_STYLE_GET_PRIVATE(obj) ((GtkRcStylePrivate *) gtk_rc_style_get_instance_private ((GtkRcStyle *) (obj)))

static void        gtk_rc_style_finalize             (GObject         *object);
static void        gtk_rc_style_real_merge           (GtkRcStyle      *dest,
                                                      GtkRcStyle      *src);
static GtkRcStyle* gtk_rc_style_real_create_rc_style (GtkRcStyle      *rc_style);
static GtkStyle*   gtk_rc_style_real_create_style    (GtkRcStyle      *rc_style);
static gint	   gtk_rc_properties_cmp	     (gconstpointer    bsearch_node1,
						      gconstpointer    bsearch_node2);

static void	   insert_rc_property		     (GtkRcStyle      *style,
						      GtkRcProperty   *property,
						      gboolean         replace);


static const GScannerConfig gtk_rc_scanner_config =
{
  (
   " \t\r\n"
   )			/* cset_skip_characters */,
  (
   "_"
   G_CSET_a_2_z
   G_CSET_A_2_Z
   )			/* cset_identifier_first */,
  (
   G_CSET_DIGITS
   "-_"
   G_CSET_a_2_z
   G_CSET_A_2_Z
   )			/* cset_identifier_nth */,
  ( "#\n" )		/* cpair_comment_single */,
  
  TRUE			/* case_sensitive */,
  
  TRUE			/* skip_comment_multi */,
  TRUE			/* skip_comment_single */,
  TRUE			/* scan_comment_multi */,
  TRUE			/* scan_identifier */,
  FALSE			/* scan_identifier_1char */,
  FALSE			/* scan_identifier_NULL */,
  TRUE			/* scan_symbols */,
  TRUE			/* scan_binary */,
  TRUE			/* scan_octal */,
  TRUE			/* scan_float */,
  TRUE			/* scan_hex */,
  TRUE			/* scan_hex_dollar */,
  TRUE			/* scan_string_sq */,
  TRUE			/* scan_string_dq */,
  TRUE			/* numbers_2_int */,
  FALSE			/* int_2_float */,
  FALSE			/* identifier_2_string */,
  TRUE			/* char_2_token */,
  TRUE			/* symbol_2_token */,
  FALSE			/* scope_0_fallback */,
};
 
static const gchar symbol_names[] = 
  "include\0"
  "NORMAL\0"
  "ACTIVE\0"
  "PRELIGHT\0"
  "SELECTED\0"
  "INSENSITIVE\0"
  "fg\0"
  "bg\0"
  "text\0"
  "base\0"
  "xthickness\0"
  "ythickness\0"
  "font\0"
  "fontset\0"
  "font_name\0"
  "bg_pixmap\0"
  "pixmap_path\0"
  "style\0"
  "binding\0"
  "bind\0"
  "widget\0"
  "widget_class\0"
  "class\0"
  "lowest\0"
  "gtk\0"
  "application\0"
  "theme\0"
  "rc\0"
  "highest\0"
  "engine\0"
  "module_path\0"
  "stock\0"
  "im_module_file\0"
  "LTR\0"
  "RTL\0"
  "color\0"
  "unbind\0";

static const struct
{
  guint name_offset;
  guint token;
} symbols[] = {
  {   0, GTK_RC_TOKEN_INCLUDE },
  {   8, GTK_RC_TOKEN_NORMAL },
  {  15, GTK_RC_TOKEN_ACTIVE },
  {  22, GTK_RC_TOKEN_PRELIGHT },
  {  31, GTK_RC_TOKEN_SELECTED },
  {  40, GTK_RC_TOKEN_INSENSITIVE },
  {  52, GTK_RC_TOKEN_FG },
  {  55, GTK_RC_TOKEN_BG },
  {  58, GTK_RC_TOKEN_TEXT },
  {  63, GTK_RC_TOKEN_BASE },
  {  68, GTK_RC_TOKEN_XTHICKNESS },
  {  79, GTK_RC_TOKEN_YTHICKNESS },
  {  90, GTK_RC_TOKEN_FONT },
  {  95, GTK_RC_TOKEN_FONTSET },
  { 103, GTK_RC_TOKEN_FONT_NAME },
  { 113, GTK_RC_TOKEN_BG_PIXMAP },
  { 123, GTK_RC_TOKEN_PIXMAP_PATH },
  { 135, GTK_RC_TOKEN_STYLE },
  { 141, GTK_RC_TOKEN_BINDING },
  { 149, GTK_RC_TOKEN_BIND },
  { 154, GTK_RC_TOKEN_WIDGET },
  { 161, GTK_RC_TOKEN_WIDGET_CLASS },
  { 174, GTK_RC_TOKEN_CLASS },
  { 180, GTK_RC_TOKEN_LOWEST },
  { 187, GTK_RC_TOKEN_GTK },
  { 191, GTK_RC_TOKEN_APPLICATION },
  { 203, GTK_RC_TOKEN_THEME },
  { 209, GTK_RC_TOKEN_RC },
  { 212, GTK_RC_TOKEN_HIGHEST },
  { 220, GTK_RC_TOKEN_ENGINE },
  { 227, GTK_RC_TOKEN_MODULE_PATH },
  { 239, GTK_RC_TOKEN_STOCK },
  { 245, GTK_RC_TOKEN_IM_MODULE_FILE },
  { 260, GTK_RC_TOKEN_LTR },
  { 264, GTK_RC_TOKEN_RTL },
  { 268, GTK_RC_TOKEN_COLOR },
  { 274, GTK_RC_TOKEN_UNBIND }
};

static GHashTable *realized_style_ht = NULL;

static gchar *im_module_file = NULL;

static gchar **gtk_rc_default_files = NULL;

/* RC file handling */

static gchar *
gtk_rc_make_default_dir (const gchar *type)
{
  const gchar *var;
  gchar *path;

  var = g_getenv ("GTK_EXE_PREFIX");

  if (var)
    path = g_build_filename (var, "lib", "gtk-3.0", GTK_BINARY_VERSION, type, NULL);
  else
    path = g_build_filename (_gtk_get_libdir (), "gtk-3.0", GTK_BINARY_VERSION, type, NULL);

  return path;
}

/**
 * gtk_rc_get_im_module_path:
 *
 * Obtains the path in which to look for IM modules. See the documentation
 * of the <link linkend="im-module-path"><envar>GTK_PATH</envar></link>
 * environment variable for more details about looking up modules. This
 * function is useful solely for utilities supplied with GTK+ and should
 * not be used by applications under normal circumstances.
 *
 * Returns: (type filename): a newly-allocated string containing the
 *    path in which to look for IM modules.
 *
 * Deprecated: 3.0: Use #GtkCssProvider instead.
 */
gchar *
gtk_rc_get_im_module_path (void)
{
  gchar **paths = _gtk_get_module_path ("immodules");
  gchar *result = g_strjoinv (G_SEARCHPATH_SEPARATOR_S, paths);
  g_strfreev (paths);

  return result;
}

/**
 * gtk_rc_get_im_module_file:
 *
 * Obtains the path to the IM modules file. See the documentation
 * of the <link linkend="im-module-file"><envar>GTK_IM_MODULE_FILE</envar></link>
 * environment variable for more details.
 *
 * Returns: (type filename): a newly-allocated string containing the
 *    name of the file listing the IM modules available for loading
 *
 * Deprecated: 3.0: Use #GtkCssProvider instead.
 */
gchar *
gtk_rc_get_im_module_file (void)
{
  const gchar *var = g_getenv ("GTK_IM_MODULE_FILE");
  gchar *result = NULL;

  if (var)
    result = g_strdup (var);

  if (!result)
    {
      if (im_module_file)
        result = g_strdup (im_module_file);
      else
        result = gtk_rc_make_default_dir ("immodules.cache");
    }

  return result;
}

/**
 * gtk_rc_get_theme_dir:
 *
 * Returns the standard directory in which themes should
 * be installed. (GTK+ does not actually use this directory
 * itself.)
 *
 * Returns: The directory (must be freed with g_free()).
 *
 * Deprecated: 3.0: Use #GtkCssProvider instead.
 */
gchar *
gtk_rc_get_theme_dir (void)
{
  const gchar *var;
  gchar *path;

  var = g_getenv ("GTK_DATA_PREFIX");

  if (var)
    path = g_build_filename (var, "share", "themes", NULL);
  else
    path = g_build_filename (_gtk_get_data_prefix (), "share", "themes", NULL);

  return path;
}

/**
 * gtk_rc_get_module_dir:
 *
 * Returns a directory in which GTK+ looks for theme engines.
 * For full information about the search for theme engines,
 * see the docs for <envar>GTK_PATH</envar> in
 * <xref linkend="gtk-running"/>.
 *
 * return value: (type filename): the directory. (Must be freed with g_free())
 *
 * Deprecated: 3.0: Use #GtkCssProvider instead.
 **/
gchar *
gtk_rc_get_module_dir (void)
{
  return gtk_rc_make_default_dir ("engines");
}

/**
 * gtk_rc_add_default_file:
 * @filename: (type filename): the pathname to the file. If @filename
 *    is not absolute, it is searched in the current directory.
 *
 * Adds a file to the list of files to be parsed at the
 * end of gtk_init().
 *
 * Deprecated:3.0: Use #GtkStyleContext with a custom #GtkStyleProvider instead
 **/
void
gtk_rc_add_default_file (const gchar *filename)
{
}

/**
 * gtk_rc_set_default_files:
 * @filenames: (array zero-terminated=1) (element-type filename): A
 *     %NULL-terminated list of filenames.
 *
 * Sets the list of files that GTK+ will read at the
 * end of gtk_init().
 *
 * Deprecated:3.0: Use #GtkStyleContext with a custom #GtkStyleProvider instead
 **/
void
gtk_rc_set_default_files (gchar **filenames)
{
}

/**
 * gtk_rc_get_default_files:
 *
 * Retrieves the current list of RC files that will be parsed
 * at the end of gtk_init().
 *
 * Return value: (transfer none) (array zero-terminated=1) (element-type filename):
 *      A %NULL-terminated array of filenames.  This memory is owned
 *     by GTK+ and must not be freed by the application.  If you want
 *     to store this information, you should make a copy.
 *
 * Deprecated:3.0: Use #GtkStyleContext instead
 **/
gchar **
gtk_rc_get_default_files (void)
{
  return gtk_rc_default_files;
}

/**
 * gtk_rc_parse_string:
 * @rc_string: a string to parse.
 *
 * Parses resource information directly from a string.
 *
 * Deprecated: 3.0: Use #GtkCssProvider instead.
 */
void
gtk_rc_parse_string (const gchar *rc_string)
{
  g_return_if_fail (rc_string != NULL);
}

/**
 * gtk_rc_parse:
 * @filename: the filename of a file to parse. If @filename is not absolute, it
 *  is searched in the current directory.
 *
 * Parses a given resource file.
 *
 * Deprecated: 3.0: Use #GtkCssProvider instead.
 */
void
gtk_rc_parse (const gchar *filename)
{
  g_return_if_fail (filename != NULL);
}

/* Handling of RC styles */

G_DEFINE_TYPE_WITH_PRIVATE (GtkRcStyle, gtk_rc_style, G_TYPE_OBJECT)

static void
gtk_rc_style_init (GtkRcStyle *style)
{
  GtkRcStylePrivate *priv = GTK_RC_STYLE_GET_PRIVATE (style);
  guint i;

  style->name = NULL;
  style->font_desc = NULL;

  for (i = 0; i < 5; i++)
    {
      static const GdkColor init_color = { 0, 0, 0, 0, };

      style->bg_pixmap_name[i] = NULL;
      style->color_flags[i] = 0;
      style->fg[i] = init_color;
      style->bg[i] = init_color;
      style->text[i] = init_color;
      style->base[i] = init_color;
    }
  style->xthickness = -1;
  style->ythickness = -1;
  style->rc_properties = NULL;

  style->rc_style_lists = NULL;
  style->icon_factories = NULL;

  priv->color_hashes = NULL;
}

static void
gtk_rc_style_class_init (GtkRcStyleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_rc_style_finalize;

  klass->parse = NULL;
  klass->create_rc_style = gtk_rc_style_real_create_rc_style;
  klass->merge = gtk_rc_style_real_merge;
  klass->create_style = gtk_rc_style_real_create_style;
}

static void
gtk_rc_style_finalize (GObject *object)
{
  GSList *tmp_list1, *tmp_list2;
  GtkRcStyle *rc_style;
  GtkRcStylePrivate *rc_priv;
  gint i;

  rc_style = GTK_RC_STYLE (object);
  rc_priv = GTK_RC_STYLE_GET_PRIVATE (rc_style);

  g_free (rc_style->name);
  if (rc_style->font_desc)
    pango_font_description_free (rc_style->font_desc);

  for (i = 0; i < 5; i++)
    g_free (rc_style->bg_pixmap_name[i]);

  /* Now remove all references to this rc_style from
   * realized_style_ht
   */
  tmp_list1 = rc_style->rc_style_lists;
  while (tmp_list1)
    {
      GSList *rc_styles = tmp_list1->data;
      GtkStyle *style = g_hash_table_lookup (realized_style_ht, rc_styles);
      g_object_unref (style);

      /* Remove the list of styles from the other rc_styles
       * in the list
       */
      tmp_list2 = rc_styles;
      while (tmp_list2)
        {
          GtkRcStyle *other_style = tmp_list2->data;

          if (other_style != rc_style)
            other_style->rc_style_lists = g_slist_remove_all (other_style->rc_style_lists,
							      rc_styles);
          tmp_list2 = tmp_list2->next;
        }

      /* And from the hash table itself
       */
      g_hash_table_remove (realized_style_ht, rc_styles);
      g_slist_free (rc_styles);

      tmp_list1 = tmp_list1->next;
    }
  g_slist_free (rc_style->rc_style_lists);

  if (rc_style->rc_properties)
    {
      guint i;

      for (i = 0; i < rc_style->rc_properties->len; i++)
	{
	  GtkRcProperty *node = &g_array_index (rc_style->rc_properties, GtkRcProperty, i);

	  g_free (node->origin);
	  g_value_unset (&node->value);
	}
      g_array_free (rc_style->rc_properties, TRUE);
      rc_style->rc_properties = NULL;
    }

  g_slist_foreach (rc_style->icon_factories, (GFunc) g_object_unref, NULL);
  g_slist_free (rc_style->icon_factories);

  g_slist_foreach (rc_priv->color_hashes, (GFunc) g_hash_table_unref, NULL);
  g_slist_free (rc_priv->color_hashes);

  G_OBJECT_CLASS (gtk_rc_style_parent_class)->finalize (object);
}

/**
 * gtk_rc_style_new:
 *
 * Creates a new #GtkRcStyle with no fields set and
 * a reference count of 1.
 *
 * Returns: the newly-created #GtkRcStyle
 *
 * Deprecated: 3.0: Use #GtkCssProvider instead.
 */
GtkRcStyle *
gtk_rc_style_new (void)
{
  GtkRcStyle *style;

  style = g_object_new (GTK_TYPE_RC_STYLE, NULL);

  return style;
}

/**
 * gtk_rc_style_copy:
 * @orig: the style to copy
 *
 * Makes a copy of the specified #GtkRcStyle. This function
 * will correctly copy an RC style that is a member of a class
 * derived from #GtkRcStyle.
 *
 * Return value: (transfer full): the resulting #GtkRcStyle
 *
 * Deprecated: 3.0: Use #GtkCssProvider instead.
 **/
GtkRcStyle *
gtk_rc_style_copy (GtkRcStyle *orig)
{
  GtkRcStyle *style;

  g_return_val_if_fail (GTK_IS_RC_STYLE (orig), NULL);
  
  style = GTK_RC_STYLE_GET_CLASS (orig)->create_rc_style (orig);
  GTK_RC_STYLE_GET_CLASS (style)->merge (style, orig);

  return style;
}

static GtkRcStyle *
gtk_rc_style_real_create_rc_style (GtkRcStyle *style)
{
  return g_object_new (G_OBJECT_TYPE (style), NULL);
}

static gint
gtk_rc_properties_cmp (gconstpointer bsearch_node1,
		       gconstpointer bsearch_node2)
{
  const GtkRcProperty *prop1 = bsearch_node1;
  const GtkRcProperty *prop2 = bsearch_node2;

  if (prop1->type_name == prop2->type_name)
    return prop1->property_name < prop2->property_name ? -1 : prop1->property_name == prop2->property_name ? 0 : 1;
  else
    return prop1->type_name < prop2->type_name ? -1 : 1;
}

static void
insert_rc_property (GtkRcStyle    *style,
		    GtkRcProperty *property,
		    gboolean       replace)
{
  guint i;
  GtkRcProperty *new_property = NULL;
  GtkRcProperty key = { 0, 0, NULL, { 0, }, };

  key.type_name = property->type_name;
  key.property_name = property->property_name;

  if (!style->rc_properties)
    style->rc_properties = g_array_new (FALSE, FALSE, sizeof (GtkRcProperty));

  i = 0;
  while (i < style->rc_properties->len)
    {
      gint cmp = gtk_rc_properties_cmp (&key, &g_array_index (style->rc_properties, GtkRcProperty, i));

      if (cmp == 0)
	{
	  if (replace)
	    {
	      new_property = &g_array_index (style->rc_properties, GtkRcProperty, i);
	      
	      g_free (new_property->origin);
	      g_value_unset (&new_property->value);
	      
	      *new_property = key;
	      break;
	    }
	  else
	    return;
	}
      else if (cmp < 0)
	break;

      i++;
    }

  if (!new_property)
    {
      g_array_insert_val (style->rc_properties, i, key);
      new_property = &g_array_index (style->rc_properties, GtkRcProperty, i);
    }

  new_property->origin = g_strdup (property->origin);
  g_value_init (&new_property->value, G_VALUE_TYPE (&property->value));
  g_value_copy (&property->value, &new_property->value);
}

static void
gtk_rc_style_real_merge (GtkRcStyle *dest,
			 GtkRcStyle *src)
{
  gint i;

  for (i = 0; i < 5; i++)
    {
      if (!dest->bg_pixmap_name[i] && src->bg_pixmap_name[i])
	dest->bg_pixmap_name[i] = g_strdup (src->bg_pixmap_name[i]);
      
      if (!(dest->color_flags[i] & GTK_RC_FG) && 
	  src->color_flags[i] & GTK_RC_FG)
	{
	  dest->fg[i] = src->fg[i];
	  dest->color_flags[i] |= GTK_RC_FG;
	}
      if (!(dest->color_flags[i] & GTK_RC_BG) && 
	  src->color_flags[i] & GTK_RC_BG)
	{
	  dest->bg[i] = src->bg[i];
	  dest->color_flags[i] |= GTK_RC_BG;
	}
      if (!(dest->color_flags[i] & GTK_RC_TEXT) && 
	  src->color_flags[i] & GTK_RC_TEXT)
	{
	  dest->text[i] = src->text[i];
	  dest->color_flags[i] |= GTK_RC_TEXT;
	}
      if (!(dest->color_flags[i] & GTK_RC_BASE) && 
	  src->color_flags[i] & GTK_RC_BASE)
	{
	  dest->base[i] = src->base[i];
	  dest->color_flags[i] |= GTK_RC_BASE;
	}
    }

  if (dest->xthickness < 0 && src->xthickness >= 0)
    dest->xthickness = src->xthickness;
  if (dest->ythickness < 0 && src->ythickness >= 0)
    dest->ythickness = src->ythickness;

  if (src->font_desc)
    {
      if (!dest->font_desc)
	dest->font_desc = pango_font_description_copy (src->font_desc);
      else
	pango_font_description_merge (dest->font_desc, src->font_desc, FALSE);
    }

  if (src->rc_properties)
    {
      guint i;

      for (i = 0; i < src->rc_properties->len; i++)
	insert_rc_property (dest,
			    &g_array_index (src->rc_properties, GtkRcProperty, i),
			    FALSE);
    }
}

static GtkStyle *
gtk_rc_style_real_create_style (GtkRcStyle *rc_style)
{
  return gtk_style_new ();
}

/**
 * gtk_rc_reset_styles:
 * @settings: a #GtkSettings
 *
 * This function recomputes the styles for all widgets that use a
 * particular #GtkSettings object. (There is one #GtkSettings object
 * per #GdkScreen, see gtk_settings_get_for_screen()); It is useful
 * when some global parameter has changed that affects the appearance
 * of all widgets, because when a widget gets a new style, it will
 * both redraw and recompute any cached information about its
 * appearance. As an example, it is used when the default font size
 * set by the operating system changes. Note that this function
 * doesn't affect widgets that have a style set explicitely on them
 * with gtk_widget_set_style().
 *
 * Since: 2.4
 *
 * Deprecated: 3.0: Use #GtkCssProvider instead.
 **/
void
gtk_rc_reset_styles (GtkSettings *settings)
{
  gtk_style_context_reset_widgets (_gtk_settings_get_screen (settings));
}

/**
 * gtk_rc_reparse_all_for_settings:
 * @settings: a #GtkSettings
 * @force_load: load whether or not anything changed
 *
 * If the modification time on any previously read file
 * for the given #GtkSettings has changed, discard all style information
 * and then reread all previously read RC files.
 *
 * Return value: %TRUE if the files were reread.
 *
 * Deprecated: 3.0: Use #GtkCssProvider instead.
 **/
gboolean
gtk_rc_reparse_all_for_settings (GtkSettings *settings,
				 gboolean     force_load)
{
  return FALSE;
}

/**
 * gtk_rc_reparse_all:
 *
 * If the modification time on any previously read file for the
 * default #GtkSettings has changed, discard all style information
 * and then reread all previously read RC files.
 *
 * Return value:  %TRUE if the files were reread.
 *
 * Deprecated: 3.0: Use #GtkCssProvider instead.
 **/
gboolean
gtk_rc_reparse_all (void)
{
  return FALSE;
}

/**
 * gtk_rc_get_style:
 * @widget: a #GtkWidget
 *
 * Finds all matching RC styles for a given widget,
 * composites them together, and then creates a
 * #GtkStyle representing the composite appearance.
 * (GTK+ actually keeps a cache of previously
 * created styles, so a new style may not be
 * created.)
 *
 * Returns: (transfer none): the resulting style. No refcount is added
 *   to the returned style, so if you want to save this style around,
 *   you should add a reference yourself.
 *
 * Deprecated:3.0: Use #GtkStyleContext instead
 **/
GtkStyle *
gtk_rc_get_style (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  gtk_widget_ensure_style (widget);

  return gtk_widget_get_style (widget);
}

/**
 * gtk_rc_get_style_by_paths:
 * @settings: a #GtkSettings object
 * @widget_path: (allow-none): the widget path to use when looking up the
 *     style, or %NULL if no matching against the widget path should be done
 * @class_path: (allow-none): the class path to use when looking up the style,
 *     or %NULL if no matching against the class path should be done.
 * @type: a type that will be used along with parent types of this type
 *     when matching against class styles, or #G_TYPE_NONE
 *
 * Creates up a #GtkStyle from styles defined in a RC file by providing
 * the raw components used in matching. This function may be useful
 * when creating pseudo-widgets that should be themed like widgets but
 * don't actually have corresponding GTK+ widgets. An example of this
 * would be items inside a GNOME canvas widget.
 *
 * The action of gtk_rc_get_style() is similar to:
 * |[
 *  gtk_widget_path (widget, NULL, &path, NULL);
 *  gtk_widget_class_path (widget, NULL, &class_path, NULL);
 *  gtk_rc_get_style_by_paths (gtk_widget_get_settings (widget),
 *                             path, class_path,
 *                             G_OBJECT_TYPE (widget));
 * ]|
 *
 * Return value: (transfer none): A style created by matching with the
 *     supplied paths, or %NULL if nothing matching was specified and the
 *     default style should be used. The returned value is owned by GTK+
 *     as part of an internal cache, so you must call g_object_ref() on
 *     the returned value if you want to keep a reference to it.
 *
 * Deprecated:3.0: Use #GtkStyleContext instead
 **/
GtkStyle *
gtk_rc_get_style_by_paths (GtkSettings *settings,
			   const char  *widget_path,
			   const char  *class_path,
			   GType        type)
{
  GtkWidgetPath *path;
  GtkStyle *style;

  path = gtk_widget_path_new ();

  /* For compatibility, we return a GtkStyle based on a GtkStyleContext
   * with a GtkWidgetPath appropriate for the supplied information.
   *
   * GtkWidgetPath is composed of a list of GTypes with optional names;
   * In GTK+-2.0, widget_path consisted of the widget names, or
   * the class names for unnamed widgets, while class_path had the
   * class names always. So, use class_path to determine the GTypes
   * and extract widget names from widget_path as applicable.
   */
  if (class_path == NULL)
    {
      gtk_widget_path_append_type (path, type == G_TYPE_NONE ? GTK_TYPE_WIDGET : type);
    }
  else
    {
      const gchar *widget_p, *widget_next;
      const gchar *class_p, *class_next;

      widget_next = widget_path;
      class_next = class_path;

      while (*class_next)
	{
	  GType component_type;
	  gchar *component_class;
	  gchar *component_name;
	  gint pos;

	  class_p = class_next;
	  if (*class_p == '.')
	    class_p++;

	  widget_p = widget_next; /* Might be NULL */
	  if (widget_p && *widget_p == '.')
	    widget_p++;

	  class_next = strchr (class_p, '.');
	  if (class_next == NULL)
	    class_next = class_p + strlen (class_p);

	  if (widget_p)
	    {
	      widget_next = strchr (widget_p, '.');
	      if (widget_next == NULL)
		widget_next = widget_p + strlen (widget_p);
	    }

	  component_class = g_strndup (class_p, class_next - class_p);
	  if (widget_p && *widget_p)
	    component_name = g_strndup (widget_p, widget_next - widget_p);
	  else
	    component_name = NULL;

	  component_type = g_type_from_name (component_class);
	  if (component_type == G_TYPE_INVALID)
	    component_type = GTK_TYPE_WIDGET;

	  pos = gtk_widget_path_append_type (path, component_type);
	  if (component_name != NULL && strcmp (component_name, component_name) != 0)
	    gtk_widget_path_iter_set_name (path, pos, component_name);

	  g_free (component_class);
	  g_free (component_name);
	}
    }

  style = _gtk_style_new_for_path (_gtk_settings_get_screen (settings),
				   path);

  gtk_widget_path_free (path);

  return style;
}

/**
 * gtk_rc_scanner_new: (skip)
 *
 * Deprecated:3.0: Use #GtkCssProvider instead
 */
GScanner*
gtk_rc_scanner_new (void)
{
  return g_scanner_new (&gtk_rc_scanner_config);
}

/*********************
 * Parsing functions *
 *********************/

static gboolean
lookup_color (GtkRcStyle *style,
              const char *color_name,
              GdkColor   *color)
{
  GtkRcStylePrivate *priv = GTK_RC_STYLE_GET_PRIVATE (style);
  GSList *iter;

  for (iter = priv->color_hashes; iter != NULL; iter = iter->next)
    {
      GHashTable *hash  = iter->data;
      GdkColor   *match = g_hash_table_lookup (hash, color_name);

      if (match)
        {
          color->red = match->red;
          color->green = match->green;
          color->blue = match->blue;
          return TRUE;
        }
    }

  return FALSE;
}

/**
 * gtk_rc_find_pixmap_in_path:
 * @settings: a #GtkSettings
 * @scanner: Scanner used to get line number information for the
 *   warning message, or %NULL
 * @pixmap_file: name of the pixmap file to locate.
 *
 * Looks up a file in pixmap path for the specified #GtkSettings.
 * If the file is not found, it outputs a warning message using
 * g_warning() and returns %NULL.
 *
 * Return value: (type filename): the filename.
 *
 * Deprecated: 3.0: Use #GtkCssProvider instead.
 **/
gchar*
gtk_rc_find_pixmap_in_path (GtkSettings  *settings,
			    GScanner     *scanner,
			    const gchar  *pixmap_file)
{
  g_warning ("Unable to locate image file in pixmap_path: \"%s\"",
             pixmap_file);
  return NULL;
}

/**
 * gtk_rc_find_module_in_path:
 * @module_file: name of a theme engine
 *
 * Searches for a theme engine in the GTK+ search path. This function
 * is not useful for applications and should not be used.
 *
 * Return value: (type filename): The filename, if found (must be
 *   freed with g_free()), otherwise %NULL.
 *
 * Deprecated: 3.0: Use #GtkCssProvider instead.
 **/
gchar*
gtk_rc_find_module_in_path (const gchar *module_file)
{
  return _gtk_find_module (module_file, "engines");
}

/**
 * gtk_rc_parse_state:
 * @scanner: a #GScanner (must be initialized for parsing an RC file)
 * @state: (out): A pointer to a #GtkStateType variable in which to
 *  store the result.
 *
 * Parses a #GtkStateType variable from the format expected
 * in a RC file.
 *
 * Returns: %G_TOKEN_NONE if parsing succeeded, otherwise the token
 *   that was expected but not found.
 *
 * Deprecated: 3.0: Use #GtkCssProvider instead
 */
guint
gtk_rc_parse_state (GScanner	 *scanner,
		    GtkStateType *state)
{
  guint old_scope;
  guint token;

  g_return_val_if_fail (scanner != NULL, G_TOKEN_ERROR);
  g_return_val_if_fail (state != NULL, G_TOKEN_ERROR);
  
  /* we don't know where we got called from, so we reset the scope here.
   * if we bail out due to errors, we *don't* reset the scope, so the
   * error messaging code can make sense of our tokens.
   */
  old_scope = g_scanner_set_scope (scanner, 0);
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_LEFT_BRACE)
    return G_TOKEN_LEFT_BRACE;
  
  token = g_scanner_get_next_token (scanner);
  switch (token)
    {
    case GTK_RC_TOKEN_ACTIVE:
      *state = GTK_STATE_ACTIVE;
      break;
    case GTK_RC_TOKEN_INSENSITIVE:
      *state = GTK_STATE_INSENSITIVE;
      break;
    case GTK_RC_TOKEN_NORMAL:
      *state = GTK_STATE_NORMAL;
      break;
    case GTK_RC_TOKEN_PRELIGHT:
      *state = GTK_STATE_PRELIGHT;
      break;
    case GTK_RC_TOKEN_SELECTED:
      *state = GTK_STATE_SELECTED;
      break;
    default:
      return /* G_TOKEN_SYMBOL */ GTK_RC_TOKEN_NORMAL;
    }
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_RIGHT_BRACE)
    return G_TOKEN_RIGHT_BRACE;
  
  g_scanner_set_scope (scanner, old_scope);

  return G_TOKEN_NONE;
}

/**
 * gtk_rc_parse_priority:
 * @scanner: a #GtkScanner (must be initialized for parsing an RC file)
 * @priority: A pointer to #GtkPathPriorityType variable in which
 *  to store the result.
 *
 * Parses a #GtkPathPriorityType variable from the format expected
 * in a RC file.
 *
 * Returns: %G_TOKEN_NONE if parsing succeeded, otherwise the token
 *   that was expected but not found.
 *
 * Deprecated:3.0: Use #GtkCssProvider instead
 */
guint
gtk_rc_parse_priority (GScanner	           *scanner,
		       GtkPathPriorityType *priority)
{
  guint old_scope;
  guint token;

  g_return_val_if_fail (scanner != NULL, G_TOKEN_ERROR);
  g_return_val_if_fail (priority != NULL, G_TOKEN_ERROR);

  /* we don't know where we got called from, so we reset the scope here.
   * if we bail out due to errors, we *don't* reset the scope, so the
   * error messaging code can make sense of our tokens.
   */
  old_scope = g_scanner_set_scope (scanner, 0);
  
  token = g_scanner_get_next_token (scanner);
  if (token != ':')
    return ':';
  
  token = g_scanner_get_next_token (scanner);
  switch (token)
    {
    case GTK_RC_TOKEN_LOWEST:
      *priority = GTK_PATH_PRIO_LOWEST;
      break;
    case GTK_RC_TOKEN_GTK:
      *priority = GTK_PATH_PRIO_GTK;
      break;
    case GTK_RC_TOKEN_APPLICATION:
      *priority = GTK_PATH_PRIO_APPLICATION;
      break;
    case GTK_RC_TOKEN_THEME:
      *priority = GTK_PATH_PRIO_THEME;
      break;
    case GTK_RC_TOKEN_RC:
      *priority = GTK_PATH_PRIO_RC;
      break;
    case GTK_RC_TOKEN_HIGHEST:
      *priority = GTK_PATH_PRIO_HIGHEST;
      break;
    default:
      return /* G_TOKEN_SYMBOL */ GTK_RC_TOKEN_APPLICATION;
    }
  
  g_scanner_set_scope (scanner, old_scope);

  return G_TOKEN_NONE;
}

/**
 * gtk_rc_parse_color:
 * @scanner: a #GScanner
 * @color: (out): a pointer to a #GdkColor structure in which to store
 *     the result
 *
 * Parses a color in the <link linkend="color=format">format</link> expected
 * in a RC file.
 *
 * Note that theme engines should use gtk_rc_parse_color_full() in
 * order to support symbolic colors.
 *
 * Returns: %G_TOKEN_NONE if parsing succeeded, otherwise the token
 *     that was expected but not found
 *
 * Deprecated:3.0: Use #GtkCssProvider instead
 */
guint
gtk_rc_parse_color (GScanner *scanner,
		    GdkColor *color)
{
  return gtk_rc_parse_color_full (scanner, NULL, color);
}

/**
 * gtk_rc_parse_color_full:
 * @scanner: a #GScanner
 * @style: (allow-none): a #GtkRcStyle, or %NULL
 * @color: (out): a pointer to a #GdkColor structure in which to store
 *     the result
 *
 * Parses a color in the <link linkend="color=format">format</link> expected
 * in a RC file. If @style is not %NULL, it will be consulted to resolve
 * references to symbolic colors.
 *
 * Returns: %G_TOKEN_NONE if parsing succeeded, otherwise the token
 *     that was expected but not found
 *
 * Since: 2.12
 *
 * Deprecated:3.0: Use #GtkCssProvider instead
 */
guint
gtk_rc_parse_color_full (GScanner   *scanner,
                         GtkRcStyle *style,
                         GdkColor   *color)
{
  guint token;

  g_return_val_if_fail (scanner != NULL, G_TOKEN_ERROR);

  /* we don't need to set our own scope here, because
   * we don't need own symbols
   */
  
  token = g_scanner_get_next_token (scanner);
  switch (token)
    {
      gint token_int;
      GdkColor c1, c2;
      gboolean negate;
      gdouble l;

    case G_TOKEN_LEFT_CURLY:
      token = g_scanner_get_next_token (scanner);
      if (token == G_TOKEN_INT)
	token_int = scanner->value.v_int;
      else if (token == G_TOKEN_FLOAT)
	token_int = scanner->value.v_float * 65535.0;
      else
	return G_TOKEN_FLOAT;
      color->red = CLAMP (token_int, 0, 65535);
      
      token = g_scanner_get_next_token (scanner);
      if (token != G_TOKEN_COMMA)
	return G_TOKEN_COMMA;
      
      token = g_scanner_get_next_token (scanner);
      if (token == G_TOKEN_INT)
	token_int = scanner->value.v_int;
      else if (token == G_TOKEN_FLOAT)
	token_int = scanner->value.v_float * 65535.0;
      else
	return G_TOKEN_FLOAT;
      color->green = CLAMP (token_int, 0, 65535);
      
      token = g_scanner_get_next_token (scanner);
      if (token != G_TOKEN_COMMA)
	return G_TOKEN_COMMA;
      
      token = g_scanner_get_next_token (scanner);
      if (token == G_TOKEN_INT)
	token_int = scanner->value.v_int;
      else if (token == G_TOKEN_FLOAT)
	token_int = scanner->value.v_float * 65535.0;
      else
	return G_TOKEN_FLOAT;
      color->blue = CLAMP (token_int, 0, 65535);
      
      token = g_scanner_get_next_token (scanner);
      if (token != G_TOKEN_RIGHT_CURLY)
	return G_TOKEN_RIGHT_CURLY;
      return G_TOKEN_NONE;
      
    case G_TOKEN_STRING:
      if (!gdk_color_parse (scanner->value.v_string, color))
	{
          g_scanner_warn (scanner, "Invalid color constant '%s'",
                          scanner->value.v_string);
          return G_TOKEN_STRING;
	}
      return G_TOKEN_NONE;

    case '@':
      token = g_scanner_get_next_token (scanner);
      if (token != G_TOKEN_IDENTIFIER)
	return G_TOKEN_IDENTIFIER;

      if (!style || !lookup_color (style, scanner->value.v_identifier, color))
        {
          g_scanner_warn (scanner, "Invalid symbolic color '%s'",
                          scanner->value.v_identifier);
          return G_TOKEN_IDENTIFIER;
        }

      return G_TOKEN_NONE;

    case G_TOKEN_IDENTIFIER:
      if (strcmp (scanner->value.v_identifier, "mix") == 0)
        {
          token = g_scanner_get_next_token (scanner);
          if (token != G_TOKEN_LEFT_PAREN)
            return G_TOKEN_LEFT_PAREN;

          negate = FALSE;
          if (g_scanner_peek_next_token (scanner) == '-')
            {
              g_scanner_get_next_token (scanner); /* eat sign */
              negate = TRUE;
            }

          token = g_scanner_get_next_token (scanner);
          if (token != G_TOKEN_FLOAT)
            return G_TOKEN_FLOAT;

          l = negate ? -scanner->value.v_float : scanner->value.v_float;

          token = g_scanner_get_next_token (scanner);
          if (token != G_TOKEN_COMMA)
            return G_TOKEN_COMMA;

          token = gtk_rc_parse_color_full (scanner, style, &c1);
          if (token != G_TOKEN_NONE)
            return token;

	  token = g_scanner_get_next_token (scanner);
	  if (token != G_TOKEN_COMMA)
            return G_TOKEN_COMMA;

	  token = gtk_rc_parse_color_full (scanner, style, &c2);
	  if (token != G_TOKEN_NONE)
            return token;

	  token = g_scanner_get_next_token (scanner);
	  if (token != G_TOKEN_RIGHT_PAREN)
            return G_TOKEN_RIGHT_PAREN;

	  color->red   = l * c1.red   + (1.0 - l) * c2.red;
	  color->green = l * c1.green + (1.0 - l) * c2.green;
	  color->blue  = l * c1.blue  + (1.0 - l) * c2.blue;

	  return G_TOKEN_NONE;
	}
      else if (strcmp (scanner->value.v_identifier, "shade") == 0)
        {
	  token = g_scanner_get_next_token (scanner);
          if (token != G_TOKEN_LEFT_PAREN)
            return G_TOKEN_LEFT_PAREN;

          negate = FALSE;
          if (g_scanner_peek_next_token (scanner) == '-')
            {
              g_scanner_get_next_token (scanner); /* eat sign */
              negate = TRUE;
            }

          token = g_scanner_get_next_token (scanner);
          if (token != G_TOKEN_FLOAT)
            return G_TOKEN_FLOAT;

          l = negate ? -scanner->value.v_float : scanner->value.v_float;

          token = g_scanner_get_next_token (scanner);
          if (token != G_TOKEN_COMMA)
            return G_TOKEN_COMMA;

          token = gtk_rc_parse_color_full (scanner, style, &c1);
          if (token != G_TOKEN_NONE)
            return token;

          token = g_scanner_get_next_token (scanner);
          if (token != G_TOKEN_RIGHT_PAREN)
            return G_TOKEN_RIGHT_PAREN;

          _gtk_style_shade (&c1, color, l);

          return G_TOKEN_NONE;
        }
      else if (strcmp (scanner->value.v_identifier, "lighter") == 0 ||
               strcmp (scanner->value.v_identifier, "darker") == 0)
        {
          if (scanner->value.v_identifier[0] == 'l')
            l = 1.3;
          else
	    l = 0.7;

	  token = g_scanner_get_next_token (scanner);
          if (token != G_TOKEN_LEFT_PAREN)
            return G_TOKEN_LEFT_PAREN;

          token = gtk_rc_parse_color_full (scanner, style, &c1);
          if (token != G_TOKEN_NONE)
            return token;

          token = g_scanner_get_next_token (scanner);
          if (token != G_TOKEN_RIGHT_PAREN)
            return G_TOKEN_RIGHT_PAREN;

          _gtk_style_shade (&c1, color, l);

          return G_TOKEN_NONE;
        }
      else
        return G_TOKEN_IDENTIFIER;

    default:
      return G_TOKEN_STRING;
    }
}
