#
# A keybinding set implementing emacs-like keybindings
#

#
# Bindings for GtkTextView and GtkEntry
#
binding "gtk-emacs-text-entry"
{
  bind "<ctrl>b" { "move-cursor" (logical-positions, -1, 0) }
  bind "<shift><ctrl>b" { "move-cursor" (logical-positions, -1, 1) }
  bind "<ctrl>f" { "move-cursor" (logical-positions, 1, 0) }
  bind "<shift><ctrl>f" { "move-cursor" (logical-positions, 1, 1) }

  bind "<alt>b" { "move-cursor" (words, -1, 0) }
  bind "<shift><alt>b" { "move-cursor" (words, -1, 1) }
  bind "<alt>f" { "move-cursor" (words, 1, 0) }
  bind "<shift><alt>f" { "move-cursor" (words, 1, 1) }

  bind "<ctrl>a" { "move-cursor" (paragraph-ends, -1, 0) }
  bind "<shift><ctrl>a" { "move-cursor" (paragraph-ends, -1, 1) }
  bind "<ctrl>e" { "move-cursor" (paragraph-ends, 1, 0) }
  bind "<shift><ctrl>e" { "move-cursor" (paragraph-ends, 1, 1) }

  bind "<ctrl>w" { "cut-clipboard" () }
  bind "<ctrl>y" { "paste-clipboard" () }

  bind "<ctrl>d" { "delete-from-cursor" (chars, 1) }
  bind "<alt>d" { "delete-from-cursor" (word-ends, 1) }
  bind "<ctrl>k" { "delete-from-cursor" (paragraph-ends, 1) }
  bind "<alt>backslash" { "delete-from-cursor" (whitespace, 1) }

  bind "<alt>space" { "delete-from-cursor" (whitespace, 1)
                      "insert-at-cursor" (" ") }
  bind "<alt>KP_Space" { "delete-from-cursor" (whitespace, 1)
                         "insert-at-cursor" (" ")  }

  #
  # Some non-Emacs keybindings people are attached to
  #
  bind "<ctrl>u" {
     "move-cursor" (paragraph-ends, -1, 0)
     "delete-from-cursor" (paragraph-ends, 1)
  }
  bind "<ctrl>h" { "delete-from-cursor" (chars, -1) }
  bind "<ctrl>w" { "delete-from-cursor" (word-ends, -1) }
}

#
# Bindings for GtkTextView
#
binding "gtk-emacs-text-view"
{
  bind "<ctrl>p" { "move-cursor" (display-lines, -1, 0) }
  bind "<shift><ctrl>p" { "move-cursor" (display-lines, -1, 1) }
  bind "<ctrl>n" { "move-cursor" (display-lines, 1, 0) }
  bind "<shift><ctrl>n" { "move-cursor" (display-lines, 1, 1) }

  bind "<ctrl>space" { "set-anchor" () }
  bind "<ctrl>KP_Space" { "set-anchor" () }
}

#
# Bindings for GtkTreeView
#
binding "gtk-emacs-tree-view"
{
  bind "<ctrl>s" { "start-interactive-search" () }
  bind "<ctrl>f" { "move-cursor" (logical-positions, 1) }
  bind "<ctrl>b" { "move-cursor" (logical-positions, -1) }
}

class "GtkEntry" binding "gtk-emacs-text-entry"
class "GtkTextView" binding "gtk-emacs-text-entry"
class "GtkTextView" binding "gtk-emacs-text-view"
class "GtkTreeView" binding "gtk-emacs-tree-view"
