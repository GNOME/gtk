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
#include "gtksettingsprivate.h"
#include "gtkstyleprovider.h"
#include "gtkstylecontextprivate.h"
#include "gtkstylepropertyprivate.h"
#include "gtkstyleproviderprivate.h"
#include "gtkwidgetpath.h"
#include "gtkbindings.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkversion.h"

/**
 * SECTION:gtkcssprovider
 * @Short_description: CSS-like styling for widgets
 * @Title: GtkCssProvider
 * @See_also: #GtkStyleContext, #GtkStyleProvider
 *
 * GtkCssProvider is an object implementing the #GtkStyleProvider interface.
 * It is able to parse [CSS-like](http://www.w3.org/TR/CSS2)
 * input in order to style widgets.
 *
 * ## Default files
 *
 * An application can cause GTK+ to parse a specific CSS style sheet by
 * calling gtk_css_provider_load_from_file() and adding the provider with
 * gtk_style_context_add_provider() or gtk_style_context_add_provider_for_screen().
 * In addition, certain files will be read when GTK+ is initialized. First,
 * the file `$XDG_CONFIG_HOME/gtk-3.0/gtk.css`
 * is loaded if it exists. Then, GTK+ tries to load
 * `$HOME/.themes/theme-name/gtk-3.0/gtk.css`,
 * falling back to
 * `datadir/share/themes/theme-name/gtk-3.0/gtk.css`,
 * where theme-name is the name of the current theme
 * (see the #GtkSettings:gtk-theme-name setting) and datadir
 * is the prefix configured when GTK+ was compiled, unless overridden by the
 * `GTK_DATA_PREFIX` environment variable.
 *
 * # Style sheets
 *
 * The basic structure of the style sheets understood by this provider is
 * a series of statements, which are either rule sets or “@-rules”, separated
 * by whitespace.
 *
 * A rule set consists of a selector and a declaration block, which is
 * a series of declarations enclosed in curly braces ({ and }). The
 * declarations are separated by semicolons (;). Multiple selectors can
 * share the same declaration block, by putting all the separators in
 * front of the block, separated by commas.
 *
 * An example of a rule set with two selectors:
 * |[
 * GtkButton, GtkEntry {
 *     color: #ff00ea;
 *     font: Comic Sans 12
 * }
 * ]|
 *
 * # Selectors # {#gtkcssprovider-selectors}
 *
 * Selectors work very similar to the way they do in CSS, with widget class
 * names taking the role of element names, and widget names taking the role
 * of IDs. When used in a selector, widget names must be prefixed with a
 * '#' character. The “*” character represents the so-called universal
 * selector, which matches any widget.
 *
 * To express more complicated situations, selectors can be combined in
 * various ways:
 * - To require that a widget satisfies several conditions,
 *   combine several selectors into one by concatenating them. E.g.
 *   `GtkButton#button1` matches a GtkButton widget
 *   with the name button1.
 * - To only match a widget when it occurs inside some other
 *   widget, write the two selectors after each other, separated by whitespace.
 *   E.g. `GtkToolBar GtkButton` matches GtkButton widgets
 *   that occur inside a GtkToolBar.
 * - In the previous example, the GtkButton is matched even
 *   if it occurs deeply nested inside the toolbar. To restrict the match
 *   to direct children of the parent widget, insert a “>” character between
 *   the two selectors. E.g. `GtkNotebook > GtkLabel` matches
 *   GtkLabel widgets that are direct children of a GtkNotebook.
 *
 * ## Examples of widget classes and names in selectors
 *
 * Theme labels that are descendants of a window:
 * |[
 * GtkWindow GtkLabel {
 *     background-color: #898989
 * }
 * ]|
 *
 * Theme notebooks, and anything that’s within these:
 * |[
 * GtkNotebook {
 *     background-color: #a939f0
 * }
 * ]|
 *
 * Theme combo boxes, and entries that are direct children of a notebook:
 * |[
 * GtkComboBox,
 * GtkNotebook > GtkEntry {
 *     color: @fg_color;
 *     background-color: #1209a2
 * }
 * ]|
 *
 * Theme any widget within a GtkBin:
 * |[
 * GtkBin * {
 *     font: Sans 20
 * }
 * ]|
 *
 * Theme a label named title-label:
 * |[
 * GtkLabel#title-label {
 *     font: Sans 15
 * }
 * ]|
 *
 * Theme any widget named main-entry:
 * |[
 * #main-entry {
 *     background-color: #f0a810
 * }
 * ]|
 *
 * Widgets may also define style classes, which can be used for matching.
 * When used in a selector, style classes must be prefixed with a “.”
 * character.
 *
 * Refer to the documentation of individual widgets to learn which
 * style classes they define and see
 * [Style Classes and Regions][gtkstylecontext-classes]
 * for a list of all style classes used by GTK+ widgets.
 *
 * Note that there is some ambiguity in the selector syntax when it comes
 * to differentiation widget class names from regions. GTK+ currently treats
 * a string as a widget class name if it contains any uppercase characters
 * (which should work for more widgets with names like GtkLabel).
 *
 * ## Examples for style classes in selectors
 *
 * Theme all widgets defining the class entry:
 * |[
 * .entry {
 *     color: #39f1f9;
 * }
 * ]|
 *
 * Theme spinbuttons’ entry:
 * |[
 * GtkSpinButton.entry {
 *     color: #900185
 * }
 * ]|
 *
 * In complicated widgets like e.g. a GtkNotebook, it may be desirable
 * to style different parts of the widget differently. To make this
 * possible, container widgets may define regions, whose names
 * may be used for matching in selectors.
 *
 * Some containers allow to further differentiate between regions by
 * applying so-called pseudo-classes to the region. For example, the
 * tab region in GtkNotebook allows to single out the first or last
 * tab by using the :first-child or :last-child pseudo-class.
 * When used in selectors, pseudo-classes must be prefixed with a
 * ':' character.
 *
 * Refer to the documentation of individual widgets to learn which
 * regions and pseudo-classes they define and see
 * [Style Classes and Regions][gtkstylecontext-classes]
 * for a list of all regions
 * used by GTK+ widgets.
 *
 * ## Examples for regions in selectors
 *
 * Theme any label within a notebook:
 * |[
 * GtkNotebook GtkLabel {
 *     color: #f90192;
 * }
 * ]|
 *
 * Theme labels within notebook tabs:
 * |[
 * GtkNotebook tab GtkLabel {
 *     color: #703910;
 * }
 * ]|
 *
 * Theme labels in the any first notebook tab, both selectors are
 * equivalent:
 * |[
 * GtkNotebook tab:nth-child(first) GtkLabel,
 * GtkNotebook tab:first-child GtkLabel {
 *     color: #89d012;
 * }
 * ]|
 *
 * Another use of pseudo-classes is to match widgets depending on their
 * state. This is conceptually similar to the :hover, :active or :focus
 * pseudo-classes in CSS. The available pseudo-classes for widget states
 * are :active, :prelight (or :hover), :insensitive, :selected, :focused
 * and :inconsistent.
 *
 * ## Examples for styling specific widget states
 *
 * Theme active (pressed) buttons: 
 * |[
 * GtkButton:active {
 *     background-color: #0274d9;
 * }
 * ]|
 *
 * Theme buttons with the mouse pointer on it, both are equivalent:
 * |[
 * GtkButton:hover,
 * GtkButton:prelight {
 *     background-color: #3085a9;
 * }
 * ]|
 *
 * Theme insensitive widgets, both are equivalent:
 * |[
 * :insensitive,
 * *:insensitive {
 *     background-color: #320a91;
 * }
 * ]|
 *
 * Theme selection colors in entries:
 * |[
 * GtkEntry:selected {
 *     background-color: #56f9a0;
 * }
 * ]|
 *
 * Theme focused labels:
 * |[
 * GtkLabel:focused {
 *     background-color: #b4940f;
 * }
 * ]|
 *
 * Theme inconsistent checkbuttons:
 * |[
 * GtkCheckButton:inconsistent {
 *     background-color: #20395a;
 * }
 * ]|
 *
 * Widget state pseudoclasses may only apply to the last element
 * in a selector.
 *
 * To determine the effective style for a widget, all the matching rule
 * sets are merged. As in CSS, rules apply by specificity, so the rules
 * whose selectors more closely match a widget path will take precedence
 * over the others.
 *
 * # @ Rules
 *
 * GTK+’s CSS supports the \@import rule, in order to load another
 * CSS style sheet in addition to the currently parsed one.
 *
 * An example for using the \@import rule:
 * |[
 * @import url ("path/to/common.css");
 * ]|
 * 
 * In order to extend key bindings affecting different widgets, GTK+
 * supports the \@binding-set rule to parse a set of bind/unbind
 * directives, see #GtkBindingSet for the supported syntax. Note that
 * the binding sets defined in this way must be associated with rule sets
 * by setting the gtk-key-bindings style property.
 *
 * Customized key bindings are typically defined in a separate
 * `gtk-keys.css` CSS file and GTK+ loads this file
 * according to the current key theme, which is defined by the
 * #GtkSettings:gtk-key-theme-name setting.
 *
 * An example for using the \@binding rule:
 * |[
 * @binding-set binding-set1 {
 *   bind "<alt>Left" { "move-cursor" (visual-positions, -3, 0) };
 *   unbind "End";
 * };
 *
 * @binding-set binding-set2 {
 *   bind "<alt>Right" { "move-cursor" (visual-positions, 3, 0) };
 *   bind "<alt>KP_space" { "delete-from-cursor" (whitespace, 1)
 *                          "insert-at-cursor" (" ") };
 * };
 *
 * GtkEntry {
 *   gtk-key-bindings: binding-set1, binding-set2;
 * }
 * ]|
 *
 * GTK+ also supports an additional \@define-color rule, in order
 * to define a color name which may be used instead of color numeric
 * representations. Also see the #GtkSettings:gtk-color-scheme setting
 * for a way to override the values of these named colors.
 *
 * An example for defining colors:
 * |[
 * @define-color bg_color #f9a039;
 *
 * * {
 *     background-color: @bg_color;
 * }
 * ]|
 *
 * # Symbolic colors
 *
 * Besides being able to define color names, the CSS parser is also able
 * to read different color expressions, which can also be nested, providing
 * a rich language to define colors which are derived from a set of base
 * colors.
 *
 * An example for using symbolic colors:
 * |[
 * @define-color entry-color shade (@bg_color, 0.7);
 *
 * GtkEntry {
 *     background-color: @entry-color;
 * }
 *
 * GtkEntry:focused {
 *     background-color: mix (@entry-color,
 *                            shade (#fff, 0.5),
 *                            0.8);
 * }
 * ]|
 *
 * # Specifying Colors # {#specifying-colors}
 * There are various ways to express colors in GTK+ CSS.
 *
 * ## rgb(r, g, b)
 *
 * An opaque color.
 *
 * - `r`, `g`, `b` can be either integers between 0 and 255, or percentages.
 *
 * |[
 *   color: rgb(128, 10, 54);
 *   background-color: rgb(20%, 30%, 0%);
 * ]|
 *
 * ## rgba(r, g, b, a)
 *
 * A translucent color.
 *
 * - `r`, `g`, `b` can be either integers between 0 and 255, or percentages.
 * - `a` is a floating point number between 0 and 1.
 *
 * |[
 *   color: rgb(128, 10, 54, 0.5);
 * ]|
 *
 * ## \#xxyyzz
 *
 * An opaque color.
 *
 * - `xx`, `yy`, `zz` are hexadecimal numbers specifying `r`, `g`, `b`
 *   variants with between 1 and 4 hexadecimal digits per component.
 *
 * |[
 *   color: #f0c;
 *   background-color: #ff00cc;
 *   border-color: #ffff0000cccc;
 * ]|
 *
 * ## \@name
 *
 * Reference to a color that has been defined with \@define-color
 *
 * |[
 *   color: @bg_color;
 * ]|
 *
 * ## mix(color1, color2, factor)
 *
 * A linear combination of `color1` and `color2`.
 *
 * - `factor` is a floating point number between 0 and 1.
 *
 * |[
 *   color: mix(#ff1e0a, @bg_color, 0.8);
 * ]|
 *
 * ## shade(color, factor)
 *
 * A lighter or darker variant of `color`.
 *
 * - `factor` is a floating point number.
 *
 * |[
 *   color: shade(@fg_color, 0.5);
 * ]|
 *
 * ## lighter(color)
 *
 * A lighter variant of `color`.
 *
 * |[
 *   color: lighter(@fg_color);
 * ]|
 *
 * ## darker(color)
 *
 * A darker variant of `color`.
 *
 * |[
 *   color: darker(@bg_color);
 * ]|
 *
 * ## alpha(color, factor)
 *
 * Modifies passed color’s alpha by a factor.
 *
 * - `factor` is a floating point number. `factor` < 1.0 results in a more
 *   transparent color while `factor` > 1.0 results in a more opaque color.
 *
 * |[
 *   color: alpha(@fg_color, 0.5);
 * ]|
 *
 *
 * # Gradients
 *
 * Linear or radial gradients can be used as background images.
 *
 * ## Linear Gradients
 *
 * A linear gradient along the line from (`start_x`, `start_y`) to
 * (`end_x`, `end_y`) is specified using the following syntax:
 *
 * > `-gtk-gradient (linear, start_x start_y, end_x end_y, color-stop (position, color), ...)`
 *
 * - `start_x` and `end_x` can be either a floating point number between
 * 0 and 1, or one of the special values: “left”, “right”, or “center”.
 * - `start_y` and `end_y` can be either a floating point number between 0 and 1, or one
 * of the special values: “top”, “bottom” or “center”.
 * - `position` is a floating point number between 0 and 1.
 * - `color` is a color expression (see above).
 *
 * The color-stop can be repeated multiple times to add more than one color
 * stop. “from (color)” and “to (color)” can be used as abbreviations for
 * color stops with position 0 and 1, respectively.
 *
 * ## Example: Linear Gradient
 * ![](gradient1.png)
 * |[
 * -gtk-gradient (linear,
 *                left top, right bottom,
 *                from(@yellow), to(@blue));
 * ]|
 *
 * ## Example: Linear Gradient 2
 * ![](gradient2.png)
 * |[
 * -gtk-gradient (linear,
 *                0 0, 0 1,
 *                color-stop(0, @yellow),
 *                color-stop(0.2, @blue),
 *                color-stop(1, #0f0))
 * ]|
 *
 * ## Radial Gradients
 *
 * A radial gradient along the two circles defined by (`start_x`,
 * `start_y`, `start_radius`) and (`end_x`, `end_y`, `end_radius`) is
 * specified using the following syntax:
 *
 * > `-gtk-gradient (radial, start_x start_y, start_radius, end_x end_y, end_radius, color-stop (position, color), ...)`
 *
 * where `start_radius` and `end_radius` are floating point numbers
 * and the other parameters are as before.
 *
 * ## Example: Radial Gradient
 * ![](gradient3.png)
 * |[
 * -gtk-gradient (radial,
 *                center center, 0,
 *                center center, 1,
 *                from(@yellow), to(@green))
 * ]|
 *
 * ## Example: Radial Gradient 2
 * ![](gradient4.png)
 * |[
 * -gtk-gradient (radial,
 *                0.4 0.4, 0.1,
 *                0.6 0.6, 0.7,
 *                color-stop (0, #f00),
 *                color-stop (0.1, #a0f),
 *                color-stop (0.2, @yellow),
 *                color-stop (1, @green))
 * ]|
 *
 * # Border images # {#border-images}
 *
 * Images and gradients can also be used in slices for the purpose of creating
 * scalable borders.
 * For more information, see the [CSS3 documentation for the border-image property](http://www.w3.org/TR/css3-background/#border-images).
 *
 * ![](slices.png)
 *
 * The parameters of the slicing process are controlled by four
 * separate properties.
 *
 * - Image Source
 * - Image Slice
 * - Image Width
 * - Image Repeat
 *
 * Note that you can use the `border-image` shorthand property to set
 * values for the properties at the same time.
 *
 * ## Image Source
 *
 * The border image source can be specified either as a
 * URL or a gradient:
 * |[
 *   border-image-source: url(path);
 * ]|
 * or
 * |[
 *   border-image-source: -gtk-gradient(...);
 * ]|
 *
 * ## Image Slice
 *
 * |[
 *   border-image-slice: top right bottom left;
 * ]|
 *
 * The sizes specified by the `top`, `right`, `bottom`, and `left` parameters
 * are the offsets (in pixels) from the relevant edge where the image
 * should be “cut off” to build the slices used for the rendering
 * of the border.
 *
 * ## Image Width
 *
 * |[
 *   border-image-width: top right bottom left;
 * ]|
 *
 * The sizes specified by the @top, @right, @bottom and @left parameters
 * are inward distances from the border box edge, used to specify the
 * rendered size of each slice determined by border-image-slice.
 * If this property is not specified, the values of border-width will
 * be used as a fallback.
 *
 * ## Image Repeat
 *
 * Specifies how the image slices should be rendered in the area
 * outlined by border-width.
 *
 * |[
 *   border-image-repeat: [stretch|repeat|round|space];
 * ]|
 * or
 * |[
 *   border-image-repeat: [stretch|repeat|round|space] [stretch|repeat|round|space];
 * ]|
 *
 * - The default (stretch) is to resize the slice to fill in the
 * whole allocated area.
 *
 * - If the value of this property is “repeat”, the image slice will
 * be tiled to fill the area.
 *
 * - If the value of this property is “round”, the image slice will be
 * tiled to fill the area, and scaled to fit it exactly a whole number
 * of times.
 *
 * - If the value of this property is “space”, the image slice will be
 * tiled to fill the area, and if it doesn’t fit it exactly a whole
 * number of times, the extra space is distributed as padding around
 * the slices.
 *
 * - If two options are specified, the first one affects the
 * horizontal behaviour and the second one the vertical behaviour.  If
 * only one option is specified, it affects both.
 *
 *
 * ## Example: Border Image
 * ![](border1.png)
 * |[
 * border-image: url("gradient1.png") 10 10 10 10;
 * ]|
 *
 * ## Example: Repeating Border Image
 * ![](border2.png)
 * |[
 * border-image: url("gradient1.png") 10 10 10 10 repeat;
 * ]|
 *
 * ## Example: Stetched Border Image
 * ![](border3.png)
 * |[
 * border-image: url("gradient1.png") 10 10 10 10 stretch;
 * ]|
 *
 *
 * # Supported Properties
 *
 * Properties are the part that differ the most to common CSS, not all
 * properties are supported (some are planned to be supported
 * eventually, some others are meaningless or don't map intuitively in
 * a widget based environment).
 *
 * The currently supported properties are:
 *
 * ## engine: [name|none];
 *
 * - `none` means to use the default (ie. builtin engine)
 * |[
 *  engine: clearlooks;
 * ]|
 *
 * ## background-color: [color|transparent];
 *
 * - `color`: See [Specifying Colors][specifying-colors]
 * |[
 *  background-color: shade (@color1, 0.5);
 * ]|
 *
 * ## color: [color|transparent];
 *
 * - `color`: See [Specifying Colors][specifying-colors]
 * |[
 *  color: #fff;
 * ]|
 *
 * ## border-color: [color|transparent]{1,4};
 *
 * - `color`: See [Specifying Colors][specifying-colors]
 * - Four values used to specify: top right bottom left
 * - Three values used to specify: top vertical bottom
 * - Two values used to specify: horizontal vertical
 * - One value used to specify: color
 * |[
 *  border-color: red green blue;
 * ]|
 *
 * ## border-top-color: [color|transparent];
 *
 * - `color`: See [Specifying Colors][specifying-colors]
 * |[
 *  border-top-color: @borders;
 * ]|
 *
 * ## border-right-color: [color|transparent];
 *
 * - `color`: See [Specifying Colors][specifying-colors]
 * |[
 *  border-right-color: @borders;
 * ]|
 *
 * ## border-bottom-color: [color|transparent];
 *
 * - `color`: See [Specifying Colors][specifying-colors]
 * |[
 *  border-bottom-color: @borders;
 * ]|
 *
 * ## border-left-color: [color|transparent];
 *
 * - `color`: See [Specifying Colors][specifying-colors]
 * |[
 *  border-left-color: @borders;
 * ]|
 *
 * ## font-family: name;
 *
 * The name of the font family or font name to use.
 *
 * - Note: unlike the CSS2 Specification this does not support using a
 *   prioritized list of font family names and/or generic family
 *   names.
 *
 * |[
 *  font-family: Sans, Cantarell;
 * ]|
 *
 * ## font-style: [normal|oblique|italic];
 *
 * Selects between normal, italic and oblique faces within a font family.
 *
 * |[
 *  font-style: italic;
 * ]|
 *
 * ## font-variant: [normal|small-caps];
 *
 * In a small-caps font the lower case letters look similar to the
 * uppercase ones, but in a smaller size and with slightly different
 * proportions.
 *
 * |[
 *  font-variant: normal;
 * ]|
 *
 * ## font-weight: [normal|bold|bolder|lighter|100|200|300|400|500|600|700|800|900];
 *
 * Selects the weight of the font. The values '100' to '900' form an
 * ordered sequence, where each number indicates a weight that is at
 * least as dark as its predecessor. The keyword 'normal' is
 * synonymous with '400', and 'bold' is synonymous with
 * '700'. Keywords other than 'normal' and 'bold' have been shown to
 * be often confused with font names and a numerical scale was
 * therefore chosen for the 9-value list.
 * - Maps to #PANGO_TYPE_WEIGHT
 * |[
 *  font-weight: bold;
 * ]|
 *
 * ## font-size: [absolute-size|relative-size|percentage];
 *
 * - `absolute-size`: The size in normal size units like `px`, `pt`,
 *    and `em`. Or symbolic sizes like `xx-small`, `x-small`, `small`,
 *    `medium`, `large`, `x-large`, `xx-large`.
 * - `relative-size`: `larger` or `smaller` relative to the parent.
 * - `percentage`: A percentage difference from the nominal size.
 * |[
 *  font-size: 12px;
 * ]|
 *
 * ## font-stretch: [face]
 *
 * Selects a normal, condensed, or expanded face from a font family.
 *
 * Absolute keyword values have the following ordering, from narrowest to widest:
 *
 * - ultra-condensed
 * - extra-condensed
 * - condensed
 * - semi-condensed
 * - normal
 * - semi-expanded
 * - expanded
 * - extra-expanded
 * - ultra-expanded
 *
 * ## font: [family] [style] [variant] [stretch] [size];
 *
 * A shorthand for setting a few font properties at once.
 * - Supports any format accepted by pango_font_description_from_string()
 * - Note: this is somewhat different from the CSS2 Specification for this property.
 * |[
 *  font: Bold 11;
 * ]|
 *
 * ## margin: [length|percentage]{1,4};
 *
 * A shorthand for setting the margin space required on all sides of
 * an element.
 * - Four values used to specify: top right bottom left
 * - Three values used to specify: top horizontal bottom
 * - Two values used to specify: vertical horizontal
 * - One value used to specify: margin
 * |[
 *  margin: 1em 2em 4em;
 * ]|
 *
 * ## margin-top: [length|percentage];
 *
 * Sets the margin space required on the top of an element.
 * |[
 *  margin-top: 10px;
 * ]|
 *
 * ## margin-right: [length|percentage];
 *
 * Sets the margin space required on the right of an element.
 * |[
 *  margin-right: 0px;
 * ]|
 *
 * ## margin-bottom: [length|percentage];
 *
 * Sets the margin space required on the bottom of an element.
 * |[
 *  margin-bottom: 10px;
 * ]|
 *
 * ## margin-left: [length|percentage];
 *
 * Sets the margin space required on the left of an element.
 * |[
 *  margin-left: 1em;
 * ]|
 *
 * ## padding: [length|percentage]{1,4};
 *
 * A shorthand for setting the padding space required on all sides of
 * an element. The padding area is the space between the content of
 * the element and its border.
 * - Four values used to specify: top right bottom left
 * - Three values used to specify: top horizontal bottom
 * - Two values used to specify: vertical horizontal
 * - One value used to specify: padding
 * |[
 *  padding: 1em 2em 4em;
 * ]|
 *
 * ## padding-top: [length|percentage];
 *
 * Sets the padding space required on the top of an element.
 * |[
 *  padding-top: 10px;
 * ]|
 *
 * ## padding-right: [length|percentage];
 *
 * Sets the padding space required on the right of an element.
 * |[
 *  padding-right: 0px;
 * ]|
 *
 * ## padding-bottom: [length|percentage];
 *
 * Sets the padding space required on the bottom of an element.
 * |[
 *  padding-bottom: 10px;
 * ]|
 *
 * ## padding-left: [length|percentage];
 *
 * Sets the padding space required on the left of an element.
 * |[
 *  padding-left: 1em;
 * ]|
 *
 * ## border-width: [width]{1,4};
 *
 * A shorthand for setting the border width on all sides of
 * an element.
 * - Four values used to specify: top right bottom left
 * - Three values used to specify: top vertical bottom
 * - Two values used to specify: horizontal vertical
 * - One value used to specify: width
 * |[
 *  border-width: 1px 2px 4px;
 * ]|
 *
 * ## border-top-width: [width];
 *
 * Sets the border width required on the top of an element.
 * |[
 *  border-top: 10px;
 * ]|
 *
 * ## border-right-width: [width];
 *
 * Sets the border width required on the right of an element.
 * |[
 *  border-right: 0px;
 * ]|
 *
 * ## border-bottom-width: [width];
 *
 * Sets the border width required on the bottom of an element.
 * |[
 *  border-bottom: 10px;
 * ]|
 *
 * ## border-left-width: [width];
 *
 * Sets the border width required on the left of an element.
 * |[
 *  border-left: 1em;
 * ]|
 *
 * ## border-radius: [length|percentage]{1,4};
 *
 * Allows setting how rounded all border corners are.
 * - Four values used to specify: top-left top-right bottom-right bottom-left
 * - Three values used to specify: top-left top-right-and-bottom-left bottom-right
 * - Two values used to specify: top-left-and-bottom-right top-right-and-bottom-left
 * - One value used to specify: radius on all sides
 * |[
 *  border-radius: 8px
 * ]|
 *
 * ## border-style: [none|solid|inset|outset]{1,4};
 *
 * A shorthand property for setting the line style for all four sides
 * of the elements border.
 * - Four values used to specify: top right bottom left;
 * - Three values used to specify: top horizontal bottom
 * - Two values used to specify: vertical horizontal
 * - One value used to specify: style
 * |[
 *  border-style: solid;
 * ]|
 *
 * ## border-image: [source] [slice] [ / width ] [repeat]; A shorthand
 * for setting an image on the borders of elements. See [Border
 * Images][border-images].
 * |[
 *  border-image: url("/path/to/image.png") 3 4 4 3 repeat stretch;
 * ]|
 *
 * ## border-image-source: [none|url|linear-gradient]{1,4};
 *
 * Defines the image to use instead of the style of the border. If
 * this property is set to none, the style defined by border-style is
 * used instead.
 * |[
 *  border-image-source: url("/path/to/image.png");
 * ]|
 *
 * ## border-image-slice: [number|percentage]{1,4};
 *
 * Divides the image specified by border-image-source in nine regions:
 * the four corners, the four edges and the middle. It does this by
 * specifying 4 inwards offsets.
 * - Four values used to specify: top right bottom left;
 * - Three values used to specify: top vertical bottom
 * - Two values used to specify: horizontal vertical
 * - One value used to specify: slice
 * |[
 *  border-image-slice: 3 3 4 3;
 * ]|
 *
 * ## border-image-width: [length|percentage]{1,4};
 *
 * Defines the offset to use for dividing the border image in nine
 * parts, the top-left corner, central top edge, top-right-corner,
 * central right edge, bottom-right corner, central bottom edge,
 * bottom-left corner, and central right edge. They represent inward
 * distance from the top, right, bottom, and left edges.
 * - Four values used to specify: top right bottom left;
 * - Three values used to specify: top horizontal bottom
 * - Two values used to specify: vertical horizontal
 * - One value used to specify: width
 * |[
 *  border-image-width: 4px 0 4px 0;
 * ]|
 *
 * ## border-image-repeat: [none|url|linear-gradient]{1,4};
 *
 * Defines how the middle part of a border image is handled to match
 * the size of the border. It has a one-value syntax which describes
 * the behavior for all sides, and a two-value syntax that sets a
 * different value for the horizontal and vertical behavior.
 * - Two values used to specify: horizontal vertical
 * - One value used to specify: repeat
 * |[
 *  border-image-repeat: stretch;
 * ]|
 *
 * ## background-image: [none|url|linear-gradient], ...
 * Sets one or several background images for an element. The images
 * are drawn on successive stacking context layers, with the first
 * specified being drawn as if it is the closest to the user. The
 * borders of the element are then drawn on top of them, and the
 * background-color is drawn beneath them.
 * - There can be several sources listed, separated by commas.
 * |[
 *  background-image: gtk-gradient (linear,
 *                                  left top, right top,
 *                                  from (#fff), to (#000));
 * ]|
 *
 * ## background-repeat: [repeat|no-repeat|space|round|repeat-x|repeat-y];
 *
 * Defines how background images are repeated. A background image can
 * be repeated along the horizontal axis, the vertical axis, both, or
 * not repeated at all.
 * - `repeat`: The image is repeated in the given direction as much as
 *    needed to cover the whole background image painting area. The
 *    last image may be clipped if the whole thing won't fit in the
 *    remaining area.
 * - `space`: The image is repeated in the given direction as much as
 *    needed to cover most of the background image painting area,
 *    without clipping an image. The remaining non-covered space is
 *    spaced out evenly between the images. The first and last images
 *    touches the edge of the element. The value of the
 *    background-position CSS property is ignored for the concerned
 *    direction, except if one single image is greater than the
 *    background image painting area, which is the only case where an
 *    image can be clipped when the space value is used.
 * - `round`: The image is repeated in the given direction as much as
 *    needed to cover most of the background image painting area,
 *    without clipping an image. If it doesn't cover exactly the area,
 *    the tiles are resized in that direction in order to match it.
 * - `no-repeat`: The image is not repeated (and hence the background
 *    image painting area will not necessarily been entirely
 *    covered). The position of the non-repeated background image is
 *    defined by the background-position CSS property.
 * - Note if not specified, the style doesn’t respect the CSS3
 *    specification, since the background will be stretched to fill
 *    the area.
 * |[
 *  background-repeat: no-repeat;
 * ]|
 *
 * ## text-shadow: horizontal_offset vertical_offset [ blur_radius ] color;
 *
 * A shadow list can be applied to text or symbolic icons, using the CSS3
 * text-shadow syntax, as defined in the
 * [CSS3 Specification](http://www.w3.org/TR/css3-text/#text-shadow).
 *
 * - The offset of the shadow is specified with the
 * `horizontal_offset` and `vertical_offset` parameters.
 * - The optional blur radius is parsed, but it is currently not
 * rendered by the GTK+ theming engine.
 *
 * To set a shadow on an icon, use the `icon-shadow` property instead,
 * with the same syntax.
 *
 * To set multiple shadows on an element, you can specify a comma-separated list
 * of shadow elements in the `text-shadow` or `icon-shadow` property. Shadows are
 * always rendered front to back (i.e. the first shadow specified is on top of the
 * others). Shadows can thus overlay each other, but they can never overlay the
 * text or icon itself, which is always rendered on top of the shadow layer.
 *
 * |[
 *   text-shadow: 1 1 0 blue, -4 -4 red;
 * ]|

 * ## box-shadow: [ inset ] horizontal_offset vertical_offset [ blur_radius ] [ spread ] color;
 *
 * Themes can apply shadows on framed elements using the CSS3 box-shadow syntax,
 * as defined in the
 * [CSS3 Specification](http://www.w3.org/TR/css3-background/#the-box-shadow).
 *
 * - A positive offset will draw a shadow that is offset to the right (down) of the box,
 * - A negative offset to the left (top).
 * - The optional spread parameter defines an additional distance to
 * expand the shadow shape in all directions, by the specified radius.
 * - The optional blur radius parameter is parsed, but it is currently not rendered by
 * the GTK+ theming engine.
 * - The inset parameter defines whether the drop shadow should be rendered inside or outside
 * the box canvas.
 *
 * To set multiple box-shadows on an element, you can specify a comma-separated list
 * of shadow elements in the `box-shadow` property. Shadows are always rendered
 * front to back (i.e. the first shadow specified is on top of the others) so they may
 * overlap other boxes or other shadows.
 *
 * |[
 *   box-shadow: inset 0 1px 1px alpha(black, 0.1);
 * ]|
 *
 * ## transition: duration [s|ms] [linear|ease|ease-in|ease-out|ease-in-out] [loop];
 *
 * Styles can specify transitions that will be used to create a
 * gradual change in the appearance when a widget state changes.
 * - The `duration` is the amount of time that the animation will take
 * for a complete cycle from start to end.
 * - If the loop option is given, the animation will be repated until
 * the state changes again.
 * - The option after the duration determines the transition function
 * from a small set of predefined functions.
 *
 * - Linear
 *
 *   ![](linear.png)
 *
 * - Ease transition
 *
 * ![](ease.png)
 *
 * - Ease-in-out transition
 *
 * ![](ease-in-out.png)
 *
 * - Ease-in transition
 *
 * ![](ease-in.png)
 *
 * - Ease-out transition
 *
 * ![](ease-out.png)
 *
 * |[
 *   transition: 150ms ease-in-out;
 * ]|
 *
 *
 * ## gtk-key-bindings: binding1, binding2, ...;
 *
 * Key binding set name list.
 *
 * ## Other Properties
 *
 * GtkThemingEngines can register their own, engine-specific style properties
 * with the function gtk_theming_engine_register_property(). These properties
 * can be set in CSS like other properties, using a name of the form
 * `-namespace-name`, where namespace is typically
 * the name of the theming engine, and name is the
 * name of the property. Style properties that have been registered by widgets
 * using gtk_widget_class_install_style_property() can also be set in this
 * way, using the widget class name for namespace.
 *
 * An example for using engine-specific style properties:
 * |[
 * * {
 *     engine: clearlooks;
 *     border-radius: 4;
 *     -GtkPaned-handle-size: 6;
 *     -clearlooks-colorize-scrollbar: false;
 * }
 * ]|
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
                        G_ADD_PRIVATE (GtkCssProvider)
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

/* This is exported privately for use in GtkInspector.
 * It is the callers responsibility to reparse the current theme.
 */
void
gtk_css_provider_set_keep_css_sections (void)
{
  gtk_keep_css_sections = TRUE;
}

static void
gtk_css_provider_class_init (GtkCssProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  if (g_getenv ("GTK_CSS_DEBUG"))
    gtk_css_provider_set_keep_css_sections ();

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

  priv = css_provider->priv = gtk_css_provider_get_instance_private (css_provider);

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

	verify_change |= _gtk_css_selector_get_change (ruleset->selector);
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

  if (state == gtk_widget_path_iter_get_state (path, -1))
    {
      gtk_widget_path_ref (path);
    }
  else
    {
      path = gtk_widget_path_copy (path);
      gtk_widget_path_iter_set_state (path, -1, state);
    }

  if (!_gtk_css_matcher_init (&matcher, path))
    {
      gtk_widget_path_unref (path);
      return FALSE;
    }

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
  gtk_widget_path_unref (path);

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
                               GtkCssLookup            *lookup,
                               GtkCssChange            *change)
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

  if (change)
    {
      GtkCssMatcher change_matcher;

      _gtk_css_matcher_superset_init (&change_matcher, matcher, GTK_CSS_CHANGE_NAME | GTK_CSS_CHANGE_CLASS);

      *change = _gtk_css_selector_tree_get_change_all (priv->tree, &change_matcher);
      verify_tree_get_change_results (css_provider, &change_matcher, *change);
    }
}

static void
gtk_css_style_provider_private_iface_init (GtkStyleProviderPrivateInterface *iface)
{
  iface->get_color = gtk_css_style_provider_get_color;
  iface->get_keyframes = gtk_css_style_provider_get_keyframes;
  iface->lookup = gtk_css_style_provider_lookup;
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
                          "expected %s", expected);
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

static gboolean
name_is_style_property (const char *name)
{
  if (name[0] != '-')
    return FALSE;

  if (g_str_has_prefix (name, "-gtk-"))
    return FALSE;

  return TRUE;
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
  if (property == NULL && !name_is_style_property (name))
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
  else if (name_is_style_property (name))
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
 *     CSS, connect to the #GtkCssProvider::parsing-error signal.
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
 *     CSS, connect to the #GtkCssProvider::parsing-error signal.
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
 *     CSS, connect to the #GtkCssProvider::parsing-error signal.
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
 * gtk_css_provider_load_from_resource:
 * @css_provider: a #GtkCssProvider
 * @resource_path: a #GResource resource path
 *
 * Loads the data contained in the resource at @resource_path into
 * the #GtkCssProvider, clearing any previously loaded information.
 *
 * To track errors while loading CSS, connect to the
 * #GtkCssProvider::parsing-error signal.
 *
 * Since: 3.16
 */
void
gtk_css_provider_load_from_resource (GtkCssProvider *css_provider,
			             const gchar    *resource_path)
{
  GFile *file;
  gchar *uri, *escaped;

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

#if (GTK_MINOR_VERSION % 2)
#define MINOR (GTK_MINOR_VERSION + 1)
#else
#define MINOR GTK_MINOR_VERSION
#endif

/*
 * Look for
 * $dir/$subdir/gtk-3.16/gtk-$variant.css
 * $dir/$subdir/gtk-3.14/gtk-$variant.css
 *  ...
 * $dir/$subdir/gtk-3.0/gtk-$variant.css
 * and return the first found file.
 * We don't check versions before 3.14,
 * since those GTK+ versions didn't have
 * the versioned loading mechanism.
 */
static gchar *
_gtk_css_find_theme_dir (const gchar *dir,
                         const gchar *subdir,
                         const gchar *name,
                         const gchar *variant)
{
  gchar *file;
  gchar *base;
  gchar *subsubdir;
  gint i;
  gchar *path;

  if (variant)
    file = g_strconcat ("gtk-", variant, ".css", NULL);
  else
    file = g_strdup ("gtk.css");

  if (subdir)
    base = g_build_filename (dir, subdir, name, NULL);
  else
    base = g_build_filename (dir, name, NULL);

  for (i = MINOR; i >= 0; i = i - 2)
    {
      if (i < 14)
        i = 0;

      subsubdir = g_strdup_printf ("gtk-3.%d", i);
      path = g_build_filename (base, subsubdir, file, NULL);
      g_free (subsubdir);

      if (g_file_test (path, G_FILE_TEST_EXISTS))
        break;

      g_free (path);
      path = NULL;
    }

  g_free (file);
  g_free (base);

  return path;
}

#undef MINOR

static gchar *
_gtk_css_find_theme (const gchar *name,
                     const gchar *variant)
{
  gchar *path;
  const gchar *var;

  /* First look in the user's config directory */
  path = _gtk_css_find_theme_dir (g_get_user_data_dir (), "themes", name, variant);
  if (path)
    return path;

  /* Next look in the user's home directory */
  path = _gtk_css_find_theme_dir (g_get_home_dir (), ".themes", name, variant);
  if (path)
    return path;

  /* Finally, try in the default theme directory */
  var = g_getenv ("GTK_DATA_PREFIX");
  if (!var)
    var = _gtk_get_data_prefix ();

  path = _gtk_css_find_theme_dir (var, "share" G_DIR_SEPARATOR_S "themes", name, variant);

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
  gchar *path;
  gchar *resource_path;

  g_return_if_fail (GTK_IS_CSS_PROVIDER (provider));
  g_return_if_fail (name != NULL);

  gtk_css_provider_reset (provider);

  /* try loading the resource for the theme. This is mostly meant for built-in
   * themes.
   */
  if (variant)
    resource_path = g_strdup_printf ("/org/gtk/libgtk/theme/%s-%s.css", name, variant);
  else
    resource_path = g_strdup_printf ("/org/gtk/libgtk/theme/%s.css", name);

  if (g_resources_get_info (resource_path, 0, NULL, NULL, NULL))
    {
      gtk_css_provider_load_from_resource (provider, resource_path);
      g_free (resource_path);
      return;
    }
  g_free (resource_path);

  /* Next try looking for files in the various theme directories. */
  path = _gtk_css_find_theme (name, variant);
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
          /* Worst case, fall back to the default */
          g_return_if_fail (!g_str_equal (name, DEFAULT_THEME_NAME)); /* infloop protection */
          _gtk_css_provider_load_named (provider, DEFAULT_THEME_NAME, NULL);
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
 * Converts the @provider into a string representation in CSS
 * format.
 *
 * Using gtk_css_provider_load_from_data() with the return value
 * from this function on a new provider created with
 * gtk_css_provider_new() will basically create a duplicate of
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

