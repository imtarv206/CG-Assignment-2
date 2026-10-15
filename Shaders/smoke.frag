#version 330 core

in vec2 TexCoord;
out vec4 colour;

uniform sampler2D smokeTexture;
uniform vec3 smokeColour;
uniform float particleOpacity;

void main()
{
    vec4 smokeSample = texture(smokeTexture, TexCoord);
    float alpha = smokeSample.a * particleOpacity;

    // ไม่วาดส่วนที่โปร่งใสเกือบทั้งหมด ลดขอบสี่เหลี่ยมของ quad
    if (alpha < 0.01)
        discard;

    colour = vec4(smokeColour * smokeSample.rgb, alpha);
}
