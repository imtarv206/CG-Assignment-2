#version 330 core

in vec3 SkyDirection;
out vec4 colour;

uniform vec3 horizonColour;
uniform vec3 zenithColour;
uniform vec3 cloudColour;
uniform float time;

float randomValue(vec3 position)
{
    return fract(sin(dot(position, vec3(127.1f, 311.7f, 74.7f))) * 43758.5453f);
}

float valueNoise(vec3 position)
{
    vec3 cell = floor(position);
    vec3 local = fract(position);
    vec3 smoothLocal = local * local * (3.0f - 2.0f * local);

    float n000 = randomValue(cell + vec3(0.0f, 0.0f, 0.0f));
    float n100 = randomValue(cell + vec3(1.0f, 0.0f, 0.0f));
    float n010 = randomValue(cell + vec3(0.0f, 1.0f, 0.0f));
    float n110 = randomValue(cell + vec3(1.0f, 1.0f, 0.0f));
    float n001 = randomValue(cell + vec3(0.0f, 0.0f, 1.0f));
    float n101 = randomValue(cell + vec3(1.0f, 0.0f, 1.0f));
    float n011 = randomValue(cell + vec3(0.0f, 1.0f, 1.0f));
    float n111 = randomValue(cell + vec3(1.0f, 1.0f, 1.0f));

    float x00 = mix(n000, n100, smoothLocal.x);
    float x10 = mix(n010, n110, smoothLocal.x);
    float x01 = mix(n001, n101, smoothLocal.x);
    float x11 = mix(n011, n111, smoothLocal.x);
    float y0 = mix(x00, x10, smoothLocal.y);
    float y1 = mix(x01, x11, smoothLocal.y);
    return mix(y0, y1, smoothLocal.z);
}

float fractalNoise(vec3 position)
{
    float result = 0.0f;
    float amplitude = 0.5f;

    // รวม noise หลายความถี่: ก้อนใหญ่กำหนดรูปร่าง เม็ดเล็กเพิ่มรายละเอียดขอบเมฆ
    for (int octave = 0; octave < 5; ++octave)
    {
        result += valueNoise(position) * amplitude;
        position = position * 2.03f + vec3(13.1f, 7.7f, 5.3f);
        amplitude *= 0.5f;
    }

    return result;
}

void main()
{
    vec3 direction = normalize(SkyDirection);
    float height = clamp(direction.y, 0.0f, 1.0f);

    // ให้สีขอบฟ้าคงอยู่นานกว่าส่วนยอด เพื่อกลืนต่อกับ fogColour ของฉาก
    float skyGradient = smoothstep(0.0f, 1.0f, pow(height, 0.65f));
    vec3 skyColour = mix(horizonColour, zenithColour, skyGradient);

    // ใช้พิกัดทิศทาง 3D จึงไม่มีรอยต่อ 0/360 องศาเหมือน UV แบบทรงกลม
    vec3 windOffset = vec3(time * 0.012f, 0.0f, time * 0.006f);
    float cloudNoise = fractalNoise(direction * 4.0f + windOffset);
    float cloudShape = smoothstep(0.48f, 0.68f, cloudNoise);
    float horizonFade = smoothstep(0.03f, 0.20f, height);
    float cloudAmount = cloudShape * horizonFade * 0.58f;

    vec3 finalColour = mix(skyColour, cloudColour, cloudAmount);
    colour = vec4(finalColour, 1.0f);
}
