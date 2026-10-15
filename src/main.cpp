#undef GLFW_DLL

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <filesystem>
#include <iostream>

#include "Libs/Mesh.h"
#include "Libs/Shader.h"
#include "Libs/Window.h"
#include "ProjectPaths.h"
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
    float yaw = -90.0f;
    float pitch = 0.0f;

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

    // Transform ของโมเดลคงที่ จึงคำนวณครั้งเดียวนอก render loop
    glm::mat4 model(1.0f);
    model = glm::translate(model, glm::vec3(0.0f, 0.0f, -10.0f));
    model = glm::scale(model, glm::vec3(0.4f));

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

	Mesh meru;
	const std::filesystem::path objPath =
    	modelDirectory / "meru.obj";
	std::cerr << "[3] Loading OBJ: "
    	      << objPath.string() << std::endl;
	if (!meru.CreateMeshFromOBJ(objPath.string()))
	{
    	std::cerr << "[ERROR] Cannot load Meru. "
        	      << "Check OBJ, MTL, and textures.\n";
    	return 1;
	}
	std::cerr << "[4] OBJ and materials loaded.\n";

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

    // กล้องเริ่มต้นอยู่หน้าวัตถุและมองไปทาง -Z
    glm::vec3 cameraPos(0.0f, 0.0f, 5.0f);
    glm::vec3 cameraDirection(0.0f, 0.0f, -1.0f);
    const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);

    // วางแสงไว้สูงและอยู่หน้าตัวโมเดล เพื่อให้เห็นมิติหลังคาและผนัง
    const glm::vec3 lightPos(3.0f, 8.0f, -5.0f);
    const glm::vec3 lightColour(1.0f, 1.0f, 1.0f);

    const float aspect = static_cast<float>(mainWindow.getBufferWidth()) /
                         static_cast<float>(mainWindow.getBufferHeight());
    const glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 500.0f);

    float previousTime = static_cast<float>(glfwGetTime());

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

        const glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraDirection, cameraUp);

        glClearColor(0.035f, 0.035f, 0.04f, 1.0f);
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

        // Mesh จะ bind Texture และ draw แยกตามแต่ละ usemtl ในไฟล์ OBJ
        meru.RenderMesh(uniformTexture, uniformUseTexture, uniformDiffuse);

        // วาดควันหลังวัตถุทึบ เพื่อให้ depth test และ alpha blending ทำงานถูกต้อง
        smokeSystem.render(
            view,
            projection,
            cameraRight,
            cameraUp,
            cameraPos);

        glUseProgram(0);
        mainWindow.swapBuffers();
    }

    // ตัวแปร Mesh และ Shader จะคืนทรัพยากรเมื่อออกจาก main
    return 0;
}
