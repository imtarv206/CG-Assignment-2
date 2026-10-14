#include "Mesh.h"

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>

// main.cpp เป็นไฟล์เดียวที่กำหนด STB_IMAGE_IMPLEMENTATION
// ส่วนไฟล์นี้ include header เพื่อเรียก stbi_load() เท่านั้น
#ifndef STBI_WINDOWS_UTF8
#define STBI_WINDOWS_UTF8
#endif
#include <stb_image.h>

namespace
{
    struct ObjIndexTriple
    {
        int position = -1;
        int uv = -1;
        int normal = -1;

        bool operator==(const ObjIndexTriple& other) const
        {
            return position == other.position && uv == other.uv && normal == other.normal;
        }
    };

    struct ObjIndexTripleHash
    {
        std::size_t operator()(const ObjIndexTriple& value) const
        {
            const std::size_t a = std::hash<int>{}(value.position);
            const std::size_t b = std::hash<int>{}(value.uv);
            const std::size_t c = std::hash<int>{}(value.normal);
            return a ^ (b << 1) ^ (c << 2);
        }
    };

    struct MaterialSource
    {
        glm::vec3 diffuse{0.8f, 0.8f, 0.8f};
        std::filesystem::path diffuseMap;
    };

    std::string trim(const std::string& text)
    {
        const auto first = std::find_if_not(text.begin(), text.end(),
            [](unsigned char ch) { return std::isspace(ch) != 0; });
        if (first == text.end()) return {};

        const auto last = std::find_if_not(text.rbegin(), text.rend(),
            [](unsigned char ch) { return std::isspace(ch) != 0; }).base();
        return std::string(first, last);
    }

    // OBJ ใช้ index เริ่มที่ 1; index ติดลบจะนับจากท้ายรายการ
    int convertObjIndex(int index, int count)
    {
        if (index > 0) return index - 1;
        if (index < 0) return count + index;
        return -1;
    }

    bool parseFaceToken(const std::string& token,
                        int positionCount,
                        int uvCount,
                        int normalCount,
                        ObjIndexTriple& result)
    {
        std::string parts[3];
        int part = 0;
        for (char ch : token)
        {
            if (ch == '/')
            {
                if (part < 2) ++part;
            }
            else
            {
                parts[part].push_back(ch);
            }
        }

        try
        {
            if (parts[0].empty()) return false;
            result.position = convertObjIndex(std::stoi(parts[0]), positionCount);
            if (!parts[1].empty()) result.uv = convertObjIndex(std::stoi(parts[1]), uvCount);
            if (!parts[2].empty()) result.normal = convertObjIndex(std::stoi(parts[2]), normalCount);
        }
        catch (...)
        {
            return false;
        }

        if (result.position < 0 || result.position >= positionCount) return false;
        if (result.uv < -1 || result.uv >= uvCount) return false;
        if (result.normal < -1 || result.normal >= normalCount) return false;
        return true;
    }

    // โหลดรูปภาพเป็น OpenGL texture; รองรับไฟล์ RGB/RGBA และ grayscale
    GLuint loadTextureFile(const std::filesystem::path& texturePath)
    {
        int width = 0;
        int height = 0;
        int channels = 0;
        const std::string nativePath = texturePath.string();
        unsigned char* pixels = stbi_load(nativePath.c_str(), &width, &height, &channels, 0);

        if (pixels == nullptr)
        {
            std::cerr << "[Texture] โหลดรูปไม่สำเร็จ: " << nativePath
                      << " | " << stbi_failure_reason() << '\n';
            return 0;
        }

        GLenum format = GL_RGB;
        GLenum internalFormat = GL_RGB8;
        switch (channels)
        {
            case 1: format = GL_RED;  internalFormat = GL_R8;    break;
            case 2: format = GL_RG;   internalFormat = GL_RG8;   break;
            case 3: format = GL_RGB;  internalFormat = GL_RGB8;  break;
            case 4: format = GL_RGBA; internalFormat = GL_RGBA8; break;
            default:
                std::cerr << "[Texture] จำนวน channel ไม่รองรับ: " << nativePath << '\n';
                stbi_image_free(pixels);
                return 0;
        }

        GLuint textureId = 0;
        glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_2D, textureId);

        // ใช้ glTexParameteri เพื่อให้เข้ากับ OpenGL 3.3
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

        // ภาพบางขนาดมีจำนวน byte ต่อแถวที่ไม่หาร 4 ลงตัว
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0,
                     format, GL_UNSIGNED_BYTE, pixels);
        glGenerateMipmap(GL_TEXTURE_2D);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

        stbi_image_free(pixels);
        glBindTexture(GL_TEXTURE_2D, 0);
        std::cout << "[Texture] โหลดแล้ว: " << nativePath << '\n';
        return textureId;
    }

    // อ่านค่า newmtl, Kd และ map_Kd จากไฟล์ .mtl
    // map_Kd จะเก็บข้อความที่เหลือทั้งบรรทัด เพื่อรองรับชื่อรูปที่มีช่องว่าง
    void readMtlFile(const std::filesystem::path& mtlPath,
                     std::unordered_map<std::string, MaterialSource>& materialSources)
    {
        std::ifstream file(mtlPath);
        if (!file)
        {
            std::cerr << "[MTL] เปิดไฟล์ไม่ได้: " << mtlPath.string() << '\n';
            return;
        }

        std::string currentMaterial;
        std::string line;
        while (std::getline(file, line))
        {
            line = trim(line);
            if (line.empty() || line[0] == '#') continue;

            std::istringstream stream(line);
            std::string command;
            stream >> command;
            std::string remainder;
            std::getline(stream, remainder);
            remainder = trim(remainder);

            if (command == "newmtl")
            {
                currentMaterial = remainder;
                if (!currentMaterial.empty())
                {
                    // ถ้า MTL ไม่กำหนด Kd ให้ใช้สีเทาอ่อนเป็นค่าเริ่มต้น
                    materialSources.try_emplace(currentMaterial, MaterialSource{});
                }
            }
            else if (!currentMaterial.empty() && command == "Kd")
            {
                std::istringstream values(remainder);
                float r = 0.8f, g = 0.8f, b = 0.8f;
                if (values >> r >> g >> b)
                {
                    auto& source = materialSources[currentMaterial];
                    source.diffuse = glm::vec3(r, g, b);
                }
            }
            else if (!currentMaterial.empty() && command == "map_Kd")
            {
                // ตัด quote ที่ครอบชื่อไฟล์ออก ถ้ามี
                if (remainder.size() >= 2 && remainder.front() == '"' && remainder.back() == '"')
                    remainder = remainder.substr(1, remainder.size() - 2);

                // โค้ดนี้รองรับ map_Kd ตามรูปแบบใน MTL ที่ผู้ใช้ส่งมา
                // ซึ่งเป็นชื่อไฟล์ตรง ๆ ไม่มี option เช่น -s หรือ -o นำหน้า
                if (!remainder.empty())
                    materialSources[currentMaterial].diffuseMap = std::filesystem::u8path(remainder);
            }
        }
    }
}

Mesh::Mesh() = default;
Mesh::~Mesh() { ClearMesh(); }

void Mesh::CreateMesh(const GLfloat* vertices,
                      const unsigned int* indices,
                      unsigned int numOfFloats,
                      unsigned int numOfIndices)
{
    std::vector<VertexPNU> vertexData;

    if (numOfFloats % 8 == 0)
    {
        // รูปแบบเดิม: ตำแหน่ง 3 + normal 3 + UV 2
        const unsigned int count = numOfFloats / 8;
        vertexData.reserve(count);
        for (unsigned int i = 0; i < count; ++i)
        {
            const GLfloat* value = vertices + i * 8;
            VertexPNU vertex;
            vertex.position = glm::vec3(value[0], value[1], value[2]);
            vertex.normal = glm::vec3(value[3], value[4], value[5]);
            vertex.uv = glm::vec2(value[6], value[7]);
            vertexData.push_back(vertex);
        }
    }
    else if (numOfFloats % 5 == 0)
    {
        // รูปแบบเดิม: ตำแหน่ง 3 + UV 2; normal จะคำนวณภายหลังถ้าจำเป็น
        const unsigned int count = numOfFloats / 5;
        vertexData.reserve(count);
        for (unsigned int i = 0; i < count; ++i)
        {
            const GLfloat* value = vertices + i * 5;
            VertexPNU vertex;
            vertex.position = glm::vec3(value[0], value[1], value[2]);
            vertex.normal = glm::vec3(0.0f);
            vertex.uv = glm::vec2(value[3], value[4]);
            vertexData.push_back(vertex);
        }
    }
    else
    {
        std::cerr << "[Mesh] จำนวน float ของ vertex ไม่รองรับ: " << numOfFloats << '\n';
        return;
    }

    std::vector<unsigned int> indexData(indices, indices + numOfIndices);
    CreateMesh(vertexData, indexData);
}

void Mesh::CreateMesh(const std::vector<VertexPNU>& vertices,
                      const std::vector<unsigned int>& indices)
{
    uploadInterleaved_(vertices, indices);
}

void Mesh::uploadInterleaved_(const std::vector<VertexPNU>& vertices,
                              const std::vector<unsigned int>& indices)
{
    // ล้าง VAO/VBO/EBO และ material เก่าก่อนสร้างชุดใหม่
    ClearMesh();
    if (vertices.empty() || indices.empty()) return;

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(VertexPNU)),
                 vertices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &IBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)),
                 indices.data(), GL_STATIC_DRAW);

    const GLsizei stride = static_cast<GLsizei>(sizeof(VertexPNU));
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(VertexPNU, position)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(VertexPNU, uv)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(VertexPNU, normal)));
    glEnableVertexAttribArray(2);

    indexCount = static_cast<GLsizei>(indices.size());
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

bool Mesh::CreateMeshFromOBJ(const std::string& path, bool flipV)
{
    const std::filesystem::path objPath = std::filesystem::u8path(path);
    std::ifstream file(objPath);
    if (!file)
    {
        std::cerr << "[OBJ] เปิดไฟล์ไม่ได้: " << objPath.string() << '\n';
        return false;
    }

    // ---------- รอบแรก: หาไฟล์ MTL ที่ OBJ อ้างถึง ----------
    std::vector<std::filesystem::path> mtlPaths;
    std::string line;
    while (std::getline(file, line))
    {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        std::istringstream mtlStream(line);
        std::string tag;
        mtlStream >> tag;
        if (tag == "mtllib")
        {
            std::string filename;
            std::getline(mtlStream, filename);
            filename = trim(filename);
            if (filename.size() >= 2 && filename.front() == '"' && filename.back() == '"')
                filename = filename.substr(1, filename.size() - 2);
            if (!filename.empty()) mtlPaths.push_back(objPath.parent_path() / std::filesystem::u8path(filename));
        }
    }

    std::unordered_map<std::string, MaterialSource> materialSources;
    for (const auto& mtlPath : mtlPaths)
        readMtlFile(mtlPath, materialSources);

    // ---------- รอบสอง: อ่าน vertex, UV, normal และแยกกลุ่ม usemtl ----------
    file.clear();
    file.seekg(0);

    std::vector<glm::vec3> positions;
    std::vector<glm::vec2> texcoords;
    std::vector<glm::vec3> normals;
    std::vector<VertexPNU> vertices;
    std::vector<unsigned int> indices;
    std::vector<bool> normalWasMissing;
    std::unordered_map<ObjIndexTriple, unsigned int, ObjIndexTripleHash> vertexLookup;
    std::vector<MaterialBatch> batches;

    positions.reserve(10000);
    texcoords.reserve(10000);
    normals.reserve(10000);
    vertices.reserve(30000);
    indices.reserve(60000);

    std::string currentMaterial = "DefaultMaterial";
    batches.push_back(MaterialBatch{currentMaterial, 0, 0});
    std::size_t currentBatch = 0;

    auto changeMaterial = [&](const std::string& name)
    {
        if (name.empty() || name == batches[currentBatch].materialName) return;

        // ถ้าเพิ่งเริ่มไฟล์และยังไม่มีหน้า ให้นำชื่อวัสดุมาใช้กับ batch แรกได้เลย
        if (batches[currentBatch].indexCount == 0)
        {
            batches[currentBatch].materialName = name;
            batches[currentBatch].firstIndex = indices.size();
        }
        else
        {
            batches.push_back(MaterialBatch{name, indices.size(), 0});
            currentBatch = batches.size() - 1;
        }
        currentMaterial = name;
    };

    auto getOrCreateVertex = [&](const ObjIndexTriple& key) -> unsigned int
    {
        const auto found = vertexLookup.find(key);
        if (found != vertexLookup.end()) return found->second;

        VertexPNU vertex;
        vertex.position = positions[static_cast<std::size_t>(key.position)];
        if (key.uv >= 0)
        {
            vertex.uv = texcoords[static_cast<std::size_t>(key.uv)];
            if (flipV) vertex.uv.y = 1.0f - vertex.uv.y;
        }
        if (key.normal >= 0)
            vertex.normal = normals[static_cast<std::size_t>(key.normal)];
        else
            vertex.normal = glm::vec3(0.0f);

        const unsigned int newIndex = static_cast<unsigned int>(vertices.size());
        vertices.push_back(vertex);
        normalWasMissing.push_back(key.normal < 0);
        vertexLookup.emplace(key, newIndex);
        return newIndex;
    };

    while (std::getline(file, line))
    {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        std::istringstream stream(line);
        std::string command;
        stream >> command;

        if (command == "v")
        {
            glm::vec3 value(0.0f);
            if (stream >> value.x >> value.y >> value.z) positions.push_back(value);
        }
        else if (command == "vt")
        {
            glm::vec2 value(0.0f);
            if (stream >> value.x >> value.y) texcoords.push_back(value);
            else texcoords.push_back(glm::vec2(0.0f));
        }
        else if (command == "vn")
        {
            glm::vec3 value(0.0f);
            if (stream >> value.x >> value.y >> value.z) normals.push_back(value);
        }
        else if (command == "usemtl")
        {
            std::string materialName;
            std::getline(stream, materialName);
            materialName = trim(materialName); // ชื่อ material อาจมีเครื่องหมาย [] หรือช่องว่าง
            changeMaterial(materialName);
        }
        else if (command == "f")
        {
            // อ่านจุดยอดทั้งหมดของ face แล้ว triangulate เป็นสามเหลี่ยม
            std::vector<ObjIndexTriple> face;
            std::string token;
            bool validFace = true;
            while (stream >> token)
            {
                ObjIndexTriple key;
                if (!parseFaceToken(token,
                                    static_cast<int>(positions.size()),
                                    static_cast<int>(texcoords.size()),
                                    static_cast<int>(normals.size()), key))
                {
                    validFace = false;
                    break;
                }
                face.push_back(key);
            }
            if (!validFace || face.size() < 3) continue;

            for (std::size_t i = 1; i + 1 < face.size(); ++i)
            {
                const unsigned int a = getOrCreateVertex(face[0]);
                const unsigned int b = getOrCreateVertex(face[i]);
                const unsigned int c = getOrCreateVertex(face[i + 1]);
                indices.push_back(a);
                indices.push_back(b);
                indices.push_back(c);
                batches[currentBatch].indexCount += 3;
            }
        }
    }

    if (vertices.empty() || indices.empty())
    {
        std::cerr << "[OBJ] ไม่พบ geometry ที่ใช้วาดได้ใน: " << objPath.string() << '\n';
        return false;
    }

    // ถ้า OBJ ไม่มี vn บางจุด ให้คำนวณ normal จากสามเหลี่ยมเพื่อช่วยเรื่องแสง
    std::vector<glm::vec3> generatedNormals(vertices.size(), glm::vec3(0.0f));
    for (std::size_t i = 0; i + 2 < indices.size(); i += 3)
    {
        const unsigned int ia = indices[i];
        const unsigned int ib = indices[i + 1];
        const unsigned int ic = indices[i + 2];
        const glm::vec3 edge1 = vertices[ib].position - vertices[ia].position;
        const glm::vec3 edge2 = vertices[ic].position - vertices[ia].position;
        const glm::vec3 faceNormal = glm::cross(edge1, edge2);
        if (glm::dot(faceNormal, faceNormal) < 1.0e-12f) continue;
        if (normalWasMissing[ia]) generatedNormals[ia] += faceNormal;
        if (normalWasMissing[ib]) generatedNormals[ib] += faceNormal;
        if (normalWasMissing[ic]) generatedNormals[ic] += faceNormal;
    }
    for (std::size_t i = 0; i < vertices.size(); ++i)
    {
        if (normalWasMissing[i] && glm::dot(generatedNormals[i], generatedNormals[i]) > 1.0e-12f)
            vertices[i].normal = glm::normalize(generatedNormals[i]);
    }

    // โหลดรูปภาพของแต่ละ Material จาก map_Kd โดยอ้างอิง path จากไฟล์ MTL
    std::unordered_map<std::string, MaterialInfo> loadedMaterials;
    std::unordered_map<std::string, GLuint> textureCache;

    for (const auto& [materialName, source] : materialSources)
    {
        MaterialInfo material;
        material.diffuse = source.diffuse;

        if (!source.diffuseMap.empty())
        {
            std::filesystem::path texturePath = source.diffuseMap;
            if (texturePath.is_relative())
            {
                // Project root: CG-Assignment-2/
                const std::filesystem::path projectRoot =
                objPath.parent_path().parent_path();

                // YourProject/Textures/<filename>
                const std::filesystem::path relativeToTextures =
                projectRoot / "Textures" / texturePath;

                // YourProject/Models/<filename>
                const std::filesystem::path relativeToMtl =
                    !mtlPaths.empty()
                        ? mtlPaths.front().parent_path() / texturePath
                        : objPath.parent_path() / texturePath;

                // Prefer the separate Textures folder.
                if (std::filesystem::exists(relativeToTextures))
                {  
                    texturePath = relativeToTextures;
                }
                else if (std::filesystem::exists(relativeToMtl))
                {
                    // Fallback: allow textures to be stored beside the MTL.
                    texturePath = relativeToMtl;
                }
                else
                {
                    // Preserve the expected Textures path for the error message.
                    texturePath = relativeToTextures;
                }
            }

            const std::string cacheKey = texturePath.lexically_normal().string();
            const auto cached = textureCache.find(cacheKey);
            if (cached != textureCache.end())
                material.textureId = cached->second;
            else if (std::filesystem::exists(texturePath))
            {
                material.textureId = loadTextureFile(texturePath);
                if (material.textureId != 0) textureCache.emplace(cacheKey, material.textureId);
            }
            else
            {
                std::cerr << "[MTL] ไม่พบรูป map_Kd: " << texturePath.string()
                          << " (Material: " << materialName << ")\n";
            }
        }

        loadedMaterials.emplace(materialName, material);
    }

    // สร้าง GPU buffers ก่อน แล้วจึงติดตั้ง material ที่เพิ่งโหลด
    uploadInterleaved_(vertices, indices);
    materials_ = std::move(loadedMaterials);

    // ตัด batch ว่างทิ้ง ป้องกัน draw call ที่ไม่มี index
    batches.erase(std::remove_if(batches.begin(), batches.end(),
        [](const MaterialBatch& batch) { return batch.indexCount <= 0; }), batches.end());
    materialBatches_ = std::move(batches);

    std::cout << "[OBJ] โหลดสำเร็จ: " << objPath.string()
              << " | vertices=" << vertices.size()
              << " | indices=" << indices.size()
              << " | material batches=" << materialBatches_.size() << '\n';
    if (mtlPaths.empty())
        std::cerr << "[OBJ] คำเตือน: OBJ ไม่มีบรรทัด mtllib; จะใช้สีสำรองเป็นหลัก\n";

    return true;
}

void Mesh::RenderMesh(GLint textureUniformLocation,
                      GLint useTextureUniformLocation,
                      GLint diffuseUniformLocation) const
{
    if (VAO == 0 || indexCount == 0) return;

    glBindVertexArray(VAO);

    // ถ้าเป็น Mesh ที่สร้างจาก array แบบเดิม หรือไม่มี usemtl ให้ใช้ draw call เดียว
    if (materialBatches_.empty())
    {
        glBindTexture(GL_TEXTURE_2D, 0);
        if (useTextureUniformLocation >= 0) glUniform1i(useTextureUniformLocation, GL_FALSE);
        if (diffuseUniformLocation >= 0) glUniform3f(diffuseUniformLocation, 0.8f, 0.8f, 0.8f);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
    }
    else
    {
        for (const MaterialBatch& batch : materialBatches_)
        {
            MaterialInfo material;
            const auto found = materials_.find(batch.materialName);
            if (found != materials_.end()) material = found->second;

            const bool hasTexture = material.textureId != 0;
            glBindTexture(GL_TEXTURE_2D, hasTexture ? material.textureId : 0);

            if (textureUniformLocation >= 0) glUniform1i(textureUniformLocation, 0);
            if (useTextureUniformLocation >= 0)
                glUniform1i(useTextureUniformLocation, hasTexture ? GL_TRUE : GL_FALSE);
            if (diffuseUniformLocation >= 0)
                glUniform3fv(diffuseUniformLocation, 1, glm::value_ptr(material.diffuse));

            const std::uintptr_t byteOffset = batch.firstIndex * sizeof(unsigned int);
            glDrawElements(GL_TRIANGLES, batch.indexCount, GL_UNSIGNED_INT,
                           reinterpret_cast<const void*>(byteOffset));
        }
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
}

void Mesh::ClearMesh()
{
    if (IBO != 0) { glDeleteBuffers(1, &IBO); IBO = 0; }
    if (VBO != 0) { glDeleteBuffers(1, &VBO); VBO = 0; }
    if (VAO != 0) { glDeleteVertexArrays(1, &VAO); VAO = 0; }

    // บาง Material อาจใช้รูปเดียวกัน จึงลบ texture ID ซ้ำเพียงครั้งเดียว
    std::unordered_set<GLuint> textureIds;
    for (const auto& [name, material] : materials_)
    {
        (void)name;
        if (material.textureId != 0) textureIds.insert(material.textureId);
    }
    for (GLuint textureId : textureIds) glDeleteTextures(1, &textureId);

    materials_.clear();
    materialBatches_.clear();
    indexCount = 0;
}
