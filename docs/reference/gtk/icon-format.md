Title: Symbolic Icons
Slug: symbolic-icons

# The symbolic icon format

GTK has been using a simple subset of SVG for symbolic, recolorable icons for a long time. More recently, a few small extensions have been added to support states and transitions between them.

This document is an attempt to describe this format.

## States, transitions and animations

An icon can define a number of states (up to 64). Icons always have an `unset` state, which is used to do draw-in and draw-out animations.

Each path can be present in a subset of states (or in all states). When the state changes, the appearance and disappearance of paths can be animated, with a dynamic stroke, a blobby morphing effect, or a fade. There are a number of parameters to influence the transition effects.

<table>
  <tr>
    <td>
      <figure>
        <video alt="Animate" autoplay controls loop muted>
          <source src="trans-animate.webm"/>
        </video>
        <figcaption>Animate</figcaption>
      </figure>
    </td>
    <td>
      <figure>
        <video alt="Morph" autoplay controls loop muted>
          <source src="trans-morph.webm"/>
        </video>
        <figcaption>Morph</figcaption>
      </figure>
    </td>
    <td>
      <figure>
        <video alt="Fade" autoplay controls loop muted>
          <source src="trans-fade.webm"/>
        </video>
        <figcaption>Fade</figcaption>
      </figure>
    </td>
  </tr>
</table>

Paths can also be continuously animated with a number of effects that vary the segment of the path to be drawn, over time.

<table>
  <tr>
    <td>
      <figure>
        <video alt="Normal" autoplay controls loop muted>
          <source src="anim-normal.webm"/>
        </video>
        <figcaption>Normal</figcaption>
      </figure>
    </td>
    <td>
      <figure>
        <video alt="Alternate" autoplay controls loop muted>
          <source src="anim-alternate.webm"/>
        </video>
        <figcaption>Alternate</figcaption>
      </figure>
    </td>
    <td>
      <figure>
        <video alt="In-Out" autoplay controls loop muted>
          <source src="anim-in-out.webm"/>
        </video>
        <figcaption>In-Out</figcaption>
      </figure>
    </td>
  </tr>
  <tr>
    <td>
      <figure>
        <video alt="In-Out Alternate" autoplay controls loop muted>
          <source src="anim-in-out-alternate.webm"/>
        </video>
        <figcaption>In-Out Alternate</figcaption>
      </figure>
    </td>
    <td>
      <figure>
        <video alt="Segment Alternate" autoplay controls loop muted>
          <source src="anim-segment-alternate.webm"/>
        </video>
        <figcaption>Segment Alternate</figcaption>
      </figure>
    </td>
  </tr>
</table>

Finally, paths can be attached to other paths, in which case they are moved along when the path they are attached to is animated. This can be used for things like arrow heads.

## Allowed SVG primitives

We use `<path>`, `<circle>` and `<rect>` elements as graphics primitives. They may be grouped with `<g>` elements, but those are otherwise ignored. Other SVG elements like `<defs>` or inkscape-specific additions are ignored as well.

## Supported SVG attributes

The `width` and `height` attributes on the `<svg>` element determine the size of the icon. The `d` attribute is required on `path` elements, to specify the path. The `cx`, `cy` and `r` attributes are required on `<circle>` elements. The `x`, `y`, `width` and `height` attributes are required on `<rect>` elements.

The following attributes are allowed on elements that specify a path:

- `fill`: ignored. The fill paint is determined based on the `gtk:fill` attribute
- `fill-opacity`: opacity for filled paths, in addition to the alpha value of the fill paint
- `fill-rule`: the fill rule used when filling
- `stroke`: ignored. The stroke paint is determined based on the `gtk:stroke` attribute
- `stroke-opacity`: opacity for stroked paths, in addition to the alpha value of the stroke paint
- `stroke-width`: line width used when stroking, ignored when `gtk:stroke-width` is set
- `stroke-linecap`: the line cap value used when stroking
- `stroke-linejoin`: the line join value used when stroking
- `id`: used when attaching paths

Other attributes, such as `style`, `color`, `overflow`, or inkscape-specific attributes, are ignored.

For backwards compatibility with older variants of symbolic icons, GTK interprets the `class` attribute if no attributes in the `gtk` namespace are present. The recognized style classes are 'transparent-fill', 'foreground-fill', 'success-fill', 'warning-fill', 'error-fill', 'foreground-stroke', 'success-stroke', 'warning-stroke', 'error-stroke', or the older values 'foreground', 'success', 'warning', 'error', which imply filling.

## GTK extensions

GTK defines a small number of attributes in the `gpa` namespace to support states and transitions. `gpa` stands for **_GTK path animation_**, and can be pronounced 'Grappa'.

The following attributes can be set on the `<svg>` element:

- `gpa:version`: The format version. Must be 1 if specified, currently.
- `gpa:keywords`: A space-separated list of keywords
- `gpa:state`: The initial state, as a number between -1 and 63

The following attributes can be set on elements that specify a path:

- `gpa:stroke`: Stroke paint. Either a symbolic color name ('foreground', 'success', 'warning', 'error', 'accent') or a fixed color in the format parsed by `gdk_rgba_parse`
- `gpa:fill`: Fill paint (similar to `gpa:stroke`)
- `gpa:states`: A space-separated list of unsigned integers between 0 and 63, or
   'all' or 'none'
- `gpa:animation-type`: The animation to use. One of 'none' or 'automatic'
- `gpa:animation-direction`: One of 'normal', 'alternate', 'reverse', 'reverse-alternate', 'in-out', 'in-out-alternate', 'in-out-reverse', 'segment, 'segment-alternate'
- `gpa:animation-duration`: The duration of one animation cycle. A floating point number, followed by 's' or 'ms'
- `gpa:animation-repeat`: The number of animation cycles to run. A floating point number, or 'indefinite'
- `gpa:animation-easing`: The easing function for animations (one of 'linear', 'ease-in-out', 'ease-in', 'ease-out' or 'ease')
- `gpa:animation-segment`: The length of the segment to animate (as a value between 0 and 1). This is used for the 'segment' and 'segment-alternate' animation directions
- `gpa:transition-type`: The transition to use. One of 'none', 'animate', 'morph' or 'fade'
- `gpa:transition-duration`: The transition duration. A floating point number, followed by 's' or 'ms'
- `gpa:transition-delay`: The transition delay. Can be negative, for overlaps. A floating point number, followed by 's' or 'ms'
- `gpa:transition-easing`: The easing function for transitions (one of 'linear', 'ease-in-out', 'ease-in', 'ease-out' or 'ease')
- `gpa:origin`: Where to start the draw transition. A number between 0 and 1
- `gpa:attach-to`: The ID of another path to attach to
- `gpa:attach-pos`: Where to attach the path (similar to `gpa:origin`)
- `gpa:stroke-width`: 3 space-separated numbers, for the minimum, default and maximum stroke-width. The minimum and maximum are used when applying weight

## File names and mime type

Historically, GTK has expected symbolic icon files to have names ending with '-symbolc.svg'. The icons in format described here should use the mime type 'image/x-gtk-path-animation', and a file extension of '.gpa'.
