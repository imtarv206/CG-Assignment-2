#version 330 core

out vec4 colour;

in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;
in vec4 FragPosLightSpace;

uniform sampler2D texture_data;
uniform bool useTexture;
uniform vec3 materialDiffuse;

uniform vec3 lightColour;
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 ambientColour;
uniform vec3 fogColour;
uniform float fogDensity;
uniform sampler2D shadowMap;

// Flickering local point light emitted by the procedural flame.
uniform vec3 fireLightPosition;
uniform vec3 fireLightColour;
uniform float fireLightIntensity;
uniform float fireLightRadius;

float shadowCalculation()
{
    vec3 projected = FragPosLightSpace.xyz / FragPosLightSpace.w;
    projected = projected * 0.5f + 0.5f;

    if (projected.z <= 0.0f || projected.z >= 1.0f ||
        projected.x <= 0.0f || projected.x >= 1.0f ||
        projected.y <= 0.0f || projected.y >= 1.0f)
        return 0.0f;

    vec3 normal = normalize(Normal);
    vec3 lightDirection = normalize(lightPos - FragPos);
    float bias = max(0.004f * (1.0f - dot(normal, lightDirection)), 0.0006f);

    vec2 texelSize = 1.0f / textureSize(shadowMap, 0);
    float shadow = 0.0f;
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float closestDepth = texture(
                shadowMap,
                projected.xy + vec2(x, y) * texelSize).r;
            shadow += projected.z - bias > closestDepth ? 1.0f : 0.0f;
        }
    }
    return shadow / 9.0f;
}

vec3 ambientLight()
{
    const float ambientStrength = 0.25f;
    return ambientColour * ambientStrength;
}

vec3 diffuseLight()
{
    const float diffuseStrength = 0.80f;
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0f);
    return lightColour * diff * diffuseStrength;
}

// Subtle unshadowed directional bounce light reveals orientation within deep shadows.
vec3 shadowFillLight()
{
    vec3 norm = normalize(Normal);
    vec3 fillDirection = normalize(vec3(-0.35f, -0.45f, 0.82f));
    float fill = max(dot(norm, fillDirection), 0.0f);
    const float fillStrength = 0.32f;
    const vec3 fillColour = vec3(0.42f, 0.37f, 0.31f);
    return fillColour * fill * fillStrength;
}

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

// Local warm illumination. It falls off with distance and is deliberately separate
// from the main light's shadow term, so firelight can add warm bounce to shadowed areas.
vec3 fireDiffuseLight()
{
    vec3 toFire = fireLightPosition - FragPos;
    float distanceToFire = length(toFire);
    if (distanceToFire >= fireLightRadius || distanceToFire <= 0.0001f)
        return vec3(0.0f);

    vec3 norm = normalize(Normal);
    vec3 lightDir = toFire / distanceToFire;
    float diffuse = max(dot(norm, lightDir), 0.0f);
    float falloff = clamp(1.0f - distanceToFire / fireLightRadius, 0.0f, 1.0f);
    float attenuation = falloff * falloff;
    return fireLightColour * diffuse * attenuation * fireLightIntensity;
}

vec3 fireSpecularLight()
{
    vec3 toFire = fireLightPosition - FragPos;
    float distanceToFire = length(toFire);
    if (distanceToFire >= fireLightRadius || distanceToFire <= 0.0001f)
        return vec3(0.0f);

    vec3 norm = normalize(Normal);
    vec3 lightDir = toFire / distanceToFire;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 halfDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfDir), 0.0f), 24.0f);
    float falloff = clamp(1.0f - distanceToFire / fireLightRadius, 0.0f, 1.0f);
    return fireLightColour * spec * falloff * falloff * fireLightIntensity * 0.12f;
}

void main()
{
    vec3 baseColour = materialDiffuse;
    float alpha = 1.0f;
    if (useTexture)
    {
        vec4 texel = texture(texture_data, TexCoord);
        baseColour *= texel.rgb;
        alpha = texel.a;
    }

    float shadow = shadowCalculation();
    vec3 directLight = (1.0f - shadow) * diffuseLight();
    vec3 reflectedLight = (1.0f - shadow) * specularLight();
    vec3 litColour = baseColour * (
        ambientLight() + directLight + shadowFillLight() + fireDiffuseLight())
        + reflectedLight + fireSpecularLight();

    float cameraDistance = length(viewPos - FragPos);
    float fogAmount = 1.0f - exp(
        -fogDensity * fogDensity * cameraDistance * cameraDistance);
    fogAmount = clamp(fogAmount, 0.0f, 1.0f);
    vec3 finalColour = mix(litColour, fogColour, fogAmount);
    colour = vec4(clamp(finalColour, 0.0f, 1.0f), alpha);
}
