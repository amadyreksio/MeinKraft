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

#include <functional>

#include "perlin.h"
#include "noisenoise.h"

const int sky_limit=256;
std::random_device rd;
std::uniform_real_distribution<float> rnd;
enum GameStates {
    MENU,WORLD_CHOOSE,INGAME
};

GameStates GAME_STATE=MENU;
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
aoLevel = mix(1.0, aoLevel, 0.8);
aoLevel=min(1.0,aoLevel);
vec4 texColor=texture(texture0, TextCoord);
if(texColor.a<0.1)discard;
    FragColor = vec4(texColor.rgb * aoLevel, 1.0);
//vec4(aoLevel, aoLevel, aoLevel, 1.0);

//FragColor=vec4(aocl,0.0,0.0,1.0);
//
}
)";

const char* SimpleVertexSource = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout (location = 1) in vec2 textcoord;
out vec2 TextCoord;
uniform mat4 MVP;
void main(){
    TextCoord=textcoord;
    gl_Position=MVP*vec4(aPos,1.0);
}
)";
const char* SimpleFragmentSource = R"(
#version 330 core
out vec4 FragColor;
in vec2 TextCoord;
uniform sampler2D texture0;
void main(){
    FragColor = texture(texture0, TextCoord);
}
)";


const char* UIVertexSource = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout (location = 1) in vec2 textcoord;
out vec2 TextCoord;
uniform mat4 MVP;

void main(){
    TextCoord=textcoord;
    gl_Position=MVP*vec4(aPos,1.0);
}
)";
const char* UIFragmentSource = R"(
#version 330 core
out vec4 FragColor;
in vec2 TextCoord;
uniform vec4 texcoord;
uniform int useTexture;
uniform sampler2D texture0;
void main(){
if(useTexture==1){
    vec2 UV=TextCoord*vec2(texcoord[2],texcoord[3])+vec2(texcoord[0],texcoord[1]);
    FragColor = texture(texture0, UV);
}else{
    FragColor=vec4(1.0,0.0,0.0,1.0);
}
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
uniform vec4 color;
void main(){
FragColor=color;
}
)";
bool DoScreenshot = false;
uint ShaderProgram;
uint SimpleShaderProgram;
uint UIShaderProgram;
uint HighlightShaderProgram;
uint ATLAS;
uint UITEXTURE;
uint SUBTITLES_TEXTURE;
uint MENU_BG;
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
enum block : uint8_t {
    AIR,
    STONE,
    COBBLE,
    DIRT,
    GRASS,
    COAL_ORE,
    IRON_ORE,
    GOLD_ORE,
    DIAMOND_ORE,
    BEDROCK,
    WHITE_CONCRETE,
    ORANGE_CONCRETE,
    MAGENTA_CONCRETE,
    LIGHT_BLUE_CONCRETE,
    YELLOW_CONCRETE,
    LIME_CONCRETE,
    PINK_CONCRETE,
    GRAY_CONCRETE,
    LIGHT_GRAY_CONCRETE,
    CYAN_CONCRETE,
    PURPLE_CONCRETE,
    BLUE_CONCRETE,
    BROWN_CONCRETE,
    GREEN_CONCRETE,
    RED_CONCRETE,
    BLACK_CONCRETE,
    OAK_LOG,
    OAK_LEAVES,

};
struct BlockTextureMapping {
    int TOP = 0, BOTTOM = 0, LEFT = 0, RIGHT = 0, FRONT = 0, BACK = 0;
    BlockTextureMapping(int sides) {
        TOP = sides;
        BOTTOM = sides;
        LEFT = sides;
        RIGHT = sides;
        FRONT = sides;
        BACK = sides;
    }
    BlockTextureMapping(int Top, int Bottom, int Left, int Right, int Front, int Back) : TOP(Top), BOTTOM(Bottom), LEFT(Left), RIGHT(Right), FRONT(Front), BACK(Back) {

    }
};
std::unordered_map<block, BlockTextureMapping> texturemappings = {
    {STONE, {1}},
    {COBBLE, {2}},
    {DIRT, {3}},
    {GRASS, {5,3,4,4,4,4}},
    {COAL_ORE, {6}},
    {IRON_ORE, {7}},
    {GOLD_ORE, {8}},
    {DIAMOND_ORE, {9}},
    {BEDROCK, {10}},
    {WHITE_CONCRETE, {11}},
    {ORANGE_CONCRETE, {12}},
    {MAGENTA_CONCRETE, {13}},
    {LIGHT_BLUE_CONCRETE, {14}},
    {YELLOW_CONCRETE, {15}},
    {LIME_CONCRETE, {16}},
    {PINK_CONCRETE, {17}},
    {GRAY_CONCRETE, {18}},
    {LIGHT_GRAY_CONCRETE, {19}},
    {CYAN_CONCRETE, {20}},
    {PURPLE_CONCRETE, {21}},
    {BLUE_CONCRETE, {22}},
    {BROWN_CONCRETE, {23}},
    {GREEN_CONCRETE, {24}},
    {RED_CONCRETE, {25}},
    {BLACK_CONCRETE, {26}},
    {OAK_LOG, {28,28,27,27,27,27}},
    {OAK_LEAVES, {29}},

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
    ChunkPos operator+(const ChunkPos& other) {
        return ChunkPos(x + other.x, z + other.z);
    }
    ChunkPos operator-(const ChunkPos& other) {
        return ChunkPos(x - other.x, z - other.z);
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
uint highlightcolorloc;
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
uint UIVAO;
uint crosshair;
float mx=0, my=0;
struct UIElement {
private:

public:
    bool useTexture = false;
    bool visible=true;
    std::string name="";
    glm::vec2 TextureLocation{ 0,0 };
    glm::vec2 TextureSize{100,100};
    glm::vec4 BackgroundColor{ 0.0,0.0,0.0,1.0 };
    glm::vec2 AnchorPoint{ 0.0f };
    glm::vec2 Size{ 0.0f,0.0f };
    glm::vec2 SizeOffset{ 0.0f,0.0f };
    glm::vec2 Location{ 0.0f,0.0f };
    glm::vec2 LocationOffset{ 0.0f,0.0f };
    glm::vec2 Scale{ 1.0f,1.0f };
    float rotation = 0.0f;
    std::function<void()> Tick = []() {};
    UIElement(std::string name, glm::vec2 Location, glm::vec2 Size, glm::vec4 BackgroundColor) {
        this->name = name;
        this->Location = Location;
        this->Size = Size;
        this->BackgroundColor = BackgroundColor;
    }
    

};

std::vector<UIElement> UI;


class Player {
public:
    camera cam;
    glm::vec3 velocity{ 0.0f };
    glm::dvec3 position{ 0.0f,6.0f,10.0f };
    float SPEED = 4.317f;
    float SPRINT_SPEED = 5.612f;
    int RenderDistance = 25;
    float reach = 5.0f;

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
void UploadChunk2(chunk* ch, glm::ivec3 pos);
struct subchunk {
public:
    std::array<block, 16 * 16 * 16> BLOCKS{};
};
class chunk {
private:
    
public:
    std::vector<vertex> vertices = {};
    std::vector<uint> indices = {};
    uint VAO = 0;
    uint VBO, EBO;
    bool AOupdated = false;
    bool structuresGenerated = false;
    int indicessize = 0;
    bool generating = false;
    bool loading = false;
    bool generated = false;
    float generatedchunk = false;
    bool loaded = false;
    bool dirty = false;
    ChunkPos chunkPos = ChunkPos(0, 0);
    std::array<block, 16 * 16 * sky_limit> BLOCKS{};
    //std::array<subchunk, 16> SUBCHUNKS{};
    chunk() {
        //SUBCHUNKS.fill(subchunk{});
    }
    ~chunk()
    {
        /*if (VAO)
            glDeleteVertexArrays(1, &VAO);

        if (VBO)
            glDeleteBuffers(1, &VBO);

        if (EBO)
            glDeleteBuffers(1, &EBO);*/
    }
    void Load() {
        if (!loaded || !generated) {

        }
    }



    void Generate() {
        UploadChunk(this);
    }
    void Generate2(glm::ivec3 pos) {
        UploadChunk2(this,pos);
    }




    void Render() {
        glBindVertexArray(VAO);
        glUseProgram(ShaderProgram);

        glDrawElements(GL_TRIANGLES, indicessize, GL_UNSIGNED_INT, NULL);
    }
    void SetBlock(glm::ivec3 position, block BlockType) {
        //int sc = position.y / 16;
        //int x = position.x;
        //int z = position.z;
        //SUBCHUNKS[sc].BLOCKS;

        //int localbly = position.y - sc*16;
        //BLOCKS[GetID(glm::ivec3(x, localbly,z))] = BlockType;
        if (position.x < 0 || position.z < 0)return;
        int id = GetID(position);
        if (id > BLOCKS.size())return;
        BLOCKS[id] = BlockType;
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
            position.y < 0 || position.y >= sky_limit ||
            position.z < 0 || position.z >= 16)
            return AIR;

        int id = GetID(position);
        if (id < 0 || id >= sizeof(BLOCKS) / sizeof(block))return AIR;
        return BLOCKS[id];
    }
    int GetID(glm::ivec3 position) {
        int id = position.x * (16 * sky_limit) + position.z * sky_limit + position.y;
        //if (id < 0 || id>sizeof(BLOCKS) / sizeof(block))return 0;
        return id;
    }
    glm::ivec3 GetPosition(int id) {
        int x = id / (16 * sky_limit);
        id %= (16 * sky_limit);

        int z = id / sky_limit;
        int y = id % sky_limit;

        return glm::ivec3(x, y, z);
    }
};
void GlobalSetBlockAtNoDirty(glm::ivec3 position, block BlockType) {
    ChunkPos cp(
        static_cast<int>(floor(position.x / 16.0)),
        static_cast<int>(floor(position.z / 16.0))
    );
    glm::ivec3 local(
        position.x - cp.x * 16,
        position.y,
        position.z - cp.z * 16
    );
    if (local.x < 0 || local.x >= 16 ||
        local.y < 0 || local.y >= sky_limit ||
        local.z < 0 || local.z >= 16)
        return;

    auto [it, inserted] = ChunkPool.try_emplace(cp);

    it->second.SetBlock(local, BlockType);
    
    //it->second.Generate2(local);
}

void GlobalSetBlockAt(glm::ivec3 position, block BlockType) {
    
    ChunkPos cp(
        static_cast<int>(floor(position.x / 16.0)),
        static_cast<int>(floor(position.z / 16.0))
    );
    glm::ivec3 local(
        position.x - cp.x * 16,
        position.y,
        position.z - cp.z * 16
    );
    if (local.x < 0 || local.x >= 16 ||
        local.y < 0 || local.y >= sky_limit ||
        local.z < 0 || local.z >= 16)
        return;

    auto [it, inserted] = ChunkPool.try_emplace(cp);

    it->second.SetBlock(local, BlockType);
    it->second.dirty = true;
    //it->second.Generate2(local);
    if (local.x > 14) {
        ChunkPos cp2(
            static_cast<int>(floor((position.x+1) / 16.0)),
            static_cast<int>(floor(position.z / 16.0))
        );
        if (ChunkPool.count(cp2)) {
            ChunkPool.at(cp2).dirty = true;
        }
    }
    if (local.x < 2) {
        ChunkPos cp2(
            static_cast<int>(floor((position.x - 1) / 16.0)),
            static_cast<int>(floor(position.z / 16.0))
        );
        if (ChunkPool.count(cp2)) {
            ChunkPool.at(cp2).dirty = true;
        }
    }
    if (local.z > 14) {
        ChunkPos cp2(
            static_cast<int>(floor((position.x) / 16.0)),
            static_cast<int>(floor((position.z+1) / 16.0))
        );
        if (ChunkPool.count(cp2)) {
            ChunkPool.at(cp2).dirty = true;
        }
    }
    if (local.z < 2) {
        ChunkPos cp2(
            static_cast<int>(floor((position.x) / 16.0)),
            static_cast<int>(floor((position.z - 1) / 16.0))
        );
        if (ChunkPool.count(cp2)) {
            ChunkPool.at(cp2).dirty = true;
        }
    }
    
}
void GlobalBreakBlock(glm::ivec3 position) {

    ChunkPos cp(
        static_cast<int>(floor(position.x / 16.0)),
        static_cast<int>(floor(position.z / 16.0))
    );
    glm::ivec3 local(
        position.x - cp.x * 16,
        position.y,
        position.z - cp.z * 16
    );
    if (local.x < 0 || local.x >= 16 ||
        local.y < 0 || local.y >= sky_limit ||
        local.z < 0 || local.z >= 16)
        return;

    auto [it, inserted] = ChunkPool.try_emplace(cp);

    it->second.RemoveBlock(local);
    it->second.dirty = true;

    if (local.x > 14) {
        ChunkPos cp2(
            static_cast<int>(floor((position.x + 1) / 16.0)),
            static_cast<int>(floor(position.z / 16.0))
        );
        if (ChunkPool.count(cp2)) {
            ChunkPool.at(cp2).dirty = true;
        }
    }
    if (local.x < 2) {
        ChunkPos cp2(
            static_cast<int>(floor((position.x - 1) / 16.0)),
            static_cast<int>(floor(position.z / 16.0))
        );
        if (ChunkPool.count(cp2)) {
            ChunkPool.at(cp2).dirty = true;
        }
    }
    if (local.z > 14) {
        ChunkPos cp2(
            static_cast<int>(floor((position.x) / 16.0)),
            static_cast<int>(floor((position.z + 1) / 16.0))
        );
        if (ChunkPool.count(cp2)) {
            ChunkPool.at(cp2).dirty = true;
        }
    }
    if (local.z < 2) {
        ChunkPos cp2(
            static_cast<int>(floor((position.x) / 16.0)),
            static_cast<int>(floor((position.z - 1) / 16.0))
        );
        if (ChunkPool.count(cp2)) {
            ChunkPool.at(cp2).dirty = true;
        }
    }

}
block GlobalGetBlockAt(glm::ivec3 position)
{
    ChunkPos cp(
        static_cast<int>(glm::floor(position.x / 16.0)),
        static_cast<int>(glm::floor(position.z / 16.0))
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

void UploadSubChunk(subchunk* ch) {

}


void UploadChunk(chunk* ch) {
    ch->generated = false;
    ch->generating = true;
    //ChunkGenerateJobs.push_back()
    
    ch->vertices.clear();
    ch->indices.clear();



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
    auto GetNeighborBlock = [&](glm::ivec3 localPos) -> block
        {
            glm::ivec3 worldPos =
                glm::ivec3(ch->chunkPos.x * 16, 0, ch->chunkPos.z * 16)
                + localPos;

            return GlobalGetBlockAt(worldPos);
        };
    auto IsTransparentBlock = [](block Block)->bool {
        if (Block == OAK_LEAVES) {
            return true;
        }
        else {
            return false;
        }
        };
    for (int i = 0;i < sizeof(ch->BLOCKS) / sizeof(block);i++) {
        if (ch->BLOCKS[i] == AIR)continue;
        std::array<vertex, 24> Blockvertices;
        int vertexCount = 0;

        glm::ivec3 pos = ch->GetPosition(i);

        bool transparent = IsTransparentBlock(ch->BLOCKS[i]);
        

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

        


        //AMBIENT OCCLUSION
#pragma region AO



        uint8_t AO[6][4];
        
        auto CalculateAO = [&](glm::ivec3 normal, glm::ivec3 side1, glm::ivec3 side2, glm::ivec3 corner) -> uint8_t
            {
                block b1= GlobalGetBlockAt(glm::ivec3(ch->chunkPos.x, 0, ch->chunkPos.z) * 16 + pos + normal + side1);
                block b2= GlobalGetBlockAt(glm::ivec3(ch->chunkPos.x, 0, ch->chunkPos.z) * 16 + pos + normal + side2);
                block b3= GlobalGetBlockAt(glm::ivec3(ch->chunkPos.x, 0, ch->chunkPos.z) * 16 + pos + normal + side1 + side2);
                bool s1 = ( b1!= AIR);
                bool s2 = ( b2!= AIR);
                bool c = ( b3!= AIR);
                if (s1 && s2)
                    return 0;

                return 3 - (s1 + s2 + c);
            };

        

        


        


        

        

        BlockTextureMapping& map = texturemappings.at(ch->BLOCKS[i]);
        

        


#pragma endregion
        //Front
        auto bl0 = GetNeighborBlock(pos + glm::ivec3{ 0,0,1 });
        if ( bl0== AIR|| transparent||IsTransparentBlock(bl0)) {
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
            
            float tileX = static_cast<int>(map.FRONT) % 32;
            float tileY = static_cast<int>(map.FRONT) / 32;
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
        auto bl1 = GetNeighborBlock(pos + glm::ivec3{ 0,0,-1 });
        if ( bl1== AIR|| transparent||IsTransparentBlock(bl1)) {
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
            float tileX = static_cast<int>(map.BACK) % 32;
            float tileY = static_cast<int>(map.BACK) / 32;
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
        auto bl2 = GetNeighborBlock(pos + glm::ivec3{ -1,0,0 });
        if (bl2 == AIR|| transparent||IsTransparentBlock(bl2)) {
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
            float tileX = static_cast<int>(map.LEFT) % 32;
            float tileY = static_cast<int>(map.LEFT) / 32;
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
        auto bl3 = GetNeighborBlock(pos + glm::ivec3{ 1,0,0 });
        if (bl3 == AIR|| transparent||IsTransparentBlock(bl3)) {
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

            float tileX = static_cast<int>(map.RIGHT) % 32;
            float tileY = static_cast<int>(map.RIGHT) / 32;
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
        auto bl4 = GetNeighborBlock(pos + glm::ivec3{ 0,1,0 });
        if (bl4 == AIR|| transparent||IsTransparentBlock(bl4)) {
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
            float tileX = static_cast<int>(map.TOP) % 32;
            float tileY = static_cast<int>(map.TOP) / 32;
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
        auto bl5 = GetNeighborBlock(pos + glm::ivec3{ 0,-1,0 });
        if (bl5 == AIR|| transparent||IsTransparentBlock(bl5)) {
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
            float tileX = static_cast<int>(map.BOTTOM) % 32;
            float tileY = static_cast<int>(map.BOTTOM) / 32;
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
            Blockvertices[vertexCount] = vertex(-0.5f, -0.5f, -0.5f, sX, sY, AO[5][0]);
            vertexCount++;
            Blockvertices[vertexCount] = vertex(0.5f, -0.5f, -0.5f, eX, sY, AO[5][1]);
            vertexCount++;
            Blockvertices[vertexCount] = vertex(0.5f, -0.5f, 0.5f, eX, eY, AO[5][2]);
            vertexCount++;
            Blockvertices[vertexCount] = vertex(-0.5f, -0.5f, 0.5f, sX, eY, AO[5][3]);
            vertexCount++;
        }


        uint offset = ch->vertices.size();
        for (int i = 0;i < vertexCount;i++) {
            vertex vert = Blockvertices[i];


            vert.x += pos.x;
            vert.y += pos.y;
            vert.z += pos.z;
            //vert.u += sX;
            //vert.v += sY;
            ch->vertices.push_back(vert);
        }

        for (uint i = 0; i < vertexCount; i += 4) {
            ch->indices.push_back(offset + i + 0);
            ch->indices.push_back(offset + i + 1);
            ch->indices.push_back(offset + i + 2);

            ch->indices.push_back(offset + i + 2);
            ch->indices.push_back(offset + i + 3);
            ch->indices.push_back(offset + i + 0);
        }
    }
    ch->indicessize = ch->indices.size();



    glBindVertexArray(0);
    if (ch->VAO != 0&& ch->VBO != 0&& ch->EBO != 0) {
        glBindVertexArray(ch->VAO);
        glBindBuffer(GL_ARRAY_BUFFER, ch->VBO);
        glBufferData(GL_ARRAY_BUFFER, ch->vertices.size() * sizeof(vertex), ch->vertices.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ch->EBO);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            ch->indices.size() * sizeof(uint),
            ch->indices.data(),
            GL_STATIC_DRAW
        );

        glBindVertexArray(0);
    }
    else {

        glGenVertexArrays(1, &ch->VAO);
        glGenBuffers(1, &ch->VBO);
        glGenBuffers(1, &ch->EBO);
        glBindVertexArray(ch->VAO);
        glBindBuffer(GL_ARRAY_BUFFER, ch->VBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ch->EBO);


        glBufferData(GL_ARRAY_BUFFER, ch->vertices.size() * sizeof(vertex), ch->vertices.data(), GL_DYNAMIC_DRAW);

        //pos
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (const void*)0);
        //uv
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (const void*)(3 * sizeof(float)));

        //AO
        glVertexAttribIPointer(2, 1, GL_UNSIGNED_BYTE, sizeof(vertex), (const void*)(5 * sizeof(float)));



        glBufferData(GL_ELEMENT_ARRAY_BUFFER, ch->indices.size() * sizeof(uint), ch->indices.data(), GL_STATIC_DRAW);


        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);


        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    ch->generating = false;
    ch->generated = true;
    ch->dirty = false;
    ch->vertices.clear();
    ch->vertices.shrink_to_fit();
    ch->indices.clear();
    ch->indices.shrink_to_fit();
}
void UploadChunk2(chunk* ch, glm::ivec3 pos) {
    ch->generated = false;
    ch->generating = true;
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
        if (glm::length(glm::vec3(ch->GetPosition(i)) - glm::vec3(pos)) > 5.0f)continue;
        if (ch->BLOCKS[i] == AIR)continue;
        std::array<vertex, 24> Blockvertices;
        int vertexCount = 0;

        glm::ivec3 pos = ch->GetPosition(i);




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




        //AMBIENT OCCLUSION
#pragma region AO



        uint8_t AO[6][4];

        auto CalculateAO = [&](glm::ivec3 normal, glm::ivec3 side1, glm::ivec3 side2, glm::ivec3 corner) -> uint8_t
            {
                bool s1 = (GlobalGetBlockAt(glm::ivec3(ch->chunkPos.x, 0, ch->chunkPos.z) * 16 + pos + normal + side1) != AIR);
                bool s2 = (GlobalGetBlockAt(glm::ivec3(ch->chunkPos.x, 0, ch->chunkPos.z) * 16 + pos + normal + side2) != AIR);
                bool c = (GlobalGetBlockAt(glm::ivec3(ch->chunkPos.x, 0, ch->chunkPos.z) * 16 + pos + normal + side1 + side2) != AIR);
                if (s1 && s2) return 0; return 3 - (s1 + s2 + c);
            };













        BlockTextureMapping& map = texturemappings.at(ch->BLOCKS[i]);





#pragma endregion
        //Front
        if (ch->GetBlockAt(pos + glm::ivec3{ 0,0,1 }) == AIR) {
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

            float tileX = static_cast<int>(map.FRONT) % 32;
            float tileY = static_cast<int>(map.FRONT) / 32;
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
            float tileX = static_cast<int>(map.BACK) % 32;
            float tileY = static_cast<int>(map.BACK) / 32;
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
            float tileX = static_cast<int>(map.LEFT) % 32;
            float tileY = static_cast<int>(map.LEFT) / 32;
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

            float tileX = static_cast<int>(map.RIGHT) % 32;
            float tileY = static_cast<int>(map.RIGHT) / 32;
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
            float tileX = static_cast<int>(map.TOP) % 32;
            float tileY = static_cast<int>(map.TOP) / 32;
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
            float tileX = static_cast<int>(map.BOTTOM) % 32;
            float tileY = static_cast<int>(map.BOTTOM) / 32;
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
            Blockvertices[vertexCount] = vertex(-0.5f, -0.5f, -0.5f, sX, sY, AO[5][0]);
            vertexCount++;
            Blockvertices[vertexCount] = vertex(0.5f, -0.5f, -0.5f, eX, sY, AO[5][1]);
            vertexCount++;
            Blockvertices[vertexCount] = vertex(0.5f, -0.5f, 0.5f, eX, eY, AO[5][2]);
            vertexCount++;
            Blockvertices[vertexCount] = vertex(-0.5f, -0.5f, 0.5f, sX, eY, AO[5][3]);
            vertexCount++;
        }


        uint offset = ch->vertices.size();
        for (int i = 0;i < vertexCount;i++) {
            vertex vert = Blockvertices[i];


            vert.x += pos.x;
            vert.y += pos.y;
            vert.z += pos.z;
            //vert.u += sX;
            //vert.v += sY;
            ch->vertices.push_back(vert);
        }

        for (uint i = 0; i < vertexCount; i += 4) {
            ch->indices.push_back(offset + i + 0);
            ch->indices.push_back(offset + i + 1);
            ch->indices.push_back(offset + i + 2);

            ch->indices.push_back(offset + i + 2);
            ch->indices.push_back(offset + i + 3);
            ch->indices.push_back(offset + i + 0);
        }
    }
    ch->indicessize = ch->indices.size();
    glBindVertexArray(0);
    if (ch->VAO != 0 && ch->VBO != 0 && ch->EBO != 0) {
        glBindVertexArray(ch->VAO);
        glBindBuffer(GL_ARRAY_BUFFER, ch->VBO);
        glBufferData(GL_ARRAY_BUFFER, ch->vertices.size() * sizeof(vertex), ch->vertices.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ch->EBO);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            ch->indices.size() * sizeof(uint),
            ch->indices.data(),
            GL_STATIC_DRAW
        );

        glBindVertexArray(0);
    }
    else {

        glGenVertexArrays(1, &ch->VAO);
        glGenBuffers(1, &ch->VBO);
        glGenBuffers(1, &ch->EBO);
        glBindVertexArray(ch->VAO);
        glBindBuffer(GL_ARRAY_BUFFER, ch->VBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ch->EBO);


        glBufferData(GL_ARRAY_BUFFER, ch->vertices.size() * sizeof(vertex), ch->vertices.data(), GL_DYNAMIC_DRAW);

        //pos
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (const void*)0);
        //uv
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (const void*)(3 * sizeof(float)));

        //AO
        glVertexAttribIPointer(2, 1, GL_UNSIGNED_BYTE, sizeof(vertex), (const void*)(5 * sizeof(float)));



        glBufferData(GL_ELEMENT_ARRAY_BUFFER, ch->indices.size() * sizeof(uint), ch->indices.data(), GL_STATIC_DRAW);


        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);


        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
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
int WIDTH=0, HEIGHT=0;
void resize(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
    WIDTH = width;
    HEIGHT = height;
}
void mouseMove(GLFWwindow* window, double xpos, double ypos) {
    static float lastX = xpos;
    static float lastY = ypos;
    static bool firstMouse = true;
    mx = xpos;
    my = ypos;

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
            glm::dvec3 ray =
                player.position +
                glm::dvec3(player.cam.position) +
                glm::dvec3(player.cam.front) * static_cast<double>(dist);

            glm::ivec3 blockPos = glm::ivec3(
                glm::floor(ray + glm::dvec3(0.5f))
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
            glm::dvec3 ray =
                player.position +
                glm::dvec3(player.cam.position) +
                glm::dvec3(player.cam.front) * static_cast<double>(dist);

            glm::ivec3 blockPos = glm::ivec3(
                glm::floor(ray + glm::dvec3(0.5f))
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
void drawCubeDisplay();
void refreshCubeDisplay();
void onScroll(GLFWwindow* window, double xoffset, double yoffset) {
    if (yoffset > 0) {
        currentblock += 1;
        if (currentblock > 27) {
            currentblock = 1;
            
        }
        refreshCubeDisplay();
    }
    else if (yoffset < 0) {
        currentblock -= 1;
        if (currentblock < 1) {
            currentblock = 27;
            
        }
        refreshCubeDisplay();
    }
    
}
bool fly = false;
float screenshottimer = 0.0f;
void ScreenshotPanorama();
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
    else if (key == GLFW_KEY_F3 && action == GLFW_PRESS) {
        ScreenshotPanorama();
    }
    else if (key == GLFW_KEY_F && action == GLFW_PRESS) {
        fly = !fly;
    }
    else if (key == GLFW_KEY_F4 && action == GLFW_PRESS) {
        yaw = std::round(yaw / 90.0f) * 90.0f;
        pitch = std::round(pitch / 90.0f) * 90.0f;

        pitch = glm::clamp(pitch, -89.0f, 89.0f);

        player.cam.front.x =
            cos(glm::radians(yaw)) *
            cos(glm::radians(pitch));

        player.cam.front.y =
            sin(glm::radians(pitch));

        player.cam.front.z =
            sin(glm::radians(yaw)) *
            cos(glm::radians(pitch));

        player.cam.front = glm::normalize(player.cam.front);

        

    }
}
enum worldtype {
    OVERWORLD=0, FLAT=1, NETHER=2, END=3
};
worldtype WORLD_TYPE = OVERWORLD;
void GenerateWorldChunk(ChunkPos cp);

void placeTree(ChunkPos cp, float x, float y, float z, float trunkHeight);
int SEED = 1308;
void GenerateStructures(ChunkPos cp) {
    chunk& ch = ChunkPool.at(cp);
    int GRID_SIZE = 400;
    switch (WORLD_TYPE) {
    case OVERWORLD:
        
        for (int x = 0;x < 16;x++) {
            for (int z = 0;z < 16;z++) {
                //drzewka
                int worldX = cp.x * 16 + x;
                int worldZ = cp.z * 16 + z;
                float trfrequency = 0.3f;


                float treef = 8.0;
                float treeamp = 15.0f;
                float treeval = PERLIN::perlin(worldX * treef / (GRID_SIZE / 2), worldZ * treef / (GRID_SIZE / 2)) * treeamp;

                //if (x % spacing == 0 && z % spacing == 0)
                //{
                float n = noise2D(worldX, worldZ, SEED) * 0.95f;

                //if (n < std::min(density- treeval*10.0f,0.3f))
                //{
                int color = sky_limit;
                for (int i = sky_limit;i > 0;i--) {
                    block bl = ch.GetBlockAt({ x,i,z });
                    if (bl == AIR||bl==OAK_LEAVES||bl==OAK_LOG) {
                        color = i;
                    }
                    else {
                        break;
                    }

                }
                if (color == 0) {
                    std::cout << "AAAA";
                }

                if (n > 0.9f && treeval > 0.0f)
                    placeTree(cp, x, color, z, RandomNumber(5,8));
                //ch.SetBlock({ x,color,z }, COBBLE);
            //}
        //}



            }
        }
        break;

    case END:

        break;

    case NETHER:

        break;



        break;
    }
    ch.structuresGenerated = true;
}



void LoadChunks(float spareTime) {
    float timepassed = 0.0f;
    float chunktime = 0.0f;
    int playerChunkX = static_cast<int>(std::floor(player.position.x / 16.0f));
    int playerChunkZ = static_cast<int>(std::floor(player.position.z / 16.0f));
    struct Chunktorender
    {
        ChunkPos pos;
        float distanceSq;
    };

    std::vector<Chunktorender> chunks;

    for (int x = playerChunkX - player.RenderDistance;
        x <= playerChunkX + player.RenderDistance;
        x++)
    {
        for (int z = playerChunkZ - player.RenderDistance;
            z <= playerChunkZ + player.RenderDistance;
            z++)
        {
            float dx = float(x - playerChunkX);
            float dz = float(z - playerChunkZ);

            float distanceSq = dx * dx + dz * dz;

            if (distanceSq <= player.RenderDistance * player.RenderDistance)
            {
                chunks.push_back({
                    ChunkPos(x, z),
                    distanceSq
                    });
            }
        }
    }
    std::sort(chunks.begin(), chunks.end(),
        [](const auto& a, const auto& b)
        {
            return a.distanceSq < b.distanceSq;
        });


    for (const auto& ch : chunks)
    {
        if (timepassed + chunktime * 2 >= spareTime)
            return;

        float tm = glfwGetTime();

        ChunkPos cp = ch.pos;

        auto [it, inserted] = ChunkPool.try_emplace(cp);
        it->second.chunkPos = cp;

        if (!it->second.generatedchunk)
        {
            float tmm = glfwGetTime();
            GenerateWorldChunk(cp);
            float dif = tmm - glfwGetTime();
            if (timepassed + dif  >= spareTime)
                return;
            it->second.Generate();
            it->second.loaded = true;
        }
        else
        {
            auto& ch = it->second;
            if (ch.dirty)
            {
                ch.Generate();
                ch.dirty = false;
            }
            if (ch.generatedchunk && !ch.generated) {
                ch.Generate();
                ch.dirty = false;
            }
            else if (!ch.AOupdated) {
                if (!ch.structuresGenerated) {
                    GenerateStructures(cp);
                }
                bool c1=false;
                bool c2 = false;
                bool c3 = false;
                bool c4 = false;
                bool c5 = false;
                bool c6 = false;
                bool c7 = false;
                bool c8 = false;

                if (ChunkPool.count(cp + ChunkPos{ -1, 0 })) {
                    c1 = ChunkPool.at(cp + ChunkPos{ -1, 0 }).generated;
                }
                if (ChunkPool.count(cp + ChunkPos{ 1, 0 })) {
                    c2 = ChunkPool.at(cp + ChunkPos{ 1, 0 }).generated;
                }
                if (ChunkPool.count(cp + ChunkPos{ 0, -1 })) {
                    c3 = ChunkPool.at(cp + ChunkPos{ 0, -1 }).generated;
                }
                if (ChunkPool.count(cp + ChunkPos{ -1, 0 })) {
                    c4 = ChunkPool.at(cp + ChunkPos{ -1, 0 }).generated;
                }

                if (ChunkPool.count(cp + ChunkPos{ -1, -1 })) {
                    c5 = ChunkPool.at(cp + ChunkPos{ -1, -1 }).generated;
                }
                if (ChunkPool.count(cp + ChunkPos{ 1, 1 })) {
                    c6 = ChunkPool.at(cp + ChunkPos{ 1, 1 }).generated;
                }
                if (ChunkPool.count(cp + ChunkPos{ 1, -1 })) {
                    c7 = ChunkPool.at(cp + ChunkPos{ 1, -1 }).generated;
                }
                if (ChunkPool.count(cp + ChunkPos{ -1, 1 })) {
                    c8 = ChunkPool.at(cp + ChunkPos{ -1, 1 }).generated;
                }
                if (c1&&c2&&c3&&c4&&c5&&c6&&c7&&c8) {//brb
                    ch.dirty = true;
                    ch.AOupdated = true;
                }
            }
            
        }

        float tm2 = glfwGetTime();
        chunktime = tm2 - tm;
        timepassed += chunktime;
    }
}
GLFWwindow* window;
uint HighLightVAO;
uint MenuVAO;
uint CubeVAO;
uint CUBEVBO;
void ScreenShot();


int main()
{
    //fly = true;
    //FARLANDS BORDER: 16777200.0
    //player.position.x = 16777216.0;
    //player.position.z = 16777216.0;
    SEED = RandomNumber(0, 99999999);
    
    PERLIN::generatePermutation(SEED);
    
#pragma region Init
    if (!glfwInit()) {
        return -1;
    }
    window = glfwCreateWindow(960, 540, "MeinKampf", NULL, NULL);//960 540

    glfwGetWindowSize(window, &WIDTH, &HEIGHT);
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


    //simple shader (menu cube)

    {
        uint VS = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(VS, 1, &SimpleVertexSource, NULL);
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
        glShaderSource(FS, 1, &SimpleFragmentSource, NULL);
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

        SimpleShaderProgram = glCreateProgram();
        glAttachShader(SimpleShaderProgram, VS);
        glAttachShader(SimpleShaderProgram, FS);
        glLinkProgram(SimpleShaderProgram);

        glDeleteShader(VS);
        glDeleteShader(FS);

    }
    //UI SHADER PROGRAM
    {
        uint VS = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(VS, 1, &UIVertexSource, NULL);
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
        glShaderSource(FS, 1, &UIFragmentSource, NULL);
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

        UIShaderProgram = glCreateProgram();
        glAttachShader(UIShaderProgram, VS);
        glAttachShader(UIShaderProgram, FS);
        glLinkProgram(UIShaderProgram);

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

   

    glViewport(0, 0, WIDTH, HEIGHT);
    glClearColor(66.0f / 255.0f, 135.0f / 255.0f, 245.0f / 255.0f, 1.0f);


#pragma region Load Texture Atlas
    {
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
    }

#pragma endregion

#pragma region Load UI Atlas
    {
        stbi_set_flip_vertically_on_load(false);
        int width, height, channels;
        const unsigned char* data = stbi_load("assets/ui.png", &width, &height, &channels, 4);


        glGenTextures(1, &UITEXTURE);
        glBindTexture(GL_TEXTURE_2D, UITEXTURE);

        glTexImage2D(GL_TEXTURE_2D, NULL, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
            GL_NEAREST);

        //glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, -1.5f);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenerateMipmap(GL_TEXTURE_2D);
    }


#pragma endregion

#pragma region Load Subtitles Atlas
    {
        stbi_set_flip_vertically_on_load(false);
        int width, height, channels;
        const unsigned char* data = stbi_load("assets/subtitles.png", &width, &height, &channels, 4);


        glGenTextures(1, &SUBTITLES_TEXTURE);
        glBindTexture(GL_TEXTURE_2D, SUBTITLES_TEXTURE);

        glTexImage2D(GL_TEXTURE_2D, NULL, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
            GL_NEAREST);

        //glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, -1.5f);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenerateMipmap(GL_TEXTURE_2D);
    }


#pragma endregion

#pragma region Load Menu Background
    stbi_set_flip_vertically_on_load(true);
    {
        int w, h, c;
        const unsigned char* ddata = stbi_load("assets/menubg.png", &w, &h, &c, 4);


        glGenTextures(1, &MENU_BG);
        glBindTexture(GL_TEXTURE_2D, MENU_BG);

        glTexImage2D(GL_TEXTURE_2D, NULL, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, ddata);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
            GL_LINEAR);

        //glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, -1.5f);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        //glGenerateMipmap(GL_TEXTURE_2D);
    }
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


#pragma region Create Menu Background Cube
    {
        float frac = 1.0f / 6.0f;
        std::vector<vertex> Blockvertices = {
            //Front
            vertex(-0.5f, -0.5f,  0.5f, 0.0f, 0.0f),
            vertex(0.5f,  0.5f,  0.5f, frac, 1.0f),
            vertex(-0.5f,  0.5f,  0.5f, 0.0f, 1.0f),

            vertex(-0.5f, -0.5f,  0.5f, 0.0f, 0.0f),
            vertex(0.5f, -0.5f,  0.5f, frac, 0.0f),
            vertex(0.5f,  0.5f,  0.5f, frac, 1.0f),

            //Back
            vertex(0.5f, -0.5f, -0.5f, frac*2, 0.0f),
            vertex(-0.5f,  0.5f, -0.5f, frac*3, 1.0f),
            vertex(0.5f,  0.5f, -0.5f, frac*2, 1.0f),

            vertex(0.5f, -0.5f, -0.5f, frac*2, 0.0f),
            vertex(-0.5f, -0.5f, -0.5f, frac*3, 0.0f),
            vertex(-0.5f,  0.5f, -0.5f, frac*3, 1.0f),

            //Left
            vertex(-0.5f, -0.5f, -0.5f, frac*3, 0.0f),
            vertex(-0.5f,  0.5f,  0.5f, frac*4, 1.0f),
            vertex(-0.5f,  0.5f, -0.5f, frac*3, 1.0f),

            vertex(-0.5f, -0.5f, -0.5f, frac*3, 0.0f),
            vertex(-0.5f, -0.5f,  0.5f, frac*4, 0.0f),
            vertex(-0.5f,  0.5f,  0.5f, frac*4, 1.0f),

            //Right
            vertex(0.5f, -0.5f,  0.5f, frac, 0.0f),
            vertex(0.5f,  0.5f, -0.5f, frac*2, 1.0f),
            vertex(0.5f,  0.5f,  0.5f, frac, 1.0f),

            vertex(0.5f, -0.5f,  0.5f, frac, 0.0f),
            vertex(0.5f, -0.5f, -0.5f, frac*2, 0.0f),
            vertex(0.5f,  0.5f, -0.5f, frac*2, 1.0f),

            //Top
            vertex(-0.5f,  0.5f,  0.5f, frac*4, 0.0f),
            vertex(0.5f,  0.5f, -0.5f, frac*5, 1.0f),
            vertex(-0.5f,  0.5f, -0.5f, frac*4, 1.0f),

            vertex(-0.5f,  0.5f,  0.5f, frac*4, 0.0f),
            vertex(0.5f,  0.5f,  0.5f, frac*5, 0.0f),
            vertex(0.5f,  0.5f, -0.5f, frac*5, 1.0f),

            //Bottom
            vertex(-0.5f, -0.5f, -0.5f, frac*5, 0.0f),
            vertex(0.5f, -0.5f,  0.5f, frac*5, 1.0f),
            vertex(-0.5f, -0.5f,  0.5f, frac*5, 1.0f),

            vertex(-0.5f, -0.5f, -0.5f, frac*5, 0.0f),
            vertex(0.5f, -0.5f, -0.5f, frac*6, 0.0f),
            vertex(0.5f, -0.5f,  0.5f, frac*6, 1.0f)
        };

        uint vbo;
        glGenVertexArrays(1, &MenuVAO);
        glGenBuffers(1, &vbo);

        glBindVertexArray(MenuVAO);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        glBufferData(GL_ARRAY_BUFFER, Blockvertices.size() * sizeof(vertex), Blockvertices.data(), GL_STATIC_DRAW);
        //pos
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (const void*)0);
        //uv
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (const void*)(3*sizeof(float)));

        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);




    }
    #pragma endregion


#pragma region Create Cube VAO

        //brb
    
    glGenVertexArrays(1, &CubeVAO);
    glGenBuffers(1, &CUBEVBO);

    glBindVertexArray(CubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, CUBEVBO);

    std::vector<vertex> Blockvertices = {
        // Front
        vertex(-0.5f, -0.5f,  0.5f, 0.0f, 0.0f, 0.0f),
        vertex(0.5f, -0.5f,  0.5f, 1.0f, 0.0f, 0.0f),
        vertex(0.5f,  0.5f,  0.5f, 1.0f, 1.0f, 0.0f),

        vertex(-0.5f, -0.5f,  0.5f, 0.0f, 0.0f, 0.0f),
        vertex(0.5f,  0.5f,  0.5f, 1.0f, 1.0f, 0.0f),
        vertex(-0.5f,  0.5f,  0.5f, 0.0f, 1.0f, 0.0f),

        // Back
        vertex(0.5f, -0.5f, -0.5f, 0.0f, 0.0f, 0.0f),
        vertex(-0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f),
        vertex(-0.5f,  0.5f, -0.5f, 1.0f, 1.0f, 0.0f),

        vertex(0.5f, -0.5f, -0.5f, 0.0f, 0.0f, 0.0f),
        vertex(-0.5f,  0.5f, -0.5f, 1.0f, 1.0f, 0.0f),
        vertex(0.5f,  0.5f, -0.5f, 0.0f, 1.0f, 0.0f),

        // Left
        vertex(-0.5f, -0.5f, -0.5f, 0.0f, 0.0f, 0.0f),
        vertex(-0.5f, -0.5f,  0.5f, 1.0f, 0.0f, 0.0f),
        vertex(-0.5f,  0.5f,  0.5f, 1.0f, 1.0f, 0.0f),

        vertex(-0.5f, -0.5f, -0.5f, 0.0f, 0.0f, 0.0f),
        vertex(-0.5f,  0.5f,  0.5f, 1.0f, 1.0f, 0.0f),
        vertex(-0.5f,  0.5f, -0.5f, 0.0f, 1.0f, 0.0f),

        // Right
        vertex(0.5f, -0.5f,  0.5f, 0.0f, 0.0f, 0.0f),
        vertex(0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f),
        vertex(0.5f,  0.5f, -0.5f, 1.0f, 1.0f, 0.0f),

        vertex(0.5f, -0.5f,  0.5f, 0.0f, 0.0f, 0.0f),
        vertex(0.5f,  0.5f, -0.5f, 1.0f, 1.0f, 0.0f),
        vertex(0.5f,  0.5f,  0.5f, 0.0f, 1.0f, 0.0f),

        // Top
        vertex(-0.5f,  0.5f,  0.5f, 0.0f, 0.0f, 0.0f),
        vertex(0.5f,  0.5f,  0.5f, 1.0f, 0.0f, 0.0f),
        vertex(0.5f,  0.5f, -0.5f, 1.0f, 1.0f, 0.0f),

        vertex(-0.5f,  0.5f,  0.5f, 0.0f, 0.0f, 0.0f),
        vertex(0.5f,  0.5f, -0.5f, 1.0f, 1.0f, 0.0f),
        vertex(-0.5f,  0.5f, -0.5f, 0.0f, 1.0f, 0.0f),

        // Bottom
        vertex(-0.5f, -0.5f, -0.5f, 0.0f, 0.0f, 0.0f),
        vertex(0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f),
        vertex(0.5f, -0.5f,  0.5f, 1.0f, 1.0f, 0.0f),

        vertex(-0.5f, -0.5f, -0.5f, 0.0f, 0.0f, 0.0f),
        vertex(0.5f, -0.5f,  0.5f, 1.0f, 1.0f, 0.0f),
        vertex(-0.5f, -0.5f,  0.5f, 0.0f, 1.0f, 0.0f)
    };




    glBufferData(GL_ARRAY_BUFFER, Blockvertices.size() * sizeof(vertex), Blockvertices.data(), GL_DYNAMIC_DRAW);
    //pos
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (const void*)0);
    //uv
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (const void*)(3 * sizeof(float)));

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

#pragma endregion


#pragma region Create UI VAO
    {
        
        std::vector<vertex> SquareVerts = {
            vertex(-0.5f, -0.5f,  0.0f, 0.0f, 0.0f),
            vertex(0.5f,  0.5f,  0.0f, 1.0f, 1.0f),
            vertex(-0.5f,  0.5f,  0.0f, 0.0f, 1.0f),

            vertex(-0.5f, -0.5f,  0.0f, 0.0f, 0.0f),
            vertex(0.5f, -0.5f,  0.0f, 1.0f, 0.0f),
            vertex(0.5f,  0.5f,  0.0f, 1.0f, 1.0f),

          
        };

        uint vbo;
        glGenVertexArrays(1, &UIVAO);
        glGenBuffers(1, &vbo);

        glBindVertexArray(UIVAO);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        glBufferData(GL_ARRAY_BUFFER, SquareVerts.size() * sizeof(vertex), SquareVerts.data(), GL_STATIC_DRAW);
        //pos
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (const void*)0);
        //uv
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (const void*)(3 * sizeof(float)));

        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
#pragma endregion

#pragma region Crosshair

    {
        float thickness = 0.04f;
        std::vector<vertex> SquareVerts = {
            vertex(-0.5f, -thickness,  0.0f, 0.0f, 0.0f),
            vertex(0.5f,  thickness,  0.0f, 1.0f, 1.0f),
            vertex(-0.5f,  thickness,  0.0f, 0.0f, 1.0f),

            vertex(-0.5f, -thickness,  0.0f, 0.0f, 0.0f),
            vertex(0.5f, -thickness,  0.0f, 1.0f, 0.0f),
            vertex(0.5f,  thickness,  0.0f, 1.0f, 1.0f),


            vertex(-thickness, -0.5f,  0.0f, 0.0f, 0.0f),
            vertex(thickness,  -thickness,  0.0f, 1.0f, 1.0f),
            vertex(-thickness,  -thickness,  0.0f, 0.0f, 1.0f),

            vertex(-thickness, -0.5f,  0.0f, 0.0f, 0.0f),
            vertex(thickness, -0.5f,  0.0f, 1.0f, 0.0f),
            vertex(thickness,  -thickness,  0.0f, 1.0f, 1.0f),

            vertex(-thickness, -0.5f+0.5f+thickness,  0.0f, 0.0f, 0.0f),
            vertex(thickness,  0.5f,  0.0f + thickness, 1.0f, 1.0f),
            vertex(-thickness,  0.5f,  0.0f + thickness, 0.0f, 1.0f),

            vertex(-thickness, -0.5f + 0.5f + thickness,  0.0f, 0.0f, 0.0f),
            vertex(thickness, -0.5f + 0.5f + thickness,  0.0f, 1.0f, 0.0f),
            vertex(thickness,  0.5f,  0.0f + thickness, 1.0f, 1.0f),


        };

        uint vbo;
        glGenVertexArrays(1, &crosshair);
        glGenBuffers(1, &vbo);

        glBindVertexArray(crosshair);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        glBufferData(GL_ARRAY_BUFFER, SquareVerts.size() * sizeof(vertex), SquareVerts.data(), GL_STATIC_DRAW);
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

    float menurotation = 0.0f;
    uint mvplocc = glGetUniformLocation(SimpleShaderProgram, "MVP");
    uint uimvploc = glGetUniformLocation(UIShaderProgram, "MVP");
    uint texturelocc = glGetUniformLocation(SimpleShaderProgram, "texture0");
    uint uitextureloc = glGetUniformLocation(UIShaderProgram, "texture0");
    uint uiusetextureloc = glGetUniformLocation(UIShaderProgram, "useTexture");
    uint uiuvloc = glGetUniformLocation(UIShaderProgram, "texcoord");


    //CREATE UI
    UI.reserve(100);
    UIElement* startButton = &UI.emplace_back("StartGame", glm::vec2{ 0.5,0.5 }, glm::vec2{ 0.0,0.0 }, glm::vec4{ 0.0 });
    startButton->SizeOffset = glm::vec2{ 200,20 }*3.0f;
    startButton->AnchorPoint = { 0.5f,0.5f };
    startButton->TextureLocation = { 0.0f,106.0f };
    startButton->TextureSize = { 200.0f,20.0f };
    startButton->useTexture = true;
    startButton->Tick = [startButton]() {
        if (GAME_STATE == MENU) {
            static bool clicked = false;
            auto localsize = glm::vec2(startButton->Size * glm::vec2(static_cast<float>(WIDTH), static_cast<float>(HEIGHT)) + startButton->SizeOffset) - startButton->AnchorPoint;

            auto localpos = glm::vec2(startButton->Location * glm::vec2(static_cast<float>(WIDTH), static_cast<float>(HEIGHT)) + startButton->LocationOffset) - startButton->AnchorPoint * localsize;

            if (mx >= localpos.x && mx <= localpos.x + localsize.x && my >= localpos.y && my <= localpos.y + localsize.y) {
                //hover
                if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1)) {
                    if (!clicked) {
                        clicked = true;
                        GAME_STATE = INGAME;
                        startButton->visible = false;

                    }
                }
                else {
                    clicked = false;
                }

                startButton->TextureLocation = { 0.0f,126.0f };
            }
            else {
                startButton->TextureLocation = { 0.0f,106.0f };

            }
        }
        else {
            startButton->visible = false;
        }
        };

    UIElement* Title = &UI.emplace_back("Title", glm::vec2{ 0.5,0.5 }, glm::vec2{ 0.0,0.0 }, glm::vec4{ 0.0 });
    Title->Size = glm::vec2{ 0.0f,0.0f };
    Title->SizeOffset = glm::vec2{ 512,140 }*1.5f;
    Title->AnchorPoint = { 0.5,0.5 };
    Title->Location = glm::vec2{ 0.5f,0.2f };
    //Title->LocationOffset = glm::vec2{ 0.0f,150.0f };
    Title->TextureLocation = { 0.0f,372.0f };
    Title->TextureSize = { 512.0f,140.0f};
    Title->useTexture = true;
    Title->Tick = [Title]() {
        if (GAME_STATE == MENU) {
            Title->Scale = glm::vec2{ static_cast<float>(WIDTH) / 960.0f };
            Title->rotation = sin(glfwGetTime()) * 2.0f;
        }
        else {
            Title->visible = false;
        }
        
        };


    UIElement SubTitle = UIElement("SubTitle", glm::vec2{ 0.5,0.5 }, glm::vec2{ 0.0,0.0 }, glm::vec4{ 0.0 });
    SubTitle.Size = glm::vec2{ 0.0f,0.0f };
    SubTitle.SizeOffset = glm::vec2{ 512,30.0f }*1.5f;
    SubTitle.AnchorPoint = { 0.5,0.5 };
    SubTitle.Location = glm::vec2{ 0.8f,0.2f };
    SubTitle.LocationOffset = glm::vec2{ 150.0f,-150.0f };
    SubTitle.rotation = -45.0f;
    //Title->LocationOffset = glm::vec2{ 0.0f,150.0f };
    float subtitle = static_cast<int>(RandomNumber(0.0f, 10.0f));

    SubTitle.TextureLocation = { 0.0f,(30.0f) * subtitle };
    
    SubTitle.TextureSize = { 512.0f,30.0f };
    SubTitle.useTexture = true;
    
    SubTitle.Tick = [&]() {
        if (GAME_STATE == MENU) {
            SubTitle.Scale = glm::vec2{ static_cast<float>(WIDTH) / 960.0f };
            SubTitle.LocationOffset = glm::vec2{ 150.0f,-150.0f }*SubTitle.Scale;
        }
       

        };



    refreshCubeDisplay();
    bool plrteleported = false;

    float fpstimer = 0.0f;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        //glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        float time = glfwGetTime();
        float deltaTime = time - lasttime;
        lasttime = time;
        float FPS = 1.0f / deltaTime;
        fpstimer += deltaTime;
        
        if (GAME_STATE == INGAME) {
            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1)) {
                mouseLocked = true;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }
            glEnable(GL_CULL_FACE);
            player.tick();
            movement(deltaTime);
            //if (deltaTime < 1.0f / 60.0f) {
                float spareTime = 1.0f / 60.0f-deltaTime;
                if (fpstimer >= 1.0f) {
                    fpstimer = 0.0f;
                    std::cout << FPS <<" " <<spareTime<< std::endl;
                    std::cout << "POSITION: " << "X: " << player.position.x << " Y: " << player.position.y << " Z: " << player.position.z<<std::endl;
                }
                LoadChunks(spareTime);
            //}
                
            auto playerchunk = ChunkPos(
                static_cast<int>(glm::floor(player.position.x / 16.0f)),
                static_cast<int>(glm::floor(player.position.z / 16.0f))
            );
            if (!ChunkPool.count(playerchunk) || !ChunkPool.at(playerchunk).generated) {
                player.position.y = 0;
            }
            else {
                if (!plrteleported) {
                    plrteleported = true;
                    //brb
                    glm::ivec3 local(
                        player.position.x - playerchunk.x * 16,
                        player.position.y,
                        player.position.z - playerchunk.z * 16
                    );
                    int lvl = 0;
                    
                        chunk& ch = ChunkPool.at(playerchunk);
                        for (int y = sky_limit;y > 0;y--) {
                            if (ch.GetBlockAt({ local.x, y, local.z }) == AIR) {
                                lvl=y;
                            }
                            else {
                                break;
                            }
                        }
                        player.position.y = lvl + 6.0f;
                    
                }
            }

            int width, height;
            glfwGetWindowSize(window, &width, &height);
            player.cam.FOV +=
                (player.cam.TargetFOV * player.cam.FOV_Multiplier - player.cam.FOV)
                * 5.0f * deltaTime;
            if (glfwGetKey(window, GLFW_KEY_F6)) {
                player.cam.FOV = 90.0f;
            }
            glm::mat4 projection = glm::perspective(glm::radians(player.cam.FOV), static_cast<float>(width) / static_cast<float>(height), 0.1f, 1000.0f);
            glm::mat4 view = glm::lookAt(
                glm::vec3(0.0f),
                player.cam.front,
                player.cam.up
            );
            glUseProgram(ShaderProgram);

            uint mvploc = glGetUniformLocation(ShaderProgram, "MVP");
            uint textureloc = glGetUniformLocation(ShaderProgram, "texture0");


            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, ATLAS);
            glUniform1i(textureloc, 0);

            //TESTCHUNK.Render();

            glm::vec3 plposdiv = player.position / 16.0;
            for (int x = plposdiv.x - player.RenderDistance;x < plposdiv.x + player.RenderDistance;x++) {
                for (int z = plposdiv.z - player.RenderDistance;z < plposdiv.z + player.RenderDistance;z++) {
                    //if (glm::length(glm::vec3(x, 0, z) - plposdiv) <= player.RenderDistance) {
                        //genchunk
                    glm::dvec3 camWorldPos = player.position + glm::dvec3(player.cam.position);
                        ChunkPos cp(static_cast<int>(x), static_cast<int>(z));
                        if (ChunkPool.count(cp)) {
                            glm::dvec3 chunkWorldOrigin = glm::dvec3(cp.x, 0.0, cp.z) * 16.0;
                            glm::vec3 relOffset = glm::vec3(chunkWorldOrigin - camWorldPos);
                            glm::mat4 model = glm::translate(glm::mat4(1.0f), relOffset);
                            
                            glm::mat4 MVP = projection * view * model;
                            glUniformMatrix4fv(mvploc, 1, GL_FALSE, glm::value_ptr(MVP));
                            ChunkPool.at(cp).Render();


                        }
                    //
//}

                }

            }

            //block highlights
            float dist = 0.0f;
            float step = 0.1f;
            mvploc2 = glGetUniformLocation(HighlightShaderProgram, "MVP");
            highlightcolorloc = glGetUniformLocation(HighlightShaderProgram, "color");
            glUseProgram(HighlightShaderProgram);
            ChunkPos Hchunk(0, 0);
            glm::ivec3 localpos(0, 0, 0);
            glm::ivec3 hitBlock;
            glm::ivec3 previousBlock(0, 0, 0);
            bool hitblock = false;
            while (dist <= player.reach)
            {
                glm::dvec3 ray =
                    player.position +
                    glm::dvec3(player.cam.position) +
                    glm::dvec3(player.cam.front) * static_cast<double>(dist);

                glm::ivec3 blockPos = glm::ivec3(
                    glm::floor(ray + glm::dvec3(0.5f))
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
            glm::dvec3 camWorldPos = player.position + glm::dvec3(player.cam.position);
            glm::dvec3 blockWorldPos(
                Hchunk.x * 16.0 + localpos.x,
                localpos.y,
                Hchunk.z * 16.0 + localpos.z
            );
            glm::vec3 relBlockPos = glm::vec3(blockWorldPos - camWorldPos);

            glm::mat4 model(1.0f);
            model = glm::translate(model, relBlockPos);
            model = glm::scale(model, glm::vec3(1.005f, 1.005f, 1.005f));
            glm::mat4 MVP = projection * view * model;
            if (hitblock) {
                glUniformMatrix4fv(mvploc2, 1, GL_FALSE, glm::value_ptr(MVP));
                glUniform4f(highlightcolorloc, 0.0f, 0.0f, 0.0f, 1.0f);
                glLineWidth(2.0f);
                glDrawArrays(GL_LINES, 0, 32);
            }

            {
                glUseProgram(HighlightShaderProgram);
                glDisable(GL_DEPTH_TEST);
                glDisable(GL_CULL_FACE);
                glEnable(GL_BLEND);

                glm::mat4 proj = glm::ortho(0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f);
                glBindVertexArray(crosshair);
                glm::mat4 model(1.0f);
                model = glm::translate(model, glm::vec3(0.5f * width, 0.5f * height, 0.0f));
                model = glm::scale(model, glm::vec3(30.0f, 30.0f, 1.0f));

                glUniform4f(highlightcolorloc, 1.0f, 1.0f, 1.0f, 0.3f);


                glUniformMatrix4fv(mvploc2, 1, GL_FALSE, glm::value_ptr(proj * model));
                //brb
                glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);
                glDrawArrays(GL_TRIANGLES, 0, 18);
            }//brb
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            //block display
            glEnable(GL_CULL_FACE);
            glEnable(GL_DEPTH_TEST);
            drawCubeDisplay();
            
            //draw crosshair
            




            //test
            //DrawAABB(player.box.min+player.box.position, player.box.max+player.box.position, projection * view);
            if (screenshottimer > 0.0f) {
                screenshottimer -= deltaTime;
            }
            if (DoScreenshot && screenshottimer <= 0.0f) {
                ScreenShot();
            }

            
        }
else if(GAME_STATE==MENU) {
    glBindVertexArray(MenuVAO);
    glDisable(GL_CULL_FACE);
    glm::mat4 projection = glm::perspective(glm::radians(60.0f), static_cast<float>(WIDTH) / static_cast<float>(HEIGHT), 0.1f, 1000.0f);
    glm::mat4 view = glm::lookAt(glm::vec3{0.0f}, glm::vec3{ 0.0f,0.0f,-1.0f }, glm::vec3{ 0.0f,1.0f,0.0f });
    glUseProgram(SimpleShaderProgram);

    


    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, MENU_BG);
    glUniform1i(texturelocc, 0);

    glm::mat4 model(1.0f);
    menurotation += deltaTime*3.0f;
    
    //brb
    model = glm::rotate(
        model,
        glm::radians(menurotation),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    

    model = glm::scale(
        model,
        glm::vec3(4.0f)
    );

    glm::mat4 MVP = projection * view * model;
    glUniformMatrix4fv(mvplocc, 1, GL_FALSE, glm::value_ptr(MVP));

    glDrawArrays(GL_TRIANGLES, 0, 36);



    
}
//draw GUI
glBindVertexArray(UIVAO);
glUseProgram(UIShaderProgram);
glDisable(GL_DEPTH_TEST);
glDisable(GL_CULL_FACE);
glEnable(GL_BLEND);
glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

glm::mat4 proj = glm::ortho(0.0f,static_cast<float>(WIDTH),static_cast<float>(HEIGHT),0.0f);
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_2D, UITEXTURE);
for (auto& gui : UI)
{
    if (!gui.visible)
        continue;
    gui.Tick();

    glm::vec2 size =
        gui.SizeOffset +
        gui.Size * glm::vec2(
            (float)WIDTH,
            (float)HEIGHT
        );

    glm::vec2 position =
        gui.LocationOffset +
        gui.Location * glm::vec2(
            (float)WIDTH,
            (float)HEIGHT
        );

    glm::vec2 anchor = gui.AnchorPoint - glm::vec2(0.5f);

    glm::mat4 model(1.0f);

    model = glm::translate(
        model,
        glm::vec3(position, 0.0f)
    );

    model = glm::rotate(
        model,
        glm::radians(gui.rotation),
        glm::vec3(0, 0, 1)
    );

    

    model = glm::scale(
        model,
        glm::vec3(size.x*gui.Scale.x, size.y * gui.Scale.y, 1.0f)
    );

    model = glm::translate(
        model,
        glm::vec3(-anchor.x, -anchor.y, 0.0f)
    );
    glUniform1i(uitextureloc, 0);
    if (gui.useTexture) {
        glUniform1i(uiusetextureloc, 1);
        glUniform1i(uitextureloc, 0);
        glUniform4fv(uiuvloc, 1, glm::value_ptr(glm::vec4(gui.TextureLocation.x/512.0f, gui.TextureLocation.y/512.0f, gui.TextureSize.x/512.0f, gui.TextureSize.y/512.0f)));
    }
    else {
        glUniform1i(uiusetextureloc, 0);

    }

    glm::mat4 MVP = proj * model;

    glUniformMatrix4fv(
        uimvploc,
        1,
        GL_FALSE,
        glm::value_ptr(MVP)
    );


    glDrawArrays(GL_TRIANGLES, 0, 6);
}
//render subtitles
if (GAME_STATE == MENU) {
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, SUBTITLES_TEXTURE);
        auto& gui = SubTitle;
        gui.Tick();

        glm::vec2 size =
            gui.SizeOffset +
            gui.Size * glm::vec2(
                (float)WIDTH,
                (float)HEIGHT
            );

        glm::vec2 position =
            gui.LocationOffset +
            gui.Location * glm::vec2(
                (float)WIDTH,
                (float)HEIGHT
            );

        glm::vec2 anchor = gui.AnchorPoint - glm::vec2(0.5f);

        glm::mat4 model(1.0f);

        model = glm::translate(
            model,
            glm::vec3(position, 0.0f)
        );

        model = glm::rotate(
            model,
            glm::radians(gui.rotation),
            glm::vec3(0, 0, 1)
        );



        model = glm::scale(
            model,
            glm::vec3(size.x * gui.Scale.x, size.y * gui.Scale.y, 1.0f)
        );

        model = glm::translate(
            model,
            glm::vec3(-anchor.x, -anchor.y, 0.0f)
        );
        glUniform1i(uitextureloc, 0);
        if (gui.useTexture) {
            glUniform1i(uiusetextureloc, 1);
            glUniform1i(uitextureloc, 0);
            glUniform4fv(uiuvloc, 1, glm::value_ptr(glm::vec4(gui.TextureLocation.x / 512.0f, gui.TextureLocation.y / 512.0f, gui.TextureSize.x / 512.0f, gui.TextureSize.y / 512.0f)));
        }
        else {
            glUniform1i(uiusetextureloc, 0);

        }

        glm::mat4 MVP = proj * model;

        glUniformMatrix4fv(
            uimvploc,
            1,
            GL_FALSE,
            glm::value_ptr(MVP)
        );


        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
}



glDisable(GL_BLEND);



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
    if (fly) {
        speed *= 10.0f;
    }
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
        if (!CollidesWithBlocks(testbox)||fly) {
            player.position.x += horMove.x;
        }
    }
    if (horMove.z != 0.0f) {
        AABB testbox = player.box;
        testbox.position.z += horMove.z;
        if (!CollidesWithBlocks(testbox)||fly) {
            player.position.z += horMove.z;
        }
    }
    if (!fly) {
        player.velocity.y -= player.GRAVITY * deltaTime;
    }
    if (fly) {
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        {
            player.position.y += speed*deltaTime;
            
        }
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        {
            player.position.y -= speed * deltaTime;

        }
    }
    else {
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS &&
            player.grounded)
        {
            player.velocity.y = player.JUMP_SPEED;
            player.grounded = false;
        }
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
uint PanoFBO = 0, PanoColorTex = 0, PanoDepthRBO = 0;
const int PANO_SIZE = 1024;

void EnsurePanoFBO() {
    if (PanoFBO) return;
    glGenFramebuffers(1, &PanoFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, PanoFBO);

    glGenTextures(1, &PanoColorTex);
    glBindTexture(GL_TEXTURE_2D, PanoColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, PANO_SIZE, PANO_SIZE, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, PanoColorTex, 0);

    glGenRenderbuffers(1, &PanoDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, PanoDepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, PANO_SIZE, PANO_SIZE);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, PanoDepthRBO);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
void ScreenshotPanorama() {
    EnsurePanoFBO();

    glm::vec3 up = (std::abs(player.cam.front.y) > 0.99f)
        ? glm::vec3(0, 0, 1)
        : glm::vec3(0, 1, 0);

    glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.05f, 1000.0f);
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f), player.cam.front, up);

    glBindFramebuffer(GL_FRAMEBUFFER, PanoFBO);
    glViewport(0, 0, PANO_SIZE, PANO_SIZE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    glUseProgram(ShaderProgram);
    uint mvploc = glGetUniformLocation(ShaderProgram, "MVP");
    uint textureloc = glGetUniformLocation(ShaderProgram, "texture0");
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ATLAS);
    glUniform1i(textureloc, 0);

    glm::dvec3 camWorldPos = player.position + glm::dvec3(player.cam.position);
    glm::dvec3 plposdiv = camWorldPos / 16.0;
    for (int x = plposdiv.x - player.RenderDistance; x < plposdiv.x + player.RenderDistance; x++) {
        for (int z = plposdiv.z - player.RenderDistance; z < plposdiv.z + player.RenderDistance; z++) {
            ChunkPos cp((int)x, (int)z);
            if (ChunkPool.count(cp)) {
                glm::dvec3 chunkWorldOrigin = glm::dvec3(cp.x, 0.0, cp.z) * 16.0;
                glm::vec3 relOffset = glm::vec3(chunkWorldOrigin - camWorldPos);
                glm::mat4 model = glm::translate(glm::mat4(1.0f), relOffset);
                glm::mat4 MVP = projection * view * model;
                glUniformMatrix4fv(mvploc, 1, GL_FALSE, glm::value_ptr(MVP));
                ChunkPool.at(cp).Render();
            }
        }
    }

    std::vector<unsigned char> pixels(PANO_SIZE * PANO_SIZE * 3);
    glReadPixels(0, 0, PANO_SIZE, PANO_SIZE, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    for (int y = 0; y < PANO_SIZE / 2; ++y) {
        unsigned char* row1 = pixels.data() + y * PANO_SIZE * 3;
        unsigned char* row2 = pixels.data() + (PANO_SIZE - 1 - y) * PANO_SIZE * 3;
        for (int x = 0; x < PANO_SIZE * 3; ++x)
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
    filename << "PANORAMA-" << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S") << ".png";
    stbi_write_png(filename.str().c_str(), PANO_SIZE, PANO_SIZE, 3, pixels.data(), PANO_SIZE * 3);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, WIDTH, HEIGHT);
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
void drawCubeDisplay() {
    glDisable(GL_CULL_FACE);
    glBindVertexArray(CubeVAO);
    glm::mat4 model(1.0f);
    model = glm::translate(model, glm::vec3(70.0f, 70.0f, 0.0f));
    /*
    player.cam.front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    player.cam.front.y = sin(glm::radians(pitch));
    player.cam.front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    
    
    */
    model = glm::rotate(model, cos(glm::radians(yaw)) * cos(glm::radians(pitch))*0.1f, glm::vec3(1, 0, 0));
    model = glm::rotate(model, sin(glm::radians(pitch)), glm::vec3(0, 1, 0));
    model = glm::rotate(model, sin(glm::radians(yaw)) * cos(glm::radians(pitch))*0.1f, glm::vec3(0, 0, 1));
    model = glm::rotate(model, glm::radians(180.0f), glm::vec3(0, 0, 1));
    model = glm::scale(model, glm::vec3(70.0f, 70.0f, 70.0f));
    //glm::mat4 view=glm::lookAt(glm::vec3(0.0f,0.0f,10.0f), glm::vec3(0.0f, 0.0f, 10.0f)+glm::vec3(0.0f,0.0f,-1.0f),glm::vec3(0.0f,1.0f,0.0f));
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(WIDTH), static_cast<float>(HEIGHT), 0.0f,-1000.0f,1000.0f);
        //glm::perspective(glm::radians(30.0f), static_cast<float>(WIDTH)/static_cast<float>(HEIGHT),0.1f,1000.0f);
    uint mvplocc = glGetUniformLocation(SimpleShaderProgram, "MVP");
    uint texturelocc = glGetUniformLocation(SimpleShaderProgram, "texture0");
    
    glUseProgram(SimpleShaderProgram);


    //brb

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ATLAS);
    glUniform1i(texturelocc, 0);

    glUniformMatrix4fv(mvplocc, 1, GL_FALSE, glm::value_ptr(projection * model));

    glDrawArrays(GL_TRIANGLES, 0, 36);
    glEnable(GL_CULL_FACE);
}
void refreshCubeDisplay() {
    BlockTextureMapping& map = texturemappings.at(static_cast<block>(currentblock));
    
    int vertexCount = 0;
    std::vector<vertex> Blockvertices;
    // Front
    {
        float tileX = static_cast<int>(map.FRONT) % 32;
        float tileY = static_cast<int>(map.FRONT) / 32;
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
        Blockvertices.emplace_back(-0.5f, -0.5f, 0.5f, sX, sY, 0);
        Blockvertices.emplace_back(0.5f, -0.5f, 0.5f, eX, sY, 0);
        Blockvertices.emplace_back(0.5f, 0.5f, 0.5f, eX, eY, 0);

        Blockvertices.emplace_back(-0.5f, -0.5f, 0.5f, sX, sY, 0);
        Blockvertices.emplace_back(0.5f, 0.5f, 0.5f, eX, eY, 0);
        Blockvertices.emplace_back(-0.5f, 0.5f, 0.5f, sX, eY, 0);
    }
    // Back
    {
        float tileX = static_cast<int>(map.BACK) % 32;
        float tileY = static_cast<int>(map.BACK) / 32;
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
        Blockvertices.emplace_back(0.5f, -0.5f, -0.5f, sX, sY, 0);
        Blockvertices.emplace_back(-0.5f, -0.5f, -0.5f, eX, sY, 0);
        Blockvertices.emplace_back(-0.5f, 0.5f, -0.5f, eX, eY, 0);

        Blockvertices.emplace_back(0.5f, -0.5f, -0.5f, sX, sY, 0);
        Blockvertices.emplace_back(-0.5f, 0.5f, -0.5f, eX, eY, 0);
        Blockvertices.emplace_back(0.5f, 0.5f, -0.5f, sX, eY, 0);
    }
    // Left
    {
        float tileX = static_cast<int>(map.LEFT) % 32;
        float tileY = static_cast<int>(map.LEFT) / 32;
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
        Blockvertices.emplace_back(-0.5f, -0.5f, -0.5f, sX, sY, 0);
        Blockvertices.emplace_back(-0.5f, -0.5f, 0.5f, eX, sY, 0);
        Blockvertices.emplace_back(-0.5f, 0.5f, 0.5f, eX, eY, 0);

        Blockvertices.emplace_back(-0.5f, -0.5f, -0.5f, sX, sY, 0);
        Blockvertices.emplace_back(-0.5f, 0.5f, 0.5f, eX, eY, 0);
        Blockvertices.emplace_back(-0.5f, 0.5f, -0.5f, sX, eY, 0);
    }
    // Right
    {
        float tileX = static_cast<int>(map.RIGHT) % 32;
        float tileY = static_cast<int>(map.RIGHT) / 32;
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
        Blockvertices.emplace_back(0.5f, -0.5f, 0.5f, sX, sY, 0);
        Blockvertices.emplace_back(0.5f, -0.5f, -0.5f, eX, sY, 0);
        Blockvertices.emplace_back(0.5f, 0.5f, -0.5f, eX, eY, 0);

        Blockvertices.emplace_back(0.5f, -0.5f, 0.5f, sX, sY, 0);
        Blockvertices.emplace_back(0.5f, 0.5f, -0.5f, eX, eY, 0);
        Blockvertices.emplace_back(0.5f, 0.5f, 0.5f, sX, eY, 0);
    }
    // Top
    {
        float tileX = static_cast<int>(map.TOP) % 32;
        float tileY = static_cast<int>(map.TOP) / 32;
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
        Blockvertices.emplace_back(-0.5f, 0.5f, 0.5f, sX, sY,0);
        Blockvertices.emplace_back(0.5f, 0.5f, 0.5f, eX, sY, 0);
        Blockvertices.emplace_back(0.5f, 0.5f, -0.5f, eX, eY, 0);

        Blockvertices.emplace_back(-0.5f, 0.5f, 0.5f, sX, sY, 0);
        Blockvertices.emplace_back(0.5f, 0.5f, -0.5f, eX, eY,0);
        Blockvertices.emplace_back(-0.5f, 0.5f, -0.5f, sX, eY, 0);
    }
    // Bottom
    {
        float tileX = static_cast<int>(map.BOTTOM) % 32;
        float tileY = static_cast<int>(map.BOTTOM) / 32;
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
        Blockvertices.emplace_back(-0.5f, -0.5f, -0.5f, sX, sY, 0);
        Blockvertices.emplace_back(0.5f, -0.5f, -0.5f, eX, sY, 0);
        Blockvertices.emplace_back(0.5f, -0.5f, 0.5f, eX, eY, 0);

        Blockvertices.emplace_back(-0.5f, -0.5f, -0.5f, sX, sY,0);
        Blockvertices.emplace_back(0.5f, -0.5f, 0.5f, eX, eY, 0);
        Blockvertices.emplace_back(-0.5f, -0.5f, 0.5f, sX, eY, 0);
    }

    glBindBuffer(GL_ARRAY_BUFFER, CUBEVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, Blockvertices.size() * sizeof(vertex), Blockvertices.data());

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

const int GRID_SIZE = 400;

void placeTree(ChunkPos cp, float x, float y, float z, float trunkHeight)
{
    glm::vec3 base = glm::vec3(cp.x * 16.0, 0.0, cp.z * 16.0)
        + glm::vec3(x, y, z);
    for (int i = 0; i < trunkHeight; i++)
        GlobalSetBlockAtNoDirty(base + glm::vec3(0, i, 0), OAK_LOG);
    GlobalSetBlockAtNoDirty(base + glm::vec3(0, trunkHeight, 0), OAK_LEAVES);
    GlobalSetBlockAtNoDirty(base + glm::vec3(1, trunkHeight, 0), OAK_LEAVES);
    GlobalSetBlockAtNoDirty(base + glm::vec3(-1, trunkHeight, 0), OAK_LEAVES);
    GlobalSetBlockAtNoDirty(base + glm::vec3(0, trunkHeight, 1), OAK_LEAVES);
    GlobalSetBlockAtNoDirty(base + glm::vec3(0, trunkHeight, -1), OAK_LEAVES);

    GlobalSetBlockAtNoDirty(base + glm::vec3(1, trunkHeight - 1, 0), OAK_LEAVES);
    GlobalSetBlockAtNoDirty(base + glm::vec3(-1, trunkHeight - 1, 0), OAK_LEAVES);
    GlobalSetBlockAtNoDirty(base + glm::vec3(0, trunkHeight - 1, 1), OAK_LEAVES);
    GlobalSetBlockAtNoDirty(base + glm::vec3(0, trunkHeight - 1, -1), OAK_LEAVES);

    GlobalSetBlockAtNoDirty(base + glm::vec3(1, trunkHeight - 1, 1), OAK_LEAVES);
    GlobalSetBlockAtNoDirty(base + glm::vec3(1, trunkHeight - 1, -1), OAK_LEAVES);
    GlobalSetBlockAtNoDirty(base + glm::vec3(-1, trunkHeight - 1, 1), OAK_LEAVES);
    GlobalSetBlockAtNoDirty(base + glm::vec3(-1, trunkHeight - 1, -1), OAK_LEAVES);

    for (int yOffset = -3; yOffset <= -2; yOffset++)
        for (int xOffset = -1; xOffset <= 1; xOffset++)
            for (int zOffset = -2; zOffset <= 2; zOffset++)
                GlobalSetBlockAtNoDirty(
                    base + glm::vec3(xOffset, trunkHeight + yOffset, zOffset),
                    OAK_LEAVES
                );

    for (int yOffset = -3; yOffset <= -2; yOffset++)
        for (int xOffset = -2; xOffset <= 2; xOffset++)
            for (int zOffset = -1; zOffset <= 1; zOffset++)
                GlobalSetBlockAtNoDirty(
                    base + glm::vec3(xOffset, trunkHeight + yOffset, zOffset),
                    OAK_LEAVES
                );

    
}
void GenerateWorldChunk(ChunkPos cp) {
    chunk& ch = ChunkPool.at(cp);
    switch (WORLD_TYPE) {
    case FLAT: {
        ch.FillBlocks({ 0,2,0 }, { 15,2,15 }, GRASS);
        ch.FillBlocks({ 0,1,0 }, { 15,1,15 }, DIRT);
        ch.FillBlocks({ 0,0,0 }, { 15,0,15 }, BEDROCK);
        break;
    }
    case OVERWORLD:
    {

        for (int x = 0;x < 16;x++) {
            for (int z = 0;z < 16;z++) {
                float octaves = 5.0f;
                float amplitude = 1.0f;
                float frequency = 0.9f;
                float val = 0.0f;
                int colorstone = 10;

                int worldX = cp.x * 16 + x;
                int worldZ = cp.z * 16 + z;




                float base = PERLIN::fbm(
                    worldX / 300.0f,
                    worldZ / 300.0f,
                    4
                );

                float mountain = PERLIN::fbm(
                    worldX / 120.0f,
                    worldZ / 120.0f,
                    5
                );

                float detail = PERLIN::fbm(
                    worldX / 35.0f,
                    worldZ / 35.0f,
                    3
                );
                float mountainMask = std::max(0.0f, base + 0.2f);

                float heightNoise =
                    base * 20.0f +
                    mountain * mountainMask * 70.0f +
                    detail * 8.0f;

                int height = 128 + (int)heightNoise;
                height = std::clamp(height, 1, 255);
                frequency *= 2;
                frequency *= 2;
                frequency *= 2;
                frequency *= 2;
                frequency *= 2;
                frequency *= 2;
                amplitude /= 2.0f;
                amplitude /= 2.0f;
                amplitude /= 2.0f;
                amplitude /= 2.0f;
                amplitude /= 2.0f;
                amplitude /= 2.0f;
                val = PERLIN::perlin(worldX * frequency / GRID_SIZE, worldZ * frequency / GRID_SIZE) * amplitude;
                colorstone = (int)(((val + 1.0f) * 0.5f) * 255);
                
                

                if (val > 1.0f)
                    val = 1.0f;
                else if (val < -1.0f)
                    val = -1.0f;
                int color = height;
               
                color = std::max(3.0f, color*1.0f);
                //farlands heheheehe
                bool farlands = false;
                if (abs(worldX) > 16777200.0 || abs(worldZ) > 16777200) {
                    farlands = true;
                    colorstone += 100;
                    color += 100;
                }
                for (int y = 0;y < color;y++) {
                    if (y == color - 1) {
                        ch.SetBlock({ x,y,z }, GRASS);
                    }
                    else {
                        ch.SetBlock({ x,y,z }, DIRT);
                    }

                }


                colorstone = std::max(3.0f, colorstone-20.0f);
                for (int y = 0;y < colorstone;y++) {
                    
                    ch.SetBlock({ x,y,z }, STONE);

                }
                //rudy
                for (int y = 1;y < 100;y++) {
                    float frequency = 30.0f;
                    float amplitude = 2.0f;
                    float ore = PERLIN::perlin3d(worldX * frequency / (GRID_SIZE/4), y * frequency / (GRID_SIZE/4), worldZ * frequency / (GRID_SIZE/4)) * amplitude;
                    //std::cout << cave << std::endl;
                    if (y < 24) {
                        float diamfreq = 1.1f;
                        float diamondNoise = PERLIN::perlin3d(
                            worldX* diamfreq,
                            y * diamfreq,
                            worldZ * diamfreq
                        );
                        //diamondNoise += 1;
                        //diamondNoise *= 0.5f;
                        //diamondNoise *= 0.1f;
                        //std::cout << diamondNoise;
                        //std::cout << diamondNoise << std::endl;
                        if (diamondNoise > 0.71f) {
                            ch.SetBlock({ x, y, z }, DIAMOND_ORE);
                            //std::cout << "DIAMONDS";
                        }
                    }
                    else {
                       
                        if (ore > 0.95f) {
                            ch.SetBlock({ x,y,z }, COAL_ORE);
                        }
                        if (ore > 0.97f) {
                            ch.SetBlock({ x,y,z }, IRON_ORE);
                        }
                        if (ore > 1.2f) {
                            ch.SetBlock({ x,y,z }, GOLD_ORE);
                        }
                        
                        
                    }
                }


                //jaskinie
                //farlands heheheehe
                int cavelimit = 100;
                if (farlands) {
                    cavelimit += 200;
                }
                for (int y = 1;y < cavelimit;y++) {
                    float frequency = 10.0f;
                    float amplitude = 2.0f;
                    if (farlands) {
                        amplitude = 30.0f;
                    }

                    float ore = PERLIN::perlin3d(worldX * frequency / (GRID_SIZE / 4), y * frequency / (GRID_SIZE / 4), worldZ * frequency / (GRID_SIZE / 4)) * amplitude;
                    //std::cout << cave << std::endl;
                    if (ore > 0.8f) {
                        ch.SetBlock({ x,y,z }, AIR);
                    } 
                }
                //duze
                for (int y = 1;y < cavelimit/2;y++) {
                    float frequency = 2.0f;
                    float amplitude = 3.0f;
                    
                    float ore = PERLIN::perlin3d(worldX * frequency / (GRID_SIZE / 3), y * frequency / (GRID_SIZE / 3), worldZ * frequency / (GRID_SIZE / 3)) * amplitude;
                    if (farlands) {
                        ore = 1.0f - ore;
                    }
                    //std::cout << cave << std::endl;
                    float heightFactor = glm::smoothstep(20.0f, 50.0f, (float)y);
                    float threshold = glm::mix(1.2f, 2.5f, heightFactor);
                    
                    if (ore > threshold) {
                        
                        ch.SetBlock({ x,y,z }, AIR);
                    }
                }
                ch.FillBlocks({ 0,0,0 }, { 15,0,15 }, BEDROCK);
                ////drzewka
                
                float bedrock = noise2D(worldX, worldZ, 1308);
                if (bedrock > 0.9f) {
                    ch.SetBlock({ x,1,z }, BEDROCK);
                }



               
                
            }
        }
        break;
    }
    default:break;
    }
    ch.generatedchunk = true;
   
}