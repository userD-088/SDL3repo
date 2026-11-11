#include "VectorRush.h"

#include <algorithm>
#include <cmath>

namespace {
    // Projection
    constexpr float FOCAL = 480.0f;
    constexpr float NEAR_PLANE = 0.3f;
    constexpr float FAR_PLANE = 160.0f;
    constexpr float FADE_START = 25.0f;
    constexpr float SCREEN_CX = SCREEN_WIDTH / 2.0f;
    constexpr float SCREEN_CY = SCREEN_HEIGHT / 2.0f;

    // Play area
    constexpr float AREA_X = 9.0f;
    constexpr float AREA_MIN_Y = 0.7f;
    constexpr float AREA_MAX_Y = 7.2f;
    constexpr float SPAWN_Z = 150.0f;

    // Ship
    constexpr float SHIP_ACCEL = 70.0f;
    constexpr float SHIP_DAMPING = 6.0f;
    constexpr float SHIP_HALF_X = 0.9f;
    constexpr float SHIP_HALF_Y = 0.35f;
    constexpr float SHIP_HALF_Z = 1.0f;
    constexpr int SHIP_MAX_SHIELD = 5;
    constexpr float INVULNERABLE_TIME = 1.6f;

    constexpr float SHOT_SPEED = 150.0f;
    constexpr float FIRE_COOLDOWN = 0.22f;
    constexpr float RAPID_COOLDOWN = 0.08f;

    constexpr float POWERUP_TIME = 10.0f;

    const char* CRYSTAL_LABELS[] = {"S", "R", "W", "T"};
    const char* CRYSTAL_NAMES[] = {"SHIELD +1", "RAPID FIRE", "WIDE SHOT", "SLOW MOTION"};

    float clampf(float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); }
}

float VectorRush::Vec3::length() const
{
    return std::sqrt(x * x + y * y + z * z);
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

VectorRush::VectorRush(SDL_Window* window, SDL_Renderer* renderer)
    : window(window), renderer(renderer)
{
    font = openFont(48);
    smallFont = openFont(24);

    highscore = loadHighscore("vectorrush");

    buildMeshes();

    // Mountain silhouette on the horizon
    float x = -260.0f;
    int index = 0;
    while (x <= 260.0f)
    {
        float height = (index++ % 2 == 0) ? randomFloat(0.0f, 4.0f) : randomFloat(6.0f, 18.0f);
        if (std::fabs(x) < 40.0f) height *= 0.4f; // Leave room for the sun
        mountains.push_back({x, height, FAR_PLANE - 5.0f});
        x += randomFloat(6.0f, 16.0f);
    }

    for (int i = 0; i < 80; ++i)
    {
        skyStars.push_back({randomFloat(0, SCREEN_WIDTH), randomFloat(0, 230)});
    }

    camera.pos = {0.0f, 5.0f, -8.0f};
}

VectorRush::~VectorRush()
{
    if (font) TTF_CloseFont(font);
    if (smallFont) TTF_CloseFont(smallFont);
}

void VectorRush::buildMeshes()
{
    // Cube
    for (int i = 0; i < 8; ++i)
    {
        cubeMesh.vertices.push_back({(i & 1) ? 1.0f : -1.0f, (i & 2) ? 1.0f : -1.0f, (i & 4) ? 1.0f : -1.0f});
    }
    for (int i = 0; i < 8; ++i)
    {
        for (int bit = 1; bit < 8; bit <<= 1)
        {
            if (!(i & bit)) cubeMesh.edges.push_back({i, i | bit});
        }
    }

    // Octahedron
    octaMesh.vertices = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (int i = 0; i < 6; ++i)
    {
        for (int j = i + 1; j < 6; ++j)
        {
            if (j != (i ^ 1)) octaMesh.edges.push_back({i, j});
        }
    }

    // Tetrahedron
    tetraMesh.vertices = {{1, 1, 1}, {1, -1, -1}, {-1, 1, -1}, {-1, -1, 1}};
    tetraMesh.edges = {{0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3}};

    // Icosahedron (edges = all vertex pairs with distance 2)
    const float phi = (1.0f + std::sqrt(5.0f)) / 2.0f;
    for (float a : {-1.0f, 1.0f})
    {
        for (float b : {-phi, phi})
        {
            icoMesh.vertices.push_back({0, a, b});
            icoMesh.vertices.push_back({a, b, 0});
            icoMesh.vertices.push_back({b, 0, a});
        }
    }
    for (size_t i = 0; i < icoMesh.vertices.size(); ++i)
    {
        for (size_t j = i + 1; j < icoMesh.vertices.size(); ++j)
        {
            float d = (icoMesh.vertices[i] - icoMesh.vertices[j]).length();
            if (std::fabs(d - 2.0f) < 0.01f) icoMesh.edges.push_back({static_cast<int>(i), static_cast<int>(j)});
        }
    }
    for (Vec3& v : icoMesh.vertices) v = v * (1.0f / std::sqrt(1.0f + phi * phi));

    // Ring (double polygon in the xy plane)
    const int segments = 14;
    for (int i = 0; i < segments; ++i)
    {
        float a = i * 2.0f * SDL_PI_F / segments;
        ringMesh.vertices.push_back({std::cos(a), std::sin(a), 0.0f});
        ringMesh.vertices.push_back({std::cos(a) * 0.82f, std::sin(a) * 0.82f, 0.0f});
    }
    for (int i = 0; i < segments; ++i)
    {
        int next = (i + 1) % segments;
        ringMesh.edges.push_back({i * 2, next * 2});
        ringMesh.edges.push_back({i * 2 + 1, next * 2 + 1});
        if (i % 2 == 0) ringMesh.edges.push_back({i * 2, i * 2 + 1});
    }

    // Player ship (forward = +z)
    shipMesh.vertices = {
        {0.0f, 0.0f, 2.0f},     // 0 nose
        {-1.5f, -0.1f, -0.7f},  // 1 left wing tip
        {1.5f, -0.1f, -0.7f},   // 2 right wing tip
        {0.0f, 0.45f, -0.4f},   // 3 cockpit top
        {0.0f, -0.25f, -0.4f},  // 4 belly
        {-0.45f, 0.0f, -0.8f},  // 5 rear left
        {0.45f, 0.0f, -0.8f},   // 6 rear right
        {0.0f, 0.9f, -0.85f}    // 7 tail fin
    };
    shipMesh.edges = {
        {0, 1}, {0, 2}, {0, 3}, {0, 4},
        {1, 5}, {2, 6}, {5, 6},
        {3, 5}, {3, 6}, {4, 5}, {4, 6},
        {3, 7}, {7, 5}, {7, 6}
    };

    // Mine: cube with spikes
    mineMesh = cubeMesh;
    for (Vec3& v : mineMesh.vertices) v = v * 0.55f;
    Vec3 spikes[] = {{1.6f, 0, 0}, {-1.6f, 0, 0}, {0, 1.6f, 0}, {0, -1.6f, 0}, {0, 0, 1.6f}, {0, 0, -1.6f}};
    for (const Vec3& spike : spikes)
    {
        int tip = static_cast<int>(mineMesh.vertices.size());
        mineMesh.vertices.push_back(spike);
        mineMesh.vertices.push_back(spike * 0.3f);
        mineMesh.edges.push_back({tip, tip + 1});
    }

    // Drone: flat diamond body with two wing blades
    droneMesh.vertices = {
        {0, 0, -1.2f}, {1.0f, 0, 0}, {0, 0, 1.0f}, {-1.0f, 0, 0}, {0, 0.6f, 0}, {0, -0.6f, 0},
        {2.2f, 0.3f, -0.3f}, {2.2f, -0.3f, 0.3f}, {-2.2f, 0.3f, -0.3f}, {-2.2f, -0.3f, 0.3f}
    };
    droneMesh.edges = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 0}, {4, 1}, {4, 2}, {4, 3},
        {5, 0}, {5, 1}, {5, 2}, {5, 3},
        {1, 6}, {1, 7}, {6, 7}, {3, 8}, {3, 9}, {8, 9}
    };
}

// ---------------------------------------------------------------------------
// Main loop
// ---------------------------------------------------------------------------

void VectorRush::run()
{
    while (running)
    {
        dt = std::min(timer.deltaTime(), 0.05f);

        handleEvents();
        update();
        render();

        SDL_Delay(16);
    }
}

void VectorRush::handleEvents()
{
    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_EVENT_QUIT)
        {
            running = false;
            SDL_PushEvent(&event);
            return;
        }

        if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat)
        {
            continue;
        }

        SDL_Keycode key = event.key.key;

        switch (state)
        {
            case State::Title:
                if (key == SDLK_RETURN || key == SDLK_SPACE) resetGame();
                if (key == SDLK_ESCAPE) running = false;
                break;

            case State::Playing:
                if (key == SDLK_P) state = State::Paused;
                if (key == SDLK_ESCAPE) state = State::Paused;
                break;

            case State::Paused:
                if (key == SDLK_P || key == SDLK_RETURN) state = State::Playing;
                if (key == SDLK_ESCAPE) state = State::Title;
                break;

            case State::GameOver:
                if (key == SDLK_RETURN || key == SDLK_R) resetGame();
                if (key == SDLK_ESCAPE) state = State::Title;
                break;
        }
    }
}

void VectorRush::update()
{
    time += dt;

    damageFlash = std::max(0.0f, damageFlash - dt * 2.0f);
    shake = std::max(0.0f, shake - dt * 20.0f);

    if (banner.timer > 0.0f) banner.timer -= dt;

    if (state == State::Title)
    {
        // Idle flight over the grid
        scroll += 20.0f * dt;
        camera.pos = {std::sin(time * 0.3f) * 3.0f, 4.5f, -8.0f};
        camera.roll = std::sin(time * 0.3f) * 0.05f;
        return;
    }

    if (state != State::Playing)
    {
        return;
    }

    // Slow motion only affects the world, the ship stays responsive
    float worldDt = slowTimer > 0.0f ? dt * 0.5f : dt;

    updateShip(dt);

    float baseSpeed = 32.0f + sector * 4.0f;
    speed = boosting ? baseSpeed * 1.7f : baseSpeed;

    float travelled = speed * worldDt;
    scroll += travelled;
    totalDistance += travelled;

    if (!bossActive)
    {
        sectorDistance += travelled;
        score += travelled * (boosting ? 1.0f : 0.5f);

        if (sectorDistance >= sectorLength)
        {
            if (sector % 3 == 0) spawnBoss();
            else finishSector();
        }
    }

    updateSpawner(travelled);
    updateShots(worldDt);
    updateEntities(worldDt);
    updateSparks(worldDt);
    updateCamera(dt);

    for (auto it = floatingTexts.begin(); it != floatingTexts.end(); )
    {
        it->pos.y += 2.0f * dt;
        it->pos.z -= speed * worldDt * 0.5f;
        it->life -= dt;

        if (it->life <= 0.0f) it = floatingTexts.erase(it);
        else ++it;
    }

    if (rapidTimer > 0.0f) rapidTimer -= dt;
    if (spreadTimer > 0.0f) spreadTimer -= dt;
    if (slowTimer > 0.0f) slowTimer -= dt;
}

// ---------------------------------------------------------------------------
// Game flow
// ---------------------------------------------------------------------------

void VectorRush::resetGame()
{
    ship = Ship();
    entities.clear();
    pending.clear();
    shots.clear();
    sparks.clear();
    floatingTexts.clear();

    score = 0.0f;
    ringCombo = 0;
    ringsCollected = 0;
    enemiesDestroyed = 0;
    totalDistance = 0.0f;
    newHighscore = false;

    rapidTimer = 0.0f;
    spreadTimer = 0.0f;
    slowTimer = 0.0f;

    bossActive = false;
    bossLevel = 0;

    state = State::Playing;

    startSector(1);
}

void VectorRush::startSector(int number)
{
    sector = number;
    sectorDistance = 0.0f;
    sectorLength = 900.0f + sector * 150.0f;
    spawnDistance = 40.0f;

    std::string subtitle = "Speed " + std::to_string(32 + sector * 4);
    if (sector % 3 == 0) subtitle += "  -  Guardian ahead!";

    showBanner("SECTOR " + std::to_string(sector), subtitle);
}

void VectorRush::finishSector()
{
    int bonus = 500 * sector + ship.shield * 100;
    score += bonus;

    startSector(sector + 1);
    banner.subtitle = "Sector bonus +" + std::to_string(bonus);
}

void VectorRush::gameOver()
{
    state = State::GameOver;

    spawnSparks(ship.pos, 80, 25.0f);

    if (static_cast<int>(score) > highscore)
    {
        highscore = static_cast<int>(score);
        newHighscore = true;
        saveHighscore("vectorrush", highscore);
    }
}

void VectorRush::damageShip()
{
    if (ship.invulnerable > 0.0f) return;

    ship.shield--;
    ship.invulnerable = INVULNERABLE_TIME;
    ringCombo = 0;
    shake = 12.0f;
    damageFlash = 1.0f;

    spawnSparks(ship.pos, 25, 15.0f);

    if (ship.shield <= 0)
    {
        gameOver();
    }
}

void VectorRush::showBanner(const std::string& title, const std::string& subtitle)
{
    banner.title = title;
    banner.subtitle = subtitle;
    banner.timer = 3.0f;
}

// ---------------------------------------------------------------------------
// Update parts
// ---------------------------------------------------------------------------

void VectorRush::updateShip(float realDt)
{
    const bool* keyboard = SDL_GetKeyboardState(nullptr);

    Vec3 input;
    if (keyboard[SDL_SCANCODE_A] || keyboard[SDL_SCANCODE_LEFT]) input.x -= 1.0f;
    if (keyboard[SDL_SCANCODE_D] || keyboard[SDL_SCANCODE_RIGHT]) input.x += 1.0f;
    if (keyboard[SDL_SCANCODE_W] || keyboard[SDL_SCANCODE_UP]) input.y += 1.0f;
    if (keyboard[SDL_SCANCODE_S] || keyboard[SDL_SCANCODE_DOWN]) input.y -= 1.0f;

    // Smooth acceleration with damping
    ship.vel.x += (input.x * SHIP_ACCEL - ship.vel.x * SHIP_DAMPING) * realDt;
    ship.vel.y += (input.y * SHIP_ACCEL - ship.vel.y * SHIP_DAMPING) * realDt;

    ship.pos.x += ship.vel.x * realDt;
    ship.pos.y += ship.vel.y * realDt;

    if (ship.pos.x < -AREA_X) { ship.pos.x = -AREA_X; ship.vel.x = 0.0f; }
    if (ship.pos.x > AREA_X) { ship.pos.x = AREA_X; ship.vel.x = 0.0f; }
    if (ship.pos.y < AREA_MIN_Y) { ship.pos.y = AREA_MIN_Y; ship.vel.y = 0.0f; }
    if (ship.pos.y > AREA_MAX_Y) { ship.pos.y = AREA_MAX_Y; ship.vel.y = 0.0f; }

    // Banking
    ship.roll = -ship.vel.x * 0.05f;
    ship.pitch = -ship.vel.y * 0.035f;

    // Boost
    bool wantsBoost = keyboard[SDL_SCANCODE_LSHIFT] || keyboard[SDL_SCANCODE_RSHIFT];
    boosting = wantsBoost && ship.energy > 0.0f;

    if (boosting) ship.energy = std::max(0.0f, ship.energy - 28.0f * realDt);
    else ship.energy = std::min(100.0f, ship.energy + 3.0f * realDt);

    if (ship.invulnerable > 0.0f) ship.invulnerable -= realDt;
    if (ship.fireCooldown > 0.0f) ship.fireCooldown -= realDt;

    // Shooting
    if (keyboard[SDL_SCANCODE_SPACE] && ship.fireCooldown <= 0.0f)
    {
        Vec3 muzzle = ship.pos + Vec3{0.0f, 0.0f, 2.0f};

        shots.push_back({muzzle + Vec3{-0.7f, 0, 0}, {0, 0, SHOT_SPEED}, 1.2f});
        shots.push_back({muzzle + Vec3{0.7f, 0, 0}, {0, 0, SHOT_SPEED}, 1.2f});

        if (spreadTimer > 0.0f)
        {
            shots.push_back({muzzle, {-SHOT_SPEED * 0.12f, 0, SHOT_SPEED}, 1.2f});
            shots.push_back({muzzle, {SHOT_SPEED * 0.12f, 0, SHOT_SPEED}, 1.2f});
            shots.push_back({muzzle, {0, SHOT_SPEED * 0.08f, SHOT_SPEED}, 1.2f});
        }

        ship.fireCooldown = rapidTimer > 0.0f ? RAPID_COOLDOWN : FIRE_COOLDOWN;
    }
}

void VectorRush::updateCamera(float realDt)
{
    // Follow the ship with a little lag
    Vec3 target = {ship.pos.x * 0.75f, ship.pos.y * 0.85f + 2.6f, boosting ? -9.5f : -8.0f};

    float follow = 1.0f - std::exp(-realDt * 6.0f);
    camera.pos = camera.pos + (target - camera.pos) * follow;
    camera.roll = ship.roll * 0.35f;
    camera.pitch = 0.12f;
}

namespace {
    // Distance from point p to the segment a-b
    float segmentDistance(const VectorRush::Vec3& a, const VectorRush::Vec3& b, const VectorRush::Vec3& p)
    {
        VectorRush::Vec3 ab = b - a;
        VectorRush::Vec3 ap = p - a;

        float lengthSquared = ab.x * ab.x + ab.y * ab.y + ab.z * ab.z;
        float t = lengthSquared > 0.0f ? (ap.x * ab.x + ap.y * ab.y + ap.z * ab.z) / lengthSquared : 0.0f;
        t = std::max(0.0f, std::min(1.0f, t));

        return (a + ab * t - p).length();
    }
}

void VectorRush::updateShots(float worldDt)
{
    for (auto it = shots.begin(); it != shots.end(); )
    {
        Vec3 previous = it->pos;
        it->pos = it->pos + it->vel * worldDt;

        // Previous position relative to scrolling objects (they move towards the ship)
        Vec3 previousScrolled = previous - Vec3{0.0f, 0.0f, speed * worldDt};
        it->life -= worldDt;

        bool remove = it->life <= 0.0f || it->pos.z > FAR_PLANE;

        for (Entity& e : entities)
        {
            if (remove) break;
            if (!e.alive) continue;

            switch (e.type)
            {
                case EntityType::Box:
                    if (pointInBox(it->pos, e))
                    {
                        spawnSparks(it->pos, 3, 6.0f);
                        remove = true;
                    }
                    break;

                case EntityType::Mine:
                case EntityType::Drone:
                {
                    float radius = e.type == EntityType::Mine ? 1.4f : 2.0f;

                    if (segmentDistance(previousScrolled, it->pos, e.pos) < radius)
                    {
                        e.hp--;
                        e.flash = 0.1f;
                        remove = true;

                        if (e.hp <= 0)
                        {
                            e.alive = false;
                            enemiesDestroyed++;

                            int points = e.type == EntityType::Mine ? 150 : 400;
                            score += points;
                            floatingTexts.push_back({"+" + std::to_string(points), e.pos, 1.0f});
                            spawnSparks(e.pos, 40, 14.0f);

                            if (e.type == EntityType::Drone && randomInt(0, 2) == 0)
                            {
                                spawnCrystal(e.pos.x, e.pos.y, e.pos.z);
                            }
                        }
                        else
                        {
                            spawnSparks(it->pos, 5, 8.0f);
                        }
                    }
                    break;
                }

                case EntityType::Boss:
                    if (segmentDistance(previous, it->pos, e.pos) < 5.0f)
                    {
                        e.hp--;
                        e.flash = 0.08f;
                        remove = true;
                        spawnSparks(it->pos, 4, 10.0f);
                    }
                    break;

                default:
                    break;
            }
        }

        if (remove) it = shots.erase(it);
        else ++it;
    }
}

void VectorRush::updateEntities(float worldDt)
{
    for (Entity& entity : entities)
    {
        if (entity.alive) updateEntity(entity, worldDt);
    }

    if (state != State::Playing) return;

    entities.erase(
        std::remove_if(entities.begin(), entities.end(), [](const Entity& e) { return !e.alive; }),
        entities.end());

    for (Entity& e : pending) entities.push_back(e);
    pending.clear();
}

void VectorRush::updateEntity(Entity& e, float worldDt)
{
    if (e.type == EntityType::Boss)
    {
        updateBoss(e, worldDt);
        return;
    }

    float previousZ = e.pos.z;

    e.timer += worldDt;
    e.rot = e.rot + e.spin * worldDt;
    if (e.flash > 0.0f) e.flash -= worldDt;

    // World scrolls towards the ship (bullets fly on their own)
    if (e.type != EntityType::Bullet)
    {
        e.base.z -= speed * worldDt;
    }

    switch (e.type)
    {
        case EntityType::Box:
        {
            e.pos.z = e.base.z;
            e.pos.x = e.base.x + std::sin(e.timer * e.frequency + e.phase) * e.amplitude.x;
            e.pos.y = e.base.y + std::sin(e.timer * e.frequency + e.phase) * e.amplitude.y;

            // Swept test along z, so fast boxes can not tunnel through the ship
            bool overlapXY = std::fabs(ship.pos.x - e.pos.x) < SHIP_HALF_X + e.half.x &&
                             std::fabs(ship.pos.y - e.pos.y) < SHIP_HALF_Y + e.half.y;
            float zMin = std::min(previousZ, e.pos.z) - e.half.z;
            float zMax = std::max(previousZ, e.pos.z) + e.half.z;
            bool overlapZ = zMin < ship.pos.z + SHIP_HALF_Z && zMax > ship.pos.z - SHIP_HALF_Z;

            if (ship.invulnerable <= 0.0f && overlapXY && overlapZ)
            {
                spawnSparks(ship.pos + Vec3{0, 0, 1.0f}, 20, 10.0f);
                damageShip();
            }
            break;
        }

        case EntityType::Ring:
        {
            e.pos.z = e.base.z;

            // Ship crosses the ring plane
            if (!e.passed && previousZ > 0.0f && e.pos.z <= 0.0f)
            {
                e.passed = true;

                float dx = ship.pos.x - e.pos.x;
                float dy = ship.pos.y - e.pos.y;

                if (std::sqrt(dx * dx + dy * dy) < e.half.x * 0.9f)
                {
                    ringCombo++;
                    ringsCollected++;

                    int points = 50 * std::min(ringCombo, 10);
                    score += points;
                    ship.energy = std::min(100.0f, ship.energy + 15.0f);

                    floatingTexts.push_back({"+" + std::to_string(points), e.pos + Vec3{0, 1.5f, 0}, 1.0f});
                    e.flash = 0.4f;
                    e.half.x *= 1.3f;
                }
                else
                {
                    ringCombo = 0;
                }
            }
            break;
        }

        case EntityType::Crystal:
        {
            e.pos = e.base;
            e.pos.y += std::sin(e.timer * 3.0f) * 0.3f;

            if ((e.pos - ship.pos).length() < 1.8f)
            {
                switch (e.kind)
                {
                    case CRYSTAL_SHIELD: ship.shield = std::min(SHIP_MAX_SHIELD, ship.shield + 1); break;
                    case CRYSTAL_RAPID: rapidTimer = POWERUP_TIME; break;
                    case CRYSTAL_SPREAD: spreadTimer = POWERUP_TIME; break;
                    case CRYSTAL_SLOW: slowTimer = POWERUP_TIME * 0.6f; break;
                }

                score += 100;
                showBanner(CRYSTAL_NAMES[e.kind], "");
                banner.timer = 1.5f;
                spawnSparks(e.pos, 20, 8.0f);
                e.alive = false;
            }
            break;
        }

        case EntityType::Mine:
        {
            // Homing mines drift towards the ship when close
            if (e.kind == 1 && e.base.z < 60.0f && e.base.z > 2.0f)
            {
                e.base.x += clampf(ship.pos.x - e.base.x, -1.0f, 1.0f) * 3.5f * worldDt;
                e.base.y += clampf(ship.pos.y - e.base.y, -1.0f, 1.0f) * 3.5f * worldDt;
            }

            e.pos = e.base;

            if (ship.invulnerable <= 0.0f && (e.pos - ship.pos).length() < 1.6f)
            {
                spawnSparks(e.pos, 40, 14.0f);
                e.alive = false;
                damageShip();
            }
            break;
        }

        case EntityType::Drone:
        {
            // Phase 0: approach, Phase 1: hover in front of the ship and shoot, Phase 2: fly away
            if (e.kind == 0)
            {
                e.pos.z = e.base.z;

                if (e.pos.z <= e.amplitude.z)
                {
                    e.kind = 1;
                    e.timer = 0.0f;
                }
            }
            else if (e.kind == 1)
            {
                e.base.z += speed * worldDt; // Cancel scrolling: stay in front of the ship
                e.pos.z = e.amplitude.z;

                e.vel.y -= worldDt;

                if (e.vel.y <= 0.0f)
                {
                    Vec3 dir = ship.pos - e.pos;
                    float len = dir.length();
                    fireEnemyBullet(e.pos, dir * (28.0f / std::max(len, 0.001f)));
                    e.vel.y = std::max(0.9f, 2.0f - sector * 0.1f);
                }

                if (e.timer > 7.0f)
                {
                    e.kind = 2;
                }
            }
            else
            {
                // Climb and accelerate out of view
                e.amplitude.y += 6.0f * worldDt;
                e.pos.z += 30.0f * worldDt;
                e.base.z = e.pos.z;
                if (e.pos.z > FAR_PLANE) e.alive = false;
            }

            e.pos.x = e.base.x + std::sin(time * e.frequency + e.phase) * e.amplitude.x;
            e.pos.y = e.base.y + std::sin(time * e.frequency * 1.3f + e.phase) * 1.0f + (e.kind == 2 ? e.amplitude.y : 0.0f);

            if (ship.invulnerable <= 0.0f && (e.pos - ship.pos).length() < 2.0f)
            {
                spawnSparks(e.pos, 30, 14.0f);
                e.alive = false;
                damageShip();
            }
            break;
        }

        case EntityType::Bullet:
        {
            e.pos = e.pos + e.vel * worldDt;

            if (ship.invulnerable <= 0.0f && (e.pos - ship.pos).length() < 0.9f)
            {
                e.alive = false;
                damageShip();
            }

            if (e.pos.z < -15.0f || e.pos.z > FAR_PLANE) e.alive = false;
            break;
        }

        default:
            break;
    }

    // Passed behind the camera
    if (e.type != EntityType::Bullet && e.type != EntityType::Drone && e.pos.z < -15.0f)
    {
        e.alive = false;
    }

    if (e.type == EntityType::Drone && e.kind == 0 && e.pos.z < -15.0f)
    {
        e.alive = false;
    }
}

void VectorRush::updateBoss(Entity& boss, float worldDt)
{
    boss.timer += worldDt;
    boss.rot = boss.rot + boss.spin * worldDt;
    if (boss.flash > 0.0f) boss.flash -= worldDt;

    // Fly in, then hover
    if (boss.pos.z > 48.0f) boss.pos.z -= 25.0f * worldDt;

    boss.pos.x = std::sin(boss.timer * 0.6f) * 6.0f;
    boss.pos.y = 4.0f + std::sin(boss.timer * 1.1f) * 2.0f;

    // Attack patterns
    boss.vel.x -= worldDt;   // Aimed volley
    boss.vel.y -= worldDt;   // Bullet ring
    boss.vel.z -= worldDt;   // Spiral (level 2+)

    if (boss.vel.x <= 0.0f)
    {
        for (int i = -1; i <= 1; ++i)
        {
            Vec3 target = ship.pos + Vec3{i * 2.5f, 0.0f, 0.0f};
            Vec3 dir = target - boss.pos;
            fireEnemyBullet(boss.pos, dir * (30.0f / dir.length()));
        }
        boss.vel.x = std::max(1.0f, 2.2f - bossLevel * 0.3f);
    }

    if (boss.vel.y <= 0.0f)
    {
        int count = 10 + bossLevel * 2;
        for (int i = 0; i < count; ++i)
        {
            float a = i * 2.0f * SDL_PI_F / count;
            fireEnemyBullet(boss.pos, {std::cos(a) * 5.0f, std::sin(a) * 3.0f, -24.0f});
        }
        boss.vel.y = 5.0f;
    }

    if (bossLevel >= 2 && boss.vel.z <= 0.0f)
    {
        float a = boss.timer * 4.0f;
        fireEnemyBullet(boss.pos, {std::cos(a) * 6.0f, std::sin(a) * 4.0f, -26.0f});
        boss.vel.z = 0.15f;
    }

    if (ship.invulnerable <= 0.0f && (boss.pos - ship.pos).length() < 5.0f)
    {
        damageShip();
    }

    if (boss.hp <= 0)
    {
        boss.alive = false;
        bossActive = false;
        enemiesDestroyed++;

        int bonus = 5000 * bossLevel;
        score += bonus;

        spawnSparks(boss.pos, 150, 30.0f);
        shake = 15.0f;

        // Clear remaining bullets
        for (Entity& e : entities)
        {
            if (e.type == EntityType::Bullet) e.alive = false;
        }
        pending.clear();

        // Reward
        spawnCrystal(boss.pos.x, boss.pos.y, boss.pos.z);

        startSector(sector + 1);
        banner.title = "GUARDIAN DESTROYED";
        banner.subtitle = "+" + std::to_string(bonus) + "   -   Next: Sector " + std::to_string(sector);
        banner.timer = 3.5f;
    }
}

void VectorRush::updateSparks(float worldDt)
{
    for (auto it = sparks.begin(); it != sparks.end(); )
    {
        it->pos = it->pos + it->vel * worldDt;
        it->pos.z -= speed * worldDt * 0.6f;
        it->vel.y -= 9.0f * worldDt;
        it->life -= worldDt;

        if (it->life <= 0.0f) it = sparks.erase(it);
        else ++it;
    }
}

void VectorRush::updateSpawner(float travelled)
{
    spawnDistance -= travelled;

    if (spawnDistance > 0.0f) return;

    if (bossActive)
    {
        // Only occasional rings and crystals during the boss fight
        if (randomInt(0, 2) == 0) spawnRings();
        else if (randomInt(0, 4) == 0) spawnCrystal(randomFloat(-7, 7), randomFloat(1.5f, 6.0f), SPAWN_Z);

        spawnDistance = 70.0f;
        return;
    }

    spawnPattern();

    spawnDistance = std::max(16.0f, 34.0f - sector * 1.5f) * randomFloat(0.8f, 1.2f);
}

// ---------------------------------------------------------------------------
// Spawning
// ---------------------------------------------------------------------------

VectorRush::Entity& VectorRush::addBox(const Vec3& pos, const Vec3& half)
{
    Entity box;
    box.type = EntityType::Box;
    box.pos = pos;
    box.base = pos;
    box.half = half;

    pending.push_back(box);
    return pending.back();
}

void VectorRush::spawnPattern()
{
    struct Option { int id; int weight; int minSector; };

    const Option options[] = {
        {0, 5, 1},   // Pillars
        {1, 4, 1},   // Rings
        {2, 1, 1},   // Crystal
        {3, 3, 2},   // Mines
        {4, 3, 2},   // Gate
        {5, 2, 2},   // Drones
        {6, 2, 3},   // Sweeping bar
        {7, 2, 4},   // Moving gate
        {8, 2, 5}    // Homing mines
    };

    int total = 0;
    for (const Option& o : options)
    {
        if (sector >= o.minSector) total += o.weight;
    }

    int pick = randomInt(0, total - 1);
    int chosen = 0;

    for (const Option& o : options)
    {
        if (sector < o.minSector) continue;

        if (pick < o.weight)
        {
            chosen = o.id;
            break;
        }

        pick -= o.weight;
    }

    switch (chosen)
    {
        case 0: spawnPillars(); break;
        case 1: spawnRings(); break;
        case 2: spawnCrystal(randomFloat(-7, 7), randomFloat(1.5f, 6.0f), SPAWN_Z); break;
        case 3: spawnMines(false); break;
        case 4: spawnGate(false); break;
        case 5: spawnDrones(); break;
        case 6: spawnSweeper(); break;
        case 7: spawnGate(true); break;
        case 8: spawnMines(true); break;
    }
}

void VectorRush::spawnPillars()
{
    int count = randomInt(2, 3 + sector / 2);

    for (int i = 0; i < count; ++i)
    {
        float height = randomFloat(2.0f, 9.0f);
        float width = randomFloat(0.8f, 1.8f);

        // Some pillars hang from the "ceiling"
        bool hanging = sector >= 2 && randomInt(0, 3) == 0;
        float y = hanging ? 9.0f - height / 2.0f : height / 2.0f;

        addBox({randomFloat(-AREA_X, AREA_X), y, SPAWN_Z + randomFloat(0.0f, 20.0f)}, {width, height / 2.0f, width});
    }
}

void VectorRush::spawnRings()
{
    int count = randomInt(3, 6);

    float x = randomFloat(-6.0f, 6.0f);
    float y = randomFloat(2.0f, 5.5f);
    float dx = randomFloat(-1.5f, 1.5f);
    float dy = randomFloat(-0.7f, 0.7f);

    for (int i = 0; i < count; ++i)
    {
        Entity ring;
        ring.type = EntityType::Ring;
        ring.base = {clampf(x, -AREA_X + 1, AREA_X - 1), clampf(y, 1.5f, 6.5f), SPAWN_Z + i * 9.0f};
        ring.pos = ring.base;
        ring.half = {2.0f, 2.0f, 0.2f};
        ring.spin = {0.0f, 0.0f, 1.0f};

        pending.push_back(ring);

        x += dx;
        y += dy;
    }

    // A crystal sometimes waits after a ring line
    if (randomInt(0, 4) == 0) spawnCrystal(x, y, SPAWN_Z + count * 9.0f + 5.0f);
}

void VectorRush::spawnCrystal(float x, float y, float z)
{
    Entity crystal;
    crystal.type = EntityType::Crystal;
    crystal.base = {clampf(x, -AREA_X, AREA_X), clampf(y, AREA_MIN_Y + 0.5f, AREA_MAX_Y), z};
    crystal.pos = crystal.base;
    crystal.spin = {0.0f, 2.0f, 0.0f};
    crystal.kind = randomInt(0, CRYSTAL_COUNT - 1);

    pending.push_back(crystal);
}

void VectorRush::spawnMines(bool homing)
{
    int count = randomInt(3, 4 + sector / 2);

    for (int i = 0; i < count; ++i)
    {
        Entity mine;
        mine.type = EntityType::Mine;
        mine.base = {randomFloat(-AREA_X, AREA_X), randomFloat(1.0f, 6.5f), SPAWN_Z + randomFloat(0.0f, 30.0f)};
        mine.pos = mine.base;
        mine.spin = {randomFloat(-2, 2), randomFloat(-2, 2), randomFloat(-2, 2)};
        mine.hp = 1;
        mine.kind = homing ? 1 : 0;

        pending.push_back(mine);
    }
}

void VectorRush::spawnGate(bool moving)
{
    // A full wall with one hole to fly through
    const float wallLeft = -15.0f;
    const float wallRight = 15.0f;
    const float wallTop = 9.0f;
    const float thickness = 0.6f;

    float gapW = std::max(3.2f, 5.0f - sector * 0.15f);
    float gapH = std::max(2.4f, 3.4f - sector * 0.08f);
    float gx = randomFloat(-AREA_X + gapW / 2.0f, AREA_X - gapW / 2.0f);
    float gy = randomFloat(AREA_MIN_Y + gapH / 2.0f, AREA_MAX_Y - gapH / 2.0f + 0.5f);

    if (moving) gx = clampf(gx, -4.0f, 4.0f);

    float z = SPAWN_Z;
    float phase = randomFloat(0.0f, 6.0f);
    float amplitude = moving ? 4.0f : 0.0f;

    auto piece = [&](float x1, float x2, float y1, float y2) {
        if (x2 - x1 < 0.05f || y2 - y1 < 0.05f) return;

        Entity& box = addBox({(x1 + x2) / 2.0f, (y1 + y2) / 2.0f, z}, {(x2 - x1) / 2.0f, (y2 - y1) / 2.0f, thickness});
        box.amplitude = {amplitude, 0.0f, 0.0f};
        box.frequency = 1.2f;
        box.phase = phase;
        box.kind = 1; // Wall piece (drawn with cross bracing)
    };

    // When moving, the side pieces get extra width so the wall never opens up
    float extra = moving ? amplitude : 0.0f;

    piece(wallLeft - extra, gx - gapW / 2.0f, 0.0f, wallTop);                 // Left
    piece(gx + gapW / 2.0f, wallRight + extra, 0.0f, wallTop);                // Right
    piece(gx - gapW / 2.0f, gx + gapW / 2.0f, 0.0f, gy - gapH / 2.0f);        // Bottom
    piece(gx - gapW / 2.0f, gx + gapW / 2.0f, gy + gapH / 2.0f, wallTop);     // Top

    // Reward ring in the gap
    if (!moving)
    {
        Entity ring;
        ring.type = EntityType::Ring;
        ring.base = {gx, gy, z + 6.0f};
        ring.pos = ring.base;
        ring.half = {1.3f, 1.3f, 0.2f};
        ring.spin = {0.0f, 0.0f, -1.5f};
        pending.push_back(ring);
    }
}

void VectorRush::spawnDrones()
{
    int count = std::min(1 + sector / 3, 4);

    for (int i = 0; i < count; ++i)
    {
        Entity drone;
        drone.type = EntityType::Drone;
        drone.base = {randomFloat(-6.0f, 6.0f), randomFloat(2.5f, 6.0f), SPAWN_Z + i * 8.0f};
        drone.pos = drone.base;
        drone.amplitude = {randomFloat(2.0f, 4.0f), 0.0f, randomFloat(35.0f, 50.0f)};
        drone.frequency = randomFloat(0.8f, 1.5f);
        drone.phase = randomFloat(0.0f, 6.0f);
        drone.spin = {0.0f, 1.5f, 0.0f};
        drone.vel = {0.0f, randomFloat(0.5f, 1.5f), 0.0f}; // vel.y = shoot timer
        drone.hp = 2 + sector / 4;
        drone.kind = 0;

        pending.push_back(drone);
    }

    showBanner("WARNING", "Drones incoming");
    banner.timer = 1.5f;
}

void VectorRush::spawnSweeper()
{
    // A long horizontal bar sweeping up and down
    Entity& bar = addBox({0.0f, 4.0f, SPAWN_Z}, {15.0f, 0.35f, 0.35f});
    bar.amplitude = {0.0f, 3.0f, 0.0f};
    bar.frequency = randomFloat(1.0f, 1.6f);
    bar.phase = randomFloat(0.0f, 6.0f);
    bar.kind = 2;

    // Posts at both ends
    addBox({-15.5f, 4.5f, SPAWN_Z}, {0.4f, 4.5f, 0.4f});
    addBox({15.5f, 4.5f, SPAWN_Z}, {0.4f, 4.5f, 0.4f});
}

void VectorRush::spawnBoss()
{
    bossActive = true;
    bossLevel = sector / 3;

    Entity boss;
    boss.type = EntityType::Boss;
    boss.pos = {0.0f, 4.0f, SPAWN_Z};
    boss.spin = {0.4f, 0.7f, 0.2f};
    boss.hp = 40 + (bossLevel - 1) * 25;
    boss.kind = boss.hp; // Max hp for the health bar
    boss.vel = {3.0f, 6.0f, 0.0f};

    pending.push_back(boss);

    showBanner("GUARDIAN MK " + std::to_string(bossLevel), "Destroy the core!");
}

void VectorRush::spawnSparks(const Vec3& pos, int count, float sparkSpeed)
{
    for (int i = 0; i < count; ++i)
    {
        Vec3 dir = {randomFloat(-1, 1), randomFloat(-1, 1), randomFloat(-1, 1)};
        float len = std::max(dir.length(), 0.01f);
        float life = randomFloat(0.3f, 1.0f);

        sparks.push_back({pos, dir * (sparkSpeed * randomFloat(0.3f, 1.0f) / len), life, life});
    }
}

void VectorRush::fireEnemyBullet(const Vec3& from, const Vec3& velocity)
{
    Entity bullet;
    bullet.type = EntityType::Bullet;
    bullet.pos = from;
    bullet.base = from;
    bullet.vel = velocity;
    bullet.spin = {3.0f, 4.0f, 0.0f};

    pending.push_back(bullet);
}

// ---------------------------------------------------------------------------
// Collision helpers
// ---------------------------------------------------------------------------

bool VectorRush::pointInBox(const Vec3& point, const Entity& box) const
{
    // Shots move fast, so give the box some extra depth
    return std::fabs(point.x - box.pos.x) < box.half.x &&
           std::fabs(point.y - box.pos.y) < box.half.y &&
           std::fabs(point.z - box.pos.z) < box.half.z + 1.5f;
}

// ---------------------------------------------------------------------------
// 3D rendering
// ---------------------------------------------------------------------------

VectorRush::Vec3 VectorRush::rotate(const Vec3& p, const Vec3& r)
{
    // Roll (z), then pitch (x), then yaw (y)
    float cz = std::cos(r.z), sz = std::sin(r.z);
    float cx = std::cos(r.x), sx = std::sin(r.x);
    float cy = std::cos(r.y), sy = std::sin(r.y);

    float x1 = p.x * cz - p.y * sz;
    float y1 = p.x * sz + p.y * cz;
    float z1 = p.z;

    float y2 = y1 * cx - z1 * sx;
    float z2 = y1 * sx + z1 * cx;

    float x3 = x1 * cy + z2 * sy;
    float z3 = -x1 * sy + z2 * cy;

    return {x3, y2, z3};
}

VectorRush::Vec3 VectorRush::toCamera(const Vec3& world) const
{
    Vec3 d = world - camera.pos;

    // Camera looks slightly down
    float cp = std::cos(camera.pitch), sp = std::sin(camera.pitch);
    float y1 = d.y * cp + d.z * sp;
    float z1 = -d.y * sp + d.z * cp;

    // Camera roll
    float cr = std::cos(camera.roll), sr = std::sin(camera.roll);
    float x2 = d.x * cr + y1 * sr;
    float y2 = -d.x * sr + y1 * cr;

    return {x2, y2, z1};
}

bool VectorRush::project(const Vec3& world, SDL_FPoint& screen) const
{
    Vec3 c = toCamera(world);

    if (c.z < NEAR_PLANE) return false;

    screen.x = SCREEN_CX + c.x / c.z * FOCAL;
    screen.y = SCREEN_CY - c.y / c.z * FOCAL;
    return true;
}

void VectorRush::line3D(const Vec3& a, const Vec3& b, float alpha, bool depthFade)
{
    Vec3 ca = toCamera(a);
    Vec3 cb = toCamera(b);

    // Near plane clipping
    if (ca.z < NEAR_PLANE && cb.z < NEAR_PLANE) return;

    if (ca.z < NEAR_PLANE)
    {
        float t = (NEAR_PLANE - ca.z) / (cb.z - ca.z);
        ca = ca + (cb - ca) * t;
    }
    else if (cb.z < NEAR_PLANE)
    {
        float t = (NEAR_PLANE - cb.z) / (ca.z - cb.z);
        cb = cb + (ca - cb) * t;
    }

    if (depthFade)
    {
        float depth = (ca.z + cb.z) * 0.5f;
        alpha *= clampf(1.0f - (depth - FADE_START) / (FAR_PLANE - FADE_START), 0.0f, 1.0f);
    }

    if (alpha <= 0.01f) return;

    float sx = shake > 0.0f ? randomFloat(-shake, shake) * 0.3f : 0.0f;

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, static_cast<Uint8>(clampf(alpha, 0.0f, 1.0f) * 255));
    SDL_RenderLine(renderer,
        SCREEN_CX + ca.x / ca.z * FOCAL + sx, SCREEN_CY - ca.y / ca.z * FOCAL,
        SCREEN_CX + cb.x / cb.z * FOCAL + sx, SCREEN_CY - cb.y / cb.z * FOCAL);
}

void VectorRush::drawMesh(const Mesh& mesh, const Vec3& pos, const Vec3& scale, const Vec3& rot, float alpha)
{
    std::vector<Vec3> world;
    world.reserve(mesh.vertices.size());

    for (const Vec3& v : mesh.vertices)
    {
        Vec3 scaled = {v.x * scale.x, v.y * scale.y, v.z * scale.z};
        world.push_back(rotate(scaled, rot) + pos);
    }

    for (const auto& edge : mesh.edges)
    {
        line3D(world[edge.first], world[edge.second], alpha);
    }
}

void VectorRush::drawLabel(const std::string& text, const Vec3& pos, float size)
{
    SDL_FPoint screen;
    if (!project(pos, screen)) return;

    float depth = toCamera(pos).z;
    float alpha = clampf(1.0f - (depth - FADE_START) / (FAR_PLANE - FADE_START), 0.0f, 1.0f);

    SDL_Color color = {255, 255, 255, static_cast<Uint8>(alpha * 255)};
    renderTextCentered(renderer, smallFont, text, screen.x, screen.y - 10.0f, color, size);
}

void VectorRush::renderBackground()
{
    // Stars in the sky
    for (const SDL_FPoint& star : skyStars)
    {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 90);
        SDL_RenderPoint(renderer, star.x, star.y);
    }

    // Striped sun on the horizon
    SDL_FPoint sunCenter;
    Vec3 sunPos = {camera.pos.x * 0.9f, 20.0f, FAR_PLANE};

    if (project(sunPos, sunCenter))
    {
        float radius = 24.0f / toCamera(sunPos).z * FOCAL;
        SDL_FPoint horizon;
        project({camera.pos.x, 0.0f, FAR_PLANE * 4.0f}, horizon);

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 140);
        drawArc(renderer, sunCenter, radius, SDL_PI_F, 2.0f * SDL_PI_F, 48);

        // Horizontal stripes, getting thinner towards the bottom
        float y = sunCenter.y - radius * 0.9f;
        float gap = 3.0f;
        while (y < sunCenter.y + radius && y < horizon.y)
        {
            float dy = y - sunCenter.y;
            float half = std::sqrt(std::max(0.0f, radius * radius - dy * dy));
            SDL_RenderLine(renderer, sunCenter.x - half, y, sunCenter.x + half, y);

            y += gap;
            if (y > sunCenter.y) gap += 1.5f;
        }
    }

    // Mountains (slow parallax: they move a bit with the camera)
    for (size_t i = 1; i < mountains.size(); ++i)
    {
        Vec3 a = mountains[i - 1];
        Vec3 b = mountains[i];
        a.x += camera.pos.x * 0.8f;
        b.x += camera.pos.x * 0.8f;
        line3D(a, b, 0.55f, false);

        // Some peaks get a vertical ridge line
        if (i % 3 == 0 && b.y > 8.0f)
        {
            line3D(b, {b.x + 3.0f, 0.0f, b.z}, 0.25f, false);
        }
    }
}

void VectorRush::renderGrid()
{
    const float spacing = 6.0f;
    const float zStart = camera.pos.z + 1.0f;

    // Lines along the flight direction, split into segments for the depth fade
    for (float x = -42.0f; x <= 42.0f; x += 4.0f)
    {
        float alpha = std::fabs(x) <= 10.0f ? 0.5f : 0.3f;

        for (float z = zStart; z < FAR_PLANE; z += 16.0f)
        {
            line3D({x, 0.0f, z}, {x, 0.0f, std::min(z + 16.0f, FAR_PLANE)}, alpha);
        }
    }

    // Cross lines scroll towards the camera
    float offset = std::fmod(scroll, spacing);

    for (float z = -spacing - offset + spacing; z < FAR_PLANE; z += spacing)
    {
        if (z < zStart) continue;
        line3D({-42.0f, 0.0f, z}, {42.0f, 0.0f, z}, 0.4f);
    }

    // Side pylons mark the edge of the flight area
    const float pylonSpacing = 24.0f;
    float pylonOffset = std::fmod(scroll, pylonSpacing);

    for (float z = pylonSpacing - pylonOffset; z < FAR_PLANE; z += pylonSpacing)
    {
        for (float side : {-1.0f, 1.0f})
        {
            float x = side * (AREA_X + 2.5f);
            line3D({x, 0.0f, z}, {x, 8.0f, z}, 0.7f);
            line3D({x, 8.0f, z}, {x - side * 1.0f, 8.0f, z}, 0.7f);
            line3D({x, 0.0f, z - 1.0f}, {x, 0.0f, z + 1.0f}, 0.7f);
        }
    }

    // Speed streaks while boosting
    if (boosting && state == State::Playing)
    {
        for (int i = 0; i < 12; ++i)
        {
            float x = randomFloat(-14.0f, 14.0f);
            float y = randomFloat(0.0f, 10.0f);
            float z = randomFloat(0.0f, 40.0f);
            line3D({x, y, z}, {x, y, z + 6.0f}, 0.35f);
        }
    }
}

void VectorRush::renderEntities()
{
    for (const Entity& e : entities)
    {
        if (!e.alive) continue;

        float alpha = e.flash > 0.0f ? 0.4f : 1.0f;

        switch (e.type)
        {
            case EntityType::Box:
            {
                drawMesh(cubeMesh, e.pos, e.half, {0, 0, 0}, alpha);

                // Wall pieces get cross bracing on the front face
                if (e.kind == 1)
                {
                    Vec3 h = e.half;
                    float z = e.pos.z - h.z;
                    line3D({e.pos.x - h.x, e.pos.y - h.y, z}, {e.pos.x + h.x, e.pos.y + h.y, z}, 0.35f);
                    line3D({e.pos.x - h.x, e.pos.y + h.y, z}, {e.pos.x + h.x, e.pos.y - h.y, z}, 0.35f);
                }

                // Pillars: horizontal bands
                if (e.kind == 0 && e.half.y > 1.5f)
                {
                    for (float y = e.pos.y - e.half.y + 1.5f; y < e.pos.y + e.half.y; y += 1.5f)
                    {
                        Vec3 h = e.half;
                        line3D({e.pos.x - h.x, y, e.pos.z - h.z}, {e.pos.x + h.x, y, e.pos.z - h.z}, 0.4f);
                    }
                }
                break;
            }

            case EntityType::Ring:
            {
                float ringAlpha = e.passed ? 0.3f : 1.0f;
                drawMesh(ringMesh, e.pos, {e.half.x, e.half.x, 1.0f}, e.rot, ringAlpha);
                break;
            }

            case EntityType::Crystal:
            {
                drawMesh(octaMesh, e.pos, {0.8f, 1.2f, 0.8f}, e.rot, 1.0f);
                drawMesh(octaMesh, e.pos, {0.4f, 0.6f, 0.4f}, e.rot * -1.5f, 0.6f);
                drawLabel(CRYSTAL_LABELS[e.kind], e.pos + Vec3{0.0f, 2.0f, 0.0f});
                break;
            }

            case EntityType::Mine:
            {
                drawMesh(mineMesh, e.pos, {1.0f, 1.0f, 1.0f}, e.rot, alpha);

                // Homing mines blink
                if (e.kind == 1 && std::fmod(time, 0.4f) < 0.2f)
                {
                    drawMesh(octaMesh, e.pos, {0.3f, 0.3f, 0.3f}, e.rot, 1.0f);
                }
                break;
            }

            case EntityType::Drone:
            {
                drawMesh(droneMesh, e.pos, {1.0f, 1.0f, 1.0f}, {0.0f, e.rot.y, std::sin(time * 3.0f) * 0.3f}, alpha);

                // Charging eye before shooting
                if (e.kind == 1 && e.vel.y < 0.4f)
                {
                    drawMesh(octaMesh, e.pos + Vec3{0, 0, -1.3f}, {0.3f, 0.3f, 0.3f}, {0, time * 8.0f, 0}, 1.0f);
                }
                break;
            }

            case EntityType::Bullet:
            {
                drawMesh(octaMesh, e.pos, {0.35f, 0.35f, 0.35f}, e.rot, 1.0f);
                break;
            }

            case EntityType::Boss:
            {
                float pulse = 1.0f + std::sin(time * 4.0f) * 0.05f;

                drawMesh(icoMesh, e.pos, {4.0f * pulse, 4.0f * pulse, 4.0f * pulse}, e.rot, alpha);
                drawMesh(icoMesh, e.pos, {1.8f, 1.8f, 1.8f}, e.rot * -2.0f, 1.0f);

                // Orbiting armor plates
                for (int i = 0; i < 6; ++i)
                {
                    float a = time * 1.5f + i * SDL_PI_F / 3.0f;
                    Vec3 offset = {std::cos(a) * 7.0f, std::sin(a * 2.0f) * 1.5f, std::sin(a) * 7.0f};
                    drawMesh(cubeMesh, e.pos + offset, {0.7f, 0.7f, 0.7f}, {a, a, 0}, 0.8f);
                }

                // Orbit ring
                drawMesh(ringMesh, e.pos, {7.0f, 7.0f, 1.0f}, {SDL_PI_F / 2.0f, 0.0f, time}, 0.25f);
                break;
            }
        }
    }

    // Player shots
    for (const Shot& shot : shots)
    {
        line3D(shot.pos, shot.pos - Vec3{shot.vel.x, shot.vel.y, shot.vel.z} * 0.03f, 1.0f);
    }

    // Sparks
    for (const Spark& spark : sparks)
    {
        float t = spark.life / spark.maxLife;
        line3D(spark.pos, spark.pos - spark.vel * 0.04f, t);
    }

    // Floating score texts
    for (const FloatingText& text : floatingTexts)
    {
        drawLabel(text.text, text.pos, 0.7f);
    }
}

void VectorRush::renderShip()
{
    if (state == State::GameOver) return;

    // Blink while invulnerable
    if (ship.invulnerable > 0.0f && std::fmod(ship.invulnerable, 0.16f) < 0.08f) return;

    Vec3 rot = {ship.pitch, 0.0f, ship.roll};
    drawMesh(shipMesh, ship.pos, {1.0f, 1.0f, 1.0f}, rot, 1.0f);

    // Engine flames
    float flame = (boosting ? 2.2f : 1.0f) * randomFloat(0.7f, 1.2f);

    for (float side : {-0.3f, 0.3f})
    {
        Vec3 nozzle = rotate({side, 0.0f, -0.8f}, rot) + ship.pos;
        Vec3 tip = rotate({side, 0.0f, -0.8f - flame}, rot) + ship.pos;
        line3D(nozzle, tip, 0.8f, false);
    }

    // Shadow on the floor
    float shadowAlpha = clampf(0.5f - ship.pos.y * 0.05f, 0.1f, 0.5f);
    line3D({ship.pos.x - 1.2f, 0.0f, ship.pos.z - 0.5f}, {ship.pos.x + 1.2f, 0.0f, ship.pos.z - 0.5f}, shadowAlpha);
    line3D({ship.pos.x, 0.0f, ship.pos.z - 0.5f}, {ship.pos.x, 0.0f, ship.pos.z + 1.5f}, shadowAlpha);

    // Aiming reticle in front of the ship
    SDL_FPoint reticle;
    if (project(ship.pos + Vec3{0.0f, 0.0f, 30.0f}, reticle))
    {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 140);
        SDL_FRect box = {reticle.x - 6.0f, reticle.y - 6.0f, 12.0f, 12.0f};
        SDL_RenderRect(renderer, &box);
        SDL_RenderLine(renderer, reticle.x - 12.0f, reticle.y, reticle.x - 8.0f, reticle.y);
        SDL_RenderLine(renderer, reticle.x + 8.0f, reticle.y, reticle.x + 12.0f, reticle.y);
    }
}

void VectorRush::renderHud()
{
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color gray = {255, 255, 255, 130};

    // Score
    renderText(renderer, smallFont, ("SCORE " + std::to_string(static_cast<int>(score))).c_str(), 15, 12, white, 0.9f);
    renderText(renderer, smallFont, ("HI " + std::to_string(std::max(highscore, static_cast<int>(score)))).c_str(), 15, 38, gray, 0.7f);

    // Sector progress
    renderTextCentered(renderer, smallFont, "SECTOR " + std::to_string(sector), SCREEN_WIDTH / 2.0f, 10, white, 0.8f);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_FRect progressFrame = {300, 36, 200, 6};
    SDL_RenderRect(renderer, &progressFrame);

    float progress = bossActive ? 1.0f : clampf(sectorDistance / sectorLength, 0.0f, 1.0f);
    SDL_FRect progressFill = {300, 36, 200 * progress, 6};
    SDL_RenderFillRect(renderer, &progressFill);

    // Boss health
    for (const Entity& e : entities)
    {
        if (e.type != EntityType::Boss || !e.alive) continue;

        renderTextCentered(renderer, smallFont, "GUARDIAN MK " + std::to_string(bossLevel), SCREEN_WIDTH / 2.0f, 50, white, 0.7f);

        SDL_FRect frame = {200, 74, 400, 10};
        SDL_FRect fill = {200, 74, 400.0f * std::max(0, e.hp) / std::max(1, e.kind), 10};
        SDL_RenderRect(renderer, &frame);
        SDL_RenderFillRect(renderer, &fill);
    }

    // Shield pips (diamonds)
    renderText(renderer, smallFont, "SHIELD", SCREEN_WIDTH - 190, 12, gray, 0.7f);
    for (int i = 0; i < SHIP_MAX_SHIELD; ++i)
    {
        float cx = SCREEN_WIDTH - 100.0f + i * 18.0f;
        float cy = 22.0f;

        SDL_FPoint diamond[] = {{cx, cy - 7}, {cx + 6, cy}, {cx, cy + 7}, {cx - 6, cy}, {cx, cy - 7}};

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, i < ship.shield ? 255 : 50);
        SDL_RenderLines(renderer, diamond, 5);

        if (i < ship.shield)
        {
            SDL_RenderLine(renderer, cx - 3, cy, cx + 3, cy);
            SDL_RenderLine(renderer, cx, cy - 4, cx, cy + 4);
        }
    }

    // Energy bar
    renderText(renderer, smallFont, "BOOST", 15, SCREEN_HEIGHT - 32, boosting ? white : gray, 0.7f);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_FRect energyFrame = {80, SCREEN_HEIGHT - 26.0f, 150, 10};
    SDL_FRect energyFill = {80, SCREEN_HEIGHT - 26.0f, 150 * ship.energy / 100.0f, 10};
    SDL_RenderRect(renderer, &energyFrame);
    SDL_RenderFillRect(renderer, &energyFill);

    // Speed
    std::string speedText = std::to_string(static_cast<int>(speed * 10.0f)) + " km/h";
    renderText(renderer, smallFont, speedText.c_str(), SCREEN_WIDTH - 15 - textWidth(speedText, smallFont, 0.7f), SCREEN_HEIGHT - 32, gray, 0.7f);

    // Power ups
    float y = SCREEN_HEIGHT - 60.0f;
    auto powerBar = [&](const char* name, float value, float max) {
        if (value <= 0.0f) return;

        renderText(renderer, smallFont, name, 15, y, white, 0.6f);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_FRect fill = {120, y + 6, 80.0f * value / max, 5};
        SDL_RenderFillRect(renderer, &fill);
        y -= 20.0f;
    };

    powerBar("RAPID FIRE", rapidTimer, POWERUP_TIME);
    powerBar("WIDE SHOT", spreadTimer, POWERUP_TIME);
    powerBar("SLOW MOTION", slowTimer, POWERUP_TIME * 0.6f);

    // Ring combo
    if (ringCombo >= 2)
    {
        renderTextCentered(renderer, smallFont, "RING COMBO x" + std::to_string(std::min(ringCombo, 10)), SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT - 34, white, 0.7f);
    }

    // Banner
    if (banner.timer > 0.0f)
    {
        SDL_Color color = {255, 255, 255, static_cast<Uint8>(clampf(banner.timer, 0.0f, 1.0f) * 255)};
        renderTextCentered(renderer, font, banner.title, SCREEN_WIDTH / 2.0f, 130, color, 0.8f);

        if (!banner.subtitle.empty())
        {
            renderTextCentered(renderer, smallFont, banner.subtitle, SCREEN_WIDTH / 2.0f, 180, color, 0.8f);
        }
    }

    // Damage flash: white frame
    if (damageFlash > 0.0f)
    {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, static_cast<Uint8>(damageFlash * 200));
        for (int i = 0; i < 4; ++i)
        {
            SDL_FRect frame = {static_cast<float>(i * 3), static_cast<float>(i * 3), SCREEN_WIDTH - i * 6.0f, SCREEN_HEIGHT - i * 6.0f};
            SDL_RenderRect(renderer, &frame);
        }
    }
}

void VectorRush::renderTitle()
{
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color gray = {255, 255, 255, 130};

    // Rotating showcase objects
    Vec3 center = {camera.pos.x, 5.5f, 22.0f};
    drawMesh(icoMesh, center, {3.0f, 3.0f, 3.0f}, {time * 0.4f, time * 0.7f, 0.0f}, 1.0f);
    drawMesh(ringMesh, center, {5.0f, 5.0f, 1.0f}, {0.3f, time * 0.5f, time}, 0.5f);
    drawMesh(shipMesh, {camera.pos.x, 2.5f + std::sin(time * 2.0f) * 0.3f, 4.0f}, {1, 1, 1}, {0.0f, 0.0f, std::sin(time) * 0.3f}, 1.0f);

    renderTextCentered(renderer, font, "VECTOR RUSH 3D", SCREEN_WIDTH / 2.0f, 40, white, 1.1f);
    renderTextCentered(renderer, smallFont, "Highscore: " + std::to_string(highscore), SCREEN_WIDTH / 2.0f, 105, gray, 0.8f);

    float y = 400.0f;
    renderTextCentered(renderer, smallFont, "WASD / Arrows = Fly    SPACE = Shoot    SHIFT = Boost", SCREEN_WIDTH / 2.0f, y, white, 0.75f);
    renderTextCentered(renderer, smallFont, "Fly through rings for points and boost energy. Avoid walls and mines.", SCREEN_WIDTH / 2.0f, y + 28, gray, 0.7f);
    renderTextCentered(renderer, smallFont, "Crystals:  S Shield   R Rapid fire   W Wide shot   T Slow motion", SCREEN_WIDTH / 2.0f, y + 52, gray, 0.7f);
    renderTextCentered(renderer, smallFont, "Every third sector a Guardian is waiting...", SCREEN_WIDTH / 2.0f, y + 76, gray, 0.7f);

    if (std::fmod(time, 1.0f) < 0.6f)
    {
        renderTextCentered(renderer, smallFont, "PRESS ENTER", SCREEN_WIDTH / 2.0f, 545, white);
    }
}

void VectorRush::renderOverlay(const std::string& title, const std::vector<std::string>& lines)
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 170);
    SDL_FRect overlay = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_RenderFillRect(renderer, &overlay);

    float height = 90.0f + lines.size() * 30.0f;
    SDL_FRect box = {150, 300 - height / 2.0f, 500, height};
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderRect(renderer, &box);

    SDL_Color white = {255, 255, 255, 255};
    SDL_Color gray = {255, 255, 255, 150};

    renderTextCentered(renderer, font, title, SCREEN_WIDTH / 2.0f, box.y + 15, white, 0.75f);

    for (size_t i = 0; i < lines.size(); ++i)
    {
        bool last = i + 1 == lines.size();
        renderTextCentered(renderer, smallFont, lines[i], SCREEN_WIDTH / 2.0f, box.y + 75 + i * 30.0f, last ? gray : white, 0.8f);
    }
}

void VectorRush::render()
{
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    renderBackground();
    renderGrid();

    if (state == State::Title)
    {
        renderTitle();
    }
    else
    {
        renderEntities();
        renderShip();
        renderHud();

        if (state == State::Paused)
        {
            renderOverlay("PAUSED", {"P = Continue", "ESC = Title screen"});
        }

        if (state == State::GameOver)
        {
            renderOverlay("GAME OVER", {
                "Score: " + std::to_string(static_cast<int>(score)) + (newHighscore ? "   NEW HIGHSCORE!" : ""),
                "Sector " + std::to_string(sector) + "   Distance " + std::to_string(static_cast<int>(totalDistance)) + " m",
                "Rings " + std::to_string(ringsCollected) + "   Enemies " + std::to_string(enemiesDestroyed),
                "ENTER = Retry   ESC = Title"
            });
        }
    }

    SDL_RenderPresent(renderer);
}
