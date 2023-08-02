.. _gtk4-rendernode-tool(1):

====================
gtk4-rendernode-tool
====================

-----------------------
GskRenderNode Utility
-----------------------

SYNOPSIS
--------
|   **gtk4-rendernode-tool** <COMMAND> [OPTIONS...] <FILE>
|
|   **gtk4-rendernode-tool** info [OPTIONS...] <FILE>
|   **gtk4-rendernode-tool** show [OPTIONS...] <FILE>
|   **gtk4-rendernode-tool** render [OPTIONS...] <FILE> [<FILE>]

DESCRIPTION
-----------

``gtk4-rendernode-tool`` can perform various operations on serialized rendernodes.

COMMANDS
--------

Information
^^^^^^^^^^^

The ``info`` command shows general information about the rendernode, such
as the number of nodes, and the depth of the tree.

Showing
^^^^^^^

The ``show`` command displays the rendernode.

Rendering
^^^^^^^^^

The ``render`` command saves a rendering of the rendernode as a png or tiff image.
The name of the file to write can be specified as a second FILE argument.

``--renderer=RENDERER``

  Use the given renderer. Use ``--renderer=help`` to get a information
  about poassible values for the ``RENDERER``.
