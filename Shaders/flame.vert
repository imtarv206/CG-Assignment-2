#version 330 core

// ตำแหน่งมุม Quad ของอนุภาค มีค่าอยู่ในช่วง -0.5 ถึง 0.5
layout (location = 0) in vec2 vertexPosition;

// ข้อมูลที่แตกต่างกันในแต่ละอนุภาค
layout (location = 1) in vec3 instancePosition;
layout (location = 2) in vec2 instanceSize;
layout (location = 3) in vec4 instanceColor;

out vec2 TexCoords;
out vec4 ParticleColor;

uniform mat4 view;
uniform mat4 projection;
uniform vec3 cameraRight;
uniform vec3 cameraUp;

void main()
{
    // แปลงตำแหน่งมุม Quad ให้เป็นพิกัด Texture ช่วง 0 ถึง 1
    TexCoords = vertexPosition + vec2(0.5);
    ParticleColor = instanceColor;

    // หัน Quad เข้าหากล้อง โดยใช้แกนขวาและแกนขึ้นของกล้อง
    vec3 worldPosition = instancePosition
                       + cameraRight * (vertexPosition.x * instanceSize.x)
                       + cameraUp * (vertexPosition.y * instanceSize.y);

    gl_Position = projection * view * vec4(worldPosition, 1.0);
}
