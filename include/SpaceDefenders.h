#pragma once

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string>
#include <vector>
#include <cmath>

#include "Utils.h"
#include "const.h"

class SpaceDefenders 
{
public:
    SpaceDefenders(SDL_Window* window, SDL_Renderer* renderer);
    ~SpaceDefenders();

    void run();

private:
    enum class State {
        Title,
        Playing,
        Paused,
        GameOver
    };

    struct Planet {
        SDL_FPoint position;
        float radius;
        float mass;
    };

    struct Moon {
        float orbitRadius;
        float angle;
        float angularSpeed;
        float radius;

        SDL_FPoint position() const;
    };

    struct Player {
        Polygon polygon;

        float direction;
        float rotationSpeed;

        SDL_FPoint velocity;
        SDL_FPoint accelaration;

        float thrust = 100.0f;

        int hp = 3;

        float invulnerable = 0.0f;
        float fireCooldown = 0.0f;
        bool thrusting = false;
    };

    struct Asteroid {
        std::vector<SDL_FPoint> Body;

        SDL_FPoint position;
        SDL_FPoint velocity;
        SDL_FPoint acceleration;

        int hp;
        int damage;

        int size;         // 3 = large, 2 = medium, 1 = small
        float radius;
        float rotation;
        float rotationSpeed;
        float hitFlash = 0.0f;
    };

    struct Laser {
        float x;
        float y;
        float angle;
        float len;
        float lifetime;
    };

    struct Ufo {
        SDL_FPoint position;
        float baseY;
        float speedX;
        float phase;
        float shootTimer;
        int hp;
    };

    struct Bullet {
        SDL_FPoint position;
        SDL_FPoint velocity;
        float lifetime;
    };

    enum class PickupType {
        Heart,
        Rapid,
        Triple,
        Repair
    };

    struct Pickup {
        SDL_FPoint position;
        SDL_FPoint velocity;
        PickupType type;
        float lifetime;
    };

    struct Particle {
        SDL_FPoint position;
        SDL_FPoint velocity;
        float length;
        float life;
        float maxLife;
    };

    struct Star {
        SDL_FPoint position;
        float brightness;
        float twinkle;
    };

    struct FloatingText {
        std::string text;
        SDL_FPoint position;
        float life;
    };

    // Speed of every laser
    float laserSpeed = 365.0f;

    SDL_FPoint calcGravPullEarth(const SDL_FPoint& pos, const Planet& earth);

    bool isPlayerPlanetIntersect(const Player& player, const Planet& planet);
    bool isPlayerCircleIntersect(const SDL_FPoint& center, float radius) const;

    // Game flow
    void resetGame();
    void startWave();
    void updateWave();
    void damagePlayer();
    void damageEarth(int amount);
    void gameOver();

    // Spawning
    void spawnAsteroid();
    Asteroid createAsteroid(SDL_FPoint position, SDL_FPoint velocity, int size);
    void splitAsteroid(const Asteroid& asteroid);
    void spawnUfo();
    void spawnExplosion(SDL_FPoint position, int count, float speed);
    void spawnPickup(SDL_FPoint position);
    void addFloatingText(const std::string& text, SDL_FPoint position);

    // Update parts
    void updatePlayer();
    void updateLasers();
    void updateAsteroids();
    void updateUfos();
    void updatePickups();
    void updateEffects();

    // Render parts
    void renderStars();
    void renderWorld();
    void renderTrajectory();
    void renderHud();
    void renderTitle();
    void renderOverlay(const std::string& title, const std::string& line1, const std::string& line2);

    Timer timer;
    float dt; 
    float time = 0.0f;

    // Colors
    SDL_Color white = { 255, 255, 255, 255 };
    SDL_Color gray = { 255, 255, 255, 120 };

    void handleEvents();
    void update();
    void render();

    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* font;
    TTF_Font* smallFont;

    // Game Objects and Variables
    float gravConstant = 6.674E-11f;

    Planet earth = {{SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f}, 55.0f, 5.9722E24f};
    Moon moon = {175.0f, 0.0f, 0.35f, 13.0f};

    Player player = {{{100.0f, 100.0f}, {{0, -16.0f},{0, 16.0f}, {36.0f, 0}}}, 0, SDL_PI_F, {0, 0}, {0, 0}, 100.0f};

    std::vector<Laser> lasers;
    std::vector<Asteroid> asteroids;
    std::vector<Ufo> ufos;
    std::vector<Bullet> bullets;
    std::vector<Pickup> pickups;
    std::vector<Particle> particles;
    std::vector<Star> stars;
    std::vector<FloatingText> floatingTexts;

    State state = State::Title;

    static constexpr int EARTH_MAX_HP = 10;
    static constexpr int PLAYER_MAX_HP = 5;

    int earthHp = EARTH_MAX_HP;
    int score = 0;
    int highscore = 0;
    int wave = 0;
    int asteroidsToSpawn = 0;
    int ufosToSpawn = 0;
    int combo = 0;

    float spawnTimer = 0.0f;
    float waveBannerTimer = 0.0f;
    float comboTimer = 0.0f;
    float rapidFireTimer = 0.0f;
    float tripleShotTimer = 0.0f;
    float shake = 0.0f;
    float earthFlash = 0.0f;

    bool showTrajectory = true;
    bool newHighscore = false;

    bool running = true;
};
