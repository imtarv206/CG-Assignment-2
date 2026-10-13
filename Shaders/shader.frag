#version 330 core

out vec4 colour;

in vec4 vCol;
in vec2 TexCoord;
in vec3 FragPos;
in vec3 Normal;

uniform vec3 lightColour;
uniform vec3 lightPos;
uniform vec3 viewPos;

uniform sampler2D texture_data;

vec3 ambientLight()
{
    float ambientStrength = 0.35f;
    return lightColour * ambientStrength;
}

vec3 diffuseLight()
{
    float diffuseStrength = 0.55f;
    vec3 lightDir = normalize(lightPos - FragPos);
    vec3 norm = normalize(Normal);
    float diff = max(dot(norm, lightDir), 0.0f);

    return lightColour * diff * diffuseStrength;
}

vec3 specularLight()
{
    float specularStrength = 0.2f;
    float shininess = 32.0f;
    vec3 lightDir = normalize(lightPos - FragPos);
    vec3 norm = normalize(Normal);
    vec3 reflectDir = reflect(-lightDir, norm);
    vec3 viewDir = normalize(viewPos - FragPos);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0f), shininess);

    return lightColour * spec * specularStrength;
}

void main()
{
    // วัสดุสีขาวอมเทาแบบ Blender Solid View: ไม่ใช้ texture
    // แต่ยังรับ ambient, diffuse และ specular จึงมองเห็นความลึกของพื้นผิว
    // const vec3 baseColour = vec3(0.82f);
    // vec3 litColour = baseColour * (ambientLight() + diffuseLight()) + specularLight();
    // colour = vec4(clamp(litColour, 0.0f, 1.0f), 1.0f);

    // texture
    vec4 texColor = texture(texture_data, TexCoord);

    // คำนวนแสง
    vec3 ambient  = ambientLight();
    vec3 diffuse  = diffuseLight();
    vec3 specular = specularLight();

    // รวมแสง + texture
    vec3 litColour = texColor.rgb * (ambient + diffuse) + specular;

    // final colour
    colour = vec4(clamp(litColour, 0.0f, 1.0f), texColor.a);
}
