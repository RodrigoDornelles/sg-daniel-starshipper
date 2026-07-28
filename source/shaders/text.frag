#ifdef GL_ES
precision mediump float;
#endif
uniform sampler2D uTex;
varying vec2 vUV;
varying vec4 vColor;
void main() {
    float a = texture2D(uTex, vUV).a;
    gl_FragColor = vec4(vColor.rgb, vColor.a * a);
}
