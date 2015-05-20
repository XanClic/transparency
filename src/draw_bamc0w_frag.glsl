#version 150 core


in vec3 vf_col;

out vec4 out_col;
out float out_transp;

uniform float alpha;


float weight(float depth, float alpha)
{
    return alpha * max(0.01, 3000.0 * pow(1.0 - depth, 10.0));
}


void main(void)
{
    out_col = vec4(vf_col, 1.0) * weight(gl_FragCoord.z, alpha);
    out_transp = 1.0 - alpha;
}
