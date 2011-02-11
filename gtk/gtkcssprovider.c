/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
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

#include "config.h"

#include <string.h>
#include <stdlib.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cairo-gobject.h>

#include "gtkcssproviderprivate.h"

#include "gtkanimationdescription.h"
#include "gtk9slice.h"
#include "gtkgradient.h"
#include "gtkthemingengine.h"
#include "gtkstyleprovider.h"
#include "gtkstylecontextprivate.h"
#include "gtkbindings.h"
#include "gtkprivate.h"

/**
 * SECTION:gtkcssprovider
 * @Short_description: CSS-like styling for widgets
 * @Title: GtkCssProvider
 * @See_also: #GtkStyleContext, #GtkStyleProvider
 *
 * GtkCssProvider is an object implementing the #GtkStyleProvider interface.
 * It is able to parse <ulink url="http://www.w3.org/TR/CSS2">CSS</ulink>-like
 * input in order to style widgets.
 *
 * <refsect2 id="gtkcssprovider-files">
 * <title>Default files</title>
 * <para>
 * An application can cause GTK+ to parse a specific CSS style sheet by
 * calling gtk_css_provider_load_from_file() and adding the provider with
 * gtk_style_context_add_provider() or gtk_style_context_add_provider_for_screen().
 * In addition, certain files will be read when GTK+ is initialized. First,
 * the file <filename><envar>$XDG_CONFIG_HOME</envar>/gtk-3.0/gtk.css</filename>
 * is loaded if it exists. Then, GTK+ tries to load
 * <filename><envar>$HOME</envar>/.themes/<replaceable>theme-name</replaceable>/gtk-3.0/gtk.css</filename>,
 * falling back to
 * <filename><replaceable>datadir</replaceable>/share/themes/<replaceable>theme-name</replaceable>/gtk-3.0/gtk.css</filename>,
 * where <replaceable>theme-name</replaceable> is the name of the current theme
 * (see the #GtkSettings:gtk-theme-name setting) and <replaceable>datadir</replaceable>
 * is the prefix configured when GTK+ was compiled, unless overridden by the
 * <envar>GTK_DATA_PREFIX</envar> environment variable.
 * </para>
 * </refsect2>
 * <refsect2 id="gtkcssprovider-stylesheets">
 * <title>Style sheets</title>
 * <para>
 * The basic structure of the style sheets understood by this provider is
 * a series of statements, which are either rule sets or '@-rules', separated
 * by whitespace.
 * </para>
 * <para>
 * A rule set consists of a selector and a declaration block, which is
 * a series of declarations enclosed in curly braces ({ and }). The
 * declarations are separated by semicolons (;). Multiple selectors can
 * share the same declaration block, by putting all the separators in
 * front of the block, separated by commas.
 * </para>
 * <example><title>A rule set with two selectors</title>
 * <programlisting language="text">
 * GtkButton, GtkEntry {
 *     color: &num;ff00ea;
 *     font: Comic Sans 12
 * }
 * </programlisting>
 * </example>
 * </refsect2>
 * <refsect2 id="gtkcssprovider-selectors">
 * <title>Selectors</title>
 * <para>
 * Selectors work very similar to the way they do in CSS, with widget class
 * names taking the role of element names, and widget names taking the role
 * of IDs. When used in a selector, widget names must be prefixed with a
 * '&num;' character. The '*' character represents the so-called universal
 * selector, which matches any widget.
 * </para>
 * <para>
 * To express more complicated situations, selectors can be combined in
 * various ways:
 * <itemizedlist>
 * <listitem><para>To require that a widget satisfies several conditions,
 *   combine several selectors into one by concatenating them. E.g.
 *   <literal>GtkButton&num;button1</literal> matches a GtkButton widget
 *   with the name button1.</para></listitem>
 * <listitem><para>To only match a widget when it occurs inside some other
 *   widget, write the two selectors after each other, separated by whitespace.
 *   E.g. <literal>GtkToolBar GtkButton</literal> matches GtkButton widgets
 *   that occur inside a GtkToolBar.</para></listitem>
 * <listitem><para>In the previous example, the GtkButton is matched even
 *   if it occurs deeply nested inside the toolbar. To restrict the match
 *   to direct children of the parent widget, insert a '>' character between
 *   the two selectors. E.g. <literal>GtkNotebook > GtkLabel</literal> matches
 *   GtkLabel widgets that are direct children of a GtkNotebook.</para></listitem>
 * </itemizedlist>
 * </para>
 * <example>
 * <title>Widget classes and names in selectors</title>
 * <programlisting language="text">
 * /&ast; Theme labels that are descendants of a window &ast;/
 * GtkWindow GtkLabel {
 *     background-color: &num;898989
 * }
 *
 * /&ast; Theme notebooks, and anything that's within these &ast;/
 * GtkNotebook {
 *     background-color: &num;a939f0
 * }
 *
 * /&ast; Theme combo boxes, and entries that
 *  are direct children of a notebook &ast;/
 * GtkComboBox,
 * GtkNotebook > GtkEntry {
 *     color: @fg_color;
 *     background-color: &num;1209a2
 * }
 *
 * /&ast; Theme any widget within a GtkBin &ast;/
 * GtkBin * {
 *     font-name: Sans 20
 * }
 *
 * /&ast; Theme a label named title-label &ast;/
 * GtkLabel&num;title-label {
 *     font-name: Sans 15
 * }
 *
 * /&ast; Theme any widget named main-entry &ast;/
 * &num;main-entry {
 *     background-color: &num;f0a810
 * }
 * </programlisting>
 * </example>
 * <para>
 * Widgets may also define style classes, which can be used for matching.
 * When used in a selector, style classes must be prefixed with a '.'
 * character.
 * </para>
 * <para>
 * Refer to the documentation of individual widgets to learn which
 * style classes they define and see <xref linkend="gtkstylecontext-classes"/>
 * for a list of all style classes used by GTK+ widgets.
 * </para>
 * <para>
 * Note that there is some ambiguity in the selector syntax when it comes
 * to differentiation widget class names from regions. GTK+ currently treats
 * a string as a widget class name if it contains any uppercase characters
 * (which should work for more widgets with names like GtkLabel).
 * </para>
 * <example>
 * <title>Style classes in selectors</title>
 * <programlisting language="text">
 * /&ast; Theme all widgets defining the class entry &ast;/
 * .entry {
 *     color: &num;39f1f9;
 * }
 *
 * /&ast; Theme spinbuttons' entry &ast;/
 * GtkSpinButton.entry {
 *     color: &num;900185
 * }
 * </programlisting>
 * </example>
 * <para>
 * In complicated widgets like e.g. a GtkNotebook, it may be desirable
 * to style different parts of the widget differently. To make this
 * possible, container widgets may define regions, whose names
 * may be used for matching in selectors.
 * </para>
 * <para>
 * Some containers allow to further differentiate between regions by
 * applying so-called pseudo-classes to the region. For example, the
 * tab region in GtkNotebook allows to single out the first or last
 * tab by using the :first-child or :last-child pseudo-class.
 * When used in selectors, pseudo-classes must be prefixed with a
 * ':' character.
 * </para>
 * <para>
 * Refer to the documentation of individual widgets to learn which
 * regions and pseudo-classes they define and see
 * <xref linkend="gtkstylecontext-classes"/> for a list of all regions
 * used by GTK+ widgets.
 * </para>
 * <example>
 * <title>Regions in selectors</title>
 * <programlisting language="text">
 * /&ast; Theme any label within a notebook &ast;/
 * GtkNotebook GtkLabel {
 *     color: &num;f90192;
 * }
 *
 * /&ast; Theme labels within notebook tabs &ast;/
 * GtkNotebook tab GtkLabel {
 *     color: &num;703910;
 * }
 *
 * /&ast; Theme labels in the any first notebook
 *  tab, both selectors are equivalent &ast;/
 * GtkNotebook tab:nth-child(first) GtkLabel,
 * GtkNotebook tab:first-child GtkLabel {
 *     color: &num;89d012;
 * }
 * </programlisting>
 * </example>
 * <para>
 * Another use of pseudo-classes is to match widgets depending on their
 * state. This is conceptually similar to the :hover, :active or :focus
 * pseudo-classes in CSS. The available pseudo-classes for widget states
 * are :active, :prelight (or :hover), :insensitive, :selected, :focused
 * and :inconsistent.
 * </para>
 * <example>
 * <title>Styling specific widget states</title>
 * <programlisting language="text">
 * /&ast; Theme active (pressed) buttons &ast;/
 * GtkButton:active {
 *     background-color: &num;0274d9;
 * }
 *
 * /&ast; Theme buttons with the mouse pointer on it,
 *    both are equivalent &ast;/
 * GtkButton:hover,
 * GtkButton:prelight {
 *     background-color: &num;3085a9;
 * }
 *
 * /&ast; Theme insensitive widgets, both are equivalent &ast;/
 * :insensitive,
 * *:insensitive {
 *     background-color: &num;320a91;
 * }
 *
 * /&ast; Theme selection colors in entries &ast;/
 * GtkEntry:selected {
 *     background-color: &num;56f9a0;
 * }
 *
 * /&ast; Theme focused labels &ast;/
 * GtkLabel:focused {
 *     background-color: &num;b4940f;
 * }
 *
 * /&ast; Theme inconsistent checkbuttons &ast;/
 * GtkCheckButton:inconsistent {
 *     background-color: &num;20395a;
 * }
 * </programlisting>
 * </example>
 * <para>
 * Widget state pseudoclasses may only apply to the last element
 * in a selector.
 * </para>
 * <para>
 * To determine the effective style for a widget, all the matching rule
 * sets are merged. As in CSS, rules apply by specificity, so the rules
 * whose selectors more closely match a widget path will take precedence
 * over the others.
 * </para>
 * </refsect2>
 * <refsect2 id="gtkcssprovider-rules">
 * <title>&commat; Rules</title>
 * <para>
 * GTK+'s CSS supports the &commat;import rule, in order to load another
 * CSS style sheet in addition to the currently parsed one.
 * </para>
 * <example>
 * <title>Using the &commat;import rule</title>
 * <programlisting language="text">
 * &commat;import url ("path/to/common.css");
 * </programlisting>
 * </example>
 * <para id="css-binding-set">
 * In order to extend key bindings affecting different widgets, GTK+
 * supports the &commat;binding-set rule to parse a set of bind/unbind
 * directives, see #GtkBindingSet for the supported syntax. Note that
 * the binding sets defined in this way must be associated with rule sets
 * by setting the gtk-key-bindings style property.
 * </para>
 * <para>
 * Customized key bindings are typically defined in a separate
 * <filename>gtk-keys.css</filename> CSS file and GTK+ loads this file
 * according to the current key theme, which is defined by the
 * #GtkSettings:gtk-key-theme-name setting.
 * </para>
 * <example>
 * <title>Using the &commat;binding rule</title>
 * <programlisting language="text">
 * &commat;binding-set binding-set1 {
 *   bind "&lt;alt&gt;Left" { "move-cursor" (visual-positions, -3, 0) };
 *   unbind "End";
 * };
 *
 * &commat;binding-set binding-set2 {
 *   bind "&lt;alt&gt;Right" { "move-cursor" (visual-positions, 3, 0) };
 *   bind "&lt;alt&gt;KP_space" { "delete-from-cursor" (whitespace, 1)
 *                          "insert-at-cursor" (" ") };
 * };
 *
 * GtkEntry {
 *   gtk-key-bindings: binding-set1, binding-set2;
 * }
 * </programlisting>
 * </example>
 * <para>
 * GTK+ also supports an additional &commat;define-color rule, in order
 * to define a color name which may be used instead of color numeric
 * representations. Also see the #GtkSettings:gtk-color-scheme setting
 * for a way to override the values of these named colors.
 * </para>
 * <example>
 * <title>Defining colors</title>
 * <programlisting language="text">
 * &commat;define-color bg_color &num;f9a039;
 *
 * &ast; {
 *     background-color: &commat;bg_color;
 * }
 * </programlisting>
 * </example>
 * </refsect2>
 * <refsect2 id="gtkcssprovider-symbolic-colors">
 * <title>Symbolic colors</title>
 * <para>
 * Besides being able to define color names, the CSS parser is also able
 * to read different color expressions, which can also be nested, providing
 * a rich language to define colors which are derived from a set of base
 * colors.
 * </para>
 * <example>
 * <title>Using symbolic colors</title>
 * <programlisting language="text">
 * &commat;define-color entry-color shade (&commat;bg_color, 0.7);
 *
 * GtkEntry {
 *     background-color: @entry-color;
 * }
 *
 * GtkEntry:focused {
 *     background-color: mix (&commat;entry-color,
 *                            shade (&num;fff, 0.5),
 *                            0.8);
 * }
 * </programlisting>
 * </example>
 * <para>
 *   The various ways to express colors in GTK+ CSS are:
 * </para>
 * <informaltable>
 *   <tgroup cols="3">
 *     <thead>
 *       <row>
 *         <entry>Syntax</entry>
 *         <entry>Explanation</entry>
 *         <entry>Examples</entry>
 *       </row>
 *     </thead>
 *     <tbody>
 *       <row>
 *         <entry>rgb(@r, @g, @b)</entry>
 *         <entry>An opaque color; @r, @g, @b can be either integers between
 *                0 and 255 or percentages</entry>
 *         <entry><literallayout>rgb(128, 10, 54)
 * rgb(20%, 30%, 0%)</literallayout></entry>
 *       </row>
 *       <row>
 *         <entry>rgba(@r, @g, @b, @a)</entry>
 *         <entry>A translucent color; @r, @g, @b are as in the previous row,
 *                @a is a floating point number between 0 and 1</entry>
 *         <entry><literallayout>rgba(255, 255, 0, 0.5)</literallayout></entry>
 *       </row>
 *       <row>
 *         <entry>&num;@xxyyzz</entry>
 *         <entry>An opaque color; @xx, @yy, @zz are hexadecimal numbers
 *                specifying @r, @g, @b variants with between 1 and 4
 *                hexadecimal digits per component are allowed</entry>
 *         <entry><literallayout>&num;ff12ab
 * &num;f0c</literallayout></entry>
 *       </row>
 *       <row>
 *         <entry>&commat;name</entry>
 *         <entry>Reference to a color that has been defined with
 *                &commat;define-color
 *         </entry>
 *         <entry>&commat;bg_color</entry>
 *       </row>
 *       <row>
 *         <entry>mix(@color1, @color2, @f)</entry>
 *         <entry>A linear combination of @color1 and @color2. @f is a
 *                floating point number between 0 and 1.</entry>
 *         <entry><literallayout>mix(&num;ff1e0a, &commat;bg_color, 0.8)</literallayout></entry>
 *       </row>
 *       <row>
 *         <entry>shade(@color, @f)</entry>
 *         <entry>A lighter or darker variant of @color. @f is a
 *                floating point number.
 *         </entry>
 *         <entry>shade(&commat;fg_color, 0.5)</entry>
 *       </row>
 *       <row>
 *         <entry>lighter(@color)</entry>
 *         <entry>A lighter variant of @color</entry>
 *       </row>
 *       <row>
 *         <entry>darker(@color)</entry>
 *         <entry>A darker variant of @color</entry>
 *       </row>
 *     </tbody>
 *   </tgroup>
 * </informaltable>
 * </refsect2>
 * <refsect2 id="gtkcssprovider-gradients">
 * <title>Gradients</title>
 * <para>
 * Linear or radial Gradients can be used as background images.
 * </para>
 * <para>
 * A linear gradient along the line from (@start_x, @start_y) to
 * (@end_x, @end_y) is specified using the syntax
 * <literallayout>-gtk-gradient (linear,
 *               @start_x @start_y, @end_x @end_y,
 *               color-stop (@position, @color),
 *               ...)</literallayout>
 * where @start_x and @end_x can be either a floating point number between
 * 0 and 1 or one of the special values 'left', 'right' or 'center', @start_y
 * and @end_y can be either a floating point number between 0 and 1 or one
 * of the special values 'top', 'bottom' or 'center', @position is a floating
 * point number between 0 and 1 and @color is a color expression (see above).
 * The color-stop can be repeated multiple times to add more than one color
 * stop. 'from (@color)' and 'to (@color)' can be used as abbreviations for
 * color stops with position 0 and 1, respectively.
 * </para>
 * <example>
 * <title>A linear gradient</title>
 * <inlinegraphic fileref="gradient1.png" format="PNG"/>
 * <para>This gradient was specified with
 * <literallayout>-gtk-gradient (linear,
 *                left top, right bottom,
 *                from(&commat;yellow), to(&commat;blue))</literallayout></para>
 * </example>
 * <example>
 * <title>Another linear gradient</title>
 * <inlinegraphic fileref="gradient2.png" format="PNG"/>
 * <para>This gradient was specified with
 * <literallayout>-gtk-gradient (linear,
 *                0 0, 0 1,
 *                color-stop(0, &commat;yellow),
 *                color-stop(0.2, &commat;blue),
 *                color-stop(1, &num;0f0))</literallayout></para>
 * </example>
 * <para>
 * A radial gradient along the two circles defined by (@start_x, @start_y,
 * @start_radius) and (@end_x, @end_y, @end_radius) is specified using the
 * syntax
 * <literallayout>-gtk-gradient (radial,
 *                @start_x @start_y, @start_radius,
 *                @end_x @end_y, @end_radius,
 *                color-stop (@position, @color),
 *                ...)</literallayout>
 * where @start_radius and @end_radius are floating point numbers and
 * the other parameters are as before.
 * </para>
 * <example>
 * <title>A radial gradient</title>
 * <inlinegraphic fileref="gradient3.png" format="PNG"/>
 * <para>This gradient was specified with
 * <literallayout>-gtk-gradient (radial,
 *                center center, 0,
 *                center center, 1,
 *                from(&commat;yellow), to(&commat;green))</literallayout></para>
 * </example>
 * <example>
 * <title>Another radial gradient</title>
 * <inlinegraphic fileref="gradient4.png" format="PNG"/>
 * <para>This gradient was specified with
 * <literallayout>-gtk-gradient (radial,
 *                0.4 0.4, 0.1,
 *                0.6 0.6, 0.7,
 *                color-stop (0, &num;f00),
 *                color-stop (0.1, &num;a0f),
 *                color-stop (0.2, &commat;yellow),
 *                color-stop (1, &commat;green))</literallayout></para>
 * </example>
 * </refsect2>
 * <refsect2 id="gtkcssprovider-slices">
 * <title>Border images</title>
 * <para>
 * Images can be used in 'slices' for the purpose of creating scalable
 * borders.
 * </para>
 * <inlinegraphic fileref="slices.png" format="PNG"/>
 * <para>
 * The syntax for specifying border images of this kind is:
 * <literallayout>url(@path) @top @right @bottom @left [repeat|stretch]? [repeat|stretch]?</literallayout>
 * The sizes of the 'cut off' portions are specified
 * with the @top, @right, @bottom and @left parameters.
 * The 'middle' sections can be repeated or stretched to create
 * the desired effect, by adding the 'repeat' or 'stretch' options after
 * the dimensions. If two options are specified, the first one affects
 * the horizontal behaviour and the second one the vertical behaviour.
 * If only one option is specified, it affects both.
 * </para>
 * <example>
 * <title>A border image</title>
 * <inlinegraphic fileref="border1.png" format="PNG"/>
 * <para>This border image was specified with
 * <literallayout>url("gradient1.png") 10 10 10 10</literallayout>
 * </para>
 * </example>
 * <example>
 * <title>A repeating border image</title>
 * <inlinegraphic fileref="border2.png" format="PNG"/>
 * <para>This border image was specified with
 * <literallayout>url("gradient1.png") 10 10 10 10 repeat</literallayout>
 * </para>
 * </example>
 * <example>
 * <title>A stretched border image</title>
 * <inlinegraphic fileref="border3.png" format="PNG"/>
 * <para>This border image was specified with
 * <literallayout>url("gradient1.png") 10 10 10 10 stretch</literallayout>
 * </para>
 * </example>
 * </refsect2>
 * <refsect2 id="gtkcssprovider-transitions">
 * <para>Styles can specify transitions that will be used to create a gradual
 * change in the appearance when a widget state changes. The following
 * syntax is used to specify transitions:
 * <literallayout>@duration [s|ms] [linear|ease|ease-in|ease-out|ease-in-out] [loop]?</literallayout>
 * The @duration is the amount of time that the animation will take for
 * a complete cycle from start to end. If the loop option is given, the
 * animation will be repated until the state changes again.
 * The option after the duration determines the transition function from a
 * small set of predefined functions.
 * <figure><title>Linear transition</title>
 * <graphic fileref="linear.png" format="PNG"/>
 * </figure>
 * <figure><title>Ease transition</title>
 * <graphic fileref="ease.png" format="PNG"/>
 * </figure>
 * <figure><title>Ease-in-out transition</title>
 * <graphic fileref="ease-in-out.png" format="PNG"/>
 * </figure>
 * <figure><title>Ease-in transition</title>
 * <graphic fileref="ease-in.png" format="PNG"/>
 * </figure>
 * <figure><title>Ease-out transition</title>
 * <graphic fileref="ease-out.png" format="PNG"/>
 * </figure>
 * </para>
 * </refsect2>
 * <refsect2 id="gtkcssprovider-properties">
 * <title>Supported properties</title>
 * <para>
 * Properties are the part that differ the most to common CSS,
 * not all properties are supported (some are planned to be
 * supported eventually, some others are meaningless or don't
 * map intuitively in a widget based environment).
 * </para>
 * <para>
 * There is also a difference in shorthand properties, for
 * example in common CSS it is fine to define a font through
 * the different @font-family, @font-style, @font-size
 * properties, meanwhile in GTK+'s CSS only the canonical
 * @font property is supported.
 * </para>
 * <para>
 * The currently supported properties are:
 * </para>
 * <informaltable>
 *   <tgroup cols="4">
 *     <thead>
 *       <row>
 *         <entry>Property name</entry>
 *         <entry>Syntax</entry>
 *         <entry>Maps to</entry>
 *         <entry>Examples</entry>
 *       </row>
 *     </thead>
 *     <tbody>
 *       <row>
 *         <entry>engine</entry>
 *         <entry>engine-name</entry>
 *         <entry>#GtkThemingEngine</entry>
 *         <entry>engine: clearlooks;
 *  engine: none; /&ast; use the default (i.e. builtin) engine) &ast;/ </entry>
 *       </row>
 *       <row>
 *         <entry>background-color</entry>
 *         <entry morerows="2">color (see above)</entry>
 *         <entry morerows="2">#GdkRGBA</entry>
 *         <entry morerows="2"><literallayout>background-color: &num;fff;
 * color: &amp;color1;
 * background-color: shade (&amp;color1, 0.5);
 * color: mix (&amp;color1, &num;f0f, 0.8);</literallayout>
 *         </entry>
 *       </row>
 *       <row>
 *         <entry>color</entry>
 *       </row>
 *       <row>
 *         <entry>border-color</entry>
 *       </row>
 *       <row>
 *         <entry>font</entry>
 *         <entry>@family [@style] [@size]</entry>
 *         <entry>#PangoFontDescription</entry>
 *         <entry>font: Sans 15;</entry>
 *       </row>
 *       <row>
 *         <entry>margin</entry>
 *         <entry morerows="1"><literallayout>@width
 * @vertical_width @horizontal_width
 * @top_width @horizontal_width @bottom_width
 * @top_width @right_width @bottom_width @left_width</literallayout>
 *         </entry>
 *         <entry morerows="1">#GtkBorder</entry>
 *         <entry morerows="1"><literallayout>margin: 5;
 * margin: 5 10;
 * margin: 5 10 3;
 * margin: 5 10 3 5;</literallayout>
 *         </entry>
 *       </row>
 *       <row>
 *         <entry>padding</entry>
 *       </row>
 *       <row>
 *         <entry>background-image</entry>
 *         <entry><literallayout>gradient (see above) or
 * url(@path)</literallayout></entry>
 *         <entry>#cairo_pattern_t</entry>
 *         <entry><literallayout>-gtk-gradient (linear,
 *                left top, right top,
 *                from (&num;fff), to (&num;000));
 * -gtk-gradient (linear, 0.0 0.5, 0.5 1.0,
 *                from (&num;fff),
 *                color-stop (0.5, &num;f00),
 *                to (&num;000));
 * -gtk-gradient (radial,
 *                center center, 0.2,
 *                center center, 0.8,
 *                color-stop (0.0, &num;fff),
 *                color-stop (1.0, &num;000));
 * url ('background.png');</literallayout>
 *         </entry>
 *       </row>
 *       <row>
 *         <entry>border-width</entry>
 *         <entry>integer</entry>
 *         <entry>#gint</entry>
 *         <entry>border-width: 5;</entry>
 *       </row>
 *       <row>
 *         <entry>border-radius</entry>
 *         <entry>integer</entry>
 *         <entry>#gint</entry>
 *         <entry>border-radius: 5;</entry>
 *       </row>
 *       <row>
 *         <entry>border-style</entry>
 *         <entry>[none|solid|inset|outset]</entry>
 *         <entry>#GtkBorderStyle</entry>
 *         <entry>border-style: solid;</entry>
 *       </row>
 *       <row>
 *         <entry>border-image</entry>
 *         <entry><literallayout>border image (see above)</literallayout></entry>
 *         <entry>internal use only</entry>
 *         <entry><literallayout>border-image: url("/path/to/image.png") 3 4 3 4 stretch;
 * border-image: url("/path/to/image.png") 3 4 4 3 repeat stretch;</literallayout>
 *         </entry>
 *       </row>
 *       <row>
 *         <entry>transition</entry>
 *         <entry>transition (see above)</entry>
 *         <entry>internal use only</entry>
 *         <entry><literallayout>transition: 150ms ease-in-out;
 * transition: 1s linear loop;</literallayout>
 *         </entry>
 *       </row>
 *       <row>
 *         <entry>gtk-key-bindings</entry>
 *         <entry>binding set name list</entry>
 *         <entry>internal use only</entry>
 *         <entry><literallayout>gtk-bindings: binding1, binding2, ...;</literallayout>
 *         </entry>
 *       </row>
 *     </tbody>
 *   </tgroup>
 * </informaltable>
 * <para>
 * GtkThemingEngines can register their own, engine-specific style properties
 * with the function gtk_theming_engine_register_property(). These properties
 * can be set in CSS like other properties, using a name of the form
 * <literallayout>-<replaceable>namespace</replaceable>-<replaceable>name</replaceable></literallayout>, where <replaceable>namespace</replaceable> is typically
 * the name of the theming engine, and <replaceable>name</replaceable> is the
 * name of the property. Style properties that have been registered by widgets
 * using gtk_widget_class_install_style_property() can also be set in this
 * way, using the widget class name for <replaceable>namespace</replaceable>.
 * </para>
 * <example>
 * <title>Using engine-specific style properties</title>
 * <programlisting>
 * * {
 *     engine: clearlooks;
 *     border-radius: 4;
 *     -GtkPaned-handle-size: 6;
 *     -clearlooks-colorize-scrollbar: false;
 * }
 * </programlisting>
 * </example>
 * </refsect2>
 */

typedef struct GtkCssProviderPrivate GtkCssProviderPrivate;
typedef struct SelectorElement SelectorElement;
typedef struct SelectorPath SelectorPath;
typedef struct SelectorStyleInfo SelectorStyleInfo;
typedef enum SelectorElementType SelectorElementType;
typedef enum CombinatorType CombinatorType;
typedef enum ParserScope ParserScope;
typedef enum ParserSymbol ParserSymbol;

enum SelectorElementType {
  SELECTOR_TYPE_NAME,
  SELECTOR_NAME,
  SELECTOR_GTYPE,
  SELECTOR_REGION,
  SELECTOR_CLASS,
  SELECTOR_GLOB
};

enum CombinatorType {
  COMBINATOR_DESCENDANT, /* No direct relation needed */
  COMBINATOR_CHILD       /* Direct child */
};

struct SelectorElement
{
  SelectorElementType elem_type;
  CombinatorType combinator;

  union
  {
    GQuark name;
    GType type;

    struct
    {
      GQuark name;
      GtkRegionFlags flags;
    } region;
  };
};

struct SelectorPath
{
  GSList *elements;
  GtkStateFlags state;
  guint ref_count;
};

struct SelectorStyleInfo
{
  SelectorPath *path;
  GHashTable *style;
};

struct GtkCssProviderPrivate
{
  GScanner *scanner;
  gchar *filename;

  const gchar *buffer;
  const gchar *value_pos;

  GHashTable *symbolic_colors;

  GPtrArray *selectors_info;

  /* Current parser state */
  GSList *state;
  GSList *cur_selectors;
  GHashTable *cur_properties;
};

enum ParserScope {
  SCOPE_SELECTOR,
  SCOPE_PSEUDO_CLASS,
  SCOPE_NTH_CHILD,
  SCOPE_DECLARATION,
  SCOPE_VALUE,
  SCOPE_BINDING_SET
};

/* Extend GtkStateType, since these
 * values are also used as symbols
 */
enum ParserSymbol {
  /* Scope: pseudo-class */
  SYMBOL_NTH_CHILD = GTK_STATE_FOCUSED + 1,
  SYMBOL_FIRST_CHILD,
  SYMBOL_LAST_CHILD,
  SYMBOL_SORTED_CHILD,

  /* Scope: nth-child */
  SYMBOL_NTH_CHILD_EVEN,
  SYMBOL_NTH_CHILD_ODD,
  SYMBOL_NTH_CHILD_FIRST,
  SYMBOL_NTH_CHILD_LAST
};

static void gtk_css_provider_finalize (GObject *object);
static void gtk_css_style_provider_iface_init (GtkStyleProviderIface *iface);

static void scanner_apply_scope (GScanner    *scanner,
                                 ParserScope  scope);
static gboolean css_provider_parse_value (GtkCssProvider  *css_provider,
                                          const gchar     *value_str,
                                          GValue          *value,
                                          GError         **error);
static gboolean gtk_css_provider_load_from_path_internal (GtkCssProvider  *css_provider,
                                                          const gchar     *path,
                                                          gboolean         reset,
                                                          GError         **error);

GQuark
gtk_css_provider_error_quark (void)
{
  return g_quark_from_static_string ("gtk-css-provider-error-quark");
}

G_DEFINE_TYPE_EXTENDED (GtkCssProvider, gtk_css_provider, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_STYLE_PROVIDER,
                                               gtk_css_style_provider_iface_init));

static void
gtk_css_provider_class_init (GtkCssProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_css_provider_finalize;

  g_type_class_add_private (object_class, sizeof (GtkCssProviderPrivate));
}

static SelectorPath *
selector_path_new (void)
{
  SelectorPath *path;

  path = g_slice_new0 (SelectorPath);
  path->ref_count = 1;

  return path;
}

static SelectorPath *
selector_path_ref (SelectorPath *path)
{
  path->ref_count++;
  return path;
}

static void
selector_path_unref (SelectorPath *path)
{
  path->ref_count--;

  if (path->ref_count > 0)
    return;

  while (path->elements)
    {
      g_slice_free (SelectorElement, path->elements->data);
      path->elements = g_slist_delete_link (path->elements, path->elements);
    }

  g_slice_free (SelectorPath, path);
}

static void
selector_path_prepend_type (SelectorPath *path,
                            const gchar  *type_name)
{
  SelectorElement *elem;
  GType type;

  elem = g_slice_new (SelectorElement);
  elem->combinator = COMBINATOR_DESCENDANT;
  type = g_type_from_name (type_name);

  if (type == G_TYPE_INVALID)
    {
      elem->elem_type = SELECTOR_TYPE_NAME;
      elem->name = g_quark_from_string (type_name);
    }
  else
    {
      elem->elem_type = SELECTOR_GTYPE;
      elem->type = type;
    }

  path->elements = g_slist_prepend (path->elements, elem);
}

static void
selector_path_prepend_glob (SelectorPath *path)
{
  SelectorElement *elem;

  elem = g_slice_new (SelectorElement);
  elem->elem_type = SELECTOR_GLOB;
  elem->combinator = COMBINATOR_DESCENDANT;

  path->elements = g_slist_prepend (path->elements, elem);
}

static void
selector_path_prepend_region (SelectorPath   *path,
                              const gchar    *name,
                              GtkRegionFlags  flags)
{
  SelectorElement *elem;

  elem = g_slice_new (SelectorElement);
  elem->combinator = COMBINATOR_DESCENDANT;
  elem->elem_type = SELECTOR_REGION;

  elem->region.name = g_quark_from_string (name);
  elem->region.flags = flags;

  path->elements = g_slist_prepend (path->elements, elem);
}

static void
selector_path_prepend_name (SelectorPath *path,
                            const gchar  *name)
{
  SelectorElement *elem;

  elem = g_slice_new (SelectorElement);
  elem->combinator = COMBINATOR_DESCENDANT;
  elem->elem_type = SELECTOR_NAME;

  elem->name = g_quark_from_string (name);

  path->elements = g_slist_prepend (path->elements, elem);
}

static void
selector_path_prepend_class (SelectorPath *path,
                             const gchar  *name)
{
  SelectorElement *elem;

  elem = g_slice_new (SelectorElement);
  elem->combinator = COMBINATOR_DESCENDANT;
  elem->elem_type = SELECTOR_CLASS;

  elem->name = g_quark_from_string (name);

  path->elements = g_slist_prepend (path->elements, elem);
}

static void
selector_path_prepend_combinator (SelectorPath   *path,
                                  CombinatorType  combinator)
{
  SelectorElement *elem;

  g_assert (path->elements != NULL);

  /* It is actually stored in the last element */
  elem = path->elements->data;
  elem->combinator = combinator;
}

static gint
selector_path_depth (SelectorPath *path)
{
  return g_slist_length (path->elements);
}

static SelectorStyleInfo *
selector_style_info_new (SelectorPath *path)
{
  SelectorStyleInfo *info;

  info = g_slice_new0 (SelectorStyleInfo);
  info->path = selector_path_ref (path);

  return info;
}

static void
selector_style_info_free (SelectorStyleInfo *info)
{
  if (info->style)
    g_hash_table_unref (info->style);

  if (info->path)
    selector_path_unref (info->path);

  g_slice_free (SelectorStyleInfo, info);
}

static void
selector_style_info_set_style (SelectorStyleInfo *info,
                               GHashTable        *style)
{
  if (info->style)
    g_hash_table_unref (info->style);

  if (style)
    info->style = g_hash_table_ref (style);
  else
    info->style = NULL;
}

static GScanner *
create_scanner (void)
{
  GScanner *scanner;

  scanner = g_scanner_new (NULL);

  g_scanner_scope_add_symbol (scanner, SCOPE_PSEUDO_CLASS, "active", GUINT_TO_POINTER (GTK_STATE_ACTIVE));
  g_scanner_scope_add_symbol (scanner, SCOPE_PSEUDO_CLASS, "prelight", GUINT_TO_POINTER (GTK_STATE_PRELIGHT));
  g_scanner_scope_add_symbol (scanner, SCOPE_PSEUDO_CLASS, "hover", GUINT_TO_POINTER (GTK_STATE_PRELIGHT));
  g_scanner_scope_add_symbol (scanner, SCOPE_PSEUDO_CLASS, "selected", GUINT_TO_POINTER (GTK_STATE_SELECTED));
  g_scanner_scope_add_symbol (scanner, SCOPE_PSEUDO_CLASS, "insensitive", GUINT_TO_POINTER (GTK_STATE_INSENSITIVE));
  g_scanner_scope_add_symbol (scanner, SCOPE_PSEUDO_CLASS, "inconsistent", GUINT_TO_POINTER (GTK_STATE_INCONSISTENT));
  g_scanner_scope_add_symbol (scanner, SCOPE_PSEUDO_CLASS, "focused", GUINT_TO_POINTER (GTK_STATE_FOCUSED));
  g_scanner_scope_add_symbol (scanner, SCOPE_PSEUDO_CLASS, "focus", GUINT_TO_POINTER (GTK_STATE_FOCUSED));

  g_scanner_scope_add_symbol (scanner, SCOPE_PSEUDO_CLASS, "nth-child", GUINT_TO_POINTER (SYMBOL_NTH_CHILD));
  g_scanner_scope_add_symbol (scanner, SCOPE_PSEUDO_CLASS, "first-child", GUINT_TO_POINTER (SYMBOL_FIRST_CHILD));
  g_scanner_scope_add_symbol (scanner, SCOPE_PSEUDO_CLASS, "last-child", GUINT_TO_POINTER (SYMBOL_LAST_CHILD));
  g_scanner_scope_add_symbol (scanner, SCOPE_PSEUDO_CLASS, "sorted", GUINT_TO_POINTER (SYMBOL_SORTED_CHILD));

  g_scanner_scope_add_symbol (scanner, SCOPE_NTH_CHILD, "even", GUINT_TO_POINTER (SYMBOL_NTH_CHILD_EVEN));
  g_scanner_scope_add_symbol (scanner, SCOPE_NTH_CHILD, "odd", GUINT_TO_POINTER (SYMBOL_NTH_CHILD_ODD));
  g_scanner_scope_add_symbol (scanner, SCOPE_NTH_CHILD, "first", GUINT_TO_POINTER (SYMBOL_NTH_CHILD_FIRST));
  g_scanner_scope_add_symbol (scanner, SCOPE_NTH_CHILD, "last", GUINT_TO_POINTER (SYMBOL_NTH_CHILD_LAST));

  scanner_apply_scope (scanner, SCOPE_SELECTOR);

  return scanner;
}

static void
gtk_css_provider_init (GtkCssProvider *css_provider)
{
  GtkCssProviderPrivate *priv;

  priv = css_provider->priv = G_TYPE_INSTANCE_GET_PRIVATE (css_provider,
                                                           GTK_TYPE_CSS_PROVIDER,
                                                           GtkCssProviderPrivate);

  priv->selectors_info = g_ptr_array_new_with_free_func ((GDestroyNotify) selector_style_info_free);
  priv->scanner = create_scanner ();

  priv->symbolic_colors = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                 (GDestroyNotify) g_free,
                                                 (GDestroyNotify) gtk_symbolic_color_unref);
}

typedef struct ComparePathData ComparePathData;

struct ComparePathData
{
  guint64 score;
  SelectorPath *path;
  GSList *iter;
};

static gboolean
compare_selector_element (GtkWidgetPath   *path,
                          guint            index,
                          SelectorElement *elem,
                          guint8          *score)
{
  *score = 0;

  if (elem->elem_type == SELECTOR_TYPE_NAME)
    {
      const gchar *type_name;
      GType resolved_type;

      /* Resolve the type name */
      type_name = g_quark_to_string (elem->name);
      resolved_type = g_type_from_name (type_name);

      if (resolved_type == G_TYPE_INVALID)
        {
          /* Type couldn't be resolved, so the selector
           * clearly doesn't affect the given widget path
           */
          return FALSE;
        }

      elem->elem_type = SELECTOR_GTYPE;
      elem->type = resolved_type;
    }

  if (elem->elem_type == SELECTOR_GTYPE)
    {
      GType type;

      type = gtk_widget_path_iter_get_object_type (path, index);

      if (!g_type_is_a (type, elem->type))
        return FALSE;

      if (type == elem->type)
        *score |= 0xF;
      else
        {
          GType parent = type;

          *score = 0xE;

          while ((parent = g_type_parent (parent)) != G_TYPE_INVALID)
            {
              if (parent == elem->type)
                break;

              *score -= 1;

              if (*score == 1)
                {
                  g_warning ("Hierarchy is higher than expected.");
                  break;
                }
            }
        }

      return TRUE;
    }
  else if (elem->elem_type == SELECTOR_REGION)
    {
      GtkRegionFlags flags;

      if (!gtk_widget_path_iter_has_qregion (path, index,
                                             elem->region.name,
                                             &flags))
        return FALSE;

      if (elem->region.flags != 0 &&
          (flags & elem->region.flags) == 0)
        return FALSE;

      *score = 0xF;
      return TRUE;
    }
  else if (elem->elem_type == SELECTOR_GLOB)
    {
      /* Treat as lowest matching type */
      *score = 1;
      return TRUE;
    }
  else if (elem->elem_type == SELECTOR_NAME)
    {
      if (!gtk_widget_path_iter_has_qname (path, index, elem->name))
        return FALSE;

      *score = 0xF;
      return TRUE;
    }
  else if (elem->elem_type == SELECTOR_CLASS)
    {
      if (!gtk_widget_path_iter_has_qclass (path, index, elem->name))
        return FALSE;

      *score = 0xF;
      return TRUE;
    }

  return FALSE;
}

static guint64
compare_selector (GtkWidgetPath *path,
                  SelectorPath  *selector)
{
  GSList *elements = selector->elements;
  gboolean match = TRUE, first = TRUE, first_match = FALSE;
  guint64 score = 0;
  gint i;

  i = gtk_widget_path_length (path) - 1;

  while (elements && match && i >= 0)
    {
      SelectorElement *elem;
      guint8 elem_score;

      elem = elements->data;

      match = compare_selector_element (path, i, elem, &elem_score);

      if (match && first)
        first_match = TRUE;

      /* Only move on to the next index if there is no match
       * with the current element (whether to continue or not
       * handled right after in the combinator check), or a
       * GType or glob has just been matched.
       *
       * Region and widget names do not trigger this because
       * the next element in the selector path could also be
       * related to the same index.
       */
      if (!match ||
          (elem->elem_type == SELECTOR_GTYPE ||
           elem->elem_type == SELECTOR_GLOB))
        i--;

      if (!match &&
          elem->elem_type != SELECTOR_TYPE_NAME &&
          elem->combinator == COMBINATOR_DESCENDANT)
        {
          /* With descendant combinators there may
           * be intermediate chidren in the hierarchy
           */
          match = TRUE;
        }
      else if (match)
        elements = elements->next;

      if (match)
        {
          /* Only 4 bits are actually used */
          score <<= 4;
          score |= elem_score;
        }

      first = FALSE;
    }

  /* If there are pending selector
   * elements to compare, it's not
   * a match.
   */
  if (elements)
    match = FALSE;

  if (!match)
    score = 0;
  else if (first_match)
    {
      /* Assign more weight to these selectors
       * that matched right from the first element.
       */
      score <<= 4;
    }

  return score;
}

typedef struct StylePriorityInfo StylePriorityInfo;

struct StylePriorityInfo
{
  guint64 score;
  GHashTable *style;
  GtkStateFlags state;
};

static GArray *
css_provider_get_selectors (GtkCssProvider *css_provider,
                            GtkWidgetPath  *path)
{
  GtkCssProviderPrivate *priv;
  GArray *priority_info;
  guint i, j;

  priv = css_provider->priv;
  priority_info = g_array_new (FALSE, FALSE, sizeof (StylePriorityInfo));

  for (i = 0; i < priv->selectors_info->len; i++)
    {
      SelectorStyleInfo *info;
      StylePriorityInfo new;
      gboolean added = FALSE;
      guint64 score;

      info = g_ptr_array_index (priv->selectors_info, i);
      score = compare_selector (path, info->path);

      if (score <= 0)
        continue;

      new.score = score;
      new.style = info->style;
      new.state = info->path->state;

      for (j = 0; j < priority_info->len; j++)
        {
          StylePriorityInfo *cur;

          cur = &g_array_index (priority_info, StylePriorityInfo, j);

          if (cur->score > new.score)
            {
              g_array_insert_val (priority_info, j, new);
              added = TRUE;
              break;
            }
        }

      if (!added)
        g_array_append_val (priority_info, new);
    }

  return priority_info;
}

static void
css_provider_dump_symbolic_colors (GtkCssProvider     *css_provider,
                                   GtkStyleProperties *props)
{
  GtkCssProviderPrivate *priv;
  GHashTableIter iter;
  gpointer key, value;

  priv = css_provider->priv;
  g_hash_table_iter_init (&iter, priv->symbolic_colors);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      const gchar *name;
      GtkSymbolicColor *color;

      name = key;
      color = value;

      gtk_style_properties_map_color (props, name, color);
    }
}

static GtkStyleProperties *
gtk_css_provider_get_style (GtkStyleProvider *provider,
                            GtkWidgetPath    *path)
{
  GtkCssProvider *css_provider;
  GtkStyleProperties *props;
  GArray *priority_info;
  guint i;

  css_provider = GTK_CSS_PROVIDER (provider);
  props = gtk_style_properties_new ();

  css_provider_dump_symbolic_colors (css_provider, props);
  priority_info = css_provider_get_selectors (css_provider, path);

  for (i = 0; i < priority_info->len; i++)
    {
      StylePriorityInfo *info;
      GHashTableIter iter;
      gpointer key, value;

      info = &g_array_index (priority_info, StylePriorityInfo, i);
      g_hash_table_iter_init (&iter, info->style);

      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          gchar *prop = key;

          /* Properties starting with '-' may be both widget style properties
           * or custom properties from the theming engine, so check whether
           * the type is registered or not.
           */
          if (prop[0] == '-' &&
              !gtk_style_properties_lookup_property (prop, NULL, NULL))
            continue;

          gtk_style_properties_set_property (props, key, info->state, value);
        }
    }

  g_array_free (priority_info, TRUE);

  return props;
}

static gboolean
gtk_css_provider_get_style_property (GtkStyleProvider *provider,
                                     GtkWidgetPath    *path,
                                     GtkStateFlags     state,
                                     GParamSpec       *pspec,
                                     GValue           *value)
{
  GArray *priority_info;
  gboolean found = FALSE;
  gchar *prop_name;
  gint i;

  prop_name = g_strdup_printf ("-%s-%s",
                               g_type_name (pspec->owner_type),
                               pspec->name);

  priority_info = css_provider_get_selectors (GTK_CSS_PROVIDER (provider), path);

  for (i = priority_info->len - 1; i >= 0; i--)
    {
      StylePriorityInfo *info;
      GValue *val;

      info = &g_array_index (priority_info, StylePriorityInfo, i);
      val = g_hash_table_lookup (info->style, prop_name);

      if (val &&
          (info->state == 0 ||
           info->state == state ||
           ((info->state & state) != 0 &&
            (info->state & ~(state)) == 0)))
        {
          const gchar *val_str;

          val_str = g_value_get_string (val);
          found = TRUE;

          css_provider_parse_value (GTK_CSS_PROVIDER (provider), val_str, value, NULL);
          break;
        }
    }

  g_array_free (priority_info, TRUE);
  g_free (prop_name);

  return found;
}

static void
gtk_css_style_provider_iface_init (GtkStyleProviderIface *iface)
{
  iface->get_style = gtk_css_provider_get_style;
  iface->get_style_property = gtk_css_provider_get_style_property;
}

static void
gtk_css_provider_finalize (GObject *object)
{
  GtkCssProvider *css_provider;
  GtkCssProviderPrivate *priv;

  css_provider = GTK_CSS_PROVIDER (object);
  priv = css_provider->priv;

  g_scanner_destroy (priv->scanner);
  g_free (priv->filename);

  g_ptr_array_free (priv->selectors_info, TRUE);

  g_slist_foreach (priv->cur_selectors, (GFunc) selector_path_unref, NULL);
  g_slist_free (priv->cur_selectors);

  if (priv->cur_properties)
    g_hash_table_unref (priv->cur_properties);
  if (priv->symbolic_colors)
    g_hash_table_destroy (priv->symbolic_colors);

  G_OBJECT_CLASS (gtk_css_provider_parent_class)->finalize (object);
}

/**
 * gtk_css_provider_new:
 *
 * Returns a newly created #GtkCssProvider.
 *
 * Returns: A new #GtkCssProvider
 **/
GtkCssProvider *
gtk_css_provider_new (void)
{
  return g_object_new (GTK_TYPE_CSS_PROVIDER, NULL);
}

static void
property_value_free (GValue *value)
{
  if (G_IS_VALUE (value))
    g_value_unset (value);

  g_slice_free (GValue, value);
}

static void
scanner_apply_scope (GScanner    *scanner,
                     ParserScope  scope)
{
  g_scanner_set_scope (scanner, scope);

  if (scope == SCOPE_VALUE)
    {
      scanner->config->cset_identifier_first = G_CSET_a_2_z G_CSET_A_2_Z G_CSET_DIGITS "@#-_";
      scanner->config->cset_identifier_nth = G_CSET_a_2_z G_CSET_A_2_Z G_CSET_DIGITS "@#-_ +(),.%\t\n'/\"";
      scanner->config->scan_identifier_1char = TRUE;
    }
  else if (scope == SCOPE_BINDING_SET)
    {
      scanner->config->cset_identifier_first = G_CSET_a_2_z G_CSET_A_2_Z G_CSET_DIGITS "@#-_";
      scanner->config->cset_identifier_nth = G_CSET_a_2_z G_CSET_A_2_Z G_CSET_DIGITS "@#-_ +(){}<>,.%\t\n'/\"";
      scanner->config->scan_identifier_1char = TRUE;
    }
  else if (scope == SCOPE_SELECTOR)
    {
      scanner->config->cset_identifier_first = G_CSET_a_2_z G_CSET_A_2_Z "*@";
      scanner->config->cset_identifier_nth = G_CSET_a_2_z G_CSET_A_2_Z G_CSET_DIGITS "-_#.";
      scanner->config->scan_identifier_1char = TRUE;
    }
  else if (scope == SCOPE_PSEUDO_CLASS ||
           scope == SCOPE_NTH_CHILD ||
           scope == SCOPE_DECLARATION)
    {
      scanner->config->cset_identifier_first = G_CSET_a_2_z G_CSET_A_2_Z "-_";
      scanner->config->cset_identifier_nth = G_CSET_a_2_z G_CSET_A_2_Z G_CSET_DIGITS "-_";
      scanner->config->scan_identifier_1char = FALSE;
    }
  else
    g_assert_not_reached ();

  scanner->config->scan_float = FALSE;
  scanner->config->cpair_comment_single = NULL;
}

static void
css_provider_push_scope (GtkCssProvider *css_provider,
                         ParserScope     scope)
{
  GtkCssProviderPrivate *priv;

  priv = css_provider->priv;
  priv->state = g_slist_prepend (priv->state, GUINT_TO_POINTER (scope));

  scanner_apply_scope (priv->scanner, scope);
}

static ParserScope
css_provider_pop_scope (GtkCssProvider *css_provider)
{
  GtkCssProviderPrivate *priv;
  ParserScope scope = SCOPE_SELECTOR;

  priv = css_provider->priv;

  if (!priv->state)
    {
      g_warning ("Push/pop calls to parser scope aren't paired");
      scanner_apply_scope (priv->scanner, SCOPE_SELECTOR);
      return SCOPE_SELECTOR;
    }

  priv->state = g_slist_delete_link (priv->state, priv->state);

  /* Fetch new scope */
  if (priv->state)
    scope = GPOINTER_TO_INT (priv->state->data);

  scanner_apply_scope (priv->scanner, scope);

  return scope;
}

static void
css_provider_reset_parser (GtkCssProvider *css_provider)
{
  GtkCssProviderPrivate *priv;

  priv = css_provider->priv;

  g_slist_free (priv->state);
  priv->state = NULL;

  scanner_apply_scope (priv->scanner, SCOPE_SELECTOR);
  priv->scanner->user_data = NULL;
  priv->value_pos = NULL;

  g_slist_foreach (priv->cur_selectors, (GFunc) selector_path_unref, NULL);
  g_slist_free (priv->cur_selectors);
  priv->cur_selectors = NULL;

  if (priv->cur_properties)
    g_hash_table_unref (priv->cur_properties);

  priv->cur_properties = g_hash_table_new_full (g_str_hash,
                                                g_str_equal,
                                                (GDestroyNotify) g_free,
                                                (GDestroyNotify) property_value_free);
}

static void
css_provider_commit (GtkCssProvider *css_provider)
{
  GtkCssProviderPrivate *priv;
  GSList *l;

  priv = css_provider->priv;
  l = priv->cur_selectors;

  while (l)
    {
      SelectorPath *path = l->data;
      SelectorStyleInfo *info;

      info = selector_style_info_new (path);
      selector_style_info_set_style (info, priv->cur_properties);

      g_ptr_array_add (priv->selectors_info, info);
      l = l->next;
    }
}

static GTokenType
parse_nth_child (GtkCssProvider *css_provider,
                 GScanner       *scanner,
                 GtkRegionFlags *flags)
{
  ParserSymbol symbol;

  g_scanner_get_next_token (scanner);

  if (scanner->token != G_TOKEN_SYMBOL)
    return G_TOKEN_SYMBOL;

  symbol = GPOINTER_TO_INT (scanner->value.v_symbol);

  if (symbol == SYMBOL_NTH_CHILD)
    {
      g_scanner_get_next_token (scanner);

      if (scanner->token != G_TOKEN_LEFT_PAREN)
        return G_TOKEN_LEFT_PAREN;

      css_provider_push_scope (css_provider, SCOPE_NTH_CHILD);
      g_scanner_get_next_token (scanner);

      if (scanner->token != G_TOKEN_SYMBOL)
        return G_TOKEN_SYMBOL;

      symbol = GPOINTER_TO_INT (scanner->value.v_symbol);

      switch (symbol)
        {
        case SYMBOL_NTH_CHILD_EVEN:
          *flags = GTK_REGION_EVEN;
          break;
        case SYMBOL_NTH_CHILD_ODD:
          *flags = GTK_REGION_ODD;
          break;
        case SYMBOL_NTH_CHILD_FIRST:
          *flags = GTK_REGION_FIRST;
          break;
        case SYMBOL_NTH_CHILD_LAST:
          *flags = GTK_REGION_LAST;
          break;
        default:
          break;
        }

      g_scanner_get_next_token (scanner);

      if (scanner->token != G_TOKEN_RIGHT_PAREN)
        return G_TOKEN_RIGHT_PAREN;

      css_provider_pop_scope (css_provider);
    }
  else if (symbol == SYMBOL_FIRST_CHILD)
    *flags = GTK_REGION_FIRST;
  else if (symbol == SYMBOL_LAST_CHILD)
    *flags = GTK_REGION_LAST;
  else if (symbol == SYMBOL_SORTED_CHILD)
    *flags = GTK_REGION_SORTED;
  else
    {
      *flags = 0;
      return G_TOKEN_SYMBOL;
    }

  return G_TOKEN_NONE;
}

static GTokenType
parse_pseudo_class (GtkCssProvider *css_provider,
                    GScanner       *scanner,
                    SelectorPath   *selector)
{
  GtkStateType state;

  g_scanner_get_next_token (scanner);

  if (scanner->token != G_TOKEN_SYMBOL)
    return G_TOKEN_SYMBOL;

  state = GPOINTER_TO_INT (scanner->value.v_symbol);

  switch (state)
    {
    case GTK_STATE_ACTIVE:
      selector->state |= GTK_STATE_FLAG_ACTIVE;
      break;
    case GTK_STATE_PRELIGHT:
      selector->state |= GTK_STATE_FLAG_PRELIGHT;
      break;
    case GTK_STATE_SELECTED:
      selector->state |= GTK_STATE_FLAG_SELECTED;
      break;
    case GTK_STATE_INSENSITIVE:
      selector->state |= GTK_STATE_FLAG_INSENSITIVE;
      break;
    case GTK_STATE_INCONSISTENT:
      selector->state |= GTK_STATE_FLAG_INCONSISTENT;
      break;
    case GTK_STATE_FOCUSED:
      selector->state |= GTK_STATE_FLAG_FOCUSED;
      break;
    default:
      return G_TOKEN_SYMBOL;
    }

  return G_TOKEN_NONE;
}

/* Parses a number of concatenated classes */
static void
parse_classes (SelectorPath   *path,
               const gchar    *str)
{
  gchar *pos;

  if ((pos = strchr (str, '.')) != NULL)
    {
      /* Leave the last class to the call after the loop */
      while (pos)
        {
          *pos = '\0';
          selector_path_prepend_class (path, str);

          str = pos + 1;
          pos = strchr (str, '.');
        }
    }

  selector_path_prepend_class (path, str);
}

static gboolean
is_widget_class_name (const gchar *str)
{
  /* Do a pretty lax check here, not all
   * widget class names contain only CamelCase
   * (gtkmm widgets don't), but at least part of
   * the name will be CamelCase, so check for
   * the first uppercase char
   */
  while (*str)
    {
      if (g_ascii_isupper (*str))
        return TRUE;

      str++;
    }

  return FALSE;
}

static GTokenType
parse_selector (GtkCssProvider  *css_provider,
                GScanner        *scanner,
                SelectorPath   **selector_out)
{
  SelectorPath *path;

  path = selector_path_new ();
  *selector_out = path;

  if (scanner->token != ':' &&
      scanner->token != '#' &&
      scanner->token != '.' &&
      scanner->token != G_TOKEN_IDENTIFIER)
    return G_TOKEN_IDENTIFIER;

  while (scanner->token == '#' ||
         scanner->token == '.' ||
         scanner->token == G_TOKEN_IDENTIFIER)
    {
      if (scanner->token == '#' ||
          scanner->token == '.')
        {
          gboolean is_class;
          gchar *pos;

          is_class = (scanner->token == '.');

          g_scanner_get_next_token (scanner);

          if (scanner->token != G_TOKEN_IDENTIFIER)
            return G_TOKEN_IDENTIFIER;

          selector_path_prepend_glob (path);
          selector_path_prepend_combinator (path, COMBINATOR_CHILD);

          if (is_class)
            parse_classes (path, scanner->value.v_identifier);
          else
            {
              if ((pos = strchr (scanner->value.v_identifier, '.')) != NULL)
                *pos = '\0';

              selector_path_prepend_name (path, scanner->value.v_identifier);

              /* Parse any remaining classes */
              if (pos)
                parse_classes (path, pos + 1);
            }
        }
      else if (is_widget_class_name (scanner->value.v_identifier))
        {
          gchar *pos;

          if ((pos = strchr (scanner->value.v_identifier, '#')) != NULL ||
              (pos = strchr (scanner->value.v_identifier, '.')) != NULL)
            {
              gchar *type_name, *name;
              gboolean is_class;

              is_class = (*pos == '.');

              /* Widget type and name/class put together */
              name = pos + 1;
              *pos = '\0';
              type_name = scanner->value.v_identifier;

              selector_path_prepend_type (path, type_name);

              /* This is only so there is a direct relationship
               * between widget type and its name.
               */
              selector_path_prepend_combinator (path, COMBINATOR_CHILD);

              if (is_class)
                parse_classes (path, name);
              else
                {
                  if ((pos = strchr (name, '.')) != NULL)
                    *pos = '\0';

                  selector_path_prepend_name (path, name);

                  /* Parse any remaining classes */
                  if (pos)
                    parse_classes (path, pos + 1);
                }
            }
          else
            selector_path_prepend_type (path, scanner->value.v_identifier);
        }
      else if (_gtk_style_context_check_region_name (scanner->value.v_identifier))
        {
          GtkRegionFlags flags = 0;
          gchar *region_name;

          region_name = g_strdup (scanner->value.v_identifier);

          if (g_scanner_peek_next_token (scanner) == ':')
            {
              ParserSymbol symbol;

              g_scanner_get_next_token (scanner);
              css_provider_push_scope (css_provider, SCOPE_PSEUDO_CLASS);

              /* Check for the next token being nth-child, parse in that
               * case, and fallback into common state parsing if not.
               */
              if (g_scanner_peek_next_token (scanner) != G_TOKEN_SYMBOL)
                return G_TOKEN_SYMBOL;

              symbol = GPOINTER_TO_INT (scanner->next_value.v_symbol);

              if (symbol == SYMBOL_FIRST_CHILD ||
                  symbol == SYMBOL_LAST_CHILD ||
                  symbol == SYMBOL_NTH_CHILD ||
                  symbol == SYMBOL_SORTED_CHILD)
                {
                  GTokenType token;

                  if ((token = parse_nth_child (css_provider, scanner, &flags)) != G_TOKEN_NONE)
                    return token;

                  css_provider_pop_scope (css_provider);
                }
              else
                {
                  css_provider_pop_scope (css_provider);
                  selector_path_prepend_region (path, region_name, 0);
                  g_free (region_name);
                  break;
                }
            }

          selector_path_prepend_region (path, region_name, flags);
          g_free (region_name);
        }
      else if (scanner->value.v_identifier[0] == '*')
        selector_path_prepend_glob (path);
      else
        return G_TOKEN_IDENTIFIER;

      g_scanner_get_next_token (scanner);

      if (scanner->token == '>')
        {
          selector_path_prepend_combinator (path, COMBINATOR_CHILD);
          g_scanner_get_next_token (scanner);
        }
    }

  if (scanner->token == ':')
    {
      /* Add glob selector if path is empty */
      if (selector_path_depth (path) == 0)
        selector_path_prepend_glob (path);

      css_provider_push_scope (css_provider, SCOPE_PSEUDO_CLASS);

      while (scanner->token == ':')
        {
          GTokenType token;

          if ((token = parse_pseudo_class (css_provider, scanner, path)) != G_TOKEN_NONE)
            return token;

          g_scanner_get_next_token (scanner);
        }

      css_provider_pop_scope (css_provider);
    }

  return G_TOKEN_NONE;
}

#define SKIP_SPACES(s) while (s[0] == ' ' || s[0] == '\t' || s[0] == '\n') s++;
#define SKIP_SPACES_BACK(s) while (s[0] == ' ' || s[0] == '\t' || s[0] == '\n') s--;

static GtkSymbolicColor *
symbolic_color_parse_str (const gchar  *string,
                          gchar       **end_ptr)
{
  GtkSymbolicColor *symbolic_color = NULL;
  gchar *str;

  str = (gchar *) string;
  *end_ptr = str;

  if (str[0] == '@')
    {
      const gchar *end;
      gchar *name;

      str++;
      end = str;

      while (*end == '-' || *end == '_' || g_ascii_isalpha (*end))
        end++;

      name = g_strndup (str, end - str);
      symbolic_color = gtk_symbolic_color_new_name (name);
      g_free (name);

      *end_ptr = (gchar *) end;
    }
  else if (g_str_has_prefix (str, "lighter") ||
           g_str_has_prefix (str, "darker"))
    {
      GtkSymbolicColor *param_color;
      gboolean is_lighter = FALSE;

      is_lighter = g_str_has_prefix (str, "lighter");

      if (is_lighter)
        str += strlen ("lighter");
      else
        str += strlen ("darker");

      SKIP_SPACES (str);

      if (*str != '(')
        {
          *end_ptr = (gchar *) str;
          return NULL;
        }

      str++;
      SKIP_SPACES (str);
      param_color = symbolic_color_parse_str (str, end_ptr);

      if (!param_color)
        return NULL;

      str = *end_ptr;
      SKIP_SPACES (str);
      *end_ptr = (gchar *) str;

      if (*str != ')')
        {
          gtk_symbolic_color_unref (param_color);
          return NULL;
        }

      if (is_lighter)
        symbolic_color = gtk_symbolic_color_new_shade (param_color, 1.3);
      else
        symbolic_color = gtk_symbolic_color_new_shade (param_color, 0.7);

      gtk_symbolic_color_unref (param_color);
      (*end_ptr)++;
    }
  else if (g_str_has_prefix (str, "shade") ||
           g_str_has_prefix (str, "alpha"))
    {
      GtkSymbolicColor *param_color;
      gboolean is_shade = FALSE;
      gdouble factor;

      is_shade = g_str_has_prefix (str, "shade");

      if (is_shade)
        str += strlen ("shade");
      else
        str += strlen ("alpha");

      SKIP_SPACES (str);

      if (*str != '(')
        {
          *end_ptr = (gchar *) str;
          return NULL;
        }

      str++;
      SKIP_SPACES (str);
      param_color = symbolic_color_parse_str (str, end_ptr);

      if (!param_color)
        return NULL;

      str = *end_ptr;
      SKIP_SPACES (str);

      if (str[0] != ',')
        {
          gtk_symbolic_color_unref (param_color);
          *end_ptr = (gchar *) str;
          return NULL;
        }

      str++;
      SKIP_SPACES (str);
      factor = g_ascii_strtod (str, end_ptr);

      str = *end_ptr;
      SKIP_SPACES (str);
      *end_ptr = (gchar *) str;

      if (str[0] != ')')
        {
          gtk_symbolic_color_unref (param_color);
          return NULL;
        }

      if (is_shade)
        symbolic_color = gtk_symbolic_color_new_shade (param_color, factor);
      else
        symbolic_color = gtk_symbolic_color_new_alpha (param_color, factor);

      gtk_symbolic_color_unref (param_color);
      (*end_ptr)++;
    }
  else if (g_str_has_prefix (str, "mix"))
    {
      GtkSymbolicColor *color1, *color2;
      gdouble factor;

      str += strlen ("mix");
      SKIP_SPACES (str);

      if (*str != '(')
        {
          *end_ptr = (gchar *) str;
          return NULL;
        }

      str++;
      SKIP_SPACES (str);
      color1 = symbolic_color_parse_str (str, end_ptr);

      if (!color1)
        return NULL;

      str = *end_ptr;
      SKIP_SPACES (str);

      if (str[0] != ',')
        {
          gtk_symbolic_color_unref (color1);
          *end_ptr = (gchar *) str;
          return NULL;
        }

      str++;
      SKIP_SPACES (str);
      color2 = symbolic_color_parse_str (str, end_ptr);

      if (!color2 || *end_ptr[0] != ',')
        {
          gtk_symbolic_color_unref (color1);
          return NULL;
        }

      str = *end_ptr;
      SKIP_SPACES (str);

      if (str[0] != ',')
        {
          gtk_symbolic_color_unref (color1);
          gtk_symbolic_color_unref (color2);
          *end_ptr = (gchar *) str;
          return NULL;
        }

      str++;
      SKIP_SPACES (str);
      factor = g_ascii_strtod (str, end_ptr);

      str = *end_ptr;
      SKIP_SPACES (str);
      *end_ptr = (gchar *) str;

      if (str[0] != ')')
        {
          gtk_symbolic_color_unref (color1);
          gtk_symbolic_color_unref (color2);
          return NULL;
        }

      symbolic_color = gtk_symbolic_color_new_mix (color1, color2, factor);
      gtk_symbolic_color_unref (color1);
      gtk_symbolic_color_unref (color2);
      (*end_ptr)++;
    }
  else
    {
      GdkRGBA color;
      gchar *color_str;
      const gchar *end;

      end = str + 1;

      if (str[0] == '#')
        {
          /* Color in hex format */
          while (g_ascii_isxdigit (*end))
            end++;
        }
      else if (g_str_has_prefix (str, "rgb"))
        {
          /* color in rgb/rgba format */
          while (*end != ')' && *end != '\0')
            end++;

          if (*end == ')')
            end++;
        }
      else
        {
          /* Color name */
          while (*end != '\0' &&
                 (g_ascii_isalnum (*end) || *end == ' '))
            end++;
        }

      color_str = g_strndup (str, end - str);
      *end_ptr = (gchar *) end;

      if (!gdk_rgba_parse (&color, color_str))
        {
          g_free (color_str);
          return NULL;
        }

      symbolic_color = gtk_symbolic_color_new_literal (&color);
      g_free (color_str);
    }

  return symbolic_color;
}

static GtkSymbolicColor *
symbolic_color_parse (const gchar  *str,
                      GError      **error)
{
  GtkSymbolicColor *color;
  gchar *end;

  color = symbolic_color_parse_str (str, &end);

  if (*end != '\0')
    {
      g_set_error_literal (error,
                           GTK_CSS_PROVIDER_ERROR,
                           GTK_CSS_PROVIDER_ERROR_FAILED,
                           "Could not parse symbolic color");

      if (color)
        {
          gtk_symbolic_color_unref (color);
          color = NULL;
        }
    }

  return color;
}

static GtkGradient *
gradient_parse_str (const gchar  *str,
                    gchar       **end_ptr)
{
  GtkGradient *gradient = NULL;
  gdouble coords[6];
  gchar *end;
  guint i;

  if (g_str_has_prefix (str, "-gtk-gradient"))
    {
      cairo_pattern_type_t type;

      str += strlen ("-gtk-gradient");
      SKIP_SPACES (str);

      if (*str != '(')
        {
          *end_ptr = (gchar *) str;
          return NULL;
        }

      str++;
      SKIP_SPACES (str);

      /* Parse gradient type */
      if (g_str_has_prefix (str, "linear"))
        {
          type = CAIRO_PATTERN_TYPE_LINEAR;
          str += strlen ("linear");
        }
      else if (g_str_has_prefix (str, "radial"))
        {
          type = CAIRO_PATTERN_TYPE_RADIAL;
          str += strlen ("radial");
        }
      else
        {
          *end_ptr = (gchar *) str;
          return NULL;
        }

      SKIP_SPACES (str);

      /* Parse start/stop position parameters */
      for (i = 0; i < 2; i++)
        {
          if (*str != ',')
            {
              *end_ptr = (gchar *) str;
              return NULL;
            }

          str++;
          SKIP_SPACES (str);

          if (strncmp (str, "left", 4) == 0)
            {
              coords[i * 3] = 0;
              str += strlen ("left");
            }
          else if (strncmp (str, "right", 5) == 0)
            {
              coords[i * 3] = 1;
              str += strlen ("right");
            }
          else if (strncmp (str, "center", 6) == 0)
            {
              coords[i * 3] = 0.5;
              str += strlen ("center");
            }
          else
            {
              coords[i * 3] = g_ascii_strtod (str, &end);

              if (str == end)
                {
                  *end_ptr = (gchar *) str;
                  return NULL;
                }

              str = end;
            }

          SKIP_SPACES (str);

          if (strncmp (str, "top", 3) == 0)
            {
              coords[(i * 3) + 1] = 0;
              str += strlen ("top");
            }
          else if (strncmp (str, "bottom", 6) == 0)
            {
              coords[(i * 3) + 1] = 1;
              str += strlen ("bottom");
            }
          else if (strncmp (str, "center", 6) == 0)
            {
              coords[(i * 3) + 1] = 0.5;
              str += strlen ("center");
            }
          else
            {
              coords[(i * 3) + 1] = g_ascii_strtod (str, &end);

              if (str == end)
                {
                  *end_ptr = (gchar *) str;
                  return NULL;
                }

              str = end;
            }

          SKIP_SPACES (str);

          if (type == CAIRO_PATTERN_TYPE_RADIAL)
            {
              /* Parse radius */
              if (*str != ',')
                {
                  *end_ptr = (gchar *) str;
                  return NULL;
                }

              str++;
              SKIP_SPACES (str);

              coords[(i * 3) + 2] = g_ascii_strtod (str, &end);
              str = end;

              SKIP_SPACES (str);
            }
        }

      if (type == CAIRO_PATTERN_TYPE_LINEAR)
        gradient = gtk_gradient_new_linear (coords[0], coords[1], coords[3], coords[4]);
      else
        gradient = gtk_gradient_new_radial (coords[0], coords[1], coords[2],
                                            coords[3], coords[4], coords[5]);

      while (*str == ',')
        {
          GtkSymbolicColor *color;
          gdouble position;

          if (*str != ',')
            {
              *end_ptr = (gchar *) str;
              return gradient;
            }

          str++;
          SKIP_SPACES (str);

          if (g_str_has_prefix (str, "from"))
            {
              position = 0;
              str += strlen ("from");
              SKIP_SPACES (str);

              if (*str != '(')
                {
                  *end_ptr = (gchar *) str;
                  return gradient;
                }
            }
          else if (g_str_has_prefix (str, "to"))
            {
              position = 1;
              str += strlen ("to");
              SKIP_SPACES (str);

              if (*str != '(')
                {
                  *end_ptr = (gchar *) str;
                  return gradient;
                }
            }
          else if (g_str_has_prefix (str, "color-stop"))
            {
              str += strlen ("color-stop");
              SKIP_SPACES (str);

              if (*str != '(')
                {
                  *end_ptr = (gchar *) str;
                  return gradient;
                }

              str++;
              SKIP_SPACES (str);

              position = g_ascii_strtod (str, &end);

              str = end;
              SKIP_SPACES (str);

              if (*str != ',')
                {
                  *end_ptr = (gchar *) str;
                  return gradient;
                }
            }
          else
            {
              *end_ptr = (gchar *) str;
              return gradient;
            }

          str++;
          SKIP_SPACES (str);

          color = symbolic_color_parse_str (str, &end);

          str = end;
          SKIP_SPACES (str);

          if (*str != ')')
            {
              *end_ptr = (gchar *) str;
              return gradient;
            }

          str++;
          SKIP_SPACES (str);

          if (color)
            {
              gtk_gradient_add_color_stop (gradient, position, color);
              gtk_symbolic_color_unref (color);
            }
        }

      if (*str != ')')
        {
          *end_ptr = (gchar *) str;
          return gradient;
        }

      str++;
    }

  *end_ptr = (gchar *) str;

  return gradient;
}

static gchar *
path_parse_str (GtkCssProvider  *css_provider,
                const gchar     *str,
                gchar          **end_ptr,
                GError         **error)
{
  gchar *path, *chr;
  const gchar *start, *end;
  start = str;

  if (g_str_has_prefix (str, "url"))
    {
      str += strlen ("url");
      SKIP_SPACES (str);

      if (*str != '(')
        {
          *end_ptr = (gchar *) str;
          return NULL;
        }

      chr = strchr (str, ')');
      if (!chr)
        {
          *end_ptr = (gchar *) str;
          return NULL;
        }

      end = chr + 1;

      str++;
      SKIP_SPACES (str);

      if (*str == '"' || *str == '\'')
        {
          const gchar *p;
          p = str;
          str++;

          chr--;
          SKIP_SPACES_BACK (chr);

          if (*chr != *p || chr == p)
            {
              *end_ptr = (gchar *)str;
              return NULL;
            }
        }
      else
        {
          *end_ptr = (gchar *)str;
          return NULL;
        }

      path = g_strndup (str, chr - str);
      g_strstrip (path);

      *end_ptr = (gchar *)end;
    }
  else
    {
      path = g_strdup (str);
      *end_ptr = (gchar *)str + strlen (str);
    }

  /* Always return an absolute path */
  if (!g_path_is_absolute (path))
    {
      GtkCssProviderPrivate *priv;
      gchar *dirname, *full_path;

      priv = css_provider->priv;

      /* Use relative path to the current CSS file path, if any */
      if (priv->filename)
        dirname = g_path_get_dirname (priv->filename);
      else
        dirname = g_get_current_dir ();

      full_path = g_build_filename (dirname, path, NULL);
      g_free (path);
      g_free (dirname);

      path = full_path;
    }

  if (!g_file_test (path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR))
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_EXIST,
                   "File doesn't exist: %s", path);
      g_free (path);
      path = NULL;
      *end_ptr = (gchar *)start;
    }

  return path;
}

static gchar *
path_parse (GtkCssProvider  *css_provider,
            const gchar     *str,
            GError         **error)
{
  gchar *path;
  gchar *end;

  path = path_parse_str (css_provider, str, &end, error);

  if (!path)
    return NULL;

  if (*end != '\0')
    {
      g_set_error_literal (error,
                           GTK_CSS_PROVIDER_ERROR,
                           GTK_CSS_PROVIDER_ERROR_FAILED,
                           "Error parsing path");
      g_free (path);
      path = NULL;
    }

  return path;
}

static Gtk9Slice *
slice_parse_str (GtkCssProvider  *css_provider,
                 const gchar     *str,
                 gchar          **end_ptr,
                 GError         **error)
{
  gdouble distance_top, distance_bottom;
  gdouble distance_left, distance_right;
  GtkSliceSideModifier mods[2];
  GdkPixbuf *pixbuf;
  Gtk9Slice *slice;
  gchar *path;
  gint i = 0;

  SKIP_SPACES (str);

  /* Parse image url */
  path = path_parse_str (css_provider, str, end_ptr, error);

  if (!path)
      return NULL;

  str = *end_ptr;
  SKIP_SPACES (str);

  /* Parse top/left/bottom/right distances */
  distance_top = g_ascii_strtod (str, end_ptr);

  str = *end_ptr;
  SKIP_SPACES (str);

  distance_right = g_ascii_strtod (str, end_ptr);

  str = *end_ptr;
  SKIP_SPACES (str);

  distance_bottom = g_ascii_strtod (str, end_ptr);

  str = *end_ptr;
  SKIP_SPACES (str);

  distance_left = g_ascii_strtod (str, end_ptr);

  str = *end_ptr;
  SKIP_SPACES (str);

  while (*str && i < 2)
    {
      if (g_str_has_prefix (str, "stretch"))
        {
          str += strlen ("stretch");
          mods[i] = GTK_SLICE_STRETCH;
        }
      else if (g_str_has_prefix (str, "repeat"))
        {
          str += strlen ("repeat");
          mods[i] = GTK_SLICE_REPEAT;
        }
      else
        {
          g_free (path);
          *end_ptr = (gchar *) str;
          return NULL;
        }

      SKIP_SPACES (str);
      i++;
    }

  *end_ptr = (gchar *) str;

  if (*str != '\0')
    {
      g_free (path);
      return NULL;
    }

  if (i != 2)
    {
      /* Fill in second modifier, same as the first */
      mods[1] = mods[0];
    }

  pixbuf = gdk_pixbuf_new_from_file (path, error);
  g_free (path);

  if (!pixbuf)
    {
      *end_ptr = (gchar *) str;
      return NULL;
    }

  slice = _gtk_9slice_new (pixbuf,
                           distance_top, distance_bottom,
                           distance_left, distance_right,
                           mods[0], mods[1]);
  g_object_unref (pixbuf);

  return slice;
}

static gdouble
unit_parse_str (const gchar     *str,
                gchar          **end_str)
{
  gdouble unit;

  SKIP_SPACES (str);
  unit = g_ascii_strtod (str, end_str);
  str = *end_str;

  /* Now parse the unit type, if any. We
   * don't admit spaces between these.
   */
  if (*str != ' ' && *str != '\0')
    {
      while (**end_str != ' ' && **end_str != '\0')
        (*end_str)++;

      /* Only handle pixels at the moment */
      if (strncmp (str, "px", 2) != 0)
        {
          gchar *type;

          type = g_strndup (str, *end_str - str);
          g_warning ("Unknown unit '%s', only pixel units are "
                     "currently supported in CSS style", type);
          g_free (type);
        }
    }

  return unit;
}

static GtkBorder *
border_parse_str (const gchar  *str,
                  gchar       **end_str)
{
  gdouble first, second, third, fourth;
  GtkBorder *border;

  border = gtk_border_new ();

  SKIP_SPACES (str);
  if (!g_ascii_isdigit (*str) && *str != '-')
    return border;

  first = unit_parse_str (str, end_str);
  str = *end_str;
  SKIP_SPACES (str);

  if (!g_ascii_isdigit (*str) && *str != '-')
    {
      border->left = border->right = border->top = border->bottom = (gint) first;
      *end_str = (gchar *) str;
      return border;
    }

  second = unit_parse_str (str, end_str);
  str = *end_str;
  SKIP_SPACES (str);

  if (!g_ascii_isdigit (*str) && *str != '-')
    {
      border->top = border->bottom = (gint) first;
      border->left = border->right = (gint) second;
      *end_str = (gchar *) str;
      return border;
    }

  third = unit_parse_str (str, end_str);
  str = *end_str;
  SKIP_SPACES (str);

  if (!g_ascii_isdigit (*str) && *str != '-')
    {
      border->top = (gint) first;
      border->left = border->right = (gint) second;
      border->bottom = (gint) third;
      *end_str = (gchar *) str;
      return border;
    }

  fourth = unit_parse_str (str, end_str);

  border->top = (gint) first;
  border->right = (gint) second;
  border->bottom = (gint) third;
  border->left = (gint) fourth;

  return border;
}

static void
resolve_binding_sets (const gchar *value_str,
                      GValue      *value)
{
  GPtrArray *array;
  gchar **bindings, **str;

  bindings = g_strsplit (value_str, ",", -1);
  array = g_ptr_array_new ();

  for (str = bindings; *str; str++)
    {
      GtkBindingSet *binding_set;

      binding_set = gtk_binding_set_find (g_strstrip (*str));

      if (!binding_set)
        continue;

      g_ptr_array_add (array, binding_set);
    }

  g_value_take_boxed (value, array);
  g_strfreev (bindings);
}

static gboolean
css_provider_parse_value (GtkCssProvider  *css_provider,
                          const gchar     *value_str,
                          GValue          *value,
                          GError         **error)
{
  GtkCssProviderPrivate *priv;
  GType type;
  gboolean parsed = TRUE;
  gchar *end = NULL;

  priv = css_provider->priv;
  type = G_VALUE_TYPE (value);

  if (type == GDK_TYPE_RGBA ||
      type == GDK_TYPE_COLOR)
    {
      GdkRGBA rgba;
      GdkColor color;

      if (type == GDK_TYPE_RGBA &&
          gdk_rgba_parse (&rgba, value_str))
        g_value_set_boxed (value, &rgba);
      else if (type == GDK_TYPE_COLOR &&
               gdk_color_parse (value_str, &color))
        g_value_set_boxed (value, &color);
      else
        {
          GtkSymbolicColor *symbolic_color;

          symbolic_color = symbolic_color_parse_str (value_str, &end);

          if (symbolic_color)
            {
              g_value_unset (value);
              g_value_init (value, GTK_TYPE_SYMBOLIC_COLOR);
              g_value_take_boxed (value, symbolic_color);
            }
          else
            parsed = FALSE;
        }
    }
  else if (type == PANGO_TYPE_FONT_DESCRIPTION)
    {
      PangoFontDescription *font_desc;

      font_desc = pango_font_description_from_string (value_str);
      g_value_take_boxed (value, font_desc);
    }
  else if (type == G_TYPE_BOOLEAN)
    {
      if (value_str[0] == '1' ||
          g_ascii_strcasecmp (value_str, "true") == 0)
        g_value_set_boolean (value, TRUE);
      else
        g_value_set_boolean (value, FALSE);
    }
  else if (type == G_TYPE_INT)
    g_value_set_int (value, atoi (value_str));
  else if (type == G_TYPE_UINT)
    g_value_set_uint (value, (guint) atoi (value_str));
  else if (type == G_TYPE_DOUBLE)
    g_value_set_double (value, g_ascii_strtod (value_str, NULL));
  else if (type == G_TYPE_FLOAT)
    g_value_set_float (value, (gfloat) g_ascii_strtod (value_str, NULL));
  else if (type == GTK_TYPE_THEMING_ENGINE)
    {
      GtkThemingEngine *engine;

      engine = gtk_theming_engine_load (value_str);
      if (engine)
        g_value_set_object (value, engine);
      else
        parsed = FALSE;
    }
  else if (type == GTK_TYPE_ANIMATION_DESCRIPTION)
    {
      GtkAnimationDescription *desc;

      desc = _gtk_animation_description_from_string (value_str);

      if (desc)
        g_value_take_boxed (value, desc);
      else
        parsed = FALSE;
    }
  else if (type == GTK_TYPE_BORDER)
    {
      GtkBorder *border;

      border = border_parse_str (value_str, &end);
      g_value_take_boxed (value, border);
    }
  else if (type == CAIRO_GOBJECT_TYPE_PATTERN)
    {
      GtkGradient *gradient;

      gradient = gradient_parse_str (value_str, &end);

      if (gradient)
        {
          g_value_unset (value);
          g_value_init (value, GTK_TYPE_GRADIENT);
          g_value_take_boxed (value, gradient);
        }
      else
        {
          gchar *path;
          GdkPixbuf *pixbuf;

          g_clear_error (error);
          path = path_parse_str (css_provider, value_str, &end, error);

          if (path)
            {
              pixbuf = gdk_pixbuf_new_from_file (path, NULL);
              g_free (path);

              if (pixbuf)
                {
                  cairo_surface_t *surface;
                  cairo_pattern_t *pattern;
                  cairo_t *cr;
                  cairo_matrix_t matrix;

                  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                                        gdk_pixbuf_get_width (pixbuf),
                                                        gdk_pixbuf_get_height (pixbuf));
                  cr = cairo_create (surface);
                  gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
                  cairo_paint (cr);
                  pattern = cairo_pattern_create_for_surface (surface);

                  cairo_matrix_init_scale (&matrix,
                                           gdk_pixbuf_get_width (pixbuf),
                                           gdk_pixbuf_get_height (pixbuf));
                  cairo_pattern_set_matrix (pattern, &matrix);

                  cairo_surface_destroy (surface);
                  cairo_destroy (cr);
                  g_object_unref (pixbuf);

                  g_value_take_boxed (value, pattern);
                }
              else
                parsed = FALSE;
            }
          else
            parsed = FALSE;
        }
    }
  else if (G_TYPE_IS_ENUM (type))
    {
      GEnumClass *enum_class;
      GEnumValue *enum_value;

      enum_class = g_type_class_ref (type);
      enum_value = g_enum_get_value_by_nick (enum_class, value_str);

      if (!enum_value)
        {
          g_set_error (error,
                       GTK_CSS_PROVIDER_ERROR,
                       GTK_CSS_PROVIDER_ERROR_FAILED,
                       "Unknown value '%s' for enum type '%s'",
                       value_str, g_type_name (type));
          parsed = FALSE;
        }
      else
        g_value_set_enum (value, enum_value->value);

      g_type_class_unref (enum_class);
    }
  else if (G_TYPE_IS_FLAGS (type))
    {
      GFlagsClass *flags_class;
      GFlagsValue *flag_value;
      guint flags = 0;
      gchar *ptr;

      flags_class = g_type_class_ref (type);

      /* Parse comma separated values */
      ptr = strchr (value_str, ',');

      while (ptr && parsed)
        {
          gchar *flag_str;

          *ptr = '\0';
          ptr++;

          flag_str = (gchar *) value_str;
          flag_value = g_flags_get_value_by_nick (flags_class,
                                                  g_strstrip (flag_str));

          if (!flag_value)
            {
              g_set_error (error,
                           GTK_CSS_PROVIDER_ERROR,
                           GTK_CSS_PROVIDER_ERROR_FAILED,
                           "Unknown flag '%s' for type '%s'",
                           value_str, g_type_name (type));
              parsed = FALSE;
            }
          else
            flags |= flag_value->value;

          value_str = ptr;
          ptr = strchr (value_str, ',');
        }

      /* Store last/only value */
      flag_value = g_flags_get_value_by_nick (flags_class, value_str);

      if (!flag_value)
        {
          g_set_error (error,
                       GTK_CSS_PROVIDER_ERROR,
                       GTK_CSS_PROVIDER_ERROR_FAILED,
                       "Unknown flag '%s' for type '%s'",
                       value_str, g_type_name (type));
          parsed = FALSE;
        }
      else
        flags |= flag_value->value;

      if (parsed)
        g_value_set_enum (value, flags);

      g_type_class_unref (flags_class);
    }
  else if (type == GTK_TYPE_9SLICE)
    {
      Gtk9Slice *slice;

      slice = slice_parse_str (css_provider, value_str, &end, error);

      if (slice)
        g_value_take_boxed (value, slice);
      else
        parsed = FALSE;
    }
  else
    {
      g_set_error (error,
                   GTK_CSS_PROVIDER_ERROR,
                   GTK_CSS_PROVIDER_ERROR_FAILED,
                   "Cannot parse string '%s' for type %s",
                   value_str, g_type_name (type));
      parsed = FALSE;
    }

  if (end && *end)
    {
      /* Set error position in the scanner
       * according to what we've parsed so far
       */
      priv->value_pos += (end - value_str);

      if (error && !*error)
        g_set_error_literal (error,
                             GTK_CSS_PROVIDER_ERROR,
                             GTK_CSS_PROVIDER_ERROR_FAILED,
                             "Failed to parse value");
    }

  return parsed;
}

static void
scanner_report_warning (GtkCssProvider *css_provider,
                        GTokenType      expected_token,
                        GError         *error)
{
  GtkCssProviderPrivate *priv;
  const gchar *line_end, *line_start;
  const gchar *expected_str;
  gchar buf[2], *line, *str;
  guint pos;

  priv = css_provider->priv;

  if (error)
    str = g_strdup (error->message);
  else
    {
      if (priv->scanner->user_data)
        expected_str = priv->scanner->user_data;
      else
        {
          switch (expected_token)
            {
            case G_TOKEN_SYMBOL:
              expected_str = "Symbol";
            case G_TOKEN_IDENTIFIER:
              expected_str = "Identifier";
            default:
              buf[0] = expected_token;
              buf[1] = '\0';
              expected_str = buf;
            }
        }

      str = g_strdup_printf ("Parse error, expecting a %s '%s'",
                             (expected_str != buf) ? "valid" : "",
                             expected_str);
    }

  if (priv->value_pos)
    line_start = priv->value_pos - 1;
  else
    line_start = priv->scanner->text - 1;

  while (*line_start != '\n' &&
         line_start != priv->buffer)
    line_start--;

  if (*line_start == '\n')
    line_start++;

  if (priv->value_pos)
    pos = priv->value_pos - line_start + 1;
  else
    pos = priv->scanner->text - line_start - 1;

  line_end = strchr (line_start, '\n');

  if (line_end)
    line = g_strndup (line_start, (line_end - line_start));
  else
    line = g_strdup (line_start);

  g_message ("CSS: %s\n"
             "%s, line %d, char %d:\n"
             "%*c %s\n"
             "%*c ^",
             str, priv->scanner->input_name,
             priv->scanner->line, priv->scanner->position,
             3, ' ', line,
             3 + pos, ' ');

  g_free (line);
  g_free (str);
}

static GTokenType
parse_rule (GtkCssProvider  *css_provider,
            GScanner        *scanner,
            GError         **error)
{
  GtkCssProviderPrivate *priv;
  GTokenType expected_token;
  SelectorPath *selector;

  priv = css_provider->priv;

  css_provider_push_scope (css_provider, SCOPE_SELECTOR);

  /* Handle directives */
  if (scanner->token == G_TOKEN_IDENTIFIER &&
      scanner->value.v_identifier[0] == '@')
    {
      gchar *directive;

      directive = &scanner->value.v_identifier[1];

      if (strcmp (directive, "define-color") == 0)
        {
          GtkSymbolicColor *color;
          gchar *color_name, *color_str;

          /* Directive is a color mapping */
          g_scanner_get_next_token (scanner);

          if (scanner->token != G_TOKEN_IDENTIFIER)
            {
              scanner->user_data = "Color name";
              return G_TOKEN_IDENTIFIER;
            }

          color_name = g_strdup (scanner->value.v_identifier);
          css_provider_push_scope (css_provider, SCOPE_VALUE);
          g_scanner_get_next_token (scanner);

          if (scanner->token != G_TOKEN_IDENTIFIER)
            {
              scanner->user_data = "Color definition";
              return G_TOKEN_IDENTIFIER;
            }

          color_str = g_strstrip (scanner->value.v_identifier);
          color = symbolic_color_parse (color_str, error);

          if (!color)
            {
              scanner->user_data = "Color definition";
              return G_TOKEN_IDENTIFIER;
            }

          g_hash_table_insert (priv->symbolic_colors, color_name, color);

          css_provider_pop_scope (css_provider);
          g_scanner_get_next_token (scanner);

          if (scanner->token != ';')
            return ';';

          return G_TOKEN_NONE;
        }
      else if (strcmp (directive, "import") == 0)
        {
          GScanner *scanner_backup;
          GSList *state_backup;
          gboolean loaded;
          gchar *path = NULL;

          css_provider_push_scope (css_provider, SCOPE_VALUE);
          g_scanner_get_next_token (scanner);

          if (scanner->token == G_TOKEN_IDENTIFIER &&
              g_str_has_prefix (scanner->value.v_identifier, "url"))
            path = path_parse (css_provider,
                               g_strstrip (scanner->value.v_identifier),
                               error);
          else if (scanner->token == G_TOKEN_STRING)
            path = path_parse (css_provider,
                               g_strstrip (scanner->value.v_string),
                               error);

          if (path == NULL)
            {
              scanner->user_data = "File URL";
              return G_TOKEN_IDENTIFIER;
            }

          css_provider_pop_scope (css_provider);
          g_scanner_get_next_token (scanner);

          if (scanner->token != ';')
            {
              g_free (path);
              return ';';
            }

          /* Snapshot current parser state and scanner in order to restore after importing */
          state_backup = priv->state;
          scanner_backup = priv->scanner;

          priv->state = NULL;
          priv->scanner = create_scanner ();

          /* FIXME: Avoid recursive importing */
          loaded = gtk_css_provider_load_from_path_internal (css_provider, path,
                                                             FALSE, error);

          /* Restore previous state */
          css_provider_reset_parser (css_provider);
          priv->state = state_backup;
          g_scanner_destroy (priv->scanner);
          priv->scanner = scanner_backup;

          g_free (path);

          if (!loaded)
            {
              scanner->user_data = "File URL";
              return G_TOKEN_IDENTIFIER;
            }
          else
            return G_TOKEN_NONE;
        }
      else if (strcmp (directive, "binding-set") == 0)
        {
          GtkBindingSet *binding_set;
          gchar *binding_set_name;

          g_scanner_get_next_token (scanner);

          if (scanner->token != G_TOKEN_IDENTIFIER)
            {
              scanner->user_data = "Binding name";
              return G_TOKEN_IDENTIFIER;
            }

          binding_set_name = scanner->value.v_identifier;
          binding_set = gtk_binding_set_find (binding_set_name);

          if (!binding_set)
            {
              binding_set = gtk_binding_set_new (binding_set_name);
              binding_set->parsed = TRUE;
            }

          g_scanner_get_next_token (scanner);

          if (scanner->token != G_TOKEN_LEFT_CURLY)
            return G_TOKEN_LEFT_CURLY;

          css_provider_push_scope (css_provider, SCOPE_BINDING_SET);
          g_scanner_get_next_token (scanner);

          do
            {
              GTokenType ret;

              if (scanner->token != G_TOKEN_IDENTIFIER)
                {
                  scanner->user_data = "Binding definition";
                  return G_TOKEN_IDENTIFIER;
                }

              ret = gtk_binding_entry_add_signal_from_string (binding_set,
                                                              scanner->value.v_identifier);
              if (ret != G_TOKEN_NONE)
                {
                  scanner->user_data = "Binding definition";
                  return ret;
                }

              g_scanner_get_next_token (scanner);

              if (scanner->token != ';')
                return ';';

              g_scanner_get_next_token (scanner);
            }
          while (scanner->token != G_TOKEN_RIGHT_CURLY);

          css_provider_pop_scope (css_provider);
          g_scanner_get_next_token (scanner);

          return G_TOKEN_NONE;
        }
      else
        {
          scanner->user_data = "Directive";
          return G_TOKEN_IDENTIFIER;
        }
    }

  expected_token = parse_selector (css_provider, scanner, &selector);

  if (expected_token != G_TOKEN_NONE)
    {
      selector_path_unref (selector);
      scanner->user_data = "Selector";
      return expected_token;
    }

  priv->cur_selectors = g_slist_prepend (priv->cur_selectors, selector);

  while (scanner->token == ',')
    {
      g_scanner_get_next_token (scanner);

      expected_token = parse_selector (css_provider, scanner, &selector);

      if (expected_token != G_TOKEN_NONE)
        {
          selector_path_unref (selector);
          scanner->user_data = "Selector";
          return expected_token;
        }

      priv->cur_selectors = g_slist_prepend (priv->cur_selectors, selector);
    }

  css_provider_pop_scope (css_provider);

  if (scanner->token != G_TOKEN_LEFT_CURLY)
    return G_TOKEN_LEFT_CURLY;

  /* Declarations parsing */
  css_provider_push_scope (css_provider, SCOPE_DECLARATION);
  g_scanner_get_next_token (scanner);

  while (scanner->token == G_TOKEN_IDENTIFIER)
    {
      gchar *value_str = NULL;
      GtkStylePropertyParser parse_func = NULL;
      GParamSpec *pspec;
      gchar *prop;

      prop = g_strdup (scanner->value.v_identifier);
      g_scanner_get_next_token (scanner);

      if (scanner->token != ':')
        {
          g_free (prop);
          return ':';
        }

      priv->value_pos = priv->scanner->text;

      css_provider_push_scope (css_provider, SCOPE_VALUE);
      g_scanner_get_next_token (scanner);

      if (scanner->token != G_TOKEN_IDENTIFIER)
        {
          g_free (prop);
          scanner->user_data = "Property value";
          return G_TOKEN_IDENTIFIER;
        }

      value_str = scanner->value.v_identifier;
      SKIP_SPACES (value_str);
      g_strchomp (value_str);

      if (gtk_style_properties_lookup_property (prop, &parse_func, &pspec))
        {
          GValue *val;

          val = g_slice_new0 (GValue);
          g_value_init (val, pspec->value_type);

          if (strcmp (value_str, "none") == 0)
            {
              /* Insert the default value, so it has an opportunity
               * to override other style providers when merged
               */
              g_param_value_set_default (pspec, val);
              g_hash_table_insert (priv->cur_properties, prop, val);
            }
          else if (strcmp (prop, "gtk-key-bindings") == 0)
            {
              /* Private property holding the binding sets */
              resolve_binding_sets (value_str, val);
              g_hash_table_insert (priv->cur_properties, prop, val);
            }
          else if (pspec->value_type == G_TYPE_STRING)
            {
              g_value_set_string (val, value_str);
              g_hash_table_insert (priv->cur_properties, prop, val);
            }
          else if ((parse_func && (parse_func) (value_str, val, error)) ||
                   (!parse_func && css_provider_parse_value (css_provider, value_str, val, error)))
            g_hash_table_insert (priv->cur_properties, prop, val);
          else
            {
              g_value_unset (val);
              g_slice_free (GValue, val);
              g_free (prop);

              scanner->user_data = "Property value";
              return G_TOKEN_IDENTIFIER;
            }
        }
      else if (prop[0] == '-')
        {
          GValue *val;

          val = g_slice_new0 (GValue);
          g_value_init (val, G_TYPE_STRING);
          g_value_set_string (val, value_str);

          g_hash_table_insert (priv->cur_properties, prop, val);
        }
      else
        g_free (prop);

      css_provider_pop_scope (css_provider);
      g_scanner_get_next_token (scanner);

      if (scanner->token != ';')
        break;

      g_scanner_get_next_token (scanner);
    }

  if (scanner->token != G_TOKEN_RIGHT_CURLY)
    return G_TOKEN_RIGHT_CURLY;

  css_provider_pop_scope (css_provider);

  return G_TOKEN_NONE;
}

static gboolean
parse_stylesheet (GtkCssProvider  *css_provider,
                  GError         **error)
{
  GtkCssProviderPrivate *priv;
  gboolean result;

  result = TRUE;

  priv = css_provider->priv;
  g_scanner_get_next_token (priv->scanner);

  while (!g_scanner_eof (priv->scanner))
    {
      GTokenType expected_token;
      GError *err = NULL;

      css_provider_reset_parser (css_provider);
      expected_token = parse_rule (css_provider, priv->scanner, &err);

      if (expected_token != G_TOKEN_NONE)
        {
          /* If a GError was passed in, propagate the error and bail out,
           * else report a warning and keep going
           */
          if (error != NULL)
            {
              result = FALSE;
              if (err)
                g_propagate_error (error, err);
              else
                g_set_error_literal (error,
                                     GTK_CSS_PROVIDER_ERROR,
                                     GTK_CSS_PROVIDER_ERROR_FAILED,
                                     "Error parsing stylesheet");
              break;
            }
          else
            {
              scanner_report_warning (css_provider, expected_token, err);
              g_clear_error (&err);
            }

          while (!g_scanner_eof (priv->scanner) &&
                 priv->scanner->token != G_TOKEN_RIGHT_CURLY)
            g_scanner_get_next_token (priv->scanner);
        }
      else
        css_provider_commit (css_provider);

      g_scanner_get_next_token (priv->scanner);
    }

  return result;
}

/**
 * gtk_css_provider_load_from_data:
 * @css_provider: a #GtkCssProvider
 * @data: CSS data loaded in memory
 * @length: the length of @data in bytes, or -1 for NUL terminated strings
 * @error: (out) (allow-none): return location for a #GError, or %NULL
 *
 * Loads @data into @css_provider, making it clear any previously loaded
 * information.
 *
 * Returns: %TRUE if the data could be loaded.
 **/
gboolean
gtk_css_provider_load_from_data (GtkCssProvider  *css_provider,
                                 const gchar     *data,
                                 gssize           length,
                                 GError         **error)
{
  GtkCssProviderPrivate *priv;

  g_return_val_if_fail (GTK_IS_CSS_PROVIDER (css_provider), FALSE);
  g_return_val_if_fail (data != NULL, FALSE);

  priv = css_provider->priv;

  if (length < 0)
    length = strlen (data);

  if (priv->selectors_info->len > 0)
    g_ptr_array_remove_range (priv->selectors_info, 0, priv->selectors_info->len);

  priv->scanner->input_name = "-";
  priv->buffer = data;
  g_scanner_input_text (priv->scanner, data, (guint) length);

  g_free (priv->filename);
  priv->filename = NULL;
  priv->buffer = NULL;

  return parse_stylesheet (css_provider, error);
}

/**
 * gtk_css_provider_load_from_file:
 * @css_provider: a #GtkCssProvider
 * @file: #GFile pointing to a file to load
 * @error: (out) (allow-none): return location for a #GError, or %NULL
 *
 * Loads the data contained in @file into @css_provider, making it
 * clear any previously loaded information.
 *
 * Returns: %TRUE if the data could be loaded.
 **/
gboolean
gtk_css_provider_load_from_file (GtkCssProvider  *css_provider,
                                 GFile           *file,
                                 GError         **error)
{
  GtkCssProviderPrivate *priv;
  GError *internal_error = NULL;
  gchar *data;
  gsize length;
  gboolean ret;

  g_return_val_if_fail (GTK_IS_CSS_PROVIDER (css_provider), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  priv = css_provider->priv;

  if (!g_file_load_contents (file, NULL,
                             &data, &length,
                             NULL, &internal_error))
    {
      g_propagate_error (error, internal_error);
      return FALSE;
    }

  if (priv->selectors_info->len > 0)
    g_ptr_array_remove_range (priv->selectors_info, 0, priv->selectors_info->len);

  g_free (priv->filename);
  priv->filename = g_file_get_path (file);

  priv->scanner->input_name = priv->filename;
  priv->buffer = data;
  g_scanner_input_text (priv->scanner, data, (guint) length);

  ret = parse_stylesheet (css_provider, error);

  priv->buffer = NULL;
  g_free (data);

  return ret;
}

static gboolean
gtk_css_provider_load_from_path_internal (GtkCssProvider  *css_provider,
                                          const gchar     *path,
                                          gboolean         reset,
                                          GError         **error)
{
  GtkCssProviderPrivate *priv;
  GError *internal_error = NULL;
  GMappedFile *mapped_file;
  const gchar *data;
  gsize length;
  gboolean ret;

  priv = css_provider->priv;

  mapped_file = g_mapped_file_new (path, FALSE, &internal_error);

  if (internal_error)
    {
      g_propagate_error (error, internal_error);
      return FALSE;
    }

  length = g_mapped_file_get_length (mapped_file);
  data = g_mapped_file_get_contents (mapped_file);

  if (!data)
    data = "";

  if (reset)
    {
      if (priv->selectors_info->len > 0)
        g_ptr_array_remove_range (priv->selectors_info, 0, priv->selectors_info->len);

      g_free (priv->filename);
      priv->filename = g_strdup (path);
    }

  priv->scanner->input_name = priv->filename;
  priv->buffer = data;
  g_scanner_input_text (priv->scanner, data, (guint) length);

  ret = parse_stylesheet (css_provider, error);

  priv->buffer = NULL;
  g_mapped_file_unref (mapped_file);

  return ret;
}

/**
 * gtk_css_provider_load_from_path:
 * @css_provider: a #GtkCssProvider
 * @path: the path of a filename to load, in the GLib filename encoding
 * @error: (out) (allow-none): return location for a #GError, or %NULL
 *
 * Loads the data contained in @path into @css_provider, making it clear
 * any previously loaded information.
 *
 * Returns: %TRUE if the data could be loaded.
 **/
gboolean
gtk_css_provider_load_from_path (GtkCssProvider  *css_provider,
                                 const gchar     *path,
                                 GError         **error)
{
  g_return_val_if_fail (GTK_IS_CSS_PROVIDER (css_provider), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  return gtk_css_provider_load_from_path_internal (css_provider, path,
                                                   TRUE, error);
}

/**
 * gtk_css_provider_get_default:
 *
 * Returns the provider containing the style settings used as a
 * fallback for all widgets.
 *
 * Returns: (transfer none): The provider used for fallback styling.
 *          This memory is owned by GTK+, and you must not free it.
 **/
GtkCssProvider *
gtk_css_provider_get_default (void)
{
  static GtkCssProvider *provider;

  if (G_UNLIKELY (!provider))
    {
      const gchar *str =
        "@define-color fg_color #000; \n"
        "@define-color bg_color #dcdad5; \n"
        "@define-color text_color #000; \n"
        "@define-color base_color #fff; \n"
        "@define-color selected_bg_color #4b6983; \n"
        "@define-color selected_fg_color #fff; \n"
        "@define-color tooltip_bg_color #eee1b3; \n"
        "@define-color tooltip_fg_color #000; \n"
        "\n"
        "@define-color info_fg_color rgb (181, 171, 156);\n"
        "@define-color info_bg_color rgb (252, 252, 189);\n"
        "@define-color warning_fg_color rgb (173, 120, 41);\n"
        "@define-color warning_bg_color rgb (250, 173, 61);\n"
        "@define-color question_fg_color rgb (97, 122, 214);\n"
        "@define-color question_bg_color rgb (138, 173, 212);\n"
        "@define-color error_fg_color rgb (166, 38, 38);\n"
        "@define-color error_bg_color rgb (237, 54, 54);\n"
        "\n"
        "* {\n"
        "  background-color: @bg_color;\n"
        "  color: @fg_color;\n"
        "  border-color: shade (@bg_color, 0.6);\n"
        "  padding: 2;\n"
        "  border-width: 0;\n"
        "}\n"
        "\n"
        "*:prelight {\n"
        "  background-color: shade (@bg_color, 1.05);\n"
        "  color: shade (@fg_color, 1.3);\n"
        "}\n"
        "\n"
        "*:selected {\n"
        "  background-color: @selected_bg_color;\n"
        "  color: @selected_fg_color;\n"
        "}\n"
        "\n"
        ".expander, GtkTreeView.view.expander {\n"
        "  color: #fff;\n"
        "}\n"
        "\n"
        ".expander:prelight {\n"
        "  color: @text_color;\n"
        "}\n"
        "\n"
        ".expander:active {\n"
        "  transition: 200ms linear;\n"
        "}\n"
        "\n"
        "*:insensitive {\n"
        "  border-color: shade (@bg_color, 0.7);\n"
        "  background-color: shade (@bg_color, 0.9);\n"
        "  color: shade (@bg_color, 0.7);\n"
        "}\n"
        "\n"
        ".view {\n"
        "  border-width: 0;\n"
        "  border-radius: 0;\n"
        "  background-color: @base_color;\n"
        "  color: @text_color;\n"
        "}\n"
        ".view:selected {\n"
        "  background-color: shade (@bg_color, 0.9);\n"
        "  color: @fg_color;\n"
        "}\n"
        "\n"
        ".view:selected:focused {\n"
        "  background-color: @selected_bg_color;\n"
        "  color: @selected_fg_color;\n"
        "}\n"
        "\n"
        ".view column:sorted row,\n"
        ".view column:sorted row:prelight {\n"
        "  background-color: shade (@bg_color, 0.85);\n"
        "}\n"
        "\n"
        ".view column:sorted row:nth-child(odd),\n"
        ".view column:sorted row:nth-child(odd):prelight {\n"
        "  background-color: shade (@bg_color, 0.8);\n"
        "}\n"
        "\n"
        ".view row,\n"
        ".view row:prelight {\n"
        "  background-color: @base_color;\n"
        "  color: @text_color;\n"
        "}\n"
        "\n"
        ".view row:nth-child(odd),\n"
        ".view row:nth-child(odd):prelight {\n"
        "  background-color: shade (@base_color, 0.93); \n"
        "}\n"
        "\n"
        ".view row:selected:focused {\n"
        "  background-color: @selected_bg_color;\n"
        "}\n"
        "\n"
        ".view row:selected {\n"
        "  background-color: darker (@bg_color);\n"
        "  color: @selected_fg_color;\n"
        "}\n"
        "\n"
        ".view.cell.trough,\n"
        ".view.cell.trough:hover,\n"
        ".view.cell.trough:selected,\n"
        ".view.cell.trough:selected:focused {\n"
        "  background-color: @bg_color;\n"
        "  color: @fg_color;\n"
        "}\n"
        "\n"
        ".view.cell.progressbar,\n"
        ".view.cell.progressbar:hover,\n"
        ".view.cell.progressbar:selected,\n"
        ".view.cell.progressbar:selected:focused {\n"
        "  background-color: @selected_bg_color;\n"
        "  color: @selected_fg_color;\n"
        "}\n"
        "\n"
        ".rubberband {\n"
        "  background-color: alpha (@fg_color, 0.25);\n"
        "  border-color: @fg_color;\n"
        "  border-style: solid;\n"
        "  border-width: 1;\n"
        "}\n"
        "\n"
        ".tooltip {\n"
        "  background-color: @tooltip_bg_color; \n"
        "  color: @tooltip_fg_color; \n"
        "  border-color: @tooltip_fg_color; \n"
        "  border-width: 1;\n"
        "  border-style: solid;\n"
        "}\n"
        "\n"
        ".button,\n"
        ".slider {\n"
        "  border-style: outset; \n"
        "  border-width: 2; \n"
        "}\n"
        "\n"
        ".button:active {\n"
        "  background-color: shade (@bg_color, 0.7);\n"
        "  border-style: inset; \n"
        "}\n"
        "\n"
        ".button:prelight,\n"
        ".slider:prelight {\n"
        "  background-color: @selected_bg_color;\n"
        "  color: @selected_fg_color;\n"
        "  border-color: shade (@selected_bg_color, 0.7);\n"
        "}\n"
        "\n"
        ".trough {\n"
        "  background-color: darker (@bg_color);\n"
        "  border-style: inset;\n"
        "  border-width: 1;\n"
        "  padding: 0;\n"
        "}\n"
        "\n"
        ".entry {\n"
        "  border-style: inset;\n"
        "  border-width: 2;\n"
        "  background-color: @base_color;\n"
        "  color: @text_color;\n"
        "}\n"
        "\n"
        ".entry:insensitive {\n"
        "  background-color: shade (@base_color, 0.9);\n"
        "  color: shade (@base_color, 0.7);\n"
        "}\n"
        ".entry:active {\n"
        "  background-color: #c4c2bd;\n"
        "  color: #000;\n"
        "}\n"
        "\n"
        ".progressbar,\n"
        ".entry.progressbar, \n"
        ".cell.progressbar {\n"
        "  background-color: @selected_bg_color;\n"
        "  border-color: shade (@selected_bg_color, 0.7);\n"
        "  color: @selected_fg_color;\n"
        "  border-style: outset;\n"
        "  border-width: 1;\n"
        "}\n"
        "\n"
        "GtkCheckButton:hover,\n"
        "GtkCheckButton:selected,\n"
        "GtkRadioButton:hover,\n"
        "GtkRadioButton:selected {\n"
        "  background-color: shade (@bg_color, 1.05);\n"
        "}\n"
        "\n"
        ".check, .radio,"
        ".cell.check, .cell.radio,\n"
        ".cell.check:hover, .cell.radio:hover {\n"
        "  border-style: solid;\n"
        "  border-width: 1;\n"
        "  background-color: @base_color;\n"
        "  border-color: @fg_color;\n"
        "}\n"
        "\n"
        ".check:active, .radio:active,\n"
        ".check:hover, .radio:hover {\n"
        "  background-color: @base_color;\n"
        "  border-color: @fg_color;\n"
        "  color: @text_color;\n"
        "}\n"
        "\n"
        ".check:selected, .radio:selected {\n"
        "  background-color: darker (@bg_color);\n"
        "  color: @selected_fg_color;\n"
        "  border-color: @selected_fg_color;\n"
        "}\n"
        "\n"
        ".check:selected:focused, .radio:selected:focused {\n"
        "  background-color: @selected_bg_color;\n"
        "}\n"
        "\n"
        ".menu.check, .menu.radio {\n"
        "  color: @fg_color;\n"
        "  border-style: none;\n"
        "  border-width: 0;\n"
        "}\n"
        "\n"
        ".popup {\n"
        "  border-style: outset;\n"
        "  border-width: 1;\n"
        "}\n"
        "\n"
        ".viewport {\n"
        "  border-style: inset;\n"
        "  border-width: 2;\n"
        "}\n"
        "\n"
        ".notebook {\n"
        "  border-style: outset;\n"
        "  border-width: 1;\n"
        "}\n"
        "\n"
        ".frame {\n"
        "  border-style: inset;\n"
        "  border-width: 1;\n"
        "}\n"
        "\n"
        "GtkScrolledWindow.frame {\n"
        "  padding: 0;\n"
        "}\n"
        "\n"
        ".menu,\n"
        ".menubar,\n"
        ".toolbar {\n"
        "  border-style: outset;\n"
        "  border-width: 1;\n"
        "}\n"
        "\n"
        ".menu:hover,\n"
        ".menubar:hover,\n"
        ".menu.check:hover,\n"
        ".menu.radio:hover {\n"
        "  background-color: @selected_bg_color;\n"
        "  color: @selected_fg_color;\n"
        "}\n"
        "\n"
        "GtkSpinButton.button {\n"
        "  border-width: 1;\n"
        "}\n"
        "\n"
        ".scale.slider:hover,\n"
        "GtkSpinButton.button:hover {\n"
        "  background-color: shade (@bg_color, 1.05);\n"
        "  border-color: shade (@bg_color, 0.8);\n"
        "}\n"
        "\n"
        "GtkSwitch.trough:active {\n"
        "  background-color: @selected_bg_color;\n"
        "  color: @selected_fg_color;\n"
        "}\n"
        "\n"
        "GtkToggleButton.button:inconsistent {\n"
        "  border-style: outset;\n"
        "  border-width: 1px;\n"
        "  background-color: shade (@bg_color, 0.9);\n"
        "  border-color: shade (@bg_color, 0.7);\n"
        "}\n"
        "\n"
        "GtkLabel:selected {\n"
        "  background-color: shade (@bg_color, 0.9);\n"
        "}\n"
        "\n"
        "GtkLabel:selected:focused {\n"
        "  background-color: @selected_bg_color;\n"
        "}\n"
        "\n"
        ".spinner:active {\n"
        "  transition: 750ms linear loop;\n"
        "}\n"
        "\n"
        ".info {\n"
        "  background-color: @info_bg_color;\n"
        "  color: @info_fg_color;\n"
        "}\n"
        "\n"
        ".warning {\n"
        "  background-color: @warning_bg_color;\n"
        "  color: @warning_fg_color;\n"
        "}\n"
        "\n"
        ".question {\n"
        "  background-color: @question_bg_color;\n"
        "  color: @question_fg_color;\n"
        "}\n"
        "\n"
        ".error {\n"
        "  background-color: @error_bg_color;\n"
        "  color: @error_fg_color;\n"
        "}\n"
        "\n"
        ".highlight {\n"
        "  background-color: @selected_bg_color;\n"
        "  color: @selected_fg_color;\n"
        "}\n"
        "\n"
        ".light-area-focus {\n"
        "  color: #000;\n"
        "}\n"
        "\n"
        ".dark-area-focus {\n"
        "  color: #fff;\n"
        "}\n"
        "GtkCalendar.view {\n"
        "  border-width: 1;\n"
        "  border-style: inset;\n"
        "  padding: 1;\n"
        "}\n"
        "\n"
        "GtkCalendar.view:inconsistent {\n"
        "  color: darker (@bg_color);\n"
        "}\n"
        "\n"
        "GtkCalendar.header {\n"
        "  background-color: @bg_color;\n"
        "  border-style: outset;\n"
        "  border-width: 2;\n"
        "}\n"
        "\n"
        "GtkCalendar.highlight {\n"
        "  border-width: 0;\n"
        "}\n"
        "\n"
        "GtkCalendar.button {\n"
        "  background-color: @bg_color;\n"
        "}\n"
        "\n"
        "GtkCalendar.button:hover {\n"
        "  background-color: lighter (@bg_color);\n"
        "  color: @fg_color;\n"
        "}\n"
        "\n"
        ".menu {\n"
        "  border-width: 1;\n"
        "  padding: 0;\n"
        "}\n"
        "\n"
        ".menu * {\n"
        "  border-width: 0;\n"
        "  padding: 2;\n"
        "}\n"
        "\n";

      provider = gtk_css_provider_new ();
      if (!gtk_css_provider_load_from_data (provider, str, -1, NULL))
        {
          g_error ("Failed to load the internal default CSS.");
        }
    }

  return provider;
}

gchar *
_gtk_css_provider_get_theme_dir (void)
{
  const gchar *var;
  gchar *path;

  var = g_getenv ("GTK_DATA_PREFIX");

  if (var)
    path = g_build_filename (var, "share", "themes", NULL);
  else
    path = g_build_filename (GTK_DATA_PREFIX, "share", "themes", NULL);

  return path;
}

/**
 * gtk_css_provider_get_named:
 * @name: A theme name
 * @variant: (allow-none): variant to load, for example, "dark", or
 *     %NULL for the default
 *
 * Loads a theme from the usual theme paths
 *
 * Returns: (transfer none): a #GtkCssProvider with the theme loaded.
 *     This memory is owned by GTK+, and you must not free it.
 */
GtkCssProvider *
gtk_css_provider_get_named (const gchar *name,
                            const gchar *variant)
{
  static GHashTable *themes = NULL;
  GtkCssProvider *provider;
  gchar *key;

  if (G_UNLIKELY (!themes))
    themes = g_hash_table_new (g_str_hash, g_str_equal);

  if (variant == NULL)
    key = (gchar *)name;
  else
    key = g_strconcat (name, "-", variant, NULL);

  provider = g_hash_table_lookup (themes, key);

  if (!provider)
    {
      const gchar *home_dir;
      gchar *subpath, *path = NULL;

      if (variant)
        subpath = g_strdup_printf ("gtk-3.0" G_DIR_SEPARATOR_S "gtk-%s.css", variant);
      else
        subpath = g_strdup ("gtk-3.0" G_DIR_SEPARATOR_S "gtk.css");

      /* First look in the users home directory
       */
      home_dir = g_get_home_dir ();
      if (home_dir)
        {
          path = g_build_filename (home_dir, ".themes", name, subpath, NULL);

          if (!g_file_test (path, G_FILE_TEST_EXISTS))
            {
              g_free (path);
              path = NULL;
            }
        }

      if (!path)
        {
          gchar *theme_dir;

          theme_dir = _gtk_css_provider_get_theme_dir ();
          path = g_build_filename (theme_dir, name, subpath, NULL);
          g_free (theme_dir);

          if (!g_file_test (path, G_FILE_TEST_EXISTS))
            {
              g_free (path);
              path = NULL;
            }
        }

      g_free (subpath);

      if (path)
        {
          GError *error;

          provider = gtk_css_provider_new ();
          error = NULL;
          if (!gtk_css_provider_load_from_path (provider, path, &error))
            {
              g_warning ("Could not load named theme \"%s\": %s", name, error->message);
              g_error_free (error);

              g_object_unref (provider);
              provider = NULL;
            }
          else
            g_hash_table_insert (themes, g_strdup (key), provider);

          g_free (path);
        }
    }

  if (key != name)
    g_free (key);

  return provider;
}
