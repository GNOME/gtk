uniform mat4 u_projection;
uniform mat4 u_modelview;
uniform float u_alpha;

#ifdef GSK_GLES
precision highp float;
#endif

#if GSK_GLES
#define _OUT_ varying
#define _IN_ varying
attribute vec2 aPosition;
attribute vec2 aUv;
_OUT_ vec2 vUv;
#elif GSK_LEGACY
#define _OUT_ varying
#define _IN_ varying
attribute vec2 aPosition;
attribute vec2 aUv;
_OUT_ vec2 vUv;
#else
#define _OUT_ out
#define _IN_ in
_IN_ vec2 aPosition;
_IN_ vec2 aUv;
_OUT_ vec2 vUv;
#endif
