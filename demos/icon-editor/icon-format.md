# The symbolic icon format

GTK has been using a simple subset of SVG for symbolic, recolorable
icons for a long time. More recently, we have added a few small
extensions to support states and transitions between them.

This document is an attempt to describe this format.

## States and transitions

An icon can define a number of states. Icons always have an `unset`
state, which is used to do draw-in and draw-out animations.

Each path can be present in a subset of states (or in all states).
When the state changes, the appearance and disappearance of paths
is animated, either with a dynamic stroke, or with a blur effect.
There are a number of parameters to influence the transition effects.

Finally, paths can be attached to other paths, in wich case they
are moved along when the path they are attached to is animated.
This can be used for things like arrow heads.

# Allowed SVG primitives

We use `path`, `circle` and `rect` elements as graphics primitives.
They may be grouped with `g` elements, but those are otherwise ignored.
Other SVG elements like `defs` or inkscape-specific additions are ignored
as well.

# Supported SVG attributes

The `width` and `height` attributes on the `svg` element determine
the size of the icon. The `d` attribute is required on `path` elements,
to specify the path. The `cx`, `cy` and `r` attributes are required on
`circle` elements. The `x`, `y`, `width` and `height` attributes are
required on `rect` elements.

The following attributes are allowed on elements that specify a path:

- `fill`: ignored. The fill is determined based on the `gtk:fill`
  attribute
- `stroke`: ignored. The stroke is determined based on the `gtk:stroke`
  attribute
- `fill-opacity`: opacity for filled paths, in addition to the symbolic
  color alpha
- `stroke-opacity`: opacity for stroked paths, in addition to the symbolic
  color alpha
- `fill-rule`: the fill rule used when filling
- `stroke-width`: line width used when stroking
- `stroke-linecap`: the line cap value used when stroking
- `stroke-linejoin`: the line join value used when stroking
- `id`: used when attaching paths

Other attributes, such as `style`, `color`, `overflow` or inkscape-specific
attributes, are ignored.

For backwards compatibility with older variants of symbolic icons,
GTK interprets the `class` attribute if no attributes in the `gtk`
namespace are present.

# GTK extensions

GTK defines a small number of attributes in the `gtk` namespace
to support states and transitions.

The following attributes can be set on the `svg` element:

- `gtk:duration`: the transition duration (in seconds)
- `gtk:delay`: the transition delay (in seconds)
- `gtk:easing`: the easing function for transitions (one of
  `linear`, `ease-in-out`, `ease-in`, `ease-out` or `ease`)

The following attributes can be set on elements that specify a path:

- `gtk:stroke`: Stroke paint. Either a symbolic color name
  (`foreground`, `success`, `warning`, `error`)
  or a fixed color in the format parsed by `gdk_rgba_parse`
- `gtk:fill`: Fill paint (similar to `gtk:stroke`)
- `gtk:states`: a comma-separated list of unsigned integers
- `gtk:transition`: The transition to use. One of `none`,
  `animate` or `blur`
- `gtk:origin`: Where to start the animation. One of `start`,
  `middle` or `end`, or a number between 0 and 1
- `gtk:attach-to`: the ID of another path to attach to
- `gtk:attach-pos`: Where to attach the path (similar to `gtk:origin`)
