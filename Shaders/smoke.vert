#version 330 core

layout(location = 0) in vec2 corner;
layout(location = 1) in vec2 textureCoordinate;

out vec2 TexCoord;

uniform mat4 view;
uniform mat4 projection;
uniform vec3 cameraRight;
uniform vec3 cameraUp;
uniform vec3 particleCenter;
uniform float particleSize;
uniform float particleRotation;

void main()
{
    float cosine = cos(particleRotation);
    float sine = sin(particleRotation);
    mat2 rotation = mat2(cosine, sine, -sine, cosine);
    vec2 rotatedCorner = rotation * corner;

    // สร้าง quad ในโลก 3D จากแกนขวาและแกนบนของกล้อง
    vec3 worldPosition = particleCenter
        + cameraRight * rotatedCorner.x * particleSize
        + cameraUp * rotatedCorner.y * particleSize;

    TexCoord = textureCoordinate;
    gl_Position = projection * view * vec4(worldPosition, 1.0);
}
