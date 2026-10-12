#version 330 core

layout(location = 0) in vec3 pos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec3 aNormal;

out vec4 vCol;
out vec2 TexCoord;
out vec3 FragPos;
out vec3 Normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{                 //Clip      //View   //World //Local
    vec4 worldPosition = model * vec4(pos, 1.0f);

    gl_Position = projection * view * worldPosition;
    vCol = vec4(clamp(pos, 0.0f, 1.0f),1.0f);
    TexCoord = aTexCoord;
    FragPos = vec3(worldPosition);
    Normal = mat3(transpose(inverse(model))) * aNormal;
}
