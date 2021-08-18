Title: CSS Properties

## Supported CSS Properties

GTK supports CSS properties and shorthands as far as they can be applied in
the context of widgets, and adds its own properties only when needed. All
GTK-specific properties have a `-gtk` prefix.

All properties support the following keywords: `inherit`, `initial`,
`unset`, with the same meaning as in
[CSS](https://www.w3.org/TR/css3-cascade/#defaulting-keywords).

The following basic datatypes are used throughout:

```
〈length〉 = 〈number〉 [ px | pt | em | ex |rem | pc | in | cm | mm ] | 〈calc expression〉
〈percentage〉 = 〈number〉 % | 〈calc expression〉
〈angle〉 = 〈number〉 [ deg | grad | turn ] | 〈calc expression〉
〈time〉 = 〈number〉 [ s | ms ] | 〈calc expression〉
```

Length values with the `em` or `ex` units are resolved using the font size
value, unless they occur in setting the font-size itself, in which case they
are resolved using the inherited font size value.

The `rem` unit is resolved using the initial font size value, which is not
quite the same as the [CSS definition of
rem](https://drafts.csswg.org/css-values-3/#font-relative-lengths).

Whereever a number is allowed, GTK also accepts a Windows-specific theme
size:

```
〈win32 theme size〉 = 〈win32 size〉 | 〈win32 part size〉
〈win32 size〉 = -gtk-win32-size ( 〈theme name〉, 〈metric id〉 )
〈win32 part size〉 = [ -gtk-win32-part-width | -gtk-win32-part-height |
                      -gtk-win32-part-border-top | -gtk-win32-part-border-right |
                      -gtk-win32-part-border-bottom | -gtk-win32-part-border-left ]
                    ( 〈theme name〉 , 〈integer〉 , 〈integer〉 )
```

```
〈calc expression〉 = calc( 〈calc sum〉 )
〈calc sum〉 = 〈calc product〉 [ [ + | - ] 〈calc product〉 ]*
〈calc product〉 = 〈calc value〉 [ * 〈calc value〉 | / 〈number〉 ]*
〈calc value〉 = 〈number〉 | 〈length〉 | 〈percentage〉 | 〈angle〉 | 〈time〉 | ( 〈calc sum〉 )
```

The `calc()` notation adds considerable expressive power. There are limits
on what types can be combined in such an expression (e.g. it does not make
sense to add a number and a time). For the full details, see the [CSS3 Values
and Units](https://www.w3.org/TR/css3-values/#calc-notation) spec.

A common pattern among shorthand properties (called 'four sides') is one
where one to four values can be specified, to determine a value for each
side of an area. In this case, the specified values are interpreted as
follows:

4 values
: top right bottom left

3 values
: top horizontal bottom

2 values
: vertical horizontal

1 value
: all

### Color properties

| Name    | Value             | Initial         | Inh. | Ani. | Reference |
|---------|-------------------|-----------------|------|------|-----------|
| color   | `〈color〉`       | `rgba(1,1,1,1)` | ✓    | ✓    | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/colors.html#propdef-color), [CSS3](https://www.w3.org/TR/css3-color/#foreground) |
| opacity | `〈alpha value〉` | 1               |      | ✓    | [CSS3](https://www.w3.org/TR/css3-color/#opacity) |

The color property specifies the color to use for text, icons and other
foreground rendering. The opacity property specifies the opacity that is
used to composite the widget onto its parent widget.

### Font properties

| Name                  | Value | Initial | Inh. | Ani. | Reference | Notes |
|-----------------------|-------|---------|------|------|-----------|-------|
| font-family           | `〈family name〉 [ , 〈family name〉 ]*` | gtk-font-name setting | ✓ |   | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/fonts.html#propdef-font-family), [CSS3](https://www.w3.org/TR/css3-fonts/#font-family-prop) | |
| font-size             | `〈absolute size〉 | 〈relative size〉 | 〈length〉 | 〈percentage`〉 | gtk-font-name setting | ✓ | ✓ | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/fonts.html#font-size-props), [CSS3](https://www.w3.org/TR/css3-fonts/#font-size-prop) | |
| font-style            | `normal | oblique | italic` | `normal` | ✓ |   | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/fonts.html#propdef-font-style), [CSS3](https://www.w3.org/TR/css3-fonts/#font-style-prop) |
| font-variant          | `normal | small-caps` | `normal` | ✓ |   | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/fonts.html#propdef-font-variant), [CSS3](https://www.w3.org/TR/css3-fonts/#descdef-font-variant) | only CSS2 values supported |
| font-weight           | `normal | bold | bolder | lighter | 100 | 200 | 300 | 400 | 500 | 600 | 700 | 800 | 900` | `normal` | ✓ | ✓ | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/fonts.html#propdef-font-weight), [CSS3](https://www.w3.org/TR/2011/REC-CSS2-20110607/fonts.html#propdef-font-weight) | normal is synonymous with 400, bold with 700 |
| font-stretch          | `ultra-condensed | extra-condensed | condensed | semi-condensed | normal | semi-expanded | expanded | extra-expanded | ultra-expanded` | `normal` | ✓ |   | [CSS3](https://www.w3.org/TR/css3-fonts/#font-stretch-prop) | |
| -gtk-dpi              | `〈number〉` | screen resolution | ✓ | ✓ | | |
| font-feature-settings | `〈feature-tag-value〉#` | "" | ✓ | > | [CSS3](https://www.w3.org/TR/css3-fonts/#font-feature-settings-prop) | |

| Shorthand | Value | Initial | Reference | Notes |
|-----------|-------|---------|-----------|-------|
| font | `[ 〈font-style〉 || 〈font-variant〉 || 〈font-weight〉 || 〈font-stretch〉 ]? 〈font-size〉 〈font-family〉` | see individual properties | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/fonts.html#font-shorthand), [CSS3](https://www.w3.org/TR/css3-fonts/#font-prop) | CSS allows line-height, etc |

```
〈absolute size〉 = xx-small | x-small | small | medium | large | x-large | xx-large
〈relative size〉 = larger | smaller
```

The font properties determine the font to use for rendering text. Relative
font sizes and weights are resolved relative to the inherited value for
these properties.

### Text caret properties

| Name                       | Value       | Initial        | Inh. | Ani. | Reference | Notes |
|----------------------------|-------------|----------------|------|------|-----------|-------|
| caret-color                | `〈color〉` | `currentColor` | ✓    | ✓    | [CSS3](https://www.w3.org/TR/css3-ui/#caret-color) | CSS allows an auto value |
| -gtk-secondary-caret-color | `〈color〉` | `currentColor` | ✓    | ✓    |           | Used for the secondary caret in bidirectional text |

The caret properties provide a way to change the appearance of the insertion caret in editable text. 

### Text decoration properties

| Name                  | Value | Initial | Inh. | Ani. | Reference | Notes |
|-----------------------|-------|---------|------|------|-----------|-------|
| letter-spacing        | `〈length〉` | 0px | ✓ | ✓ | [CSS3](https://www.w3.org/TR/css3-text/#letter-spacing) |   |
| text-decoration-line  | `none | underline | line-through` | `none` |   |   | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/text.html#propdef-text-decoration), [CSS3](https://www.w3.org/TR/css-text-decor-3/#text-decoration-line-property) | CSS allows overline |
| text-decoration-color | `〈color〉` | `currentColor` |   | ✓ | [CSS3](https://www.w3.org/TR/css-text-decor-3/#text-decoration-color-property) |   |
| text-decoration-style | `solid | double | wavy` | `solid` |   |   | [CSS3](https://www.w3.org/TR/css-text-decor-3/#text-decoration-style-property) | CSS allows dashed and dotted |
| text-shadow           | `none | 〈shadow〉` | `none` | ✓ | ✓ | [CSS3](https://www.w3.org/TR/css-text-decor-3/#text-shadow-property) |   |

| Shorthand       | Value | Initial | Reference | Notes |
|-----------------|-------|---------|-----------|-------|
| text-decoration | `〈text-decoration-line〉 || 〈text-decoration-style〉 || 〈text-decoration-color〉` | see individual properties | [CSS3](https://www.w3.org/TR/css-text-decor-3/#text-decoration-property) |   |

```
〈shadow〉 = 〈length〉 〈length〉 〈color〉?
```

The text decoration properties determine whether to apply extra decorations when rendering text.

### Icon Properties

| Name                | Value                            | Initial            | Inh. | Ani. | Reference | Notes |
|---------------------|----------------------------------|--------------------|------|------|-----------|-------|
| -gtk-icon-source    | `builtin | 〈image〉 | none`     | `builtin`          |      | ✓    |           |       |
| -gtk-icon-transform | `none | 〈transform〉+`          | `none`             |      | ✓    |           |       |
| -gtk-icon-style     | `requested | regular | symbolic` | `requested`        | ✓    |      |           | Determines the preferred style for application-loaded icons |
| -gtk-icon-theme     | `〈name〉`                       | current icon theme | ✓    |      |           | The icon theme to use with `-gtk-icontheme`. Since 3.20 |
| -gtk-icon-palette   | `〈color palette〉`              | `default`          | ✓    | ✓    |           | Used to recolor symbolic icons (both application-loaded and from `-gtk-icontheme`). Since 3.20 |
| -gtk-icon-shadow    | `none | 〈shadow〉`              | `none`             | ✓    | ✓    |           |       |
| -gtk-icon-effect    | `none | highlight | dim`         | `none`             | ✓    |      |           |       |

```
〈transform〉 = matrix( 〈number〉 [ , 〈number〉 ]{5} ) | translate( 〈length〉, 〈length〉 ) | translateX( 〈length〉 ) | translateY( 〈length〉 ) |
                scale( 〈number〉 [ , 〈number〉 ]? ) | scaleX( 〈number〉 ) | scaleY( 〈number〉 ) | rotate( 〈angle〉 ) | skew( 〈angle〉 [ , 〈angle〉 ]? ) |
                skewX( 〈angle〉 ) |  skewY( 〈angle〉 )
〈color palette〉 = default | 〈name〉 〈color〉 [ , 〈name〉 〈color〉 ]*
```

The `-gtk-icon-source` property is used by widgets that are rendering
'built-in' icons, such as arrows, expanders, spinners, checks or radios.

The `-gtk-icon-style` property determines the preferred style for
application-provided icons.

The `-gtk-icon-transform` and `-gtk-icon-shadow` properties affect the
rendering of both built-in and application-provided icons.

`-gtk-icon-palette` defines a color palette for recoloring symbolic icons.
The recognized names for colors in symbolic icons are error, warning and
success. The default palette maps these three names to symbolic colors with
the names `error_color`, `warning_color` and `success_color` respectively.

### Box properties

| Name           | Value        | Initial | Inh. | Ani. | Reference | Notes |
|----------------|--------------|---------|------|------|-----------|-------|
| min-width      | `〈length〉` | 0px     |      | ✓    | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/visudet.html#propdef-min-width), [CSS3](https://www.w3.org/TR/css3-box/#min-width) | CSS allows percentages |
| min-height     | `〈length〉` | 0px     |      | ✓    | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/visudet.html#propdef-min-height), [CSS3](https://www.w3.org/TR/css3-box/#min-height) | CSS allows percentages |
| margin-top     | `〈length〉` | 0px     |      | ✓    | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/box.html#propdef-margin-top), [CSS3](https://www.w3.org/TR/css3-box/#margin-top) | CSS allows percentages or auto |
| margin-right   | `〈length〉` | 0px     |      | ✓    | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/box.html#propdef-margin-right), [CSS3](https://www.w3.org/TR/css3-box/#margin-right) | CSS allows percentages or auto |
| margin-bottom  | `〈length〉` | 0px     |      | ✓    | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/box.html#propdef-margin-bottom), [CSS3](https://www.w3.org/TR/css3-box/#margin-bottom) | CSS allows percentages or auto |
| margin-left    | `〈length〉` | 0px     |      | ✓    | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/box.html#propdef-margin-left), [CSS3](https://www.w3.org/TR/css3-box/#margin-left) | CSS allows percentages or auto |
| padding-top    | `〈length〉` | 0px     |      | ✓    | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/box.html#propdef-padding-top), [CSS3](https://www.w3.org/TR/css3-box/#padding-top) | CSS allows percentages |
| padding-right  | `〈length〉` | 0px     |      | ✓    | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/box.html#propdef-padding-right), [CSS3](https://www.w3.org/TR/css3-box/#padding-right) | CSS allows percentages |
| padding-bottom | `〈length〉` | 0px     |      | ✓    | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/box.html#propdef-padding-bottom), [CSS3](https://www.w3.org/TR/css3-box/#padding-bottom) | CSS allows percentages |
| padding-left   | `〈length〉` | 0px     |      | ✓    | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/box.html#propdef-padding-left), [CSS3](https://www.w3.org/TR/css3-box/#padding-left) | CSS allows percentages |

| Shorthand | Value           | Initial                   | Reference  | Notes                    |
|-----------|-----------------|---------------------------|------------|--------------------------|
| margin    | `〈length〉{1,4}` | see individual properties | CSS2, CSS3 | a 'four sides' shorthand |
| padding   | `〈length〉{1,4}` | see individual properties | CSS2, CSS3 | a 'four sides' shorthand |

### Border properties

| Name                       | Value | Initial | Inh. | Ani. | Reference | Notes |
|----------------------------|-------|---------|------|------|-----------|-------|
| border-top-width           | `〈length〉` | 0px |   | ✓ | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/box.html#propdef-border-top-width), [CSS3](https://www.w3.org/TR/css3-background/#the-border-width) | CSS allows other values |
| border-right-width         | `〈length〉` | 0px |   | ✓ | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/box.html#propdef-border-right-width), [CSS3](https://www.w3.org/TR/css3-background/#the-border-width) | CSS allows other values |
| border-bottom-width        | `〈length〉` | 0px |   | ✓ | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/box.html#propdef-border-bottom-width), [CSS3](https://www.w3.org/TR/css3-background/#the-border-width) | CSS allows other values |
| border-left-width          | `〈length〉` | 0px |   | ✓ | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/box.html#propdef-border-left-width), [CSS3](https://www.w3.org/TR/css3-background/#the-border-width) | CSS allows other values |
| border-top-style           | `〈border style〉` | `none` |   |   | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/box.html#propdef-border-top-style), [CSS3](https://www.w3.org/TR/css3-background/#the-border-style) |   |
| border-right-style         | `〈border style〉` | `none` |   |   | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/box.html#propdef-border-right-style), [CSS3](https://www.w3.org/TR/css3-background/#the-border-style) |   |
| border-bottom-style        | `〈border style〉` | `none` |   |   | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/box.html#propdef-border-bottom-style), [CSS3](https://www.w3.org/TR/css3-background/#the-border-style) |   |
| border-left-style          | `〈border style〉` | `none` |   |   | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/box.html#propdef-border-left-style), [CSS3](https://www.w3.org/TR/css3-background/#the-border-style) |   |
| border-top-right-radius    | `〈corner radius〉` | 0 |   | ✓ | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/box.html#propdef-border-top-right-radius), [CSS3](https://www.w3.org/TR/css3-background/#the-border-radius) |   |
| border-bottom-right-radius | `〈corner radius〉` | 0 |   | ✓ | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/box.html#propdef-border-bottom-right-radius), [CSS3](https://www.w3.org/TR/css3-background/#the-border-radius) |   |
| border-bottom-left-radius  | `〈corner radius〉` | 0 |   | ✓ | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/box.html#propdef-border-bottom-left-radius), [CSS3](https://www.w3.org/TR/css3-background/#the-border-radius) |   |
| border-top-left-radius     | `〈corner radius〉` | 0 |   | ✓ | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/box.html#propdef-border-top-left-radius), [CSS3](https://www.w3.org/TR/css3-background/#the-border-radius) |   |
| border-top-color           | `〈color〉` | `currentColor` |   | ✓ | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/box.html#propdef-border-top-color), [CSS3](https://www.w3.org/TR/css3-background/#the-border-color) |   |
| border-right-color         | `〈color〉` | `currentColor` |   | ✓ | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/box.html#propdef-border-right-color), [CSS3](https://www.w3.org/TR/css3-background/#the-border-color) |   |
| border-bottom-color        | `〈color〉` | `currentColor` |   | ✓ | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/box.html#propdef-border-bottom-color), [CSS3](https://www.w3.org/TR/css3-background/#the-border-color) |   |
| border-left-color          | `〈color〉` | `currentColor` |   | ✓ | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/box.html#propdef-border-left-color), [CSS3](https://www.w3.org/TR/css3-background/#the-border-color) |   |
| border-image-source        | `〈image〉 | none` | `none` |   | ✓ | [CSS3](https://www.w3.org/TR/css3-background/#the-border-image-source) |   |
| border-image-repeat        | `〈border repeat〉{1,2}` | `stretch` |   |   | [CSS3](https://www.w3.org/TR//css3-background/#the-border-image-repeat) |   |
| border-image-slice         | `[ 〈number〉 | 〈percentage〉 ]{1,4} && fill?` | 100% |   |   | [CSS3](https://www.w3.org/TR//css3-background/#the-border-image-slice) | a 'four sides' shorthand |
| border-image-width         | `[ 〈length〉 | 〈number〉 | 〈percentage〉 | auto ]{1,4}` | 1 |   |   | [CSS3](https://www.w3.org/TR//css3-background/#the-border-image-width) | a 'four sides' shorthand |

| Shorthand     | Value | Initial | Reference | Notes |
|---------------|-------|---------|-----------|-------|
| border-width  | `〈length〉{1,4}` | see individual properties | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/box.html#propdef-border-width), [CSS3](https://www.w3.org/TR/css3-background/#the-border-width) | a 'four sides' shorthand |
| border-style  | `〈border style〉{1,4}` | see individual properties | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/box.html#propdef-border-style), [CSS3](https://www.w3.org/TR/css3-background/#the-border-style) | a 'four sides' shorthand |
| border-color  | `〈color〉{1,4}` | see individual properties | [CSS3](https://www.w3.org/TR/css3-background/#border-color) | a 'four sides' shorthand |
| border-top    | `〈length〉 || 〈border style〉 || 〈color〉` | see individual properties | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/box.html#propdef-border-top), [CSS3](https://www.w3.org/TR/css3-background/#border-top) |   |
| border-right  | `〈length〉 || 〈border style〉 || 〈color〉` | see individual properties | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/box.html#propdef-border-right), [CSS3](https://www.w3.org/TR/css3-background/#border-right) |   |
| border-bottom | `〈length〉 || 〈border style〉 || 〈color〉` | see individual properties | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/box.html#propdef-border-bottom), [CSS3](https://www.w3.org/TR/css3-background/#border-bottom) |   |
| border-left   | `〈length〉 || 〈border style〉 || 〈color〉` | see individual properties | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/box.html#propdef-border-left), [CSS3](https://www.w3.org/TR/css3-background/#border-left) |   |
| border        | `〈length〉 || 〈border style〉 || 〈color〉` | see individual properties | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/box.html#propdef-border), [CSS3](https://www.w3.org/TR/css3-background/#border) |   |
| border-radius | `[ 〈length〉 | 〈percentage〉 ]{1,4} [ / [ 〈length〉 | 〈percentage〉 ]{1,4} ]?` | see individual properties | [CSS3](https://www.w3.org/TR/css3-background/#border-radius) |   |
| border-image  | `〈border-image-source〉 || 〈border-image-slice〉 [ / 〈border-image-width〉 | / 〈border-image-width〉? / 〈border-image-outset〉 ]? || 〈border-image-repeat〉` | see individual properties | [CSS3](https://www.w3.org/TR/css3-background/#border-image) |   |

```
〈border style〉 = none | solid | inset | outset | hidden | dotted | dashed | double | groove | ridge
〈corner radius〉 = [ 〈length〉 | 〈percentage〉 ]{1,2}
```

### Outline properties

| Name                             | Value | Initial | Inh. | Ani. | Reference | Notes |
|----------------------------------|-------|---------|------|------|-----------|-------|
| outline-style                    | `none | solid | inset | outset | hidden | dotted | dashed | double | groove | ridge` | `none` |   |   | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/ui.html#propdef-outline-style), [CSS3](https://www.w3.org/TR/css3-ui/#outline-style) |   |
| outline-width                    | `〈length〉` | 0px |   | ✓ | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/ui.html#propdef-outline-width), [CSS3](https://www.w3.org/TR/css3-ui/#outline-width) |   |
| outline-color                    | `〈color〉` | `currentColor` |   | ✓ | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/ui.html#propdef-outline-color), [CSS3](https://www.w3.org/TR/css3-ui/#outline-color) |   |
| outline-offset                   | `〈length〉` | 0px |   |   | [CSS3](https://www.w3.org/TR/css3-ui/#outline-offset) |   |
| -gtk-outline-top-left-radius     | `〈corner radius〉` | 0 |   | ✓ |   |   |
| -gtk-outline-top-right-radius    | `〈corner radius〉` | 0 |   | ✓ |   |   |
| -gtk-outline-bottom-right-radius | `〈corner radius〉` | 0 |   | ✓ |   |   |
| -gtk-outline-bottom-left-radius  | `〈corner radius〉` | 0 |   | ✓ |   |   |

| Shorthand           | Value | Initial | Reference | Notes |
|---------------------|-------|---------|-----------|-------|
| outline             | `〈outline-color〉 || 〈outline-style〉 || 〈outline-width〉` | see individual properties | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/ui.html#propdef-outline), [CSS3](https://www.w3.org/TR/css3-ui/#propdef-outline) |   |
| -gtk-outline-radius | `[ 〈length〉 | 〈percentage〉 ]{1,4} [ / [ 〈length〉 | 〈percentage〉 ]{1,4} ]?` | see individual properties |   |   |

GTK uses the CSS outline properties to render the 'focus rectangle'.

### Background properties

| Name                  | Value                                         | Initial         | Inh. | Ani. | Reference | Notes |
|-----------------------|-----------------------------------------------|-----------------|------|------|-----------|-------|
| background-color      | `〈color〉`                                   | `rgba(0,0,0,0)` |      | ✓    | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/colors.html#propdef-background-color), [CSS3](https://www.w3.org/TR/css3-background/#background-color) |   |
| background-clip       | `〈box〉 [ , 〈box〉 ]*`                      | `border-box`    |      |      | [CSS3](https://www.w3.org/TR/css3-background/#background-clip) |   |
| background-origin     | `〈box〉 [ , 〈box〉 ]*`                      | `padding-box`   |      |      | [CSS3](https://www.w3.org/TR/css3-background/#background-origin) |   |
| background-size       | `〈bg-size〉 [ , 〈bg-size〉 ]*`              | `auto`          |      | ✓    | [CSS3](https://www.w3.org/TR/css3-background/#background-size) |   |
| background-position   | `〈position〉 [ , 〈position〉 ]*`            | 0               |      | ✓    | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/colors.html#propdef-background-position), [CSS3](https://www.w3.org/TR/css3-background/#background-position) |   |
| background-repeat     | `〈bg-repeat〉 [ , 〈bg-repeat〉 ]*`          | `repeat`        |      |      | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/colors.html#propdef-background-repeat), [CSS3](https://www.w3.org/TR/css3-background/#background-repeat) |   |
| background-image      | `〈bg-image〉 [ , 〈bg-image〉 ]*`            | `none`          |      | ✓    | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/colors.html#propdef-background-image), [CSS3](https://www.w3.org/TR/css3-background/#background-image) | not supported: urls without quotes, CSS radial gradients, colors in crossfades |
| background-blend-mode | `〈blend-mode〉 [ , 〈blend-mode〉 ]*`        | `normal`        |      |      |           | only affects multiple backgrounds |
| box-shadow            | `none | 〈box shadow〉 [ , 〈box shadow〉 ]*` | `none`          |      | ✓    | [CSS3](https://www.w3.org/TR/css3-background/#box-shadow) |   |

| Shorthand  | Value                                    | Initial                   | Reference | Notes |
|------------|------------------------------------------|---------------------------|-----------|-------|
| background | `[ 〈bg-layer〉 , ]* 〈final-bg-layer〉` | see individual properties | [CSS2](https://www.w3.org/TR/2011/REC-CSS2-20110607/colors.html#propdef-background), [CSS3](https://www.w3.org/TR/css3-background/#background) |   |

```
〈box〉 = border-box | padding-box | content-box
〈bg-size〉 = [ 〈length〉 | 〈percentage〉 | auto ]{1,2} | cover | contain
〈position〉 = [ left | right | center | top | bottom | 〈percentage〉 | 〈length〉 ]{1,2,3,4}
〈bg-repeat〉 = repeat-x | repeat-y | [ no-repeat | repeat | round | space ]{1,2}
〈bg-image〉 = 〈image〉 | none
〈bg-layer〉 = 〈bg-image〉 || 〈position〉 [ / 〈bg-size〉 ]? || 〈bg-repeat〉 || 〈box〉 || 〈box〉
〈final-bg-layer〉 = 〈bg-image〉 || 〈position〉 [ / 〈bg-size〉 ]? || 〈bg-repeat〉 || 〈box〉 || 〈box〉 || 〈color〉
〈blend-mode〉 = color || color-burn || color-dodge || darken || difference || exclusion ||
               hard-light || hue || lighten || luminosity || multiply || normal || overlay ||
               saturate || screen || soft-light
〈box shadow〉 = inset? && 〈length〉{2,4}? && 〈color〉?
```

As in CSS, the background color is rendered underneath all the background
image layers, so it will only be visible if background images are absent or
have transparency.

Alternatively, multiple backgrounds can be blended using the
background-blend-mode property.

### Transition properties

| Name                       | Value | Initial | Inh. | Ani. | Reference | Notes |
|----------------------------|-------|---------|------|------|-----------|-------|
| transition-property        | `none | all | 〈property name〉 [ , 〈property name〉 ]*` | `all` |   |   | [CSS3](https://www.w3.org/TR/css3-transitions/#transition-property) |   |
| transition-duration        | `〈time〉 [ , 〈time〉 ]*` | 0s |   |   | [CSS3](https://www.w3.org/TR/css3-transitions/#transition-duration) |   |
| transition-timing-function | `〈single-timing-function〉 [ , 〈single-timing-function〉 ]*` | `ease` |   |   | [CSS3](https://www.w3.org/TR/css3-transitions/#transition-timing-function) |   |
| transition-delay           | `〈time〉 [ , 〈time〉 ]*` | 0s |   |   | [CSS3](https://www.w3.org/TR/css3-transitions/#transition-delay) |   |

| Shorthand  | Value                                                | Initial                   | Reference | Notes |
|------------|------------------------------------------------------|---------------------------|-----------|-------|
| transition | `〈single-transition〉 [ , 〈single-transition〉 ]*` | see individual properties | [CSS3](https://www.w3.org/TR/css3-transitions/#transition) |   |

```
〈single-timing-function〉 = ease | linear | ease-in | ease-out | ease-in-out |
                           step-start | step-end | steps( 〈integer〉 [ , [ start | end ] ]? ) |
                           cubic-bezier( 〈number〉, 〈number〉, 〈number〉, 〈number〉 )
〈single-transition〉 = [ none | 〈property name〉 ] || 〈time〉 || 〈single-transition-timing-function〉 || 〈time〉
```

### Animation properties

| Name                      | Value | Initial | Inh. | Ani. | Reference | Notes |
|---------------------------|-------|---------|------|------|-----------|-------|
| animation-name            | `〈single-animation-name〉 [ , 〈single-animation-name〉 ]*` | `none` |   |   | [CSS3](https://www.w3.org/TR/css3-animations/#animation-name) |   |
| animation-duration        | `〈time〉 [ , 〈time〉 ]*` | 0s |   |   | [CSS3](https://www.w3.org/TR/css3-animations/#animation-duration) |   |
| animation-timing-function | `〈single-timing-function〉 [ , 〈single-timing-function〉 ]*` | `ease` |   |   | [CSS3](https://www.w3.org/TR/css3-animations/#animation-timing-function) |   |
| animation-iteration-count | `〈single-animation-iteration-count〉 [ , 〈single-animation-iteration-count〉 ]*` | 1 |   |   | [CSS3](https://www.w3.org/TR/css3-animations/#animation-iteration-count) |   |
| animation-direction       | `〈single-animation-direction〉 [ , 〈single-animation-direction〉 ]*` | `normal` |   |   | [CSS3](https://www.w3.org/TR/css3-animations/#animation-direction) |   |
| animation-play-state      | `〈single-animation-play-state〉 [ , 〈single-animation-play-state〉 ]*` | `running` |   |   | [CSS3](https://www.w3.org/TR/css3-animations/#animation-play-state) |   |
| animation-delay           | `〈time〉 [ , 〈time〉 ]*` | 0s |   |   | [CSS3](https://www.w3.org/TR/css3-animations/#animation-delay) |   |
| animation-fill-mode       | `〈single-animation-fill-mode〉 [ , 〈single-animation-fill-mode〉 ]*` | `none` |   |   | [CSS3](https://www.w3.org/TR/css3-animations/#animation-fill-mode) |   |

| Shorthand | Value                                              | Initial                   | Reference | Notes |
|-----------|----------------------------------------------------|---------------------------|-----------|-------|
| animation | `〈single-animation〉 [ , 〈single-animation〉 ]*` | see individual properties | [CSS3](https://www.w3.org/TR/css3-animations/#animation) |   |

```
〈single-animation-name〉 = none | 〈property name〉
〈single-animation-iteration-count〉 = infinite | 〈number〉
〈single-animation-direction〉 = normal | reverse | alternate | alternate-reverse
〈single-animation-play-state〉 = running | paused
〈single-animation-fill-mode〉 = none | forwards | backwards | both
〈single-animation〉 = 〈single-animation-name〉 || 〈time〉 || 〈single-timing-function〉 || 〈time〉 ||
                     〈single-animation-iteration-count〉 || 〈single-animation-direction〉 ||
                     〈single-animation-play-state〉 || 〈single-animation-fill-mode〉
```

### Key binding properties

| Name              | Value                                             | Initial | Inh. | Ani. | Reference | Notes |
|-------------------|---------------------------------------------------|---------|------|------|-----------|-------|
| -gtk-key-bindings | `none | 〈binding name〉 [ , 〈binding name〉 ]*` | `none`  |      |      |           |       |

`〈binding name〉` must have been assigned to a binding set with a `binding-set` rule. 
