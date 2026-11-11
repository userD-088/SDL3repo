#pragma once

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string>
#include <vector>
#include <utility>

#include "Utils.h"
#include "const.h"

// Vector Rush 3D
// A wireframe 3D arcade flyer rendered with a tiny software 3D pipeline
// (perspective projection + near plane clipping) on top of SDL_RenderLine.
class VectorRush
{
public:
    VectorRush(SDL_Window* window, SDL_Renderer* renderer);
    ~VectorRush();

    void run();

    // Minimal 3D math
    struct Vec3 {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;

        Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
        Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
        Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
        float length() const;
    };

    struct Mesh {
        std::vector<Vec3> vertices;
        std::vector<std::pair<int, int>> edges;
    };

private:
    enum class State {
        Title,
        Playing,
        Paused,
        GameOver
    };

    enum class EntityType {
        Box,        // Pillars, wall pieces, sweeping bars
        Ring,       // Fly through for points and boost energy
        Crystal,    // Power ups
        Mine,       // Shootable, explodes on contact
        Drone,      // Enemy that hovers in front of you and shoots
        Bullet,     // Enemy projectile
        Boss
    };

    enum CrystalType {
        CRYSTAL_SHIELD = 0,
        CRYSTAL_RAPID,
        CRYSTAL_SPREAD,
        CRYSTAL_SLOW,
        CRYSTAL_COUNT
    };

    struct Entity {
        EntityType type;
        Vec3 pos;
        Vec3 vel;
        Vec3 half = {1.0f, 1.0f, 1.0f};   // Half extents (boxes) / radius in x
        Vec3 rot;
        Vec3 spin;

        // Oscillating movement (moving walls, sweeping bars, drones)
        Vec3 base;
        Vec3 amplitude;
        float frequency = 0.0f;
        float phase = 0.0f;

        float timer = 0.0f;
        float flash = 0.0f;
        int hp = 1;
        int kind = 0;
        bool alive = true;
        bool passed = false;
    };

    struct Shot {
        Vec3 pos;
        Vec3 vel;
        float life;
    };

    struct Spark {
        Vec3 pos;
        Vec3 vel;
        float life;
        float maxLife;
    };

    struct Ship {
        Vec3 pos = {0.0f, 3.0f, 0.0f};
        Vec3 vel;
        float roll = 0.0f;
        float pitch = 0.0f;

        int shield = 3;
        float energy = 50.0f;
        float invulnerable = 0.0f;
        float fireCooldown = 0.0f;
    };

    struct Camera {
        Vec3 pos;
        float pitch = 0.12f;
        float roll = 0.0f;
    };

    struct Banner {
        std::string title;
        std::string subtitle;
        float timer = 0.0f;
    };

    struct FloatingText {
        std::string text;
        Vec3 pos;
        float life;
    };

    // Main loop
    void handleEvents();
    void update();
    void render();

    // Game flow
    void resetGame();
    void startSector(int number);
    void finishSector();
    void gameOver();
    void damageShip();
    void showBanner(const std::string& title, const std::string& subtitle);

    // Update parts
    void updateShip(float realDt);
    void updateCamera(float realDt);
    void updateShots(float worldDt);
    void updateEntities(float worldDt);
    void updateEntity(Entity& entity, float worldDt);
    void updateBoss(Entity& boss, float worldDt);
    void updateSparks(float worldDt);
    void updateSpawner(float travelled);

    // Spawning
    void spawnPattern();
    void spawnPillars();
    void spawnRings();
    void spawnCrystal(float x, float y, float z);
    void spawnMines(bool homing);
    void spawnGate(bool moving);
    void spawnDrones();
    void spawnSweeper();
    void spawnBoss();
    void spawnSparks(const Vec3& pos, int count, float speed);
    void fireEnemyBullet(const Vec3& from, const Vec3& velocity);
    Entity& addBox(const Vec3& pos, const Vec3& half);

    // Collision helpers
    bool pointInBox(const Vec3& point, const Entity& box) const;

    // 3D rendering
    Vec3 toCamera(const Vec3& world) const;
    bool project(const Vec3& world, SDL_FPoint& screen) const;
    void line3D(const Vec3& a, const Vec3& b, float alpha = 1.0f, bool depthFade = true);
    void drawMesh(const Mesh& mesh, const Vec3& pos, const Vec3& scale, const Vec3& rot, float alpha = 1.0f);
    void drawLabel(const std::string& text, const Vec3& pos, float size = 0.7f);

    void renderBackground();
    void renderGrid();
    void renderEntities();
    void renderShip();
    void renderHud();
    void renderTitle();
    void renderOverlay(const std::string& title, const std::vector<std::string>& lines);

    static Vec3 rotate(const Vec3& p, const Vec3& r);
    void buildMeshes();

    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* font;
    TTF_Font* smallFont;

    Timer timer;
    float dt = 0.0f;
    float time = 0.0f;

    State state = State::Title;

    // Meshes
    Mesh cubeMesh;
    Mesh octaMesh;
    Mesh tetraMesh;
    Mesh icoMesh;
    Mesh ringMesh;
    Mesh shipMesh;
    Mesh mineMesh;
    Mesh droneMesh;

    // Scenery
    std::vector<Vec3> mountains;
    std::vector<SDL_FPoint> skyStars;

    // World
    Ship ship;
    Camera camera;
    std::vector<Entity> entities;
    std::vector<Entity> pending;   // Spawned this frame, merged after the update loop
    std::vector<Shot> shots;
    std::vector<Spark> sparks;
    std::vector<FloatingText> floatingTexts;

    float scroll = 0.0f;           // Total distance flown (for grid animation)
    float speed = 30.0f;           // Current forward speed
    float spawnDistance = 0.0f;    // Distance until the next pattern

    int sector = 1;
    float sectorDistance = 0.0f;
    float sectorLength = 1000.0f;
    bool bossActive = false;
    int bossLevel = 0;

    // Power ups
    float rapidTimer = 0.0f;
    float spreadTimer = 0.0f;
    float slowTimer = 0.0f;

    bool boosting = false;

    // Scoring & stats
    float score = 0.0f;
    int highscore = 0;
    int ringCombo = 0;
    int ringsCollected = 0;
    int enemiesDestroyed = 0;
    float totalDistance = 0.0f;
    bool newHighscore = false;

    float shake = 0.0f;
    float damageFlash = 0.0f;

    Banner banner;

    bool running = true;
};
