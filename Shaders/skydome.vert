#version 330 core

layout(location = 0) in vec3 pos;

out vec3 SkyDirection;

uniform mat4 view;
uniform mat4 projection;

void main()
{
    SkyDirection = normalize(pos);

    // ตัด translation ออกจาก view ให้กล้องหมุนมองฟ้าได้ แต่ไม่มีวันเดินถึงขอบโดม
    mat4 rotationOnlyView = mat4(mat3(view));
    vec4 clipPosition = projection * rotationOnlyView * vec4(pos, 1.0f);

    // ใช้ z = w เพื่อวางท้องฟ้าไว้ที่ความลึกสูงสุดหลัง perspective divide
    gl_Position = clipPosition.xyww;
}
