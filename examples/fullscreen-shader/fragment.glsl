// original shader by @XorDev
// https://www.shadertoy.com/view/3csSWB

#version 460

layout(std140, set = 3, binding = 0) uniform UniformData {
    vec2 resolution;
    float time;
};

layout(location = 0) out vec4 frag_color;

mat2 rot(float a) {
    float c = cos(a), s = sin(a);
    return mat2(c, s, -s, c);
}

void main() {
    vec2 F = vec2(gl_FragCoord.x, resolution.y - gl_FragCoord.y);
    float i = 0.2;
    float a = 0.0;
    vec2 r = resolution.xy;
    vec2 p = (F + F - r) / r.y / 0.7;
    vec2 d = vec2(-1.0, 1.0);
    vec2 b = p - i * d;

    float denom = 0.1 + i / dot(b, b);
    mat2 M1 = mat2(1.0, 1.0, d.x / denom, d.y / denom);
    vec2 c = p * M1;

    float base_ang = 0.5 * log(a = dot(c, c)) + time * i;
    vec4 ang = base_ang + vec4(0.0, 33.0, 11.0, 0.0);
    mat2 M2 = mat2(cos(ang.x), cos(ang.y), cos(ang.z), cos(ang.w));
    vec2 v = (c * M2) / i;

    vec2 w = vec2(0.0);
    for(; i++ < 9.0;) {
        w += 1.0 + sin(v);
        v += 0.7 * sin(v.yx * i + time) / i + 0.5;
    }

    i = length(sin(v / 0.3) * 0.4 + c * (3.0 + d));

    vec4 grad = c.x * vec4(0.6, -0.4, -1.0, 0.0);
    vec4 wave = vec4(w.x, w.y, w.y, w.x);

    vec4 O = 1.0 - exp(
        -exp(grad)
        / wave
        / (2.0 + i*i*0.25 - i)
        / (0.5 + 1.0/a)
        / (0.03 + abs(length(p) - 0.7))
    );

    frag_color = clamp(O, 0.0, 1.0);
}
