#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "stb_image.h"

#include <vector>
#include <array>

#include <unordered_map>
#include <deque>
#include <thread>
#include <chrono>
#include <mutex>
using uint = unsigned int;
const char* VertexSource = R"(
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
const char* FragmentSource = R"(
#version 330 core
out vec4 FragColor;
in vec2 TextCoord;
uniform sampler2D texture0;
void main(){
FragColor=texture(texture0, TextCoord);
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
uint ShaderProgram;
uint HighlightShaderProgram;
uint ATLAS;
bool mouseLocked = false;
struct camera {
    glm::vec3 position{ 0.0f }, front{ 0.0f,0.0f,-1.0f }, up{ 0.0f,1.0f,0.0f };
    camera() {

    }
    camera(glm::vec3 position, glm::vec3 front, glm::vec3 up) : position(position), front(front), up(up) {

    }
};

struct vertex {
    float x, y, z, u, v;
    vertex(float x, float y, float z, float u, float v) : x(x), y(y), z(z), u(u), v(v) {

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

class Player {
public:
    camera cam;
    glm::vec3 position{ 0.0f,0.0f,10.0f };
    float SPEED = 10.0f;
    int RenderDistance = 10;
    int reach = 15.0f;
    Player() {

    }
};
Player player;
class chunk;
std::unordered_map<ChunkPos, chunk, ChunkHash> ChunkPool;




class chunk {
private:
    uint VAO = 0;
    uint VBO, EBO;

    int indicessize = 0;
public:
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
        generated = false;
        generating = true;
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

        for (int i = 0;i < sizeof(BLOCKS) / sizeof(block);i++) {
            if (BLOCKS[i] == AIR)continue;
            std::array<vertex, 24> Blockvertices;
            int vertexCount = 0;

            glm::ivec3 pos = GetPosition(i);

            //Front
            if (GetBlockAt(pos + glm::ivec3{ 0,0,1 }) == AIR) {
                Blockvertices[vertexCount]=vertex(-0.5f, -0.5f, 0.5f, 0.0f, 0.0f);
                vertexCount++;
                Blockvertices[vertexCount] = vertex(0.5f, -0.5f, 0.5f, 1.0f / 32.0f, 0.0f);
                vertexCount++;
                Blockvertices[vertexCount] = vertex(0.5f, 0.5f, 0.5f, 1.0f / 32.0f, 1.0f / 32.0f);
                vertexCount++;
                Blockvertices[vertexCount] = vertex(-0.5f, 0.5f, 0.5f, 0.0f, 1.0f / 32.0f);
                vertexCount++;
            }
            //Back
            if (GetBlockAt(pos + glm::ivec3{ 0,0,-1 }) == AIR) {
                Blockvertices[vertexCount] = vertex(0.5f, -0.5f, -0.5f, 0.0f, 0.0f);
                vertexCount++;
                Blockvertices[vertexCount] = vertex(-0.5f, -0.5f, -0.5f, 1.0f / 32.0f, 0.0f);
                vertexCount++;
                Blockvertices[vertexCount] = vertex(-0.5f, 0.5f, -0.5f, 1.0f / 32.0f, 1.0f / 32.0f);
                vertexCount++;
                Blockvertices[vertexCount] = vertex(0.5f, 0.5f, -0.5f, 0.0f, 1.0f / 32.0f);
                vertexCount++;
            }
            //Left
            if (GetBlockAt(pos + glm::ivec3{ -1,0,0 }) == AIR) {
                Blockvertices[vertexCount] = vertex(-0.5f, -0.5f, -0.5f, 0.0f, 0.0f);
                vertexCount++;
                Blockvertices[vertexCount] = vertex(-0.5f, -0.5f, 0.5f, 1.0f / 32.0f, 0.0f);
                vertexCount++;
                Blockvertices[vertexCount] = vertex(-0.5f, 0.5f, 0.5f, 1.0f / 32.0f, 1.0f / 32.0f);
                vertexCount++;
                Blockvertices[vertexCount] = vertex(-0.5f, 0.5f, -0.5f, 0.0f, 1.0f / 32.0f);
                vertexCount++;
            }
            //Right
            if (GetBlockAt(pos + glm::ivec3{ 1,0,0 }) == AIR) {
                Blockvertices[vertexCount] = vertex(0.5f, -0.5f, 0.5f, 0.0f, 0.0f);
                vertexCount++;
                Blockvertices[vertexCount] = vertex(0.5f, -0.5f, -0.5f, 1.0f / 32.0f, 0.0f);
                vertexCount++;
                Blockvertices[vertexCount] = vertex(0.5f, 0.5f, -0.5f, 1.0f / 32.0f, 1.0f / 32.0f);
                vertexCount++;
                Blockvertices[vertexCount] = vertex(0.5f, 0.5f, 0.5f, 0.0f, 1.0f / 32.0f);
                vertexCount++;
            }
            //Top
            if (GetBlockAt(pos + glm::ivec3{ 0,1,0 }) == AIR) {
                Blockvertices[vertexCount] = vertex(-0.5f, 0.5f, 0.5f, 0.0f, 0.0f),
                    vertexCount++;
                Blockvertices[vertexCount] = vertex(0.5f, 0.5f, 0.5f, 1.0f / 32.0f, 0.0f);
                vertexCount++;
                Blockvertices[vertexCount] = vertex(0.5f, 0.5f, -0.5f, 1.0f / 32.0f, 1.0f / 32.0f);
                vertexCount++;
                Blockvertices[vertexCount] = vertex(-0.5f, 0.5f, -0.5f, 0.0f, 1.0f / 32.0f);
                vertexCount++;
            }
            //Bottom
            if (GetBlockAt(pos + glm::ivec3{ 0,-1,0 }) == AIR) {
                Blockvertices[vertexCount] = vertex(-0.5f, -0.5f, -0.5f, 0.0f, 0.0f);
                vertexCount++;
                Blockvertices[vertexCount] = vertex(0.5f, -0.5f, -0.5f, 1.0f / 32.0f, 0.0f);
                vertexCount++;
                Blockvertices[vertexCount] = vertex(0.5f, -0.5f, 0.5f, 1.0f / 32.0f, 1.0f / 32.0f);
                vertexCount++;
                Blockvertices[vertexCount] = vertex(-0.5f, -0.5f, 0.5f, 0.0f, 1.0f / 32.0f);
                vertexCount++;
            }

            float tileX = static_cast<int>(BLOCKS[i]) % 32;
            float tileY = static_cast<int>(BLOCKS[i]) / 32;

            float sX = tileX / 32.0f;
            float sY = 1.0f - (tileY + 1) / 32.0f;
            uint offset = vertices.size();
            for (auto vert : Blockvertices) {



                vert.x += pos.x;
                vert.y += pos.y;
                vert.z += pos.z;
                vert.u += sX;
                vert.v += sY;
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
        indicessize = indices.size();



        glBindVertexArray(0);
        if (VAO != 0) {
            glDeleteVertexArrays(1, &VAO);
            if(VBO!=0)
                glDeleteBuffers(1, &VBO);
            if(EBO!=0)
                glDeleteBuffers(1, &EBO);
            VAO = 0;
            VBO = 0;
            EBO = 0;
        }
        
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);


        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vertex), vertices.data(), GL_STATIC_DRAW);

        //pos
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (const void*)0);
        //uv
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (const void*)(3 * sizeof(float)));


        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint), indices.data(), GL_STATIC_DRAW);


        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);


        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

         generating = false;
         generated = true;
        dirty = false;
    }




    void Render() {
        glBindVertexArray(VAO);
        glUseProgram(ShaderProgram);

        glDrawElements(GL_TRIANGLES, indicessize, GL_UNSIGNED_INT, NULL);
    }
    void PlaceBlock(glm::ivec3 position, block BlockType) {
        BLOCKS[GetID(position)] = BlockType;
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
                    PlaceBlock({ x,y,z }, BlockType);
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

    it->second.PlaceBlock(local, BlockType);
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
void resize(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
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
        if (previousset&& hitblockset) {
            GlobalSetBlockAt(previousBlock, DIRT);
        }
    }
}

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
                        it->second.FillBlocks({ 0,0,0 }, { 15,0,15 }, BEDROCK);

                        it->second.FillBlocks({ 0,0,0 }, { 15,0,0 }, COBBLE);
                        it->second.FillBlocks({ 0,0,0 }, { 0,0,15 }, COBBLE);
                        it->second.FillBlocks({ 15,0,0 }, { 15,0,15 }, COBBLE);
                        it->second.FillBlocks({ 0,0,15 }, { 15,0,15 }, COBBLE);
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


        uint FS = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(FS, 1, &FragmentSource, NULL);
        glCompileShader(FS);

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
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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
        GL_NEAREST_MIPMAP_LINEAR);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, -1.5f);

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
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1)) {
            mouseLocked = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        float time = glfwGetTime();
        float deltaTime = time - lasttime;
        lasttime = time;
        movement(deltaTime);
        LoadChunks();
        int width, height;
        glfwGetWindowSize(window, &width, &height);

        glm::mat4 projection = glm::perspective(glm::radians(60.0f), static_cast<float>(width) / static_cast<float>(height), 0.1f, 1000.0f);
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
        uint mvploc2 = glGetUniformLocation(HighlightShaderProgram, "MVP");
        glUseProgram(HighlightShaderProgram);
        ChunkPos Hchunk(0,0);
        glm::ivec3 localpos(0,0,0);
        glm::ivec3 hitBlock;
        glm::ivec3 previousBlock(0,0,0);
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

        glUniformMatrix4fv(mvploc2, 1, GL_FALSE, glm::value_ptr(MVP));
        glLineWidth(2.0f);
        glDrawArrays(GL_LINES, 0, 32);

        //test
        

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}

void movement(float deltaTime) {
    if (glfwGetKey(window, GLFW_KEY_W)) {
        player.position += glm::normalize(player.cam.front) * deltaTime * player.SPEED;
    }
    if (glfwGetKey(window, GLFW_KEY_S)) {
        player.position -= glm::normalize(player.cam.front) * deltaTime * player.SPEED;
    }
    if (glfwGetKey(window, GLFW_KEY_A)) {
        glm::vec3 camRight = glm::normalize(glm::cross(player.cam.front, player.cam.up));
        player.position -= glm::normalize(camRight) * deltaTime * player.SPEED;
    }
    if (glfwGetKey(window, GLFW_KEY_D)) {
        glm::vec3 camRight = glm::normalize(glm::cross(player.cam.front, player.cam.up));
        player.position += glm::normalize(camRight) * deltaTime * player.SPEED;
    }
}