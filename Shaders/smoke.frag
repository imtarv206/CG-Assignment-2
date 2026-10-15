#version 330 core

in vec2 TexCoord;
in vec3 FragPos;
out vec4 colour;

uniform sampler2D smokeTexture;
uniform vec3 smokeColour;
uniform float particleOpacity;
uniform vec3 viewPos;
uniform float fogDensity;

void main()
{
    vec4 smokeSample = texture(smokeTexture, TexCoord);

    // ใช้ค่า density เดียวกับฉากทึบ เพื่อให้ควันหายเข้าไปในหมอกตามระยะเดียวกับตัวเมรุ
    float cameraDistance = length(viewPos - FragPos);
    float fogVisibility = exp(
        -fogDensity * fogDensity * cameraDistance * cameraDistance);
    float alpha = smokeSample.a * particleOpacity * fogVisibility;

    // ไม่วาดส่วนที่โปร่งใสเกือบทั้งหมด ลดขอบสี่เหลี่ยมของ quad
    if (alpha < 0.01)
        discard;

    colour = vec4(smokeColour * smokeSample.rgb, alpha);
}
