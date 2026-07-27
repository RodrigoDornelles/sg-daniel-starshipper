#ifdef GL_ES
precision highp float;
#endif
attribute vec2 aPosition;
attribute vec2 aUV;
attribute vec4 aColor;
uniform mat4 uMVP;
varying vec2 vUV;
varying vec4 vColor;
void main() {
    gl_Position = uMVP * vec4(aPosition, 0.0, 1.0);
    vUV = aUV;
    vColor = aColor;
}
