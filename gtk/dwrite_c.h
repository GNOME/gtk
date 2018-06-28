/*
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the mingw-w64 runtime package.
 * No warranty is given.
 */
/*
 * Stripped version, based on the Public Domain version provided in libass.
 * Only definitions needed by GTK+. Contains fixes to
 * make it compile with C. Also needed on MSVC.
 */
#ifndef __INC_DWRITE__
#define __INC_DWRITE__

#define DWRITEAPI DECLSPEC_IMPORT

#include <unknwn.h>

typedef struct IDWriteFactory IDWriteFactory;
typedef struct IDWriteFont IDWriteFont;
typedef struct IDWriteFontCollection IDWriteFontCollection;
typedef struct IDWriteFontFace IDWriteFontFace;
typedef struct IDWriteFontFamily IDWriteFontFamily;
typedef struct IDWriteFontList IDWriteFontList;
typedef struct IDWriteFontFile IDWriteFontFile;
typedef struct IDWriteFontFileLoader IDWriteFontFileLoader;
typedef struct IDWriteFontFileStream IDWriteFontFileStream;
typedef struct IDWriteGdiInterop IDWriteGdiInterop;
typedef struct IDWriteInlineObject IDWriteInlineObject;
typedef struct IDWriteLocalizedStrings IDWriteLocalizedStrings;
typedef struct IDWritePixelSnapping IDWritePixelSnapping;
typedef struct IDWriteTextFormat IDWriteTextFormat;
typedef struct IDWriteTextLayout IDWriteTextLayout;
typedef struct IDWriteTextRenderer IDWriteTextRenderer;

#include <dcommon.h>

typedef enum DWRITE_INFORMATIONAL_STRING_ID {
  DWRITE_INFORMATIONAL_STRING_NONE = 0,
  DWRITE_INFORMATIONAL_STRING_COPYRIGHT_NOTICE,
  DWRITE_INFORMATIONAL_STRING_VERSION_STRINGS,
  DWRITE_INFORMATIONAL_STRING_TRADEMARK,
  DWRITE_INFORMATIONAL_STRING_MANUFACTURER,
  DWRITE_INFORMATIONAL_STRING_DESIGNER,
  DWRITE_INFORMATIONAL_STRING_DESIGNER_URL,
  DWRITE_INFORMATIONAL_STRING_DESCRIPTION,
  DWRITE_INFORMATIONAL_STRING_FONT_VENDOR_URL,
  DWRITE_INFORMATIONAL_STRING_LICENSE_DESCRIPTION,
  DWRITE_INFORMATIONAL_STRING_LICENSE_INFO_URL,
  DWRITE_INFORMATIONAL_STRING_WIN32_FAMILY_NAMES,
  DWRITE_INFORMATIONAL_STRING_WIN32_SUBFAMILY_NAMES,
  DWRITE_INFORMATIONAL_STRING_PREFERRED_FAMILY_NAMES,
  DWRITE_INFORMATIONAL_STRING_PREFERRED_SUBFAMILY_NAMES,
  DWRITE_INFORMATIONAL_STRING_SAMPLE_TEXT,
  DWRITE_INFORMATIONAL_STRING_FULL_NAME,
  DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_NAME,
  DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_CID_NAME,
  DWRITE_INFORMATIONAL_STRING_WEIGHT_STRETCH_STYLE_FAMILY_NAME,
  DWRITE_INFORMATIONAL_STRING_DESIGN_SCRIPT_LANGUAGE_TAG,
  DWRITE_INFORMATIONAL_STRING_SUPPORTED_SCRIPT_LANGUAGE_TAG,
} DWRITE_INFORMATIONAL_STRING_ID;

typedef enum DWRITE_FACTORY_TYPE {
  DWRITE_FACTORY_TYPE_SHARED = 0,
  DWRITE_FACTORY_TYPE_ISOLATED 
} DWRITE_FACTORY_TYPE;

typedef enum DWRITE_FONT_FACE_TYPE {
  DWRITE_FONT_FACE_TYPE_CFF = 0,
  DWRITE_FONT_FACE_TYPE_TRUETYPE,
  DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION,
  DWRITE_FONT_FACE_TYPE_TYPE1,
  DWRITE_FONT_FACE_TYPE_VECTOR,
  DWRITE_FONT_FACE_TYPE_BITMAP,
  DWRITE_FONT_FACE_TYPE_UNKNOWN,
  DWRITE_FONT_FACE_TYPE_RAW_CFF
} DWRITE_FONT_FACE_TYPE;

typedef enum DWRITE_FONT_SIMULATIONS {
  DWRITE_FONT_SIMULATIONS_NONE      = 0x0000,
  DWRITE_FONT_SIMULATIONS_BOLD      = 0x0001,
  DWRITE_FONT_SIMULATIONS_OBLIQUE   = 0x0002 
} DWRITE_FONT_SIMULATIONS;

typedef enum DWRITE_FONT_STRETCH {
  DWRITE_FONT_STRETCH_UNDEFINED         = 0,
  DWRITE_FONT_STRETCH_ULTRA_CONDENSED   = 1,
  DWRITE_FONT_STRETCH_EXTRA_CONDENSED   = 2,
  DWRITE_FONT_STRETCH_CONDENSED         = 3,
  DWRITE_FONT_STRETCH_SEMI_CONDENSED    = 4,
  DWRITE_FONT_STRETCH_NORMAL            = 5,
  DWRITE_FONT_STRETCH_MEDIUM            = 5,
  DWRITE_FONT_STRETCH_SEMI_EXPANDED     = 6,
  DWRITE_FONT_STRETCH_EXPANDED          = 7,
  DWRITE_FONT_STRETCH_EXTRA_EXPANDED    = 8,
  DWRITE_FONT_STRETCH_ULTRA_EXPANDED    = 9 
} DWRITE_FONT_STRETCH;

typedef enum DWRITE_FONT_STYLE {
  DWRITE_FONT_STYLE_NORMAL = 0,
  DWRITE_FONT_STYLE_OBLIQUE,
  DWRITE_FONT_STYLE_ITALIC 
} DWRITE_FONT_STYLE;

typedef enum DWRITE_FONT_WEIGHT {
  DWRITE_FONT_WEIGHT_MEDIUM        = 500,
  /* rest dropped */
} DWRITE_FONT_WEIGHT;

#define DWRITE_MAKE_OPENTYPE_TAG(a,b,c,d) \
    (((UINT32)((UINT8)d) << 24) | \
     ((UINT32)((UINT8)c) << 16) | \
     ((UINT32)((UINT8)b) << 8)  | \
     ((UINT32)((UINT8)a)))

typedef enum DWRITE_FONT_FEATURE_TAG
{
    DWRITE_FONT_FEATURE_TAG_ALTERNATIVE_FRACTIONS               = DWRITE_MAKE_OPENTYPE_TAG('a','f','r','c'),
    DWRITE_FONT_FEATURE_TAG_PETITE_CAPITALS_FROM_CAPITALS       = DWRITE_MAKE_OPENTYPE_TAG('c','2','p','c'),
    DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS_FROM_CAPITALS        = DWRITE_MAKE_OPENTYPE_TAG('c','2','s','c'),
    DWRITE_FONT_FEATURE_TAG_CONTEXTUAL_ALTERNATES               = DWRITE_MAKE_OPENTYPE_TAG('c','a','l','t'),
    DWRITE_FONT_FEATURE_TAG_CASE_SENSITIVE_FORMS                = DWRITE_MAKE_OPENTYPE_TAG('c','a','s','e'),
    DWRITE_FONT_FEATURE_TAG_GLYPH_COMPOSITION_DECOMPOSITION     = DWRITE_MAKE_OPENTYPE_TAG('c','c','m','p'),
    DWRITE_FONT_FEATURE_TAG_CONTEXTUAL_LIGATURES                = DWRITE_MAKE_OPENTYPE_TAG('c','l','i','g'),
    DWRITE_FONT_FEATURE_TAG_CAPITAL_SPACING                     = DWRITE_MAKE_OPENTYPE_TAG('c','p','s','p'),
    DWRITE_FONT_FEATURE_TAG_CONTEXTUAL_SWASH                    = DWRITE_MAKE_OPENTYPE_TAG('c','s','w','h'),
    DWRITE_FONT_FEATURE_TAG_CURSIVE_POSITIONING                 = DWRITE_MAKE_OPENTYPE_TAG('c','u','r','s'),
    DWRITE_FONT_FEATURE_TAG_DEFAULT                             = DWRITE_MAKE_OPENTYPE_TAG('d','f','l','t'),
    DWRITE_FONT_FEATURE_TAG_DISCRETIONARY_LIGATURES             = DWRITE_MAKE_OPENTYPE_TAG('d','l','i','g'),
    DWRITE_FONT_FEATURE_TAG_EXPERT_FORMS                        = DWRITE_MAKE_OPENTYPE_TAG('e','x','p','t'),
    DWRITE_FONT_FEATURE_TAG_FRACTIONS                           = DWRITE_MAKE_OPENTYPE_TAG('f','r','a','c'),
    DWRITE_FONT_FEATURE_TAG_FULL_WIDTH                          = DWRITE_MAKE_OPENTYPE_TAG('f','w','i','d'),
    DWRITE_FONT_FEATURE_TAG_HALF_FORMS                          = DWRITE_MAKE_OPENTYPE_TAG('h','a','l','f'),
    DWRITE_FONT_FEATURE_TAG_HALANT_FORMS                        = DWRITE_MAKE_OPENTYPE_TAG('h','a','l','n'),
    DWRITE_FONT_FEATURE_TAG_ALTERNATE_HALF_WIDTH                = DWRITE_MAKE_OPENTYPE_TAG('h','a','l','t'),
    DWRITE_FONT_FEATURE_TAG_HISTORICAL_FORMS                    = DWRITE_MAKE_OPENTYPE_TAG('h','i','s','t'),
    DWRITE_FONT_FEATURE_TAG_HORIZONTAL_KANA_ALTERNATES          = DWRITE_MAKE_OPENTYPE_TAG('h','k','n','a'),
    DWRITE_FONT_FEATURE_TAG_HISTORICAL_LIGATURES                = DWRITE_MAKE_OPENTYPE_TAG('h','l','i','g'),
    DWRITE_FONT_FEATURE_TAG_HALF_WIDTH                          = DWRITE_MAKE_OPENTYPE_TAG('h','w','i','d'),
    DWRITE_FONT_FEATURE_TAG_HOJO_KANJI_FORMS                    = DWRITE_MAKE_OPENTYPE_TAG('h','o','j','o'),
    DWRITE_FONT_FEATURE_TAG_JIS04_FORMS                         = DWRITE_MAKE_OPENTYPE_TAG('j','p','0','4'),
    DWRITE_FONT_FEATURE_TAG_JIS78_FORMS                         = DWRITE_MAKE_OPENTYPE_TAG('j','p','7','8'),
    DWRITE_FONT_FEATURE_TAG_JIS83_FORMS                         = DWRITE_MAKE_OPENTYPE_TAG('j','p','8','3'),
    DWRITE_FONT_FEATURE_TAG_JIS90_FORMS                         = DWRITE_MAKE_OPENTYPE_TAG('j','p','9','0'),
    DWRITE_FONT_FEATURE_TAG_KERNING                             = DWRITE_MAKE_OPENTYPE_TAG('k','e','r','n'),
    DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES                  = DWRITE_MAKE_OPENTYPE_TAG('l','i','g','a'),
    DWRITE_FONT_FEATURE_TAG_LINING_FIGURES                      = DWRITE_MAKE_OPENTYPE_TAG('l','n','u','m'),
    DWRITE_FONT_FEATURE_TAG_LOCALIZED_FORMS                     = DWRITE_MAKE_OPENTYPE_TAG('l','o','c','l'),
    DWRITE_FONT_FEATURE_TAG_MARK_POSITIONING                    = DWRITE_MAKE_OPENTYPE_TAG('m','a','r','k'),
    DWRITE_FONT_FEATURE_TAG_MATHEMATICAL_GREEK                  = DWRITE_MAKE_OPENTYPE_TAG('m','g','r','k'),
    DWRITE_FONT_FEATURE_TAG_MARK_TO_MARK_POSITIONING            = DWRITE_MAKE_OPENTYPE_TAG('m','k','m','k'),
    DWRITE_FONT_FEATURE_TAG_ALTERNATE_ANNOTATION_FORMS          = DWRITE_MAKE_OPENTYPE_TAG('n','a','l','t'),
    DWRITE_FONT_FEATURE_TAG_NLC_KANJI_FORMS                     = DWRITE_MAKE_OPENTYPE_TAG('n','l','c','k'),
    DWRITE_FONT_FEATURE_TAG_OLD_STYLE_FIGURES                   = DWRITE_MAKE_OPENTYPE_TAG('o','n','u','m'),
    DWRITE_FONT_FEATURE_TAG_ORDINALS                            = DWRITE_MAKE_OPENTYPE_TAG('o','r','d','n'),
    DWRITE_FONT_FEATURE_TAG_PROPORTIONAL_ALTERNATE_WIDTH        = DWRITE_MAKE_OPENTYPE_TAG('p','a','l','t'),
    DWRITE_FONT_FEATURE_TAG_PETITE_CAPITALS                     = DWRITE_MAKE_OPENTYPE_TAG('p','c','a','p'),
    DWRITE_FONT_FEATURE_TAG_PROPORTIONAL_FIGURES                = DWRITE_MAKE_OPENTYPE_TAG('p','n','u','m'),
    DWRITE_FONT_FEATURE_TAG_PROPORTIONAL_WIDTHS                 = DWRITE_MAKE_OPENTYPE_TAG('p','w','i','d'),
    DWRITE_FONT_FEATURE_TAG_QUARTER_WIDTHS                      = DWRITE_MAKE_OPENTYPE_TAG('q','w','i','d'),
    DWRITE_FONT_FEATURE_TAG_REQUIRED_LIGATURES                  = DWRITE_MAKE_OPENTYPE_TAG('r','l','i','g'),
    DWRITE_FONT_FEATURE_TAG_RUBY_NOTATION_FORMS                 = DWRITE_MAKE_OPENTYPE_TAG('r','u','b','y'),
    DWRITE_FONT_FEATURE_TAG_STYLISTIC_ALTERNATES                = DWRITE_MAKE_OPENTYPE_TAG('s','a','l','t'),
    DWRITE_FONT_FEATURE_TAG_SCIENTIFIC_INFERIORS                = DWRITE_MAKE_OPENTYPE_TAG('s','i','n','f'),
    DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS                      = DWRITE_MAKE_OPENTYPE_TAG('s','m','c','p'),
    DWRITE_FONT_FEATURE_TAG_SIMPLIFIED_FORMS                    = DWRITE_MAKE_OPENTYPE_TAG('s','m','p','l'),
    DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_1                     = DWRITE_MAKE_OPENTYPE_TAG('s','s','0','1'),
    DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_2                     = DWRITE_MAKE_OPENTYPE_TAG('s','s','0','2'),
    DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_3                     = DWRITE_MAKE_OPENTYPE_TAG('s','s','0','3'),
    DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_4                     = DWRITE_MAKE_OPENTYPE_TAG('s','s','0','4'),
    DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_5                     = DWRITE_MAKE_OPENTYPE_TAG('s','s','0','5'),
    DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_6                     = DWRITE_MAKE_OPENTYPE_TAG('s','s','0','6'),
    DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_7                     = DWRITE_MAKE_OPENTYPE_TAG('s','s','0','7'),
    DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_8                     = DWRITE_MAKE_OPENTYPE_TAG('s','s','0','8'),
    DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_9                     = DWRITE_MAKE_OPENTYPE_TAG('s','s','0','9'),
    DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_10                    = DWRITE_MAKE_OPENTYPE_TAG('s','s','1','0'),
    DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_11                    = DWRITE_MAKE_OPENTYPE_TAG('s','s','1','1'),
    DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_12                    = DWRITE_MAKE_OPENTYPE_TAG('s','s','1','2'),
    DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_13                    = DWRITE_MAKE_OPENTYPE_TAG('s','s','1','3'),
    DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_14                    = DWRITE_MAKE_OPENTYPE_TAG('s','s','1','4'),
    DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_15                    = DWRITE_MAKE_OPENTYPE_TAG('s','s','1','5'),
    DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_16                    = DWRITE_MAKE_OPENTYPE_TAG('s','s','1','6'),
    DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_17                    = DWRITE_MAKE_OPENTYPE_TAG('s','s','1','7'),
    DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_18                    = DWRITE_MAKE_OPENTYPE_TAG('s','s','1','8'),
    DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_19                    = DWRITE_MAKE_OPENTYPE_TAG('s','s','1','9'),
    DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_20                    = DWRITE_MAKE_OPENTYPE_TAG('s','s','2','0'),
    DWRITE_FONT_FEATURE_TAG_SUBSCRIPT                           = DWRITE_MAKE_OPENTYPE_TAG('s','u','b','s'),
    DWRITE_FONT_FEATURE_TAG_SUPERSCRIPT                         = DWRITE_MAKE_OPENTYPE_TAG('s','u','p','s'),
    DWRITE_FONT_FEATURE_TAG_SWASH                               = DWRITE_MAKE_OPENTYPE_TAG('s','w','s','h'),
    DWRITE_FONT_FEATURE_TAG_TITLING                             = DWRITE_MAKE_OPENTYPE_TAG('t','i','t','l'),
    DWRITE_FONT_FEATURE_TAG_TRADITIONAL_NAME_FORMS              = DWRITE_MAKE_OPENTYPE_TAG('t','n','a','m'),
    DWRITE_FONT_FEATURE_TAG_TABULAR_FIGURES                     = DWRITE_MAKE_OPENTYPE_TAG('t','n','u','m'),
    DWRITE_FONT_FEATURE_TAG_TRADITIONAL_FORMS                   = DWRITE_MAKE_OPENTYPE_TAG('t','r','a','d'),
    DWRITE_FONT_FEATURE_TAG_THIRD_WIDTHS                        = DWRITE_MAKE_OPENTYPE_TAG('t','w','i','d'),
    DWRITE_FONT_FEATURE_TAG_UNICASE                             = DWRITE_MAKE_OPENTYPE_TAG('u','n','i','c'),
    DWRITE_FONT_FEATURE_TAG_VERTICAL_WRITING                    = DWRITE_MAKE_OPENTYPE_TAG('v','e','r','t'),
    DWRITE_FONT_FEATURE_TAG_VERTICAL_ALTERNATES_AND_ROTATION    = DWRITE_MAKE_OPENTYPE_TAG('v','r','t','2'),
    DWRITE_FONT_FEATURE_TAG_SLASHED_ZERO                        = DWRITE_MAKE_OPENTYPE_TAG('z','e','r','o'),
} DWRITE_FONT_FEATURE_TAG;

typedef struct DWRITE_FONT_METRICS {
  UINT16 designUnitsPerEm;
  UINT16 ascent;
  UINT16 descent;
  INT16  lineGap;
  UINT16 capHeight;
  UINT16 xHeight;
  INT16  underlinePosition;
  UINT16 underlineThickness;
  INT16  strikethroughPosition;
  UINT16 strikethroughThickness;
} DWRITE_FONT_METRICS;

typedef struct DWRITE_GLYPH_OFFSET DWRITE_GLYPH_OFFSET;

typedef struct DWRITE_GLYPH_RUN {
  IDWriteFontFace           *fontFace;
  FLOAT                     fontEmSize;
  UINT32                    glyphCount;
  const UINT16              *glyphIndices;
  const FLOAT               *glyphAdvances;
  const DWRITE_GLYPH_OFFSET *glyphOffsets;
  BOOL                      isSideways;
  UINT32                    bidiLevel;
} DWRITE_GLYPH_RUN;

typedef struct DWRITE_GLYPH_RUN_DESCRIPTION DWRITE_GLYPH_RUN_DESCRIPTION;
typedef struct DWRITE_HIT_TEST_METRICS DWRITE_HIT_TEST_METRICS;
typedef struct DWRITE_LINE_METRICS DWRITE_LINE_METRICS;
typedef struct DWRITE_MATRIX DWRITE_MATRIX;
typedef struct DWRITE_STRIKETHROUGH DWRITE_STRIKETHROUGH;
typedef struct DWRITE_TEXT_METRICS DWRITE_TEXT_METRICS;

typedef struct DWRITE_TEXT_RANGE {
  UINT32 startPosition;
  UINT32 length;
} DWRITE_TEXT_RANGE;

typedef struct DWRITE_TRIMMING DWRITE_TRIMMING;
typedef struct DWRITE_UNDERLINE DWRITE_UNDERLINE;

#ifndef __MINGW_DEF_ARG_VAL
#ifdef __cplusplus
#define __MINGW_DEF_ARG_VAL(x) = x
#else
#define __MINGW_DEF_ARG_VAL(x)
#endif
#endif

#undef  INTERFACE
#define INTERFACE IDWriteFactory
DECLARE_INTERFACE_(IDWriteFactory,IUnknown)
{
    BEGIN_INTERFACE

#ifndef __cplusplus
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void **ppvObject) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;
#endif

    /* IDWriteFactory methods */
    STDMETHOD(GetSystemFontCollection)(THIS_
        IDWriteFontCollection **fontCollection,
        BOOL checkForUpdates __MINGW_DEF_ARG_VAL(FALSE)) PURE;

    STDMETHOD(dummy1)(THIS);
    STDMETHOD(dummy2)(THIS);
    STDMETHOD(dummy3)(THIS);
    STDMETHOD(dummy4)(THIS);
    STDMETHOD(dummy5)(THIS);
    STDMETHOD(dummy6)(THIS);
    STDMETHOD(dummy7)(THIS);
    STDMETHOD(dummy8)(THIS);
    STDMETHOD(dummy9)(THIS);
    STDMETHOD(dummy10)(THIS);
    STDMETHOD(dummy11)(THIS);

    STDMETHOD(CreateTextFormat)(THIS_
        WCHAR const *fontFamilyName,
        IDWriteFontCollection *fontCollection,
        DWRITE_FONT_WEIGHT fontWeight,
        DWRITE_FONT_STYLE fontStyle,
        DWRITE_FONT_STRETCH fontStretch,
        FLOAT fontSize,
        WCHAR const *localeName,
        IDWriteTextFormat **textFormat) PURE;

    STDMETHOD(dummy12)(THIS);
    STDMETHOD(GetGdiInterop)(THIS_
        IDWriteGdiInterop **gdiInterop) PURE;

    STDMETHOD(CreateTextLayout)(THIS_
        WCHAR const *string,
        UINT32 stringLength,
        IDWriteTextFormat *textFormat,
        FLOAT maxWidth,
        FLOAT maxHeight,
        IDWriteTextLayout **textLayout) PURE;

    /* remainder dropped */
    END_INTERFACE
};
#ifdef COBJMACROS
#define IDWriteFactory_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IDWriteFactory_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IDWriteFactory_Release(This) (This)->lpVtbl->Release(This)
#define IDWriteFactory_GetSystemFontCollection(This,fontCollection,checkForUpdates) (This)->lpVtbl->GetSystemFontCollection(This,fontCollection,checkForUpdates)
#define IDWriteFactory_CreateTextFormat(This,fontFamilyName,fontCollection,fontWeight,fontStyle,fontStretch,fontSize,localeName,textFormat) (This)->lpVtbl->CreateTextFormat(This,fontFamilyName,fontCollection,fontWeight,fontStyle,fontStretch,fontSize,localeName,textFormat)
#define IDWriteFactory_GetGdiInterop(This,gdiInterop) (This)->lpVtbl->GetGdiInterop(This,gdiInterop)
#define IDWriteFactory_CreateTextLayout(This,string,stringLength,textFormat,maxWidth,maxHeight,textLayout) (This)->lpVtbl->CreateTextLayout(This,string,stringLength,textFormat,maxWidth,maxHeight,textLayout)
#endif /*COBJMACROS*/

#undef  INTERFACE
#define INTERFACE IDWriteFont
DECLARE_INTERFACE_(IDWriteFont,IUnknown)
{
    BEGIN_INTERFACE

#ifndef __cplusplus
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void **ppvObject) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;
#endif

    /* IDWriteFont methods */
    STDMETHOD(GetFontFamily)(THIS_
        IDWriteFontFamily **fontFamily) PURE;

    STDMETHOD_(DWRITE_FONT_WEIGHT, GetWeight)(THIS) PURE;
    STDMETHOD_(DWRITE_FONT_STRETCH, GetStretch)(THIS) PURE;
    STDMETHOD_(DWRITE_FONT_STYLE, GetStyle)(THIS) PURE;
    STDMETHOD_(BOOL, IsSymbolFont)(THIS) PURE;

    STDMETHOD(GetFaceNames)(THIS_
        IDWriteLocalizedStrings **names) PURE;

    STDMETHOD(GetInformationalStrings)(THIS_
        DWRITE_INFORMATIONAL_STRING_ID informationalStringID,
        IDWriteLocalizedStrings **informationalStrings,
        BOOL *exists) PURE;

    STDMETHOD_(DWRITE_FONT_SIMULATIONS, GetSimulations)(THIS) PURE;

    STDMETHOD_(void, GetMetrics)(THIS_
        DWRITE_FONT_METRICS *fontMetrics) PURE;

    STDMETHOD(HasCharacter)(THIS_
        UINT32 unicodeValue,
        BOOL *exists) PURE;

    STDMETHOD(CreateFontFace)(THIS_
        IDWriteFontFace **fontFace) PURE;

    END_INTERFACE
};
#ifdef COBJMACROS
#define IDWriteFont_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IDWriteFont_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IDWriteFont_Release(This) (This)->lpVtbl->Release(This)
#define IDWriteFont_CreateFontFace(This,fontFace) (This)->lpVtbl->CreateFontFace(This,fontFace)
#define IDWriteFont_GetFaceNames(This,names) (This)->lpVtbl->GetFaceNames(This,names)
#define IDWriteFont_GetFontFamily(This,fontFamily) (This)->lpVtbl->GetFontFamily(This,fontFamily)
#define IDWriteFont_GetInformationalStrings(This,informationalStringID,informationalStrings,exists) (This)->lpVtbl->GetInformationalStrings(This,informationalStringID,informationalStrings,exists)
#define IDWriteFont_GetMetrics(This,fontMetrics) (This)->lpVtbl->GetMetrics(This,fontMetrics)
#define IDWriteFont_GetSimulations(This) (This)->lpVtbl->GetSimulations(This)
#define IDWriteFont_GetStretch(This) (This)->lpVtbl->GetStretch(This)
#define IDWriteFont_GetStyle(This) (This)->lpVtbl->GetStyle(This)
#define IDWriteFont_GetWeight(This) (This)->lpVtbl->GetWeight(This)
#define IDWriteFont_HasCharacter(This,unicodeValue,exists) (This)->lpVtbl->HasCharacter(This,unicodeValue,exists)
#define IDWriteFont_IsSymbolFont(This) (This)->lpVtbl->IsSymbolFont(This)
#endif /*COBJMACROS*/

#undef  INTERFACE
#define INTERFACE IDWriteFontCollection
DECLARE_INTERFACE_(IDWriteFontCollection,IUnknown)
{
    BEGIN_INTERFACE

#ifndef __cplusplus
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void **ppvObject) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;
#endif

    /* IDWriteFontCollection methods */
    STDMETHOD_(UINT32, GetFontFamilyCount)(THIS) PURE;

    STDMETHOD(GetFontFamily)(THIS_
        UINT32 index,
        IDWriteFontFamily **fontFamily) PURE;

    STDMETHOD(FindFamilyName)(THIS_
        WCHAR const *familyName,
        UINT32 *index,
        BOOL *exists) PURE;

    STDMETHOD(GetFontFromFontFace)(THIS_
        IDWriteFontFace* fontFace,
        IDWriteFont **font) PURE;

    END_INTERFACE
};
#ifdef COBJMACROS
#define IDWriteFontCollection_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IDWriteFontCollection_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IDWriteFontCollection_Release(This) (This)->lpVtbl->Release(This)
#define IDWriteFontCollection_FindFamilyName(This,familyName,index,exists) (This)->lpVtbl->FindFamilyName(This,familyName,index,exists)
#define IDWriteFontCollection_GetFontFamily(This,index,fontFamily) (This)->lpVtbl->GetFontFamily(This,index,fontFamily)
#define IDWriteFontCollection_GetFontFamilyCount(This) (This)->lpVtbl->GetFontFamilyCount(This)
#define IDWriteFontCollection_GetFontFromFontFace(This,fontFace,font) (This)->lpVtbl->GetFontFromFontFace(This,fontFace,font)
#endif /*COBJMACROS*/

#undef  INTERFACE
#define INTERFACE IDWriteFontFace
DECLARE_INTERFACE_(IDWriteFontFace,IUnknown)
{
    BEGIN_INTERFACE

#ifndef __cplusplus
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void **ppvObject) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;
#endif

    /* IDWriteFontFace methods */
    STDMETHOD_(DWRITE_FONT_FACE_TYPE, GetType)(THIS) PURE;

    STDMETHOD(GetFiles)(THIS_
        UINT32 *numberOfFiles,
        IDWriteFontFile **fontFiles) PURE;

    STDMETHOD_(UINT32, GetIndex)(THIS) PURE;

    /* rest dropped */
    END_INTERFACE
};
#ifdef COBJMACROS
#define IDWriteFontFace_Release(This) (This)->lpVtbl->Release(This)
#define IDWriteFontFace_GetType(This) (This)->lpVtbl->GetType(This)
#define IDWriteFontFace_GetFiles(This,fontFiles,b) (This)->lpVtbl->GetFiles(This,fontFiles,b)
#define IDWriteFontFace_GetIndex(This) (This)->lpVtbl->GetIndex(This)
#endif /*COBJMACROS*/

#undef  INTERFACE
#define INTERFACE IDWriteFontFamily
DECLARE_INTERFACE_(IDWriteFontFamily,IDWriteFontList)
{
    BEGIN_INTERFACE

#ifndef __cplusplus
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void **ppvObject) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    /* IDWriteFontList methods */
    STDMETHOD(GetFontCollection)(THIS_
        IDWriteFontCollection** fontCollection) PURE;

    STDMETHOD_(UINT32, GetFontCount)(THIS) PURE;

    STDMETHOD(GetFont)(THIS_
        UINT32 index,
        IDWriteFont **font) PURE;
#endif

    /* IDWriteFontFamily methods */
    STDMETHOD(GetFamilyNames)(THIS_
        IDWriteLocalizedStrings **names) PURE;

    /* rest dropped */
    END_INTERFACE
};
#ifdef COBJMACROS
#define IDWriteFontFamily_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IDWriteFontFamily_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IDWriteFontFamily_Release(This) (This)->lpVtbl->Release(This)
#define IDWriteFontFamily_GetFont(This,index,font) (This)->lpVtbl->GetFont(This,index,font)
#define IDWriteFontFamily_GetFontCount(This) (This)->lpVtbl->GetFontCount(This)
#define IDWriteFontFamily_GetFamilyNames(This,names) (This)->lpVtbl->GetFamilyNames(This,names)
#endif /*COBJMACROS*/

#undef  INTERFACE
#define INTERFACE IDWriteFontFile
DECLARE_INTERFACE_(IDWriteFontFile,IUnknown)
{
    BEGIN_INTERFACE

#ifndef __cplusplus
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void **ppvObject) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;
#endif

    /* IDWriteFontFile methods */
    STDMETHOD(GetReferenceKey)(THIS_
        void const **fontFileReferenceKey,
        UINT32 *fontFileReferenceKeySize) PURE;

    STDMETHOD(GetLoader)(THIS_
        IDWriteFontFileLoader **fontFileLoader) PURE;

    /* rest dropped */
    END_INTERFACE
};
#ifdef COBJMACROS
#define IDWriteFontFile_Release(This) (This)->lpVtbl->Release(This)
#define IDWriteFontFile_GetLoader(This,fontFileLoader) (This)->lpVtbl->GetLoader(This,fontFileLoader)
#define IDWriteFontFile_GetReferenceKey(This,fontFileReferenceKey,fontFileReferenceKeySize) (This)->lpVtbl->GetReferenceKey(This,fontFileReferenceKey,fontFileReferenceKeySize)
#endif /*COBJMACROS*/

#undef  INTERFACE
#define INTERFACE IDWriteFontFileLoader
DECLARE_INTERFACE_(IDWriteFontFileLoader,IUnknown)
{
    BEGIN_INTERFACE

#ifndef __cplusplus
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void **ppvObject) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;
#endif

    /* IDWriteFontFileLoader methods */
    STDMETHOD(CreateStreamFromKey)(THIS_
        void const *fontFileReferenceKey,
        UINT32 fontFileReferenceKeySize,
        IDWriteFontFileStream **fontFileStream) PURE;

    END_INTERFACE
};
#ifdef COBJMACROS
#define IDWriteFontFileLoader_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IDWriteFontFileLoader_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IDWriteFontFileLoader_Release(This) (This)->lpVtbl->Release(This)
#define IDWriteFontFileLoader_CreateStreamFromKey(This,fontFileReferenceKey,fontFileReferenceKeySize,fontFileStream) (This)->lpVtbl->CreateStreamFromKey(This,fontFileReferenceKey,fontFileReferenceKeySize,fontFileStream)
#endif /*COBJMACROS*/

#undef  INTERFACE
#define INTERFACE IDWriteFontFileStream
DECLARE_INTERFACE_(IDWriteFontFileStream,IUnknown)
{
    BEGIN_INTERFACE

#ifndef __cplusplus
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void **ppvObject) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;
#endif

    /* IDWriteFontFileStream methods */
    STDMETHOD(ReadFileFragment)(THIS_
        void const **fragmentStart,
        UINT64 fileOffset,
        UINT64 fragmentSize,
        void** fragmentContext) PURE;

    STDMETHOD_(void, ReleaseFileFragment)(THIS_
        void *fragmentContext) PURE;

    STDMETHOD(GetFileSize)(THIS_
        UINT64 *fileSize) PURE;

    STDMETHOD(GetLastWriteTime)(THIS_
        UINT64 *lastWriteTime) PURE;

    END_INTERFACE
};
#ifdef COBJMACROS
#define IDWriteFontFileStream_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IDWriteFontFileStream_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IDWriteFontFileStream_Release(This) (This)->lpVtbl->Release(This)
#define IDWriteFontFileStream_GetFileSize(This,fileSize) (This)->lpVtbl->GetFileSize(This,fileSize)
#define IDWriteFontFileStream_ReadFileFragment(This,fragmentStart,fileOffset,fragmentSize,fragmentContext) (This)->lpVtbl->ReadFileFragment(This,fragmentStart,fileOffset,fragmentSize,fragmentContext)
#define IDWriteFontFileStream_ReleaseFileFragment(This,fragmentContext) (This)->lpVtbl->ReleaseFileFragment(This,fragmentContext)
#endif /*COBJMACROS*/

#undef  INTERFACE
#define INTERFACE IDWriteLocalizedStrings
DECLARE_INTERFACE_(IDWriteLocalizedStrings,IUnknown)
{
    BEGIN_INTERFACE

#ifndef __cplusplus
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void **ppvObject) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;
#endif

    /* IDWriteLocalizedStrings methods */
    STDMETHOD_(UINT32, GetCount)(THIS) PURE;
    STDMETHOD(FindLocaleName)(THIS_
        _In_z_ WCHAR const* localeName,
        _Out_ UINT32* index,
        _Out_ BOOL* exists
        ) PURE;
    STDMETHOD(GetLocaleNameLength)(THIS_
        UINT32 index,
        _Out_ UINT32* length
        ) PURE;
    STDMETHOD(GetLocaleName)(THIS_
        UINT32 index,
        _Out_writes_z_(size) WCHAR* localeName,
        UINT32 size
        ) PURE;
    STDMETHOD(GetStringLength)(THIS_
        UINT32 index,
        _Out_ UINT32* length
        ) PURE;

    STDMETHOD(GetString)(THIS_
        UINT32 index,
        WCHAR *stringBuffer,
        UINT32 size) PURE;

    END_INTERFACE
};
#ifdef COBJMACROS
#define IDWriteLocalizedStrings_Release(This) (This)->lpVtbl->Release(This)
#define IDWriteLocalizedStrings_GetCount(This) (This)->lpVtbl->GetCount(This)
#define IDWriteLocalizedStrings_FindLocaleName(This,locale,index,exists) (This)->lpVtbl->FindLocaleName(This,locale,index,exists)
#define IDWriteLocalizedStrings_GetLocaleNameLength(This,index,size) (This)->lpVtbl->GetLocaleNameLength(This,index,size)
#define IDWriteLocalizedStrings_GetLocaleName(This,index,locale,size) (This)->lpVtbl->GetLocaleName(This,index,locale,size)
#define IDWriteLocalizedStrings_GetStringLength(This,index,size) (This)->lpVtbl->GetStringLength(This,index,size)
#define IDWriteLocalizedStrings_GetString(This,index,stringBuffer,size) (This)->lpVtbl->GetString(This,index,stringBuffer,size)
#endif /*COBJMACROS*/

#undef  INTERFACE
#define INTERFACE IDWriteTextFormat
DECLARE_INTERFACE_(IDWriteTextFormat,IUnknown)
{
    BEGIN_INTERFACE

#ifndef __cplusplus
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void **ppvObject) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;
#endif

    /* IDWriteTextFormat methods */
    /* rest dropped */
    END_INTERFACE
};
#ifdef COBJMACROS
#define IDWriteTextFormat_Release(This) (This)->lpVtbl->Release(This)
#endif /*COBJMACROS*/

#undef  INTERFACE
#define INTERFACE IDWriteTextLayout
DECLARE_INTERFACE_(IDWriteTextLayout,IDWriteTextFormat)
{
    BEGIN_INTERFACE

#ifndef __cplusplus
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void **ppvObject) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    /* IDWriteTextFormat methods */
    STDMETHOD(dummy1)(THIS);
    STDMETHOD(dummy2)(THIS);
    STDMETHOD(dummy3)(THIS);
    STDMETHOD(dummy4)(THIS);
    STDMETHOD(dummy5)(THIS);
    STDMETHOD(dummy6)(THIS);
    STDMETHOD(dummy7)(THIS);
    STDMETHOD(dummy8)(THIS);
    STDMETHOD(dummy9)(THIS);
    STDMETHOD(dummy10)(THIS);
    STDMETHOD(dummy11)(THIS);
    STDMETHOD(dummy12)(THIS);
    STDMETHOD(dummy13)(THIS);
    STDMETHOD(dummy14)(THIS);
    STDMETHOD(dummy15)(THIS);
    STDMETHOD(dummy16)(THIS);
    STDMETHOD(dummy17)(THIS);
    STDMETHOD(dummy18)(THIS);
    STDMETHOD(dummy19)(THIS);
    STDMETHOD(dummy20)(THIS);
    STDMETHOD(dummy21)(THIS);
    STDMETHOD(dummy22)(THIS);
    STDMETHOD(dummy23)(THIS);
    STDMETHOD(dummy24)(THIS);
    STDMETHOD(dummy25)(THIS);
#endif

    /* IDWriteTextLayout methods */
    STDMETHOD(dummy26)(THIS);
    STDMETHOD(dummy27)(THIS);
    STDMETHOD(dummy28)(THIS);
    STDMETHOD(dummy29)(THIS);
    STDMETHOD(dummy30)(THIS);
    STDMETHOD(dummy31)(THIS);
    STDMETHOD(dummy32)(THIS);
    STDMETHOD(dummy33)(THIS);
    STDMETHOD(dummy34)(THIS);
    STDMETHOD(dummy35)(THIS);
    STDMETHOD(dummy36)(THIS);
    STDMETHOD(dummy37)(THIS);
    STDMETHOD(dummy38)(THIS);
    STDMETHOD(dummy39)(THIS);
    STDMETHOD(dummy40)(THIS);
    STDMETHOD(dummy41)(THIS);
    STDMETHOD(dummy42)(THIS);
    STDMETHOD(dummy43)(THIS);
    STDMETHOD(dummy44)(THIS);
    STDMETHOD(dummy45)(THIS);
    STDMETHOD(dummy46)(THIS);
    STDMETHOD(dummy47)(THIS);
    STDMETHOD(dummy48)(THIS);
    STDMETHOD(dummy49)(THIS);
    STDMETHOD(dummy50)(THIS);
    STDMETHOD(dummy51)(THIS);
    STDMETHOD(dummy52)(THIS);
    STDMETHOD(dummy53)(THIS);
    STDMETHOD(dummy54)(THIS);
    STDMETHOD(dummy55)(THIS);
    STDMETHOD(Draw)(THIS_
            void *clientDrawingContext,
            IDWriteTextRenderer *renderer,
            FLOAT originX,
            FLOAT originY) PURE;
    /* rest dropped */
    END_INTERFACE
};
#ifdef COBJMACROS
#define IDWriteTextLayout_Release(This) (This)->lpVtbl->Release(This)
#define IDWriteTextLayout_Draw(This,clientDrawingContext,renderer,originX,originY) (This)->lpVtbl->Draw(This,clientDrawingContext,renderer,originX,originY)
#endif /*COBJMACROS*/

#undef  INTERFACE
#define INTERFACE IDWriteTextRenderer
DECLARE_INTERFACE_(IDWriteTextRenderer,IDWritePixelSnapping)
{
    BEGIN_INTERFACE

#ifndef __cplusplus
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void **ppvObject) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    /* IDWritePixelSnapping methods */
    STDMETHOD(IsPixelSnappingDisabled)(THIS_
            void *clientDrawingContext,
            BOOL *isDisabled) PURE;
    STDMETHOD(GetCurrentTransform)(THIS_
            void *clientDrawingContext,
            DWRITE_MATRIX *transform) PURE;
    STDMETHOD(GetPixelsPerDip)(THIS_
            void *clientDrawingContext,
            FLOAT *pixelsPerDip) PURE;
#endif

    /* IDWriteTextRenderer methods */
    STDMETHOD(DrawGlyphRun)(THIS_
            void *clientDrawingContext,
            FLOAT baselineOriginX,
            FLOAT baselineOriginY,
            DWRITE_MEASURING_MODE measuringMode,
            DWRITE_GLYPH_RUN const *glyphRun,
            DWRITE_GLYPH_RUN_DESCRIPTION const *glyphRunDescription,
            IUnknown* clientDrawingEffect) PURE;
    STDMETHOD(DrawUnderline)(THIS_
            void *clientDrawingContext,
            FLOAT baselineOriginX,
            FLOAT baselineOriginY,
            DWRITE_UNDERLINE const *underline,
            IUnknown *clientDrawingEffect) PURE;
    STDMETHOD(DrawStrikethrough)(THIS_
            void *clientDrawingContext,
            FLOAT baselineOriginX,
            FLOAT baselineOriginY,
            DWRITE_STRIKETHROUGH const *strikethrough,
            IUnknown* clientDrawingEffect) PURE;
    STDMETHOD(DrawInlineObject)(THIS_
            void *clientDrawingContext,
            FLOAT originX,
            FLOAT originY,
            IDWriteInlineObject *inlineObject,
            BOOL isSideways,
            BOOL isRightToLeft,
            IUnknown *clientDrawingEffect) PURE;

    END_INTERFACE
};

/* for our needs */

#undef  INTERFACE
#define INTERFACE IDWriteGdiInterop
DECLARE_INTERFACE_(IDWriteGdiInterop,IUnknown)
{
    BEGIN_INTERFACE

#ifndef __cplusplus
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void **ppvObject) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;
#endif

    /* IDWriteGdiInterop methods */
    STDMETHOD(CreateFontFromLOGFONT)(THIS_ _In_ LOGFONTW const* logFont,
            _COM_Outptr_ IDWriteFont** font) PURE;
    STDMETHOD(ConvertFontToLOGFONT)(THIS_ _In_ IDWriteFont* font,
            _Out_ LOGFONTW* logFont,
            _Out_ BOOL* isSystemFont) PURE;
    STDMETHOD(ConvertFontFaceToLOGFONT)(THIS_ _In_ IDWriteFontFace* font,
            _Out_ LOGFONTW* logFont) PURE;
    STDMETHOD(CreateFontFaceFromHdc)(THIS_ HDC hdc,
            _COM_Outptr_ IDWriteFontFace** fontFace) PURE;
    STDMETHOD(dummy1)(THIS);		
    /*STDMETHOD(CreateBitmapRenderTarget)(THIS_ _In_opt_ HDC hdc,
            UINT32 width,
            UINT32 height,
            _COM_Outptr_ IDWriteBitmapRenderTarget** renderTarget) PURE;*/

    END_INTERFACE
};

#ifdef COBJMACROS
#define IDWriteGdiInterop_Release(This) (This)->lpVtbl->Release(This)
#define IDWriteGdiInterop_CreateFontFromLOGFONT(This,logFont,font) (This)->lpVtbl->CreateFontFromLOGFONT(This, logfont, font)
#define IDWriteGdiInterop_ConvertFontToLOGFONT(This,font,logFont,isSystemFont) (This)->lpVtbl->ConvertFontToLOGFONT)(This, font, logFont, isSystemFont)
#define IDWriteGdiInterop_ConvertFontFaceToLOGFONT(This,font,logFont) (This)->lpVtbl->ConvertFontFaceToLOGFONT(This,font,logFont)
#define IDWriteGdiInterop_CreateFontFaceFromHdc(This,hdc,fontFace) (This)->lpVtbl->CreateFontFaceFromHdc(This, hdc, fontFace)
/* #define IDWriteGdiInterop_CreateBitmapRenderTarget(This,hdc,width,height,renderTarget) (This)->lpVtbl->CreateBitmapRenderTarget(This, hdc, width, height, renderTarget) */
#endif

DEFINE_GUID(IID_IDWriteFactory, 0xb859ee5a,0xd838,0x4b5b,0xa2,0xe8,0x1a,0xdc,0x7d,0x93,0xdb,0x48);
DEFINE_GUID(IID_IDWritePixelSnapping, 0xeaf3a2da,0xecf4,0x4d24,0xb6,0x44,0xb3,0x4f,0x68,0x42,0x02,0x4b);
DEFINE_GUID(IID_IDWriteTextRenderer, 0xef8a8135,0x5cc6,0x45fe,0x88,0x25,0xc5,0xa0,0x72,0x4e,0xb8,0x19);
DEFINE_GUID(IID_IDWriteGdiInterop, 0x1edd9491,0x9853,0x4299,0x89,0x8f,0x64,0x32,0x98,0x3b,0x6f,0x3a);

HRESULT WINAPI DWriteCreateFactory (_In_         DWRITE_FACTORY_TYPE factory_type,
                                    _In_         REFIID              iid,
                                    _COM_Outptr_ IUnknown          **factory);

#endif /* __INC_DWRITE__ */
