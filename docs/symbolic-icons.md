## Symbolic icon format

GTK has been using a simple subset of SVG for symbolic, recolorable icons for a long time. This document is an attempt to describe the format that we use.

Note that support for strokes, and style classes other than "success", "warning" and "error" has been added in GTK 4.20 and GTK 3.24.50.

### Allowed primitives

We use `path`, `circle` and `rect` elements as graphics primitives. They may be grouped with `g` elements, but those are otherwise ignored.

### Supported attributes

The `d` attribute is required on `path` elements, to specify the path.
The `cx`, `cy` and `r` attributes are required on `circle` elements.
The `x`, `y`, `width` and `height` attributes are required on `rect` elements.

These attributes are allowed on `path` elements:

- `fill`: ignored. The fill is determined based on the `class` attribute
- `stroke`: ignored. The stroke is determined based on the `class` attribute
- `class`: the symbolic part of symbolic icons. This can contain one of the following classes to determine the symbolic fill color: "success", "warning", "error", "success-fill", "warning-fill", "error-fill", "foreground-fill", "transparent-fill". The first 3 classes are the originally supported values, they have been duplicated with a -fill suffix for consistency. Any element that does not have one of these classes set is filled using the foreground color.
The `class` attribute can also contain one of the following classes to determine the symbolic stroke color: "foreground-stroke", "success-stroke", "warning-stroke", "error-stroke". Any element that does not have one of these classes is not stroked.
- `opacity`: overall opacity
- `fill-opacity`: opacity for filled paths, in addition to the symbolic color alpha
- `stroke-opacity`: opacity for stroked paths, in addition to the symbolic color alpha
- `fill-rule`: the fill rule used when filling
- `stroke-width`: line width used when stroking
- `stroke-linecap`: the line cap value used when stroking
- `stroke-linejoin`: the line join value used when stroking
- `stroke-miterlimit`: the miter limit value used when stroking
- `stroke-dasharray`: the dash array used when stroking
- `stroke-dashoffset`: the dash offset used when stroking
- `style`: ignored
- `id`: ignored
- `color`: ignored
- `overflow`: ignored

Note that when a full SVG renderer is used to render symbolics, other attributes or elements may have an effect. But you should not rely on that. GTK may use its own parser for this subset of SVG, and ignore everything else.

### Styling

The effective style that is used for recoloring is the following:

```
path,.foreground-fill {
  fill: <foreground color> !important;
}

.warning,.warning-fill {
  fill: <warning color> !important;
}

.error,.error-fill {
  fill: <error color> !important;
}

.success,.success-fill {
  fill: <success color> !important;
}

.transparent-fill {
  fill: none !important;
}

.foreground-stroke {
  stroke: <foreground color> !important;
}

.warning-stroke {
  stroke: <warning color> !important;
}

.error-stroke {
  stroke: <error color> !important;
}

.success-stroke {
  stroke: <success color> !important;
}
```

### Example icon

Here is a silly example, just to demonstrate some of the possibilities:

```
<?xml version="1.0" encoding="UTF-8"?>
<svg height="16px" viewBox="0 0 16 16" width="16px" xmlns="http://www.w3.org/2000/svg">
    <!-- a path filled with foreground color, but opacity 0.6 -->
    <path d="m 12.980469 -0.0117188 c -0.039063 0.0039063 -0.074219 0.00781255 -0.113281 0.0117188 h -4.867188 v 0.832031 c -0.050781 0.292969 0.03125 0.59375 0.226562 0.820313 c 0.191407 0.222656 0.476563 0.351562 0.773438 0.347656 h 1.585938 l -2.585938 2.585938 l -2.292969 -2.292969 c -0.25 -0.261719 -0.625 -0.367188 -0.972656 -0.273438 c -0.351563 0.089844 -0.625 0.363281 -0.714844 0.714844 c -0.09375 0.347656 0.011719 0.722656 0.273438 0.972656 l 3 3 c 0.390625 0.390625 1.023437 0.390625 1.414062 0 l 3.292969 -3.292969 v 1.585938 c -0.003906 0.296875 0.125 0.578125 0.347656 0.769531 c 0.222656 0.191407 0.519532 0.277344 0.808594 0.230469 h 0.84375 v -4.875 c 0.011719 -0.089844 0.011719 -0.183594 0 -0.273438 v -0.851562 h -0.855469 c -0.054687 -0.0078125 -0.109375 -0.0117188 -0.164062 -0.0117188 z m 0 0"
          fill-opacity="0.6"/>
    <!-- a path filled with the warning color -->
    <path d="m 14.242188 15.714844 c -0.386719 0.382812 -1.003907 0.382812 -1.386719 0 l -1.042969 -1.039063 l -1.039062 -1.042969 c -0.382813 -0.382812 -0.382813 -1 0 -1.386718 l 0.492187 -0.492188 c -2.035156 -1.109375 -4.496094 -1.109375 -6.53125 0 l 0.492187 0.492188 c 0.382813 0.386718 0.382813 1.003906 0 1.386718 l -1.039062 1.042969 l -1.042969 1.039063 c -0.382812 0.382812 -1 0.382812 -1.386719 0 l -1.039062 -1.039063 c -0.957031 -0.957031 -0.957031 -2.511719 0 -3.46875 l 0.347656 -0.347656 c 3.816406 -3.816406 10.050782 -3.816406 13.867188 0 l 0.347656 0.347656 c 0.957031 0.957031 0.957031 2.511719 0 3.46875 z m 0 0"
          class="warning"/>
    <!-- a path stroked with the error color -->
    <path d="m 14.242188 15.714844 c -0.386719 0.382812 -1.003907 0.382812 -1.386719 0 l -1.042969 -1.039063 l -1.039062 -1.042969 c -0.382813 -0.382812 -0.382813 -1 0 -1.386718 l 0.492187 -0.492188 c -2.035156 -1.109375 -4.496094 -1.109375 -6.53125 0 l 0.492187 0.492188 c 0.382813 0.386718 0.382813 1.003906 0 1.386718 l -1.039062 1.042969 l -1.042969 1.039063 c -0.382812 0.382812 -1 0.382812 -1.386719 0 l -1.039062 -1.039063 c -0.957031 -0.957031 -0.957031 -2.511719 0 -3.46875 l 0.347656 -0.347656 c 3.816406 -3.816406 10.050782 -3.816406 13.867188 0 l 0.347656 0.347656 c 0.957031 0.957031 0.957031 2.511719 0 3.46875 z m 1 0"
          class="transparent-fill error-stroke"/>
</svg>
```
