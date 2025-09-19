# The symbolic icon format

GTK has been using a simple subset of SVG for symbolic, recolorable icons for a long time. More recently, a few small extensions have been added to support states and transitions between them.

This document is an attempt to describe this format.

## States, transitions and animations

An icon can define a number of states (up to 64). Icons always have an `unset` state, which is used to do draw-in and draw-out animations.

Each path can be present in a subset of states (or in all states). When the state changes, the appearance and disappearance of paths can be animated, with a dynamic stroke, a blur effect, or a fade. There are a number of parameters to influence the transition effects.

Paths can also be continuously animated with a number of effects that vary the segment of the path to be drawn, over time.

Finally, paths can be attached to other paths, in wich case they are moved along when the path they are attached to is animated. This can be used for things like arrow heads.

## Allowed SVG primitives

We use `<path>`, `<circle>` and `<rect>` elements as graphics primitives. They may be grouped with `<g>` elements, but those are otherwise ignored. Other SVG elements like `<defs>` or inkscape-specific additions are ignored as well.

## Supported SVG attributes

The `width` and `height` attributes on the `<svg>` element determine the size of the icon. The `d` attribute is required on `path` elements, to specify the path. The `cx`, `cy` and `r` attributes are required on `<circle>` elements. The `x`, `y`, `width` and `height` attributes are required on `<rect>` elements.

The following attributes are allowed on elements that specify a path:

- `fill`: ignored. The fill is determined based on the `gtk:fill` attribute
- `fill-opacity`: opacity for filled paths, in addition to the symbolic color alpha
- `fill-rule`: the fill rule used when filling
- `stroke`: ignored. The stroke is determined based on the `gtk:stroke` attribute
- `stroke-opacity`: opacity for stroked paths, in addition to the symbolic color alpha
- `stroke-width`: line width used when stroking, ignored when `gtk:stroke-width` is set
- `stroke-linecap`: the line cap value used when stroking
- `stroke-linejoin`: the line join value used when stroking
- `id`: used when attaching paths

Other attributes, such as `style`, `color`, `overflow`, or inkscape-specific attributes, are ignored.

For backwards compatibility with older variants of symbolic icons, GTK interprets the `class` attribute if no attributes in the `gtk` namespace are present.

## GTK extensions

GTK defines a small number of attributes in the `gtk` namespace to support states and transitions.

The following attributes can be set on the `<svg>` element:

- `gtk:keywords`: a space-separated list of keywords
- `gtk:state`: The initial state

The following attributes can be set on elements that specify a path:

- `gtk:stroke`: Stroke paint. Either a symbolic color name ('foreground', 'success', 'warning', 'error') or a fixed color in the format parsed by `gdk_rgba_parse`
- `gtk:fill`: Fill paint (similar to `gtk:stroke`)
- `gtk:states`: A space-separated list of unsigned integers, or 'all' or 'none'
- `gtk:animation-type`: The animation to use. One of 'none' or 'automatic'
- `gtk:animation-direction`: One of 'normal', 'alternate', 'reverse', 'reverse-alternate', 'in-out', 'in-out-alternate', 'in-out-reverse', 'segment, 'segment-alternate'
- `gtk:animation-duration`: The duration of one animation cycle (in seconds)
- `gtk:animation-easing`: the easing function for animations (one of 'linear', 'ease-in-out', 'ease-in', 'ease-out' or 'ease')
- `gtk:transition-type`: The transition to use. One of 'none', 'animate', 'blur' or 'fade'
- `gtk:transition-duration`: The transition duration (in seconds)
- `gtk:transition-easing`: The easing function for transitions (one of 'linear', 'ease-in-out', 'ease-in', 'ease-out' or 'ease')
- `gtk:origin`: Where to start the draw transition. A number between 0 and 1
- `gtk:attach-to`: The ID of another path to attach to
- `gtk:attach-pos`: Where to attach the path (similar to `gtk:origin`)
- `gtk:stroke-width`: 3 space-separated numbers, for the minimum, default and maximum stroke-width. The minimum and maximum are used when applying font weight
