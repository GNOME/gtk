Title: CSS Overview

# Overview of CSS in GTK

This chapter describes in detail how GTK uses CSS for styling and layout.

We loosely follow the CSS value definition specification in the formatting of syntax productions.

- Nonterminals are enclosed in angle backets (`〈〉`), all other strings that are not listed here are literals
- Juxtaposition means all components must occur, in the given order
- A double ampersand (`&&`) means all components must occur, in any order
- A double bar (`||`) means one or more of the components must occur, in any order
- A single bar (`|`) indicates an alternative; exactly one of the components must occur
- Brackets (`[]`) are used for grouping
- A question mark (`?`) means that the preceding component is optional
- An asterisk (`*`) means zero or more copies of the preceding component
- A plus (`+`) means one or more copies of the preceding component
- A number in curly braces (`{n}`) means that the preceding component occurs exactly n times
- Two numbers in curly braces (`{m,n}`) mean that the preceding component occurs at least m times and at most n times

## CSS nodes

GTK applies the style information found in style sheets by matching the
selectors against a tree of nodes. Each node in the tree has a name, a state
and possibly style classes. The children of each node are linearly ordered.

Every widget has one or more of these CSS nodes, and determines their name,
state, style classes and how they are layed out as children and siblings in
the overall node tree. The documentation for each widget explains what CSS
nodes it has.

### The CSS nodes of a GtkScale

```
scale[.fine-tune]
├── marks.top
│   ├── mark
┊   ┊
│   ╰── mark
├── trough
│   ├── slider
│   ├── [highlight]
│   ╰── [fill]
╰── marks.bottom
    ├── mark
    ┊
    ╰── mark
```

## Style sheets

The basic structure of the style sheets understood by GTK is a series of statements, which are either rule sets or “@-rules”, separated by whitespace.

A rule set consists of a selector and a declaration block, which is a series of declarations enclosed in curly braces. The declarations are separated by semicolons. Multiple selectors can share the same declaration block, by putting all the separators in front of the block, separated by commas.

### A rule set with two selectors

```css
button, entry {
  color: #ff00ea;
  font: 12px "Comic Sans";
}
```

## Importing style sheets

GTK supports the CSS `@import` rule, in order to load another style sheet in
addition to the currently parsed one.

The syntax for `@import` rules is as follows:

```
〈import rule〉 = @import [ 〈url〉 | 〈string〉 ]
〈url〉 = url( 〈string〉 )
```

### An example for using the `import` rule

```css
@import url("path/to/common.css");
```

To learn more about the `import` rule, you can read the [Cascading
module](https://www.w3.org/TR/css3-cascade/#at-import) of the CSS
specification.

## Selectors

Selectors work very similar to the way they do in CSS.

All widgets have one or more CSS nodes with element names and style classes.
When style classes are used in selectors, they have to be prefixed with a
period. Widget names can be used in selectors like IDs. When used in a
selector, widget names must be prefixed with a `#` character.

In more complicated situations, selectors can be combined in various ways.
To require that a node satisfies several conditions, combine several
selectors into one by concatenating them. To only match a node when it
occurs inside some other node, write the two selectors after each other,
separated by whitespace. To restrict the match to direct children of the
parent node, insert a `>` character between the two selectors.

### Theme labels that are descendants of a window

```css
window label {
  background-color: #898989;
}
```

### Theme notebooks, and anything within

```css
notebook {
  background-color: #a939f0;
}
```

### Theme combo boxes, and entries that are direct children of a notebook

```css
combobox,
notebook > entry {
  color: @fg_color;
  background-color: #1209a2;
}
```

### Theme any widget within a GtkBox

```css
box * {
  font: 20px Sans;
}
```

### Theme a label named title-label

```css
label#title-label {
  font: 15px Sans;
}
```

### Theme any widget named main-entry

```css
#main-entry {
  background-color: #f0a810;
}
```

### Theme all widgets with the style class entry

```css
.entry {
  color: #39f1f9;
}
```

### Theme the entry of a `GtkSpinButton`

```css
spinbutton entry {
  color: #900185;
}
```

It is possible to select CSS nodes depending on their position amongst their
siblings by applying pseudo-classes to the selector, like `:first-child`,
`:last-child` or `:nth-child(even)`. When used in selectors, pseudo-classes must
be prefixed with a `:` character.

### Theme labels in the first notebook tab

```css
notebook tab:first-child label {
  color: #89d012;
}
```

Another use of pseudo-classes is to match widgets depending on their state.
The available pseudo-classes for widget states are `:active`, `:hover`
`:disabled`, `:selected`, `:focus`, `:indeterminate`, `:checked` and
`:backdrop`. In addition, the following pseudo-classes don't have a direct
equivalent as a widget state: `:dir(ltr)` and `:dir(rtl)` (for text
direction); `:link` and `:visited` (for links); `:drop(active)` (for
highlighting drop targets).  Widget state pseudo-classes may only apply to
the last element in a selector.

### Theme pressed buttons

```css
button:active {
  background-color: #0274d9;
}
```

### Theme buttons with the mouse pointer over it

```css
button:hover {
  background-color: #3085a9;
}
```

### Theme insensitive widgets

```css
*:disabled {
  background-color: #320a91;
}
```

### Theme checkbuttons that are checked

```css
checkbutton:checked {
  background-color: #56f9a0;
}
```

### Theme focused labels

```css
label:focus {
  background-color: #b4940f;
}
```

### Theme inconsistent checkbuttons

```css
checkbutton:indeterminate {
  background-color: #20395a;
}
```

To determine the effective style for a widget, all the matching rule sets
are merged. As in CSS, rules apply by specificity, so the rules whose
selectors more closely match a node will take precedence over the others.

The full syntax for selectors understood by GTK can be found in the table
below. The main difference to CSS is that GTK does not currently support
attribute selectors.

| Pattern                         | Matches | Reference | Notes |
|---------------------------------|---------|-----------|-------|
| `*`                             | any node | [CSS](https://www.w3.org/TR/css3-selectors/#universal-selector) | - |
| E                               | any node with name E | [CSS](https://www.w3.org/TR/css3-selectors/#type-selectors) | - |
| E.class                         | any E node with the given style class | [CSS](https://www.w3.org/TR/css3-selectors/#class-html) | - |
| E#id                            | any E node with the given ID | [CSS](https://www.w3.org/TR/css3-selectors/#id-selectors) | GTK uses the widget name as ID |
| E:nth-child(〈nth-child〉)      | any E node which is the n-th child of its parent node | [CSS](https://www.w3.org/TR/css3-selectors/#structural-pseudos) | - |
| E:nth-last-child(〈nth-child〉) | any E node which is the n-th child of its parent node, counting from the end | [CSS](https://www.w3.org/TR/css3-selectors/#structural-pseudos) | - |
| E:first-child                   | any E node which is the first child of its parent node | [CSS](https://www.w3.org/TR/css3-selectors/#structural-pseudos) | - |
| E:last-child                    | any E node which is the last child of its parent node | [CSS](https://www.w3.org/TR/css3-selectors/#structural-pseudos) | - |
| E:only-child                    | any E node which is the only child of its parent node | [CSS](https://www.w3.org/TR/css3-selectors/#structural-pseudos) | Equivalent to E:first-child:last-child |
| E:link, E:visited               | any E node which represents a hyperlink, not yet visited (:link) or already visited (:visited) | [CSS](https://www.w3.org/TR/css3-selectors/#link) | Corresponds to `GTK_STATE_FLAG_LINK` and `GTK_STATE_FLAGS_VISITED` |
| E:active, E:hover, E:focus      | any E node which is part of a widget with the corresponding state | [CSS](https://www.w3.org/TR/css3-selectors/#useraction-pseudos) | Corresponds to `GTK_STATE_FLAG_ACTIVE`, `GTK_STATE_FLAG_PRELIGHT` and `GTK_STATE_FLAGS_FOCUSED`; GTK also allows E:prelight and E:focused |
| E:disabled                      | any E node which is part of a widget which is disabled | [CSS](https://www.w3.org/TR/css3-selectors/#UIstates) | Corresponds to `GTK_STATE_FLAG_INSENSITIVE`; GTK also allows E:insensitive |
| E:checked                       | any E node which is part of a widget (e.g. radio- or checkbuttons) which is checked | [CSS](https://www.w3.org/TR/css3-selectors/#UIstates) | Corresponds to `GTK_STATE_FLAG_CHECKED` |
| E:indeterminate                 | any E node which is part of a widget (e.g. radio- or checkbuttons) which is in an indeterminate state | [CSS3](https://www.w3.org/TR/css3-selectors/#indeterminate), [CSS4](https://drafts.csswg.org/selectors/#indeterminate) | Corresponds to `GTK_STATE_FLAG_INCONSISTENT`; GTK also allows E:inconsistent |
| E:backdrop, E:selected          | any E node which is part of a widget with the corresponding state | - | Corresponds to `GTK_STATE_FLAG_BACKDROP`, `GTK_STATE_FLAG_SELECTED` |
| E:not(〈selector〉)             | any E node which does not match the simple selector `〈selector〉` | [CSS](https://www.w3.org/TR/css3-selectors/#negation) | - |
| E:dir(ltr), E:dir(rtl)          | any E node that has the corresponding text direction | [CSS4](https://drafts.csswg.org/selectors/#the-dir-pseudo) | - |
| E:drop(active)                  | any E node that is an active drop target for a current DND operation | [CSS4](https://drafts.csswg.org/selectors/#drag-pseudos) | - |
| E F                             | any F node which is a descendent of an E node | [CSS](https://www.w3.org/TR/css3-selectors/#descendent-combinators) | - |
| E > F                           | any F node which is a child of an E node | [CSS](https://www.w3.org/TR/css3-selectors/#child-combinators) | - |
| E ~ F                           | any F node which is preceded by an E node | [CSS](https://www.w3.org/TR/css3-selectors/#general-sibling-combinators) | - |
| E + F                           | any F node which is immediately preceded by an E node | [CSS](https://www.w3.org/TR/css3-selectors/#adjacent-sibling-combinators) | - |

Where:

```
〈nth-child〉 = even | odd | 〈integer〉 | 〈integer〉n | 〈integer〉n [ + | - ] 〈integer〉
```

To learn more about selectors in CSS, read the [Selectors
module](https://www.w3.org/TR/css3-selectors/) of the CSS specification.

## Colors

CSS allows to specify colors in various ways, using numeric values or names
from a predefined list of colors.

```
〈color〉 = currentColor | transparent | 〈color name〉 | 〈rgb color〉 | 〈rgba color〉 | 〈hex color〉 | 〈gtk color〉
〈rgb color〉 = rgb( 〈number〉, 〈number〉, 〈number〉 ) | rgb( 〈percentage〉, 〈percentage〉, 〈percentage〉 )
〈rgba color〉 = rgba( 〈number〉, 〈number〉, 〈number〉, 〈alpha value〉 ) | rgba( 〈percentage〉, 〈percentage〉, 〈percentage〉, 〈alpha value〉 )
〈hex color〉 = #〈hex digits〉
〈alpha value〉 = 〈number〉, clamped to values between 0 and 1
```

The keyword `currentColor` resolves to the current value of the color
property when used in another property, and to the inherited value of the
color property when used in the color property itself.

The keyword `transparent` can be considered a shorthand for `rgba(0,0,0,0)`.

For a list of valid color names and for more background on colors in CSS,
see the [Color module](https://www.w3.org/TR/css3-color/#svg-color) of the
CSS specification.

### Specifying colors in various ways

```
color: transparent;
background-color: red;
border-top-color: rgb(128,57,0);
border-left-color: rgba(10%,20%,30%,0.5);
border-right-color: #ff00cc;
border-bottom-color: #ffff0000cccc;
```

GTK adds several additional ways to specify colors.

```
〈gtk color〉 = 〈symbolic color〉 | 〈color expression〉 | 〈win32 color〉
```

The first is a reference to a color defined via a `@define-color` rule. The
syntax for `@define-color` rules is as follows:

```
〈define color rule〉 = @define-color 〈name〉 〈color〉
```

To refer to the color defined by a `@define-color` rule, use the name from the rule, prefixed with `@`.

```
〈symbolic color〉 = @〈name〉
```

### An example for defining colors

```
@define-color bg_color #f9a039;

* {
  background-color: @bg_color;
}
```

GTK also supports color expressions, which allow colors to be transformed to
new ones and can be nested, providing a rich language to define colors.
Color expressions resemble functions, taking 1 or more colors and in some
cases a number as arguments.

`shade()` leaves the color unchanged when the number is 1 and transforms it
to black or white as the number approaches 0 or 2 respectively. For `mix()`,
0 or 1 return the unaltered 1st or 2nd color respectively; numbers between 0
and 1 return blends of the two; and numbers below 0 or above 1 intensify the
RGB components of the 1st or 2nd color respectively. `alpha()` takes a
number from 0 to 1 and applies that as the opacity of the supplied color.

```
〈color expression〉 = lighter( 〈color〉 ) | darker( 〈color〉 ) | shade( 〈color〉, 〈number〉 ) |
                     alpha( 〈color〉, 〈number〉 ) | mix( 〈color〉, 〈color〉, 〈number〉 )
```

On Windows, GTK allows to refer to system colors, as follows:

```
〈win32 color〉 = -gtk-win32-color( 〈name〉, 〈integer〉 )
```

## Images

CSS allows to specify images in various ways, for backgrounds and borders.

```
〈image〉 = 〈url〉 | 〈crossfade〉 | 〈alternatives〉 | 〈gradient〉 | 〈gtk image〉
〈crossfade〉 = cross-fade( 〈percentage〉, 〈image〉, 〈image〉 )
〈alternatives〉 = image([ 〈image〉, ]* [ 〈image〉 | 〈color〉 ] )
〈gradient〉 = 〈linear gradient〉 | 〈radial gradient〉
〈linear gradient〉 = [ linear-gradient | repeating-linear-gradient ] (
                      [ [ 〈angle〉 | to 〈side or corner〉 ] , ]?
                      〈color stops〉 )
〈radial gradient〉 = [ radial-gradient | repeating-radial-gradient ] (
                      [ [ 〈shape〉 || 〈size〉 ] [ at 〈position〉 ]? , | at 〈position〉, ]?
                      〈color stops〉 )
〈side or corner〉 = [ left | right ] || [ top | bottom ]
〈color stops〉 =  〈color stop〉 [ , 〈color stop〉 ]+
〈color stop〉 = 〈color〉 [ 〈percentage〉 | 〈length〉 ]?
〈shape〉 = circle | ellipse
〈size〉 = 〈extent keyword〉 | 〈length〉 | [ 〈length〉 | 〈percentage〉 ]{1,2}
〈extent keyword〉 = closest-size | farthest-side | closest-corner | farthest-corner
```

The simplest way to specify an image in CSS is to load an image file from a
URL. CSS does not specify anything about supported file formats; within GTK,
you can expect at least PNG, JPEG and SVG to work. The full list of
supported image formats is determined by the available gdk-pixbuf image
loaders and may vary between systems.

### Loading an image file

```css
button {
  background-image: url("water-lily.png");
}
```

A crossfade lets you specify an image as an intermediate between two images.
Crossfades are specified in the draft of the level 4 [Image
module](https://www.w3.org/TR/css4-images) of the CSS specification.

### Crossfading two images

```css
button {
  background-image: cross-fade(50%, url("water-lily.png"), url("buffalo.jpg"));
}
```

The `image()` syntax provides a way to specify fallbacks in case an image
format may not be supported. Multiple fallback images can be specified, and
will be tried in turn until one can be loaded successfully. The last
fallback may be a color, which will be rendered as a solid color image.

### Image fallback

```css
button {
  background-image: image(url("fancy.svg"), url("plain.png"), green);
}
```

Gradients are images that smoothly fades from one color to another. CSS
provides ways to specify repeating and non-repeating linear and radial
gradients. Radial gradients can be circular, or axis-aligned ellipses. In
addition to CSS gradients, GTK has its own `-gtk-gradient` extensions.

A linear gradient is created by specifying a gradient line and then several
colors placed along that line. The gradient line may be specified using an
angle, or by using direction keywords.

### Linear gradients

```css
button {
  background-image: linear-gradient(45deg, yellow, blue);
}

label {
  background-image: linear-gradient(to top right, blue 20%, #f0f 80%);
}
```

A radial gradient is created by specifying a center point and one or two
radii. The radii may be given explicitly as lengths or percentages or
indirectly, by keywords that specify how the end circle or ellipsis should
be positioned relative to the area it is derawn in.

### Radial gradients

```css
button {
  background-image: radial-gradient(ellipse at center, yellow 0%, green 100%);
}

label {
  background-image: radial-gradient(circle farthest-side at left bottom, red, yellow 50px, green);
}
```

To learn more about gradients in CSS, including details of how color stops
are placed on the gradient line and keywords for specifying radial sizes,
you can read the [Image module](https://www.w3.org/TR/css4-images) of the
CSS specification.

GTK extends the CSS syntax for images and also uses it for specifying icons.

```
〈gtk image〉 = 〈gtk gradient〉 | 〈themed icon〉 | 〈scaled image〉 | 〈recolored image〉 | 〈win32 theme part〉
```

GTK supports an alternative syntax for linear and radial gradients (which
was implemented before CSS gradients were supported).

```
〈gtk gradient〉 = 〈gtk linear gradient〉 | 〈gtk radial gradient〉
〈gtk linear gradient〉 = -gtk-gradient(linear,
                          [ 〈x position〉 〈y position〉 , ]{2}
                          〈gtk color stops〉 )
〈gtk radial gradient〉 = -gtk-gradient(radial,
                          [ 〈x position〉 〈y position〉 , 〈radius〉 , ]{2}
                          〈gtk color stops〉 )
〈x position〉 = left | right | center | 〈number〉
〈y position〉 = top | bottom | center | 〈number〉
〈radius 〉 = 〈number〉
〈gtk color stops〉 = 〈gtk color stop〉 [ , 〈gtk color stop〉 ]+
〈gtk color stop〉 = color-stop( 〈number〉 , 〈color〉 ) | from( 〈color〉 ) | to( 〈color〉 )
```

The numbers used to specify x and y positions, radii, as well as the
positions of color stops, must be between 0 and 1. The keywords for for x
and y positions (`left`, `right`, `top`, `bottom`, `center`), map to numeric
values of 0, 1 and 0.5 in the obvious way. Color stops using the `from()`
and `to()` syntax are abbreviations for color-stop with numeric positions of
0 and 1, respectively.

### Linear gradients

```css
button {
  background-image: -gtk-gradient (linear,
                                   left top, right bottom,
                                   from(yellow), to(blue));
}
label {
  background-image: -gtk-gradient (linear,
                                   0 0, 0 1,
                                   color-stop(0, yellow),
                                   color-stop(0.2, blue),
                                   color-stop(1, #0f0));
}
```

### Radial gradients

```css
button {
  background-image: -gtk-gradient (radial,
                                   center center, 0,
                                   center center, 1,
                                   from(yellow), to(green));
}
label {
  background-image: -gtk-gradient (radial,
                                   0.4 0.4, 0.1,
                                   0.6 0.6, 0.7,
                                   color-stop(0, #f00),
                                   color-stop(0.1, $a0f),
                                   color-stop(0.2, yellow),
                                   color-stop(1, green));
}
```

GTK has extensive support for loading icons from icon themes. It is
accessible from CSS with the `-gtk-icontheme` syntax.

```
〈themed icon〉 = -gtk-icontheme( 〈icon name〉 )
```

The specified icon name is used to look up a themed icon, while taking into
account the values of the `-gtk-icon-theme` and `-gtk-icon-palette`
properties.  This kind of image is mainly used as value of the
`-gtk-icon-source` property.

### Using themed icons in CSS

```css
spinner {
  -gtk-icon-source: -gtk-icontheme('process-working-symbolic');
  -gtk-icon-palette: success blue, warning #fc3, error magenta;
}
arrow.fancy {
  -gtk-icon-source: -gtk-icontheme('pan-down');
  -gtk-icon-theme: 'Oxygen';
}
```

GTK supports scaled rendering on hi-resolution displays. This works best if
images can specify normal and hi-resolution variants. From CSS, this can be
done with the `-gtk-scaled` syntax.

```
〈scaled image〉 = -gtk-scaled( 〈image〉[ , 〈image〉 ]* )
```

While `-gtk-scaled` accepts multiple higher-resolution variants, in
practice, it will mostly be used to specify a regular image and one variant
for scale 2.

### Scaled images in CSS

```css
arrow {
  -gtk-icon-source: -gtk-scaled(url('my-arrow.png'),
                                url('my-arrow@2.png'));
}
```

```
〈recolored image〉 = -gtk-recolor( 〈url〉 [ , 〈color palette〉 ] )
```

Symbolic icons from the icon theme are recolored according to the
`-gtk-icon-palette` property. The recoloring is sometimes needed for images
that are not part of an icon theme, and the -gtk-recolor syntax makes this
available. `-gtk-recolor` requires a url as first argument. The remaining
arguments specify the color palette to use. If the palette is not explicitly
specified, the current value of the `-gtk-icon-palette` property is used.

### Recoloring an image

```css
arrow {
  -gtk-icon-source: -gtk-recolor(url('check.svg'), success blue, error rgb(255,0,0));
}
```

On Windows, GTK allows to refer to system theme parts as images, as follows:

```
〈win32 theme part〉 = -gtk-win32-theme-part( 〈name〉, 〈integer〉 〈integer〉
                                              [ , [ over( 〈integer〉 〈integer〉 [ , 〈alpha value〉 ]? ) | margins( 〈integer〉{1,4} ) ] ]* )
```

## Transitions

CSS defines a mechanism by which changes in CSS property values can be made
to take effect gradually, instead of all at once. GTK supports these
transitions as well.

To enable a transition for a property when a rule set takes effect, it needs
to be listed in the transition-property property in that rule set. Only
animatable properties can be listed in the transition-property.

The details of a transition can modified with the transition-duration,
transition-timing-function and transition-delay properties.

To learn more about transitions, you can read the [Transitions
module](https://www.w3.org/TR/css3-transitions/) of the CSS specification.

## Animations

In addition to transitions, which are triggered by changes of the underlying
node tree, CSS also supports defined animations. While transitions specify
how property values change from one value to a new value, animations
explicitly define intermediate property values in keyframes.

Keyframes are defined with an `@`-rule which contains one or more of rule
sets with special selectors. Property declarations for nonanimatable
properties are ignored in these rule sets (with the exception of animation
properties).

```
〈keyframe rule〉 = @keyframes 〈name〉 { 〈animation rule〉 }
〈animation rule〉 = 〈animation selector〉 { 〈declaration〉* }
〈animation selector〉 = 〈single animation selector〉 [ , 〈single animation selector〉 ]*
〈single animation selector〉 = from | to | 〈percentage〉
```

To enable an animation, the name of the keyframes must be set as the value
of the animation-name property. The details of the animation can modified
with the animation-duration, animation-timing-function,
animation-iteration-count and other animation properties.

### A CSS animation

```
@keyframes spin {
  to { -gtk-icon-transform: rotate(1turn); }
}

spinner {
  animation-name: spin;
  animation-duration: 1s;
  animation-timing-function: linear;
  animation-iteration-count: infinite;
}
```

To learn more about animations, you can read the [Animations
module](https://www.w3.org/TR/css3-animations/) of the CSS specification.

## Key bindings

In order to extend key bindings affecting different widgets, GTK supports
the `binding-set` rule to parse a set of `bind`/`unbind` directives. Note
that in order to take effect, the binding sets defined in this way must be
associated with rule sets by setting the `-gtk-key-bindings` property.

The syntax for `binding-set` rules is as follows:

```
〈binding set rule〉 = @binding-set 〈binding name〉 { [ [ 〈binding〉 | 〈unbinding〉 ] ; ]* }
〈binding〉 = bind "〈accelerator〉" { 〈signal emission〉* }
〈signal emission〉 = "〈signal name〉" ( [ 〈argument〉 [ , 〈argument〉 ]* ]? }
〈unbinding〉 = unbind "〈accelerator〉"
```

where `〈accelerator〉` is a string that can be parsed by
[`func@Gtk.accelerator_parse`], `〈signal name〉` is the name of a
keybinding signal of the widget in question, and the `〈argument〉` list
must be according to the signals declaration.

### An example for using the `binding-set` rule

```
@binding-set binding-set1 {
  bind "<alt>Left" { "move-cursor" (visual-positions, -3, 0) };
  unbind "End";
};

@binding-set binding-set2 {
  bind "<alt>Right" { "move-cursor" (visual-positions, 3, 0) };
  bind "<alt>KP_space" { "delete-from-cursor" (whitespace, 1)
                         "insert-at-cursor" (" ") };
};

entry {
  -gtk-key-bindings: binding-set1, binding-set2;
}
```
