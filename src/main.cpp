#pragma once
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


class Enemy {
protected:
    Vector2 position;
    Texture2D sprite;
    int frameWidth, frameHeight;
    int health;
    bool alive;

public:
    Enemy(Vector2 pos, const char* spritePath, int frameW, int frameH, int hp)
        : position(pos), frameWidth(frameW), frameHeight(frameH), health(hp), alive(true)
    {
        sprite = LoadTexture(spritePath);
    }

    virtual ~Enemy() {
        UnloadTexture(sprite);
    }

    virtual void Update(float dt) {
        // Default: maybe idle animation, or nothing
    }

    virtual void Draw() {
        if (!alive) return;
        Rectangle src = {0, 0, (float)frameWidth, (float)frameHeight};
        Rectangle dst = {position.x, position.y, (float)frameWidth, (float)frameHeight};
        DrawTexturePro(sprite, src, dst, {0, 0}, 0.0f, WHITE);
    }

    virtual void TakeDamage(int dmg) {
        health -= dmg;
        if (health <= 0) alive = false;
    }

    bool IsAlive() const { return alive; }
    Vector2 GetPosition() const { return position; }
};


class Slash {
public:
    static Texture2D frames[3]; // shared textures
    Vector2 position;
    Vector2 direction;  // normalized
    int frame;
    float animTimer;
    bool finished;

    static void LoadAssets() {
        for (int i = 0; i < 3; i++) {
            frames[i] = LoadTexture(TextFormat("assets/Player/slash_f%d.png", i));
        }
    }

    static void UnloadAssets() {
        for (int i = 0; i < 3; i++) {
            UnloadTexture(frames[i]);
        }
    }

    Slash(Vector2 playerPos, Vector2 dir) {
        direction = Vector2Normalize(dir);
        position = Vector2Add(playerPos, Vector2Scale(direction, 20));
        frame = 0;
        animTimer = 0.0f;
        finished = false;
    }

    void Update(float dt) {
        animTimer += dt;
        if (animTimer > 0.1f) {
            animTimer = 0.0f;
            frame++;
            if (frame >= 3) finished = true;
        }
    }

    void Draw() {
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

    Rectangle GetHitbox() {
        Texture2D tex = frames[frame];
        return {position.x - tex.width/2, position.y - tex.height/2,
                (float)tex.width, (float)tex.height};
    }
};

Texture2D Slash::frames[3]; // definition
std::vector<Slash> slashes;
std::vector<Enemy*> enemies;


// creates a state machine for the player
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

    // loads the textures from the disk, has to be unloaded later
    void Load() {
        // thanks 0x72 for making very good sprite names
        for (int i = 0; i < 4; i++) {
            idleAnim[i] = LoadTexture(("assets/Player/knight_f_idle_anim_f" + std::to_string(i) + ".png").c_str());
            runAnim[i] = LoadTexture(("assets/Player/knight_f_run_anim_f" + std::to_string(i) + ".png").c_str()); 
        }
        hitSprite = LoadTexture("assets/Player/knight_f_hit_anim_f0.png");
    }

    // unloading the textures
    void Unload() {
        for (int i = 0; i < 4; i++) {
            UnloadTexture(idleAnim[i]);
            UnloadTexture(runAnim[i]);
        }
        UnloadTexture(hitSprite);
    }

    void Update() {
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

        // Movement 
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

        if (vel.x != 0 || vel.y != 0) state = PlayerState::Run; // when moving, use the run animation
        else state = PlayerState::Idle;

        // Trigger hit
        if (IsKeyPressed(KEY_SPACE)) {
            state = PlayerState::Hit;
            hitTimer = hitDuration;
            currentFrame = 0;
        }
        if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_J)){
            // attack and animation
             Vector2 dir = {0, 0};

            // Use last velocity if moving
            if (vel.x != 0 || vel.y != 0) {
                dir = Vector2Normalize(vel);
            } else {
                // fallback to facing direction
                dir = facingRight ? Vector2{1,0} : Vector2{-1,0};
            }

            // spawn slash attack in Game class
            extern std::vector<Slash> slashes;
            slashes.push_back(Slash(pos, dir));
        }

        // applies velocity to the position
        pos.x += vel.x;
        pos.y += vel.y;
        
        // updates the frame of the animation
        frameTimer += GetFrameTime();
        if (frameTimer >= frameTime) {
            frameTimer = 0.0f;
            currentFrame++;
        }

        if (state == PlayerState::Idle && currentFrame >= 4) currentFrame = 0;
        if (state == PlayerState::Run && currentFrame >= 4) currentFrame = 0;
        if (state == PlayerState::Hit) currentFrame = 0; // single-frame sprite
    }

    // draws the sprite its currently at with its current frame from Update
    void Draw() {
        Texture2D sprite;
        if (state == PlayerState::Idle) sprite = idleAnim[currentFrame];
        else if (state == PlayerState::Run) sprite = runAnim[currentFrame];
        else sprite = hitSprite;

        Rectangle src = {0, 0, (float)sprite.width, (float)sprite.height}; /// what its drawing (source)
        if (!facingRight) src.width *= -1; // flips to left if its not facing right

        Rectangle dest = {pos.x, pos.y, (float)sprite.width, (float)sprite.height}; // draws at current position
        Vector2 origin = {sprite.width/2.0f, sprite.height/2.0f};
        DrawTexturePro(sprite, src, dest, origin, 0.0f, WHITE);
    }
};


// Globals
Player player; // creates player
Texture2D tilemap; // creates tilemap
Camera2D camera; // creates camera

RenderTexture2D target; // Camera size
bool fullscreen = false;

//  Functions 

// Defining everything for the game
void GameStartup() {
    // Window at 1280x720, internal game res 320x180
    InitWindow(1280, 720, "Raylib Player Example"); // viewport
    SetTargetFPS(60);

    target = LoadRenderTexture(320, 180);
    SetTextureFilter(target.texture, TEXTURE_FILTER_POINT); // keeps pixels sharp

    tilemap = LoadTexture("assets/Tilemap/test.png"); // loads tilemap image, png for now
    player.Load();
    Slash::LoadAssets();
    

    camera.target = player.pos; // shows the player
    camera.offset = {320/2, 180/2}; // centers the player
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

    // Remove finished ones
    slashes.erase(
        std::remove_if(slashes.begin(), slashes.end(),
                [](Slash& s){ return s.finished; }),
        slashes.end()
    );

    for (auto& e : enemies) e->Update(GetFrameTime());

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
    DrawTexture(tilemap, 0, 0, WHITE);
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
    UnloadTexture(tilemap);
    UnloadRenderTexture(target);
    CloseWindow();
}

//  Main 
int main() {
    GameStartup();

    while (!WindowShouldClose()) {
        GameUpdate();
        GameRender();
    }

    GameShutdown();
    return 0;
}
