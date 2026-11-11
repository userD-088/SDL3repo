#include "SpaceDefenders.h"
#include "const.h"

#include <algorithm>
#include <array>
#include <vector>
#include <iostream>
#include <SDL3/SDL.h>

namespace {
    constexpr float PLAYER_MAX_SPEED = 320.0f;
    constexpr float PLAYER_RADIUS = 14.0f;
    constexpr float FIRE_COOLDOWN = 0.25f;
    constexpr float RAPID_COOLDOWN = 0.09f;
    constexpr float LASER_LIFETIME = 1.8f;
    constexpr float INVULNERABLE_TIME = 2.0f;

    float distanceSquared(const SDL_FPoint& a, const SDL_FPoint& b)
    {
        float dx = a.x - b.x;
        float dy = a.y - b.y;
        return dx * dx + dy * dy;
    }
}

SDL_FPoint SpaceDefenders::Moon::position() const
{
    return {
        SCREEN_WIDTH / 2.0f + std::cos(angle) * orbitRadius,
        SCREEN_HEIGHT / 2.0f + std::sin(angle) * orbitRadius
    };
}

SpaceDefenders::SpaceDefenders(SDL_Window* window, SDL_Renderer* renderer) {
    this->window = window;
    this->renderer = renderer;

    font = openFont(48);
    smallFont = openFont(24);

    highscore = loadHighscore("spacedefenders");

    for (int i = 0; i < 140; ++i)
    {
        stars.push_back({
            {randomFloat(0, SCREEN_WIDTH), randomFloat(0, SCREEN_HEIGHT)},
            randomFloat(0.15f, 0.7f),
            randomFloat(0.0f, 2.0f * SDL_PI_F)
        });
    }
}

SpaceDefenders::~SpaceDefenders() {
    if (font) TTF_CloseFont(font);
    if (smallFont) TTF_CloseFont(smallFont);
}

void SpaceDefenders::run() {
    while(running) {
        dt = std::min(timer.deltaTime(), 0.05f);

        handleEvents();
        update();
        render();
        
        SDL_Delay(16);
    }
}

void SpaceDefenders::handleEvents() {
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

        if (key == SDLK_ESCAPE)
        {
            if (state == State::Playing || state == State::Paused) state = State::Title;
            else running = false;
            continue;
        }

        switch (state)
        {
            case State::Title:
                if (key == SDLK_RETURN || key == SDLK_SPACE) resetGame();
                break;

            case State::Playing:
                if (key == SDLK_P) state = State::Paused;
                if (key == SDLK_T) showTrajectory = !showTrajectory;
                break;

            case State::Paused:
                if (key == SDLK_P || key == SDLK_RETURN) state = State::Playing;
                break;

            case State::GameOver:
                if (key == SDLK_RETURN || key == SDLK_R) resetGame();
                break;
        }
    }
}

// ---------------------------------------------------------------------------
// Game flow
// ---------------------------------------------------------------------------

void SpaceDefenders::resetGame()
{
    player.polygon.pos = {100.0f, 100.0f};
    player.velocity = {0.0f, 0.0f};
    player.accelaration = {0.0f, 0.0f};
    player.direction = 0.0f;
    player.hp = 3;
    player.invulnerable = INVULNERABLE_TIME;

    lasers.clear();
    asteroids.clear();
    ufos.clear();
    bullets.clear();
    pickups.clear();
    particles.clear();
    floatingTexts.clear();

    earthHp = EARTH_MAX_HP;
    score = 0;
    wave = 0;
    combo = 0;
    rapidFireTimer = 0.0f;
    tripleShotTimer = 0.0f;
    newHighscore = false;

    state = State::Playing;

    startWave();
}

void SpaceDefenders::startWave()
{
    wave++;

    asteroidsToSpawn = 3 + wave * 2;
    ufosToSpawn = wave >= 3 ? (wave - 1) / 2 : 0;

    spawnTimer = 2.5f;
    waveBannerTimer = 2.5f;
}

void SpaceDefenders::updateWave()
{
    if (waveBannerTimer > 0.0f) waveBannerTimer -= dt;

    spawnTimer -= dt;

    if (spawnTimer <= 0.0f && (asteroidsToSpawn > 0 || ufosToSpawn > 0))
    {
        // UFOs appear in the second half of a wave
        bool spawnUfoNow = ufosToSpawn > 0 && (asteroidsToSpawn == 0 || randomInt(0, 4) == 0);

        if (spawnUfoNow)
        {
            spawnUfo();
            ufosToSpawn--;
        }
        else
        {
            spawnAsteroid();
            asteroidsToSpawn--;
        }

        spawnTimer = std::max(0.6f, 2.6f - wave * 0.18f);
    }

    // Wave cleared?
    if (asteroidsToSpawn == 0 && ufosToSpawn == 0 && asteroids.empty() && ufos.empty())
    {
        int bonus = earthHp * 25 * wave;
        score += bonus;

        addFloatingText("WAVE CLEAR  +" + std::to_string(bonus), {SCREEN_WIDTH / 2.0f, 150.0f});

        startWave();
    }
}

void SpaceDefenders::damagePlayer()
{
    if (player.invulnerable > 0.0f) return;

    player.hp--;
    player.invulnerable = INVULNERABLE_TIME;
    shake = 8.0f;
    combo = 0;

    spawnExplosion(player.polygon.pos, 25, 160.0f);

    if (player.hp <= 0)
    {
        gameOver();
    }
}

void SpaceDefenders::damageEarth(int amount)
{
    earthHp -= amount;
    earthFlash = 1.0f;
    shake = 10.0f;

    if (earthHp <= 0)
    {
        earthHp = 0;
        spawnExplosion(earth.position, 80, 260.0f);
        gameOver();
    }
}

void SpaceDefenders::gameOver()
{
    state = State::GameOver;

    if (score > highscore)
    {
        highscore = score;
        newHighscore = true;
        saveHighscore("spacedefenders", highscore);
    }
}

// ---------------------------------------------------------------------------
// Spawning
// ---------------------------------------------------------------------------

SpaceDefenders::Asteroid SpaceDefenders::createAsteroid(SDL_FPoint position, SDL_FPoint velocity, int size)
{
    Asteroid asteroid;

    const float radii[] = {0.0f, 11.0f, 21.0f, 34.0f};

    asteroid.size = size;
    asteroid.radius = radii[size];
    asteroid.position = position;
    asteroid.velocity = velocity;
    asteroid.acceleration = {0.0f, 0.0f};
    asteroid.hp = size;
    asteroid.damage = size;
    asteroid.rotation = randomFloat(0.0f, 2.0f * SDL_PI_F);
    asteroid.rotationSpeed = randomFloat(-1.5f, 1.5f);

    // Jagged outline
    int corners = randomInt(8, 12);

    for (int i = 0; i < corners; ++i)
    {
        float angle = i * 2.0f * SDL_PI_F / corners;
        float r = asteroid.radius * randomFloat(0.7f, 1.15f);

        asteroid.Body.push_back({std::cos(angle) * r, std::sin(angle) * r});
    }

    return asteroid;
}

void SpaceDefenders::spawnAsteroid()
{
    // Spawn somewhere just outside the screen
    SDL_FPoint position;
    int side = randomInt(0, 3);

    if (side == 0) position = {randomFloat(0, SCREEN_WIDTH), -40.0f};
    else if (side == 1) position = {randomFloat(0, SCREEN_WIDTH), SCREEN_HEIGHT + 40.0f};
    else if (side == 2) position = {-40.0f, randomFloat(0, SCREEN_HEIGHT)};
    else position = {SCREEN_WIDTH + 40.0f, randomFloat(0, SCREEN_HEIGHT)};

    // Aim roughly at earth, but not perfectly
    float angle = std::atan2(earth.position.y - position.y, earth.position.x - position.x) + randomFloat(-0.45f, 0.45f);
    float speed = randomFloat(25.0f, 50.0f) + wave * 4.0f;

    int size = wave >= 2 && randomInt(0, 3) == 0 ? 2 : 3;

    asteroids.push_back(createAsteroid(position, {std::cos(angle) * speed, std::sin(angle) * speed}, size));
}

void SpaceDefenders::splitAsteroid(const Asteroid& asteroid)
{
    if (asteroid.size <= 1) return;

    for (int i = 0; i < 2; ++i)
    {
        float angle = std::atan2(asteroid.velocity.y, asteroid.velocity.x) + (i == 0 ? 0.7f : -0.7f) + randomFloat(-0.2f, 0.2f);
        float speed = std::sqrt(distanceSquared(asteroid.velocity, {0, 0})) * 1.3f + 20.0f;

        asteroids.push_back(createAsteroid(asteroid.position, {std::cos(angle) * speed, std::sin(angle) * speed}, asteroid.size - 1));
    }
}

void SpaceDefenders::spawnUfo()
{
    bool fromLeft = randomInt(0, 1) == 0;

    Ufo ufo;
    ufo.position = {fromLeft ? -40.0f : SCREEN_WIDTH + 40.0f, randomFloat(80.0f, SCREEN_HEIGHT - 80.0f)};
    ufo.baseY = ufo.position.y;
    ufo.speedX = (fromLeft ? 1.0f : -1.0f) * (70.0f + wave * 4.0f);
    ufo.phase = randomFloat(0.0f, 6.0f);
    ufo.shootTimer = 1.5f;
    ufo.hp = 3;

    ufos.push_back(ufo);
}

void SpaceDefenders::spawnExplosion(SDL_FPoint position, int count, float speed)
{
    for (int i = 0; i < count; ++i)
    {
        float angle = randomFloat(0.0f, 2.0f * SDL_PI_F);
        float s = randomFloat(speed * 0.2f, speed);
        float life = randomFloat(0.4f, 1.1f);

        particles.push_back({position, {std::cos(angle) * s, std::sin(angle) * s}, randomFloat(2.0f, 7.0f), life, life});
    }
}

void SpaceDefenders::spawnPickup(SDL_FPoint position)
{
    PickupType types[] = {PickupType::Heart, PickupType::Rapid, PickupType::Triple, PickupType::Repair};

    float angle = randomFloat(0.0f, 2.0f * SDL_PI_F);

    pickups.push_back({position, {std::cos(angle) * 15.0f, std::sin(angle) * 15.0f}, types[randomInt(0, 3)], 10.0f});
}

void SpaceDefenders::addFloatingText(const std::string& text, SDL_FPoint position)
{
    floatingTexts.push_back({text, position, 1.2f});
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

void SpaceDefenders::update()
{
    time += dt;

    for (Star& star : stars) star.twinkle += dt * 2.0f;

    updateEffects();

    if (state != State::Playing)
    {
        return;
    }

    moon.angle += moon.angularSpeed * dt;

    updatePlayer();
    updateLasers();
    updateAsteroids();
    updateUfos();
    updatePickups();

    if (state == State::Playing) updateWave();
}

void SpaceDefenders::updatePlayer()
{
    if (player.invulnerable > 0.0f) player.invulnerable -= dt;
    if (player.fireCooldown > 0.0f) player.fireCooldown -= dt;
    if (rapidFireTimer > 0.0f) rapidFireTimer -= dt;
    if (tripleShotTimer > 0.0f) tripleShotTimer -= dt;

    // Check if inside of the Planet
    if (isPlayerPlanetIntersect(player, earth)) {
        bool wasVulnerable = player.invulnerable <= 0.0f;

        player.polygon.pos = {100.0f, 100.0f};
        player.velocity = {0.0f, 0.0f};
        player.accelaration = {0.0f, 0.0f};

        if (wasVulnerable) damagePlayer();
        else player.invulnerable = INVULNERABLE_TIME;

        if (state != State::Playing) return;
    }

    // Moon collision
    if (isPlayerCircleIntersect(moon.position(), moon.radius))
    {
        damagePlayer();

        // Bounce off the moon
        SDL_FPoint m = moon.position();
        float angle = std::atan2(player.polygon.pos.y - m.y, player.polygon.pos.x - m.x);
        player.velocity = {std::cos(angle) * 150.0f, std::sin(angle) * 150.0f};
    }

    // Inputs
    const bool* keyboard = SDL_GetKeyboardState(nullptr);

    if (keyboard[SDL_SCANCODE_LEFT] || keyboard[SDL_SCANCODE_A])
    {
        player.direction -= player.rotationSpeed * dt;
    }

    if (keyboard[SDL_SCANCODE_RIGHT] || keyboard[SDL_SCANCODE_D])
    {
        player.direction += player.rotationSpeed * dt;
    }

    player.accelaration = {0.0f, 0.0f};
    player.thrusting = false;

    if (keyboard[SDL_SCANCODE_UP] || keyboard[SDL_SCANCODE_W])
    {
        player.accelaration.x = std::cos(player.direction) * player.thrust * 1.6f;
        player.accelaration.y = std::sin(player.direction) * player.thrust * 1.6f;
        player.thrusting = true;
    }

    SDL_FPoint gravity = calcGravPullEarth(player.polygon.pos, earth);

    player.velocity.x += (player.accelaration.x + gravity.x) * dt;
    player.velocity.y += (player.accelaration.y + gravity.y) * dt;

    // Speed limit
    float speed = std::sqrt(player.velocity.x * player.velocity.x + player.velocity.y * player.velocity.y);

    if (speed > PLAYER_MAX_SPEED)
    {
        player.velocity.x *= PLAYER_MAX_SPEED / speed;
        player.velocity.y *= PLAYER_MAX_SPEED / speed;
    }

    player.polygon.pos.x += player.velocity.x * dt;
    player.polygon.pos.y += player.velocity.y * dt;

    // Screen wrap
    if (player.polygon.pos.x < -20) player.polygon.pos.x += SCREEN_WIDTH + 40;
    if (player.polygon.pos.x > SCREEN_WIDTH + 20) player.polygon.pos.x -= SCREEN_WIDTH + 40;
    if (player.polygon.pos.y < -20) player.polygon.pos.y += SCREEN_HEIGHT + 40;
    if (player.polygon.pos.y > SCREEN_HEIGHT + 20) player.polygon.pos.y -= SCREEN_HEIGHT + 40;

    // Engine exhaust
    if (player.thrusting && randomInt(0, 1) == 0)
    {
        float back = player.direction + SDL_PI_F + randomFloat(-0.3f, 0.3f);
        particles.push_back({
            player.polygon.pos,
            {std::cos(back) * 120.0f + player.velocity.x, std::sin(back) * 120.0f + player.velocity.y},
            3.0f, 0.3f, 0.3f
        });
    }

    // Shooting
    if ((keyboard[SDL_SCANCODE_SPACE] || keyboard[SDL_SCANCODE_DOWN]) && player.fireCooldown <= 0.0f)
    {
        float noseX = player.polygon.pos.x + 36.0f * std::cos(player.direction);
        float noseY = player.polygon.pos.y + 36.0f * std::sin(player.direction);

        lasers.push_back({noseX, noseY, player.direction, 12.0f, 0.0f});

        if (tripleShotTimer > 0.0f)
        {
            lasers.push_back({noseX, noseY, player.direction - 0.2f, 12.0f, 0.0f});
            lasers.push_back({noseX, noseY, player.direction + 0.2f, 12.0f, 0.0f});
        }

        player.fireCooldown = rapidFireTimer > 0.0f ? RAPID_COOLDOWN : FIRE_COOLDOWN;
    }
}

void SpaceDefenders::updateLasers()
{
    for (auto it = lasers.begin(); it != lasers.end(); ) {
        it->x = it->x + laserSpeed * cosf(it->angle) * dt;
        it->y = it->y + laserSpeed * sinf(it->angle) * dt;

        float tipX = it->x + it->len * cosf(it->angle);
        float tipY = it->y + it->len * sinf(it->angle);
        SDL_FPoint tip = {tipX, tipY};

        it->lifetime += dt;
        
        bool eraseFlag = it->lifetime > LASER_LIFETIME;
        eraseFlag = eraseFlag || it->x < 0 || it->x > SCREEN_WIDTH || it->y < 0 || it->y > SCREEN_HEIGHT;
        eraseFlag = eraseFlag || distanceSquared(tip, earth.position) < earth.radius * earth.radius;
        eraseFlag = eraseFlag || distanceSquared(tip, moon.position()) < moon.radius * moon.radius;

        // Hit asteroid?
        if (!eraseFlag)
        {
            for (Asteroid& asteroid : asteroids)
            {
                if (distanceSquared(tip, asteroid.position) < asteroid.radius * asteroid.radius)
                {
                    asteroid.hp--;
                    asteroid.hitFlash = 0.15f;
                    eraseFlag = true;
                    spawnExplosion(tip, 4, 80.0f);
                    break;
                }
            }
        }

        // Hit UFO?
        if (!eraseFlag)
        {
            for (Ufo& ufo : ufos)
            {
                if (std::fabs(tip.x - ufo.position.x) < 24.0f && std::fabs(tip.y - ufo.position.y) < 12.0f)
                {
                    ufo.hp--;
                    eraseFlag = true;
                    spawnExplosion(tip, 6, 100.0f);
                    break;
                }
            }
        }

        if (eraseFlag) {
            it = lasers.erase(it);
        }
        else {
            ++it;
        }
    }
}

void SpaceDefenders::updateAsteroids()
{
    std::vector<Asteroid> destroyed;

    for (auto it = asteroids.begin(); it != asteroids.end(); )
    {
        Asteroid& asteroid = *it;

        // Asteroids feel the gravity too (a bit weaker, so they stay catchable)
        SDL_FPoint gravity = calcGravPullEarth(asteroid.position, earth);
        asteroid.acceleration = {gravity.x * 0.35f, gravity.y * 0.35f};

        asteroid.velocity.x += asteroid.acceleration.x * dt;
        asteroid.velocity.y += asteroid.acceleration.y * dt;
        asteroid.position.x += asteroid.velocity.x * dt;
        asteroid.position.y += asteroid.velocity.y * dt;
        asteroid.rotation += asteroid.rotationSpeed * dt;

        if (asteroid.hitFlash > 0.0f) asteroid.hitFlash -= dt;

        bool remove = false;

        // Impact on earth
        float earthDistance = earth.radius + asteroid.radius * 0.7f;
        if (distanceSquared(asteroid.position, earth.position) < earthDistance * earthDistance)
        {
            spawnExplosion(asteroid.position, 20 + asteroid.size * 10, 180.0f);
            addFloatingText("-" + std::to_string(asteroid.damage), asteroid.position);
            damageEarth(asteroid.damage);
            remove = true;
        }

        // The moon protects earth
        float moonDistance = moon.radius + asteroid.radius * 0.8f;
        if (!remove && distanceSquared(asteroid.position, moon.position()) < moonDistance * moonDistance)
        {
            spawnExplosion(asteroid.position, 15, 150.0f);
            remove = true;
        }

        // Player collision
        if (!remove && player.invulnerable <= 0.0f && isPlayerCircleIntersect(asteroid.position, asteroid.radius * 0.85f))
        {
            damagePlayer();
            asteroid.hp = 0;
        }

        // Destroyed by lasers / player
        if (!remove && asteroid.hp <= 0)
        {
            const int points[] = {0, 100, 50, 20};

            combo++;
            comboTimer = 2.0f;

            int gained = points[asteroid.size] * std::min(combo, 8);
            score += gained;

            addFloatingText("+" + std::to_string(gained), asteroid.position);
            spawnExplosion(asteroid.position, 10 + asteroid.size * 8, 140.0f);

            if (randomInt(1, 100) <= 10 + asteroid.size * 2)
            {
                spawnPickup(asteroid.position);
            }

            destroyed.push_back(asteroid);
            remove = true;
        }

        // Lost in deep space
        if (!remove && (asteroid.position.x < -200 || asteroid.position.x > SCREEN_WIDTH + 200 ||
                        asteroid.position.y < -200 || asteroid.position.y > SCREEN_HEIGHT + 200))
        {
            remove = true;
        }

        if (remove) it = asteroids.erase(it);
        else ++it;
    }

    for (const Asteroid& asteroid : destroyed)
    {
        splitAsteroid(asteroid);
    }
}

void SpaceDefenders::updateUfos()
{
    for (auto it = ufos.begin(); it != ufos.end(); )
    {
        Ufo& ufo = *it;

        ufo.phase += dt;
        ufo.position.x += ufo.speedX * dt;
        ufo.position.y = ufo.baseY + std::sin(ufo.phase * 2.0f) * 40.0f;

        ufo.shootTimer -= dt;

        if (ufo.shootTimer <= 0.0f)
        {
            // Alternate between shooting at the player and at earth
            SDL_FPoint target = randomInt(0, 2) == 0 ? earth.position : player.polygon.pos;

            float angle = std::atan2(target.y - ufo.position.y, target.x - ufo.position.x) + randomFloat(-0.1f, 0.1f);
            bullets.push_back({ufo.position, {std::cos(angle) * 170.0f, std::sin(angle) * 170.0f}, 4.0f});

            ufo.shootTimer = std::max(0.8f, 2.0f - wave * 0.08f);
        }

        bool remove = false;

        if (ufo.hp <= 0)
        {
            combo++;
            comboTimer = 2.0f;
            score += 300 * std::min(combo, 8);

            addFloatingText("+" + std::to_string(300 * std::min(combo, 8)), ufo.position);
            spawnExplosion(ufo.position, 40, 200.0f);
            spawnPickup(ufo.position);
            remove = true;
        }

        if (!remove && player.invulnerable <= 0.0f && isPlayerCircleIntersect(ufo.position, 18.0f))
        {
            damagePlayer();
        }

        if (ufo.position.x < -80 || ufo.position.x > SCREEN_WIDTH + 80)
        {
            remove = true;
        }

        if (remove) it = ufos.erase(it);
        else ++it;
    }

    for (auto it = bullets.begin(); it != bullets.end(); )
    {
        it->position.x += it->velocity.x * dt;
        it->position.y += it->velocity.y * dt;
        it->lifetime -= dt;

        bool remove = it->lifetime <= 0.0f;

        if (!remove && distanceSquared(it->position, earth.position) < earth.radius * earth.radius)
        {
            spawnExplosion(it->position, 8, 90.0f);
            damageEarth(1);
            remove = true;
        }

        if (!remove && player.invulnerable <= 0.0f && isPlayerCircleIntersect(it->position, 3.0f))
        {
            damagePlayer();
            remove = true;
        }

        if (remove) it = bullets.erase(it);
        else ++it;
    }
}

void SpaceDefenders::updatePickups()
{
    for (auto it = pickups.begin(); it != pickups.end(); )
    {
        it->position.x += it->velocity.x * dt;
        it->position.y += it->velocity.y * dt;
        it->lifetime -= dt;

        bool remove = it->lifetime <= 0.0f;

        if (!remove && isPlayerCircleIntersect(it->position, 12.0f))
        {
            switch (it->type)
            {
                case PickupType::Heart:
                    player.hp = std::min(PLAYER_MAX_HP, player.hp + 1);
                    addFloatingText("+1 LIFE", it->position);
                    break;

                case PickupType::Rapid:
                    rapidFireTimer = 8.0f;
                    addFloatingText("RAPID FIRE", it->position);
                    break;

                case PickupType::Triple:
                    tripleShotTimer = 8.0f;
                    addFloatingText("TRIPLE SHOT", it->position);
                    break;

                case PickupType::Repair:
                    earthHp = std::min(EARTH_MAX_HP, earthHp + 2);
                    addFloatingText("EARTH REPAIRED", it->position);
                    break;
            }

            spawnExplosion(it->position, 12, 80.0f);
            remove = true;
        }

        if (remove) it = pickups.erase(it);
        else ++it;
    }
}

void SpaceDefenders::updateEffects()
{
    for (auto it = particles.begin(); it != particles.end(); )
    {
        it->position.x += it->velocity.x * dt;
        it->position.y += it->velocity.y * dt;
        it->life -= dt;

        if (it->life <= 0.0f) it = particles.erase(it);
        else ++it;
    }

    for (auto it = floatingTexts.begin(); it != floatingTexts.end(); )
    {
        it->position.y -= 30.0f * dt;
        it->life -= dt;

        if (it->life <= 0.0f) it = floatingTexts.erase(it);
        else ++it;
    }

    if (comboTimer > 0.0f)
    {
        comboTimer -= dt;
        if (comboTimer <= 0.0f) combo = 0;
    }

    shake = std::max(0.0f, shake - dt * 25.0f);
    earthFlash = std::max(0.0f, earthFlash - dt * 2.0f);
}

// ---------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------

void SpaceDefenders::render() {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

    SDL_RenderClear(renderer);

    renderStars();

    if (state == State::Title)
    {
        renderTitle();
    }
    else
    {
        renderWorld();
        renderHud();

        if (state == State::Paused)
        {
            renderOverlay("PAUSED", "P = Continue", "ESC = Title screen");
        }

        if (state == State::GameOver)
        {
            std::string reason = earthHp <= 0 ? "EARTH WAS DESTROYED" : "YOUR SHIP WAS DESTROYED";
            std::string scoreLine = "Score: " + std::to_string(score) + "   Wave: " + std::to_string(wave) +
                                    (newHighscore ? "   NEW HIGHSCORE!" : "");

            renderOverlay(reason, scoreLine, "ENTER = Play again   ESC = Quit");
        }
    }

    // Render all
    SDL_RenderPresent(renderer);
}

void SpaceDefenders::renderStars()
{
    for (const Star& star : stars)
    {
        float b = star.brightness * (0.75f + 0.25f * std::sin(star.twinkle));
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, static_cast<Uint8>(b * 255));
        SDL_RenderPoint(renderer, star.position.x, star.position.y);
    }
}

void SpaceDefenders::renderWorld()
{
    // Screen shake: move everything by an offset
    SDL_FPoint offset = {0.0f, 0.0f};
    if (shake > 0.0f) offset = {randomFloat(-shake, shake), randomFloat(-shake, shake)};

    auto shifted = [&](SDL_FPoint p) { return SDL_FPoint{p.x + offset.x, p.y + offset.y}; };

    // Moon orbit
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 25);
    drawCircle(renderer, shifted(earth.position), moon.orbitRadius, 96);

    // Earth with some continents
    SDL_FPoint earthPos = shifted(earth.position);

    if (earthFlash > 0.0f)
    {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, static_cast<Uint8>(earthFlash * 80));
        drawFilledCircle(renderer, earthPos, earth.radius);
    }

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    drawCircle(renderer, earthPos, earth.radius);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 110);
    drawCurve(renderer, {earthPos.x - 40, earthPos.y - 20}, {earthPos.x - 10, earthPos.y - 45}, {earthPos.x + 15, earthPos.y - 25}, CoordinateMode::Absolute);
    drawCurve(renderer, {earthPos.x + 15, earthPos.y - 25}, {earthPos.x + 5, earthPos.y + 5}, {earthPos.x - 30, earthPos.y + 10}, CoordinateMode::Absolute);
    drawCurve(renderer, {earthPos.x + 10, earthPos.y + 25}, {earthPos.x + 40, earthPos.y + 10}, {earthPos.x + 35, earthPos.y + 35}, CoordinateMode::Absolute);
    drawEllipse(renderer, {earthPos.x, earthPos.y}, {earth.radius, earth.radius * 0.25f});

    // Earth shield: one arc segment per hp
    const float segmentAngle = 2.0f * SDL_PI_F / EARTH_MAX_HP;

    for (int i = 0; i < EARTH_MAX_HP; ++i)
    {
        float start = -SDL_PI_F / 2.0f + i * segmentAngle + 0.06f;
        float end = start + segmentAngle - 0.12f;

        if (i < earthHp) SDL_SetRenderDrawColor(renderer, 255, 255, 255, 220);
        else SDL_SetRenderDrawColor(renderer, 255, 255, 255, 35);

        drawArc(renderer, earthPos, earth.radius + 12.0f, start, end, 8);
        drawArc(renderer, earthPos, earth.radius + 14.0f, start, end, 8);
    }

    // Moon
    SDL_FPoint moonPos = shifted(moon.position());
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    drawCircle(renderer, moonPos, moon.radius, 32);
    drawCircle(renderer, {moonPos.x - 4, moonPos.y - 3}, 3.0f, 12);
    drawCircle(renderer, {moonPos.x + 5, moonPos.y + 4}, 2.0f, 10);

    // Asteroids
    for (const Asteroid& asteroid : asteroids)
    {
        Uint8 alpha = asteroid.hitFlash > 0.0f ? 120 : 255;
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, alpha);
        drawPolygon(renderer, {shifted(asteroid.position), asteroid.Body}, asteroid.rotation);
    }

    // UFOs
    for (const Ufo& ufo : ufos)
    {
        SDL_FPoint p = shifted(ufo.position);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        drawEllipse(renderer, p, {24.0f, 8.0f}, 32);
        drawArc(renderer, {p.x, p.y - 3}, 11.0f, SDL_PI_F, 2.0f * SDL_PI_F, 16);

        // Blinking lights
        if (std::fmod(ufo.phase, 0.4f) < 0.2f)
        {
            SDL_RenderPoint(renderer, p.x - 12, p.y + 1);
            SDL_RenderPoint(renderer, p.x + 12, p.y + 1);
        }
        else
        {
            SDL_RenderPoint(renderer, p.x, p.y + 4);
        }
    }

    // Enemy bullets
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    for (const Bullet& bullet : bullets)
    {
        drawCircle(renderer, shifted(bullet.position), 3.0f, 8);
    }

    // Pickups
    const char* labels[] = {"+", "R", "3", "E"};

    for (const Pickup& pickup : pickups)
    {
        // Blink when about to vanish
        if (pickup.lifetime < 3.0f && std::fmod(pickup.lifetime, 0.3f) < 0.15f) continue;

        SDL_FPoint p = shifted(pickup.position);
        float pulse = 1.0f + 0.15f * std::sin(time * 6.0f);

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        drawCircle(renderer, p, 11.0f * pulse, 24);

        const char* label = labels[static_cast<int>(pickup.type)];
        renderText(renderer, smallFont, label, p.x - textWidth(label, smallFont, 0.6f) / 2.0f, p.y - 9.0f, white, 0.6f);
    }

    // Lasers
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    for (auto& laser : lasers) {
        DrawPolarLine(renderer, laser.x + offset.x, laser.y + offset.y, laser.angle, laser.len);
    }

    // Particles
    for (const Particle& p : particles)
    {
        float t = p.life / p.maxLife;
        float speed = std::sqrt(p.velocity.x * p.velocity.x + p.velocity.y * p.velocity.y);

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, static_cast<Uint8>(t * 255));

        if (speed > 1.0f)
        {
            float angle = std::atan2(p.velocity.y, p.velocity.x);
            DrawPolarLine(renderer, p.position.x + offset.x, p.position.y + offset.y, angle, p.length * t);
        }
    }

    // Player (blinks while invulnerable)
    bool visible = player.invulnerable <= 0.0f || std::fmod(player.invulnerable, 0.2f) < 0.1f;

    if (visible && state != State::GameOver)
    {
        if (showTrajectory) renderTrajectory();

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

        Polygon ship = player.polygon;
        ship.pos = shifted(ship.pos);
        drawPolygon(renderer, ship, player.direction);

        // Thrust flame
        if (player.thrusting)
        {
            float flameLength = randomFloat(10.0f, 22.0f);
            float backX = ship.pos.x;
            float backY = ship.pos.y;
            float c = std::cos(player.direction);
            float s = std::sin(player.direction);

            SDL_FPoint left = {backX - s * -8.0f, backY + c * -8.0f};
            SDL_FPoint right = {backX - s * 8.0f, backY + c * 8.0f};
            SDL_FPoint tip = {backX - c * flameLength, backY - s * flameLength};

            SDL_RenderLine(renderer, left.x, left.y, tip.x, tip.y);
            SDL_RenderLine(renderer, right.x, right.y, tip.x, tip.y);
        }
    }

    // Floating texts
    for (const FloatingText& text : floatingTexts)
    {
        SDL_Color color = {255, 255, 255, static_cast<Uint8>(std::min(1.0f, text.life) * 255)};
        renderTextCentered(renderer, smallFont, text.text, text.position.x, text.position.y, color, 0.7f);
    }
}

void SpaceDefenders::renderTrajectory()
{
    // Simulate the next seconds of flight without thrust to show where gravity pulls the ship
    SDL_FPoint pos = player.polygon.pos;
    SDL_FPoint vel = player.velocity;

    const float step = 1.0f / 30.0f;

    for (int i = 1; i <= 60; ++i)
    {
        SDL_FPoint gravity = calcGravPullEarth(pos, earth);
        vel.x += gravity.x * step;
        vel.y += gravity.y * step;
        pos.x += vel.x * step;
        pos.y += vel.y * step;

        if (distanceSquared(pos, earth.position) < earth.radius * earth.radius) break;

        if (i % 3 == 0)
        {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, static_cast<Uint8>(120 - i * 2));
            SDL_RenderPoint(renderer, pos.x, pos.y);
        }
    }
}

void SpaceDefenders::renderHud()
{
    // Score & wave
    renderText(renderer, smallFont, ("SCORE " + std::to_string(score)).c_str(), 15, 12, white, 0.9f);
    renderText(renderer, smallFont, ("HI " + std::to_string(std::max(score, highscore))).c_str(), 15, 38, gray, 0.7f);
    renderTextCentered(renderer, smallFont, "WAVE " + std::to_string(wave), SCREEN_WIDTH / 2.0f, 12, white, 0.9f);

    if (combo >= 2)
    {
        renderTextCentered(renderer, smallFont, "COMBO x" + std::to_string(std::min(combo, 8)), SCREEN_WIDTH / 2.0f, 40, gray, 0.7f);
    }

    // Hearts
    std::string hearts;
    if (player.hp > 0) {
        for (int i = 0; i < player.hp; ++i) { hearts += "♥ "; }
        renderText(renderer, font, hearts.c_str(), SCREEN_WIDTH - 20 - textWidth(hearts, font, 0.6f), 8, white, 0.6f);
    }

    // Active power ups
    float y = SCREEN_HEIGHT - 30.0f;

    auto powerBar = [&](const char* name, float timer) {
        if (timer <= 0.0f) return;

        renderText(renderer, smallFont, name, 15, y, white, 0.6f);

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_FRect frame = {110, y + 4, 100, 8};
        SDL_FRect fill = {110, y + 4, 100 * timer / 8.0f, 8};
        SDL_RenderRect(renderer, &frame);
        SDL_RenderFillRect(renderer, &fill);

        y -= 22.0f;
    };

    powerBar("RAPID FIRE", rapidFireTimer);
    powerBar("TRIPLE SHOT", tripleShotTimer);

    // Earth status
    std::string earthText = "EARTH " + std::to_string(earthHp * 10) + "%";
    renderText(renderer, smallFont, earthText.c_str(), SCREEN_WIDTH - 15 - textWidth(earthText, smallFont, 0.7f), SCREEN_HEIGHT - 28, earthHp <= 3 ? white : gray, 0.7f);

    // Wave banner
    if (waveBannerTimer > 0.0f)
    {
        SDL_Color color = {255, 255, 255, static_cast<Uint8>(std::min(1.0f, waveBannerTimer) * 255)};
        renderTextCentered(renderer, font, "WAVE " + std::to_string(wave), SCREEN_WIDTH / 2.0f, 120, color);

        if (wave >= 3 && ufosToSpawn > 0)
        {
            renderTextCentered(renderer, smallFont, "UFOs detected!", SCREEN_WIDTH / 2.0f, 180, color, 0.8f);
        }
    }
}

void SpaceDefenders::renderTitle()
{
    // Decorative orbit animation
    SDL_FPoint center = {SCREEN_WIDTH / 2.0f, 330.0f};

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    drawCircle(renderer, center, 40.0f);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 60);
    drawEllipse(renderer, center, {120.0f, 40.0f});

    SDL_FPoint shipPos = {center.x + std::cos(time) * 120.0f, center.y + std::sin(time) * 40.0f};
    float shipDir = std::atan2(std::cos(time) * 40.0f, -std::sin(time) * 120.0f);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    drawPolygon(renderer, {shipPos, {{0, -8.0f}, {0, 8.0f}, {18.0f, 0}}}, shipDir);

    renderTextCentered(renderer, font, "SPACE DEFENDERS", SCREEN_WIDTH / 2.0f, 80, white);
    renderTextCentered(renderer, smallFont, "Protect Earth from the incoming asteroids!", SCREEN_WIDTH / 2.0f, 150, gray, 0.8f);
    renderTextCentered(renderer, smallFont, "Highscore: " + std::to_string(highscore), SCREEN_WIDTH / 2.0f, 185, gray, 0.8f);

    renderTextCentered(renderer, smallFont, "LEFT/RIGHT = Turn   UP = Thrust   SPACE = Shoot", SCREEN_WIDTH / 2.0f, 420, white, 0.75f);
    renderTextCentered(renderer, smallFont, "T = Toggle trajectory   P = Pause   ESC = Quit", SCREEN_WIDTH / 2.0f, 448, gray, 0.75f);
    renderTextCentered(renderer, smallFont, "Pickups:  + Life   R Rapid fire   3 Triple shot   E Earth repair", SCREEN_WIDTH / 2.0f, 476, gray, 0.75f);

    if (std::fmod(time, 1.0f) < 0.6f)
    {
        renderTextCentered(renderer, smallFont, "PRESS ENTER", SCREEN_WIDTH / 2.0f, 530, white);
    }
}

void SpaceDefenders::renderOverlay(const std::string& title, const std::string& line1, const std::string& line2)
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 170);
    SDL_FRect overlay = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_RenderFillRect(renderer, &overlay);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_FRect box = {130, 200, 540, 190};
    SDL_RenderRect(renderer, &box);

    renderTextCentered(renderer, font, title, SCREEN_WIDTH / 2.0f, 220, white, 0.7f);
    renderTextCentered(renderer, smallFont, line1, SCREEN_WIDTH / 2.0f, 290, white, 0.8f);
    renderTextCentered(renderer, smallFont, line2, SCREEN_WIDTH / 2.0f, 335, gray, 0.8f);
}

// ---------------------------------------------------------------------------
// Physics helpers
// ---------------------------------------------------------------------------

SDL_FPoint SpaceDefenders::calcGravPullEarth(const SDL_FPoint& pos, const Planet& earth) {
    float dx = earth.position.x - pos.x;
    float dy = earth.position.y - pos.y;

    float g;

    float r_squared = (dx * dx + dy * dy);
    
    if (r_squared < 3025.0f) {
        return {0, 0};
    }

    float r_scaledSquared = r_squared * 8.0E8f;
    g = (gravConstant * earth.mass) / r_scaledSquared;
    
    float angle = std::atan2(dy, dx);

    return { std::cos(angle) * g, std::sin(angle) * g };
}

bool SpaceDefenders::isPlayerPlanetIntersect(const Player& player, const Planet& planet)
{
    const float radiusSquared = planet.radius * planet.radius;

    auto isInsidePlanet = [&](const SDL_FPoint& point)
    {
        float dx = point.x - planet.position.x;
        float dy = point.y - planet.position.y;

        return dx * dx + dy * dy < radiusSquared;
    };

    if (isInsidePlanet(player.polygon.pos)) return true;

    for (const SDL_FPoint& point : player.polygon.body)
    {
        float rotatedX = point.x * std::cos(player.direction) - point.y * std::sin(player.direction);

        float rotatedY = point.x * std::sin(player.direction) + point.y * std::cos(player.direction);

        SDL_FPoint worldPoint = {
            player.polygon.pos.x + rotatedX,
            player.polygon.pos.y + rotatedY
        };

        if (isInsidePlanet(worldPoint)) return true;
    }

    return false;
}

bool SpaceDefenders::isPlayerCircleIntersect(const SDL_FPoint& center, float radius) const
{
    // Approximate the ship by a circle around its centroid
    SDL_FPoint shipCenter = {
        player.polygon.pos.x + std::cos(player.direction) * 12.0f,
        player.polygon.pos.y + std::sin(player.direction) * 12.0f
    };

    float r = radius + PLAYER_RADIUS;

    return distanceSquared(shipCenter, center) < r * r;
}
