#version 150 core


in vec3 vf_col;

out vec4 out_col;

uniform sampler2D fb, depth;
uniform float alpha;


void main(void)
{
    if (texelFetch(depth, ivec2(gl_FragCoord.xy), 0).r <= gl_FragCoord.z) {
        discard;
    }

    out_col = mix(vec4(vf_col, 1.0),
                  texelFetch(fb, ivec2(gl_FragCoord.xy), 0),
                  alpha);
}
