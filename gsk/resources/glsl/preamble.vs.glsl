uniform mat4 u_projection;
uniform mat4 u_modelview;


#if GSK_GLES
attribute vec2 aPosition;
attribute vec2 aUv;
varying vec2 vUv;
#elif GSK_LEGACY
attribute vec2 aPosition;
attribute vec2 aUv;
varying vec2 vUv;
#else
in vec2 aPosition;
in vec2 aUv;
out vec2 vUv;
#endif
