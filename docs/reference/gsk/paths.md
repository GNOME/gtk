Title: Paths
Slug: paths

GSK provides a path object that can be used to render more complex
shapes than lines or rounded rectangles. It is comparable to cairos
[path API](https://www.cairographics.org/manual/cairo-Paths.html),
with some notable differences.

In general, a path consists of one or more connected **_contours_**,
each of which may have multiple **_segments_**, and may or may not be closed.

<figure>
  <picture>
    <source srcset="path-dark.png" media="(prefers-color-scheme: dark)">
    <img alt="A path with multiple contours" src="path-light.png">
  </picture>
  <figcaption>A path with one closed, and one open contour</figcaption>
</figure>

The central object of the GSK path api is the immutable [struct@Gsk.Path]
struct, which contains path data in compact form suitable for rendering.

## Creating Paths

Since `GskPath` is immutable, the auxiliary [struct@Gsk.PathBuilder] struct
can be used to construct a path piece by piece. The `GskPathBuilder` API
has three distinct groups of functions:

- Functions for adding complete contours, such as [method@Gsk.PathBuilder.add_rect],
  [method@Gsk.PathBuilder.add_rounded_rect], [method@Gsk.PathBuilder.add_circle].

- Functions for building contours from individual segments, like [method@Gsk.PathBuilder.move_to],
  [method@Gsk.PathBuilder.line_to], [method@Gsk.PathBuilder.cubic_to], [method@Gsk.PathBuilder.close].

- Adding parts of a preexisting path. Functions in this group include
  [method@Gsk.PathBuilder.add_path] and [method@Gsk.PathBuilder.add_segment].

When you are done with building a path, you can convert the accumulated path
data into a `GskPath` struct with [method@Gsk.PathBuilder.free_to_path].

A sometimes convenient alternative is to create a path from a serialized
form, with [func@Gsk.Path.parse]. This function interprets strings
in SVG path syntax, such as:

    M 100 100 C 100 200 200 200 200 100 Z

## Rendering with Paths

There are two main ways to render with paths. The first is to **_fill_** the
interior of a path with a color or more complex content, such as a gradient.
GSK currently supports two different ways of determining what part of the plane
are interior to the path, which can be selected with a  [enum@Gsk.FillRule]
value.

<figure>
  <picture>
    <img alt="A filled path" src="fill-winding.png">
  </picture>
  <figcaption>A path filled with GSK_FILL_RULE_WINDING</figcaption>
</figure>

<figure>
  <picture>
    <img alt="A filled path" src="fill-even-odd.png">
  </picture>
  <figcaption>The same path, filled with GSK_FILL_RULE_EVEN_ODD</figcaption>
</figure>

Alternatively, a path can be **_stroked_**, which means to emulate drawing
with an idealized pen along the path. The result of stroking a path is another
path (the **_stroke path_**), which is then filled.

The stroke operation can be influenced with the [struct@Gsk.Stroke] struct
that collects various stroke parameters, such as the line width, the style
of line joins and line caps to use, and a dash pattern.

<figure>
  <picture>
    <img alt="A stroked path" src="stroke-miter.png">
  </picture>
  <figcaption>The same path, stroked with GSK_LINE_JOIN_MITER</figcaption>
</figure>

<figure>
  <picture>
    <img alt="A stroked path" src="stroke-round.png">
  </picture>
  <figcaption>The same path, stroked with GSK_LINE_JOIN_ROUND</figcaption>
</figure>

GSK provides render nodes for these path rendering operations: [class@Gsk.FillNode]
and [class@Gsk.StrokeNode], and GTK has convenience API to use them with
[class@Gtk.Snapshot].

## Other Path APIs

Beyond rendering, paths can be used e.g. as trajectories in animations.
In such uses, it is often important to access properties of paths, such as
their tangents at certain points. GSK provides an abstract representation
for points on a path in the form of the [struct@Gsk.PathPoint] struct.
You can query properties of a path at certain point once you have a
`GskPathPoint` representing that point.

`GskPathPoint` structs can be compared for equality with [method@Gsk.PathPoint.equal]
and ordered wrt. to which one comes first, using [method@Gsk.PathPoint.compare].

To obtain a `GskPathPoint`, use [method@Gsk.Path.get_closest_point], [method@Gsk.Path.get_start_point] or [method@Gsk.Path.get_end_point].

To query properties of the path at a point, use [method@Gsk.PathPoint.get_position],
[method@Gsk.PathPoint.get_tangent], [method@Gsk.PathPoint.get_rotation] and
[method@Gsk.PathPoint.get_curvature].
