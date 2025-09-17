#include <raylib.h> // game header
#include "raymath.h" // controlling vectors
#include <string> // for frame names
#include <cmath> // for fmin
#include <vector>
#include <algorithm>
#include <bits/stdc++.h>

#define RAYTMX_IMPLEMENTATION
#include "raytmx.h"

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
};
Player player; // creates player
enum class EnemyState {
    Patrol,
    Chase
};

class Enemy {
protected:
    Vector2 position;
    Vector2 spawnPos;
    Texture2D runFrames[4];
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

public:
    Enemy(Vector2 pos, const char* spritePrefix, int hp, int dmg, float spd, float range);
    virtual ~Enemy();

    virtual void Update(float dt, const Player& player);
    virtual void Draw();
    virtual void TakeDamage(int dmg, Vector2 hitDirection);;

    bool IsAlive() const;
    int GetDamage() const;
    Vector2 GetPosition() const;
};


std::vector<Enemy*> enemies;

class Goblin : public Enemy {
public:
    Goblin(Vector2 pos);
};

class Imp : public Enemy {
public:
    Imp(Vector2 pos);
};

class BigZombie : public Enemy {
public:
    BigZombie(Vector2 pos);
};

class BigDemon : public Enemy {
public:
    BigDemon(Vector2 pos);
};

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
    // thanks 0x72 for making very good sprite names
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

// Enemy implementations
Enemy::Enemy(Vector2 pos, const char* spritePrefix, int hp, int dmg, float spd, float range)
    : position(pos), spawnPos(pos), currentFrame(0), frameTimer(0.0f), frameTime(0.15f), speed(spd),
      health(hp), damage(dmg), alive(true), state(EnemyState::Patrol), detectRange(range),
      target(pos), facingRight(true), knockbackVelocity({0,0}), knockbackTimer(0.0f)
{
    for (int i = 0; i < 4; i++) {
        std::string path = "assets/Enemies/" + std::string(spritePrefix) + "_run_anim_f" + std::to_string(i) + ".png";
        TraceLog(LOG_INFO, "Loading enemy frame: %s", path.c_str());
        Texture2D texture = LoadTexture(path.c_str());
        if (texture.id == 0) {
            TraceLog(LOG_WARNING, "Failed to load texture: %s", path.c_str());
            runFrames[i] = {0};
        } else {
            runFrames[i] = texture;
        }
    }
}


Enemy::~Enemy() {
    for (int i = 0; i < 4; i++) UnloadTexture(runFrames[i]);
}

void Enemy::Update(float dt, const Player& player) {
    if (!alive) return;

    // If under knockback, apply it and reduce timer
    if (knockbackTimer > 0.0f) {
        position = Vector2Add(position, Vector2Scale(knockbackVelocity, dt * 60));
        knockbackTimer -= dt;
        if (knockbackTimer <= 0.0f) {
            knockbackVelocity = {0, 0};
        }
        return; // Skip normal AI while knocked back
    }

    // Check distance to player
    float dist = Vector2Distance(position, player.pos);
    if (dist < detectRange) {
        state = EnemyState::Chase;
        target = player.pos;
    } else {
        state = EnemyState::Patrol;
        if (Vector2Distance(position, target) < 5.0f) {
            target.x = GetRandomValue((int)spawnPos.x - 100, (int)spawnPos.x + 100);
            target.y = GetRandomValue((int)spawnPos.y - 100, (int)spawnPos.y + 100);
        }
    }

    // Move towards target
    Vector2 dir = Vector2Normalize(Vector2Subtract(target, position));
    position = Vector2Add(position, Vector2Scale(dir, speed * dt * 60));

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
        alive = false;
        return;
    }

    // Apply knockback
    Vector2 dirFromPlayer = Vector2Normalize(Vector2Subtract(position, player.pos)); 
    knockbackVelocity = Vector2Scale(dirFromPlayer, 2.5f); // strength of knockback
    knockbackTimer = 0.15f; // knockback duration in seconds
}


bool Enemy::IsAlive() const { return alive; }
int Enemy::GetDamage() const { return damage; }
Vector2 Enemy::GetPosition() const { return position; }

// Subclasses
Goblin::Goblin(Vector2 pos)
    : Enemy(pos, "goblin", 30, 5, 1.5f, 80.0f) {}

Imp::Imp(Vector2 pos)
    : Enemy(pos, "imp", 20, 3, 2.0f, 90.0f) {}

BigZombie::BigZombie(Vector2 pos)
    : Enemy(pos, "big_zombie", 50, 10, 1.2f, 100.0f) {}

BigDemon::BigDemon(Vector2 pos)
    : Enemy(pos, "big_demon", 80, 15, 1.3f, 120.0f) {}

// Globals

Camera2D camera; // creates camera

Texture2D tilemap;

RenderTexture2D target; // Camera size
bool fullscreen = false;

// Functions 

// Defining everything for the game
void GameStartup() {
    printf("Hello");
    //TmxMap* 
    currentMap = LoadTMX("assets/Tilemap/First.tmx");
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

    // Load enemies from object layer
    for (uint32_t i = 0; i < currentMap->layersLength; i++) {
        TmxLayer& layer = currentMap->layers[i];
        if (layer.type == LAYER_TYPE_OBJECT_GROUP && layer.name && strcmp(layer.name, "Enemy") == 0 && layer.exact.objectGroup.objects) {
            for (uint32_t j = 0; j < layer.exact.objectGroup.objectsLength; j++) {
                TmxObject* obj = &layer.exact.objectGroup.objects[j];
                if (obj->name) {
                    if (strcmp(obj->name, "goblin") == 0)
                        enemies.push_back(new Goblin({(float)obj->x, (float)obj->y}));
                    else if (strcmp(obj->name, "imp") == 0)
                        enemies.push_back(new Imp({(float)obj->x, (float)obj->y}));
                    else if (strcmp(obj->name, "big_demon") == 0)
                        enemies.push_back(new BigDemon({(float)obj->x, (float)obj->y}));
                    else if (strcmp(obj->name, "big_zombie") == 0)
                        enemies.push_back(new BigZombie({(float)obj->x, (float)obj->y}));
                }
            }
        }
    }

    // Default player position
    player.pos = {160, 90};

    // Window setup
    

    target = LoadRenderTexture(320, 180);
    SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

    //tilemap = LoadTexture("assets/Tilemap/First.png");

    player.Load();
    Slash::LoadAssets();

    camera.target = player.pos;
    camera.offset = {320.0f / 2, 180.0f / 2};
    camera.zoom = 1.0f;

    SetExitKey(KEY_F1);
}

// Updates things every frame
void GameUpdate() {
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

    if (IsKeyDown(KEY_ESCAPE)){
        // hold down to quit after 3 seconds
    }

    player.Update();
    camera.target = player.pos;

    // Update slashes
    for (auto& s : slashes) s.Update(GetFrameTime());
    for (auto& s : slashes) {
        Rectangle hitbox = s.GetHitbox();
        for (auto& e : enemies) {
            if (CheckCollisionPointRec(e->GetPosition(), hitbox)) {
                e->TakeDamage(10, Vector2Normalize(Vector2Subtract(e->GetPosition(), player.pos))); // player deals 10 damage
            }
        }
    }
    // Remove finished ones
    slashes.erase(
        std::remove_if(slashes.begin(), slashes.end(),
                [](Slash& s){ return s.finished; }),
        slashes.end()
    );

    for (auto& e : enemies) e->Update(GetFrameTime(), player);

    enemies.erase(
        std::remove_if(enemies.begin(), enemies.end(),
                   [](Enemy* e) { return !e->IsAlive(); }),
        enemies.end()
    );
}

void GameRender() {
    // Draw to internal 320x180 canvas
    BeginTextureMode(target);
    ClearBackground(BLACK);

    BeginMode2D(camera);
    //DrawTexture(tilemap, 0, 0, WHITE);
    DrawTMX(currentMap, &camera, 0, 0, WHITE);
    player.Draw();
    for (auto& s : slashes) s.Draw();
    for (auto& e : enemies) e->Draw();
    EndMode2D();

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
    for (auto& e : enemies) delete e;
    enemies.clear();
    //UnloadTexture(tilemap);
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