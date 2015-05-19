#version 150 core


out vec4 out_col;

uniform sampler2D fb;
uniform vec4 color;


void main(void)
{
    vec3 old_col = texelFetch(fb, ivec2(gl_FragCoord.xy), 0).rgb;
    out_col = vec4(color.rgb - color.a * old_col, 1.0);
}
