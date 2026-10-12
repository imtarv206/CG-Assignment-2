#undef GLFW_DLL

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <filesystem>
#include <vector>
#include <iostream>

#include "Libs/Mesh.h"
#include "Libs/Shader.h"
#include "Libs/Window.h"
#include "ProjectPaths.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_WINDOWS_UTF8
#include <stb_image.h>

// 1. ค่าคงที่และสถานะส่วนกลางของโปรแกรม

constexpr GLint WIDTH = 800;
constexpr GLint HEIGHT = 600;

std::vector<Mesh*> meshList;
std::vector<Shader*> shaderList;

// มุมกล้องเริ่มต้น: yaw = -90 องศา ทำให้กล้องมองไปทางแกน -Z
float yaw = -90.0f;
float pitch = 0.0f;

// CMake สร้าง path เหล่านี้จาก src/ProjectPaths.h.in
const std::filesystem::path shaderDirectory =
	std::filesystem::u8path(OPENGL_STARTER_SHADER_DIR);
const std::filesystem::path textureDirectory =
	std::filesystem::u8path(OPENGL_STARTER_TEXTURE_DIR);
const std::filesystem::path modelDirectory =
	std::filesystem::u8path(OPENGL_STARTER_MODEL_DIR);

// 2. การสร้างและโหลดทรัพยากร (Mesh, Model, Shader, Texture)

// สร้าง pyramid จาก vertex และ index โดยตรง เพื่อใช้เป็นตัวอย่างสร้างวัตถุด้วย vertices
void CreateTriangle()
{
	GLfloat vertices[] =
	{
		// x, y, z				u, v
		-1.0f, -1.0f, 0.0f,		0.0f, 0.0f,
		 0.0f, -1.0f, 1.0f,		0.5f, 0.0f,
		 1.0f, -1.0f, 0.0f,		1.0f, 0.0f,
		 0.0f, 1.0f, 0.0f,		0.5f, 1.0f
	};

	unsigned int indices[] =
	{
		0, 3, 1,
		1, 3, 2,
		2, 3, 0,
		0, 1, 2
	};

	Mesh* pyramidMesh = new Mesh();
	pyramidMesh->CreateMesh(vertices, indices, 20, 12);
	for (int i = 0; i < 10; i++)
	{
		meshList.push_back(pyramidMesh);
	}
}

// โหลดโมเดล OBJ แล้วเก็บ mesh ไว้ในรายการสำหรับนำไปวาด
void CreateOBJ(const std::filesystem::path& modelPath)
{
	Mesh* model = new Mesh();
	const bool loaded = model->CreateMeshFromOBJ(modelPath.u8string().c_str());
	if (!loaded)
	{
		std::cerr << "Failed to load model: " << modelPath << std::endl;
		return;
	}

	meshList.push_back(model);
}

// โหลดและ compile vertex/fragment shader หลักของฉาก
void CreateShaders()
{
	Shader* sceneShader = new Shader();
	sceneShader->CreateFromFiles(
		shaderDirectory / "shader.vert",
		shaderDirectory / "shader.frag"
	);
	shaderList.push_back(sceneShader);
}

// โหลดไฟล์ภาพด้วย stb_image และส่งข้อมูลขึ้น GPU เป็น OpenGL texture
GLuint LoadTexture(const std::filesystem::path& texturePath)
{
	int width = 0;
	int height = 0;
	int channels = 0;

	unsigned char* data = stbi_load(
		texturePath.u8string().c_str(),
		&width,
		&height,
		&channels,
		0
	);

	if (data == nullptr)
	{
		std::cerr << "Failed to load texture: " << stbi_failure_reason() << "\n";
		return 0;
	}

	GLenum format = GL_RGB;

	switch (channels)
	{
		case 4:
			format = GL_RGBA;
			break;
		case 3:
			format = GL_RGB;
			break;
		case 2:
			format = GL_RG;
			break;
		case 1:
			format = GL_RED;
			break;
		default:
			std::cerr << "Unsupported texture format.\n";
			stbi_image_free(data);
			return 0;
	}

	// สร้าง texture object และกำหนดวิธี wrap/filter ภาพ
	GLuint texture = 0;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

	// รองรับแถวข้อมูลภาพที่มีจำนวน byte ไม่หารสี่ลงตัว
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);

	glGenerateMipmap(GL_TEXTURE_2D);
	stbi_image_free(data);
	glBindTexture(GL_TEXTURE_2D, 0);

	return texture;
}

// 3. การคืนทรัพยากร OpenGL

void Cleanup()
{
	// CreateTriangle() ใช้ pointer เดียวกันซ้ำ 10 ตำแหน่ง จึง delete เพียงครั้งเดียว
	if (!meshList.empty() && meshList[0] != nullptr)
	{
		delete meshList[0];
	}
	meshList.clear();

	for (Shader* shader : shaderList)
	{
		delete shader;
	}
	shaderList.clear();
}

// 4. การควบคุมกล้องด้วยเมาส์

void UpdateMouseLook(Window& window)
{
	double mouseX = 0.0;
	double mouseY = 0.0;
	glfwGetCursorPos(window.getWindow(), &mouseX, &mouseY);

	// ครั้งแรกจะใช้ตำแหน่งเมาส์ปัจจุบัน เพื่อป้องกันกล้องกระโดด
	static double lastX = mouseX;
	static double lastY = mouseY;

	float xOffset = static_cast<float>(mouseX - lastX);
	float yOffset = static_cast<float>(mouseY - lastY);
	constexpr float sensitivity = 0.1f;

	xOffset *= sensitivity;
	yOffset *= sensitivity;

	// จำตำแหน่งไว้คำนวณ offset ในเฟรมถัดไป
	lastX = mouseX;
	lastY = mouseY;

	yaw += xOffset;
	// ค่า Y ของ cursor เพิ่มลงด้านล่าง จึงต้องลบเพื่อให้ pitch ทำงานตามธรรมชาติ
	pitch -= yOffset;

	// จำกัดมุมก้ม/เงยเพื่อป้องกันกล้องพลิกกลับหัว
	if (pitch > 89.0f) pitch = 89.0f;
	if (pitch < -89.0f) pitch = -89.0f;
}

// 5. จุดเริ่มต้นโปรแกรมและ Render Loop

int main()
{
	// 5.1 สร้างหน้าต่าง 800 x 600 และ OpenGL 3.3 context
	Window mainWindow(WIDTH, HEIGHT, 3, 3);
	if (mainWindow.initialise() != 0)
	{
		return 1;
	}

	// 5.2 โหลด model, shader และ texture ที่ใช้ในฉาก
	// CreateTriangle(); // สลับมาใช้เมื่อต้องการทดสอบ mesh ที่สร้างจาก vertices
	CreateOBJ(modelDirectory / "Meru-lowpoly-clean.obj");
	CreateShaders();

	GLuint textureData = LoadTexture(textureDirectory / "uvmap.png");

	if (textureData == 0)
	{
		glDeleteTextures(1, &textureData);
		return 1;
	}

	// 5.3 เก็บตำแหน่ง uniform เพื่อส่งค่าจาก C++ ไปยัง shader
	const GLint uniformModel = shaderList[0]->GetUniformLocation("model");
	const GLint uniformView = shaderList[0]->GetUniformLocation("view");
	const GLint uniformProjection = shaderList[0]->GetUniformLocation("projection");
	const GLint uniformTextureData = shaderList[0]->GetUniformLocation("texture_data");
	const GLint uniformLightColour = shaderList[0]->GetUniformLocation("lightColour");
	const GLint uniformLightPos = shaderList[0]->GetUniformLocation("lightPos");
	const GLint uniformViewPos = shaderList[0]->GetUniformLocation("viewPos");

	// เปิด depth test เพื่อให้วัตถุใกล้บังวัตถุไกลอย่างถูกต้อง
	glEnable(GL_DEPTH_TEST);

	const glm::mat4 projection = glm::perspective(
		glm::radians(45.0f),
		static_cast<GLfloat>(mainWindow.getBufferWidth()) /
			static_cast<GLfloat>(mainWindow.getBufferHeight()),
		0.1f,
		500.0f
	);

	// 5.4 กำหนดค่าเริ่มต้นของกล้องและแสง
	glm::vec3 cameraPos(0.0f, 0.0f, 5.0f);
	const glm::vec3 lightPos(5.0f, 5.0f, 0.0f);
	const glm::vec3 lightColour(1.0f, 1.0f, 1.0f);
	const glm::vec3 cameraTarget(0.0f, 0.0f, -1.0f);
	glm::vec3 cameraDirection = glm::normalize(cameraTarget - cameraPos);

	cameraDirection.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	cameraDirection.y = sin(glm::radians(pitch));
	cameraDirection.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

	const glm::vec3 up(0.0f, 1.0f, 0.0f);
	// ใช้ cross product หาแกนขวาและแกนบนของกล้องตามกฎมือขวา
	glm::vec3 cameraRight = glm::normalize(glm::cross(cameraDirection, up));
	glm::vec3 cameraUp = glm::normalize(glm::cross(cameraRight, cameraDirection));

	float lastTime = static_cast<float>(glfwGetTime());

	// 5.5 Render loop: รับ input -> คำนวณกล้อง -> ส่ง uniforms -> วาดฉาก
	while (!mainWindow.getShouldClose())
	{
		// เวลาแต่ละเฟรม ใช้ทำให้ความเร็วการเคลื่อนที่ไม่ขึ้นกับ frame rate
		const float currentTime = static_cast<float>(glfwGetTime());
		const float deltaTime = currentTime - lastTime;
		lastTime = currentTime;

		// รับ input จาก keyboard และ mouse
		glfwPollEvents();
		UpdateMouseLook(mainWindow);

		constexpr float moveSpeed = 5.0f;
		if (glfwGetKey(mainWindow.getWindow(), GLFW_KEY_W) == GLFW_PRESS)
		{
			cameraPos = cameraPos + cameraDirection * moveSpeed * deltaTime;
		}
		if (glfwGetKey(mainWindow.getWindow(), GLFW_KEY_S) == GLFW_PRESS)
		{
			cameraPos = cameraPos - cameraDirection * moveSpeed * deltaTime;
		}
		if (glfwGetKey(mainWindow.getWindow(), GLFW_KEY_A) == GLFW_PRESS)
		{
			cameraPos = cameraPos - cameraRight * moveSpeed * deltaTime;
		}
		if (glfwGetKey(mainWindow.getWindow(), GLFW_KEY_D) == GLFW_PRESS)
		{
			cameraPos = cameraPos + cameraRight * moveSpeed * deltaTime;
		}

		// เตรียมวาดเฟรมใหม่
		glClearColor(0.06f, 0.06f, 0.06f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// เปิด shader และส่งค่าแสง/กล้อง
		shaderList[0]->UseShader();

		glUniform1i(uniformTextureData, 0);
		glUniform3fv(uniformLightColour, 1, glm::value_ptr(lightColour));
		glUniform3fv(uniformLightPos, 1, glm::value_ptr(lightPos));
		glUniform3fv(uniformViewPos, 1, glm::value_ptr(cameraPos));

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, textureData);

		// คำนวณ view matrix จากตำแหน่งและทิศทางกล้อง
		glm::mat4 view(1.0f);
		glm::mat4 cameraRotateMat(1.0f);
		glm::mat4 cameraPosMat(1.0f);

		cameraDirection.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
		cameraDirection.y = sin(glm::radians(pitch));
		cameraDirection.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

		glm::vec3 cameraRight = glm::normalize(glm::cross(cameraDirection, up));
		glm::vec3 cameraUp = glm::normalize(glm::cross(cameraRight, cameraDirection));

		// GLM เข้าถึง matrix แบบ [column][row]
		cameraPosMat[3][0] = -cameraPos.x;
		cameraPosMat[3][1] = -cameraPos.y;
		cameraPosMat[3][2] = -cameraPos.z;

		cameraRotateMat[0] = glm::vec4(cameraRight.x, cameraUp.x, -cameraDirection.x, 0.0f);
		cameraRotateMat[1] = glm::vec4(cameraRight.y, cameraUp.y, -cameraDirection.y, 0.0f);
		cameraRotateMat[2] = glm::vec4(cameraRight.z, cameraUp.z, -cameraDirection.z, 0.0f);

		view = cameraRotateMat * cameraPosMat;

		// วิธีที่สั้นกว่าและให้ผลเทียบเท่ากัน:
		// view = glm::lookAt(cameraPos, cameraPos + cameraDirection, cameraUp);

		// ไฟล์ที่ล้างแล้วอยู่กึ่งกลาง origin และคงแกนเดิมของ OBJ
		glm::mat4 model(1.0f);
		model = glm::translate(model, glm::vec3(0.0f, 0.0f, -10.0f));
		model = glm::scale(model, glm::vec3(0.4f));

		glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
		glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(projection));
		glUniformMatrix4fv(uniformView, 1, GL_FALSE, glm::value_ptr(view));

		meshList[0]->RenderMesh();

		glBindTexture(GL_TEXTURE_2D, 0);

		// ปิด shader และนำภาพที่วาดเสร็จแล้วขึ้นแสดงบนหน้าต่าง
		glUseProgram(0);
		mainWindow.swapBuffers();
	}

	// คืนทรัพยากร OpenGL ก่อน Window destructor ทำลาย context
	Cleanup();
	return 0;
}
