.. _gtk4-path-tool(1):

=================
gtk4-path-tool
=================

-----------------------
GskPath Utility
-----------------------

SYNOPSIS
--------
|   **gtk4-path-tool** <COMMAND> [OPTIONS...] <PATH>
|
|   **gtk4-path-tool** stroke [OPTIONS...] <PATH>
|   **gtk4-path-tool** offset [OPTIONS...] <PATH>
|   **gtk4-path-tool** decompose [OPTIONS...] <PATH>
|   **gtk4-path-tool** transform [OPTIONS...] <PATH>
|   **gtk4-path-tool** reverse [OPTIONS...] <PATH>
|   **gtk4-path-tool** restrict [OPTIONS...] <PATH>
|   **gtk4-path-tool** show [OPTIONS...] <PATH>
|   **gtk4-path-tool** render [OPTIONS...] <PATH>
|   **gtk4-path-tool** info [OPTIONS...] <PATH>

DESCRIPTION
-----------

``gtk4-path-tool`` can perform various tasks on paths. Paths are specified
in SVG syntax, as strings like "M 100 100 C 100 200 200 200 200 100 Z".

To read a path from a file, use a filename that starts with a '.' or a '/'.
To read a path from stdin, use '-'.

COMMANDS
--------

Stroking
^^^^^^^^

The ``stroke`` command performs a stroke operation along the path according to
the parameters specified via options.

``--line-width=VALUE``

  The line width to use for the stroke. ``VALUE`` must be a positive number.
  The default line width is 1.

``--line-cap=VALUE``

  The cap style to use at line ends. The possible values are ``butt``, ``round``
  or ``square``. See the SVG specification for details on these styles.
  The default cap style is ``butt``.

``--line-join=VALUE``

  The join style to use at line joins. The possible values are ``miter``,
  ``miter-clip``, ``round``, ``bevel`` or ``arcs``. See the SVG specification
  for details on these styles.
  The default join style is ``miter``.

``--miter-limit=VALUE``

  The limit at which to clip miters at line joins. The default value is 4.

``--dashes=VALUE``

  The dash pattern to use for this stroke. A dash pattern is specified by
  a comma-separated list of alternating non-negative numbers. Each number
  provides the length of alternate "on" and "off" portions of the stroke.
  If the dash pattern is empty, dashing is disabled, which is the default.
  See the SVG specification for details on dashing.

``--dash-offset=VALUE``

  The offset into the dash pattern where dashing should begin.
  The default value is 0.

Offsetting
^^^^^^^^^^

The ``offset`` command applies a lateral offset to the path. Note that this
is different from applying a translation transformation.

``--distance=VALUE``

  The distance by which to offset the path. Positive values offset to the right,
  negative values to the left (wrt to the direction of the path). The default
  value is 0.

``--line-join=VALUE``

  The join style to use at line joins. The possible values are ``miter``,
  ``miter-clip``, ``round``, ``bevel`` or ``arcs``. See the SVG specification
  for details on these styles.
  The default join style is ``miter``.

``--miter-limit=VALUE``

  The limit at which to clip miters at line joins. The default value is 4.

Decomposing
^^^^^^^^^^^

The ``decompose`` command approximates the path by one with simpler elements.
When used without options, the curves of the path are approximated by line
segments.

``--allow-curves``

  Allow cubic Bézier curves to be used in the generated path.

``--allow-conics``

  Allow rational quadratic Bézier curves to be used in the generated path.

Transforming
^^^^^^^^^^^^

The ``transform`` command applies a transformation to the path. The transform
can be 3D transform, but the resulting path is projected to the z=0 plane.

``--transform=TRANSFORM``

  The transform to apply. Transforms are specified in CSS syntax, for example:
  "translate(10,10) rotate(45)"

Reversing
^^^^^^^^^

The ``reverse`` command creates a path that traces the same outlines as
the original path, in the opposite direction.

Restricting
^^^^^^^^^^^

The ``restrict`` command creates a path that traces a segment of the original
path. Note that the start and the end of the segment are specified as
path length from the beginning of the path.

``--start=LENGTH``

  The distance from the beginning of the path where the segment begins. The
  default values is 0.

``--end=LENGTH``

  The distance from the beginning of the path where the segment ends. The
  default value is the length of path.

Showing
^^^^^^^

The ``show`` command displays the given path in a window. The interior
of the path is filled.

``--fill-rule=VALUE``

  The fill rule that is used to determine what areas are inside the path.
  The possible values are ``winding`` or ``even-odd``. The default is ``winding``.

``--fg-color=COLOR``

  The color that is used to fill the interior of the path.
  If not specified, black is used.

``--bg-color=COLOR``

  The color that is used to render the background behind the path.
  If not specified, white is used.

Rendering
^^^^^^^^^

The ``render`` command renders the given path as a PNG image.
The interior of the path is filled.

``--fill-rule=VALUE``

  The fill rule that is used to determine what areas are inside the path.
  The possible values are ``winding`` or ``even-odd``. The default is ``winding``.

``--fg-color=COLOR``

  The color that is used to fill the interior of the path.
  If not specified, black is used.

``--bg-color=COLOR``

  The color that is used to render the background behind the path.
  If not specified, white is used.

``--output-file=FILE``

  The file to save the PNG image to.
  If not specified, "path.png" is used.

Info
^^^^

The ``info`` command shows various information about the given path,
such as the number of contours, its bounding box and and its length.

REFERENCES
----------

- SVG Path Specification, https://www.w3.org/TR/SVG2/paths.html
