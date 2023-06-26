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
|   **gtk4-path-tool** decompose [OPTIONS...] <PATH>
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

Decomposing
^^^^^^^^^^^

The ``decompose`` command approximates the path by one with simpler elements.
When used without options, the curves of the path are approximated by line
segments.

``--allow-curves``

  Allow cubic Bézier curves to be used in the generated path.

``--allow-conics``

  Allow rational quadratic Bézier curves to be used in the generated path.

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
such as its bounding box and and its length.

REFERENCES
----------

- SVG Path Specification, https://www.w3.org/TR/SVG2/paths.html
