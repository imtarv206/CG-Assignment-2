#undef GLFW_DLL

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <vector>

#include "Libs/Mesh.h"
#include "Libs/Shader.h"
#include "Libs/Window.h"
#include "ProjectPaths.h"
#include "ShadowMap.h"
#include "SkyDome.h"
#include "FlameRenderer.h"
#include "SmokeSystem.h"

// กำหนด STB_IMAGE_IMPLEMENTATION ในไฟล์นี้เพียงไฟล์เดียวเท่านั้น
#define STB_IMAGE_IMPLEMENTATION
#define STBI_WINDOWS_UTF8
#include <stb_image.h>

namespace
{
    constexpr GLint WINDOW_WIDTH = 800;
    constexpr GLint WINDOW_HEIGHT = 600;

    // ค่ามุมกล้องเริ่มต้น: หันไปทางแกน -Z
    // float yaw = -90.0f;
    // float pitch = 0.0f;

    // ค่ามุมกล้องเริ่มต้น: test
    float yaw = -159.0f;
    float pitch = 13.0f;

    const std::filesystem::path shaderDirectory =
        std::filesystem::u8path(OPENGL_STARTER_SHADER_DIR);
    const std::filesystem::path modelDirectory =
        std::filesystem::u8path(OPENGL_STARTER_MODEL_DIR);
    const std::filesystem::path textureDirectory =
        std::filesystem::u8path(OPENGL_STARTER_TEXTURE_DIR);

    // อ่านการเคลื่อนเมาส์แล้วปรับทิศทางกล้อง
    void updateMouseLook(Window& window)
    {
        double mouseX = 0.0;
        double mouseY = 0.0;
        glfwGetCursorPos(window.getWindow(), &mouseX, &mouseY);

        static bool firstMouse = true;
        static double lastX = 0.0;
        static double lastY = 0.0;

        if (firstMouse)
        {
            lastX = mouseX;
            lastY = mouseY;
            firstMouse = false;
        }

        float xOffset = static_cast<float>(mouseX - lastX);
        float yOffset = static_cast<float>(mouseY - lastY);
        lastX = mouseX;
        lastY = mouseY;

        constexpr float sensitivity = 0.1f;
        yaw += xOffset * sensitivity;
        pitch -= yOffset * sensitivity;

        // จำกัดมุมก้ม/เงยไม่ให้กล้องพลิกกลับหัว
        pitch = glm::clamp(pitch, -89.0f, 89.0f);
    }
}

int main()
{
    // // สร้างหน้าต่างและ OpenGL context เวอร์ชัน 3.3
    // Window mainWindow(WINDOW_WIDTH, WINDOW_HEIGHT, 3, 3);
    // if (mainWindow.initialise() != 0)
    //     return 1;

    // glEnable(GL_DEPTH_TEST); // เปิด depth test ให้ส่วนใกล้บังส่วนไกลอย่างถูกต้อง

    // // โหลด Mesh; ชื่อไฟล์ต้องตรงกับไฟล์ที่ export มา
    // Mesh meru;
    // const std::filesystem::path objPath = modelDirectory / "meru-new.obj";
    // if (!meru.CreateMeshFromOBJ(objPath.string()))
    // {
    //     std::cerr << "ไม่สามารถโหลด Meru ได้ โปรดตรวจสอบไฟล์ OBJ, MTL และ Texture\n";
    //     return 1;
    // }

	std::cerr << "[1] Initializing window..." << std::endl;
	Window mainWindow(WINDOW_WIDTH, WINDOW_HEIGHT, 3, 3);
	if (mainWindow.initialise() != 0)
	{
    	std::cerr << "[ERROR] Window initialization failed.\n";
    	return 1;
	}
	std::cerr << "[2] Window initialized.\n";
	glEnable(GL_DEPTH_TEST);

    // Skydome Step 1: สร้างโดมรัศมี 100 หน่วยจาก 48x16 ส่วน
    // ขั้นนี้เตรียมเฉพาะ VAO/VBO/EBO; shader และการวาดจะเพิ่มในขั้นถัดไป
    SkyDome skyDome;
    if (!skyDome.initialise(100.0f, 48, 16))
    {
        std::cerr << "[ERROR] Cannot initialise skydome geometry.\n";
        return 1;
    }
    std::cerr << "[SKYDOME] Geometry is ready: "
              << skyDome.getVertexCount() << " vertices, "
              << skyDome.getIndexCount() << " indices.\n";
    if (!skyDome.initialiseShaders(
            shaderDirectory / "skydome.vert",
            shaderDirectory / "skydome.frag"))
    {
        std::cerr << "[ERROR] Cannot initialise skydome shaders.\n";
        return 1;
    }
    if (!skyDome.initialiseTexture(
            textureDirectory / "qwantani_dusk_2_puresky.jpg"))
    {
        std::cerr << "[ERROR] Cannot initialise skydome panorama.\n";
        return 1;
    }
    std::cerr << "[SKYDOME] Panorama shader and texture are ready.\n";

    // Transform ของโมเดลคงที่ จึงคำนวณครั้งเดียวนอก render loop
    glm::mat4 model(1.0f);
    model = glm::translate(model, glm::vec3(0.0f, 0.0f, -10.0f));
    model = glm::scale(model, glm::vec3(0.4f));

    // ============================================================
    // ตั้งค่า Transform ของพวงหรีดแยกจากตัวเมรุ
    // ============================================================

    // ปรับตำแหน่งพวงหรีดได้เอง:
    // X = ซ้าย / ขวา
    // Y = ต่ำ / สูง
    // Z = หน้า / หลัง
    // เริ่มจาก (0, 0, 0) ก่อน แล้วค่อยปรับตามตำแหน่งที่ต้องการ
    glm::vec3 wreathPosition(3.5f, 0.0f, 1.5f);

    // ปรับขนาดพวงหรีด: ค่าน้อยลง = เล็กลง, ค่ามากขึ้น = ใหญ่ขึ้น
    float wreathScale = 0.55f;

    // ปรับมุมหมุนพวงหรีด หน่วยเป็นองศา
    // X = ก้ม / เงย, Y = หมุนซ้าย / ขวา, Z = เอียง
    glm::vec3 wreathRotationDegrees(0.0f, 20.0f, 0.0f);

    // สร้าง Model Matrix สำหรับพวงหรีดโดยเฉพาะ
    // ใasช้ตำแหน่งฐานเดียวกับฉากเมรุ แต่สามารถเลื่อนพวงหรีดแยกได้
    glm::mat4 wreathModel(1.0f);

    // เลื่อนฉากไปยังตำแหน่งเดียวกับเมรุ
    wreathModel = glm::translate(
        wreathModel,
        glm::vec3(0.0f, 0.0f, -10.0f)
    );

    // เลื่อนพวงหรีดแยกจากเมรุ โดยแก้ค่า wreathPosition ด้านบน
    wreathModel = glm::translate(wreathModel, wreathPosition);

    // หมุนพวงหรีดรอบแกน X, Y และ Z ตามลำดับ
    wreathModel = glm::rotate(
        wreathModel,
        glm::radians(wreathRotationDegrees.x),
        glm::vec3(1.0f, 0.0f, 0.0f)
    );
    wreathModel = glm::rotate(
        wreathModel,
        glm::radians(wreathRotationDegrees.y),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    wreathModel = glm::rotate(
        wreathModel,
        glm::radians(wreathRotationDegrees.z),
        glm::vec3(0.0f, 0.0f, 1.0f)
    );

    // ปรับขนาดพวงหรีดโดยไม่เปลี่ยนขนาดตัวเมรุ
    wreathModel = glm::scale(
        wreathModel,
        glm::vec3(wreathScale)
    );

    // ============================================================
    // ตั้งค่า Transform ของต้นไม้ทั้งสองต้นแยกจากเมรุและพวงหรีด
    // ต้นที่หนึ่งอยู่ทางซ้าย และขยับให้ลึกเข้าไปตามแนวมองของกล้องอีกเล็กน้อย
    // ปรับตำแหน่ง ขนาด และมุมหมุนของต้นที่หนึ่งได้จากตัวแปรชุดแรก
    // ============================================================
    // ต้นที่หนึ่ง: ขยับจากตำแหน่งเดิม (-6.6, 0, -9.0) ให้ลึกเข้าไปจากเมรุ
    // หากต้องการถอยลึกเข้าไปอีก ให้ลดค่า X และ Z ไปตามแนวมองของกล้อง
    glm::vec3 treePosition(-8.5f, 0.0f, -9.6f);
    float treeScale = 0.4f;
    glm::vec3 treeRotationDegrees(0.0f, 0.0f, 0.0f);

    glm::mat4 treeModel(1.0f);
    treeModel = glm::translate(treeModel, treePosition);
    treeModel = glm::rotate(
        treeModel, glm::radians(treeRotationDegrees.x),
        glm::vec3(1.0f, 0.0f, 0.0f));
    treeModel = glm::rotate(
        treeModel, glm::radians(treeRotationDegrees.y),
        glm::vec3(0.0f, 1.0f, 0.0f));
    treeModel = glm::rotate(
        treeModel, glm::radians(treeRotationDegrees.z),
        glm::vec3(0.0f, 0.0f, 1.0f));
    treeModel = glm::scale(treeModel, glm::vec3(treeScale));

    // ============================================================
    // ตั้งค่า Transform ของต้นที่สองให้เห็นทางขวาของเมรุ
    // ตำแหน่งเริ่มต้นอยู่ด้านข้างและขยับเข้าหากล้องเล็กน้อย
    // เพื่อไม่ให้ต้นไม้ถูกเมรุบังมากเกินไปจากมุมกล้องปัจจุบัน
    // ปรับ treeRightPosition, treeRightScale และ treeRightRotationDegrees ได้เอง
    // ============================================================
    glm::vec3 treeRightPosition(3.3f, 0.0f, -14.0f);
    float treeRightScale = 0.4f;
    glm::vec3 treeRightRotationDegrees(0.0f, 0.0f, 0.0f);

    glm::mat4 treeRightModel(1.0f);
    treeRightModel = glm::translate(treeRightModel, treeRightPosition);
    treeRightModel = glm::rotate(
        treeRightModel, glm::radians(treeRightRotationDegrees.x),
        glm::vec3(1.0f, 0.0f, 0.0f));
    treeRightModel = glm::rotate(
        treeRightModel, glm::radians(treeRightRotationDegrees.y),
        glm::vec3(0.0f, 1.0f, 0.0f));
    treeRightModel = glm::rotate(
        treeRightModel, glm::radians(treeRightRotationDegrees.z),
        glm::vec3(0.0f, 0.0f, 1.0f));
    treeRightModel = glm::scale(treeRightModel, glm::vec3(treeRightScale));

    // ตำแหน่งปากปล่องในพิกัด local ของไฟล์ OBJ
    // ปรับ X = ซ้าย/ขวา, Y = ต่ำ/สูง, Z = หน้า/หลัง ได้ที่บรรทัดนี้
    const glm::vec3 smokeEmitterLocal(-7.43f, 20.55f, -0.15f);
    const glm::vec3 smokeEmitterWorld =
        glm::vec3(model * glm::vec4(smokeEmitterLocal, 1.0f));

    // เตรียมทรัพยากรควันหลังมี OpenGL context และก่อนโหลดโมเดลขนาดใหญ่
    SmokeSystem smokeSystem(smokeEmitterWorld);
    if (!smokeSystem.initialise(
            textureDirectory / "smoke-clean.png",
            shaderDirectory / "smoke.vert",
            shaderDirectory / "smoke.frag"))
    {
        std::cerr << "[ERROR] Cannot initialise smoke rendering.\n";
        return 1;
    }
    std::cerr << "[SMOKE] Texture, shader, and billboard quad are ready.\n";

    // ตั้งตำแหน่งฐานไฟในพิกัดโลก และกำหนดความกว้าง/ความสูงของบริเวณกำเนิดอนุภาค
    // ปรับ X/Y/Z เพื่อย้ายตำแหน่งไฟ โดยไม่ต้องเปลี่ยนระบบกล้องหรือโมเดล
    FlameRenderer flameRenderer;
    if (!flameRenderer.initialise(
            shaderDirectory / "flame.vert",
            shaderDirectory / "flame.frag"))
    {
        std::cerr << "[ERROR] Cannot initialise flame rendering.\n";
        return 1;
    }
    const glm::vec3 firePosition(-2.1f, 1.25f, -10.07f);
    constexpr float fireWidth = 0.9f;
    constexpr float fireHeight = 0.65f;
    constexpr float fireGlowStrength = 0.85f;

    // สร้าง Shadow Map ขนาด 1024x1024 และใช้ Shader สำหรับขั้นตอนสร้างแผนที่ความลึก
    ShadowMap shadowMap;
    if (!shadowMap.initialise(1024, 1024))
    {
        std::cerr << "[ERROR] Cannot initialise shadow map.\n";
        return 1;
    }

    Shader depthShader;
    depthShader.CreateFromFiles(
        shaderDirectory / "shadow_depth.vert",
        shaderDirectory / "shadow_depth.frag");
    const GLint depthUniformModel = depthShader.GetUniformLocation("model");
    const GLint depthUniformView = depthShader.GetUniformLocation("view");
    const GLint depthUniformProjection = depthShader.GetUniformLocation("projection");
    if (depthUniformModel < 0 || depthUniformView < 0 || depthUniformProjection < 0)
    {
        std::cerr << "[ERROR] Shadow depth shader is not ready.\n";
        return 1;
    }
    std::cerr << "[SHADOW] Depth framebuffer and shader are ready.\n";

	Mesh meru;
	const std::filesystem::path objPath =
    	modelDirectory / "meru.obj";
	std::cerr << "[3] Loading MERU: "
    	      << objPath.string() << std::endl;
	if (!meru.CreateMeshFromOBJ(objPath.string()))
	{
    	std::cerr << "[ERROR] Cannot load Meru. "
        	      << "Check OBJ, MTL, and textures.\n";
    	return 1;
	}
    Mesh wreath;
    const std::filesystem::path wreathObjPath =
        modelDirectory / "wreath-v1.obj";
    std::cerr << "[4] Loading WREATH: "
              << wreathObjPath.string() << std::endl;
    if (!wreath.CreateMeshFromOBJ(wreathObjPath.string()))
    {
        std::cerr << "[ERROR] Cannot load wreath-v1.obj. "
                  << "Check OBJ, MTL, and textures.\n";
        return 1;
    }

    Mesh tree;
    const std::filesystem::path treeObjPath = modelDirectory / "tree01.obj";
    std::cerr << "[5] Loading TREE: " << treeObjPath.string() << std::endl;
    if (!tree.CreateMeshFromOBJ(treeObjPath.string()))
    {
        std::cerr << "[ERROR] Cannot load tree01.obj. "
                  << "Check OBJ, MTL, and textures.\n";
        return 1;
    }

	std::cerr << "[6] OBJ and materials loaded.\n";

    // พื้นรับเงา ไม่ถูกวาดลง depth map เพราะมีหน้าที่รับเงาจากตัวเมรุ
    Mesh ground;
    const std::vector<VertexPNU> groundVertices = {
        {{-18.0f, -0.08f,  12.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 18.0f, -0.08f,  12.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{ 18.0f, -0.08f, -32.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
        {{-18.0f, -0.08f, -32.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}}
    };
    const std::vector<unsigned int> groundIndices = {0, 1, 2, 2, 3, 0};
    ground.CreateMesh(groundVertices, groundIndices);
    const glm::mat4 groundModel(1.0f);
    const glm::vec3 groundColour(0.12f, 0.16f, 0.13f);

    // โหลด shader สำหรับโมเดลที่มีหลาย Material
    Shader sceneShader;
    sceneShader.CreateFromFiles(
        shaderDirectory / "shader.vert",
        shaderDirectory / "shader.frag");

    // จองตำแหน่ง uniform ไว้ก่อน render loop
    const GLint uniformModel = sceneShader.GetUniformLocation("model");
    const GLint uniformView = sceneShader.GetUniformLocation("view");
    const GLint uniformProjection = sceneShader.GetUniformLocation("projection");
    const GLint uniformTexture = sceneShader.GetUniformLocation("texture_data");
    const GLint uniformUseTexture = sceneShader.GetUniformLocation("useTexture");
    const GLint uniformDiffuse = sceneShader.GetUniformLocation("materialDiffuse");
    const GLint uniformLightColour = sceneShader.GetUniformLocation("lightColour");
    const GLint uniformLightPos = sceneShader.GetUniformLocation("lightPos");
    const GLint uniformViewPos = sceneShader.GetUniformLocation("viewPos");
    const GLint uniformAmbientColour = sceneShader.GetUniformLocation("ambientColour");
    const GLint uniformFogColour = sceneShader.GetUniformLocation("fogColour");
    const GLint uniformFogDensity = sceneShader.GetUniformLocation("fogDensity");
    const GLint uniformLightView = sceneShader.GetUniformLocation("lightView");
    const GLint uniformLightProjection = sceneShader.GetUniformLocation("lightProjection");
    const GLint uniformShadowMap = sceneShader.GetUniformLocation("shadowMap");
    const GLint uniformFireLightPosition = sceneShader.GetUniformLocation("fireLightPosition");
    const GLint uniformFireLightColour = sceneShader.GetUniformLocation("fireLightColour");
    const GLint uniformFireLightIntensity = sceneShader.GetUniformLocation("fireLightIntensity");
    const GLint uniformFireLightRadius = sceneShader.GetUniformLocation("fireLightRadius");

    if (uniformFireLightPosition < 0 || uniformFireLightColour < 0 ||
        uniformFireLightIntensity < 0 || uniformFireLightRadius < 0)
    {
        std::cerr << "[ERROR] Scene shader is missing fire-light uniforms. "
                     "Make sure the updated shader.frag is in the active shader directory.\n";
        return 1;
    }

    // กล้องเริ่มต้น: test
    glm::vec3 cameraPos(9.25, 1.08f, -6.93f);
    glm::vec3 cameraDirection(-0.91f, 0.23f, -0.35f);

    // กล้องเริ่มต้นอยู่หน้าวัตถุและมองไปทาง -Z
    // glm::vec3 cameraPos(0.0f, 0.0f, 5.0f);
    // glm::vec3 cameraDirection(0.0f, 0.0f, -1.0f);
    const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);

    // หมุน panorama ตามเดิม แต่กำหนดตำแหน่งและทิศทางแสงหลักใหม่ด้านล่าง
    // หมายเหตุ: ดวงอาทิตย์ที่อยู่ในภาพ panorama เป็น texture จึงอาจไม่ตรงกับแสงจริง
    constexpr float panoramaYawDegrees = 182.0f;
    const float panoramaYawRadians = glm::radians(panoramaYawDegrees);

    // ตำแหน่งแหล่งกำเนิดแสงตามค่าที่ระบุ (พิกัดโลก)
    const glm::vec3 lightPos(5.2f, 6.0f, -3.05f);

    // ทิศที่แสงส่องไป: ใช้เวกเตอร์ที่ให้มาและ normalize เพื่อความแม่นยำ
    const glm::vec3 lightDirection = glm::normalize(
        glm::vec3(-0.625f, -0.334f, -0.706f));

    // จุดเป้าหมายของกล้องแสงอยู่ไปตามทิศทางแสง เพื่อให้ Shadow Map
    // มองเห็นพื้นที่ที่แสงกำลังส่องผ่าน
    const glm::vec3 lightTarget = lightPos + lightDirection * 30.0f;
    const glm::vec3 lightColour(1.0f, 0.72f, 0.62f);
    const glm::vec3 ambientColour(0.56f, 0.52f, 0.68f);
    const glm::vec3 fogColour(0.56f, 0.50f, 0.62f);
    constexpr float fogDensity = 0.022f;

    // กล้องแสงแบบ Orthographic ครอบคลุมตัวเมรุและพื้นที่ที่ต้องรับเงา
    const glm::mat4 lightProjection =
        glm::ortho(-14.0f, 14.0f, -14.0f, 14.0f, 0.1f, 40.0f);
    const glm::mat4 lightView = glm::lookAt(
        lightPos,
        lightTarget,
        worldUp);

    float previousTime = static_cast<float>(glfwGetTime());

    // แสดงค่ากล้องทุก 0.5 วินาทีเมื่อมีการเคลื่อนที่หรือหมุนมุมมอง
    float lastCameraPrintTime = previousTime;
    glm::vec3 lastPrintedCameraPos = cameraPos;
    float lastPrintedYaw = yaw;
    float lastPrintedPitch = pitch;

    while (!mainWindow.getShouldClose())
    {
        const float currentTime = static_cast<float>(glfwGetTime());
        const float deltaTime = currentTime - previousTime;
        previousTime = currentTime;

        smokeSystem.update(deltaTime, currentTime);

        glfwPollEvents();
        updateMouseLook(mainWindow);

        if (glfwGetKey(mainWindow.getWindow(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(mainWindow.getWindow(), GLFW_TRUE);

        // ทิศทางกล้องคำนวณจาก yaw และ pitch
        cameraDirection.x = std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch));
        cameraDirection.y = std::sin(glm::radians(pitch));
        cameraDirection.z = std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch));
        cameraDirection = glm::normalize(cameraDirection);

        const glm::vec3 cameraRight = glm::normalize(glm::cross(cameraDirection, worldUp));
        const glm::vec3 cameraUp = glm::normalize(glm::cross(cameraRight, cameraDirection));

        // อัปเดตอายุ ตำแหน่ง สี และการเกิดใหม่ของอนุภาคไฟในแต่ละเฟรม
        flameRenderer.update(
            deltaTime,
            firePosition,
            fireWidth,
            fireHeight,
            cameraRight);

        // ควบคุมกล้องด้วย W/A/S/D
        constexpr float moveSpeed = 5.0f;
        if (glfwGetKey(mainWindow.getWindow(), GLFW_KEY_W) == GLFW_PRESS)
            cameraPos += cameraDirection * moveSpeed * deltaTime;
        if (glfwGetKey(mainWindow.getWindow(), GLFW_KEY_S) == GLFW_PRESS)
            cameraPos -= cameraDirection * moveSpeed * deltaTime;
        if (glfwGetKey(mainWindow.getWindow(), GLFW_KEY_A) == GLFW_PRESS)
            cameraPos -= cameraRight * moveSpeed * deltaTime;
        if (glfwGetKey(mainWindow.getWindow(), GLFW_KEY_D) == GLFW_PRESS)
            cameraPos += cameraRight * moveSpeed * deltaTime;

        // พิมพ์พิกัดกล้องเป็นระยะ เพื่อไม่ให้ข้อความใน Console มากเกินไป
        const bool cameraPositionChanged = glm::length(cameraPos - lastPrintedCameraPos) > 0.025f;
        const bool cameraOrientationChanged =
            std::fabs(yaw - lastPrintedYaw) > 0.35f ||
            std::fabs(pitch - lastPrintedPitch) > 0.35f;
        if ((cameraPositionChanged || cameraOrientationChanged) &&
            currentTime - lastCameraPrintTime >= 0.5f)
        {
            std::cout << std::fixed << std::setprecision(3)
                      << "[CAMERA] pos = glm::vec3(" << cameraPos.x << "f, "
                      << cameraPos.y << "f, " << cameraPos.z << "f);  yaw = "
                      << yaw << "f; pitch = " << pitch << "f; direction = ("
                      << cameraDirection.x << ", " << cameraDirection.y << ", "
                      << cameraDirection.z << ")\n";
            lastCameraPrintTime = currentTime;
            lastPrintedCameraPos = cameraPos;
            lastPrintedYaw = yaw;
            lastPrintedPitch = pitch;
        }

        const glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraDirection, cameraUp);

        // อ่านขนาด framebuffer จริงทุกเฟรม เพราะขนาดอาจเปลี่ยนเมื่อ maximize/full screen
        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(mainWindow.getWindow(), &framebufferWidth, &framebufferHeight);

        // หากหน้าต่างถูก minimize ขนาดอาจเป็น 0 จึงข้ามการวาดเฟรมนี้ไปก่อน
        if (framebufferWidth <= 0 || framebufferHeight <= 0)
            continue;

        // คำนวณ Projection ใหม่ตามอัตราส่วนภาพปัจจุบัน ป้องกันภาพยืด/ถูกตัด
        const float aspect = static_cast<float>(framebufferWidth) /
                             static_cast<float>(framebufferHeight);
        const glm::mat4 projection = glm::perspective(
            glm::radians(45.0f), aspect, 0.1f, 500.0f);

        // ขั้นตอนที่ 1: วาดตัวเมรุจากมุมมองของแสงลงใน Depth Texture
        shadowMap.beginDepthPass();
        depthShader.UseShader();

        // ส่ง View และ Projection ของกล้องแสงเข้า Depth Shader ทุกเฟรม
        glUniformMatrix4fv(
            depthUniformView, 1, GL_FALSE, glm::value_ptr(lightView));
        glUniformMatrix4fv(
            depthUniformProjection, 1, GL_FALSE, glm::value_ptr(lightProjection));

        // ใช้ front-face culling ระหว่างสร้าง Shadow Map เพื่อลด shadow acne
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);

        // วาดเงาของต้นที่หนึ่งด้วย Model Matrix ของต้นนั้น
        glUniformMatrix4fv(
            depthUniformModel, 1, GL_FALSE, glm::value_ptr(treeModel)
        );
        tree.RenderDepthOnly();

        // วาดเงาของต้นที่สองโดยใช้ Mesh เดิม แต่เปลี่ยน Model Matrix
        glUniformMatrix4fv(
            depthUniformModel, 1, GL_FALSE, glm::value_ptr(treeRightModel)
        );
        tree.RenderDepthOnly();

        // วาดเงาของเมรุจากมุมมองของแสง
        glUniformMatrix4fv(
            depthUniformModel, 1, GL_FALSE, glm::value_ptr(model)
        );
        meru.RenderDepthOnly();

        // วาดเงาของพวงหรีดด้วย Model Matrix ของตัวเอง
        glUniformMatrix4fv(
            depthUniformModel, 1, GL_FALSE,
            glm::value_ptr(wreathModel)
        );
        wreath.RenderDepthOnly();

        // คืนค่า Model Matrix เป็นของเมรุ
        // เพื่อป้องกันการใช้ตำแหน่งพวงหรีดกับวัตถุอื่นโดยไม่ตั้งใจ
        glUniformMatrix4fv(
            depthUniformModel, 1, GL_FALSE, glm::value_ptr(model)
        );

        glCullFace(GL_BACK);
        glDisable(GL_CULL_FACE);

        shadowMap.endDepthPass(framebufferWidth, framebufferHeight);

        // คืน viewport เป็นขนาด framebuffer ของหน้าต่างจริง (ไม่ใช่ 800x600 คงที่)
        glViewport(0, 0, framebufferWidth, framebufferHeight);

        // ขั้นตอนที่ 2: วาดฉากจากมุมมองกล้องและใช้ Depth Texture คำนวณเงา
        // สีพื้นหลังต้องตรงกับสีหมอก ขอบฉากจึงจะกลมกลืน
        glClearColor(fogColour.r, fogColour.g, fogColour.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        sceneShader.UseShader();
        glActiveTexture(GL_TEXTURE0);

        glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(uniformView, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(projection));
        glUniform1i(uniformTexture, 0); // texture_data ใช้ texture unit 0
        glUniform3fv(uniformLightColour, 1, glm::value_ptr(lightColour));
        glUniform3fv(uniformLightPos, 1, glm::value_ptr(lightPos));
        glUniform3fv(uniformViewPos, 1, glm::value_ptr(cameraPos));
        glUniform3fv(uniformAmbientColour, 1, glm::value_ptr(ambientColour));
        glUniform3fv(uniformFogColour, 1, glm::value_ptr(fogColour));
        glUniform1f(uniformFogDensity, fogDensity);

        // แสงจุดสีอุ่นบริเวณฐานไฟช่วยให้พื้นผิวใกล้เคียงได้รับแสงจากเปลวไฟ
        // ปรับความเข้มเล็กน้อยตามเวลาเพื่อให้แสงบนโมเดลกะพริบตามไฟ
        const glm::vec3 fireLightColour(1.0f, 0.24f, 0.045f);
        const float fireLightIntensity =
            1.65f + 0.16f * std::sin(currentTime * 8.0f)
                  + 0.09f * std::sin(currentTime * 14.7f);
        constexpr float fireLightRadius = 4.5f;
        glUniform3fv(uniformFireLightPosition, 1, glm::value_ptr(firePosition));
        glUniform3fv(uniformFireLightColour, 1, glm::value_ptr(fireLightColour));
        glUniform1f(uniformFireLightIntensity, fireLightIntensity);
        glUniform1f(uniformFireLightRadius, fireLightRadius);

        glUniformMatrix4fv(uniformLightView, 1, GL_FALSE, glm::value_ptr(lightView));
        glUniformMatrix4fv(
            uniformLightProjection,
            1,
            GL_FALSE,
            glm::value_ptr(lightProjection));
        shadowMap.bindTexture(GL_TEXTURE1);
        glUniform1i(uniformShadowMap, 1);
        glActiveTexture(GL_TEXTURE0);

        // วาดต้นที่หนึ่ง โดย Depth Test จะจัดการการบังกับวัตถุอื่นให้
        glUniformMatrix4fv(
            uniformModel, 1, GL_FALSE, glm::value_ptr(treeModel));
        tree.RenderMesh(uniformTexture, uniformUseTexture, uniformDiffuse);

        // วาดต้นที่สองทางขวาของเมรุ โดยใช้ Mesh เดิมและ Model Matrix แยกกัน
        glUniformMatrix4fv(
            uniformModel, 1, GL_FALSE, glm::value_ptr(treeRightModel));
        tree.RenderMesh(uniformTexture, uniformUseTexture, uniformDiffuse);

        // วาดเมรุด้วย Model Matrix ของเมรุ
        glUniformMatrix4fv(
            uniformModel, 1, GL_FALSE, glm::value_ptr(model));
        // Mesh จะผูก Texture และวาดแยกตาม Material แต่ละรายการที่ระบุด้วย usemtl ในไฟล์ OBJ
        meru.RenderMesh(uniformTexture, uniformUseTexture, uniformDiffuse);

        // เปลี่ยน Model Matrix ให้เป็นของพวงหรีด
        glUniformMatrix4fv(
            uniformModel, 1, GL_FALSE,
            glm::value_ptr(wreathModel)
        );

        // วาดพวงหรีดโดยใช้ Shader และ Material ระบบเดียวกับเมรุ
        wreath.RenderMesh(
            uniformTexture,
            uniformUseTexture,
            uniformDiffuse
        );

        // คืน Model Matrix กลับเป็นของเมรุ
        // พื้นและวัตถุที่วาดต่อจากนี้จะได้ไม่ใช้ Transform ของพวงหรีด
        glUniformMatrix4fv(
            uniformModel, 1, GL_FALSE, glm::value_ptr(model)
        );

        // พื้นใช้ shader เดียวกัน จึงรับเงา แสง และหมอกได้ต่อเนื่องกับตัวเมรุ
        glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(groundModel));
        ground.RenderMesh(
            uniformTexture,
            uniformUseTexture,
            uniformDiffuse,
            groundColour);

        // วาดท้องฟ้าหลังวัตถุทึบ แต่ก่อนอนุภาคโปร่งใส เพื่อให้ blending ถูกต้อง
        skyDome.render(
            view,
            projection,
            panoramaYawRadians);

        // วาดอนุภาคไฟหลังวัตถุทึบ เพื่อให้ผนังและอาคารบังไฟได้ด้วย Depth Test
        flameRenderer.render(
            view,
            projection,
            cameraRight,
            cameraUp,
            fireGlowStrength);

        // วาดควันหลังวัตถุทึบ เพื่อให้ Depth Test และ Alpha Blending ทำงานถูกต้อง
        smokeSystem.render(
            view,
            projection,
            cameraRight,
            cameraUp,
            cameraPos,
            fogDensity);

        glUseProgram(0);
        mainWindow.swapBuffers();
    }

    // ตัวแปร Mesh และ Shader จะคืนทรัพยากรเมื่อออกจาก main
    return 0;
}
