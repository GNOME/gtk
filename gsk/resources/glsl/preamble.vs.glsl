uniform mat4 u_projection;
uniform mat4 u_modelview;

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
