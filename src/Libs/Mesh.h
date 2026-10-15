#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

// โครงสร้างข้อมูล vertex แบบ interleaved: ตำแหน่ง + UV + normal
struct VertexPNU
{
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f};
    glm::vec2 uv{0.0f};
};

class Mesh
{
public:
    Mesh();
    ~Mesh();

    // สร้าง Mesh จาก array แบบเดิม เพื่อให้โค้ดส่วนอื่นของโปรเจกต์ยังเรียกใช้ได้
    void CreateMesh(const GLfloat* vertices,
                    const unsigned int* indices,
                    unsigned int numOfFloats,
                    unsigned int numOfIndices);

    void CreateMesh(const std::vector<VertexPNU>& vertices,
                    const std::vector<unsigned int>& indices);

    // โหลด OBJ พร้อม material จาก MTL และ texture ที่ระบุด้วย map_Kd
    // flipV = true ใช้กลับแกน V ของ UV เมื่อ texture แสดงกลับหัว
    bool CreateMeshFromOBJ(const std::string& path, bool flipV = false);

    // วาด Mesh โดยเลือก texture/material ตามช่วง usemtl ใน OBJ
    // ส่ง location ของ uniform จาก shader.frag เข้ามา
    void RenderMesh(GLint textureUniformLocation = -1,
                    GLint useTextureUniformLocation = -1,
                    GLint diffuseUniformLocation = -1,
                    const glm::vec3& fallbackDiffuse = glm::vec3(0.8f)) const;

    // Depth pass ไม่ใช้ material จึงวาด index ทั้งหมดได้ใน draw call เดียว
    void RenderDepthOnly() const;

    void ClearMesh();

private:
    struct MaterialInfo
    {
        glm::vec3 diffuse{0.8f, 0.8f, 0.8f}; // สีสำรองจาก MTL: Kd
        GLuint textureId = 0;                // texture จาก MTL: map_Kd
    };

    struct MaterialBatch
    {
        std::string materialName;
        std::size_t firstIndex = 0; // index แรกของกลุ่มนี้ใน EBO
        GLsizei indexCount = 0;     // จำนวน index ที่ต้องวาด
    };

    void uploadInterleaved_(const std::vector<VertexPNU>& vertices,
                            const std::vector<unsigned int>& indices);

    GLuint VAO = 0;
    GLuint VBO = 0;
    GLuint IBO = 0;
    GLsizei indexCount = 0;

    // รายการวัสดุที่อ่านมาจากไฟล์ MTL และช่วง index ของแต่ละ usemtl
    std::unordered_map<std::string, MaterialInfo> materials_;
    std::vector<MaterialBatch> materialBatches_;
};
