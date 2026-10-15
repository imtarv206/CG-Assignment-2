#version 330 core

layout(location = 0) in vec2 corner;

out vec2 LocalPosition;

uniform mat4 view;
uniform mat4 projection;
uniform vec3 cameraRight;
uniform vec3 cameraUp;
uniform vec3 lightPosition;
uniform float lightSize;

void main()
{
    vec3 worldPosition = lightPosition
        + cameraRight * corner.x * lightSize
        + cameraUp * corner.y * lightSize;

    // ส่งพิกัดกลางวงกลมไปใช้สร้างแกนสว่างและรัศมีใน fragment shader
    LocalPosition = corner * 2.0f;
    gl_Position = projection * view * vec4(worldPosition, 1.0f);
}
