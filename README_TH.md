# โครงการ Meru Thai Crematorium — OpenGL และโมเดลหลาย Material/Texture

README นี้อธิบายโครงสร้างไฟล์ วิธีเตรียมโมเดลจาก Blender วิธีตั้งค่า OBJ/MTL/Texture และวิธีรันโปรแกรม OpenGL สำหรับแสดงโมเดลเมรุ

## 1. ภาพรวม

โปรแกรม C++ นี้ใช้ OpenGL แสดงโมเดลจากไฟล์ Wavefront OBJ โดยอ่าน Material จากไฟล์ MTL และโหลด diffuse texture ที่กำหนดด้วย `map_Kd` แต่ละกลุ่ม `usemtl` จะถูกวาดด้วย Material ที่ตรงกัน

ส่วนประกอบหลัก:

- `main.cpp` — สร้างหน้าต่าง กล้อง แสง shader และเรียกโหลด/วาดโมเดล
- `Libs/Mesh.h`, `Libs/Mesh.cpp` — โหลด OBJ/MTL/Texture และวาด geometry แยกตาม Material
- `Shaders/shader.vert`, `Shaders/shader.frag` — shader สำหรับตำแหน่ง vertex, UV, แสง และ texture
- `Models/*.obj`, `Models/*.mtl` — geometry และข้อมูล Material ของโมเดล
- `Textures/` — ไฟล์รูปภาพที่ MTL อ้างถึง
- `ProjectPaths.h` หรือ template ที่ใช้สร้างไฟล์นี้ — กำหนด path ของโฟลเดอร์โมเดลและ shader

## 2. โครงสร้างโฟลเดอร์

ตัวอย่างโครงสร้างที่แนะนำ:

```text
YourProject/
├── CMakeLists.txt
├── Models/
│   ├── meru-edited.obj
│   └── meru-edited.mtl
├── Shaders/
│   ├── shader.vert
│   └── shader.frag
├── Textures/
│   ├── Grunge_S.jpg
│   ├── Stone_Pavers_Flagstone_Gray.jpg
│   ├── Porcelain_B_Mask.jpg
│   └── ... รูปภาพอื่นที่ MTL อ้างถึง
└── src/
    ├── main.cpp
    ├── ProjectPaths.h.in (ถ้าโปรเจกต์ใช้ template นี้)
    └── Libs/
        ├── Mesh.h
        ├── Mesh.cpp
        ├── Shader.h
        └── Window.h
```

ชื่อ `meru-edited.obj` เป็นตัวอย่าง สามารถใช้ชื่ออื่นได้ แต่ต้องตั้งค่าใน `main.cpp` ให้ตรงกับไฟล์จริง

## 3. วิธีแก้ไขโมเดลใน Blender

1. สำรองไฟล์ OBJ, MTL และโฟลเดอร์ Textures เดิมก่อนเริ่มแก้ไข
2. เปิด Blender แล้วเลือก **File → Import → Wavefront (.obj)** จากนั้นเลือก OBJ ที่ต้องการแก้ไข
3. ถ้าโมเดลเป็นสีเทา ให้ลองเปลี่ยน viewport เป็น **Material Preview** หาก texture แสดงขึ้น แสดงว่า Blender สามารถแสดงวัสดุ/texture ที่นำเข้าได้
4. เลือกเฉพาะส่วนของโมเดลที่ไม่ต้องการ แล้วลบ geometry นั้น โดยตรวจให้แน่ใจว่าไม่ได้ลบส่วนที่เชื่อมต่อกับองค์ประกอบอื่น
5. เมื่อต้องการบันทึกกลับเป็น OBJ ให้เลือก **File → Export → Wavefront (.obj)**
6. ส่งออก OBJ และ MTL ไปยังโฟลเดอร์เดียวกัน โดยเปิดตัวเลือกส่งออก Material/MTL, UV Coordinates และ Normals
7. หากมีตัวเลือก **Path Mode** ให้ใช้ `Strip Path` เพื่อส่งออกชื่อไฟล์ texture แบบไม่เป็น absolute path เช่น `Grunge_S.jpg`
8. ส่งออกเป็นชื่อใหม่ เช่น `meru-edited.obj` ก่อน อย่าเพิ่งเขียนทับไฟล์ต้นฉบับจนกว่าจะตรวจสอบผลลัพธ์

การลบ geometry ไม่ได้แปลว่า Blender จะลบ Material ที่ไม่ถูกใช้ออกจาก MTL ทุกกรณี ดังนั้นไฟล์ MTL ที่ export อาจยังมี `newmtl` ที่ไม่ได้ถูกใช้ใน OBJ อยู่ได้ โดยทั่วไปไม่ทำให้ geometry ที่เหลือหายไป แต่ loader ในโปรเจกต์นี้อาจยังพยายามอ่าน texture ที่อ้างอยู่ใน MTL นั้นด้วย

## 4. ตรวจสอบความเชื่อมโยงระหว่าง OBJ, MTL และ Texture

### 4.1 OBJ ต้องอ้าง MTL ที่ตรงกัน

เปิดไฟล์ OBJ ด้วย VS Code หรือ Notepad++ แล้วตรวจบรรทัด `mtllib` ใกล้ต้นไฟล์ ตัวอย่าง:

```obj
mtllib meru-edited.mtl
```

ชื่อไฟล์ต้องตรงกับ MTL ที่ export จริง ทั้งตัวอักษรและนามสกุล

### 4.2 ชื่อ Material ต้องตรงกัน

ใน OBJ บรรทัด `usemtl` ระบุ Material ที่ใช้กับ face ส่วน MTL ใช้ `newmtl` เพื่อประกาศ Material ชื่อเดียวกัน ตัวอย่าง:

```obj
usemtl Roofing_Scalloped_Ornate
```

ต้องมีชื่อที่ตรงกันใน MTL:

```mtl
newmtl Roofing_Scalloped_Ornate
map_Kd Roofing Scalloped Ornate.jpg
```

อย่าเปลี่ยนชื่อ `newmtl` หรือ `usemtl` แยกจากกัน เพราะชื่อ Material ต้องตรงกัน

### 4.3 Texture ต้องอยู่ในตำแหน่งที่หาเจอ

หาก MTL มีบรรทัด:

```mtl
map_Kd Grunge_S.jpg
```

ให้ตรวจว่าไฟล์ `Grunge_S.jpg` มีอยู่จริงใน `Textures/` ตามโค้ดค้นหา texture ของโปรเจกต์นี้

ใช้ชื่อไฟล์ให้ตรงทุกตัวอักษร รวมถึงช่องว่างและนามสกุล เช่น:

```mtl
map_Kd Metal Silver.jpg
map_Kd Roofing Scalloped Ornate.jpg
```

อย่าให้บรรทัดเป็น absolute path จากเครื่องอื่น เช่น:

```mtl
map_Kd C:/Metal_Rough.jpg
```

เปลี่ยนเป็นชื่อไฟล์แบบ relative เช่น:

```mtl
map_Kd Metal_Rough.jpg
```

หาก texture อยู่ในโฟลเดอร์ย่อย ต้องให้ path ใน `map_Kd` ตรงกับตำแหน่งจริงและรูปแบบที่ loader รองรับ

> Loader ปัจจุบันอ่าน `map_Kd` เป็นข้อความชื่อไฟล์ตรง ๆ และรองรับชื่อไฟล์ที่มีช่องว่าง แต่ยังไม่ได้แยกวิเคราะห์ option ของ MTL เช่น `-s` หรือ `-o` ที่อาจอยู่ก่อนชื่อไฟล์

## 5. ตั้งค่าไฟล์ที่โปรแกรมโหลด

ใน `src/main.cpp` ให้ตรวจว่าบรรทัดนี้ตรงกับชื่อ OBJ ที่ต้องการใช้:

```cpp
const std::filesystem::path objPath = modelDirectory / "meru-edited.obj";
```

หากใช้ `meru-new.obj` หรือ `meru-lowpoly-v2.obj` ให้แก้ชื่อไฟล์ในบรรทัดนี้ตามจริง ส่วน OBJ จะเป็นผู้บอก loader เองว่าต้องเปิด MTL ชื่อใดผ่าน `mtllib`

ตรวจ path ของโฟลเดอร์ใน `ProjectPaths.h` หรือ template/CMake ที่สร้าง header นี้:

- `OPENGL_STARTER_MODEL_DIR` ต้องชี้ไปที่ `YourProject/Models`
- `OPENGL_STARTER_SHADER_DIR` ต้องชี้ไปที่ `YourProject/Shaders`
- `OPENGL_STARTER_TEXTURE_DIR` ควรชี้ไปที่ `YourProject/Textures` หากโปรเจกต์ใช้ค่านี้

หาก `ProjectPaths.h` เป็นไฟล์ที่ CMake สร้างขึ้น ให้แก้ template หรือการตั้งค่า CMake แทนการแก้ generated header ในโฟลเดอร์ build

## 6. Build และรันโปรแกรม

1. ตรวจให้แน่ใจว่าไฟล์ OBJ, MTL และ texture ครบตามที่ระบุไว้ข้างต้น
2. Build โปรเจกต์ด้วยระบบ build ที่โปรเจกต์ตั้งค่าไว้ (เช่น CMake/Visual Studio)
3. รันโปรแกรมจาก IDE หรือ terminal ที่มองเห็น Console/Output log
4. หากโหลดโมเดลหรือ texture ไม่สำเร็จ ให้ดูข้อความ `[OBJ]`, `[MTL]` และ `[Texture]` ใน log

โค้ดใช้ OpenGL ร่วมกับไลบรารีที่โปรเจกต์มีอยู่ เช่น GLFW, GLEW, GLM และ stb_image และใช้ `std::filesystem` จึงต้องคอมไพล์ด้วยมาตรฐาน C++17 หรือใหม่กว่าตามการตั้งค่าโปรเจกต์

การโหลดโมเดลที่มี geometry หรือ texture จำนวนมากอาจใช้เวลาหลายวินาทีหรือมากกว่านั้นในช่วงเริ่มต้น หากยังมีข้อความโหลดออกทาง Console และ CPU/Disk กำลังทำงาน ให้รอสักครู่ก่อนสรุปว่าโปรแกรมค้าง

## 7. ปุ่มควบคุมกล้อง

ตาม `main.cpp` เวอร์ชันที่ใช้อยู่:

- `W` — เคลื่อนกล้องไปข้างหน้า
- `S` — เคลื่อนกล้องถอยหลัง
- `A` — เคลื่อนกล้องไปทางซ้าย
- `D` — เคลื่อนกล้องไปทางขวา
- ขยับเมาส์ — เปลี่ยนทิศทางที่กล้องมอง
- `Esc` — ปิดหน้าต่าง

ตำแหน่งกล้องเริ่มต้น ทิศทางมอง ตำแหน่งแสง และ transform ของโมเดลถูกกำหนดใน `main.cpp`

## 8. แก้ปัญหาที่พบบ่อย

### หน้าต่างเปิดขึ้น แต่ยังไม่เห็นโมเดล

- ตรวจว่า Console แสดงข้อความ `[OBJ] โหลดสำเร็จ` หรือไม่
- ตรวจว่าโมเดลอยู่ในมุมมองกล้องหรือไม่ และตำแหน่ง/scale ใน `main.cpp` เหมาะสมหรือไม่
- ตรวจว่า shader โหลดสำเร็จและ uniform ใน `main.cpp` ตรงกับ shader

### โมเดลแสดงสีเดียวหรือบางส่วนเป็นสีเทา

- ตรวจว่า OBJ มีทั้ง `mtllib` และ `usemtl`
- ตรวจว่า `usemtl` ใน OBJ ตรงกับ `newmtl` ใน MTL
- ตรวจว่า Material มี `map_Kd` หรือค่า `Kd` ที่ต้องการ
- ดู log ว่ามีข้อความ `[MTL] ไม่พบรูป map_Kd` หรือไม่

### รูปภาพ texture ไม่พบ

- ตรวจว่าไฟล์ภาพมีอยู่จริงใน `Textures/`
- ตรวจชื่อไฟล์ใน `map_Kd` รวมช่องว่างและตัวพิมพ์
- เปลี่ยน absolute path เช่น `C:/.../file.jpg` เป็น relative filename เช่น `file.jpg` หากโครงสร้างโปรเจกต์ใช้โฟลเดอร์ `Textures/`

### โหลดช้าหลังจากลบองค์ประกอบใน Blender

MTL ที่ export อาจยังมี Material/Texture ที่ไม่ได้ใช้กับ geometry ที่เหลือ ให้ตรวจ `newmtl` และ `map_Kd` ใน MTL หากต้องการลดการโหลดที่ไม่จำเป็น สามารถลบเฉพาะบล็อก Material ที่ไม่ถูกใช้ออกจาก MTL ได้หลังจากสำรองไฟล์และตรวจ `usemtl` ใน OBJ แล้ว ห้ามลบบล็อกที่ยังมี geometry ใช้งานอยู่

### โมเดลเปลี่ยนตำแหน่งหรือทิศทางหลัง export

- ตรวจตัวเลือก Transform, Forward และ Up ระหว่าง export
- ตรวจตำแหน่ง/rotation/scale ของ object ใน Blender
- ทดลอง export เป็นชื่อใหม่และทดสอบก่อนเขียนทับไฟล์เดิม
- หากตำแหน่งใน Blender ถูกต้อง อย่าเปลี่ยน transform ใน C++ โดยไม่ตรวจเทียบก่อน

## 9. ข้อจำกัดของ loader

- อ่านข้อมูล OBJ ได้แก่ `v`, `vt`, `vn`, `f`, `mtllib` และ `usemtl`
- อ่านข้อมูล MTL ได้แก่ `newmtl`, `Kd` และ `map_Kd` ในรูปแบบที่โค้ดรองรับ
- แสดง diffuse/base-color texture และสี fallback ของ Material
- ไม่ได้ใช้ normal map, roughness map หรือ metallic map ใน shader loader นี้
- รูปภาพที่อ้างใน MTL อาจยังถูกพยายามโหลดแม้ Material นั้นไม่ถูกใช้ใน geometry หากยังมีบล็อก `newmtl`/`map_Kd` อยู่ใน MTL

## 10. แนวทางปฏิบัติที่แนะนำ

- เก็บไฟล์ต้นฉบับไว้เป็น backup
- ตั้งชื่อ OBJ และ MTL เป็นคู่เดียวกัน เช่น `meru-edited.obj` กับ `meru-edited.mtl`
- ตรวจบรรทัด `mtllib` ใน OBJ หลัง export ทุกครั้ง
- เก็บภาพ texture ไว้ใน `Textures/` และใช้ `map_Kd` แบบ relative path ที่ loader รองรับ
- หลังแก้ไขโมเดล ให้ทดสอบเปิด OBJ ใน Blender อีกครั้ง และทดสอบโปรแกรมพร้อมดู Console ก่อนแทนที่ไฟล์ที่ใช้งานจริง
