
#include "raylib.h"
#include <cmath>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <ctime>

enum GameScreen
{
    MENU,
    SETTINGS,
    GAME,
    PAUSED,
    RESULTS
};

struct Target
{
    Vector3 position;
    Vector3 startPosition;
    float phase;
    float radius;
};

struct Resolution
{
    int width;
    int height;
};

const int TARGET_COUNT = 6;
Target targets[TARGET_COUNT];
GameScreen currentScreen = MENU;
Camera3D camera = {};

Resolution resolutions[] =
{
    { 1280, 720 },
    { 1600, 900 },
    { 1920, 1080 }
};

int resolutionIndex = 1;
bool fullscreen = false;

float mouseSensitivity = 1.0f;
float verticalSensitivityMultiplier = 0.35f;
int crosshairStyle = 1;

bool targetMovementEnabled = true;
float targetMovementSpeed = 5.0f;

bool timerEnabled = true;
float gameDuration = 60.0f;

bool benchmarkMode = false;
int fpsLimit = 144;
int graphicsQuality = 2;

int score = 0;
int hits = 0;
int misses = 0;

float remainingTime = 60.0f;
float gameElapsed = 0.0f;

float cameraYaw = 0.0f;
float cameraPitch = 0.0f;

bool mouseInitialized = false;

// ============================================================
// RANDOM HELPERS
// ============================================================

float RandomFloat(float min, float max)
{
    return min + (float)GetRandomValue(0, 10000) / 10000.0f
        * (max - min);
}

// ============================================================
// MOUSE AND SCREEN MANAGEMENT
// ============================================================

void SetGameScreen(GameScreen newScreen)
{
    if (currentScreen == newScreen)
        return;

    currentScreen = newScreen;

    if (currentScreen == GAME)
    {
        DisableCursor();
        GetMouseDelta();
    }
    else
    {
        EnableCursor();
    }
}

void InitializeMouse()
{
    if (!mouseInitialized)
    {
        EnableCursor();
        mouseInitialized = true;
    }
}

void MoveWindowToMonitorOne()
{
    Vector2 monitorPosition = GetMonitorPosition(0);

    SetWindowPosition(
        (int)monitorPosition.x + 50,
        (int)monitorPosition.y + 50
    );
}

void ToggleFullscreenOnMonitorOne()
{
    bool wasPlaying = (currentScreen == GAME);

    if (wasPlaying)
        EnableCursor();

    if (!fullscreen)
        MoveWindowToMonitorOne();

    ToggleFullscreen();

    fullscreen = IsWindowFullscreen();

    GetMouseDelta();

    if (wasPlaying)
    {
        DisableCursor();
        GetMouseDelta();
    }
}

// ============================================================
// CAMERA
// ============================================================

void UpdateCamera()
{
    Vector2 mouse = GetMouseDelta();

    float horizontalMovement =
        mouse.x * mouseSensitivity * 0.0025f;

    float verticalMovement =
        mouse.y * verticalSensitivityMultiplier * 0.0025f;

    cameraYaw += horizontalMovement;
    cameraPitch -= verticalMovement;

    cameraPitch = std::clamp(cameraPitch, -1.45f, 1.45f);

    camera.position = { 0.0f, 2.0f, 0.0f };

    camera.target.x = sinf(cameraYaw) * cosf(cameraPitch);
    camera.target.y = sinf(cameraPitch);
    camera.target.z = -cosf(cameraYaw) * cosf(cameraPitch);

    camera.target.x *= 20.0f;
    camera.target.y = 2.0f + camera.target.y * 20.0f;
    camera.target.z *= 20.0f;

    camera.up = { 0.0f, 1.0f, 0.0f };
    camera.fovy = 75.0f;
    camera.projection = CAMERA_PERSPECTIVE;
}

// ============================================================
// TARGETS
// ============================================================

void ResetTarget(int index)
{
    targets[index].startPosition =
    {
        RandomFloat(-7.0f, 7.0f),
        RandomFloat(1.0f, 7.0f),
        -18.0f
    };

    targets[index].position = targets[index].startPosition;
    targets[index].phase = RandomFloat(0.0f, 6.28f);
    targets[index].radius = 0.65f;
}

void InitializeTargets()
{
    for (int i = 0; i < TARGET_COUNT; i++)
        ResetTarget(i);
}

void UpdateTargets(float deltaTime)
{
    if (!targetMovementEnabled)
    {
        for (int i = 0; i < TARGET_COUNT; i++)
            targets[i].position = targets[i].startPosition;

        return;
    }

    float time = (float)GetTime() * targetMovementSpeed;

    for (int i = 0; i < TARGET_COUNT; i++)
    {
        float phase = targets[i].phase;

        targets[i].position.x =
            targets[i].startPosition.x +
            sinf(time + phase) * 2.5f;

        targets[i].position.y =
            targets[i].startPosition.y +
            cosf(time * 0.8f + phase) * 1.5f;

        targets[i].position.z =
            targets[i].startPosition.z;
    }
}

// ============================================================
// GAME MANAGEMENT
// ============================================================

void StartGame()
{
    score = 0;
    hits = 0;
    misses = 0;

    remainingTime = gameDuration;
    gameElapsed = 0.0f;

    cameraYaw = 0.0f;
    cameraPitch = 0.0f;

    InitializeTargets();
    UpdateCamera();

    SetGameScreen(GAME);
}

float GetAccuracy()
{
    int totalShots = hits + misses;

    if (totalShots == 0)
        return 0.0f;

    return (float)hits / (float)totalShots * 100.0f;
}

void FinishGame()
{
    SetGameScreen(RESULTS);
}

void ShootTarget()
{
    Vector2 screenCenter =
    {
        (float)GetScreenWidth() / 2.0f,
        (float)GetScreenHeight() / 2.0f
    };

    Ray ray = GetMouseRay(screenCenter, camera);

    bool hitTarget = false;

    for (int i = 0; i < TARGET_COUNT; i++)
    {
        RayCollision collision =
            GetRayCollisionSphere(
                ray,
                targets[i].position,
                targets[i].radius
            );

        if (collision.hit)
        {
            hits++;
            score += 100;

            ResetTarget(i);

            hitTarget = true;
            break;
        }
    }

    if (!hitTarget)
        misses++;
}

// ============================================================
// UI HELPERS
// ============================================================

bool Button(const char* text, Rectangle bounds)
{
    Vector2 mouse = GetMousePosition();

    bool hovered = CheckCollisionPointRec(mouse, bounds);

    Color color = hovered
        ? Color{ 75, 125, 210, 255 }
        : Color{ 45, 55, 75, 255 };

    DrawRectangleRec(bounds, color);
    DrawRectangleLinesEx(bounds, 2, WHITE);

    int textWidth = MeasureText(text, 22);

    DrawText(
        text,
        (int)(bounds.x + bounds.width / 2 - textWidth / 2),
        (int)(bounds.y + bounds.height / 2 - 11),
        22,
        WHITE
    );

    return hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

void DrawSlider(
    const char* label,
    float* value,
    float minimum,
    float maximum,
    Rectangle bounds
)
{
    DrawText(label, (int)bounds.x, (int)bounds.y - 32, 20, WHITE);

    DrawRectangleRec(bounds, DARKGRAY);

    float percentage =
        (*value - minimum) / (maximum - minimum);

    percentage = std::clamp(percentage, 0.0f, 1.0f);

    Rectangle filled =
    {
        bounds.x,
        bounds.y,
        bounds.width * percentage,
        bounds.height
    };

    DrawRectangleRec(filled, SKYBLUE);

    Vector2 mouse = GetMousePosition();

    bool hovering =
        CheckCollisionPointRec(mouse, bounds);

    if (hovering && IsMouseButtonDown(MOUSE_BUTTON_LEFT))
    {
        float position =
            (mouse.x - bounds.x) / bounds.width;

        position = std::clamp(position, 0.0f, 1.0f);

        *value = minimum + position * (maximum - minimum);
    }

    DrawCircle(
        (int)(bounds.x + bounds.width * percentage),
        (int)(bounds.y + bounds.height / 2),
        10,
        WHITE
    );
}

void DrawTitle()
{
    const char* title = "AIM TRAINER 3D";

    int width = MeasureText(title, 45);

    DrawText(
        title,
        GetScreenWidth() / 2 - width / 2,
        55,
        45,
        WHITE
    );
}

// ============================================================
// CROSSHAIR
// ============================================================

void DrawCrosshair()
{
    int centerX = GetScreenWidth() / 2;
    int centerY = GetScreenHeight() / 2;

    if (crosshairStyle == 0)
        return;

    if (crosshairStyle == 1)
    {
        DrawLine(
            centerX - 10, centerY,
            centerX + 10, centerY,
            WHITE
        );

        DrawLine(
            centerX, centerY - 10,
            centerX, centerY + 10,
            WHITE
        );

        DrawCircleLines(centerX, centerY, 3, WHITE);
    }
    else if (crosshairStyle == 2)
    {
        DrawCircle(centerX, centerY, 3, WHITE);
    }
    else if (crosshairStyle == 3)
    {
        DrawCircleLines(centerX, centerY, 9, WHITE);
    }
}

// ============================================================
// GAME WORLD
// ============================================================

void DrawWorld()
{
    const Color floorColor = { 20, 22, 29, 255 };
    const Color wallColor = { 25, 27, 34, 255 };
    const Color panelColor = { 34, 36, 44, 255 };
    const Color darkMetal = { 11, 13, 18, 255 };
    const Color redGlow = { 210, 22, 38, 255 };
    const Color redHot = { 255, 55, 65, 255 };
    const Color floorLine = { 75, 78, 88, 255 };

    BeginMode3D(camera);

    // Main floor, walls, back wall and ceiling.
    DrawCubeV(
        { 0.0f, -1.25f, -16.0f },
        { 34.0f, 0.5f, 46.0f },
        floorColor
    );

    DrawCubeV(
        { -17.0f, 5.0f, -16.0f },
        { 0.6f, 12.0f, 46.0f },
        wallColor
    );

    DrawCubeV(
        { 17.0f, 5.0f, -16.0f },
        { 0.6f, 12.0f, 46.0f },
        wallColor
    );

    DrawCubeV(
        { 0.0f, 11.2f, -16.0f },
        { 34.0f, 0.5f, 46.0f },
        darkMetal
    );

    DrawCubeV(
        { 0.0f, 5.0f, -39.0f },
        { 34.0f, 12.0f, 0.7f },
        wallColor
    );

    // Wall panels and metal ribs.
    for (int i = 0; i < 6; i++)
    {
        float z = -3.0f - i * 6.0f;

        DrawCubeV(
            { -16.62f, 5.0f, z },
            { 0.12f, 9.8f, 4.8f },
            panelColor
        );

        DrawCubeV(
            { 16.62f, 5.0f, z },
            { 0.12f, 9.8f, 4.8f },
            panelColor
        );

        DrawCubeV(
            { -16.48f, 5.0f, z - 2.5f },
            { 0.18f, 11.0f, 0.22f },
            darkMetal
        );

        DrawCubeV(
            { 16.48f, 5.0f, z - 2.5f },
            { 0.18f, 11.0f, 0.22f },
            darkMetal
        );
    }

    // Back wall panels.
    for (int i = 0; i < 5; i++)
    {
        float x = -13.5f + i * 6.75f;

        DrawCubeV(
            { x, 5.0f, -38.58f },
            { 6.3f, 9.8f, 0.12f },
            panelColor
        );

        DrawCubeV(
            { x, 10.2f, -38.42f },
            { 6.3f, 0.12f, 0.16f },
            darkMetal
        );
    }

    // Floor lane markings.
    DrawCubeV(
        { -11.0f, -0.98f, -16.0f },
        { 0.045f, 0.025f, 43.0f },
        floorLine
    );

    DrawCubeV(
        { 11.0f, -0.98f, -16.0f },
        { 0.045f, 0.025f, 43.0f },
        floorLine
    );

    for (int i = 0; i < 8; i++)
    {
        float z = -2.0f - i * 4.8f;

        DrawCubeV(
            { 0.0f, -0.98f, z },
            { 22.0f, 0.025f, 0.025f },
            Color{ 48, 51, 60, 255 }
        );
    }

    // Red neon strips along the side walls.
    DrawCubeV(
        { -16.55f, 8.9f, -16.0f },
        { 0.08f, 0.07f, 43.0f },
        redGlow
    );

    DrawCubeV(
        { 16.55f, 8.9f, -16.0f },
        { 0.08f, 0.07f, 43.0f },
        redGlow
    );

    DrawCubeV(
        { -16.48f, 8.9f, -16.0f },
        { 0.035f, 0.025f, 43.0f },
        redHot
    );

    DrawCubeV(
        { 16.48f, 8.9f, -16.0f },
        { 0.035f, 0.025f, 43.0f },
        redHot
    );

    // Ceiling lighting.
    for (int i = 0; i < 5; i++)
    {
        float z = -4.0f - i * 7.0f;

        DrawCubeV(
            { 0.0f, 10.88f, z },
            { 5.0f, 0.12f, 1.0f },
            darkMetal
        );

        DrawCubeV(
            { 0.0f, 10.80f, z },
            { 3.8f, 0.035f, 0.12f },
            redGlow
        );

        DrawCubeV(
            { 0.0f, 10.77f, z },
            { 2.2f, 0.025f, 0.045f },
            redHot
        );
    }

    // Structural ceiling beams.
    for (int i = 0; i < 5; i++)
    {
        float z = -5.0f - i * 7.0f;

        DrawCubeV(
            { 0.0f, 10.65f, z },
            { 33.0f, 0.22f, 0.25f },
            darkMetal
        );
    }

    // Layered bullseye targets.
    for (int i = 0; i < TARGET_COUNT; i++)
    {
        Vector3 p = targets[i].position;

        DrawCylinderEx(
            { p.x, p.y, p.z + 0.12f },
            { p.x, p.y, p.z + 0.04f },
            targets[i].radius + 0.08f,
            targets[i].radius + 0.08f,
            48,
            Color{ 35, 7, 12, 255 }
        );

        DrawCylinderEx(
            { p.x, p.y, p.z + 0.20f },
            { p.x, p.y, p.z + 0.13f },
            targets[i].radius * 0.78f,
            targets[i].radius * 0.78f,
            48,
            Color{ 180, 18, 32, 255 }
        );

        DrawCylinderEx(
            { p.x, p.y, p.z + 0.28f },
            { p.x, p.y, p.z + 0.21f },
            targets[i].radius * 0.50f,
            targets[i].radius * 0.50f,
            48,
            Color{ 55, 8, 16, 255 }
        );

        DrawCylinderEx(
            { p.x, p.y, p.z + 0.36f },
            { p.x, p.y, p.z + 0.29f },
            targets[i].radius * 0.23f,
            targets[i].radius * 0.23f,
            48,
            redHot
        );

        DrawSphere(
            { p.x, p.y, p.z + 0.39f },
            targets[i].radius * 0.07f,
            Color{ 255, 145, 150, 255 }
        );
    }

    EndMode3D();
}

// ============================================================
// HUD
// ============================================================

void DrawHUD()
{
    DrawRectangle(15, 15, 230, 115, Fade(BLACK, 0.65f));

    DrawText(
        TextFormat("SCORE: %i", score),
        30, 25, 24, WHITE
    );

    DrawText(
        TextFormat("HITS: %i", hits),
        30, 55, 20, GREEN
    );

    DrawText(
        TextFormat("MISSES: %i", misses),
        130, 55, 20, RED
    );

    DrawText(
        TextFormat("ACCURACY: %.1f%%", GetAccuracy()),
        30, 90, 20, WHITE
    );

    if (timerEnabled)
    {
        DrawRectangle(
            GetScreenWidth() - 190,
            15,
            175,
            55,
            Fade(BLACK, 0.65f)
        );

        DrawText(
            TextFormat("TIME: %.1f", remainingTime),
            GetScreenWidth() - 175,
            30,
            25,
            WHITE
        );
    }

    DrawText(
        TextFormat("FPS: %i", GetFPS()),
        GetScreenWidth() - 100,
        GetScreenHeight() - 35,
        20,
        GREEN
    );
}

// ============================================================
// MENU
// ============================================================

bool RedMenuButton(const char* text, Rectangle bounds)
{
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);

    Color background = hovered
        ? Color{ 95, 15, 25, 255 }
        : Color{ 22, 24, 31, 255 };

    Color border = hovered
        ? Color{ 255, 45, 60, 255 }
        : Color{ 85, 35, 43, 255 };

    DrawRectangleRec(bounds, background);
    DrawRectangleLinesEx(bounds, 1.5f, border);

    if (hovered)
    {
        DrawRectangle(
            (int)bounds.x,
            (int)bounds.y,
            4,
            (int)bounds.height,
            RED
        );
    }

    int textWidth = MeasureText(text, 22);

    DrawText(
        text,
        (int)(bounds.x + bounds.width / 2 - textWidth / 2),
        (int)(bounds.y + bounds.height / 2 - 11),
        22,
        hovered ? WHITE : LIGHTGRAY
    );

    return hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}


void DrawMenu()
{
    ClearBackground(Color{ 9, 10, 15, 255 });

    int width = GetScreenWidth();
    int height = GetScreenHeight();

    // Background panels
    DrawRectangleGradientV(
        0, 0, width, height,
        Color{ 20, 17, 23, 255 },
        Color{ 7, 8, 12, 255 }
    );

    // Red accent lighting
    DrawRectangle(0, 0, width, 4, RED);
    DrawRectangle(0, height - 4, width, 4, RED);

    DrawRectangle(
        width - 480, 0, 480, height,
        Color{ 15, 15, 21, 255 }
    );

    DrawRectangle(
        width - 482, 0, 2, height,
        Color{ 105, 20, 30, 255 }
    );

    // Decorative red lines
    for (int i = 0; i < 8; i++)
    {
        int y = 100 + i * 85;

        DrawLine(
            40, y,
            width - 520, y,
            Color{ 32, 25, 31, 255 }
        );
    }

    // Main title
    DrawText(
        "AIM",
        75, 155, 82,
        WHITE
    );

    DrawText(
        "TRAINER",
        75, 235, 82,
        RED
    );

    DrawText(
        "3D",
        75, 325, 42,
        LIGHTGRAY
    );

    DrawRectangle(75, 390, 100, 4, RED);

    DrawText(
        "PRECISION  //  SPEED  //  ACCURACY",
        75, 415, 18,
        GRAY
    );

    DrawText(
        "FIRST PERSON AIM TRAINING",
        75, height - 75, 16,
        DARKGRAY
    );

    // Menu heading
    int menuX = width - 410;

    DrawText(
        "MAIN MENU",
        menuX, 115, 30,
        WHITE
    );

    DrawText(
        "SELECT AN OPTION",
        menuX, 155, 15,
        GRAY
    );

    DrawRectangle(menuX, 185, 330, 2, RED);

    // Buttons
    Rectangle playButton =
    {
        (float)menuX, 235, 330, 62
    };

    Rectangle settingsButton =
    {
        (float)menuX, 320, 330, 62
    };

    Rectangle exitButton =
    {
        (float)menuX, 405, 330, 62
    };

    if (RedMenuButton("PLAY", playButton))
        StartGame();

    if (RedMenuButton("SETTINGS", settingsButton))
        SetGameScreen(SETTINGS);

    if (RedMenuButton("EXIT GAME", exitButton))
        CloseWindow();

    DrawText(
        "VERSION 1.0",
        menuX, height - 65, 14,
        DARKGRAY
    );
}


// ============================================================
// SETTINGS
// ============================================================

void DrawSettings()
{
    ClearBackground(Color{ 15, 18, 28, 255 });

    DrawTitle();

    float centerX = GetScreenWidth() / 2.0f;

    DrawText(
        "SETTINGS",
        (int)centerX - 65,
        115,
        25,
        SKYBLUE
    );

    Rectangle sensitivitySlider =
    {
        centerX - 180, 175, 360, 12
    };

    DrawSlider(
        "Horizontal mouse sensitivity",
        &mouseSensitivity,
        0.1f,
        3.0f,
        sensitivitySlider
    );

    DrawText(
        TextFormat("Sensitivity: %.2f", mouseSensitivity),
        (int)centerX - 180,
        195,
        18,
        LIGHTGRAY
    );

    DrawText(
        "Vertical sensitivity: 35% (independent)",
        (int)centerX - 180,
        220,
        18,
        LIGHTGRAY
    );

    Rectangle movementButton =
    {
        centerX - 180, 255, 360, 40
    };

    if (Button(
        targetMovementEnabled
            ? "TARGET MOVEMENT: ON"
            : "TARGET MOVEMENT: OFF",
        movementButton
    ))
    {
        targetMovementEnabled = !targetMovementEnabled;
    }

    Rectangle speedSlider =
    {
        centerX - 180, 345, 360, 12
    };

    DrawSlider(
        "Target movement speed",
        &targetMovementSpeed,
        0.0f,
        10.0f,
        speedSlider
    );

    DrawText(
        TextFormat("Target speed: %.1f / 10", targetMovementSpeed),
        (int)centerX - 180,
        365,
        18,
        LIGHTGRAY
    );

    const char* crosshairNames[] =
    {
        "OFF",
        "CLASSIC",
        "DOT",
        "CIRCLE"
    };

    Rectangle crosshairButton =
    {
        centerX - 180, 405, 360, 42
    };

    if (Button(
        TextFormat("CROSSHAIR: %s", crosshairNames[crosshairStyle]),
        crosshairButton
    ))
    {
        crosshairStyle++;

        if (crosshairStyle > 3)
            crosshairStyle = 0;
    }

    Rectangle timerButton =
    {
        centerX - 180, 465, 170, 40
    };

    if (Button(
        timerEnabled ? "TIMER: ON" : "TIMER: OFF",
        timerButton
    ))
    {
        timerEnabled = !timerEnabled;
    }

    Rectangle durationButton =
    {
        centerX + 10, 465, 170, 40
    };

    if (Button(
        TextFormat("TIME: %.0f SEC", gameDuration),
        durationButton
    ))
    {
        if (gameDuration >= 180.0f)
            gameDuration = 30.0f;
        else
            gameDuration += 30.0f;
    }

    Rectangle benchmarkButton =
    {
        centerX - 180, 515, 170, 40
    };

    if (Button(
        benchmarkMode ? "BENCHMARK: ON" : "BENCHMARK: OFF",
        benchmarkButton
    ))
    {
        benchmarkMode = !benchmarkMode;
    }

    Rectangle fpsButton =
    {
        centerX + 10, 515, 170, 40
    };

    if (Button(
        TextFormat("FPS LIMIT: %i", fpsLimit),
        fpsButton
    ))
    {
        if (fpsLimit == 60)
            fpsLimit = 120;
        else if (fpsLimit == 120)
            fpsLimit = 144;
        else if (fpsLimit == 144)
            fpsLimit = 240;
        else
            fpsLimit = 60;
    }

    const char* qualityNames[] =
    {
        "LOW", "MEDIUM", "HIGH", "ULTRA"
    };

    Rectangle graphicsButton =
    {
        centerX - 180, 565, 170, 40
    };

    if (Button(
        TextFormat("GRAPHICS: %s", qualityNames[graphicsQuality]),
        graphicsButton
    ))
    {
        graphicsQuality++;

        if (graphicsQuality > 3)
            graphicsQuality = 0;
    }

    Rectangle resolutionButton =
    {
        centerX + 10, 565, 170, 40
    };

    if (Button(
        TextFormat(
            "%i x %i",
            resolutions[resolutionIndex].width,
            resolutions[resolutionIndex].height
        ),
        resolutionButton
    ))
    {
        resolutionIndex++;

        if (resolutionIndex > 2)
            resolutionIndex = 0;

        if (!fullscreen)
        {
            SetWindowSize(
                resolutions[resolutionIndex].width,
                resolutions[resolutionIndex].height
            );
        }
    }

    Rectangle backButton =
    {
        centerX - 100, 635, 200, 45
    };

    if (Button("BACK", backButton))
        SetGameScreen(MENU);
}

// ============================================================
// GAME SCREEN
// ============================================================

void UpdateGame()
{
    float deltaTime = GetFrameTime();

    if (IsKeyPressed(KEY_ESCAPE))
    {
        SetGameScreen(PAUSED);
        return;
    }

    UpdateCamera();
    UpdateTargets(deltaTime);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        ShootTarget();

    if (timerEnabled)
    {
        remainingTime -= deltaTime;

        if (remainingTime <= 0.0f)
        {
            remainingTime = 0.0f;
            FinishGame();
        }
    }
}

void DrawGame()
{
    ClearBackground(Color{ 12, 14, 20, 255 });

    DrawWorld();
    DrawCrosshair();
    DrawHUD();
}

// ============================================================
// PAUSE MENU
// ============================================================


void DrawPaused()
{
    DrawGame();

    int width = GetScreenWidth();
    int height = GetScreenHeight();

    // Dark overlay
    DrawRectangle(
        0, 0, width, height,
        Color{ 5, 6, 10, 220 }
    );

    // Red borders
    DrawRectangle(0, 0, width, 3, RED);
    DrawRectangle(0, height - 3, width, 3, RED);

    // Central pause panel
    int panelX = width / 2 - 230;
    int panelY = height / 2 - 240;

    DrawRectangle(
        panelX, panelY,
        460, 480,
        Color{ 15, 16, 22, 255 }
    );

    DrawRectangleLinesEx(
        {
            (float)panelX,
            (float)panelY,
            460,
            480
        },
        2,
        Color{ 115, 25, 38, 255 }
    );

    // Heading
    DrawText(
        "GAME PAUSED",
        panelX + 85,
        panelY + 45,
        38,
        WHITE
    );

    DrawRectangle(
        panelX + 65,
        panelY + 105,
        330,
        3,
        RED
    );

    DrawText(
        "YOUR SESSION IS ON HOLD",
        panelX + 90,
        panelY + 125,
        15,
        GRAY
    );

    // Session statistics
    DrawText(
        TextFormat("SCORE: %i", score),
        panelX + 65,
        panelY + 170,
        20,
        WHITE
    );

    DrawText(
        TextFormat("ACCURACY: %.1f%%", GetAccuracy()),
        panelX + 235,
        panelY + 170,
        20,
        RED
    );

    // Buttons
    Rectangle resumeButton =
    {
        (float)(panelX + 65),
        (float)(panelY + 225),
        330,
        55
    };

    Rectangle restartButton =
    {
        (float)(panelX + 65),
        (float)(panelY + 295),
        330,
        55
    };

    Rectangle menuButton =
    {
        (float)(panelX + 65),
        (float)(panelY + 365),
        330,
        55
    };

    if (RedMenuButton("RESUME", resumeButton))
        SetGameScreen(GAME);

    if (RedMenuButton("RESTART", restartButton))
        StartGame();

    if (RedMenuButton("MAIN MENU", menuButton))
        SetGameScreen(MENU);
}


// ============================================================
// RESULTS
// ============================================================

void DrawResults()
{
    ClearBackground(Color{ 15, 18, 28, 255 });

    float centerX = GetScreenWidth() / 2.0f;

    DrawText(
        "SESSION COMPLETE",
        (int)centerX - 205,
        120,
        38,
        WHITE
    );

    DrawText(
        TextFormat("SCORE: %i", score),
        (int)centerX - 100,
        220,
        30,
        SKYBLUE
    );

    DrawText(
        TextFormat("HITS: %i", hits),
        (int)centerX - 100,
        275,
        25,
        GREEN
    );

    DrawText(
        TextFormat("MISSES: %i", misses),
        (int)centerX - 100,
        320,
        25,
        RED
    );

    DrawText(
        TextFormat("ACCURACY: %.1f%%", GetAccuracy()),
        (int)centerX - 100,
        365,
        25,
        WHITE
    );

    Rectangle restartButton =
    {
        centerX - 150, 445, 300, 55
    };

    Rectangle menuButton =
    {
        centerX - 150, 515, 300, 55
    };

    if (Button("PLAY AGAIN", restartButton))
        StartGame();

    if (Button("MAIN MENU", menuButton))
        SetGameScreen(MENU);
}

// ============================================================
// MAIN
// ============================================================

int main()
{
    SetRandomSeed((unsigned int)time(nullptr));

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);

    InitWindow(
        resolutions[resolutionIndex].width,
        resolutions[resolutionIndex].height,
        "Aim Trainer 3D"
    );

    SetExitKey(KEY_NULL);
    SetWindowMinSize(960, 600);
    SetTargetFPS(fpsLimit);

    InitializeMouse();

    camera.position = { 0.0f, 2.0f, 0.0f };
    camera.target = { 0.0f, 2.0f, -20.0f };
    camera.up = { 0.0f, 1.0f, 0.0f };
    camera.fovy = 75.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    InitializeTargets();

    while (!WindowShouldClose())
    {
        if (IsKeyPressed(KEY_F11))
            ToggleFullscreenOnMonitorOne();

        if (benchmarkMode)
            SetTargetFPS(0);
        else
            SetTargetFPS(fpsLimit);

        if (currentScreen == GAME)
            UpdateGame();

        BeginDrawing();

        switch (currentScreen)
        {
            case MENU:
                DrawMenu();
                break;

            case SETTINGS:
                DrawSettings();
                break;

            case GAME:
                DrawGame();
                break;

            case PAUSED:
                DrawPaused();
                break;

            case RESULTS:
                DrawResults();
                break;
        }

        EndDrawing();
    }

    EnableCursor();
    CloseWindow();

    return 0;
}
