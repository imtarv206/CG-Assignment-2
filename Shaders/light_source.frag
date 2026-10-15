#version 330 core

in vec2 LocalPosition;
out vec4 colour;

uniform vec3 lightColour;

void main()
{
    float radius = length(LocalPosition);
    if (radius >= 1.0f)
        discard;

    // รัศมีภายนอกนุ่ม ส่วนแกนกลางเกือบขาวเพื่อให้ดูเป็นแหล่งกำเนิดแสง
    float halo = pow(1.0f - radius, 2.2f);
    float core = 1.0f - smoothstep(0.0f, 0.22f, radius);
    float intensity = clamp(halo * 0.75f + core, 0.0f, 1.0f);
    vec3 glowColour = mix(lightColour, vec3(1.0f, 0.92f, 0.72f), core);

    colour = vec4(glowColour * intensity, intensity);
}
