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

#include "gtkcssparserprivate.h"
#include "gtkcsssectionprivate.h"
#include "gtkcssselectorprivate.h"
#include "gtksymboliccolor.h"
#include "gtkstyleprovider.h"
#include "gtkstylecontextprivate.h"
#include "gtkstylepropertiesprivate.h"
#include "gtkstylepropertyprivate.h"
#include "gtkbindings.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkintl.h"

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
 *     font: Sans 20
 * }
 *
 * /&ast; Theme a label named title-label &ast;/
 * GtkLabel&num;title-label {
 *     font: Sans 15
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
 * <refsect2 id="gtkcssprovider-shadows">
 * <title>Text shadow</title>
 * <para>
 * A shadow list can be applied to text or symbolic icons, using the CSS3
 * text-shadow syntax, as defined in
 * <ulink url="http://www.w3.org/TR/css3-text/#text-shadow">the CSS3 specification</ulink>.
 * </para>
 * <para>
 * A text shadow is specified using the syntax
 * <literallayout>text-shadow: @horizontal_offset @vertical_offset [ @blur_radius ] @color</literallayout>
 * The offset of the shadow is specified with the @horizontal_offset and @vertical_offset
 * parameters. The optional blur radius is parsed, but it is currently not rendered by
 * the GTK+ theming engine.
 * </para>
 * <para>
 * To set multiple shadows on an element, you can specify a comma-separated list
 * of shadow elements in the text-shadow property. Shadows are always rendered
 * front-back, i.e. the first shadow specified is on top of the others. Shadows
 * can thus overlay each other, but they can never overlay the text itself,
 * which is always rendered on top of the shadow layer.
 * </para>
 * </refsect2>
 * <refsect2>
 * <title>Box shadow</title>
 * <para>
 * Themes can apply shadows on framed elements using the CSS3 box-shadow syntax,
 * as defined in 
 * <ulink url="http://www.w3.org/TR/css3-background/#the-box-shadow">the CSS3 specification</ulink>.
 * </para>
 * <para>
 * A box shadow is specified using the syntax
 * <literallayout>box-shadow: [ @inset ] @horizontal_offset @vertical_offset [ @blur_radius ] [ @spread ] @color</literallayout>
 * A positive offset will draw a shadow that is offset to the right (down) of the box,
 * a negative offset to the left (top). The optional spread parameter defines an additional
 * distance to expand the shadow shape in all directions, by the specified radius.
 * The optional blur radius parameter is parsed, but it is currently not rendered by
 * the GTK+ theming engine.
 * The inset parameter defines whether the drop shadow should be rendered inside or outside
 * the box canvas. Only inset box-shadows are currently supported by the GTK+ theming engine,
 * non-inset elements are currently ignored.
 * </para>
 * <para>
 * To set multiple box-shadows on an element, you can specify a comma-separated list
 * of shadow elements in the box-shadow property. Shadows are always rendered
 * front-back, i.e. the first shadow specified is on top of the others, so they may
 * overlap other boxes or other shadows.
 * </para>
 * </refsect2>
 * <refsect2 id="gtkcssprovider-border-image">
 * <title>Border images</title>
 * <para>
 * Images and gradients can also be used in slices for the purpose of creating
 * scalable borders.
 * For more information, see the CSS3 documentation for the border-image property,
 * which can be found <ulink url="http://www.w3.org/TR/css3-background/#border-images">here</ulink>.
 * </para>
 * <inlinegraphic fileref="slices.png" format="PNG"/>
 * <para>
 * The parameters of the slicing process are controlled by
 * four separate properties. Note that you can use the
 * <literallayout>border-image</literallayout> shorthand property
 * to set values for the three properties at the same time.
 * </para>
 * <para>
 * <literallayout>border-image-source: url(@path)
 * (or border-image-source: -gtk-gradient(...))</literallayout>:
 * Specifies the source of the border image, and it can either
 * be an URL or a gradient (see above).
 * </para>
 * <para>
 * <literallayout>border-image-slice: @top @right @bottom @left</literallayout>
 * The sizes specified by the @top, @right, @bottom and @left parameters
 * are the offsets, in pixels, from the relevant edge where the image
 * should be "cut off" to build the slices used for the rendering
 * of the border.
 * </para>
 * <para>
 * <literallayout>border-image-width: @top @right @bottom @left</literallayout>
 * The sizes specified by the @top, @right, @bottom and @left parameters
 * are inward distances from the border box edge, used to specify the
 * rendered size of each slice determined by border-image-slice.
 * If this property is not specified, the values of border-width will
 * be used as a fallback.
 * </para>
 * <para>
 * <literallayout>border-image-repeat: [stretch|repeat|round|space] ? 
 * [stretch|repeat|round|space]</literallayout>
 * Specifies how the image slices should be rendered in the area
 * outlined by border-width.
 * The default (stretch) is to resize the slice to fill in the whole 
 * allocated area.
 * If the value of this property is 'repeat', the image slice
 * will be tiled to fill the area.
 * If the value of this property is 'round', the image slice will
 * be tiled to fill the area, and scaled to fit it exactly
 * a whole number of times.
 * If the value of this property is 'space', the image slice will
 * be tiled to fill the area, and if it doesn't fit it exactly a whole
 * number of times, the extra space is distributed as padding around 
 * the slices.
 * If two options are specified, the first one affects
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
 *         <entry morerows="7">#GdkRGBA</entry>
 *         <entry morerows="7"><literallayout>background-color: &num;fff;
 * color: &amp;color1;
 * background-color: shade (&amp;color1, 0.5);
 * color: mix (&amp;color1, &num;f0f, 0.8);</literallayout>
 *         </entry>
 *       </row>
 *       <row>
 *         <entry>color</entry>
 *       </row>
 *       <row>
 *         <entry>border-top-color</entry>
 *         <entry morerows="4">transparent|color (see above)</entry>
 *       </row>
 *       <row>
 *         <entry>border-right-color</entry>
 *       </row>
 *       <row>
 *         <entry>border-bottom-color</entry>
 *       </row>
 *       <row>
 *         <entry>border-left-color</entry>
 *       </row>
 *       <row>
 *         <entry>border-color</entry>
 *         <entry>[transparent|color]{1,4}</entry>
 *       </row>
 *       <row>
 *         <entry>font-family</entry>
 *         <entry>@family [, @family]*</entry>
 *         <entry>#gchararray</entry>
 *         <entry>font-family: Sans, Arial;</entry>
 *       </row>
 *       <row>
 *         <entry>font-style</entry>
 *         <entry>[normal|oblique|italic]</entry>
 *         <entry>#PANGO_TYPE_STYLE</entry>
 *         <entry>font-style: italic;</entry>
 *       </row>
 *       <row>
 *         <entry>font-variant</entry>
 *         <entry>[normal|small-caps]</entry>
 *         <entry>#PANGO_TYPE_VARIANT</entry>
 *         <entry>font-variant: normal;</entry>
 *       </row>
 *       <row>
 *         <entry>font-weight</entry>
 *         <entry>[normal|bold|bolder|lighter|100|200|300|400|500|600|700|800|900]</entry>
 *         <entry>#PANGO_TYPE_WEIGHT</entry>
 *         <entry>font-weight: bold;</entry>
 *       </row>
 *       <row>
 *         <entry>font-size</entry>
 *         <entry>Font size in point</entry>
 *         <entry>#gint</entry>
 *         <entry>font-size: 13;</entry>
 *       </row>
 *       <row>
 *         <entry>font</entry>
 *         <entry>@family [@style] [@size]</entry>
 *         <entry>#PangoFontDescription</entry>
 *         <entry>font: Sans 15;</entry>
 *       </row>
 *       <row>
 *         <entry>margin-top</entry>
 *         <entry>integer</entry>
 *         <entry>#gint</entry>
 *         <entry>margin-top: 0;</entry>
 *       </row>
 *       <row>
 *         <entry>margin-left</entry>
 *         <entry>integer</entry>
 *         <entry>#gint</entry>
 *         <entry>margin-left: 1;</entry>
 *       </row>
 *       <row>
 *         <entry>margin-bottom</entry>
 *         <entry>integer</entry>
 *         <entry>#gint</entry>
 *         <entry>margin-bottom: 2;</entry>
 *       </row>
 *       <row>
 *         <entry>margin-right</entry>
 *         <entry>integer</entry>
 *         <entry>#gint</entry>
 *         <entry>margin-right: 4;</entry>
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
 *         <entry>padding-top</entry>
 *         <entry>integer</entry>
 *         <entry>#gint</entry>
 *         <entry>padding-top: 5;</entry>
 *       </row>
 *       <row>
 *         <entry>padding-left</entry>
 *         <entry>integer</entry>
 *         <entry>#gint</entry>
 *         <entry>padding-left: 5;</entry>
 *       </row>
 *       <row>
 *         <entry>padding-bottom</entry>
 *         <entry>integer</entry>
 *         <entry>#gint</entry>
 *         <entry>padding-bottom: 5;</entry>
 *       </row>
 *       <row>
 *         <entry>padding-right</entry>
 *         <entry>integer</entry>
 *         <entry>#gint</entry>
 *         <entry>padding-right: 5;</entry>
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
 *         <entry>border-top-width</entry>
 *         <entry>integer</entry>
 *         <entry>#gint</entry>
 *         <entry>border-top-width: 5;</entry>
 *       </row>
 *       <row>
 *         <entry>border-left-width</entry>
 *         <entry>integer</entry>
 *         <entry>#gint</entry>
 *         <entry>border-left-width: 5;</entry>
 *       </row>
 *       <row>
 *         <entry>border-bottom-width</entry>
 *         <entry>integer</entry>
 *         <entry>#gint</entry>
 *         <entry>border-bottom-width: 5;</entry>
 *       </row>
 *       <row>
 *         <entry>border-right-width</entry>
 *         <entry>integer</entry>
 *         <entry>#gint</entry>
 *         <entry>border-right-width: 5;</entry>
 *       </row>
 *       <row>
 *         <entry>border-width</entry>
 *         <entry morerows="1">#GtkBorder</entry>
 *         <entry morerows="1"><literallayout>border-width: 1;
 * border-width: 1 2;
 * border-width: 1 2 3;
 * border-width: 1 2 3 5;</literallayout>
 *         </entry>
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
 *         <entry>text-shadow</entry>
 *         <entry>shadow list (see above)</entry>
 *         <entry>#GtkTextShadow</entry>
 *         <entry><literallayout>text-shadow: 1 1 0 blue, -4 -4 red;</literallayout></entry>
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

typedef struct GtkCssRuleset GtkCssRuleset;
typedef struct _GtkCssScanner GtkCssScanner;
typedef enum ParserScope ParserScope;
typedef enum ParserSymbol ParserSymbol;

struct GtkCssRuleset
{
  GtkCssSelector *selector;
  GHashTable *widget_style;
  GHashTable *style;

  guint has_inherit :1;
};

struct _GtkCssScanner
{
  GtkCssProvider *provider;
  GtkCssParser *parser;
  GtkCssSection *section;
  GtkCssScanner *parent;
  GFile *file;
  GFile *base;
  GSList *state;
};

struct _GtkCssProviderPrivate
{
  GScanner *scanner;

  GHashTable *symbolic_colors;

  GArray *rulesets;
};

enum {
  PARSING_ERROR,
  LAST_SIGNAL
};

static guint css_provider_signals[LAST_SIGNAL] = { 0 };

static void gtk_css_provider_finalize (GObject *object);
static void gtk_css_style_provider_iface_init (GtkStyleProviderIface *iface);

static gboolean
gtk_css_provider_load_internal (GtkCssProvider *css_provider,
                                GtkCssScanner  *scanner,
                                GFile          *file,
                                const char     *data,
                                GError        **error);

GQuark
gtk_css_provider_error_quark (void)
{
  return g_quark_from_static_string ("gtk-css-provider-error-quark");
}

G_DEFINE_TYPE_EXTENDED (GtkCssProvider, gtk_css_provider, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_STYLE_PROVIDER,
                                               gtk_css_style_provider_iface_init));

static void
gtk_css_provider_parsing_error (GtkCssProvider  *provider,
                                GtkCssSection   *section,
                                const GError    *error)
{
  /* Only emit a warning when we have no error handlers. This is our
   * default handlers. And in this case erroneous CSS files are a bug
   * and should be fixed.
   * Note that these warnings can also be triggered by a broken theme
   * that people installed from some weird location on the internets.
   */
  if (!g_signal_has_handler_pending (provider,
                                     css_provider_signals[PARSING_ERROR],
                                     0,
                                     TRUE))
    {
      GFileInfo *info;
      GFile *file;
      const char *path;

      file = gtk_css_section_get_file (section);
      if (file)
        {
          info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME, 0, NULL, NULL);

          if (info)
            path = g_file_info_get_display_name (info);
          else
            path = "<broken file>";
        }
      else
        {
          info = NULL;
          path = "<data>";
        }

      g_warning ("Theme parsing error: %s:%u:%u: %s",
                 path,
                 gtk_css_section_get_end_line (section) + 1,
                 gtk_css_section_get_end_position (section),
                 error->message);

      if (info)
        g_object_unref (info);
    }
}

static void
gtk_css_provider_class_init (GtkCssProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /**
   * GtkCssProvider::parsing-error:
   * @provider: the provider that had a parsing error
   * @section: section the error happened in
   * @error: The parsing error
   *
   * Signals that a parsing error occured. the @path, @line and @position
   * describe the actual location of the error as accurately as possible.
   *
   * Parsing errors are never fatal, so the parsing will resume after
   * the error. Errors may however cause parts of the given
   * data or even all of it to not be parsed at all. So it is a useful idea
   * to check that the parsing succeeds by connecting to this signal.
   *
   * Note that this signal may be emitted at any time as the css provider
   * may opt to defer parsing parts or all of the input to a later time
   * than when a loading function was called.
   */
  css_provider_signals[PARSING_ERROR] =
    g_signal_new (I_("parsing-error"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkCssProviderClass, parsing_error),
                  NULL, NULL,
                  _gtk_marshal_VOID__BOXED_BOXED,
                  G_TYPE_NONE, 2, GTK_TYPE_CSS_SECTION, G_TYPE_ERROR);

  object_class->finalize = gtk_css_provider_finalize;

  klass->parsing_error = gtk_css_provider_parsing_error;

  g_type_class_add_private (object_class, sizeof (GtkCssProviderPrivate));
}

static void
gtk_css_ruleset_init_copy (GtkCssRuleset       *new,
                           const GtkCssRuleset *ruleset,
                           GtkCssSelector      *selector)
{
  memcpy (new, ruleset, sizeof (GtkCssRuleset));

  new->selector = selector;
  if (new->widget_style)
    g_hash_table_ref (new->widget_style);
  if (new->style)
    g_hash_table_ref (new->style);
}

static void
gtk_css_ruleset_clear (GtkCssRuleset *ruleset)
{
  if (ruleset->style)
    g_hash_table_unref (ruleset->style);
  if (ruleset->widget_style)
    g_hash_table_unref (ruleset->widget_style);
  if (ruleset->selector)
    _gtk_css_selector_free (ruleset->selector);

  memset (ruleset, 0, sizeof (GtkCssRuleset));
}

typedef struct _PropertyValue PropertyValue;
struct _PropertyValue {
  GtkCssSection *section;
  GValue         value;
};

static PropertyValue *
property_value_new (GtkCssSection *section)
{
  PropertyValue *value;

  value = g_slice_new0 (PropertyValue);

  value->section = gtk_css_section_ref (section);

  return value;
}

static void
property_value_free (PropertyValue *value)
{
  if (G_IS_VALUE (&value->value))
    g_value_unset (&value->value);

  gtk_css_section_unref (value->section);

  g_slice_free (PropertyValue, value);
}

static void
gtk_css_ruleset_add_style (GtkCssRuleset *ruleset,
                           char          *name,
                           PropertyValue *value)
{
  if (ruleset->widget_style == NULL)
    ruleset->widget_style = g_hash_table_new_full (g_str_hash,
                                                   g_str_equal,
                                                   (GDestroyNotify) g_free,
                                                   (GDestroyNotify) property_value_free);

  g_hash_table_insert (ruleset->widget_style, name, value);
}

static void
gtk_css_ruleset_add (GtkCssRuleset          *ruleset,
                     const GtkStyleProperty *prop,
                     PropertyValue          *value)
{
  if (ruleset->style == NULL)
    ruleset->style = g_hash_table_new_full (g_direct_hash,
                                            g_direct_equal,
                                            NULL,
                                            (GDestroyNotify) property_value_free);

  if (_gtk_style_property_is_shorthand (prop))
    {
      GParameter *parameters;
      guint i, n_parameters;

      parameters = _gtk_style_property_unpack (prop, &value->value, &n_parameters);

      for (i = 0; i < n_parameters; i++)
        {
          const GtkStyleProperty *child;
          PropertyValue *val;

          child = _gtk_style_property_lookup (parameters[i].name);
          val = property_value_new (value->section);
          memcpy (&val->value, &parameters[i].value, sizeof (GValue));
          gtk_css_ruleset_add (ruleset, child, val);
        }
      g_free (parameters);
      property_value_free (value);
      return;
    }

  ruleset->has_inherit |= _gtk_style_property_is_inherit (prop);
  g_hash_table_insert (ruleset->style, (gpointer) prop, value);
}

static gboolean
gtk_css_ruleset_matches (GtkCssRuleset *ruleset,
                         GtkWidgetPath *path,
                         guint          length)
{
  return _gtk_css_selector_matches (ruleset->selector, path, length);
}

static void
gtk_css_scanner_destroy (GtkCssScanner *scanner)
{
  g_object_unref (scanner->provider);
  if (scanner->file)
    g_object_unref (scanner->file);
  g_object_unref (scanner->base);
  _gtk_css_parser_free (scanner->parser);

  g_slice_free (GtkCssScanner, scanner);
}

static void
gtk_css_provider_emit_error (GtkCssProvider *provider,
                             GtkCssScanner  *scanner,
                             const GError   *error)
{
  g_signal_emit (provider, css_provider_signals[PARSING_ERROR], 0,
                 scanner != NULL ? scanner->section : NULL, error);
}

static void
gtk_css_scanner_parser_error (GtkCssParser *parser,
                              const GError *error,
                              gpointer      user_data)
{
  GtkCssScanner *scanner = user_data;

  gtk_css_provider_emit_error (scanner->provider,
                               scanner,
                               error);
}

static GtkCssScanner *
gtk_css_scanner_new (GtkCssProvider *provider,
                     GtkCssScanner  *parent,
                     GtkCssSection  *section,
                     GFile          *file,
                     const gchar    *text)
{
  GtkCssScanner *scanner;

  scanner = g_slice_new0 (GtkCssScanner);

  g_object_ref (provider);
  scanner->provider = provider;
  scanner->parent = parent;
  if (section)
    scanner->section = gtk_css_section_ref (section);

  if (file)
    {
      scanner->file = g_object_ref (file);
      scanner->base = g_file_get_parent (file);
    }
  else
    {
      char *dir = g_get_current_dir ();
      scanner->base = g_file_new_for_path (dir);
      g_free (dir);
    }

  scanner->parser = _gtk_css_parser_new (text,
                                         gtk_css_scanner_parser_error,
                                         scanner);

  return scanner;
}

static GFile *
gtk_css_scanner_get_base_url (GtkCssScanner *scanner)
{
  return scanner->base;
}

static gboolean
gtk_css_scanner_would_recurse (GtkCssScanner *scanner,
                               GFile         *file)
{
  while (scanner)
    {
      if (scanner->file && g_file_equal (scanner->file, file))
        return TRUE;

      scanner = scanner->parent;
    }

  return FALSE;
}

static void
gtk_css_scanner_push_section (GtkCssScanner     *scanner,
                              GtkCssSectionType  section_type)
{
  GtkCssSection *section;

  section = _gtk_css_section_new (scanner->section,
                                  section_type,
                                  scanner->parser,
                                  scanner->file);

  if (scanner->section)
    gtk_css_section_unref (scanner->section);
  scanner->section = section;
}

static void
gtk_css_scanner_pop_section (GtkCssScanner *scanner,
                             GtkCssSectionType check_type)
{
  GtkCssSection *parent;
  
  g_assert (gtk_css_section_get_section_type (scanner->section) == check_type);

  parent = gtk_css_section_get_parent (scanner->section);
  if (parent)
    gtk_css_section_ref (parent);

  _gtk_css_section_end (scanner->section);
  gtk_css_section_unref (scanner->section);

  scanner->section = parent;
}

static void
gtk_css_provider_init (GtkCssProvider *css_provider)
{
  GtkCssProviderPrivate *priv;

  priv = css_provider->priv = G_TYPE_INSTANCE_GET_PRIVATE (css_provider,
                                                           GTK_TYPE_CSS_PROVIDER,
                                                           GtkCssProviderPrivate);

  priv->rulesets = g_array_new (FALSE, FALSE, sizeof (GtkCssRuleset));

  priv->symbolic_colors = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                 (GDestroyNotify) g_free,
                                                 (GDestroyNotify) gtk_symbolic_color_unref);
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
  GtkCssProviderPrivate *priv;
  GtkStyleProperties *props;
  guint i, l, length;

  css_provider = GTK_CSS_PROVIDER (provider);
  priv = css_provider->priv;
  length = gtk_widget_path_length (path);
  props = gtk_style_properties_new ();

  css_provider_dump_symbolic_colors (css_provider, props);

  for (l = 1; l <= length; l++)
    {
      for (i = 0; i < priv->rulesets->len; i++)
        {
          GtkCssRuleset *ruleset;
          GHashTableIter iter;
          gpointer key, val;

          ruleset = &g_array_index (priv->rulesets, GtkCssRuleset, i);

          if (ruleset->style == NULL)
            continue;

          if (l < length && (!ruleset->has_inherit || _gtk_css_selector_get_state_flags (ruleset->selector)))
            continue;

          if (!gtk_css_ruleset_matches (ruleset, path, l))
            continue;

          g_hash_table_iter_init (&iter, ruleset->style);

          while (g_hash_table_iter_next (&iter, &key, &val))
            {
              GtkStyleProperty *prop = key;
              PropertyValue *value = val;

              if (l != length && !_gtk_style_property_is_inherit (prop))
                continue;

              _gtk_style_properties_set_property_by_property (props,
                                                              prop,
                                                              _gtk_css_selector_get_state_flags (ruleset->selector),
                                                              &value->value);
            }
        }
    }

  return props;
}

static gboolean
gtk_css_provider_get_style_property (GtkStyleProvider *provider,
                                     GtkWidgetPath    *path,
                                     GtkStateFlags     state,
                                     GParamSpec       *pspec,
                                     GValue           *value)
{
  GtkCssProvider *css_provider = GTK_CSS_PROVIDER (provider);
  GtkCssProviderPrivate *priv = css_provider->priv;
  PropertyValue *val;
  gboolean found = FALSE;
  gchar *prop_name;
  gint i;

  prop_name = g_strdup_printf ("-%s-%s",
                               g_type_name (pspec->owner_type),
                               pspec->name);

  for (i = priv->rulesets->len - 1; i >= 0; i--)
    {
      GtkCssRuleset *ruleset;
      GtkStateFlags selector_state;

      ruleset = &g_array_index (priv->rulesets, GtkCssRuleset, i);

      if (ruleset->widget_style == NULL)
        continue;

      if (!gtk_css_ruleset_matches (ruleset, path, gtk_widget_path_length (path)))
        continue;

      selector_state = _gtk_css_selector_get_state_flags (ruleset->selector);
      val = g_hash_table_lookup (ruleset->widget_style, prop_name);

      if (val &&
          (selector_state == 0 ||
           selector_state == state ||
           ((selector_state & state) != 0 &&
            (selector_state & ~(state)) == 0)))
        {
          GtkCssScanner *scanner;

          scanner = gtk_css_scanner_new (css_provider,
                                         NULL,
                                         val->section,
                                         gtk_css_section_get_file (val->section),
                                         g_value_get_string (&val->value));

          found = _gtk_style_property_parse_value (NULL,
                                                   value,
                                                   scanner->parser,
                                                   NULL);

          gtk_css_scanner_destroy (scanner);

          if (found)
            break;
        }
    }

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
  guint i;

  css_provider = GTK_CSS_PROVIDER (object);
  priv = css_provider->priv;

  for (i = 0; i < priv->rulesets->len; i++)
    gtk_css_ruleset_clear (&g_array_index (priv->rulesets, GtkCssRuleset, i));

  g_array_free (priv->rulesets, TRUE);

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
gtk_css_provider_take_error (GtkCssProvider *provider,
                             GtkCssScanner  *scanner,
                             GError         *error)
{
  gtk_css_provider_emit_error (provider,
                               scanner,
                               error);

  g_error_free (error);
}

static void
gtk_css_provider_error_literal (GtkCssProvider *provider,
                                GtkCssScanner  *scanner,
                                GQuark          domain,
                                gint            code,
                                const char     *message)
{
  gtk_css_provider_take_error (provider,
                               scanner,
                               g_error_new_literal (domain, code, message));
}

static void
gtk_css_provider_error (GtkCssProvider *provider,
                        GtkCssScanner  *scanner,
                        GQuark          domain,
                        gint            code,
                        const char     *format,
                        ...)  G_GNUC_PRINTF (5, 6);
static void
gtk_css_provider_error (GtkCssProvider *provider,
                        GtkCssScanner  *scanner,
                        GQuark          domain,
                        gint            code,
                        const char     *format,
                        ...)
{
  GError *error;
  va_list args;

  va_start (args, format);
  error = g_error_new_valist (domain, code, format, args);
  va_end (args);

  gtk_css_provider_take_error (provider, scanner, error);
}

static void
gtk_css_provider_invalid_token (GtkCssProvider *provider,
                                GtkCssScanner  *scanner,
                                const char     *expected)
{
  gtk_css_provider_error (provider,
                          scanner,
                          GTK_CSS_PROVIDER_ERROR,
                          GTK_CSS_PROVIDER_ERROR_SYNTAX,
                          "expected a valid %s", expected);
}

static void 
css_provider_commit (GtkCssProvider *css_provider,
                     GSList         *selectors,
                     GtkCssRuleset  *ruleset)
{
  GtkCssProviderPrivate *priv;
  GSList *l;

  priv = css_provider->priv;

  if (ruleset->style == NULL && ruleset->widget_style == NULL)
    {
      g_slist_free_full (selectors, (GDestroyNotify) _gtk_css_selector_free);
      return;
    }

  for (l = selectors; l; l = l->next)
    {
      GtkCssRuleset new;

      gtk_css_ruleset_init_copy (&new, ruleset, l->data);

      g_array_append_val (priv->rulesets, new);
    }

  g_slist_free (selectors);
}

static void
gtk_css_provider_reset (GtkCssProvider *css_provider)
{
  GtkCssProviderPrivate *priv;
  guint i;

  priv = css_provider->priv;

  g_hash_table_remove_all (priv->symbolic_colors);

  for (i = 0; i < priv->rulesets->len; i++)
    gtk_css_ruleset_clear (&g_array_index (priv->rulesets, GtkCssRuleset, i));
  g_array_set_size (priv->rulesets, 0);
}

static void
gtk_css_provider_propagate_error (GtkCssProvider  *provider,
                                  GtkCssSection   *section,
                                  const GError    *error,
                                  GError         **propagate_to)
{

  GFileInfo *info;
  GFile *file;
  const char *path;

  file = gtk_css_section_get_file (section);
  if (file)
    {
      info = g_file_query_info (file,G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME, 0, NULL, NULL);

      if (info)
        path = g_file_info_get_display_name (info);
      else
        path = "<broken file>";
    }
  else
    {
      info = NULL;
      path = "<unknown>";
    }

  /* don't fail for deprecations */
  if (g_error_matches (error, GTK_CSS_PROVIDER_ERROR, GTK_CSS_PROVIDER_ERROR_DEPRECATED))
    {
      g_warning ("Theme parsing error: %s:%u:%u: %s", path,
                 gtk_css_section_get_end_line (section) + 1,
                 gtk_css_section_get_end_position (section), error->message);
      return;
    }

  /* we already set an error. And we'd like to keep the first one */
  if (*propagate_to)
    return;

  *propagate_to = g_error_copy (error);
  g_prefix_error (propagate_to, "%s:%u:%u: ", path,
                  gtk_css_section_get_end_line (section) + 1,
                  gtk_css_section_get_end_position (section));

  if (info)
    g_object_unref (info);
}

static gboolean
parse_import (GtkCssScanner *scanner)
{
  GFile *file;
  char *uri;

  gtk_css_scanner_push_section (scanner, GTK_CSS_SECTION_IMPORT);

  if (!_gtk_css_parser_try (scanner->parser, "@import", TRUE))
    {
      gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_IMPORT);
      return FALSE;
    }

  if (_gtk_css_parser_is_string (scanner->parser))
    uri = _gtk_css_parser_read_string (scanner->parser);
  else
    uri = _gtk_css_parser_read_uri (scanner->parser);

  if (uri == NULL)
    {
      _gtk_css_parser_resync (scanner->parser, TRUE, 0);
      gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_IMPORT);
      return TRUE;
    }

  file = g_file_resolve_relative_path (gtk_css_scanner_get_base_url (scanner), uri);
  g_free (uri);

  if (gtk_css_scanner_would_recurse (scanner, file))
    {
       char *path = g_file_get_path (file);
       gtk_css_provider_error (scanner->provider,
                               scanner,
                               GTK_CSS_PROVIDER_ERROR,
                               GTK_CSS_PROVIDER_ERROR_IMPORT,
                               "Loading '%s' would recurse",
                               path);
       g_free (path);
    }
  else
    {
      gtk_css_provider_load_internal (scanner->provider,
                                      scanner,
                                      file,
                                      NULL,
                                      NULL);
    }

  if (!_gtk_css_parser_try (scanner->parser, ";", TRUE))
    {
      gtk_css_provider_invalid_token (scanner->provider, scanner, "semicolon");
      _gtk_css_parser_resync (scanner->parser, TRUE, 0);
    }

  g_object_unref (file);

  gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_IMPORT);
  return TRUE;
}

static gboolean
parse_color_definition (GtkCssScanner *scanner)
{
  GtkSymbolicColor *symbolic;
  char *name;

  gtk_css_scanner_push_section (scanner, GTK_CSS_SECTION_COLOR_DEFINITION);

  if (!_gtk_css_parser_try (scanner->parser, "@define-color", TRUE))
    {
      gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_COLOR_DEFINITION);
      return FALSE;
    }

  name = _gtk_css_parser_try_name (scanner->parser, TRUE);
  if (name == NULL)
    {
      gtk_css_provider_error_literal (scanner->provider,
                                      scanner,
                                      GTK_CSS_PROVIDER_ERROR,
                                      GTK_CSS_PROVIDER_ERROR_SYNTAX,
                                      "Not a valid color name");
      _gtk_css_parser_resync (scanner->parser, TRUE, 0);
      gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_COLOR_DEFINITION);
      return TRUE;
    }

  symbolic = _gtk_css_parser_read_symbolic_color (scanner->parser);
  if (symbolic == NULL)
    {
      g_free (name);
      _gtk_css_parser_resync (scanner->parser, TRUE, 0);
      gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_COLOR_DEFINITION);
      return TRUE;
    }

  if (!_gtk_css_parser_try (scanner->parser, ";", TRUE))
    {
      g_free (name);
      gtk_symbolic_color_unref (symbolic);
      gtk_css_provider_error_literal (scanner->provider,
                                      scanner,
                                      GTK_CSS_PROVIDER_ERROR,
                                      GTK_CSS_PROVIDER_ERROR_SYNTAX,
                                      "Missing semicolon at end of color definition");
      _gtk_css_parser_resync (scanner->parser, TRUE, 0);

      gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_COLOR_DEFINITION);
      return TRUE;
    }

  g_hash_table_insert (scanner->provider->priv->symbolic_colors, name, symbolic);

  gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_COLOR_DEFINITION);
  return TRUE;
}

static gboolean
parse_binding_set (GtkCssScanner *scanner)
{
  GtkBindingSet *binding_set;
  char *name;

  gtk_css_scanner_push_section (scanner, GTK_CSS_SECTION_BINDING_SET);

  if (!_gtk_css_parser_try (scanner->parser, "@binding-set", TRUE))
    {
      gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_BINDING_SET);
      return FALSE;
    }

  name = _gtk_css_parser_try_ident (scanner->parser, TRUE);
  if (name == NULL)
    {
      gtk_css_provider_error_literal (scanner->provider,
                                      scanner,
                                      GTK_CSS_PROVIDER_ERROR,
                                      GTK_CSS_PROVIDER_ERROR_SYNTAX,
                                      "Expected name for binding set");
      _gtk_css_parser_resync (scanner->parser, TRUE, 0);
      goto skip_semicolon;
    }

  binding_set = gtk_binding_set_find (name);
  if (!binding_set)
    {
      binding_set = gtk_binding_set_new (name);
      binding_set->parsed = TRUE;
    }
  g_free (name);

  if (!_gtk_css_parser_try (scanner->parser, "{", TRUE))
    {
      gtk_css_provider_error_literal (scanner->provider,
                                      scanner,
                                      GTK_CSS_PROVIDER_ERROR,
                                      GTK_CSS_PROVIDER_ERROR_SYNTAX,
                                      "Expected '{' for binding set");
      _gtk_css_parser_resync (scanner->parser, TRUE, 0);
      goto skip_semicolon;
    }

  while (!_gtk_css_parser_is_eof (scanner->parser) &&
         !_gtk_css_parser_begins_with (scanner->parser, '}'))
    {
      name = _gtk_css_parser_read_value (scanner->parser);
      if (name == NULL)
        {
          _gtk_css_parser_resync (scanner->parser, TRUE, '}');
          continue;
        }

      if (gtk_binding_entry_add_signal_from_string (binding_set, name) != G_TOKEN_NONE)
        {
          gtk_css_provider_error_literal (scanner->provider,
                                          scanner,
                                          GTK_CSS_PROVIDER_ERROR,
                                          GTK_CSS_PROVIDER_ERROR_SYNTAX,
                                          "Failed to parse binding set.");
        }

      g_free (name);

      if (!_gtk_css_parser_try (scanner->parser, ";", TRUE))
        {
          if (!_gtk_css_parser_begins_with (scanner->parser, '}') &&
              !_gtk_css_parser_is_eof (scanner->parser))
            {
              gtk_css_provider_error_literal (scanner->provider,
                                              scanner,
                                              GTK_CSS_PROVIDER_ERROR,
                                              GTK_CSS_PROVIDER_ERROR_SYNTAX,
                                              "Expected semicolon");
              _gtk_css_parser_resync (scanner->parser, TRUE, '}');
            }
        }
    }

  if (!_gtk_css_parser_try (scanner->parser, "}", TRUE))
    {
      gtk_css_provider_error_literal (scanner->provider,
                                      scanner,
                                      GTK_CSS_PROVIDER_ERROR,
                                      GTK_CSS_PROVIDER_ERROR_SYNTAX,
                                      "expected '}' after declarations");
      if (!_gtk_css_parser_is_eof (scanner->parser))
        _gtk_css_parser_resync (scanner->parser, FALSE, 0);
    }

skip_semicolon:
  if (_gtk_css_parser_begins_with (scanner->parser, ';'))
    {
      gtk_css_provider_error_literal (scanner->provider,
                                      scanner,
                                      GTK_CSS_PROVIDER_ERROR,
                                      GTK_CSS_PROVIDER_ERROR_DEPRECATED,
                                      "Nonstandard semicolon at end of binding set");
      _gtk_css_parser_try (scanner->parser, ";", TRUE);
    }

  gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_BINDING_SET);

  return TRUE;
}

static void
parse_at_keyword (GtkCssScanner *scanner)
{
  if (parse_import (scanner))
    return;
  if (parse_color_definition (scanner))
    return;
  if (parse_binding_set (scanner))
    return;

  else
    {
      gtk_css_provider_error_literal (scanner->provider,
                                      scanner,
                                      GTK_CSS_PROVIDER_ERROR,
                                      GTK_CSS_PROVIDER_ERROR_SYNTAX,
                                      "unknown @ rule");
      _gtk_css_parser_resync (scanner->parser, TRUE, 0);
    }
}

static gboolean
parse_selector_class (GtkCssScanner *scanner, GArray *classes)
{
  GQuark qname;
  char *name;
    
  name = _gtk_css_parser_try_name (scanner->parser, FALSE);

  if (name == NULL)
    {
      gtk_css_provider_error_literal (scanner->provider,
                                      scanner,
                                      GTK_CSS_PROVIDER_ERROR,
                                      GTK_CSS_PROVIDER_ERROR_SYNTAX,
                                      "Expected a valid name for class");
      return FALSE;
    }

  qname = g_quark_from_string (name);
  g_array_append_val (classes, qname);
  g_free (name);
  return TRUE;
}

static gboolean
parse_selector_name (GtkCssScanner *scanner, GArray *names)
{
  GQuark qname;
  char *name;
    
  name = _gtk_css_parser_try_name (scanner->parser, FALSE);

  if (name == NULL)
    {
      gtk_css_provider_error_literal (scanner->provider,
                                      scanner,
                                      GTK_CSS_PROVIDER_ERROR,
                                      GTK_CSS_PROVIDER_ERROR_SYNTAX,
                                      "Expected a valid name for id");
      return FALSE;
    }

  qname = g_quark_from_string (name);
  g_array_append_val (names, qname);
  g_free (name);
  return TRUE;
}

static gboolean
parse_selector_pseudo_class (GtkCssScanner  *scanner,
                             GtkRegionFlags *region_to_modify,
                             GtkStateFlags  *state_to_modify)
{
  struct {
    const char *name;
    GtkRegionFlags region_flag;
    GtkStateFlags state_flag;
  } pseudo_classes[] = {
    { "first-child",  GTK_REGION_FIRST, 0 },
    { "last-child",   GTK_REGION_LAST, 0 },
    { "sorted",       GTK_REGION_SORTED, 0 },
    { "active",       0, GTK_STATE_FLAG_ACTIVE },
    { "prelight",     0, GTK_STATE_FLAG_PRELIGHT },
    { "hover",        0, GTK_STATE_FLAG_PRELIGHT },
    { "selected",     0, GTK_STATE_FLAG_SELECTED },
    { "insensitive",  0, GTK_STATE_FLAG_INSENSITIVE },
    { "inconsistent", 0, GTK_STATE_FLAG_INCONSISTENT },
    { "focused",      0, GTK_STATE_FLAG_FOCUSED },
    { "focus",        0, GTK_STATE_FLAG_FOCUSED },
    { NULL, }
  }, nth_child_classes[] = {
    { "first",        GTK_REGION_FIRST, 0 },
    { "last",         GTK_REGION_LAST, 0 },
    { "even",         GTK_REGION_EVEN, 0 },
    { "odd",          GTK_REGION_ODD, 0 },
    { NULL, }
  }, *classes;
  guint i;
  char *name;

  name = _gtk_css_parser_try_ident (scanner->parser, FALSE);
  if (name == NULL)
    {
      gtk_css_provider_error_literal (scanner->provider,
                                      scanner,
                                      GTK_CSS_PROVIDER_ERROR,
                                      GTK_CSS_PROVIDER_ERROR_SYNTAX,
                                      "Missing name of pseudo-class");
      return FALSE;
    }

  if (_gtk_css_parser_try (scanner->parser, "(", TRUE))
    {
      char *function = name;

      name = _gtk_css_parser_try_ident (scanner->parser, TRUE);
      if (!_gtk_css_parser_try (scanner->parser, ")", FALSE))
        {
          gtk_css_provider_error_literal (scanner->provider,
                                          scanner,
                                          GTK_CSS_PROVIDER_ERROR,
                                          GTK_CSS_PROVIDER_ERROR_SYNTAX,
                                          "Missing closing bracket for pseudo-class");
          return FALSE;
        }

      if (g_ascii_strcasecmp (function, "nth-child") != 0)
        {
          gtk_css_provider_error (scanner->provider,
                                  scanner,
                                  GTK_CSS_PROVIDER_ERROR,
                                  GTK_CSS_PROVIDER_ERROR_UNKNOWN_VALUE,
                                  "Unknown pseudo-class '%s(%s)'", function, name ? name : "");
          g_free (function);
          g_free (name);
          return FALSE;
        }
      
      g_free (function);
    
      if (name == NULL)
        {
          gtk_css_provider_error (scanner->provider,
                                  scanner,
                                  GTK_CSS_PROVIDER_ERROR,
                                  GTK_CSS_PROVIDER_ERROR_UNKNOWN_VALUE,
                                  "nth-child() requires an argument");
          return FALSE;
        }

      classes = nth_child_classes;
    }
  else
    classes = pseudo_classes;

  for (i = 0; classes[i].name != NULL; i++)
    {
      if (g_ascii_strcasecmp (name, classes[i].name) == 0)
        {
          if ((*region_to_modify & classes[i].region_flag) ||
              (*state_to_modify & classes[i].state_flag))
            {
              if (classes == nth_child_classes)
                gtk_css_provider_error (scanner->provider,
                                        scanner,
                                        GTK_CSS_PROVIDER_ERROR,
                                        GTK_CSS_PROVIDER_ERROR_SYNTAX,
                                        "Duplicate pseudo-class 'nth-child(%s)'", name);
              else
                gtk_css_provider_error (scanner->provider,
                                        scanner,
                                        GTK_CSS_PROVIDER_ERROR,
                                        GTK_CSS_PROVIDER_ERROR_SYNTAX,
                                        "Duplicate pseudo-class '%s'", name);
            }
          *region_to_modify |= classes[i].region_flag;
          *state_to_modify |= classes[i].state_flag;

          g_free (name);
          return TRUE;
        }
    }

  if (classes == nth_child_classes)
    gtk_css_provider_error (scanner->provider,
                            scanner,
                            GTK_CSS_PROVIDER_ERROR,
                            GTK_CSS_PROVIDER_ERROR_UNKNOWN_VALUE,
                            "Unknown pseudo-class 'nth-child(%s)'", name);
  else
    gtk_css_provider_error (scanner->provider,
                            scanner,
                            GTK_CSS_PROVIDER_ERROR,
                            GTK_CSS_PROVIDER_ERROR_UNKNOWN_VALUE,
                            "Unknown pseudo-class '%s'", name);
  g_free (name);
  return FALSE;
}

static gboolean
parse_simple_selector (GtkCssScanner *scanner,
                       char **name,
                       GArray *ids,
                       GArray *classes,
                       GtkRegionFlags *pseudo_classes,
                       GtkStateFlags *state)
{
  gboolean parsed_something;
  
  *name = _gtk_css_parser_try_ident (scanner->parser, FALSE);
  if (*name)
    parsed_something = TRUE;
  else
    parsed_something = _gtk_css_parser_try (scanner->parser, "*", FALSE);

  do {
      if (_gtk_css_parser_try (scanner->parser, "#", FALSE))
        {
          if (!parse_selector_name (scanner, ids))
            return FALSE;
        }
      else if (_gtk_css_parser_try (scanner->parser, ".", FALSE))
        {
          if (!parse_selector_class (scanner, classes))
            return FALSE;
        }
      else if (_gtk_css_parser_try (scanner->parser, ":", FALSE))
        {
          if (!parse_selector_pseudo_class (scanner, pseudo_classes, state))
            return FALSE;
        }
      else if (!parsed_something)
        {
          gtk_css_provider_error_literal (scanner->provider,
                                          scanner,
                                          GTK_CSS_PROVIDER_ERROR,
                                          GTK_CSS_PROVIDER_ERROR_SYNTAX,
                                          "Expected a valid selector");
          return FALSE;
        }
      else
        break;

      parsed_something = TRUE;
    }
  while (!_gtk_css_parser_is_eof (scanner->parser));

  _gtk_css_parser_skip_whitespace (scanner->parser);
  return TRUE;
}

static GtkCssSelector *
parse_selector (GtkCssScanner *scanner)
{
  GtkCssSelector *selector = NULL;

  do {
      char *name = NULL;
      GArray *ids = g_array_new (TRUE, FALSE, sizeof (GQuark));
      GArray *classes = g_array_new (TRUE, FALSE, sizeof (GQuark));
      GtkRegionFlags pseudo_classes = 0;
      GtkStateFlags state = 0;
      GtkCssCombinator combine = GTK_CSS_COMBINE_DESCANDANT;

      if (selector)
        {
          if (_gtk_css_parser_try (scanner->parser, ">", TRUE))
            combine = GTK_CSS_COMBINE_CHILD;
        }

      if (!parse_simple_selector (scanner, &name, ids, classes, &pseudo_classes, &state))
        {
          g_array_free (ids, TRUE);
          g_array_free (classes, TRUE);
          if (selector)
            _gtk_css_selector_free (selector);
          return NULL;
        }

      selector = _gtk_css_selector_new (selector,
                                        combine,
                                        name,
                                        (GQuark *) g_array_free (ids, ids->len == 0),
                                        (GQuark *) g_array_free (classes, classes->len == 0),
                                        pseudo_classes,
                                        state);
      g_free (name);
    }
  while (!_gtk_css_parser_is_eof (scanner->parser) &&
         !_gtk_css_parser_begins_with (scanner->parser, ',') &&
         !_gtk_css_parser_begins_with (scanner->parser, '{'));

  return selector;
}

static GSList *
parse_selector_list (GtkCssScanner *scanner)
{
  GSList *selectors = NULL;

  gtk_css_scanner_push_section (scanner, GTK_CSS_SECTION_SELECTOR);

  do {
      GtkCssSelector *select = parse_selector (scanner);

      if (select == NULL)
        {
          g_slist_free_full (selectors, (GDestroyNotify) _gtk_css_selector_free);
          _gtk_css_parser_resync (scanner->parser, FALSE, 0);
          gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_SELECTOR);
          return NULL;
        }

      selectors = g_slist_prepend (selectors, select);
    }
  while (_gtk_css_parser_try (scanner->parser, ",", TRUE));

  gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_SELECTOR);

  return selectors;
}

static void
parse_declaration (GtkCssScanner *scanner,
                   GtkCssRuleset *ruleset)
{
  const GtkStyleProperty *property;
  char *name;

  gtk_css_scanner_push_section (scanner, GTK_CSS_SECTION_DECLARATION);

  name = _gtk_css_parser_try_ident (scanner->parser, TRUE);
  if (name == NULL)
    goto check_for_semicolon;

  property = _gtk_style_property_lookup (name);
  if (property == NULL && name[0] != '-')
    {
      gtk_css_provider_error (scanner->provider,
                              scanner,
                              GTK_CSS_PROVIDER_ERROR,
                              GTK_CSS_PROVIDER_ERROR_NAME,
                              "'%s' is not a valid property name",
                              name);
      _gtk_css_parser_resync (scanner->parser, TRUE, '}');
      g_free (name);
      gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_DECLARATION);
      return;
    }

  if (!_gtk_css_parser_try (scanner->parser, ":", TRUE))
    {
      gtk_css_provider_invalid_token (scanner->provider, scanner, "':'");
      _gtk_css_parser_resync (scanner->parser, TRUE, '}');
      g_free (name);
      gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_DECLARATION);
      return;
    }

  if (property)
    {
      PropertyValue *val;

      g_free (name);

      gtk_css_scanner_push_section (scanner, GTK_CSS_SECTION_VALUE);

      val = property_value_new (scanner->section);
      g_value_init (&val->value, property->pspec->value_type);

      if (_gtk_style_property_parse_value (property,
                                           &val->value,
                                           scanner->parser,
                                           gtk_css_scanner_get_base_url (scanner)))
        {
          if (_gtk_css_parser_begins_with (scanner->parser, ';') ||
              _gtk_css_parser_begins_with (scanner->parser, '}') ||
              _gtk_css_parser_is_eof (scanner->parser))
            {
              gtk_css_ruleset_add (ruleset, property, val);
            }
          else
            {
              gtk_css_provider_error_literal (scanner->provider,
                                              scanner,
                                              GTK_CSS_PROVIDER_ERROR,
                                              GTK_CSS_PROVIDER_ERROR_SYNTAX,
                                              "Junk at end of value");
              _gtk_css_parser_resync (scanner->parser, TRUE, '}');
              property_value_free (val);
              gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_VALUE);
              gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_DECLARATION);
              return;
            }
        }
      else
        {
          property_value_free (val);
          _gtk_css_parser_resync (scanner->parser, TRUE, '}');
          gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_VALUE);
          gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_DECLARATION);
          return;
        }
      gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_VALUE);
    }
  else if (name[0] == '-')
    {
      char *value_str;

      gtk_css_scanner_push_section (scanner, GTK_CSS_SECTION_VALUE);

      value_str = _gtk_css_parser_read_value (scanner->parser);
      if (value_str)
        {
          PropertyValue *val;

          val = property_value_new (scanner->section);
          g_value_init (&val->value, G_TYPE_STRING);
          g_value_take_string (&val->value, value_str);

          gtk_css_ruleset_add_style (ruleset, name, val);
        }
      else
        {
          _gtk_css_parser_resync (scanner->parser, TRUE, '}');
          gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_VALUE);
          gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_DECLARATION);
          return;
        }

      gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_VALUE);
    }
  else
    g_free (name);

check_for_semicolon:
  gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_DECLARATION);

  if (!_gtk_css_parser_try (scanner->parser, ";", TRUE))
    {
      if (!_gtk_css_parser_begins_with (scanner->parser, '}') &&
          !_gtk_css_parser_is_eof (scanner->parser))
        {
          gtk_css_provider_error_literal (scanner->provider,
                                          scanner,
                                          GTK_CSS_PROVIDER_ERROR,
                                          GTK_CSS_PROVIDER_ERROR_SYNTAX,
                                          "Expected semicolon");
          _gtk_css_parser_resync (scanner->parser, TRUE, '}');
        }
    }
}

static void
parse_declarations (GtkCssScanner *scanner,
                    GtkCssRuleset *ruleset)
{
  while (!_gtk_css_parser_is_eof (scanner->parser) &&
         !_gtk_css_parser_begins_with (scanner->parser, '}'))
    {
      parse_declaration (scanner, ruleset);
    }
}

static void
parse_ruleset (GtkCssScanner *scanner)
{
  GSList *selectors;
  GtkCssRuleset ruleset = { 0, };

  gtk_css_scanner_push_section (scanner, GTK_CSS_SECTION_RULESET);

  selectors = parse_selector_list (scanner);
  if (selectors == NULL)
    {
      gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_RULESET);
      return;
    }

  if (!_gtk_css_parser_try (scanner->parser, "{", TRUE))
    {
      gtk_css_provider_error_literal (scanner->provider,
                                      scanner,
                                      GTK_CSS_PROVIDER_ERROR,
                                      GTK_CSS_PROVIDER_ERROR_SYNTAX,
                                      "expected '{' after selectors");
      _gtk_css_parser_resync (scanner->parser, FALSE, 0);
      g_slist_free_full (selectors, (GDestroyNotify) _gtk_css_selector_free);
      gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_RULESET);
      return;
    }

  parse_declarations (scanner, &ruleset);

  if (!_gtk_css_parser_try (scanner->parser, "}", TRUE))
    {
      gtk_css_provider_error_literal (scanner->provider,
                                      scanner,
                                      GTK_CSS_PROVIDER_ERROR,
                                      GTK_CSS_PROVIDER_ERROR_SYNTAX,
                                      "expected '}' after declarations");
      if (!_gtk_css_parser_is_eof (scanner->parser))
        {
          _gtk_css_parser_resync (scanner->parser, FALSE, 0);
          g_slist_free_full (selectors, (GDestroyNotify) _gtk_css_selector_free);
          gtk_css_ruleset_clear (&ruleset);
          gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_RULESET);
        }
    }

  css_provider_commit (scanner->provider, selectors, &ruleset);
  gtk_css_ruleset_clear (&ruleset);
  gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_RULESET);
}

static void
parse_statement (GtkCssScanner *scanner)
{
  if (_gtk_css_parser_begins_with (scanner->parser, '@'))
    parse_at_keyword (scanner);
  else
    parse_ruleset (scanner);
}

static void
parse_stylesheet (GtkCssScanner *scanner)
{
  gtk_css_scanner_push_section (scanner, GTK_CSS_SECTION_DOCUMENT);

  _gtk_css_parser_skip_whitespace (scanner->parser);

  while (!_gtk_css_parser_is_eof (scanner->parser))
    {
      if (_gtk_css_parser_try (scanner->parser, "<!--", TRUE) ||
          _gtk_css_parser_try (scanner->parser, "-->", TRUE))
        continue;

      parse_statement (scanner);
    }

  gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_DOCUMENT);
}

static int
gtk_css_provider_compare_rule (gconstpointer a_,
                               gconstpointer b_)
{
  const GtkCssRuleset *a = (const GtkCssRuleset *) a_;
  const GtkCssRuleset *b = (const GtkCssRuleset *) b_;
  int compare;

  compare = _gtk_css_selector_compare (a->selector, b->selector);
  if (compare != 0)
    return compare;

  /* compare pointers in array to ensure a stable sort */
  if (a_ < b_)
    return -1;

  if (a_ > b_)
    return 1;

  return 0;
}

static void
gtk_css_provider_postprocess (GtkCssProvider *css_provider)
{
  GtkCssProviderPrivate *priv = css_provider->priv;

  g_array_sort (priv->rulesets, gtk_css_provider_compare_rule);
}

static gboolean
gtk_css_provider_load_internal (GtkCssProvider *css_provider,
                                GtkCssScanner  *parent,
                                GFile          *file,
                                const char     *text,
                                GError        **error)
{
  GtkCssScanner *scanner;
  gulong error_handler;
  char *free_data = NULL;

  if (error)
    error_handler = g_signal_connect (css_provider,
                                      "parsing-error",
                                      G_CALLBACK (gtk_css_provider_propagate_error),
                                      error);
  else
    error_handler = 0; /* silence gcc */

  if (text == NULL)
    {
      GError *load_error = NULL;

      if (g_file_load_contents (file, NULL,
                                &free_data, NULL,
                                NULL, &load_error))
        {
          text = free_data;
        }
      else
        {
          GtkCssSection *section;
          
          if (parent)
            section = gtk_css_section_ref (parent->section);
          else
            section = _gtk_css_section_new_for_file (GTK_CSS_SECTION_DOCUMENT, file);

          gtk_css_provider_error (css_provider,
                                  parent,
                                  GTK_CSS_PROVIDER_ERROR,
                                  GTK_CSS_PROVIDER_ERROR_IMPORT,
                                  "Failed to import: %s",
                                  load_error->message);

          gtk_css_section_unref (section);
        }
    }

  if (text)
    {
      scanner = gtk_css_scanner_new (css_provider,
                                     parent,
                                     parent ? parent->section : NULL,
                                     file,
                                     text);

      parse_stylesheet (scanner);

      gtk_css_scanner_destroy (scanner);

      if (parent == NULL)
        gtk_css_provider_postprocess (css_provider);
    }

  g_free (free_data);

  if (error)
    {
      g_signal_handler_disconnect (css_provider, error_handler);

      if (*error)
        {
          /* We clear all contents from the provider for backwards compat reasons */
          gtk_css_provider_reset (css_provider);
          return FALSE;
        }
    }

  return TRUE;
}

/**
 * gtk_css_provider_load_from_data:
 * @css_provider: a #GtkCssProvider
 * @data: (array length=length) (element-type guint8): CSS data loaded in memory
 * @length: the length of @data in bytes, or -1 for NUL terminated strings. If
 *   @length is not -1, the code will assume it is not NUL terminated and will
 *   potentially do a copy.
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
  char *free_data;
  gboolean ret;

  g_return_val_if_fail (GTK_IS_CSS_PROVIDER (css_provider), FALSE);
  g_return_val_if_fail (data != NULL, FALSE);

  if (length < 0)
    {
      length = strlen (data);
      free_data = NULL;
    }
  else
    {
      free_data = g_strndup (data, length);
      data = free_data;
    }

  gtk_css_provider_reset (css_provider);

  ret = gtk_css_provider_load_internal (css_provider, NULL, NULL, data, error);

  g_free (free_data);

  return ret;
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
  g_return_val_if_fail (GTK_IS_CSS_PROVIDER (css_provider), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  gtk_css_provider_reset (css_provider);

  return gtk_css_provider_load_internal (css_provider, NULL, file, NULL, error);
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
  GFile *file;
  gboolean result;

  g_return_val_if_fail (GTK_IS_CSS_PROVIDER (css_provider), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  file = g_file_new_for_path (path);
  
  result = gtk_css_provider_load_from_file (css_provider, file, error);

  g_object_unref (file);

  return result;
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
        "@define-color placeholder_text_color #808080; \n"
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
        ".expander:prelight,\n"
        "GtkTreeView.view.expander:selected:prelight {\n"
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
        ".tooltip,\n"
        ".tooltip * {\n"
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
        ".menuitem.check, .menuitem.radio {\n"
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
        ".menuitem:hover,\n"
        ".menuitem.check:hover,\n"
        ".menuitem.radio:hover {\n"
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
          provider = gtk_css_provider_new ();

          if (!gtk_css_provider_load_from_path (provider, path, NULL))
            {
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

static int
compare_properties (gconstpointer a, gconstpointer b)
{
  return strcmp (((const GtkStyleProperty *) a)->pspec->name,
                 ((const GtkStyleProperty *) b)->pspec->name);
}

static void
gtk_css_ruleset_print (const GtkCssRuleset *ruleset,
                       GString             *str)
{
  GList *keys, *walk;

  _gtk_css_selector_print (ruleset->selector, str);

  g_string_append (str, " {\n");

  if (ruleset->style)
    {
      keys = g_hash_table_get_keys (ruleset->style);
      /* so the output is identical for identical selector styles */
      keys = g_list_sort (keys, compare_properties);

      for (walk = keys; walk; walk = walk->next)
        {
          GtkStyleProperty *prop = walk->data;
          const PropertyValue *value = g_hash_table_lookup (ruleset->style, prop);

          g_string_append (str, "  ");
          g_string_append (str, prop->pspec->name);
          g_string_append (str, ": ");
          _gtk_style_property_print_value (prop, &value->value, str);
          g_string_append (str, ";\n");
        }

      g_list_free (keys);
    }

  if (ruleset->widget_style)
    {
      keys = g_hash_table_get_keys (ruleset->widget_style);
      /* so the output is identical for identical selector styles */
      keys = g_list_sort (keys, (GCompareFunc) strcmp);

      for (walk = keys; walk; walk = walk->next)
        {
          const char *name = walk->data;
          const PropertyValue *value = g_hash_table_lookup (ruleset->widget_style, (gpointer) name);

          g_string_append (str, "  ");
          g_string_append (str, name);
          g_string_append (str, ": ");
          g_string_append (str, g_value_get_string (&value->value));
          g_string_append (str, ";\n");
        }

      g_list_free (keys);
    }

  g_string_append (str, "}\n");
}

static void
gtk_css_provider_print_colors (GHashTable *colors,
                               GString    *str)
{
  GList *keys, *walk;
  char *s;

  keys = g_hash_table_get_keys (colors);
  /* so the output is identical for identical styles */
  keys = g_list_sort (keys, (GCompareFunc) strcmp);

  for (walk = keys; walk; walk = walk->next)
    {
      const char *name = walk->data;
      GtkSymbolicColor *symbolic = g_hash_table_lookup (colors, (gpointer) name);

      g_string_append (str, "@define-color ");
      g_string_append (str, name);
      g_string_append (str, " ");
      s = gtk_symbolic_color_to_string (symbolic);
      g_string_append (str, s);
      g_free (s);
      g_string_append (str, ";\n");
    }

  g_list_free (keys);
}

/**
 * gtk_css_provider_to_string:
 * @provider: the provider to write to a string
 *
 * Convertes the @provider into a string representation in CSS
 * format.
 * 
 * Using gtk_css_provider_load_from_data() with the return value
 * from this function on a new provider created with
 * gtk_css_provider_new() will basicallu create a duplicate of
 * this @provider.
 *
 * Returns: a new string representing the @provider.
 **/
char *
gtk_css_provider_to_string (GtkCssProvider *provider)
{
  GtkCssProviderPrivate *priv;
  GString *str;
  guint i;

  g_return_val_if_fail (GTK_IS_CSS_PROVIDER (provider), NULL);

  priv = provider->priv;

  str = g_string_new ("");

  gtk_css_provider_print_colors (priv->symbolic_colors, str);

  for (i = 0; i < priv->rulesets->len; i++)
    {
      if (i > 0)
        g_string_append (str, "\n");
      gtk_css_ruleset_print (&g_array_index (priv->rulesets, GtkCssRuleset, i), str);
    }

  return g_string_free (str, FALSE);
}

