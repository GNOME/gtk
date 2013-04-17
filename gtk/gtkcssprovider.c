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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cairo-gobject.h>

#include "gtkcssproviderprivate.h"

#include "gtkbitmaskprivate.h"
#include "gtkcssarrayvalueprivate.h"
#include "gtkcsscolorvalueprivate.h"
#include "gtkcsskeyframesprivate.h"
#include "gtkcssparserprivate.h"
#include "gtkcsssectionprivate.h"
#include "gtkcssselectorprivate.h"
#include "gtkcssshorthandpropertyprivate.h"
#include "gtkcssstylefuncsprivate.h"
#include "gtkstyleprovider.h"
#include "gtkstylecontextprivate.h"
#include "gtkstylepropertiesprivate.h"
#include "gtkstylepropertyprivate.h"
#include "gtkstyleproviderprivate.h"
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
 *       <row>
 *         <entry>alpha(@color, @f)</entry>
 *         <entry>Modifies passed color's alpha by a factor @f. @f is a
 *                floating point number. @f < 1.0 results in a more transparent
 *                color while @f > 1.0 results in a more opaque color.
 *         </entry>
 *         <entry>alhpa(blue, 0.5)</entry>
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
 * To set a shadow on an icon, use the icon-shadow property instead,
 * with the same syntax.
 * </para>
 * <para>
 * To set multiple shadows on an element, you can specify a comma-separated list
 * of shadow elements in the text-shadow or icon-shadow property. Shadows are
 * always rendered front-back, i.e. the first shadow specified is on top of the
 * others. Shadows can thus overlay each other, but they can never overlay the
 * text or icon itself, which is always rendered on top of the shadow layer.
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
 *         <entry>background-repeat</entry>
 *         <entry>[repeat|no-repeat]</entry>
 *         <entry>internal</entry>
 *         <entry><literallayout>background-repeat: no-repeat;</literallayout>
 *                If not specified, the style doesn't respect the CSS3
 *                specification, since the background will be
 *                stretched to fill the area.
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
 *         <entry>internal use only</entry>
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
typedef struct _PropertyValue PropertyValue;
typedef struct _WidgetPropertyValue WidgetPropertyValue;
typedef enum ParserScope ParserScope;
typedef enum ParserSymbol ParserSymbol;

struct _PropertyValue {
  GtkCssStyleProperty *property;
  GtkCssValue         *value;
  GtkCssSection       *section;
};

struct _WidgetPropertyValue {
  WidgetPropertyValue *next;
  char *name;
  char *value;

  GtkCssSection *section;
};

struct GtkCssRuleset
{
  GtkCssSelector *selector;
  GtkCssSelectorTree *selector_match;
  WidgetPropertyValue *widget_style;
  PropertyValue *styles;
  GtkBitmask *set_styles;
  guint n_styles;
  guint owns_styles : 1;
  guint owns_widget_style : 1;
};

struct _GtkCssScanner
{
  GtkCssProvider *provider;
  GtkCssParser *parser;
  GtkCssSection *section;
  GtkCssScanner *parent;
  GSList *state;
};

struct _GtkCssProviderPrivate
{
  GScanner *scanner;

  GHashTable *symbolic_colors;
  GHashTable *keyframes;

  GArray *rulesets;
  GtkCssSelectorTree *tree;
  GResource *resource;
};

enum {
  PARSING_ERROR,
  LAST_SIGNAL
};

static gboolean gtk_keep_css_sections = FALSE;

static guint css_provider_signals[LAST_SIGNAL] = { 0 };

static void gtk_css_provider_finalize (GObject *object);
static void gtk_css_style_provider_iface_init (GtkStyleProviderIface *iface);
static void gtk_css_style_provider_private_iface_init (GtkStyleProviderPrivateInterface *iface);
static void widget_property_value_list_free (WidgetPropertyValue *head);

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
                                               gtk_css_style_provider_iface_init)
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_STYLE_PROVIDER_PRIVATE,
                                               gtk_css_style_provider_private_iface_init));

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
      char *s = _gtk_css_section_to_string (section);

      g_warning ("Theme parsing error: %s: %s",
                 s,
                 error->message);

      g_free (s);
    }
}

static void
gtk_css_provider_class_init (GtkCssProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  if (g_getenv ("GTK_CSS_DEBUG"))
    gtk_keep_css_sections = TRUE;

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
                           GtkCssRuleset       *ruleset,
                           GtkCssSelector      *selector)
{
  memcpy (new, ruleset, sizeof (GtkCssRuleset));

  new->selector = selector;
  /* First copy takes over ownership */
  if (ruleset->owns_styles)
    ruleset->owns_styles = FALSE;
  if (ruleset->owns_widget_style)
    ruleset->owns_widget_style = FALSE;
  if (new->set_styles)
    new->set_styles = _gtk_bitmask_copy (new->set_styles);
}

static void
gtk_css_ruleset_clear (GtkCssRuleset *ruleset)
{
  if (ruleset->owns_styles)
    {
      guint i;

      for (i = 0; i < ruleset->n_styles; i++)
        {
          _gtk_css_value_unref (ruleset->styles[i].value);
	  ruleset->styles[i].value = NULL;
	  if (ruleset->styles[i].section)
	    gtk_css_section_unref (ruleset->styles[i].section);
        }
      g_free (ruleset->styles);
    }
  if (ruleset->set_styles)
    _gtk_bitmask_free (ruleset->set_styles);
  if (ruleset->owns_widget_style)
    widget_property_value_list_free (ruleset->widget_style);
  if (ruleset->selector)
    _gtk_css_selector_free (ruleset->selector);

  memset (ruleset, 0, sizeof (GtkCssRuleset));
}

static WidgetPropertyValue *
widget_property_value_new (char *name, GtkCssSection *section)
{
  WidgetPropertyValue *value;

  value = g_slice_new0 (WidgetPropertyValue);

  value->name = name;
  if (gtk_keep_css_sections)
    value->section = gtk_css_section_ref (section);

  return value;
}

static void
widget_property_value_free (WidgetPropertyValue *value)
{
  g_free (value->value);
  g_free (value->name);
  if (value->section)
    gtk_css_section_unref (value->section);

  g_slice_free (WidgetPropertyValue, value);
}

static void
widget_property_value_list_free (WidgetPropertyValue *head)
{
  WidgetPropertyValue *l, *next;
  for (l = head; l != NULL; l = next)
    {
      next = l->next;
      widget_property_value_free (l);
    }
}

static WidgetPropertyValue *
widget_property_value_list_remove_name (WidgetPropertyValue *head, const char *name)
{
  WidgetPropertyValue *l, **last;

  last = &head;

  for (l = head; l != NULL; l = l->next)
    {
      if (strcmp (l->name, name) == 0)
	{
	  *last = l->next;
	  widget_property_value_free (l);
	  break;
	}

      last = &l->next;
    }

  return head;
}

static void
gtk_css_ruleset_add_style (GtkCssRuleset *ruleset,
                           char          *name,
                           WidgetPropertyValue *value)
{
  value->next = widget_property_value_list_remove_name (ruleset->widget_style, name);
  ruleset->widget_style = value;
  ruleset->owns_widget_style = TRUE;
}

static void
gtk_css_ruleset_add (GtkCssRuleset       *ruleset,
                     GtkCssStyleProperty *property,
                     GtkCssValue         *value,
                     GtkCssSection       *section)
{
  guint i;

  g_return_if_fail (ruleset->owns_styles || ruleset->n_styles == 0);

  if (ruleset->set_styles == NULL)
    ruleset->set_styles = _gtk_bitmask_new ();

  ruleset->set_styles = _gtk_bitmask_set (ruleset->set_styles,
                                          _gtk_css_style_property_get_id (property),
                                          TRUE);

  ruleset->owns_styles = TRUE;

  for (i = 0; i < ruleset->n_styles; i++)
    {
      if (ruleset->styles[i].property == property)
        {
          _gtk_css_value_unref (ruleset->styles[i].value);
	  ruleset->styles[i].value = NULL;
	  if (ruleset->styles[i].section)
	    gtk_css_section_unref (ruleset->styles[i].section);
          break;
        }
    }
  if (i == ruleset->n_styles)
    {
      ruleset->n_styles++;
      ruleset->styles = g_realloc (ruleset->styles, ruleset->n_styles * sizeof (PropertyValue));
      ruleset->styles[i].value = NULL;
      ruleset->styles[i].property = property;
    }

  ruleset->styles[i].value = value;
  if (gtk_keep_css_sections)
    ruleset->styles[i].section = gtk_css_section_ref (section);
  else
    ruleset->styles[i].section = NULL;
}

static void
gtk_css_scanner_destroy (GtkCssScanner *scanner)
{
  if (scanner->section)
    gtk_css_section_unref (scanner->section);
  g_object_unref (scanner->provider);
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

  scanner->parser = _gtk_css_parser_new (text,
                                         file,
                                         gtk_css_scanner_parser_error,
                                         scanner);

  return scanner;
}

static gboolean
gtk_css_scanner_would_recurse (GtkCssScanner *scanner,
                               GFile         *file)
{
  while (scanner)
    {
      GFile *parser_file = _gtk_css_parser_get_file (scanner->parser);
      if (parser_file && g_file_equal (parser_file, file))
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
                                  scanner->parser);

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
                                                 (GDestroyNotify) _gtk_css_value_unref);
  priv->keyframes = g_hash_table_new_full (g_str_hash, g_str_equal,
                                           (GDestroyNotify) g_free,
                                           (GDestroyNotify) _gtk_css_value_unref);
}

static void
verify_tree_match_results (GtkCssProvider *provider,
			   const GtkCssMatcher *matcher,
			   GPtrArray *tree_rules)
{
#ifdef VERIFY_TREE
  GtkCssProviderPrivate *priv = provider->priv;
  GtkCssRuleset *ruleset;
  gboolean should_match;
  int i, j;

  for (i = 0; i < priv->rulesets->len; i++)
    {
      gboolean found = FALSE;

      ruleset = &g_array_index (priv->rulesets, GtkCssRuleset, i);

      for (j = 0; j < tree_rules->len; j++)
	{
	  if (ruleset == tree_rules->pdata[j])
	    {
	      found = TRUE;
	      break;
	    }
	}
      should_match = _gtk_css_selector_matches (ruleset->selector, matcher);
      if (found != !!should_match)
	{
	  g_error ("expected rule '%s' to %s, but it %s\n",
		   _gtk_css_selector_to_string (ruleset->selector),
		   should_match ? "match" : "not match",
		   found ? "matched" : "didn't match");
	}
    }
#endif
}

static void
verify_tree_get_change_results (GtkCssProvider *provider,
				const GtkCssMatcher *matcher,
				GtkCssChange change)
{
#ifdef VERIFY_TREE
  {
    GtkCssChange verify_change = 0;
    GPtrArray *tree_rules;
    int i;

    tree_rules = _gtk_css_selector_tree_match_all (provider->priv->tree, matcher);
    verify_tree_match_results (provider, matcher, tree_rules);

    for (i = tree_rules->len - 1; i >= 0; i--)
      {
	GtkCssRuleset *ruleset;

	ruleset = tree_rules->pdata[i];

	verify_change |= _gtk_css_selector_tree_match_get_change (ruleset->selector_match);
      }

    if (change != verify_change)
      {
	GString *s;

	s = g_string_new ("");
	g_string_append_printf (s, "expected change 0x%x, but it was 0x%x", verify_change, change);
	if ((change & ~verify_change) != 0)
	  g_string_append_printf (s, ", unexpectedly set: 0x%x", change & ~verify_change);
	if ((~change & verify_change) != 0)
	  g_string_append_printf (s, ", unexpectedly no set: 0x%x",  ~change & verify_change);
	g_warning (s->str);
	g_string_free (s, TRUE);
      }

    g_ptr_array_free (tree_rules, TRUE);
  }
#endif
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
  WidgetPropertyValue *val;
  GPtrArray *tree_rules;
  GtkCssMatcher matcher;
  gboolean found = FALSE;
  gchar *prop_name;
  gint i;

  if (!_gtk_css_matcher_init (&matcher, path, state))
    return FALSE;

  tree_rules = _gtk_css_selector_tree_match_all (priv->tree, &matcher);
  verify_tree_match_results (css_provider, &matcher, tree_rules);

  prop_name = g_strdup_printf ("-%s-%s",
                               g_type_name (pspec->owner_type),
                               pspec->name);

  for (i = tree_rules->len - 1; i >= 0; i--)
    {
      GtkCssRuleset *ruleset = tree_rules->pdata[i];

      if (ruleset->widget_style == NULL)
        continue;

      for (val = ruleset->widget_style; val != NULL; val = val->next)
	{
	  if (strcmp (val->name, prop_name) == 0)
	    {
	      GtkCssScanner *scanner;

	      scanner = gtk_css_scanner_new (css_provider,
					     NULL,
					     val->section,
					     val->section != NULL ? gtk_css_section_get_file (val->section) : NULL,
					     val->value);

	      found = _gtk_css_style_parse_value (value,
						  scanner->parser);

	      gtk_css_scanner_destroy (scanner);

	      break;
	    }
	}

      if (found)
	break;
    }

  g_free (prop_name);
  g_ptr_array_free (tree_rules, TRUE);

  return found;
}

static void
gtk_css_style_provider_iface_init (GtkStyleProviderIface *iface)
{
  iface->get_style_property = gtk_css_provider_get_style_property;
}

static GtkCssValue *
gtk_css_style_provider_get_color (GtkStyleProviderPrivate *provider,
                                  const char              *name)
{
  GtkCssProvider *css_provider = GTK_CSS_PROVIDER (provider);

  return g_hash_table_lookup (css_provider->priv->symbolic_colors, name);
}

static GtkCssKeyframes *
gtk_css_style_provider_get_keyframes (GtkStyleProviderPrivate *provider,
                                      const char              *name)
{
  GtkCssProvider *css_provider = GTK_CSS_PROVIDER (provider);

  return g_hash_table_lookup (css_provider->priv->keyframes, name);
}

static void
gtk_css_style_provider_lookup (GtkStyleProviderPrivate *provider,
                               const GtkCssMatcher     *matcher,
                               GtkCssLookup            *lookup)
{
  GtkCssProvider *css_provider;
  GtkCssProviderPrivate *priv;
  GtkCssRuleset *ruleset;
  guint j;
  int i;
  GPtrArray *tree_rules;

  css_provider = GTK_CSS_PROVIDER (provider);
  priv = css_provider->priv;

  tree_rules = _gtk_css_selector_tree_match_all (priv->tree, matcher);
  verify_tree_match_results (css_provider, matcher, tree_rules);

  for (i = tree_rules->len - 1; i >= 0; i--)
    {
      ruleset = tree_rules->pdata[i];

      if (ruleset->styles == NULL)
        continue;

      if (!_gtk_bitmask_intersects (_gtk_css_lookup_get_missing (lookup),
                                    ruleset->set_styles))
        continue;

      for (j = 0; j < ruleset->n_styles; j++)
        {
          GtkCssStyleProperty *prop = ruleset->styles[j].property;
          guint id = _gtk_css_style_property_get_id (prop);

          if (!_gtk_css_lookup_is_missing (lookup, id))
            continue;

          _gtk_css_lookup_set (lookup,
                               id,
                               ruleset->styles[j].section,
                               ruleset->styles[j].value);
        }

      if (_gtk_bitmask_is_empty (_gtk_css_lookup_get_missing (lookup)))
        break;
    }

  g_ptr_array_free (tree_rules, TRUE);
}

static GtkCssChange
gtk_css_style_provider_get_change (GtkStyleProviderPrivate *provider,
                                   const GtkCssMatcher     *matcher)
{
  GtkCssProvider *css_provider;
  GtkCssProviderPrivate *priv;
  GtkCssChange change;

  css_provider = GTK_CSS_PROVIDER (provider);
  priv = css_provider->priv;

  change = _gtk_css_selector_tree_get_change_all (priv->tree, matcher);

  verify_tree_get_change_results (css_provider, matcher, change);

  return change;
}

static void
gtk_css_style_provider_private_iface_init (GtkStyleProviderPrivateInterface *iface)
{
  iface->get_color = gtk_css_style_provider_get_color;
  iface->get_keyframes = gtk_css_style_provider_get_keyframes;
  iface->lookup = gtk_css_style_provider_lookup;
  iface->get_change = gtk_css_style_provider_get_change;
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
  _gtk_css_selector_tree_free (priv->tree);

  g_hash_table_destroy (priv->symbolic_colors);
  g_hash_table_destroy (priv->keyframes);

  if (priv->resource)
    {
      g_resources_unregister (priv->resource);
      g_resource_unref (priv->resource);
      priv->resource = NULL;
    }

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

  if (ruleset->styles == NULL && ruleset->widget_style == NULL)
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

  if (priv->resource)
    {
      g_resources_unregister (priv->resource);
      g_resource_unref (priv->resource);
      priv->resource = NULL;
    }

  g_hash_table_remove_all (priv->symbolic_colors);
  g_hash_table_remove_all (priv->keyframes);

  for (i = 0; i < priv->rulesets->len; i++)
    gtk_css_ruleset_clear (&g_array_index (priv->rulesets, GtkCssRuleset, i));
  g_array_set_size (priv->rulesets, 0);
  _gtk_css_selector_tree_free (priv->tree);
  priv->tree = NULL;

}

static void
gtk_css_provider_propagate_error (GtkCssProvider  *provider,
                                  GtkCssSection   *section,
                                  const GError    *error,
                                  GError         **propagate_to)
{

  char *s;

  /* don't fail for deprecations */
  if (g_error_matches (error, GTK_CSS_PROVIDER_ERROR, GTK_CSS_PROVIDER_ERROR_DEPRECATED))
    {
      s = _gtk_css_section_to_string (section);
      g_warning ("Theme parsing error: %s: %s", s, error->message);
      g_free (s);
      return;
    }

  /* we already set an error. And we'd like to keep the first one */
  if (*propagate_to)
    return;

  *propagate_to = g_error_copy (error);
  if (section)
    {
      s = _gtk_css_section_to_string (section);
      g_prefix_error (propagate_to, "%s", s);
      g_free (s);
    }
}

static gboolean
parse_import (GtkCssScanner *scanner)
{
  GFile *file;

  gtk_css_scanner_push_section (scanner, GTK_CSS_SECTION_IMPORT);

  if (!_gtk_css_parser_try (scanner->parser, "@import", TRUE))
    {
      gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_IMPORT);
      return FALSE;
    }

  if (_gtk_css_parser_is_string (scanner->parser))
    {
      char *uri;

      uri = _gtk_css_parser_read_string (scanner->parser);
      file = _gtk_css_parser_get_file_for_path (scanner->parser, uri);
      g_free (uri);
    }
  else
    {
      file = _gtk_css_parser_read_url (scanner->parser);
    }

  if (file == NULL)
    {
      _gtk_css_parser_resync (scanner->parser, TRUE, 0);
      gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_IMPORT);
      return TRUE;
    }

  if (!_gtk_css_parser_try (scanner->parser, ";", FALSE))
    {
      gtk_css_provider_invalid_token (scanner->provider, scanner, "semicolon");
      _gtk_css_parser_resync (scanner->parser, TRUE, 0);
    }
  else if (gtk_css_scanner_would_recurse (scanner, file))
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

  g_object_unref (file);

  gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_IMPORT);
  _gtk_css_parser_skip_whitespace (scanner->parser);

  return TRUE;
}

static gboolean
parse_color_definition (GtkCssScanner *scanner)
{
  GtkCssValue *color;
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

  color = _gtk_css_color_value_parse (scanner->parser);
  if (color == NULL)
    {
      g_free (name);
      _gtk_css_parser_resync (scanner->parser, TRUE, 0);
      gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_COLOR_DEFINITION);
      return TRUE;
    }

  if (!_gtk_css_parser_try (scanner->parser, ";", TRUE))
    {
      g_free (name);
      _gtk_css_value_unref (color);
      gtk_css_provider_error_literal (scanner->provider,
                                      scanner,
                                      GTK_CSS_PROVIDER_ERROR,
                                      GTK_CSS_PROVIDER_ERROR_SYNTAX,
                                      "Missing semicolon at end of color definition");
      _gtk_css_parser_resync (scanner->parser, TRUE, 0);

      gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_COLOR_DEFINITION);
      return TRUE;
    }

  g_hash_table_insert (scanner->provider->priv->symbolic_colors, name, color);

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

static gboolean
parse_keyframes (GtkCssScanner *scanner)
{
  GtkCssKeyframes *keyframes;
  char *name;

  gtk_css_scanner_push_section (scanner, GTK_CSS_SECTION_KEYFRAMES);

  if (!_gtk_css_parser_try (scanner->parser, "@keyframes", TRUE))
    {
      gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_KEYFRAMES);
      return FALSE;
    }

  name = _gtk_css_parser_try_ident (scanner->parser, TRUE);
  if (name == NULL)
    {
      gtk_css_provider_error_literal (scanner->provider,
                                      scanner,
                                      GTK_CSS_PROVIDER_ERROR,
                                      GTK_CSS_PROVIDER_ERROR_SYNTAX,
                                      "Expected name for keyframes");
      _gtk_css_parser_resync (scanner->parser, TRUE, 0);
      goto exit;
    }

  if (!_gtk_css_parser_try (scanner->parser, "{", TRUE))
    {
      gtk_css_provider_error_literal (scanner->provider,
                                      scanner,
                                      GTK_CSS_PROVIDER_ERROR,
                                      GTK_CSS_PROVIDER_ERROR_SYNTAX,
                                      "Expected '{' for keyframes");
      _gtk_css_parser_resync (scanner->parser, TRUE, 0);
      g_free (name);
      goto exit;
    }

  keyframes = _gtk_css_keyframes_parse (scanner->parser);
  if (keyframes == NULL)
    {
      _gtk_css_parser_resync (scanner->parser, TRUE, '}');
      g_free (name);
      goto exit;
    }

  g_hash_table_insert (scanner->provider->priv->keyframes, name, keyframes);

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

exit:
  gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_KEYFRAMES);

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
  if (parse_keyframes (scanner))
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

static GSList *
parse_selector_list (GtkCssScanner *scanner)
{
  GSList *selectors = NULL;

  gtk_css_scanner_push_section (scanner, GTK_CSS_SECTION_SELECTOR);

  do {
      GtkCssSelector *select = _gtk_css_selector_parse (scanner->parser);

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
  GtkStyleProperty *property;
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
      GtkCssValue *value;

      g_free (name);

      gtk_css_scanner_push_section (scanner, GTK_CSS_SECTION_VALUE);

      value = _gtk_style_property_parse_value (property,
                                               scanner->parser);

      if (value == NULL)
        {
          _gtk_css_parser_resync (scanner->parser, TRUE, '}');
          gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_VALUE);
          gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_DECLARATION);
          return;
        }

      if (!_gtk_css_parser_begins_with (scanner->parser, ';') &&
          !_gtk_css_parser_begins_with (scanner->parser, '}') &&
          !_gtk_css_parser_is_eof (scanner->parser))
        {
          gtk_css_provider_error_literal (scanner->provider,
                                          scanner,
                                          GTK_CSS_PROVIDER_ERROR,
                                          GTK_CSS_PROVIDER_ERROR_SYNTAX,
                                          "Junk at end of value");
          _gtk_css_parser_resync (scanner->parser, TRUE, '}');
          gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_VALUE);
          gtk_css_scanner_pop_section (scanner, GTK_CSS_SECTION_DECLARATION);
          return;
        }

      if (GTK_IS_CSS_SHORTHAND_PROPERTY (property))
        {
          GtkCssShorthandProperty *shorthand = GTK_CSS_SHORTHAND_PROPERTY (property);
          guint i;

          for (i = 0; i < _gtk_css_shorthand_property_get_n_subproperties (shorthand); i++)
            {
              GtkCssStyleProperty *child = _gtk_css_shorthand_property_get_subproperty (shorthand, i);
              GtkCssValue *sub = _gtk_css_array_value_get_nth (value, i);
              
              gtk_css_ruleset_add (ruleset, child, _gtk_css_value_ref (sub), scanner->section);
            }
          
            _gtk_css_value_unref (value);
        }
      else if (GTK_IS_CSS_STYLE_PROPERTY (property))
        {
          gtk_css_ruleset_add (ruleset, GTK_CSS_STYLE_PROPERTY (property), value, scanner->section);
        }
      else
        {
          g_assert_not_reached ();
          _gtk_css_value_unref (value);
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
          WidgetPropertyValue *val;

          val = widget_property_value_new (name, scanner->section);
	  val->value = value_str;

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

  return 0;
}

static void
gtk_css_provider_postprocess (GtkCssProvider *css_provider)
{
  GtkCssProviderPrivate *priv = css_provider->priv;
  GtkCssSelectorTreeBuilder *builder;
  guint i;

  g_array_sort (priv->rulesets, gtk_css_provider_compare_rule);

  builder = _gtk_css_selector_tree_builder_new ();
  for (i = 0; i < priv->rulesets->len; i++)
    {
      GtkCssRuleset *ruleset;

      ruleset = &g_array_index (priv->rulesets, GtkCssRuleset, i);

      _gtk_css_selector_tree_builder_add (builder,
					  ruleset->selector,
					  &ruleset->selector_match,
					  ruleset);
    }

  priv->tree = _gtk_css_selector_tree_builder_build (builder);
  _gtk_css_selector_tree_builder_free (builder);

#ifndef VERIFY_TREE
  for (i = 0; i < priv->rulesets->len; i++)
    {
      GtkCssRuleset *ruleset;

      ruleset = &g_array_index (priv->rulesets, GtkCssRuleset, i);

      _gtk_css_selector_free (ruleset->selector);
      ruleset->selector = NULL;
    }
#endif
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
 * Returns: %TRUE. The return value is deprecated and %FALSE will only be
 *     returned for backwards compatibility reasons if an @error is not 
 *     %NULL and a loading error occured. To track errors while loading
 *     CSS, connect to the GtkCssProvider::parsing-error signal.
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

  _gtk_style_provider_private_changed (GTK_STYLE_PROVIDER_PRIVATE (css_provider));

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
 * Returns: %TRUE. The return value is deprecated and %FALSE will only be
 *     returned for backwards compatibility reasons if an @error is not 
 *     %NULL and a loading error occured. To track errors while loading
 *     CSS, connect to the GtkCssProvider::parsing-error signal.
 **/
gboolean
gtk_css_provider_load_from_file (GtkCssProvider  *css_provider,
                                 GFile           *file,
                                 GError         **error)
{
  gboolean success;

  g_return_val_if_fail (GTK_IS_CSS_PROVIDER (css_provider), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  gtk_css_provider_reset (css_provider);

  success = gtk_css_provider_load_internal (css_provider, NULL, file, NULL, error);

  _gtk_style_provider_private_changed (GTK_STYLE_PROVIDER_PRIVATE (css_provider));

  return success;
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
 * Returns: %TRUE. The return value is deprecated and %FALSE will only be
 *     returned for backwards compatibility reasons if an @error is not 
 *     %NULL and a loading error occured. To track errors while loading
 *     CSS, connect to the GtkCssProvider::parsing-error signal.
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

static void
gtk_css_provider_load_from_resource (GtkCssProvider  *css_provider,
			             const gchar     *resource_path)
{
  GFile *file;
  char *uri, *escaped;

  g_return_if_fail (GTK_IS_CSS_PROVIDER (css_provider));
  g_return_if_fail (resource_path != NULL);

  escaped = g_uri_escape_string (resource_path,
				 G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, FALSE);
  uri = g_strconcat ("resource://", escaped, NULL);
  g_free (escaped);

  file = g_file_new_for_uri (uri);
  g_free (uri);

  gtk_css_provider_load_from_file (css_provider, file, NULL);

  g_object_unref (file);
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
      provider = gtk_css_provider_new ();
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
    path = g_build_filename (_gtk_get_data_prefix (), "share", "themes", NULL);

  return path;
}

/**
 * _gtk_css_provider_load_named:
 * @provider: a #GtkCssProvider
 * @name: A theme name
 * @variant: (allow-none): variant to load, for example, "dark", or
 *     %NULL for the default
 *
 * Loads a theme from the usual theme paths. The actual process of
 * finding the theme might change between releases, but it is
 * guaranteed that this function uses the same mechanism to load the
 * theme than GTK uses for loading its own theme.
 **/
void
_gtk_css_provider_load_named (GtkCssProvider *provider,
                              const gchar    *name,
                              const gchar    *variant)
{
  gchar *subpath, *path;
  gchar *resource_path;

  g_return_if_fail (GTK_IS_CSS_PROVIDER (provider));
  g_return_if_fail (name != NULL);

  gtk_css_provider_reset (provider);

  /* try loading the resource for the theme. This is mostly meant for built-in
   * themes.
   */
  if (variant)
    resource_path = g_strdup_printf ("/org/gtk/libgtk/%s-%s.css", name, variant);
  else
    resource_path = g_strdup_printf ("/org/gtk/libgtk/%s.css", name);

  if (g_resources_get_info (resource_path, 0, NULL, NULL, NULL))
    {
      gtk_css_provider_load_from_resource (provider, resource_path);
      g_free (resource_path);
      return;
    }
  g_free (resource_path);


  /* Next try looking for files in the various theme directories.
   */
  if (variant)
    subpath = g_strdup_printf ("gtk-3.0" G_DIR_SEPARATOR_S "gtk-%s.css", variant);
  else
    subpath = g_strdup ("gtk-3.0" G_DIR_SEPARATOR_S "gtk.css");

  /* First look in the user's config directory
   */
  path = g_build_filename (g_get_user_data_dir (), "themes", name, subpath, NULL);
  if (!g_file_test (path, G_FILE_TEST_EXISTS))
    {
      g_free (path);
      /* Next look in the user's home directory
       */
      path = g_build_filename (g_get_home_dir (), ".themes", name, subpath, NULL);
      if (!g_file_test (path, G_FILE_TEST_EXISTS))
        {
          gchar *theme_dir;

          g_free (path);

          /* Finally, try in the default theme directory */
          theme_dir = _gtk_css_provider_get_theme_dir ();
          path = g_build_filename (theme_dir, name, subpath, NULL);
          g_free (theme_dir);

          if (!g_file_test (path, G_FILE_TEST_EXISTS))
            {
              g_free (path);
              path = NULL;
            }
        }
    }

  g_free (subpath);

  if (path)
    {
      char *dir, *resource_file;
      GResource *resource;

      dir = g_path_get_dirname (path);
      resource_file = g_build_filename (dir, "gtk.gresource", NULL);
      resource = g_resource_load (resource_file, NULL);
      g_free (resource_file);

      if (resource != NULL)
        g_resources_register (resource);

      gtk_css_provider_load_from_path (provider, path, NULL);

      /* Only set this after load, as load_from_path will clear it */
      provider->priv->resource = resource;

      g_free (path);
      g_free (dir);
    }
  else
    {
      /* Things failed! Fall back! Fall back! */

      if (variant)
        {
          /* If there was a variant, try without */
          _gtk_css_provider_load_named (provider, name, NULL);
        }
      else
        {
          /* Worst case, fall back to Raleigh */
          g_return_if_fail (!g_str_equal (name, "Raleigh")); /* infloop protection */
          _gtk_css_provider_load_named (provider, "Raleigh", NULL);
        }
    }
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

  if (variant == NULL)
    key = g_strdup (name);
  else
    key = g_strconcat (name, "-", variant, NULL);
  if (G_UNLIKELY (!themes))
    themes = g_hash_table_new (g_str_hash, g_str_equal);

  provider = g_hash_table_lookup (themes, key);
  
  if (!provider)
    {
      provider = gtk_css_provider_new ();
      _gtk_css_provider_load_named (provider, name, variant);
      g_hash_table_insert (themes, g_strdup (key), provider);
    }
  
  g_free (key);

  return provider;
}

static int
compare_properties (gconstpointer a, gconstpointer b, gpointer style)
{
  const guint *ua = a;
  const guint *ub = b;
  PropertyValue *styles = style;

  return strcmp (_gtk_style_property_get_name (GTK_STYLE_PROPERTY (styles[*ua].property)),
                 _gtk_style_property_get_name (GTK_STYLE_PROPERTY (styles[*ub].property)));
}

static int
compare_names (gconstpointer a, gconstpointer b)
{
  const WidgetPropertyValue *aa = a;
  const WidgetPropertyValue *bb = b;
  return strcmp (aa->name, bb->name);
}

static void
gtk_css_ruleset_print (const GtkCssRuleset *ruleset,
                       GString             *str)
{
  GList *values, *walk;
  WidgetPropertyValue *widget_value;
  guint i;

  _gtk_css_selector_tree_match_print (ruleset->selector_match, str);

  g_string_append (str, " {\n");

  if (ruleset->styles)
    {
      guint *sorted = g_new (guint, ruleset->n_styles);

      for (i = 0; i < ruleset->n_styles; i++)
        sorted[i] = i;

      /* so the output is identical for identical selector styles */
      g_qsort_with_data (sorted, ruleset->n_styles, sizeof (guint), compare_properties, ruleset->styles);

      for (i = 0; i < ruleset->n_styles; i++)
        {
          PropertyValue *prop = &ruleset->styles[sorted[i]];
          g_string_append (str, "  ");
          g_string_append (str, _gtk_style_property_get_name (GTK_STYLE_PROPERTY (prop->property)));
          g_string_append (str, ": ");
          _gtk_css_value_print (prop->value, str);
          g_string_append (str, ";\n");
        }

      g_free (sorted);
    }

  if (ruleset->widget_style)
    {
      values = NULL;
      for (widget_value = ruleset->widget_style; widget_value != NULL; widget_value = widget_value->next)
	values = g_list_prepend (values, widget_value);

      /* so the output is identical for identical selector styles */
      values = g_list_sort (values, compare_names);

      for (walk = values; walk; walk = walk->next)
        {
	  widget_value = walk->data;

          g_string_append (str, "  ");
          g_string_append (str, widget_value->name);
          g_string_append (str, ": ");
          g_string_append (str, widget_value->value);
          g_string_append (str, ";\n");
        }

      g_list_free (values);
    }

  g_string_append (str, "}\n");
}

static void
gtk_css_provider_print_colors (GHashTable *colors,
                               GString    *str)
{
  GList *keys, *walk;

  keys = g_hash_table_get_keys (colors);
  /* so the output is identical for identical styles */
  keys = g_list_sort (keys, (GCompareFunc) strcmp);

  for (walk = keys; walk; walk = walk->next)
    {
      const char *name = walk->data;
      GtkCssValue *color = g_hash_table_lookup (colors, (gpointer) name);

      g_string_append (str, "@define-color ");
      g_string_append (str, name);
      g_string_append (str, " ");
      _gtk_css_value_print (color, str);
      g_string_append (str, ";\n");
    }

  g_list_free (keys);
}

static void
gtk_css_provider_print_keyframes (GHashTable *keyframes,
                                  GString    *str)
{
  GList *keys, *walk;

  keys = g_hash_table_get_keys (keyframes);
  /* so the output is identical for identical styles */
  keys = g_list_sort (keys, (GCompareFunc) strcmp);

  for (walk = keys; walk; walk = walk->next)
    {
      const char *name = walk->data;
      GtkCssKeyframes *keyframe = g_hash_table_lookup (keyframes, (gpointer) name);

      if (str->len > 0)
        g_string_append (str, "\n");
      g_string_append (str, "@keyframes ");
      g_string_append (str, name);
      g_string_append (str, " {\n");
      _gtk_css_keyframes_print (keyframe, str);
      g_string_append (str, "}\n");
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
 *
 * Since: 3.2
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
  gtk_css_provider_print_keyframes (priv->keyframes, str);

  for (i = 0; i < priv->rulesets->len; i++)
    {
      if (str->len != 0)
        g_string_append (str, "\n");
      gtk_css_ruleset_print (&g_array_index (priv->rulesets, GtkCssRuleset, i), str);
    }

  return g_string_free (str, FALSE);
}

