#include <raylib.h> // game header
#include "raymath.h" // controlling vectors
#include <string> // for frame names
#include <cmath> // for fmin
#include <vector>
#include <algorithm>
#include <bits/stdc++.h>

#define RAYTMX_IMPLEMENTATION
#include "raytmx.h"

// Game state enum
enum class GameState {
    StartScreen,
    Playing
};

// comment
TmxMap* currentMap = nullptr;
TmxLayer* wallLayer = nullptr;

class Slash {
public:
    static Texture2D frames[3]; // shared textures
    Vector2 position;
    Vector2 direction;  // normalized
    int frame;
    float animTimer;
    bool finished;

    static void LoadAssets();
    static void UnloadAssets();

    Slash(Vector2 playerPos, Vector2 dir);

    void Update(float dt);

    void Draw();

    Rectangle GetHitbox();
};

Texture2D Slash::frames[3]; // definition

std::vector<Slash> slashes;

enum class PlayerState {
    Idle,
    Run,
    Hit
};

class Player {
public:
    Vector2 pos{160, 90}; // what position its at (starts at centre)
    Vector2 vel{0, 0}; // velocity/movement
    bool facingRight = true; // the player frame is only facing right, so I have to mirror it myself
    int health = 100; // current health
    int maxHealth = 100; // maximum health
    float damageCooldown = 0.0f; // cooldown timer to prevent rapid damage
    float damageCooldownDuration = 0.5f; // seconds between damage events

    PlayerState state = PlayerState::Idle; // default

    Texture2D idleAnim[4]; // idle animation has 4 frames
    Texture2D runAnim[4]; // run has 4 frames
    Texture2D hitSprite; // hit is one sprite

    // going through frames
    int currentFrame = 0; 
    float frameTime = 0.15f; // in seconds
    float frameTimer = 0.0f;

    // hit state
    float hitTimer = 0.0f; 
    float hitDuration = 0.2f; // seconds

    void Load();
    void Unload();
    void Update();
    void Draw();
    void DrawHealthBar(); // New method to draw health bar
    void TakeDamage(int dmg); // New method to take damage
    Rectangle GetHitbox() const; // New method to get player hitbox
};

Player player; // creates player

enum class EnemyState {
    Patrol,
    Chase
};

enum class EnemyType {
    Goblin,
    Imp,
    BigZombie,
    BigDemon
};

class Enemy {
protected:
    Vector2 position;
    Vector2 spawnPos;
    Texture2D* runFrames; // Pointer to preloaded textures
    int currentFrame;
    float frameTimer;
    float frameTime;
    float speed;
    int health;
    int damage;
    bool alive;
    EnemyState state;
    float detectRange;
    Vector2 target;
    bool facingRight;           // For sprite flipping
    Vector2 knockbackVelocity;  // Knockback applied when hit
    float knockbackTimer;       // Duration for knockback
    EnemyType type;             // To identify enemy type for pooling

public:
    Enemy(Vector2 pos, EnemyType t, Texture2D* frames, int baseHp, int baseDmg, float spd, float range);
    virtual ~Enemy();

    virtual void Update(float dt, const Player& player);
    virtual void Draw();
    virtual void TakeDamage(int dmg, Vector2 hitDirection);
    void Reset(Vector2 pos, EnemyType t, int baseHp, int baseDmg, float spd, float range);

    bool IsAlive() const;
    int GetDamage() const;
    Vector2 GetPosition() const;
    Rectangle GetHitbox() const;
    EnemyType GetType() const { return type; }
};

// Preloaded textures for each enemy type
static Texture2D goblinFrames[4];
static Texture2D impFrames[4];
static Texture2D bigZombieFrames[4];
static Texture2D bigDemonFrames[4];

std::vector<Enemy*> enemies;
std::vector<Enemy*> enemyPool; // Object pool for enemies

class Goblin : public Enemy {
public:
    Goblin(Vector2 pos) : Enemy(pos, EnemyType::Goblin, goblinFrames, 30, 5, 1.5f, 80.0f) {}
};

class Imp : public Enemy {
public:
    Imp(Vector2 pos) : Enemy(pos, EnemyType::Imp, impFrames, 20, 3, 2.0f, 90.0f) {}
};

class BigZombie : public Enemy {
public:
    BigZombie(Vector2 pos) : Enemy(pos, EnemyType::BigZombie, bigZombieFrames, 50, 10, 1.2f, 100.0f) {}
};

class BigDemon : public Enemy {
public:
    BigDemon(Vector2 pos) : Enemy(pos, EnemyType::BigDemon, bigDemonFrames, 80, 15, 1.3f, 120.0f) {}
};

// Spawner positions
std::vector<Vector2> goblinSpawners;
std::vector<Vector2> impSpawners;
std::vector<Vector2> bigZombieSpawners;
std::vector<Vector2> bigDemonSpawners;

// Spawn timers
float smallEnemySpawnTimer = 0.0f;
float bigEnemySpawnTimer = 0.0f;
float minuteTimer = 0.0f;
float smallEnemySpawnInterval = 15.0f;
float bigEnemySpawnInterval = 60.0f;

// Wave system
int currentWave = 1;
int totalKills = 0;
int requiredKills[8] = {3, 8, 20, 50, 110, 200, 350, 500};
float playerDamage = 10.0f;

// Slash implementations
void Slash::LoadAssets() {
    for (int i = 0; i < 3; i++) {
        frames[i] = LoadTexture(TextFormat("assets/Player/slash_f%d.png", i));
    }
}

void Slash::UnloadAssets() {
    for (int i = 0; i < 3; i++) {
        UnloadTexture(frames[i]);
    }
}

Slash::Slash(Vector2 playerPos, Vector2 dir) {
    direction = Vector2Normalize(dir);
    position = Vector2Add(playerPos, Vector2Scale(direction, 20));
    frame = 0;
    animTimer = 0.0f;
    finished = false;
}

void Slash::Update(float dt) {
    animTimer += dt;
    if (animTimer > 0.1f) {
        animTimer = 0.0f;
        frame++;
        if (frame >= 3) finished = true;
    }
}

void Slash::Draw() {
    if (finished) return;

    Texture2D tex = frames[frame];
    float angle = atan2f(direction.y, direction.x) * RAD2DEG;

    DrawTexturePro(
        tex,
        {0, 0, (float)tex.width, (float)tex.height},
        {position.x, position.y, (float)tex.width, (float)tex.height},
        {tex.width/2.0f, tex.height/2.0f}, // origin center
        angle,
        WHITE
    );
}

Rectangle Slash::GetHitbox() {
    Texture2D tex = frames[frame];
    return {position.x - tex.width/2, position.y - tex.height/2,
            (float)tex.width, (float)tex.height};
}

// Player implementations
void Player::Load() {
    for (int i = 0; i < 4; i++) {
        idleAnim[i] = LoadTexture(("assets/Player/knight_f_idle_anim_f" + std::to_string(i) + ".png").c_str());
        runAnim[i] = LoadTexture(("assets/Player/knight_f_run_anim_f" + std::to_string(i) + ".png").c_str()); 
    }
    hitSprite = LoadTexture("assets/Player/knight_f_hit_anim_f0.png");
}

void Player::Unload() {
    for (int i = 0; i < 4; i++) {
        UnloadTexture(idleAnim[i]);
        UnloadTexture(runAnim[i]);
    }
    UnloadTexture(hitSprite);
}

void Player::Update() {
    vel = {0, 0}; // velocity resets every frame, so that player only moves when they input

    // Update damage cooldown
    if (damageCooldown > 0.0f) {
        damageCooldown -= GetFrameTime();
    }

    // If in hit state, reduce timer
    if (hitTimer > 0.0f) {
        hitTimer -= GetFrameTime();
        if (hitTimer <= 0.0f) {
            // Reset back to idle after hit ends
            state = PlayerState::Idle;
        }
        return; // skip movement while hit
    }

    // Movement input
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) {
        vel.x = 2;
        facingRight = true;
    }
    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) {
        vel.x = -2;
        facingRight = false;
    }
    if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) vel.y = -2;
    if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) vel.y = 2;

    // Trigger hit
    if (IsKeyPressed(KEY_SPACE)) {
        state = PlayerState::Hit;
        hitTimer = hitDuration;
        currentFrame = 0;
    }

    if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_J)) {
        // attack and animation
        Vector2 dir = {0, 0};

        // Use last velocity if moving
        if (vel.x != 0 || vel.y != 0) {
            dir = Vector2Normalize(vel);
        } else {
            // fallback to facing direction
            dir = facingRight ? Vector2{1,0} : Vector2{-1,0};
        }

        slashes.push_back(Slash(pos, dir));
    }

    // Apply movement with collision check, separate x and y
    float new_x = pos.x + vel.x;
    float new_y = pos.y + vel.y;
    Rectangle new_rect_x = { new_x - 8, pos.y - 8, 16, 16 };
    Rectangle new_rect_y = { pos.x - 8, new_y - 8, 16, 16 };

    // Check collision with Wall layer
    bool collision_x = false, collision_y = false;
    if (wallLayer && wallLayer->type == LAYER_TYPE_TILE_LAYER && wallLayer->exact.tileLayer.tiles && (vel.x != 0 || vel.y != 0)) {
        int tile_width = currentMap->tileWidth;
        int tile_height = currentMap->tileHeight;

        // Check x movement
        if (vel.x != 0) {
            int left_tile = (int)(new_rect_x.x) / tile_width;
            int right_tile = (int)(new_rect_x.x + new_rect_x.width - 1) / tile_width;
            int top_tile = (int)(new_rect_x.y) / tile_height;
            int bottom_tile = (int)(new_rect_x.y + new_rect_x.height - 1) / tile_height;

            left_tile = std::max(0, std::min(left_tile, (int)currentMap->width - 1));
            right_tile = std::max(0, std::min(right_tile, (int)currentMap->width - 1));
            top_tile = std::max(0, std::min(top_tile, (int)currentMap->height - 1));
            bottom_tile = std::max(0, std::min(bottom_tile, (int)currentMap->height - 1));

            for (int y = top_tile; y <= bottom_tile; y++) {
                for (int x = left_tile; x <= right_tile; x++) {
                    unsigned int tileID = wallLayer->exact.tileLayer.tiles[y * wallLayer->exact.tileLayer.width + x];
                    if (tileID != 0) {
                        collision_x = true;
                        break;
                    }
                }
                if (collision_x) break;
            }
        }

        // Check y movement
        if (vel.y != 0) {
            int left_tile = (int)(new_rect_y.x) / tile_width;
            int right_tile = (int)(new_rect_y.x + new_rect_y.width - 1) / tile_width;
            int top_tile = (int)(new_rect_y.y) / tile_height;
            int bottom_tile = (int)(new_rect_y.y + new_rect_y.height - 1) / tile_height;

            left_tile = std::max(0, std::min(left_tile, (int)currentMap->width - 1));
            right_tile = std::max(0, std::min(right_tile, (int)currentMap->width - 1));
            top_tile = std::max(0, std::min(top_tile, (int)currentMap->height - 1));
            bottom_tile = std::max(0, std::min(bottom_tile, (int)currentMap->height - 1));

            for (int y = top_tile; y <= bottom_tile; y++) {
                for (int x = left_tile; x <= right_tile; x++) {
                    unsigned int tileID = wallLayer->exact.tileLayer.tiles[y * wallLayer->exact.tileLayer.width + x];
                    if (tileID != 0) {
                        collision_y = true;
                        break;
                    }
                }
                if (collision_y) break;
            }
        }
    }

    // Apply movement if no collision
    if (!collision_x) pos.x = new_x;
    if (!collision_y) pos.y = new_y;

    if (vel.x != 0 || vel.y != 0) state = PlayerState::Run; // when moving, use the run animation
    else state = PlayerState::Idle;

    // Update animation frame
    frameTimer += GetFrameTime();
    if (frameTimer >= frameTime) {
        frameTimer = 0.0f;
        currentFrame++;
    }

    if (state == PlayerState::Idle && currentFrame >= 4) currentFrame = 0;
    if (state == PlayerState::Run && currentFrame >= 4) currentFrame = 0;
    if (state == PlayerState::Hit) currentFrame = 0; // single-frame sprite
}

void Player::Draw() {
    Texture2D sprite;
    if (state == PlayerState::Idle) sprite = idleAnim[currentFrame];
    else if (state == PlayerState::Run) sprite = runAnim[currentFrame];
    else sprite = hitSprite;

    Rectangle src = {0, 0, (float)sprite.width, (float)sprite.height}; // what its drawing (source)
    if (!facingRight) src.width *= -1; // flips to left if its not facing right

    Rectangle dest = {pos.x, pos.y, (float)sprite.width, (float)sprite.height}; // draws at current position
    Vector2 origin = {sprite.width/2.0f, sprite.height/2.0f};
    DrawTexturePro(sprite, src, dest, origin, 0.0f, WHITE);
}

void Player::DrawHealthBar() {
    // Draw in screen coordinates (top-left of render texture)
    float barWidth = 100.0f; // Width of health bar in pixels
    float barHeight = 10.0f; // Height of health bar
    float x = 10.0f; // Margin from left
    float y = 10.0f; // Margin from top

    // Background (max health, red)
    DrawRectangle(x, y, barWidth, barHeight, RED);

    // Foreground (current health, green)
    float healthRatio = (float)health / maxHealth;
    DrawRectangle(x, y, barWidth * healthRatio, barHeight, GREEN);

    // Optional: Draw HP text
    DrawText(TextFormat("%d/%d", health, maxHealth), x + 5, y+1, 10, WHITE);
}

void Player::TakeDamage(int dmg) {
    if (damageCooldown <= 0.0f) {
        health = std::max(0, health - dmg);
        damageCooldown = damageCooldownDuration; // Reset cooldown
    }
}

Rectangle Player::GetHitbox() const {
    return {pos.x - 8, pos.y - 8, 16, 16}; // 16x16 hitbox centered on position
}

// Enemy implementations
Enemy::Enemy(Vector2 pos, EnemyType t, Texture2D* frames, int baseHp, int baseDmg, float spd, float range)
    : position(pos), spawnPos(pos), runFrames(frames), currentFrame(0), frameTimer(0.0f), frameTime(0.15f), speed(spd),
      alive(true), state(EnemyState::Chase), detectRange(range), target(pos), facingRight(true),
      knockbackVelocity({0,0}), knockbackTimer(0.0f), type(t)
{
    health = (int)(baseHp * pow(1.5f, currentWave - 1)); // 50% HP increase per wave
    damage = (int)(baseDmg * pow(1.2f, currentWave - 1));
}

Enemy::~Enemy() {
    // Textures are managed globally, so no unloading here
}

void Enemy::Reset(Vector2 pos, EnemyType t, int baseHp, int baseDmg, float spd, float range) {
    position = pos;
    spawnPos = pos;
    type = t;
    currentFrame = 0;
    frameTimer = 0.0f;
    frameTime = 0.15f;
    speed = spd;
    health = (int)(baseHp * pow(1.5f, currentWave - 1));
    damage = (int)(baseDmg * pow(1.2f, currentWave - 1));
    alive = true;
    state = EnemyState::Chase;
    detectRange = range;
    target = pos;
    facingRight = true;
    knockbackVelocity = {0, 0};
    knockbackTimer = 0.0f;
}

void Enemy::Update(float dt, const Player& player) {
    if (!alive) return;

    // If under knockback, apply it and reduce timer
    if (knockbackTimer > 0.0f) {
        Vector2 newPos = Vector2Add(position, Vector2Scale(knockbackVelocity, dt * 60));
        Rectangle newRect = {newPos.x - 8, newPos.y - 8, 16, 16};

        // Check wall collision for knockback
        bool collision_x = false, collision_y = false;
        if (wallLayer && wallLayer->type == LAYER_TYPE_TILE_LAYER && wallLayer->exact.tileLayer.tiles) {
            int tile_width = currentMap->tileWidth;
            int tile_height = currentMap->tileHeight;

            int left_tile = (int)(newRect.x) / tile_width;
            int right_tile = (int)(newRect.x + newRect.width - 1) / tile_width;
            int top_tile = (int)(newRect.y) / tile_height;
            int bottom_tile = (int)(newRect.y + newRect.height - 1) / tile_height;

            left_tile = std::max(0, std::min(left_tile, (int)currentMap->width - 1));
            right_tile = std::max(0, std::min(right_tile, (int)currentMap->width - 1));
            top_tile = std::max(0, std::min(top_tile, (int)currentMap->height - 1));
            bottom_tile = std::max(0, std::min(bottom_tile, (int)currentMap->height - 1));

            for (int y = top_tile; y <= bottom_tile; y++) {
                for (int x = left_tile; x <= right_tile; x++) {
                    unsigned int tileID = wallLayer->exact.tileLayer.tiles[y * wallLayer->exact.tileLayer.width + x];
                    if (tileID != 0) {
                        collision_x = true;
                        collision_y = true; // Treat as full collision during knockback
                        break;
                    }
                }
                if (collision_x) break;
            }
        }

        if (!collision_x && !collision_y) position = newPos;
        knockbackTimer -= dt;
        if (knockbackTimer <= 0.0f) {
            knockbackVelocity = {0, 0};
        }
        return; // Skip normal AI while knocked back
    }

    // Always chase the player
    state = EnemyState::Chase;
    target = player.pos;

    // Calculate new position
    Vector2 dir = Vector2Normalize(Vector2Subtract(target, position));
    Vector2 newPos = Vector2Add(position, Vector2Scale(dir, speed * dt * 60));

    // Check wall collision
    Rectangle newRect = {newPos.x - 8, newPos.y - 8, 16, 16};
    bool collision_x = false, collision_y = false;
    if (wallLayer && wallLayer->type == LAYER_TYPE_TILE_LAYER && wallLayer->exact.tileLayer.tiles) {
        int tile_width = currentMap->tileWidth;
        int tile_height = currentMap->tileHeight;

        // Check x movement
        if (newPos.x != position.x) {
            int left_tile = (int)(newRect.x) / tile_width;
            int right_tile = (int)(newRect.x + newRect.width - 1) / tile_width;
            int top_tile = (int)(newRect.y) / tile_height;
            int bottom_tile = (int)(newRect.y + newRect.height - 1) / tile_height;

            left_tile = std::max(0, std::min(left_tile, (int)currentMap->width - 1));
            right_tile = std::max(0, std::min(right_tile, (int)currentMap->width - 1));
            top_tile = std::max(0, std::min(top_tile, (int)currentMap->height - 1));
            bottom_tile = std::max(0, std::min(bottom_tile, (int)currentMap->height - 1));

            for (int y = top_tile; y <= bottom_tile; y++) {
                for (int x = left_tile; x <= right_tile; x++) {
                    unsigned int tileID = wallLayer->exact.tileLayer.tiles[y * wallLayer->exact.tileLayer.width + x];
                    if (tileID != 0) {
                        collision_x = true;
                        break;
                    }
                }
                if (collision_x) break;
            }
        }

        // Check y movement
        if (newPos.y != position.y) {
            int left_tile = (int)(newRect.x) / tile_width;
            int right_tile = (int)(newRect.x + newRect.width - 1) / tile_width;
            int top_tile = (int)(newRect.y) / tile_height;
            int bottom_tile = (int)(newRect.y + newRect.height - 1) / tile_height;

            left_tile = std::max(0, std::min(left_tile, (int)currentMap->width - 1));
            right_tile = std::max(0, std::min(right_tile, (int)currentMap->width - 1));
            top_tile = std::max(0, std::min(top_tile, (int)currentMap->height - 1));
            bottom_tile = std::max(0, std::min(bottom_tile, (int)currentMap->height - 1));

            for (int y = top_tile; y <= bottom_tile; y++) {
                for (int x = left_tile; x <= right_tile; x++) {
                    unsigned int tileID = wallLayer->exact.tileLayer.tiles[y * wallLayer->exact.tileLayer.width + x];
                    if (tileID != 0) {
                        collision_y = true;
                        break;
                    }
                }
                if (collision_y) break;
            }
        }
    }

    // Apply movement if no wall collision
    if (!collision_x) position.x = newPos.x;
    if (!collision_y) position.y = newPos.y;

    // Check collision with other enemies
    Rectangle myHitbox = GetHitbox();
    for (auto& other : enemies) {
        if (other != this && other->IsAlive()) {
            Rectangle otherHitbox = other->GetHitbox();
            if (CheckCollisionRecs(myHitbox, otherHitbox)) {
                // Resolve overlap by moving away
                Vector2 pushDir = Vector2Normalize(Vector2Subtract(position, other->GetPosition()));
                float overlap = 8.0f - Vector2Distance(position, other->GetPosition()); // Assume 16x16 hitbox, 8px radius
                if (overlap > 0) {
                    position = Vector2Add(position, Vector2Scale(pushDir, overlap));
                }
            }
        }
    }

    // Update facing direction
    if (dir.x != 0) facingRight = (dir.x > 0);

    // Animate
    frameTimer += dt;
    if (frameTimer > frameTime) {
        frameTimer = 0.0f;
        currentFrame = (currentFrame + 1) % 4;
    }
}

void Enemy::Draw() {
    if (!alive) return;
    Texture2D tex = runFrames[currentFrame];

    Rectangle src = {0, 0, (float)tex.width, (float)tex.height};
    if (!facingRight) src.width *= -1; // Flip horizontally like player

    Rectangle dst = {position.x, position.y, (float)tex.width, (float)tex.height};
    Vector2 origin = {tex.width / 2.0f, tex.height / 2.0f};

    DrawTexturePro(tex, src, dst, origin, 0.0f, WHITE);
}

void Enemy::TakeDamage(int dmg, Vector2 hitDirection) {
    health -= dmg;
    if (health <= 0) {
        if (alive) {
            alive = false;
            totalKills++;
        }
        return;
    }

    // Apply knockback
    Vector2 dirFromPlayer = Vector2Normalize(hitDirection); 
    knockbackVelocity = Vector2Scale(dirFromPlayer, 2.5f); // strength of knockback
    knockbackTimer = 0.15f; // knockback duration in seconds
}

bool Enemy::IsAlive() const { return alive; }
int Enemy::GetDamage() const { return damage; }
Vector2 Enemy::GetPosition() const { return position; }

Rectangle Enemy::GetHitbox() const {
    Texture2D tex = runFrames[currentFrame];
    return {position.x - 8, position.y - 8, 16, 16}; // Assume a 16x16 hitbox centered on position
}

// Globals
Camera2D camera; // creates camera
Texture2D startScreen; // Start screen texture
GameState gameState = GameState::StartScreen; // Start in start screen
GameState targetState = GameState::StartScreen;
float fadeAlpha = 0.0f; // Start transparent
float fadeSpeed = 1.0f; // Fade duration in seconds
bool fadingOut = false; // Track if fading to black

Texture2D tilemap;
RenderTexture2D target; // Camera size
bool fullscreen = false;

// Load all enemy textures
void LoadEnemyTextures() {
    for (int i = 0; i < 4; i++) {
        goblinFrames[i] = LoadTexture(("assets/Enemies/goblin_run_anim_f" + std::to_string(i) + ".png").c_str());
        impFrames[i] = LoadTexture(("assets/Enemies/imp_run_anim_f" + std::to_string(i) + ".png").c_str());
        bigZombieFrames[i] = LoadTexture(("assets/Enemies/big_zombie_run_anim_f" + std::to_string(i) + ".png").c_str());
        bigDemonFrames[i] = LoadTexture(("assets/Enemies/big_demon_run_anim_f" + std::to_string(i) + ".png").c_str());
    }
}

void UnloadEnemyTextures() {
    for (int i = 0; i < 4; i++) {
        UnloadTexture(goblinFrames[i]);
        UnloadTexture(impFrames[i]);
        UnloadTexture(bigZombieFrames[i]);
        UnloadTexture(bigDemonFrames[i]);
    }
}

// Initialize enemy pool
void InitializeEnemyPool() {
    // Create a pool of enemies (adjust size based on expected max enemies)
    for (int i = 0; i < 100; i++) { // Arbitrary pool size, adjust as needed
        enemyPool.push_back(new Goblin({0, 0}));
        enemyPool.push_back(new Imp({0, 0}));
        enemyPool.push_back(new BigZombie({0, 0}));
        enemyPool.push_back(new BigDemon({0, 0}));
    }
    // Set all pooled enemies to inactive
    for (auto& e : enemyPool) {
        e->TakeDamage(9999, {0, 0}); // Ensure they're "dead"
    }
}

// Get an enemy from the pool
Enemy* GetEnemyFromPool(Vector2 pos, EnemyType type) {
    for (auto& e : enemyPool) {
        if (!e->IsAlive() && e->GetType() == type) {
            switch (type) {
                case EnemyType::Goblin:
                    e->Reset(pos, type, 30, 5, 1.5f, 80.0f);
                    break;
                case EnemyType::Imp:
                    e->Reset(pos, type, 20, 3, 2.0f, 90.0f);
                    break;
                case EnemyType::BigZombie:
                    e->Reset(pos, type, 50, 10, 1.2f, 100.0f);
                    break;
                case EnemyType::BigDemon:
                    e->Reset(pos, type, 80, 15, 1.3f, 120.0f);
                    break;
            }
            return e;
        }
    }
    // If pool is exhausted, create a new enemy (fallback)
    switch (type) {
        case EnemyType::Goblin:
            return new Goblin(pos);
        case EnemyType::Imp:
            return new Imp(pos);
        case EnemyType::BigZombie:
            return new BigZombie(pos);
        case EnemyType::BigDemon:
            return new BigDemon(pos);
        default:
            return nullptr;
    }
}

// Reset game state to initial conditions
void ResetGame() {
    // Clear active enemies
    for (auto& e : enemies) {
        e->TakeDamage(9999, {0, 0}); // Return to pool by marking as dead
    }
    enemies.clear();
    slashes.clear();

    // Reset player
    player.pos = {160, 90};
    player.vel = {0, 0};
    player.facingRight = true;
    player.maxHealth = 100;
    player.health = player.maxHealth; // Ensure current health is reset
    player.damageCooldown = 0.0f;
    player.state = PlayerState::Idle;
    player.currentFrame = 0;
    player.frameTimer = 0.0f;
    player.hitTimer = 0.0f;
    playerDamage = 10.0f;

    // Reset spawn timers
    smallEnemySpawnTimer = 0.0f;
    bigEnemySpawnTimer = 0.0f;
    minuteTimer = 0.0f;
    smallEnemySpawnInterval = 5.0f;
    bigEnemySpawnInterval = 20.0f;

    // Reset wave
    currentWave = 1;
    totalKills = 0;

    // Reset camera
    camera.target = player.pos;
}

// Defining everything for the game
void GameStartup() {
    printf("Hello");
    currentMap = LoadTMX("assets/Tilemap/WAVESPAWN.tmx");
    printf("Bye");
    wallLayer = nullptr;

    // Find Wall tile layer
    for (uint32_t i = 0; i < currentMap->layersLength; i++) {
        TmxLayer& layer = currentMap->layers[i];
        if (layer.type == LAYER_TYPE_TILE_LAYER && layer.name && strcmp(layer.name, "Wall") == 0) {
            wallLayer = &layer;
            break;
        }
    }

    // Load start screen
    startScreen = LoadTexture("assets/Images/start.png");
    if (startScreen.id == 0) {
        TraceLog(LOG_WARNING, "Failed to load start screen texture: assets/Images/start.png");
    }

    // Collect spawner positions from object layer
    for (uint32_t i = 0; i < currentMap->layersLength; i++) {
        TmxLayer& layer = currentMap->layers[i];
        if (layer.type == LAYER_TYPE_OBJECT_GROUP && layer.name && strcmp(layer.name, "Enemy") == 0 && layer.exact.objectGroup.objects) {
            for (uint32_t j = 0; j < layer.exact.objectGroup.objectsLength; j++) {
                TmxObject* obj = &layer.exact.objectGroup.objects[j];
                if (obj->name) {
                    Vector2 pos = {(float)obj->x, (float)obj->y};
                    if (strcmp(obj->name, "goblin") == 0)
                        goblinSpawners.push_back(pos);
                    else if (strcmp(obj->name, "imp") == 0)
                        impSpawners.push_back(pos);
                    else if (strcmp(obj->name, "big_demon") == 0)
                        bigDemonSpawners.push_back(pos);
                    else if (strcmp(obj->name, "big_zombie") == 0)
                        bigZombieSpawners.push_back(pos);
                }
            }
        }
    }

    // Default player position
    player.pos = {160, 90};

    target = LoadRenderTexture(320, 180);
    SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

    player.Load();
    Slash::LoadAssets();
    LoadEnemyTextures();
    InitializeEnemyPool();

    camera.target = player.pos;
    camera.offset = {320.0f / 2, 180.0f / 2};
    camera.zoom = 1.0f;

    SetExitKey(KEY_F1);
}

// Updates things every frame
void GameUpdate() {
    float dt = GetFrameTime();

    // Handle fade transitions
    if (fadingOut) {
        fadeAlpha += dt / fadeSpeed;
        if (fadeAlpha >= 1.0f) {
            fadeAlpha = 1.0f;
        }
    } else {
        fadeAlpha -= dt / fadeSpeed;
        if (fadeAlpha <= 0.0f) {
            fadeAlpha = 0.0f;
        }
    }

    // Check if we need to switch states after fading to black
    if (fadingOut && fadeAlpha >= 1.0f) {
        gameState = targetState;
        if (gameState == GameState::Playing) {
            ResetGame();
        }
        fadingOut = false;
    }

    if (gameState == GameState::StartScreen) {
        // Check for any key press to start game
        if ((IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_J) || IsKeyPressed(KEY_ENTER)) && !fadingOut) {
            targetState = GameState::Playing;
            fadingOut = true;
        }
    } else if (gameState == GameState::Playing) {
        // Toggle fullscreen (borderless) with Alt+Enter
        if (IsKeyPressed(KEY_ENTER) && (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT))) {
            fullscreen = !fullscreen;
            if (fullscreen) {
                SetWindowState(FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_TOPMOST);
                SetWindowSize(GetMonitorWidth(0), GetMonitorHeight(0));
                SetWindowPosition(0, 0);
            } else {
                ClearWindowState(FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_TOPMOST);
                SetWindowSize(1280, 720);
                SetWindowPosition(100, 100);
            }
        }

        if (IsKeyDown(KEY_ESCAPE)) {
            // hold down to quit after 3 seconds
            static float escHoldTime = 0.0f;
            if (IsKeyDown(KEY_ESCAPE)) {
                escHoldTime += GetFrameTime();
                if (escHoldTime >= 3.0f) {
                    CloseWindow(); // exit game
                }
            } else {
                escHoldTime = 0.0f; // reset timer if key released
            }
        }

        player.Update();
        camera.target = player.pos;

        // Check player-enemy collisions
        Rectangle playerHitbox = player.GetHitbox();
        for (auto& e : enemies) {
            if (e->IsAlive() && CheckCollisionRecs(playerHitbox, e->GetHitbox())) {
                player.TakeDamage(e->GetDamage());
            }
        }

        // Check for player death
        if (player.health <= 0 && !fadingOut) {
            targetState = GameState::StartScreen;
            fadingOut = true;
        }

        // Update slashes
        for (auto& s : slashes) s.Update(GetFrameTime());
        for (auto& s : slashes) {
            Rectangle hitbox = s.GetHitbox();
            for (auto& e : enemies) {
                if (e->IsAlive() && CheckCollisionRecs(hitbox, e->GetHitbox())) {
                    e->TakeDamage((int)playerDamage, Vector2Normalize(Vector2Subtract(e->GetPosition(), player.pos))); // player deals damage
                }
            }
        }
        // Remove finished slashes
        slashes.erase(
            std::remove_if(slashes.begin(), slashes.end(),
                [](Slash& s){ return s.finished; }),
            slashes.end()
        );

        for (auto& e : enemies) e->Update(GetFrameTime(), player);

        // Move dead enemies back to pool
        enemies.erase(
            std::remove_if(enemies.begin(), enemies.end(),
                [](Enemy* e) { return !e->IsAlive(); }),
            enemies.end()
        );

        // Update minute timer
        minuteTimer += dt;

        // Check for wave progression
        if (currentWave <= 8 && totalKills >= requiredKills[currentWave - 1]) {
            player.maxHealth = (int)(player.maxHealth * 1.2f);
            player.health = player.maxHealth;
            playerDamage *= 1.1f;
            currentWave++;
            // Increase spawn speed
            smallEnemySpawnInterval *= 0.9f;
            bigEnemySpawnInterval *= 0.9f;
            minuteTimer = 0.0f; // Reset minute timer
            if (currentWave > 8 && !fadingOut) {
                targetState = GameState::StartScreen;
                fadingOut = true;
            }
        }

        // Check for minute-based spawn speed increase
        if (minuteTimer >= 60.0f) {
            smallEnemySpawnInterval *= 0.9f;
            bigEnemySpawnInterval *= 0.9f;
            minuteTimer = 0.0f; // Reset minute timer
        }

        // Handle spawning
        smallEnemySpawnTimer += dt;
        bigEnemySpawnTimer += dt;

        if (smallEnemySpawnTimer >= smallEnemySpawnInterval) {
            smallEnemySpawnTimer = 0.0f;
            for (auto pos : goblinSpawners) {
                for (int i = 0; i < 2; i++) { // Spawn 2 goblins
                    Enemy* e = GetEnemyFromPool(pos, EnemyType::Goblin);
                    if (e) enemies.push_back(e);
                }
            }
            for (auto pos : impSpawners) {
                for (int i = 0; i < 2; i++) { // Spawn 2 imps
                    Enemy* e = GetEnemyFromPool(pos, EnemyType::Imp);
                    if (e) enemies.push_back(e);
                }
            }
        }

        if (bigEnemySpawnTimer >= bigEnemySpawnInterval) {
            bigEnemySpawnTimer = 0.0f;
            for (auto pos : bigZombieSpawners) {
                Enemy* e = GetEnemyFromPool(pos, EnemyType::BigZombie);
                if (e) enemies.push_back(e);
            }
            for (auto pos : bigDemonSpawners) {
                Enemy* e = GetEnemyFromPool(pos, EnemyType::BigDemon);
                if (e) enemies.push_back(e);
            }
        }
    }
}

void GameRender() {
    // Draw to internal 320x180 canvas
    BeginTextureMode(target);
    ClearBackground(BLACK);

    if (gameState == GameState::StartScreen) {
        // Draw start screen
        if (startScreen.id != 0) {
            float scaleX = (float)320 / startScreen.width;
            float scaleY = (float)180 / startScreen.height;
            float scale = fmin(scaleX, scaleY);
            float destWidth = startScreen.width * scale;
            float destHeight = startScreen.height * scale;
            float offsetX = (320 - destWidth) / 2;
            float offsetY = (180 - destHeight) / 2;

            DrawTexturePro(
                startScreen,
                {0, 0, (float)startScreen.width, (float)startScreen.height},
                {offsetX, offsetY, destWidth, destHeight},
                {0, 0}, 0.0f, WHITE
            );
        }
    } else if (gameState == GameState::Playing) {
        BeginMode2D(camera);
        DrawTMX(currentMap, &camera, 0, 0, WHITE);
        player.Draw();
        for (auto& s : slashes) s.Draw();
        for (auto& e : enemies) e->Draw();
        EndMode2D();

        // Draw health bar after camera mode (in screen space)
        player.DrawHealthBar();

        // Draw wave info
        if (currentWave <= 8) {
            DrawText(TextFormat("WAVE %d", currentWave), 320 - 100, 10, 20, WHITE);
            DrawText(TextFormat("KILLS: %d", totalKills), 320 - 100, 35, 10, WHITE);
        }
    }

    // Draw fade overlay
    if (fadeAlpha > 0.0f) {
        DrawRectangle(0, 0, 320, 180, Fade(BLACK, fadeAlpha));
    }

    EndTextureMode();

    // Now draw the 320x180 canvas to the real window, scaled up
    BeginDrawing();
    ClearBackground(BLACK);

    float scaleX = (float)GetScreenWidth() / 320;
    float scaleY = (float)GetScreenHeight() / 180;
    float scale = fmin(scaleX, scaleY); // keep aspect ratio

    float finalWidth = 320 * scale;
    float finalHeight = 180 * scale;
    float offsetX = (GetScreenWidth() - finalWidth) / 2;
    float offsetY = (GetScreenHeight() - finalHeight) / 2;

    DrawTexturePro(
        target.texture,
        {0, 0, (float)target.texture.width, -(float)target.texture.height}, // flip Y
        {offsetX, offsetY, finalWidth, finalHeight},
        {0, 0}, 0, WHITE
    );

    EndDrawing();
}

// frees all resources
void GameShutdown() {
    player.Unload();
    Slash::UnloadAssets();
    UnloadEnemyTextures();
    for (auto& e : enemyPool) delete e;
    enemyPool.clear();
    enemies.clear();
    UnloadTexture(startScreen); // Unload start screen texture
    UnloadTMX(currentMap); // Free the TMX map
    UnloadRenderTexture(target);
    CloseWindow();
}

// Main 
int main() {
    InitWindow(1280, 720, "Raylib Player Example");
    printf("Hello, I have been initialized");
    SetTargetFPS(60);
    GameStartup();

    while (!WindowShouldClose()) {
        GameUpdate();
        GameRender();
    }

    GameShutdown();
    return 0;
}