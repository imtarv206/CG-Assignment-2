#version 330 core

out vec4 colour;

in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;

uniform sampler2D texture_data;
uniform bool useTexture;
uniform vec3 materialDiffuse;

uniform vec3 lightColour;
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 ambientColour;
uniform vec3 fogColour;
uniform float fogDensity;

// แสง Ambient: เพิ่มความสว่างพื้นฐานไม่ให้ด้านเงามืดสนิท
vec3 ambientLight()
{
    const float ambientStrength = 0.25f;
    return ambientColour * ambientStrength;
}

// แสง Diffuse: ทำให้พื้นผิวที่หันเข้าหาแสงสว่างกว่าพื้นผิวที่หันออก
vec3 diffuseLight()
{
    const float diffuseStrength = 0.80f;
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0f);
    return lightColour * diff * diffuseStrength;
}

// แสง Specular: เพิ่มไฮไลต์เล็กน้อยเพื่อช่วยให้เห็นมิติพื้นผิว
vec3 specularLight()
{
    const float specularStrength = 0.18f;
    const float shininess = 32.0f;

    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 halfDir = normalize(lightDir + viewDir);

    float spec = pow(max(dot(norm, halfDir), 0.0f), shininess);
    return lightColour * spec * specularStrength;
}

void main()
{
    // ถ้ามี map_Kd ให้นำสีภาพมาใช้; ถ้าไม่มีให้ใช้ค่า Kd จากไฟล์ MTL
    vec3 baseColour = materialDiffuse;
    float alpha = 1.0f;
    if (useTexture)
    {
        vec4 texel = texture(texture_data, TexCoord);
        baseColour *= texel.rgb;
        alpha = texel.a;
    }

    // แสงสีส้มส่องตัวอาคาร โดยยังคง ambient โทนเย็นไว้ในบริเวณเงา
    vec3 litColour = baseColour * (ambientLight() + diffuseLight()) + specularLight();

    // หมอกแบบ exponential ช่วยให้วัตถุที่ไกลค่อยๆ กลืนกับสีท้องฟ้า
    float cameraDistance = length(viewPos - FragPos);
    float fogAmount = 1.0f - exp(
        -fogDensity * fogDensity * cameraDistance * cameraDistance);
    fogAmount = clamp(fogAmount, 0.0f, 1.0f);

    vec3 finalColour = mix(litColour, fogColour, fogAmount);
    colour = vec4(clamp(finalColour, 0.0f, 1.0f), alpha);
}
