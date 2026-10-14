#version 330 core

layout(location = 0) in vec3 pos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec3 aNormal;

out vec2 TexCoord;
out vec3 Normal;
out vec3 FragPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    // แปลงตำแหน่ง vertex ไปยังพิกัดโลก เพื่อใช้คำนวณแสง
    vec4 worldPosition = model * vec4(pos, 1.0f);
    FragPos = worldPosition.xyz;

    // ใช้ inverse-transpose เพื่อแปลง normal ให้ถูกต้องเมื่อ model มี scale
    Normal = mat3(transpose(inverse(model))) * aNormal;

    // ส่ง UV ไปให้ fragment shader อ่านสีของ Texture
    TexCoord = aTexCoord;

    gl_Position = projection * view * worldPosition;
}
