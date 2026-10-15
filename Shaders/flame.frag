#version 330 core

in vec2 TexCoords;
in vec4 ParticleColor;
out vec4 colour;

uniform float glowStrength;

void main()
{
    // แปลงพิกัดจากช่วง 0..1 ให้แกน X อยู่ในช่วง -1..1
    // แกน Y ยังคงอยู่ในช่วง 0..1 โดย 0 คือฐาน และ 1 คือปลายบน
    vec2 local = vec2(TexCoords.x * 2.0 - 1.0, TexCoords.y);
    float y = local.y;

    // สร้างรูปทรงคล้ายหยดน้ำ: ส่วนล่างเป็นปุ่มกลม ส่วนบนค่อย ๆ เรียวเป็นปลาย
    // เปลี่ยนความกว้างตามระดับ y แทนการใช้ด้านเอียงเป็นเส้นตรงแบบสามเหลี่ยม
    // ปรับ bulbCenterY เพื่อเลื่อนตำแหน่งส่วนที่อวบ และ bulbRadiusY เพื่อปรับความมนของฐาน
    const float bulbCenterY = 0.23;
    const float bulbRadiusY = 0.26;
    const float bulbHalfWidth = 0.70;

    // หน้าตัดส่วนล่างเป็นเส้นโค้งวงรี ทำให้ฐานค่อย ๆ โค้งออกแทนที่จะเป็นมุมแหลม
    float bulbOffset = (y - bulbCenterY) / bulbRadiusY;
    float bulbWidth = bulbHalfWidth
                    * sqrt(max(1.0 - bulbOffset * bulbOffset, 0.0));

    // ส่วนบนเรียวต่อเนื่องไปจนถึงปลายด้านบน
    float upperT = clamp((y - bulbCenterY) / (1.0 - bulbCenterY), 0.0, 1.0);
    float taperWidth = bulbHalfWidth * pow(max(1.0 - upperT, 0.0), 0.92);

    // ผสมเส้นโค้งส่วนล่างและส่วนเรียวให้ต่อกันนุ่มนวล ไม่เห็นรอยต่อเป็นเหลี่ยม
    float join = smoothstep(bulbCenterY - 0.035, bulbCenterY + 0.14, y);
    float widthAtHeight = mix(bulbWidth, taperWidth, join);
    float edgeSoftness = mix(0.030, 0.008, smoothstep(0.20, 1.0, y));
    float sideMask = 1.0 - smoothstep(
        max(widthAtHeight - edgeSoftness, 0.0),
        widthAtHeight + edgeSoftness,
        abs(local.x));

    // ลด Alpha เฉพาะช่วงปลายบนสุด เพื่อให้ปลายเรียวและไม่ถูกตัดทื่อ
    float tipFade = 1.0 - smoothstep(0.975, 1.0, y);
    float flameShape = sideMask * tipFade;

    // สร้างแสงฟุ้งรอบอนุภาค เพื่อให้อนุภาคที่ซ้อนกันดูเรืองแสง
    float glow = exp(-dot(local * vec2(1.0, 0.82),
                          local * vec2(1.0, 0.82)) * 2.8);

    // ขอบด้านนอกเป็นสีส้ม ส่วนด้านในใช้สีที่ส่งมาจากระบบ Particle
    float edgeRatio = clamp(abs(local.x) / max(widthAtHeight, 0.001), 0.0, 1.0);
    vec3 outerOrange = vec3(1.0, 0.20, 0.015);
    vec3 particleRgb = mix(ParticleColor.rgb,
                           outerOrange,
                           smoothstep(0.18, 0.95, edgeRatio) * 0.82);

    // เติมสีเหลืองอ่อนตรงแกนกลางเพื่อให้ดูร้อนและสว่างกว่าแนวขอบ
    float hotCore = (1.0 - smoothstep(0.0, 0.58, abs(local.x)))
                  * (1.0 - smoothstep(0.12, 0.78, y));
    particleRgb = mix(particleRgb, vec3(1.0, 0.90, 0.48), hotCore * 0.28);

    float alpha = ParticleColor.a
                * clamp(flameShape * 0.88 + glow * 0.16 * glowStrength,
                        0.0, 0.95);

    if (alpha < 0.008)
        discard;

    colour = vec4(particleRgb, alpha);
}
