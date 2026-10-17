#version 330 core

in vec3 SkyDirection;
out vec4 colour;

uniform sampler2D skyPanorama;
uniform float panoramaYaw;

void main()
{
    vec3 direction = normalize(SkyDirection);
    const float inverseTwoPi = 0.15915494309f;
    const float inversePi = 0.31830988618f;

    // แปลงทิศทาง 3D เป็น UV ของภาพ panorama แบบ equirectangular
    // stbi_load เก็บแถวบนของภาพไว้ที่ v = 0 จึงกลับเครื่องหมายของ latitude
    float u = atan(direction.z, direction.x) * inverseTwoPi + 0.5f;
    u = fract(u + panoramaYaw * inverseTwoPi);
    float v = 0.5f - asin(clamp(direction.y, -1.0f, 1.0f)) * inversePi;

    colour = vec4(texture(skyPanorama, vec2(u, v)).rgb, 1.0f);
}
