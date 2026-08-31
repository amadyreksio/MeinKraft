#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <vector>
#include <array>

#include <unordered_map>
#include <deque>
#include <thread>
#include <chrono>
#include <mutex>
#include <random>
#include <string>
#include <ctime>
#include <iomanip>
#include <sstream>
std::random_device rd;
std::uniform_real_distribution<float> rnd;
float RandomNumber(float min, float max) {
    return min + rnd(rd) * (max - min);
}
using uint = unsigned int;
const char* VertexSource = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout (location = 1) in vec2 textcoord;
layout (location = 2) in int AO;
out vec2 TextCoord;
out float ao;
uniform mat4 MVP;
void main(){
    ao=float(AO);
    TextCoord=textcoord;
    gl_Position=MVP*vec4(aPos,1.0);
}
)";
const char* FragmentSource = R"(
#version 330 core
out vec4 FragColor;
in vec2 TextCoord;
in float ao;
uniform sampler2D texture0;
void main(){
 float aoLevel = float(ao) / 3.0;
aoLevel+=0.1;
    FragColor = texture(texture0, TextCoord)*vec4(aoLevel, aoLevel, aoLevel, 1.0);
//vec4(aoLevel, aoLevel, aoLevel, 1.0);

//FragColor=vec4(aocl,0.0,0.0,1.0);
//
}
)";

const char* HighlightVertexSource = R"(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 MVP;
void main(){
    gl_Position=MVP*vec4(aPos,1.0);
}
)";
const char* HighlightFragmentSource = R"(
#version 330 core
out vec4 FragColor;
void main(){
FragColor=vec4(0.0,0.0,0.0,1.0);
}
)";
bool DoScreenshot = false;
uint ShaderProgram;
uint HighlightShaderProgram;
uint ATLAS;
bool mouseLocked = false;
struct camera {
    glm::vec3 position{ 0.0f }, front{ 0.0f,0.0f,-1.0f }, up{ 0.0f,1.0f,0.0f };
    float FOV = 70.0f;
    float TargetFOV = 70.0f;
    float FOV_Multiplier=1.0f;
    camera() {

    }
    camera(glm::vec3 position, glm::vec3 front, glm::vec3 up) : position(position), front(front), up(up) {

    }
};

struct vertex {
    float x, y, z;
    float u, v;
    uint8_t AO=0;
    vertex(float x, float y, float z, float u, float v) : x(x), y(y), z(z), u(u), v(v) {

    }
    vertex(float x, float y, float z, float u, float v, uint8_t AO) : x(x), y(y), z(z), u(u), v(v), AO(AO) {

    }
    vertex() {
        x = 0;y = 0;z = 0;u = 0;v = 0;
    }
};
enum block {
    AIR,
    STONE,
    COBBLE,
    DIRT,
    GRASS,
    COAL_ORE,
    IRON_ORE,
    GOLD_ORE,
    DIAMOND_ORE,
    BEDROCK
};
int currentblock = 1;
struct ChunkPos {
    int x, z;
    ChunkPos(int x, int z) : x(x), z(z) {

    }
    ChunkPos(glm::ivec3 pos) {
        x = pos.x, z = pos.z;
    }
    bool operator==(const ChunkPos& other)const {
        return(x == other.x && z == other.z);
    }

};

struct ChunkHash {

    size_t operator()(const ChunkPos& ch)const {
        std::hash<int> hash_f;
        size_t hash1 = hash_f(ch.x);
        size_t hash2 = hash_f(ch.z);
        return hash1 ^ (hash2 << 1);
    }
};
struct AABB {
    glm::vec3 position;
    glm::vec3 min;
    glm::vec3 max;
    bool active = true;
    AABB(glm::vec3 position, glm::vec3 min, glm::vec3 max) : position(position), min(min), max(max) {

    }
};
bool checkAABBCollision(const AABB& a, const AABB& b)
{
    if (!a.active || !b.active)
        return false;

    glm::vec3 aMin = a.min + a.position;
    glm::vec3 aMax = a.max + a.position;

    glm::vec3 bMin = b.min + b.position;
    glm::vec3 bMax = b.max + b.position;

    return
        aMin.x <= bMax.x && aMax.x >= bMin.x &&
        aMin.y <= bMax.y && aMax.y >= bMin.y &&
        aMin.z <= bMax.z && aMax.z >= bMin.z;
}
uint mvploc2;
void DrawAABB(const glm::vec3& min, const glm::vec3& max, const glm::mat4& vp)
{
    glm::vec3 corners[8] = {
        {min.x, min.y, min.z},
        {max.x, min.y, min.z},
        {max.x, max.y, min.z},
        {min.x, max.y, min.z},
        {min.x, min.y, max.z},
        {max.x, min.y, max.z},
        {max.x, max.y, max.z},
        {min.x, max.y, max.z}
    };

    uint indices[] = {
        0,1, 1,2, 2,3, 3,0, //front
        4,5, 5,6, 6,7, 7,4, //back
        0,4, 1,5, 2,6, 3,7  //connections
    };

    uint VAO, VBO, EBO;

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);

    glm::mat4 MVP = vp;
    glUniformMatrix4fv(mvploc2, 1, GL_FALSE, glm::value_ptr(MVP));

    //glUniform4f(DefaultShader.U.COLOR, 1.0f, 0.0f, 0.0f, 1.0f); // red

    glBindVertexArray(VAO);
    glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
}

class Player {
public:
    camera cam;
    glm::vec3 velocity{ 0.0f };
    glm::vec3 position{ 0.0f,3.0f,10.0f };
    float SPEED = 4.317f;
    float SPRINT_SPEED = 5.612f;
    int RenderDistance = 10;
    float reach = 4.5f;

    bool grounded = false;
    float GRAVITY = 25.0f;
    float JUMP_SPEED = 8.0f;

    AABB box = AABB({ 0.0f, 0.0f, 0.0f },{ -0.25f, -1.9f, -0.25f },{ 0.25f, 0.0f, 0.25f });

    Player() {

    }
    void tick() {
        box.position = position;
    }
};
Player player;
class chunk;
std::unordered_map<ChunkPos, chunk, ChunkHash> ChunkPool;


void UploadChunk(chunk* ch);

class chunk {
private:
    
public:
    uint VAO = 0;
    uint VBO, EBO;

    int indicessize = 0;
    bool generating = false;
    bool loading = false;
    bool generated = false;
    bool loaded = false;
    bool dirty = false;
    ChunkPos chunkPos = ChunkPos(0, 0);
    std::array<block, 16 * 16 * 256> BLOCKS{};
    chunk() {

    }
    ~chunk()
    {
        if (VAO)
            glDeleteVertexArrays(1, &VAO);

        if (VBO)
            glDeleteBuffers(1, &VBO);

        if (EBO)
            glDeleteBuffers(1, &EBO);
    }
    void Load() {
        if (!loaded || !generated) {

        }
    }



    void Generate() {
        UploadChunk(this);
    }




    void Render() {
        glBindVertexArray(VAO);
        glUseProgram(ShaderProgram);

        glDrawElements(GL_TRIANGLES, indicessize, GL_UNSIGNED_INT, NULL);
    }
    void SetBlock(glm::ivec3 position, block BlockType) {
        BLOCKS[GetID(position)] = BlockType;
        //dirty = true;
    }
    void RemoveBlock(glm::ivec3 position) {
        BLOCKS[GetID(position)] = AIR;
        //dirty = true;
    }
    void FillBlocks(glm::ivec3 pos1, glm::ivec3 pos2, block BlockType) {
        int minX = std::min(pos1.x, pos2.x);
        int minY = std::min(pos1.y, pos2.y);
        int minZ = std::min(pos1.z, pos2.z);

        int maxX = std::max(pos1.x, pos2.x);
        int maxY = std::max(pos1.y, pos2.y);
        int maxZ = std::max(pos1.z, pos2.z);

        for (int x = minX;x <= maxX;++x) {
            for (int y = minY;y <= maxY;++y) {
                for (int z = minZ;z <= maxZ;++z) {
                    SetBlock({ x,y,z }, BlockType);
                }
            }
        }
    }

    block GetBlockAt(glm::ivec3 position) {
        if (position.x < 0 || position.x >= 16 ||
            position.y < 0 || position.y >= 256 ||
            position.z < 0 || position.z >= 16)
            return AIR;

        int id = GetID(position);
        if (id < 0 || id >= sizeof(BLOCKS) / sizeof(block))return AIR;
        return BLOCKS[id];
    }
    int GetID(glm::ivec3 position) {
        int id = position.x * (16 * 256) + position.z * 256 + position.y;
        //if (id < 0 || id>sizeof(BLOCKS) / sizeof(block))return 0;
        return id;
    }
    glm::ivec3 GetPosition(int id) {
        int x = id / (16 * 256);
        id %= (16 * 256);

        int z = id / 256;
        int y = id % 256;

        return glm::ivec3(x, y, z);
    }
};

void GlobalSetBlockAt(glm::ivec3 position, block BlockType) {
    
    ChunkPos cp(
        static_cast<int>(floor(position.x / 16.0f)),
        static_cast<int>(floor(position.z / 16.0f))
    );
    glm::ivec3 local(
        position.x - cp.x * 16,
        position.y,
        position.z - cp.z * 16
    );
    if (local.x < 0 || local.x >= 16 ||
        local.y < 0 || local.y >= 256 ||
        local.z < 0 || local.z >= 16)
        return;

    auto [it, inserted] = ChunkPool.try_emplace(cp);

    it->second.SetBlock(local, BlockType);
    it->second.dirty = true;
    
}
void GlobalBreakBlock(glm::ivec3 position) {

    ChunkPos cp(
        static_cast<int>(floor(position.x / 16.0f)),
        static_cast<int>(floor(position.z / 16.0f))
    );
    glm::ivec3 local(
        position.x - cp.x * 16,
        position.y,
        position.z - cp.z * 16
    );
    if (local.x < 0 || local.x >= 16 ||
        local.y < 0 || local.y >= 256 ||
        local.z < 0 || local.z >= 16)
        return;

    auto [it, inserted] = ChunkPool.try_emplace(cp);

    it->second.RemoveBlock(local);
    it->second.dirty = true;

}
block GlobalGetBlockAt(glm::ivec3 position)
{
    ChunkPos cp(
        static_cast<int>(glm::floor(position.x / 16.0f)),
        static_cast<int>(glm::floor(position.z / 16.0f))
    );

    glm::ivec3 local(
        position.x - cp.x * 16,
        position.y,
        position.z - cp.z * 16
    );

    if (!ChunkPool.count(cp))
        return AIR;

    return ChunkPool.at(cp).GetBlockAt(local);
}

void UploadChunk(chunk* ch) {
    ch->generated = false;
    ch->generating = true;
    //ChunkGenerateJobs.push_back()
    std::vector<vertex> vertices = {};
    std::vector<uint> indices = {};


    std::vector<uint> Blockindices = {
        //Front
         0,  1,  2,
         2,  3,  0,

         //Back
          4,  5,  6,
          6,  7,  4,

          //Left
           8,  9, 10,
          10, 11,  8,

          //Right
          12, 13, 14,
          14, 15, 12,

          //Top
          16, 17, 18,
          18, 19, 16,

          //Bottom
          20, 21, 22,
          22, 23, 20
    };

    for (int i = 0;i < sizeof(ch->BLOCKS) / sizeof(block);i++) {
        if (ch->BLOCKS[i] == AIR)continue;
        std::array<vertex, 24> Blockvertices;
        int vertexCount = 0;

        glm::ivec3 pos = ch->GetPosition(i);


        float tileX = static_cast<int>(ch->BLOCKS[i]) % 32;
        float tileY = static_cast<int>(ch->BLOCKS[i]) / 32;

        //const float ATLAS_SIZE = 256.0f;
        //const float TILE_SIZE = 16.0f;
        //const float BORDER = 2.0f;
        //const float CELL_SIZE = TILE_SIZE + BORDER * 2.0f; // 20

        //float x0 = (tileX) * CELL_SIZE + BORDER;
        //float x1 = x0 + TILE_SIZE;

        //float y0 = tileY * CELL_SIZE + BORDER;
        //float y1 = y0 + TILE_SIZE;

        //float sX = x0 / ATLAS_SIZE;
        //float eX = x1 / ATLAS_SIZE;

        //float sY = 1.0f-(y0 / ATLAS_SIZE);
        //float eY = 1.0f-(y1 / ATLAS_SIZE);

        float x0 = tileX * 16.0f;
        float x1 = x0 + 16.0f;




        float y0 = tileY * 16.0f;
        float y1 = y0 + 16.0f;

        float tm = y0;
        y0 = y1;
        y1 = tm;

        float sX = x0 / 512.0f;
        float eX = x1 / 512.0f;

        float sY = 1.0f - (y0 / 512.0f);
        float eY = 1.0f - (y1 / 512.0f);


        //AMBIENT OCCLUSION
#pragma region AO



        uint8_t AO[6][4];
        
        auto CalculateAO = [&](glm::ivec3 normal, glm::ivec3 side1, glm::ivec3 side2, glm::ivec3 corner) -> uint8_t
            {
                bool s1 = (GlobalGetBlockAt(glm::ivec3(ch->chunkPos.x, 0, ch->chunkPos.z)*16 + pos + normal + side1) != AIR);
                bool s2 = (GlobalGetBlockAt(glm::ivec3(ch->chunkPos.x, 0, ch->chunkPos.z)*16 + pos + normal + side2) != AIR);
                bool c = (GlobalGetBlockAt(glm::ivec3(ch->chunkPos.x, 0, ch->chunkPos.z)*16 + pos + normal + side1 + side2) != AIR);
                if (s1 && s2) return 0; return 3 - (s1 + s2 + c);
            };

        AO[0][0] = CalculateAO(
            { 0, 0, 1 },
            { -1, 0, 0 },
            { 0, -1, 0 },
            { -1, -1, 0 }
        );

        AO[0][1] = CalculateAO(
            { 0, 0, 1 },
            { 1, 0, 0 },
            { 0, -1, 0 },
            { 1, -1, 0 }
        );

        AO[0][2] = CalculateAO(
            { 0, 0, 1 },
            { 1, 0, 0 },
            { 0, 1, 0 },
            { 1, 1, 0 }
        );

        AO[0][3] = CalculateAO(
            { 0, 0, 1 },
            { -1, 0, 0 },
            { 0, 1, 0 },
            { -1, 1, 0 }
        );


        AO[1][0] = CalculateAO(
            { 0, 0, -1 },
            { 1, 0, 0 },
            { 0, -1, 0 },
            { 1, -1, 0 }
        );

        AO[1][1] = CalculateAO(
            { 0, 0, -1 },
            { -1, 0, 0 },
            { 0, -1, 0 },
            { -1, -1, 0 }
        );

        AO[1][2] = CalculateAO(
            { 0, 0, -1 },
            { -1, 0, 0 },
            { 0, 1, 0 },
            { -1, 1, 0 }
        );

        AO[1][3] = CalculateAO(
            { 0, 0, -1 },
            { 1, 0, 0 },
            { 0, 1, 0 },
            { 1, 1, 0 }
        );


        AO[2][0] = CalculateAO(
            { -1, 0, 0 },
            { 0, 0, -1 },
            { 0, -1, 0 },
            { 0, -1, -1 }
        );

        AO[2][1] = CalculateAO(
            { -1, 0, 0 },
            { 0, 0, 1 },
            { 0, -1, 0 },
            { 0, -1, 1 }
        );

        AO[2][2] = CalculateAO(
            { -1, 0, 0 },
            { 0, 0, 1 },
            { 0, 1, 0 },
            { 0, 1, 1 }
        );

        AO[2][3] = CalculateAO(
            { -1, 0, 0 },
            { 0, 0, -1 },
            { 0, 1, 0 },
            { 0, 1, -1 }
        );


        AO[3][0] = CalculateAO(
            { 1, 0, 0 },
            { 0, 0, 1 },
            { 0, -1, 0 },
            { 0, -1, 1 }
        );

        AO[3][1] = CalculateAO(
            { 1, 0, 0 },
            { 0, 0, -1 },
            { 0, -1, 0 },
            { 0, -1, -1 }
        );

        AO[3][2] = CalculateAO(
            { 1, 0, 0 },
            { 0, 0, -1 },
            { 0, 1, 0 },
            { 0, 1, -1 }
        );

        AO[3][3] = CalculateAO(
            { 1, 0, 0 },
            { 0, 0, 1 },
            { 0, 1, 0 },
            { 0, 1, 1 }
        );


        AO[4][0] = CalculateAO(
            { 0, 1, 0 },
            { -1, 0, 0 },
            { 0, 0, 1 },
            { -1, 0, 1 }
        );

        AO[4][1] = CalculateAO(
            { 0, 1, 0 },
            { 1, 0, 0 },
            { 0, 0, 1 },
            { 1, 0, 1 }
        );

        AO[4][2] = CalculateAO(
            { 0, 1, 0 },
            { 1, 0, 0 },
            { 0, 0, -1 },
            { 1, 0, -1 }
        );

        AO[4][3] = CalculateAO(
            { 0, 1, 0 },
            { -1, 0, 0 },
            { 0, 0, -1 },
            { -1, 0, -1 }
        );

        AO[5][0] = CalculateAO(
            { 0, -1, 0 },
            { -1, 0, 0 },
            { 0, 0, -1 },
            { -1, 0, -1 }
        );

        AO[5][1] = CalculateAO(
            { 0, -1, 0 },
            { 1, 0, 0 },
            { 0, 0, -1 },
            { 1, 0, -1 }
        );

        AO[5][2] = CalculateAO(
            { 0, -1, 0 },
            { 1, 0, 0 },
            { 0, 0, 1 },
            { 1, 0, 1 }
        );

        AO[5][3] = CalculateAO(
            { 0, -1, 0 },
            { -1, 0, 0 },
            { 0, 0, 1 },
            { -1, 0, 1 }
        );


#pragma endregion
        //Front
        if (ch->GetBlockAt(pos + glm::ivec3{ 0,0,1 }) == AIR) {
            Blockvertices[vertexCount] = vertex(-0.5f, -0.5f, 0.5f, sX, sY, AO[0][0]);
            vertexCount++;
            Blockvertices[vertexCount] = vertex(0.5f, -0.5f, 0.5f, eX, sY, AO[0][1]);
            vertexCount++;
            Blockvertices[vertexCount] = vertex(0.5f, 0.5f, 0.5f, eX, eY, AO[0][2]);
            vertexCount++;
            Blockvertices[vertexCount] = vertex(-0.5f, 0.5f, 0.5f, sX, eY, AO[0][3]);
            vertexCount++;
        }
        //Back
        if (ch->GetBlockAt(pos + glm::ivec3{ 0,0,-1 }) == AIR) {
            Blockvertices[vertexCount] = vertex(0.5f, -0.5f, -0.5f, sX, sY, AO[1][0]);
            vertexCount++;
            Blockvertices[vertexCount] = vertex(-0.5f, -0.5f, -0.5f, eX, sY, AO[1][1]);
            vertexCount++;
            Blockvertices[vertexCount] = vertex(-0.5f, 0.5f, -0.5f, eX, eY, AO[1][2]);
            vertexCount++;
            Blockvertices[vertexCount] = vertex(0.5f, 0.5f, -0.5f, sX, eY, AO[1][3]);
            vertexCount++;
        }
        //Left
        if (ch->GetBlockAt(pos + glm::ivec3{ -1,0,0 }) == AIR) {
            Blockvertices[vertexCount] = vertex(-0.5f, -0.5f, -0.5f, sX, sY, AO[2][0]);
            vertexCount++;
            Blockvertices[vertexCount] = vertex(-0.5f, -0.5f, 0.5f, eX, sY, AO[2][1]);
            vertexCount++;
            Blockvertices[vertexCount] = vertex(-0.5f, 0.5f, 0.5f, eX, eY, AO[2][2]);
            vertexCount++;
            Blockvertices[vertexCount] = vertex(-0.5f, 0.5f, -0.5f, sX, eY, AO[2][3]);
            vertexCount++;
        }
        //Right
        if (ch->GetBlockAt(pos + glm::ivec3{ 1,0,0 }) == AIR) {
            Blockvertices[vertexCount] = vertex(0.5f, -0.5f, 0.5f, sX, sY, AO[3][0]);
            vertexCount++;
            Blockvertices[vertexCount] = vertex(0.5f, -0.5f, -0.5f, eX, sY, AO[3][1]);
            vertexCount++;
            Blockvertices[vertexCount] = vertex(0.5f, 0.5f, -0.5f, eX, eY, AO[3][2]);
            vertexCount++;
            Blockvertices[vertexCount] = vertex(0.5f, 0.5f, 0.5f, sX, eY, AO[3][3]);
            vertexCount++;
        }
        //Top
        if (ch->GetBlockAt(pos + glm::ivec3{ 0,1,0 }) == AIR) {
            Blockvertices[vertexCount] = vertex(-0.5f, 0.5f, 0.5f, sX, sY, AO[4][0]),
                vertexCount++;
            Blockvertices[vertexCount] = vertex(0.5f, 0.5f, 0.5f, eX, sY, AO[4][1]);
            vertexCount++;
            Blockvertices[vertexCount] = vertex(0.5f, 0.5f, -0.5f, eX, eY, AO[4][2]);
            vertexCount++;
            Blockvertices[vertexCount] = vertex(-0.5f, 0.5f, -0.5f, sX, eY, AO[4][3]);
            vertexCount++;
        }
        //Bottom
        if (ch->GetBlockAt(pos + glm::ivec3{ 0,-1,0 }) == AIR) {
            Blockvertices[vertexCount] = vertex(-0.5f, -0.5f, -0.5f, sX, sY, AO[5][0]);
            vertexCount++;
            Blockvertices[vertexCount] = vertex(0.5f, -0.5f, -0.5f, eX, sY, AO[5][1]);
            vertexCount++;
            Blockvertices[vertexCount] = vertex(0.5f, -0.5f, 0.5f, eX, eY, AO[5][2]);
            vertexCount++;
            Blockvertices[vertexCount] = vertex(-0.5f, -0.5f, 0.5f, sX, eY, AO[5][3]);
            vertexCount++;
        }


        uint offset = vertices.size();
        for (int i = 0;i < vertexCount;i++) {
            vertex vert = Blockvertices[i];


            vert.x += pos.x;
            vert.y += pos.y;
            vert.z += pos.z;
            //vert.u += sX;
            //vert.v += sY;
            vertices.push_back(vert);
        }

        for (uint i = 0; i < Blockvertices.size(); i += 4) {
            indices.push_back(offset + i + 0);
            indices.push_back(offset + i + 1);
            indices.push_back(offset + i + 2);

            indices.push_back(offset + i + 2);
            indices.push_back(offset + i + 3);
            indices.push_back(offset + i + 0);
        }
    }
    ch->indicessize = indices.size();



    glBindVertexArray(0);
    if (ch->VAO != 0) {
        glDeleteVertexArrays(1, &ch->VAO);
        if (ch->VBO != 0)
            glDeleteBuffers(1, &ch->VBO);
        if (ch->EBO != 0)
            glDeleteBuffers(1, &ch->EBO);
        ch->VAO = 0;
        ch->VBO = 0;
        ch->EBO = 0;
    }

    glGenVertexArrays(1, &ch->VAO);
    glGenBuffers(1, &ch->VBO);
    glGenBuffers(1, &ch->EBO);
    glBindVertexArray(ch->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, ch->VBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ch->EBO);


    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vertex), vertices.data(), GL_STATIC_DRAW);

    //pos
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (const void*)0);
    //uv
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (const void*)(3 * sizeof(float)));

    //AO
    glVertexAttribIPointer(2, 1, GL_UNSIGNED_BYTE, sizeof(vertex), (const void*)(5 * sizeof(float)));



    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint), indices.data(), GL_STATIC_DRAW);


    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);


    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    ch->generating = false;
    ch->generated = true;
    ch->dirty = false;
}

bool ChunkLoaded(ChunkPos position) {
    if (ChunkPool.count(position)) {
        chunk& ch = ChunkPool.at(position);
        return ch.generated && ch.loaded;
    }
    else {
        return false;
    }
}
bool ChunkGenerated(ChunkPos position) {
    if (ChunkPool.count(position)) {
        chunk& ch = ChunkPool.at(position);
        if (ch.generated) {
            return true;
        }
        else {
            return false;
        }
    }
    else {
        return false;
    }
}
void movement(float deltaTime);
float pitch = 0.0f;
float yaw = 0.0f;
int WIDTH=1920/2, HEIGHT=1080/2;
void resize(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
    WIDTH = width;
    HEIGHT = height;
}
void mouseMove(GLFWwindow* window, double xpos, double ypos) {
    static float lastX = xpos;
    static float lastY = ypos;
    static bool firstMouse = true;

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xOffset = xpos - lastX;
    float yOffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    if (!mouseLocked)return;
    yaw += xOffset * sensitivity;
    pitch += yOffset * sensitivity;

    pitch = glm::clamp(pitch, -89.0f, 89.0f);

    player.cam.front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    player.cam.front.y = sin(glm::radians(pitch));
    player.cam.front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

    player.cam.front = glm::normalize(player.cam.front);
}
void mouseDown(GLFWwindow* window, int button, int action, int mods) {
    //place block
    
    if (action == GLFW_PRESS&&button==GLFW_MOUSE_BUTTON_2) {
        float dist = 0.0f;
        float step = 0.1f;
        ChunkPos Hchunk(0, 0);
        glm::ivec3 localpos(0, 0, 0);
        glm::ivec3 hitBlock;
        glm::ivec3 previousBlock(0, 0, 0);
        bool hitblockset = false;
        bool previousset = false;
        ChunkPos cp(0,0);
        while (dist <= player.reach)
        {
            glm::vec3 ray =
                player.position +
                player.cam.position +
                player.cam.front * dist;

            glm::ivec3 blockPos = glm::ivec3(
                glm::floor(ray + glm::vec3(0.5f))
            );

            cp=ChunkPos(
                static_cast<int>(glm::floor(blockPos.x / 16.0f)),
                static_cast<int>(glm::floor(blockPos.z / 16.0f))
            );

            glm::ivec3 local(
                blockPos.x - cp.x * 16,
                blockPos.y,
                blockPos.z - cp.z * 16
            );

            if (ChunkPool.count(cp) &&
                ChunkPool.at(cp).GetBlockAt(local) != AIR)
            {
                localpos = local;
                Hchunk = cp;
                hitBlock = blockPos;
                hitblockset = true;
                break;
            }
            previousBlock = blockPos;
            previousset = true;
            dist += step;
        }
        if (previousset&& hitblockset) {//brb
            AABB blockAABB(
                previousBlock,
                { -0.5f, -0.5f, -0.5f },
                { 0.5f,  0.5f,  0.5f }
            );
            if(!checkAABBCollision(blockAABB,player.box))
                GlobalSetBlockAt(previousBlock, static_cast<block>(currentblock));
        }
    }
    if (action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_1) {
        float dist = 0.0f;
        float step = 0.1f;
        ChunkPos Hchunk(0, 0);
        glm::ivec3 localpos(0, 0, 0);
        glm::ivec3 hitBlock;
        glm::ivec3 previousBlock(0, 0, 0);
        bool hitblockset = false;
        bool previousset = false;
        ChunkPos cp(0, 0);
        while (dist <= player.reach)
        {
            glm::vec3 ray =
                player.position +
                player.cam.position +
                player.cam.front * dist;

            glm::ivec3 blockPos = glm::ivec3(
                glm::floor(ray + glm::vec3(0.5f))
            );

            cp = ChunkPos(
                static_cast<int>(glm::floor(blockPos.x / 16.0f)),
                static_cast<int>(glm::floor(blockPos.z / 16.0f))
            );

            glm::ivec3 local(
                blockPos.x - cp.x * 16,
                blockPos.y,
                blockPos.z - cp.z * 16
            );

            if (ChunkPool.count(cp) &&
                ChunkPool.at(cp).GetBlockAt(local) != AIR)
            {
                localpos = local;
                Hchunk = cp;
                hitBlock = blockPos;
                hitblockset = true;
                break;
            }
            previousBlock = blockPos;
            previousset = true;
            dist += step;
        }
        if (hitblockset) {
            GlobalBreakBlock(hitBlock);
        }
    }
}
void onScroll(GLFWwindow* window, double xoffset, double yoffset) {
    if (yoffset > 0) {
        currentblock += 1;
        if (currentblock > 9) {
            currentblock = 1;
        }
    }
    else if (yoffset < 0) {
        currentblock -= 1;
        if (currentblock < 1) {
            currentblock = 9;
        }
    }
}
float screenshottimer = 0.0f;
void keyDown(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_RELEASE) {
        mouseLocked = !mouseLocked;
        if (mouseLocked) {

            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        else {

            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
    else if (key == GLFW_KEY_F2 && action == GLFW_PRESS) {
        if (screenshottimer <= 0.0f) {
            DoScreenshot = true;
            screenshottimer = 0.5f;
        }

    }
}
void LoadChunks() {
    glm::vec3 plposdiv = player.position / 16.0f;
    for (int x = plposdiv.x - player.RenderDistance;x < plposdiv.x + player.RenderDistance;x++) {
        for (int z = plposdiv.z - player.RenderDistance;z < plposdiv.z + player.RenderDistance;z++) {
            if (glm::length(glm::vec3(x, 0, z) - plposdiv) <= player.RenderDistance) {
                //genchunk
                ChunkPos cp(static_cast<int>(x), static_cast<int>(z));
                if (!ChunkPool.count(cp)) {

                    auto [it, inserted] = ChunkPool.try_emplace(cp);
                    it->second.chunkPos = cp;

                    if (inserted)
                    {
                        it->second.FillBlocks({ 0,0,0 }, { 15,0,15 }, STONE);

                        /*it->second.FillBlocks({ 0,0,0 }, { 15,0,0 }, COBBLE);
                        it->second.FillBlocks({ 0,0,0 }, { 0,0,15 }, COBBLE);
                        it->second.FillBlocks({ 15,0,0 }, { 15,0,15 }, COBBLE);
                        it->second.FillBlocks({ 0,0,15 }, { 15,0,15 }, COBBLE);*/
                        /*STONE,
                            COBBLE,
                            DIRT,
                            GRASS,
                            COAL_ORE,
                            IRON_ORE,
                            GOLD_ORE,
                            DIAMOND_ORE,
                            BEDROCK*/
                       /* for (int i = 0;i < 10;i++) {
                            it->second.SetBlock({ i,1,0 }, static_cast<block>(i));
                        }*/
                        /*for (int i = 0;i < 15 * 15;i+=3) {
                            
                            int x = i % 15;
                            int z = i / 15;
                            it->second.SetBlock({ x,1,z }, COBBLE);
                            it->second.SetBlock({ x+1,1,z }, COBBLE);
                            it->second.SetBlock({ x+2,1,z }, COBBLE);
                            it->second.SetBlock({ x+1,2,z }, COBBLE);
                            it->second.SetBlock({ x+1,3,z }, COBBLE);
                        }*/
                        
                        
                        
                        it->second.Generate();
                        it->second.loaded = true;
                    }

                }
                else if (ChunkPool.at(cp).dirty) {
                    //regen
                    ChunkPool.at(cp).Generate();
                }
            }

        }

    }
}
GLFWwindow* window;
uint HighLightVAO;
void ScreenShot();
int main()
{
#pragma region Init
    if (!glfwInit()) {
        return -1;
    }
    window = glfwCreateWindow(960, 540, "MeinKampf", NULL, NULL);
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        return -1;
    }
#pragma endregion



#pragma region Shaderki
    {
        uint VS = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(VS, 1, &VertexSource, NULL);
        glCompileShader(VS);
        {
            int success;
            glGetShaderiv(VS, GL_COMPILE_STATUS, &success);
            if (!success) {
                char log[512];
                glGetShaderInfoLog(VS, 512, nullptr, log);
                std::cout << "VERTEX SHADER ERROR: " << log;
            }

        }


        uint FS = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(FS, 1, &FragmentSource, NULL);
        glCompileShader(FS);
        {
            int success;
            glGetShaderiv(FS, GL_COMPILE_STATUS, &success);
            if (!success) {
                char log[512];
                glGetShaderInfoLog(FS, 512, nullptr, log);
                std::cout << "FRAGMENT SHADER ERROR: " << log;
            }

        }

        ShaderProgram = glCreateProgram();
        glAttachShader(ShaderProgram, VS);
        glAttachShader(ShaderProgram, FS);
        glLinkProgram(ShaderProgram);

        glDeleteShader(VS);
        glDeleteShader(FS);
    }


    //Block highligh shader
    {
        uint VS = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(VS, 1, &HighlightVertexSource, NULL);
        glCompileShader(VS);


        uint FS = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(FS, 1, &HighlightFragmentSource, NULL);
        glCompileShader(FS);

        HighlightShaderProgram = glCreateProgram();
        glAttachShader(HighlightShaderProgram, VS);
        glAttachShader(HighlightShaderProgram, FS);
        glLinkProgram(HighlightShaderProgram);

        glDeleteShader(VS);
        glDeleteShader(FS);
    }


#pragma endregion
    glfwSetKeyCallback(window, keyDown);
    glfwSetCursorPosCallback(window, mouseMove);
    glfwSetFramebufferSizeCallback(window, resize);
    glfwSetMouseButtonCallback(window, mouseDown);
    glfwSetScrollCallback(window, onScroll);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

   /* glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);*/

    glViewport(0, 0, 960, 540);
    glClearColor(66.0f / 255.0f, 135.0f / 255.0f, 245.0f / 255.0f, 1.0f);


#pragma region Load Texture Atlas
    stbi_set_flip_vertically_on_load(true);
    int width, height, channels;
    const unsigned char* data = stbi_load("assets/block.png", &width, &height, &channels, 4);


    glGenTextures(1, &ATLAS);
    glBindTexture(GL_TEXTURE_2D, ATLAS);

    glTexImage2D(GL_TEXTURE_2D, NULL, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
        GL_NEAREST);

    //glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, -1.5f);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenerateMipmap(GL_TEXTURE_2D);


#pragma endregion

#pragma region Create Highlight VAO
    {
        std::vector<vertex> Cube = {
            // Front
            vertex(-0.5f, -0.5f,  0.5f, 0, 0),
            vertex(0.5f, -0.5f,  0.5f, 0, 0),

            vertex(0.5f, -0.5f,  0.5f, 0, 0),
            vertex(0.5f,  0.5f,  0.5f, 0, 0),

            vertex(0.5f,  0.5f,  0.5f, 0, 0),
            vertex(-0.5f,  0.5f,  0.5f, 0, 0),

            vertex(-0.5f,  0.5f,  0.5f, 0, 0),
            vertex(-0.5f, -0.5f,  0.5f, 0, 0),

            // Back
            vertex(-0.5f, -0.5f, -0.5f, 0, 0),
            vertex(0.5f, -0.5f, -0.5f, 0, 0),

            vertex(0.5f, -0.5f, -0.5f, 0, 0),
            vertex(0.5f,  0.5f, -0.5f, 0, 0),

            vertex(0.5f,  0.5f, -0.5f, 0, 0),
            vertex(-0.5f,  0.5f, -0.5f, 0, 0),

            vertex(-0.5f,  0.5f, -0.5f, 0, 0),
            vertex(-0.5f, -0.5f, -0.5f, 0, 0),

            // Left
            vertex(-0.5f, -0.5f, -0.5f, 0, 0),
            vertex(-0.5f, -0.5f,  0.5f, 0, 0),

            vertex(-0.5f,  0.5f, -0.5f, 0, 0),
            vertex(-0.5f,  0.5f,  0.5f, 0, 0),

            // Right
            vertex(0.5f, -0.5f, -0.5f, 0, 0),
            vertex(0.5f, -0.5f,  0.5f, 0, 0),

            vertex(0.5f,  0.5f, -0.5f, 0, 0),
            vertex(0.5f,  0.5f,  0.5f, 0, 0),

            // Bottom
            vertex(-0.5f, -0.5f, -0.5f, 0, 0),
            vertex(0.5f, -0.5f, -0.5f, 0, 0),

            vertex(-0.5f, -0.5f,  0.5f, 0, 0),
            vertex(0.5f, -0.5f,  0.5f, 0, 0),

            // Top
            vertex(-0.5f,  0.5f, -0.5f, 0, 0),
            vertex(0.5f,  0.5f, -0.5f, 0, 0),

            vertex(-0.5f,  0.5f,  0.5f, 0, 0),
            vertex(0.5f,  0.5f,  0.5f, 0, 0)
        };
        uint vbo;
        glGenVertexArrays(1, &HighLightVAO);
        glGenBuffers(1, &vbo);

        glBindVertexArray(HighLightVAO);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        glBufferData(GL_ARRAY_BUFFER, Cube.size() * sizeof(vertex), Cube.data(), GL_STATIC_DRAW);
        //pos
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (const void*)0);

        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
#pragma endregion




    float lasttime = 0.0f;
    //chunk TESTCHUNK;


    //TESTCHUNK.Generate();

    
    
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1)) {
            mouseLocked = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        float time = glfwGetTime();
        float deltaTime = time - lasttime;
        lasttime = time;
        player.tick();
        movement(deltaTime);
        LoadChunks();
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        player.cam.FOV +=
            (player.cam.TargetFOV * player.cam.FOV_Multiplier - player.cam.FOV)
            * 5.0f * deltaTime;
        glm::mat4 projection = glm::perspective(glm::radians(player.cam.FOV), static_cast<float>(width) / static_cast<float>(height), 0.1f, 1000.0f);
        glm::mat4 view = glm::lookAt(player.position + player.cam.position, player.position + player.cam.position + player.cam.front, player.cam.up);
        glUseProgram(ShaderProgram);

        uint mvploc = glGetUniformLocation(ShaderProgram, "MVP");
        uint textureloc = glGetUniformLocation(ShaderProgram, "texture0");


        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, ATLAS);
        glUniform1i(textureloc, 0);

        //TESTCHUNK.Render();

        glm::vec3 plposdiv = player.position / 16.0f;
        for (int x = plposdiv.x - player.RenderDistance;x < plposdiv.x + player.RenderDistance;x++) {
            for (int z = plposdiv.z - player.RenderDistance;z < plposdiv.z + player.RenderDistance;z++) {
                if (glm::length(glm::vec3(x, 0, z) - plposdiv) <= player.RenderDistance) {
                    //genchunk
                    ChunkPos cp(static_cast<int>(x), static_cast<int>(z));
                    if (ChunkPool.count(cp)) {
                        glm::mat4 model(1.0f);
                        model = glm::translate(model, glm::vec3(cp.x, 0.0f, cp.z) * 16.0f);
                        glm::mat4 MVP = projection * view * model;
                        glUniformMatrix4fv(mvploc, 1, GL_FALSE, glm::value_ptr(MVP));
                        ChunkPool.at(cp).Render();


                    }
                }

            }

        }

        //block highlights
        float dist = 0.0f;
        float step = 0.1f;
        mvploc2 = glGetUniformLocation(HighlightShaderProgram, "MVP");
        glUseProgram(HighlightShaderProgram);
        ChunkPos Hchunk(0,0);
        glm::ivec3 localpos(0,0,0);
        glm::ivec3 hitBlock;
        glm::ivec3 previousBlock(0,0,0);
        bool hitblock = false;
        while (dist <= player.reach)
        {
            glm::vec3 ray =
                player.position +
                player.cam.position +
                player.cam.front * dist;

            glm::ivec3 blockPos = glm::ivec3(
                glm::floor(ray + glm::vec3(0.5f))
            );

            ChunkPos cp(
                static_cast<int>(glm::floor(blockPos.x / 16.0f)),
                static_cast<int>(glm::floor(blockPos.z / 16.0f))
            );

            glm::ivec3 local(
                blockPos.x - cp.x * 16,
                blockPos.y,
                blockPos.z - cp.z * 16
            );

            if (ChunkPool.count(cp) &&
                ChunkPool.at(cp).GetBlockAt(local) != AIR)
            {
                localpos = local;
                Hchunk = cp;
                hitBlock = blockPos;
                hitblock = true;
                break;
            }
            previousBlock = blockPos;
            dist += step;
        }
        glBindVertexArray(HighLightVAO);

        glm::vec3 blockWorldPos(
            Hchunk.x * 16.0f + localpos.x,
            localpos.y,
            Hchunk.z * 16.0f + localpos.z
        );

        glm::mat4 model(1.0f);
        model = glm::translate(model, blockWorldPos);
        model = glm::scale(model, glm::vec3(1.005f, 1.005f, 1.005f));
        glm::mat4 MVP = projection * view * model;
        if (hitblock) {
            glUniformMatrix4fv(mvploc2, 1, GL_FALSE, glm::value_ptr(MVP));
            glLineWidth(2.0f);
            glDrawArrays(GL_LINES, 0, 32);
        }
        //test
        //DrawAABB(player.box.min+player.box.position, player.box.max+player.box.position, projection * view);
        if (screenshottimer > 0.0f) {
            screenshottimer -= deltaTime;
        }
        if (DoScreenshot&&screenshottimer<=0.0f) {
            ScreenShot();
        }

        glfwSwapBuffers(window);
        
    }
}
bool CollidesWithBlocks(const AABB& box)
{
    glm::vec3 worldMin = box.min + box.position;
    glm::vec3 worldMax = box.max + box.position;

    int minX = static_cast<int>(glm::floor(worldMin.x + 0.5f));
    int maxX = static_cast<int>(glm::floor(worldMax.x + 0.5f));

    int minY = static_cast<int>(glm::floor(worldMin.y + 0.5f));
    int maxY = static_cast<int>(glm::floor(worldMax.y + 0.5f));

    int minZ = static_cast<int>(glm::floor(worldMin.z + 0.5f));
    int maxZ = static_cast<int>(glm::floor(worldMax.z + 0.5f));

    minY = std::max(minY, 0);
    maxY = std::min(maxY, 255);

    for (int x = minX; x <= maxX; ++x)
    {
        for (int y = minY; y <= maxY; ++y)
        {
            for (int z = minZ; z <= maxZ; ++z)
            {
                block b = GlobalGetBlockAt({ x, y, z });

                if (b == AIR)
                    continue;

                glm::vec3 blockPosition(
                    static_cast<float>(x),
                    static_cast<float>(y),
                    static_cast<float>(z)
                );

                AABB blockAABB(
                    blockPosition,
                    { -0.5f, -0.5f, -0.5f },
                    { 0.5f,  0.5f,  0.5f }
                );

                if (checkAABBCollision(box, blockAABB))
                    return true;
            }
        }
    }

    return false;
}

//i hate physics with all my heart
void movement(float deltaTime)
{
    deltaTime = glm::min(deltaTime, 0.05f);
    float speed = player.SPEED;
    glm::vec3 input(0, 0, 0);
    if (glfwGetKey(window, GLFW_KEY_W)) {
        input.x += 1.0f;
    }
    if (glfwGetKey(window, GLFW_KEY_S)) {
        input.x -= 1.0f;
    }
    if (glfwGetKey(window, GLFW_KEY_A)) {
        input.z -= 1.0f;
    }
    if (glfwGetKey(window, GLFW_KEY_D)) {
        input.z += 1.0f;
    }

    if (glm::length(input) > 0.0f) {
        input = glm::normalize(input);
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
        {
            speed = player.SPRINT_SPEED;
            player.cam.FOV_Multiplier = 1.15f;
        }
        else
        {
            player.cam.FOV_Multiplier = 1.0f;
        }
    }
    glm::vec3 forward = glm::normalize(player.cam.front);
    forward.y = 0.0f;
    if (glm::length(forward) > 0.0f)
        forward = glm::normalize(forward);

    glm::vec3 right = glm::normalize(glm::cross(forward, player.cam.up));
    //right.y = 0.0f;
   
    glm::vec3 dir = forward * input.x + right * input.z;
    if (!player.grounded) {
        //speed *= 1.27;
    }
    glm::vec3 horMove = dir * speed * deltaTime;

    if (horMove.x != 0.0f) {
        AABB testbox = player.box;
        testbox.position.x += horMove.x;
        if (!CollidesWithBlocks(testbox)) {
            player.position.x += horMove.x;
        }
    }
    if (horMove.z != 0.0f) {
        AABB testbox = player.box;
        testbox.position.z += horMove.z;
        if (!CollidesWithBlocks(testbox)) {
            player.position.z += horMove.z;
        }
    }
    player.velocity.y -= player.GRAVITY * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS &&
        player.grounded)
    {
        player.velocity.y = player.JUMP_SPEED;
        player.grounded = false;
    }

    float verMove = player.velocity.y * deltaTime;
    if (verMove != 0.0f) {
        AABB testbox = player.box;
        testbox.position.y += verMove;
        if (!CollidesWithBlocks(testbox)) {
            player.position.y += verMove;
            player.grounded = false;
        }
        else {
            if (player.velocity.y < 0.0f) {
                player.grounded = true;
            }
            else {
                player.grounded = false;
            }
            player.velocity.y = 0.0f;
        }
    }
    player.box.position = player.position;
    
}

void ScreenShot() {
    std::vector<unsigned char> pixels(WIDTH * HEIGHT* 3);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    glReadPixels(0, 0,WIDTH, HEIGHT,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());

    for (int y = 0; y < HEIGHT / 2; ++y)
    {
        unsigned char* row1 = pixels.data() + y * WIDTH * 3;
        unsigned char* row2 = pixels.data() + (HEIGHT - 1 - y) * WIDTH * 3;

        for (int x = 0; x < WIDTH * 3; ++x)
            std::swap(row1[x], row2[x]);
    }
   
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);

    std::tm tm{};

#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    std::ostringstream filename;
    filename << "SCREENSHOT-"
        << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S")
        << ".png";

    std::string path = filename.str();

    std::cout << path.c_str();
    if (!stbi_write_png(path.c_str(), WIDTH, HEIGHT, 3, pixels.data(), WIDTH * 3)) {
        std::cerr<<"chuj";
        
        return;
    }
    DoScreenshot = false;
    screenshottimer = 0.5f;
    

}