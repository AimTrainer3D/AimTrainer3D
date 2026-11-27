#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOGDI
#define NOUSER
#include <windows.h>

#include "raylib.h"
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>

enum GameScreen
{
    MENU,
    SETTINGS,
    CROSSHAIR,
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
    float age;
};

struct Star
{
    float x, y, size, speed, alpha;
};

struct Resolution
{
    int width;
    int height;
};

struct Particle
{
    Vector3 position;
    Vector3 velocity;
    float life;
    float maxLife;
    float size;
};

const int TARGET_COUNT = 6;
const int CROSSHAIR_COUNT = 120;
const int STARS_COUNT = 150;
const int MAX_PARTICLES = 160;
const int HIGH_SCORE_SLOTS = 6;

Target targets[TARGET_COUNT];
Star stars[STARS_COUNT];
Particle particles[MAX_PARTICLES];

Resolution resolutions[] =
{
    {1280, 720},
    {1600, 900},
    {1920, 1080}
};

GameScreen currentScreen = MENU;

Camera3D camera = {};
Font gameFont = {};
bool customFontLoaded = false;

Color RED_MAIN = {255, 32, 32, 255};    // #ff2020
Color RED_DARK = {90, 0, 0, 255};
Color RED_GLOW = {255, 69, 69, 255};    // #ff4545
Color BACKGROUND = {5, 5, 5, 255};      // #050505
Color PANEL = {10, 10, 10, 255};
Color TEXT_WHITE = {255, 255, 255, 255};
Color TEXT_GREY = {170, 170, 170, 255};

float mouseSensitivity = 1.0f;
float verticalSensitivityMultiplier = 0.35f;

bool targetMovementEnabled = true;
float targetMovementSpeed = 5.0f;
float targetRadius = 0.65f;
float targetTime = 0.0f;

bool timerEnabled = true;
float gameDuration = 60.0f;

bool benchmarkMode = false;
int fpsLimit = 144;
int graphicsQuality = 2;

int score = 0;
int hits = 0;
int misses = 0;
int streak = 0;
int bestStreak = 0;

float sessionElapsed = 0.0f;
float lastHitTime = 0.0f;
float totalHitInterval = 0.0f;

float hitMarkerTimer = 0.0f;
float missFlashTimer = 0.0f;

int highScores[HIGH_SCORE_SLOTS] = {0, 0, 0, 0, 0, 0};
bool newHighScore = false;

bool soundEnabled = true;
bool audioReady = false;
Sound hitSound = {};
Sound missSound = {};

bool shouldExit = false;

float remainingTime = 60.0f;
float cameraYaw = 0.0f;
float cameraPitch = 0.0f;

int selectedCrosshair = 1;
int crosshairPage = 0;
int crosshairColorIndex = 0;

float crosshairSize = 1.0f;
float crosshairThickness = 2.0f;
float crosshairGap = 6.0f;

bool fullscreen = false;
int resolutionIndex = 1;

Color crosshairColors[] =
{
    WHITE,
    RED_MAIN,
    GREEN,
    SKYBLUE,
    YELLOW,
    MAGENTA
};

const char* crosshairColorNames[] =
{
    "WHITE", "RED", "GREEN", "CYAN", "YELLOW", "PURPLE"
};

// ============================================================
// RANDOM
// ============================================================

float RandomFloat(float minimum, float maximum)
{
    return minimum +
        (float)GetRandomValue(0, 10000) / 10000.0f *
        (maximum - minimum);
}

// ============================================================
// FONT
// ============================================================

static char LowerChar(char value)
{
    if (value >= 'A' && value <= 'Z')
        return (char)(value - 'A' + 'a');

    return value;
}

static bool NameContains(const char* text, const char* word)
{
    for (int i = 0; text[i]; i++)
    {
        int j = 0;

        while (word[j] &&
            LowerChar(text[i + j]) == LowerChar(word[j]))
            j++;

        if (!word[j])
            return true;
    }

    return false;
}

static int InterScore(const char* name)
{
    if (!NameContains(name, "inter"))
        return -1;

    if (NameContains(name, "variable"))
        return 1;

    if (NameContains(name, "black"))
        return 6;

    if (NameContains(name, "extrabold"))
        return 5;

    if (NameContains(name, "bold"))
        return 4;

    if (NameContains(name, "semibold"))
        return 3;

    return 2;
}

static bool AcceptFont(Font font)
{
    if (!IsFontValid(font))
        return false;

    gameFont = font;
    customFontLoaded = true;
    SetTextureFilter(gameFont.texture, TEXTURE_FILTER_BILINEAR);
    return true;
}

static bool TryLoadFontPath(const char* path)
{
    if (!FileExists(path))
        return false;

    return AcceptFont(LoadFontEx(path, 64, nullptr, 0));
}

static void ExeDirectory(char* out, int outSize)
{
    char path[MAX_PATH];
    DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);

    if (length == 0 || length >= MAX_PATH)
    {
        out[0] = 0;
        return;
    }

    for (int i = (int)length - 1; i >= 0; i--)
    {
        if (path[i] == '\\' || path[i] == '/')
        {
            path[i] = 0;
            break;
        }
    }

    snprintf(out, outSize, "%s", path);
}

static bool TryLoadInterFromFolder(const char* folder)
{
    char pattern[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\*.ttf", folder);

    WIN32_FIND_DATAA found;
    HANDLE handle = FindFirstFileA(pattern, &found);

    if (handle == INVALID_HANDLE_VALUE)
        return false;

    char bestPath[MAX_PATH] = {};
    int bestScore = -1;

    do
    {
        int score = InterScore(found.cFileName);

        if (score > bestScore)
        {
            snprintf(
                bestPath,
                sizeof(bestPath),
                "%s\\%s",
                folder,
                found.cFileName
            );

            bestScore = score;
        }
    }
    while (FindNextFileA(handle, &found));

    FindClose(handle);

    if (bestScore < 0)
        return false;

    return TryLoadFontPath(bestPath);
}

static unsigned char* DownloadInter(int* outSize)
{
    *outSize = 0;

    HMODULE library = LoadLibraryA("winhttp.dll");

    if (!library)
        return nullptr;

    typedef void* HttpHandle;

    auto open = (HttpHandle (WINAPI *)(LPCWSTR, DWORD, LPCWSTR, LPCWSTR, DWORD))
        GetProcAddress(library, "WinHttpOpen");

    auto connect = (HttpHandle (WINAPI *)(HttpHandle, LPCWSTR, unsigned short, DWORD))
        GetProcAddress(library, "WinHttpConnect");

    auto openRequest = (HttpHandle (WINAPI *)(
        HttpHandle, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR*, DWORD))
        GetProcAddress(library, "WinHttpOpenRequest");

    auto send = (BOOL (WINAPI *)(
        HttpHandle, LPCWSTR, DWORD, LPVOID, DWORD, DWORD, DWORD_PTR))
        GetProcAddress(library, "WinHttpSendRequest");

    auto receive = (BOOL (WINAPI *)(HttpHandle, LPVOID))
        GetProcAddress(library, "WinHttpReceiveResponse");

    auto available = (BOOL (WINAPI *)(HttpHandle, LPDWORD))
        GetProcAddress(library, "WinHttpQueryDataAvailable");

    auto read = (BOOL (WINAPI *)(HttpHandle, LPVOID, DWORD, LPDWORD))
        GetProcAddress(library, "WinHttpReadData");

    auto closeHandle = (BOOL (WINAPI *)(HttpHandle))
        GetProcAddress(library, "WinHttpCloseHandle");

    auto setTimeouts = (BOOL (WINAPI *)(HttpHandle, int, int, int, int))
        GetProcAddress(library, "WinHttpSetTimeouts");

    if (!open || !connect || !openRequest || !send || !receive ||
        !available || !read || !closeHandle)
    {
        FreeLibrary(library);
        return nullptr;
    }

    HttpHandle session = open(L"AimTrainer3D", 0, nullptr, nullptr, 0);

    if (!session)
    {
        FreeLibrary(library);
        return nullptr;
    }

    if (setTimeouts)
        setTimeouts(session, 2500, 2500, 4000, 4000);

    HttpHandle connection = connect(session, L"cdn.jsdelivr.net", 443, 0);
    HttpHandle request = nullptr;
    unsigned char* buffer = nullptr;
    int total = 0;

    if (connection)
    {
        request = openRequest(
            connection,
            L"GET",
            L"/fontsource/fonts/inter@5.2.5/latin-700-normal.ttf",
            nullptr,
            nullptr,
            nullptr,
            0x00800000
        );
    }

    if (request &&
        send(request, nullptr, 0, nullptr, 0, 0, 0) &&
        receive(request, nullptr))
    {
        for (;;)
        {
            DWORD chunk = 0;

            if (!available(request, &chunk) || chunk == 0)
                break;

            unsigned char* grown = (unsigned char*)MemRealloc(
                buffer,
                (unsigned int)(total + (int)chunk)
            );

            if (!grown)
            {
                MemFree(buffer);
                buffer = nullptr;
                total = 0;
                break;
            }

            buffer = grown;

            DWORD got = 0;

            if (!read(request, buffer + total, chunk, &got) || got == 0)
                break;

            total += (int)got;

            if (total > 2 * 1024 * 1024)
            {
                MemFree(buffer);
                buffer = nullptr;
                total = 0;
                break;
            }
        }
    }

    if (request)
        closeHandle(request);

    if (connection)
        closeHandle(connection);

    closeHandle(session);
    FreeLibrary(library);

    if (!buffer || total < 1000 || buffer[0] == '<')
    {
        MemFree(buffer);
        return nullptr;
    }

    *outSize = total;
    return buffer;
}

void LoadGameFont()
{
    char exeDir[MAX_PATH];
    char cachePath[MAX_PATH];

    ExeDirectory(exeDir, MAX_PATH);

    if (exeDir[0])
        snprintf(cachePath, sizeof(cachePath), "%s\\inter.ttf", exeDir);
    else
        snprintf(cachePath, sizeof(cachePath), "inter.ttf");

    if (TryLoadFontPath(cachePath))
        return;

    char localFonts[MAX_PATH];
    DWORD length = ExpandEnvironmentStringsA(
        "%LOCALAPPDATA%\\Microsoft\\Windows\\Fonts",
        localFonts,
        MAX_PATH
    );

    if (length > 0 && length < MAX_PATH &&
        TryLoadInterFromFolder(localFonts))
        return;

    if (TryLoadInterFromFolder("C:\\Windows\\Fonts"))
        return;

    int fontBytes = 0;
    unsigned char* fontData = DownloadInter(&fontBytes);

    if (fontData)
    {
        SaveFileData(cachePath, fontData, fontBytes);

        bool loaded = AcceptFont(
            LoadFontFromMemory(".ttf", fontData, fontBytes, 64, nullptr, 0)
        );

        MemFree(fontData);

        if (loaded)
            return;
    }

    const char* fallbacks[] =
    {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/seguisb.ttf",
        "C:/Windows/Fonts/arial.ttf"
    };

    for (int i = 0; i < 3; i++)
    {
        if (TryLoadFontPath(fallbacks[i]))
            return;
    }
}

void DrawTextCustom(
    const char* text,
    float x,
    float y,
    float size,
    Color color
)
{
    if (customFontLoaded)
    {
        DrawTextEx(
            gameFont,
            text,
            {x, y},
            size,
            1.0f,
            color
        );
    }
    else
    {
        DrawText(text, (int)x, (int)y, (int)size, color);
    }
}

float MeasureCustomText(const char* text, float size)
{
    if (customFontLoaded)
        return MeasureTextEx(gameFont, text, size, 1.0f).x;

    return (float)MeasureText(text, (int)size);
}

void DrawTextCentered(
    const char* text,
    float centerX,
    float y,
    float size,
    Color color
)
{
    DrawTextCustom(
        text,
        centerX - MeasureCustomText(text, size) / 2.0f,
        y,
        size,
        color
    );
}

// ============================================================
// STARS AND BACKGROUND
// ============================================================

void InitializeStars()
{
    for (int i = 0; i < STARS_COUNT; i++)
    {
        stars[i].x = RandomFloat(0.0f, 1.0f);
        stars[i].y = RandomFloat(0.0f, 1.0f);
        stars[i].size = RandomFloat(0.7f, 2.0f);
        stars[i].speed = RandomFloat(0.2f, 1.0f);
        stars[i].alpha = RandomFloat(80.0f, 230.0f);
    }
}

void DrawStars()
{
    int visibleStars = std::min(
        STARS_COUNT,
        STARS_COUNT * (graphicsQuality + 1) / 3
    );

    for (int i = 0; i < visibleStars; i++)
    {
        float pulse =
            (float)(sin(GetTime() * stars[i].speed + i) + 1.0) * 0.5f;

        unsigned char alpha = (unsigned char)(
            stars[i].alpha * (0.45f + pulse * 0.55f)
        );

        // White stars
        Color starColor = {255, 255, 255, alpha};

        DrawCircleV(
            {stars[i].x * GetScreenWidth(), stars[i].y * GetScreenHeight()},
            stars[i].size,
            starColor
        );
    }
}

void DrawBackground()
{
    ClearBackground(BACKGROUND);

    DrawRectangle(
        0, 0,
        GetScreenWidth(),
        GetScreenHeight(),
        BACKGROUND
    );

        DrawCircleGradient(
        {(float)(GetScreenWidth() / 2), GetScreenHeight() * 0.16f},
        520.0f,
        Color{255, 32, 32, 36},
        Color{255, 32, 32, 0}
    );

    DrawStars();

    DrawRectangle(0, 0, GetScreenWidth(), 2, RED_DARK);

    DrawRectangle(
        0,
        GetScreenHeight() - 2,
        GetScreenWidth(),
        2,
        RED_DARK
    );
}

// ============================================================
// SETTINGS FILE + HIGH SCORES
// ============================================================

int GetHighScoreSlot()
{
    int slot = (int)(gameDuration / 30.0f + 0.5f) - 1;

    return std::clamp(slot, 0, HIGH_SCORE_SLOTS - 1);
}

void SaveSettings()
{
    FILE* file = fopen("settings.cfg", "w");

    if (!file)
        return;

    fprintf(file, "mouseSensitivity %f\n", mouseSensitivity);
    fprintf(file, "verticalSensitivity %f\n", verticalSensitivityMultiplier);
    fprintf(file, "targetMovementEnabled %d\n", targetMovementEnabled ? 1 : 0);
    fprintf(file, "targetMovementSpeed %f\n", targetMovementSpeed);
    fprintf(file, "targetRadius %f\n", targetRadius);
    fprintf(file, "timerEnabled %d\n", timerEnabled ? 1 : 0);
    fprintf(file, "gameDuration %f\n", gameDuration);
    fprintf(file, "benchmarkMode %d\n", benchmarkMode ? 1 : 0);
    fprintf(file, "fpsLimit %d\n", fpsLimit);
    fprintf(file, "graphicsQuality %d\n", graphicsQuality);
    fprintf(file, "resolutionIndex %d\n", resolutionIndex);
    fprintf(file, "soundEnabled %d\n", soundEnabled ? 1 : 0);
    fprintf(file, "selectedCrosshair %d\n", selectedCrosshair);
    fprintf(file, "crosshairColorIndex %d\n", crosshairColorIndex);
    fprintf(file, "crosshairSize %f\n", crosshairSize);
    fprintf(file, "crosshairThickness %f\n", crosshairThickness);
    fprintf(file, "crosshairGap %f\n", crosshairGap);

    for (int i = 0; i < HIGH_SCORE_SLOTS; i++)
        fprintf(file, "highScore%d %d\n", i, highScores[i]);

    fclose(file);
}

void LoadSettings()
{
    FILE* file = fopen("settings.cfg", "r");

    if (!file)
        return;

    char key[64];
    float value = 0.0f;

    while (fscanf(file, "%63s %f", key, &value) == 2)
    {
        if (strncmp(key, "highScore", 9) == 0)
        {
            int slot = atoi(key + 9);

            if (slot >= 0 && slot < HIGH_SCORE_SLOTS)
                highScores[slot] = (int)value;
        }
        else if (strcmp(key, "mouseSensitivity") == 0)
            mouseSensitivity = value;
        else if (strcmp(key, "verticalSensitivity") == 0)
            verticalSensitivityMultiplier = value;
        else if (strcmp(key, "targetMovementEnabled") == 0)
            targetMovementEnabled = value > 0.5f;
        else if (strcmp(key, "targetMovementSpeed") == 0)
            targetMovementSpeed = value;
        else if (strcmp(key, "targetRadius") == 0)
            targetRadius = value;
        else if (strcmp(key, "timerEnabled") == 0)
            timerEnabled = value > 0.5f;
        else if (strcmp(key, "gameDuration") == 0)
            gameDuration = value;
        else if (strcmp(key, "benchmarkMode") == 0)
            benchmarkMode = value > 0.5f;
        else if (strcmp(key, "fpsLimit") == 0)
            fpsLimit = (int)value;
        else if (strcmp(key, "graphicsQuality") == 0)
            graphicsQuality = (int)value;
        else if (strcmp(key, "resolutionIndex") == 0)
            resolutionIndex = (int)value;
        else if (strcmp(key, "soundEnabled") == 0)
            soundEnabled = value > 0.5f;
        else if (strcmp(key, "selectedCrosshair") == 0)
            selectedCrosshair = (int)value;
        else if (strcmp(key, "crosshairColorIndex") == 0)
            crosshairColorIndex = (int)value;
        else if (strcmp(key, "crosshairSize") == 0)
            crosshairSize = value;
        else if (strcmp(key, "crosshairThickness") == 0)
            crosshairThickness = value;
        else if (strcmp(key, "crosshairGap") == 0)
            crosshairGap = value;
    }

    fclose(file);

    // Never trust a file on disk: clamp everything to valid ranges.
    mouseSensitivity = std::clamp(mouseSensitivity, 0.1f, 3.0f);
    verticalSensitivityMultiplier =
        std::clamp(verticalSensitivityMultiplier, 0.1f, 1.5f);
    targetMovementSpeed = std::clamp(targetMovementSpeed, 0.0f, 10.0f);
    targetRadius = std::clamp(targetRadius, 0.3f, 1.2f);
    gameDuration =
        std::clamp(roundf(gameDuration / 30.0f) * 30.0f, 30.0f, 180.0f);
    graphicsQuality = std::clamp(graphicsQuality, 0, 3);
    resolutionIndex = std::clamp(resolutionIndex, 0, 2);
    selectedCrosshair = std::clamp(selectedCrosshair, 0, CROSSHAIR_COUNT);
    crosshairColorIndex = std::clamp(crosshairColorIndex, 0, 5);
    crosshairSize = std::clamp(crosshairSize, 0.5f, 2.5f);
    crosshairThickness = std::clamp(crosshairThickness, 1.0f, 6.0f);
    crosshairGap = std::clamp(crosshairGap, 0.0f, 15.0f);

    if (fpsLimit != 60 && fpsLimit != 120 &&
        fpsLimit != 144 && fpsLimit != 240)
        fpsLimit = 144;
}

// ============================================================
// SOUND (generated in code, no audio files needed)
// ============================================================

Sound CreateTone(
    float frequency,
    float sweep,
    float duration,
    float volume
)
{
    const int sampleRate = 44100;
    unsigned int frameCount = (unsigned int)(sampleRate * duration);

    short* samples =
        (short*)MemAlloc((unsigned int)(frameCount * sizeof(short)));

    float phase = 0.0f;

    for (unsigned int i = 0; i < frameCount; i++)
    {
        float t = (float)i / (float)sampleRate;
        float progress = t / duration;

        phase += 2.0f * PI * (frequency + sweep * progress) / sampleRate;

        float fadeIn = std::min(1.0f, t / 0.002f);
        float fadeOut = (1.0f - progress) * (1.0f - progress);

        samples[i] =
            (short)(sinf(phase) * fadeIn * fadeOut * volume * 32000.0f);
    }

    Wave wave = {};
    wave.frameCount = frameCount;
    wave.sampleRate = sampleRate;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.data = samples;

    Sound sound = LoadSoundFromWave(wave);
    UnloadWave(wave);

    return sound;
}

void LoadGameSounds()
{
    InitAudioDevice();
    audioReady = IsAudioDeviceReady();

    if (!audioReady)
        return;

    hitSound = CreateTone(900.0f, 500.0f, 0.09f, 0.5f);
    missSound = CreateTone(180.0f, -60.0f, 0.07f, 0.35f);
}

void UnloadGameSounds()
{
    if (!audioReady)
        return;

    UnloadSound(hitSound);
    UnloadSound(missSound);
    CloseAudioDevice();
}

void PlayHitSound()
{
    if (!audioReady || !soundEnabled)
        return;

    // Pitch climbs with your streak.
    SetSoundPitch(hitSound, 1.0f + std::min(streak, 12) * 0.04f);
    PlaySound(hitSound);
}

void PlayMissSound()
{
    if (!audioReady || !soundEnabled)
        return;

    PlaySound(missSound);
}

// ============================================================
// SCREEN MANAGEMENT
// ============================================================

void SetGameScreen(GameScreen newScreen)
{
    bool wasInSettings =
        currentScreen == SETTINGS || currentScreen == CROSSHAIR;

    bool goingToSettings =
        newScreen == SETTINGS || newScreen == CROSSHAIR;

    currentScreen = newScreen;

    // Settings are saved whenever you leave the settings screens.
    if (wasInSettings && !goingToSettings)
        SaveSettings();

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

// ============================================================
// CAMERA
// ============================================================

void UpdateCamera()
{
    Vector2 mouse = GetMouseDelta();

    mouse.x = std::clamp(mouse.x, -100.0f, 100.0f);
    mouse.y = std::clamp(mouse.y, -100.0f, 100.0f);

    cameraYaw += mouse.x * mouseSensitivity * 0.0025f;

    cameraPitch -=
        mouse.y * verticalSensitivityMultiplier * 0.0025f;

    cameraPitch = std::clamp(cameraPitch, -1.45f, 1.45f);

    camera.position = {0.0f, 2.0f, 0.0f};

    const float lookDistance = 20.0f;

    Vector3 direction =
    {
        sinf(cameraYaw) * cosf(cameraPitch),
        sinf(cameraPitch),
        -cosf(cameraYaw) * cosf(cameraPitch)
    };

    camera.target =
    {
        camera.position.x + direction.x * lookDistance,
        camera.position.y + direction.y * lookDistance,
        camera.position.z + direction.z * lookDistance
    };

    camera.up = {0.0f, 1.0f, 0.0f};
    camera.fovy = 75.0f;
    camera.projection = CAMERA_PERSPECTIVE;
}

// ============================================================
// TARGETS
// ============================================================

void ResetTarget(int index)
{
    Vector3 candidate = {0.0f, 0.0f, -18.0f};

    // Try a few times to spawn away from the other targets.
    for (int attempt = 0; attempt < 30; attempt++)
    {
        candidate.x = RandomFloat(-7.0f, 7.0f);
        candidate.y = RandomFloat(1.0f, 7.0f);

        bool tooClose = false;

        for (int i = 0; i < TARGET_COUNT; i++)
        {
            if (i == index)
                continue;

            float dx = candidate.x - targets[i].startPosition.x;
            float dy = candidate.y - targets[i].startPosition.y;

            if (dx * dx + dy * dy < 3.2f * 3.2f)
            {
                tooClose = true;
                break;
            }
        }

        if (!tooClose)
            break;
    }

    targets[index].startPosition = candidate;
    targets[index].position = candidate;
    targets[index].phase = RandomFloat(0.0f, 6.28f);
    targets[index].radius = targetRadius;
    targets[index].age = 0.0f;
}

void InitializeTargets()
{
    // Park everything far away so old positions don't block spawning.
    for (int i = 0; i < TARGET_COUNT; i++)
        targets[i].startPosition = {1000.0f, 1000.0f, -18.0f};

    for (int i = 0; i < TARGET_COUNT; i++)
        ResetTarget(i);
}

void UpdateTargets(float deltaTime)
{
    for (int i = 0; i < TARGET_COUNT; i++)
        targets[i].age += deltaTime;

    if (!targetMovementEnabled)
    {
        for (int i = 0; i < TARGET_COUNT; i++)
            targets[i].position = targets[i].startPosition;

        return;
    }

    // Accumulated time (instead of GetTime) so pausing doesn't make
    // targets jump when you resume.
    targetTime += deltaTime * targetMovementSpeed;

    for (int i = 0; i < TARGET_COUNT; i++)
    {
        float phase = targets[i].phase;

        targets[i].position.x =
            targets[i].startPosition.x +
            sinf(targetTime + phase) * 2.5f;

        targets[i].position.y =
            targets[i].startPosition.y +
            cosf(targetTime * 0.8f + phase) * 1.5f;
    }
}

// ============================================================
// HIT PARTICLES
// ============================================================

void ClearParticles()
{
    for (int i = 0; i < MAX_PARTICLES; i++)
        particles[i].life = 0.0f;
}

void SpawnHitParticles(Vector3 origin)
{
    int spawned = 0;

    for (int i = 0; i < MAX_PARTICLES && spawned < 22; i++)
    {
        if (particles[i].life > 0.0f)
            continue;

        float dx = RandomFloat(-1.0f, 1.0f);
        float dy = RandomFloat(-1.0f, 1.0f);
        float dz = RandomFloat(-0.2f, 1.0f);

        float length = sqrtf(dx * dx + dy * dy + dz * dz);

        if (length < 0.05f)
        {
            dx = 0.0f;
            dy = 0.0f;
            dz = 1.0f;
            length = 1.0f;
        }

        float speed = RandomFloat(2.0f, 6.0f);

        particles[i].position = {origin.x, origin.y, origin.z + 0.3f};

        particles[i].velocity =
        {
            dx / length * speed,
            dy / length * speed,
            dz / length * speed
        };

        particles[i].maxLife = RandomFloat(0.3f, 0.6f);
        particles[i].life = particles[i].maxLife;
        particles[i].size = RandomFloat(0.05f, 0.12f);

        spawned++;
    }
}

void UpdateParticles(float deltaTime)
{
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        if (particles[i].life <= 0.0f)
            continue;

        particles[i].life -= deltaTime;

        particles[i].position.x += particles[i].velocity.x * deltaTime;
        particles[i].position.y += particles[i].velocity.y * deltaTime;
        particles[i].position.z += particles[i].velocity.z * deltaTime;

        particles[i].velocity.y -= 5.0f * deltaTime;
    }
}

void DrawParticles()
{
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        if (particles[i].life <= 0.0f)
            continue;

        float fraction = particles[i].life / particles[i].maxLife;
        float size = particles[i].size * (0.4f + 0.6f * fraction);

        DrawCubeV(
            particles[i].position,
            {size, size, size},
            Fade(RED_GLOW, fraction)
        );
    }
}

// ============================================================
// GAME MANAGEMENT
// ============================================================

float GetAccuracy()
{
    int total = hits + misses;

    if (total == 0)
        return 0.0f;

    return (float)hits / total * 100.0f;
}

int GetScoreMultiplier()
{
    // x1 to start, +1 for every 5 hits in a row, capped at x5.
    return std::min(1 + streak / 5, 5);
}

void StartGame()
{
    score = 0;
    hits = 0;
    misses = 0;
    streak = 0;
    bestStreak = 0;

    sessionElapsed = 0.0f;
    lastHitTime = 0.0f;
    totalHitInterval = 0.0f;

    hitMarkerTimer = 0.0f;
    missFlashTimer = 0.0f;
    newHighScore = false;

    ClearParticles();

    remainingTime = gameDuration;

    cameraYaw = 0.0f;
    cameraPitch = 0.0f;

    InitializeTargets();

    SetGameScreen(GAME);
}

void FinishGame()
{
    newHighScore = false;

    // High scores are tracked per session length (30s, 60s, ...).
    if (timerEnabled)
    {
        int slot = GetHighScoreSlot();

        if (score > highScores[slot])
        {
            highScores[slot] = score;
            newHighScore = true;
            SaveSettings();
        }
    }

    SetGameScreen(RESULTS);
}

void ShootTarget()
{
    Vector2 center =
    {
        GetScreenWidth() / 2.0f,
        GetScreenHeight() / 2.0f
    };

    Ray ray = GetMouseRay(center, camera);

    int hitIndex = -1;
    float closest = 1000000.0f;

    // Pick the nearest target along the ray, not just the first in the list.
    for (int i = 0; i < TARGET_COUNT; i++)
    {
        RayCollision collision =
            GetRayCollisionSphere(
                ray,
                targets[i].position,
                targets[i].radius
            );

        if (collision.hit && collision.distance < closest)
        {
            closest = collision.distance;
            hitIndex = i;
        }
    }

    if (hitIndex >= 0)
    {
        score += 100 * GetScoreMultiplier();
        hits++;

        streak++;
        bestStreak = std::max(bestStreak, streak);

        totalHitInterval += sessionElapsed - lastHitTime;
        lastHitTime = sessionElapsed;

        SpawnHitParticles(targets[hitIndex].position);
        ResetTarget(hitIndex);

        hitMarkerTimer = 0.18f;
        PlayHitSound();
    }
    else
    {
        misses++;
        streak = 0;

        missFlashTimer = 0.12f;
        PlayMissSound();
    }
}

// ============================================================
// BUTTONS
// ============================================================

bool RedButton(const char* text, Rectangle bounds)
{
    const float size = 18.0f;
    const float spacing = 1.8f;

    float textWidth = customFontLoaded
        ? MeasureTextEx(gameFont, text, size, spacing).x
        : (float)MeasureText(text, (int)size);

    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);

    float x = bounds.x + (bounds.width - textWidth) * 0.5f;
    float y = bounds.y + (bounds.height - size) * 0.5f;

    if (hovered)
        y -= 2.0f;

    Color color = hovered ? RED_MAIN : TEXT_WHITE;

    if (customFontLoaded)
    {
        DrawTextEx(
            gameFont,
            text,
            {x, y},
            size,
            spacing,
            color
        );
    }
    else
    {
        DrawText(text, (int)x, (int)y, (int)size, color);
    }

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
    static float* activeSlider = nullptr;

    DrawTextCustom(
        label,
        bounds.x,
        bounds.y - 27,
        16,
        TEXT_WHITE
    );

    Vector2 mouse = GetMousePosition();

    // Generous click area so the thin bar is easy to grab.
    Rectangle grabArea =
    {
        bounds.x - 8,
        bounds.y - 10,
        bounds.width + 16,
        bounds.height + 20
    };

    if (
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        CheckCollisionPointRec(mouse, grabArea)
    )
        activeSlider = value;

    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        activeSlider = nullptr;

    // Once grabbed, keep dragging even if the cursor leaves the bar.
    if (activeSlider == value)
    {
        float position = (mouse.x - bounds.x) / bounds.width;

        position = std::clamp(position, 0.0f, 1.0f);

        *value = minimum + position * (maximum - minimum);
    }

    DrawRectangleRec(
        bounds,
        Color{55, 55, 55, 255}
    );

    float percentage =
        (*value - minimum) / (maximum - minimum);

    percentage = std::clamp(percentage, 0.0f, 1.0f);

    DrawRectangle(
        (int)bounds.x,
        (int)bounds.y,
        (int)(bounds.width * percentage),
        (int)bounds.height,
        RED_MAIN
    );

    DrawCircle(
        (int)(bounds.x + bounds.width * percentage),
        (int)(bounds.y + bounds.height / 2),
        7,
        WHITE
    );
}

// ============================================================
// CROSSHAIR SYSTEM
// ============================================================

void DrawCrosshairShape(
    int style,
    float centerX,
    float centerY,
    float scale,
    float thickness,
    float gap,
    Color color
)
{
    if (style == 0)
        return;

    int type = (style - 1) % 12;

    float length = 8.0f * scale;
    float g = gap * scale;
    float t = thickness;

    if (type == 0)
    {
        DrawLineEx({centerX - g - length, centerY},
                   {centerX - g, centerY}, t, color);

        DrawLineEx({centerX + g, centerY},
                   {centerX + g + length, centerY}, t, color);

        DrawLineEx({centerX, centerY - g - length},
                   {centerX, centerY - g}, t, color);

        DrawLineEx({centerX, centerY + g},
                   {centerX, centerY + g + length}, t, color);
    }
    else if (type == 1)
    {
        DrawCircleV({centerX, centerY}, 3.0f * scale, color);
    }
    else if (type == 2)
    {
        DrawCircleLines((int)centerX, (int)centerY, 9.0f * scale, color);
    }
    else if (type == 3)
    {
        DrawLineEx({centerX - length, centerY},
                   {centerX + length, centerY}, t, color);

        DrawLineEx({centerX, centerY - length},
                   {centerX, centerY + length}, t, color);
    }
    else if (type == 4)
    {
        DrawLineEx({centerX - length, centerY - length},
                   {centerX - g, centerY - g}, t, color);

        DrawLineEx({centerX + length, centerY - length},
                   {centerX + g, centerY - g}, t, color);

        DrawLineEx({centerX - length, centerY + length},
                   {centerX - g, centerY + g}, t, color);

        DrawLineEx({centerX + length, centerY + length},
                   {centerX + g, centerY + g}, t, color);
    }
    else if (type == 5)
    {
        DrawLineEx({centerX - length, centerY - g},
                   {centerX + length, centerY - g}, t, color);

        DrawLineEx({centerX, centerY - g},
                   {centerX, centerY + length}, t, color);
    }
    else if (type == 6)
    {
        DrawCircleLines((int)centerX, (int)centerY, 12.0f * scale, color);
        DrawCircleV({centerX, centerY}, 2.0f * scale, color);
    }
    else if (type == 7)
    {
        DrawLineEx({centerX - length, centerY},
                   {centerX - g, centerY}, t, color);

        DrawLineEx({centerX + g, centerY},
                   {centerX + length, centerY}, t, color);

        DrawCircleV({centerX, centerY}, 2.0f * scale, color);
    }
    else if (type == 8)
    {
        DrawLineEx({centerX - length, centerY - length},
                   {centerX - g, centerY - g}, t, color);

        DrawLineEx({centerX + length, centerY - length},
                   {centerX + g, centerY - g}, t, color);

        DrawLineEx({centerX, centerY + g},
                   {centerX, centerY + length}, t, color);
    }
    else if (type == 9)
    {
        DrawRectangle(
            (int)(centerX - 2 * scale),
            (int)(centerY - 2 * scale),
            (int)(4 * scale),
            (int)(4 * scale),
            color
        );
    }
    else if (type == 10)
    {
        DrawLineEx({centerX - length, centerY},
                   {centerX - g, centerY}, t, color);

        DrawLineEx({centerX + g, centerY},
                   {centerX + length, centerY}, t, color);

        DrawLineEx({centerX, centerY - length},
                   {centerX, centerY - g}, t, color);
    }
    else
    {
        DrawCircleLines((int)centerX, (int)centerY, 5.0f * scale, color);

        DrawLineEx({centerX - length, centerY},
                   {centerX - 6 * scale, centerY}, t, color);

        DrawLineEx({centerX + 6 * scale, centerY},
                   {centerX + length, centerY}, t, color);
    }
}

// Designs 1-12 are the base shapes. Designs 13-120 reuse those shapes
// with a variant (centre dot, outline, bigger, smaller, wider gap) so
// every one of the 120 designs actually looks different.
void DrawCrosshairPattern(
    int style,
    float centerX,
    float centerY,
    float scale,
    float thickness,
    float gap,
    Color color
)
{
    if (style == 0)
        return;

    int variant = ((style - 1) / 12) % 10;

    bool dot =
        variant == 1 || variant == 3 || variant == 5 || variant == 9;

    bool outline =
        variant == 2 || variant == 3 || variant == 7;

    if (variant == 4 || variant == 5)
        scale *= 1.35f;
    else if (variant == 6 || variant == 7)
        scale *= 0.7f;

    if (variant == 8 || variant == 9)
        gap += 4.0f;

    if (outline)
    {
        const Color shadow = {0, 0, 0, 200};

        const float offsets[4][2] =
        {
            {-1.0f, 0.0f},
            {1.0f, 0.0f},
            {0.0f, -1.0f},
            {0.0f, 1.0f}
        };

        for (int k = 0; k < 4; k++)
        {
            DrawCrosshairShape(
                style,
                centerX + offsets[k][0],
                centerY + offsets[k][1],
                scale,
                thickness + 1.0f,
                gap,
                shadow
            );
        }
    }

    DrawCrosshairShape(
        style, centerX, centerY, scale, thickness, gap, color
    );

    if (dot)
        DrawCircleV({centerX, centerY}, 1.6f * scale, color);
}

void DrawCrosshair()
{
    float centerX = GetScreenWidth() / 2.0f;
    float centerY = GetScreenHeight() / 2.0f;

    DrawCrosshairPattern(
        selectedCrosshair,
        centerX,
        centerY,
        crosshairSize,
        crosshairThickness,
        crosshairGap,
        crosshairColors[crosshairColorIndex]
    );

    // Hit marker: four small ticks that spread out and fade.
    if (hitMarkerTimer > 0.0f)
    {
        float fade = hitMarkerTimer / 0.18f;
        float inner = 7.0f + (1.0f - fade) * 5.0f;
        float outer = inner + 7.0f;

        Color markerColor = Fade(WHITE, fade);

        DrawLineEx({centerX - inner, centerY - inner},
                   {centerX - outer, centerY - outer}, 2.0f, markerColor);

        DrawLineEx({centerX + inner, centerY - inner},
                   {centerX + outer, centerY - outer}, 2.0f, markerColor);

        DrawLineEx({centerX - inner, centerY + inner},
                   {centerX - outer, centerY + outer}, 2.0f, markerColor);

        DrawLineEx({centerX + inner, centerY + inner},
                   {centerX + outer, centerY + outer}, 2.0f, markerColor);
    }
}

void DrawCrosshairPreview(
    int style,
    float x,
    float y,
    float scale,
    Color color
)
{
    DrawCrosshairPattern(style, x, y, scale, 2.0f, 5.0f, color);
}

// ============================================================
// CROSSHAIR MENU
// ============================================================

void DrawCrosshairScreen()
{
    DrawBackground();

    DrawTextCustom(
        "CROSSHAIR CUSTOMIZATION",
        45, 35, 32, RED_GLOW
    );

    DrawTextCustom(
        "SELECT YOUR CROSSHAIR",
        45, 82, 16, TEXT_GREY
    );

    int startX = 65;
    int startY = 155;

    for (int i = 0; i < 20; i++)
    {
        int style = crosshairPage * 20 + i;

        if (style >= CROSSHAIR_COUNT)
            break;

        int column = i % 5;
        int row = i / 5;

        float x = startX + column * 155;
        float y = startY + row * 105;

        Rectangle cell = {x, y, 135, 82};

        bool hovered = CheckCollisionPointRec(
            GetMousePosition(),
            cell
        );

        bool selected = selectedCrosshair == style + 1;

        Color border = selected
            ? RED_GLOW
            : Color{65, 65, 65, 255};

        if (hovered)
            border = RED_MAIN;

        DrawRectangle(
            (int)x, (int)y, 135, 82,
            Color{12, 12, 12, 255}
        );

        DrawRectangleLinesEx(
            cell,
            selected ? 2.5f : 1.0f,
            border
        );

        DrawCrosshairPreview(
            style + 1,
            x + 67,
            y + 34,
            0.85f,
            crosshairColors[crosshairColorIndex]
        );

        DrawTextCustom(
            TextFormat("DESIGN %03i", style + 1),
            x + 27, y + 65, 11,
            selected ? RED_GLOW : TEXT_GREY
        );

        if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            selectedCrosshair = style + 1;
    }

    if (RedButton("PREVIOUS", {45, 600, 120, 40}))
    {
        crosshairPage--;

        if (crosshairPage < 0)
            crosshairPage = 5;
    }

    if (RedButton("NEXT", {185, 600, 120, 40}))
    {
        crosshairPage++;

        if (crosshairPage > 5)
            crosshairPage = 0;
    }

    DrawTextCustom(
        TextFormat("PAGE %i / 6", crosshairPage + 1),
        335, 610, 17, TEXT_WHITE
    );

    DrawTextCustom(
        TextFormat("SELECTED: %03i", selectedCrosshair),
        500, 610, 17, RED_GLOW
    );

    DrawTextCustom("PREVIEW", 900, 145, 18, TEXT_WHITE);

    DrawRectangle(875, 175, 220, 160, PANEL);
    DrawRectangleLines(875, 175, 220, 160, RED_DARK);

    DrawCrosshairPreview(
        selectedCrosshair,
        985, 255,
        1.5f,
        crosshairColors[crosshairColorIndex]
    );

    if (RedButton(
        TextFormat("COLOR: %s", crosshairColorNames[crosshairColorIndex]),
        {880, 370, 210, 42}
    ))
    {
        crosshairColorIndex++;

        if (crosshairColorIndex > 5)
            crosshairColorIndex = 0;
    }

    DrawSlider(
        "Crosshair size",
        &crosshairSize,
        0.5f, 2.5f,
        {880, 465, 210, 10}
    );

    DrawSlider(
        "Thickness",
        &crosshairThickness,
        1.0f, 6.0f,
        {880, 535, 210, 10}
    );

    DrawSlider(
        "Gap",
        &crosshairGap,
        0.0f, 15.0f,
        {880, 605, 210, 10}
    );

    if (RedButton("TURN OFF", {45, 660, 150, 40}))
        selectedCrosshair = 0;

    if (RedButton("BACK", {215, 660, 150, 40}))
        SetGameScreen(SETTINGS);
}

// ============================================================
// GAME WORLD
// ============================================================

void DrawWorld()
{
    const Color floorColor = {20, 20, 20, 255};
    const Color wallColor = {25, 25, 25, 255};
    const Color panelColor = {34, 34, 34, 255};
    const Color darkMetal = {11, 11, 11, 255};
    const Color redGlow = {255, 32, 32, 255};
    const Color redHot = {255, 69, 69, 255};

    BeginMode3D(camera);

    DrawCubeV(
        {0.0f, -1.25f, -16.0f},
        {34.0f, 0.5f, 46.0f},
        floorColor
    );

    DrawCubeV(
        {-17.0f, 5.0f, -16.0f},
        {0.6f, 12.0f, 46.0f},
        wallColor
    );

    DrawCubeV(
        {17.0f, 5.0f, -16.0f},
        {0.6f, 12.0f, 46.0f},
        wallColor
    );

    DrawCubeV(
        {0.0f, 11.2f, -16.0f},
        {34.0f, 0.5f, 46.0f},
        darkMetal
    );

    DrawCubeV(
        {0.0f, 5.0f, -39.0f},
        {34.0f, 12.0f, 0.7f},
        wallColor
    );

    for (int i = 0; i < 6; i++)
    {
        float z = -3.0f - i * 6.0f;

        DrawCubeV(
            {-16.62f, 5.0f, z},
            {0.12f, 9.8f, 4.8f},
            panelColor
        );

        DrawCubeV(
            {16.62f, 5.0f, z},
            {0.12f, 9.8f, 4.8f},
            panelColor
        );
    }

    DrawCubeV(
        {-16.55f, 8.9f, -16.0f},
        {0.08f, 0.07f, 43.0f},
        redGlow
    );

    DrawCubeV(
        {16.55f, 8.9f, -16.0f},
        {0.08f, 0.07f, 43.0f},
        redGlow
    );

    for (int i = 0; i < 5; i++)
    {
        float z = -4.0f - i * 7.0f;

        DrawCubeV(
            {0.0f, 10.8f, z},
            {3.8f, 0.035f, 0.12f},
            redGlow
        );
    }

    // Graphics quality now controls how smooth the targets are.
    const int segmentOptions[] = {16, 24, 36, 48};
    const int segments = segmentOptions[graphicsQuality];

    for (int i = 0; i < TARGET_COUNT; i++)
    {
        Vector3 p = targets[i].position;

        // Targets pop in when they spawn.
        float t = std::min(1.0f, targets[i].age / 0.15f);
        float eased = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
        float r = targets[i].radius * (0.25f + 0.75f * eased);

        DrawCylinderEx(
            {p.x, p.y, p.z + 0.12f},
            {p.x, p.y, p.z + 0.04f},
            r + 0.08f,
            r + 0.08f,
            segments,
            Color{35, 7, 12, 255}
        );

        DrawCylinderEx(
            {p.x, p.y, p.z + 0.20f},
            {p.x, p.y, p.z + 0.13f},
            r * 0.78f,
            r * 0.78f,
            segments,
            RED_MAIN
        );

        DrawCylinderEx(
            {p.x, p.y, p.z + 0.28f},
            {p.x, p.y, p.z + 0.21f},
            r * 0.50f,
            r * 0.50f,
            segments,
            Color{55, 8, 16, 255}
        );

        DrawCylinderEx(
            {p.x, p.y, p.z + 0.36f},
            {p.x, p.y, p.z + 0.29f},
            r * 0.23f,
            r * 0.23f,
            segments,
            redHot
        );

        DrawSphere(
            {p.x, p.y, p.z + 0.39f},
            r * 0.07f,
            Color{255, 145, 150, 255}
        );
    }

    DrawParticles();

    EndMode3D();
}

// ============================================================
// HUD
// ============================================================

void DrawHUD()
{
    int multiplier = GetScoreMultiplier();

    DrawRectangle(
        15, 15, 230, 150,
        Fade(BLACK, 0.65f)
    );

    DrawTextCustom(
        TextFormat("SCORE: %i", score),
        30, 25, 22, WHITE
    );

    DrawTextCustom(
        TextFormat("HITS: %i", hits),
        30, 55, 18, GREEN
    );

    DrawTextCustom(
        TextFormat("MISSES: %i", misses),
        130, 55, 18, RED_MAIN
    );

    DrawTextCustom(
        TextFormat("ACCURACY: %.1f%%", GetAccuracy()),
        30, 90, 18, WHITE
    );

    DrawTextCustom(
        TextFormat("STREAK: %i  (x%i)", streak, multiplier),
        30, 122, 18,
        multiplier > 1 ? RED_GLOW : TEXT_GREY
    );

    if (timerEnabled)
    {
        int panelX = GetScreenWidth() - 190;

        DrawRectangle(
            panelX, 15, 175, 70,
            Fade(BLACK, 0.65f)
        );

        DrawTextCustom(
            TextFormat("TIME: %.1f", remainingTime),
            (float)(panelX + 15),
            27, 22, WHITE
        );

        float fraction =
            std::clamp(remainingTime / gameDuration, 0.0f, 1.0f);

        DrawRectangle(panelX + 15, 62, 145, 5, Color{55, 55, 55, 255});

        DrawRectangle(
            panelX + 15, 62,
            (int)(145.0f * fraction), 5,
            fraction < 0.2f ? RED_MAIN : RED_GLOW
        );
    }

    DrawTextCustom(
        TextFormat("FPS: %i", GetFPS()),
        GetScreenWidth() - 100,
        GetScreenHeight() - 35,
        18,
        RED_GLOW
    );
}

// ============================================================
// MAIN MENU
// ============================================================

void DrawMenu()
{
    DrawBackground();

    int width = GetScreenWidth();
    int height = GetScreenHeight();

    DrawTextCustom("AIM", 75, 140, 96, WHITE);
    DrawTextCustom("TRAINER3D", 75, 240, 58, RED_MAIN);

    DrawTextCustom(
        "A NEW GENERATION OF AIM TRAINING",
        80, 320, 15, RED_GLOW
    );

    DrawRectangle(80, 360, 110, 3, RED_MAIN);

    DrawTextCustom(
        "PRECISION  //  SPEED  //  ACCURACY",
        80, 385, 16, TEXT_GREY
    );

    DrawTextCustom(
        "FIRST PERSON AIM TRAINING",
        80, height - 65, 15, TEXT_GREY
    );

    int menuX = width - 410;

    DrawTextCustom("MAIN MENU", menuX, 120, 32, RED_GLOW);
    DrawTextCustom("SELECT AN OPTION", menuX, 160, 15, TEXT_GREY);

    DrawLine(menuX, 190, menuX + 320, 190, RED_DARK);

    if (RedButton("PLAY", {(float)menuX, 235, 330, 60}))
        StartGame();

    if (RedButton("SETTINGS", {(float)menuX, 325, 330, 60}))
        SetGameScreen(SETTINGS);

    if (RedButton("EXIT GAME", {(float)menuX, 415, 330, 60}))
        shouldExit = true;

    DrawTextCustom(
        TextFormat(
            "BEST SCORE (%.0fs): %i",
            gameDuration,
            highScores[GetHighScoreSlot()]
        ),
        (float)menuX, 510, 15, TEXT_GREY
    );

    DrawTextCustom(
        "VERSION 2.1",
        (float)menuX, height - 55.0f, 13, TEXT_GREY
    );
}

// ============================================================
// UPDATED SETTINGS MENU
// ============================================================

void DrawSettings()
{
    DrawBackground();

    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    float centerX = screenW / 2.0f;

    // HEADER - TOP CENTRE

    const char* title = "SETTINGS";
    float titleWidth = MeasureCustomText(title, 36);

    DrawTextCustom(
        title,
        centerX - titleWidth / 2,
        22,
        36,
        RED_GLOW
    );

    const char* subtitle = "CUSTOMIZE YOUR AIM TRAINER";
    float subtitleWidth = MeasureCustomText(subtitle, 15);

    DrawTextCustom(
        subtitle,
        centerX - subtitleWidth / 2,
        65,
        15,
        TEXT_GREY
    );

    // TOP TABS

    Rectangle generalTab =
    {
        centerX - 185, 100, 165, 42
    };

    Rectangle crosshairTab =
    {
        centerX + 20, 100, 165, 42
    };

    if (RedButton("GENERAL", generalTab))
    {
        // Already on the general settings page.
    }

    if (RedButton("CROSSHAIR", crosshairTab))
        SetGameScreen(CROSSHAIR);

    // TWO COLUMN LAYOUT

    float columnWidth = std::min(400.0f, (screenW - 100.0f) / 2.0f);

    float leftX = centerX - columnWidth - 35.0f;
    float rightX = centerX + 35.0f;

    // LEFT COLUMN: MOUSE SETTINGS

    DrawTextCustom("MOUSE SETTINGS", leftX, 175, 23, RED_GLOW);

    DrawRectangle(
        (int)leftX, 210, (int)columnWidth, 1,
        Color{65, 65, 65, 255}
    );

    DrawSlider(
        "Horizontal mouse sensitivity",
        &mouseSensitivity,
        0.1f, 3.0f,
        {leftX, 255, columnWidth, 10}
    );

    DrawTextCustom(
        TextFormat("Horizontal: %.2f", mouseSensitivity),
        leftX, 275, 15, TEXT_GREY
    );

    DrawSlider(
        "Vertical mouse sensitivity",
        &verticalSensitivityMultiplier,
        0.1f, 1.5f,
        {leftX, 330, columnWidth, 10}
    );

    DrawTextCustom(
        TextFormat(
            "Vertical: %.0f%%",
            verticalSensitivityMultiplier * 100.0f
        ),
        leftX, 350, 15, TEXT_GREY
    );

    // LEFT COLUMN: TARGET SETTINGS

    DrawTextCustom("TARGET SETTINGS", leftX, 390, 23, RED_GLOW);

    DrawRectangle(
        (int)leftX, 425, (int)columnWidth, 1,
        Color{65, 65, 65, 255}
    );

    if (RedButton(
        targetMovementEnabled
            ? "TARGET MOVEMENT: ON"
            : "TARGET MOVEMENT: OFF",
        {leftX, 440, columnWidth, 45}
    ))
    {
        targetMovementEnabled = !targetMovementEnabled;
    }

    DrawSlider(
        "Target movement speed",
        &targetMovementSpeed,
        0.0f, 10.0f,
        {leftX, 535, columnWidth, 10}
    );

    DrawTextCustom(
        TextFormat("Speed: %.1f / 10", targetMovementSpeed),
        leftX, 555, 15, TEXT_GREY
    );

    DrawSlider(
        "Target size",
        &targetRadius,
        0.3f, 1.2f,
        {leftX, 605, columnWidth, 10}
    );

    DrawTextCustom(
        TextFormat("Radius: %.2f  (applies to new targets)", targetRadius),
        leftX, 625, 15, TEXT_GREY
    );

    // RIGHT COLUMN: GAME SETTINGS

    DrawTextCustom("GAME SETTINGS", rightX, 175, 23, RED_GLOW);

    DrawRectangle(
        (int)rightX, 210, (int)columnWidth, 1,
        Color{65, 65, 65, 255}
    );

    if (RedButton(
        timerEnabled ? "TIMER: ON" : "TIMER: OFF",
        {rightX, 240, columnWidth, 45}
    ))
    {
        timerEnabled = !timerEnabled;
    }

    if (RedButton(
        TextFormat("GAME DURATION: %.0f SECONDS", gameDuration),
        {rightX, 300, columnWidth, 45}
    ))
    {
        gameDuration += 30.0f;

        if (gameDuration > 180.0f)
            gameDuration = 30.0f;
    }

    if (RedButton(
        soundEnabled ? "SOUND: ON" : "SOUND: OFF",
        {rightX, 360, columnWidth, 45}
    ))
    {
        soundEnabled = !soundEnabled;
    }

    if (RedButton(
        benchmarkMode ? "BENCHMARK: ON" : "BENCHMARK: OFF",
        {rightX, 420, columnWidth, 45}
    ))
    {
        benchmarkMode = !benchmarkMode;
    }

    if (RedButton(
        TextFormat("FPS LIMIT: %i", fpsLimit),
        {rightX, 480, columnWidth, 45}
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

    if (RedButton(
        TextFormat("GRAPHICS: %s", qualityNames[graphicsQuality]),
        {rightX, 540, columnWidth, 45}
    ))
    {
        graphicsQuality++;

        if (graphicsQuality > 3)
            graphicsQuality = 0;
    }

    if (RedButton(
        TextFormat(
            "RESOLUTION: %i x %i",
            resolutions[resolutionIndex].width,
            resolutions[resolutionIndex].height
        ),
        {rightX, 600, columnWidth, 45}
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

    // BOTTOM CENTRE BUTTONS

    float buttonY = screenH - 65.0f;
    float bottomButtonWidth = 140.0f;
    float bottomGap = 25.0f;

    float totalWidth =
        bottomButtonWidth * 3 + bottomGap * 2;

    float startX = centerX - totalWidth / 2.0f;

    Rectangle backButton =
    {
        startX,
        buttonY,
        bottomButtonWidth,
        42
    };

    Rectangle playButton =
    {
        startX + bottomButtonWidth + bottomGap,
        buttonY,
        bottomButtonWidth,
        42
    };

    Rectangle mainMenuButton =
    {
        startX + (bottomButtonWidth + bottomGap) * 2,
        buttonY,
        bottomButtonWidth,
        42
    };

    if (RedButton("BACK", backButton))
        SetGameScreen(MENU);

    if (RedButton("PLAY", playButton))
        StartGame();

    if (RedButton("MAIN MENU", mainMenuButton))
        SetGameScreen(MENU);
}

// ============================================================
// GAME UPDATE
// ============================================================

void UpdateGame()
{
    float deltaTime = GetFrameTime();

    if (IsKeyPressed(KEY_ESCAPE))
    {
        hitMarkerTimer = 0.0f;
        missFlashTimer = 0.0f;

        SetGameScreen(PAUSED);
        return;
    }

    UpdateCamera();
    UpdateTargets(deltaTime);
    UpdateParticles(deltaTime);

    sessionElapsed += deltaTime;

    hitMarkerTimer = std::max(0.0f, hitMarkerTimer - deltaTime);
    missFlashTimer = std::max(0.0f, missFlashTimer - deltaTime);

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
    ClearBackground(BLACK);

    DrawWorld();

    // Faint red flash when you miss.
    if (missFlashTimer > 0.0f)
    {
        DrawRectangle(
            0, 0,
            GetScreenWidth(),
            GetScreenHeight(),
            Fade(RED_MAIN, missFlashTimer / 0.12f * 0.08f)
        );
    }

    DrawCrosshair();
    DrawHUD();
}

// ============================================================
// PAUSE MENU
// ============================================================

void DrawPaused()
{
    DrawGame();

    DrawRectangle(
        0, 0,
        GetScreenWidth(),
        GetScreenHeight(),
        Color{0, 0, 0, 220}
    );

    int centerX = GetScreenWidth() / 2;
    int centerY = GetScreenHeight() / 2;

    DrawTextCentered("GAME PAUSED", (float)centerX, (float)(centerY - 190), 38, RED_GLOW);

    DrawTextCentered(
        TextFormat("SCORE: %i", score),
        (float)centerX, (float)(centerY - 125), 20, WHITE
    );

    DrawTextCentered(
        TextFormat("ACCURACY: %.1f%%", GetAccuracy()),
        (float)centerX, (float)(centerY - 90), 18, TEXT_GREY
    );

    if (RedButton(
        "RESUME",
        {(float)(centerX - 160), (float)(centerY - 25), 320, 50}
    ))
        SetGameScreen(GAME);

    if (RedButton(
        "RESTART",
        {(float)(centerX - 160), (float)(centerY + 45), 320, 50}
    ))
        StartGame();

    if (RedButton(
        "END SESSION",
        {(float)(centerX - 160), (float)(centerY + 115), 320, 50}
    ))
        FinishGame();

    if (RedButton(
        "MAIN MENU",
        {(float)(centerX - 160), (float)(centerY + 185), 320, 50}
    ))
        SetGameScreen(MENU);

    DrawTextCentered(
        "PRESS ESC TO RESUME",
        (float)centerX, (float)(centerY + 255), 14, TEXT_GREY
    );
}

// ============================================================
// RESULTS
// ============================================================

void DrawResults()
{
    DrawBackground();

    float centerX = GetScreenWidth() / 2.0f;

    DrawTextCentered("SESSION COMPLETE", centerX, 80, 38, RED_GLOW);

    if (newHighScore)
        DrawTextCentered("NEW HIGH SCORE!", centerX, 135, 22, YELLOW);

    DrawTextCentered(TextFormat("SCORE: %i", score), centerX, 180, 30, WHITE);

    if (timerEnabled)
    {
        DrawTextCentered(
            TextFormat(
                "BEST (%.0fs SESSION): %i",
                gameDuration,
                highScores[GetHighScoreSlot()]
            ),
            centerX, 222, 16, TEXT_GREY
        );
    }

    float averageHitTime = hits > 0 ? totalHitInterval / hits : 0.0f;

    float hitsPerMinute =
        sessionElapsed > 0.0f ? hits / sessionElapsed * 60.0f : 0.0f;

    float y = 270.0f;
    float step = 36.0f;

    DrawTextCentered(TextFormat("HITS: %i", hits), centerX, y, 20, GREEN);
    y += step;

    DrawTextCentered(TextFormat("MISSES: %i", misses), centerX, y, 20, RED_MAIN);
    y += step;

    DrawTextCentered(
        TextFormat("ACCURACY: %.1f%%", GetAccuracy()),
        centerX, y, 20, WHITE
    );
    y += step;

    DrawTextCentered(
        TextFormat("BEST STREAK: %i", bestStreak),
        centerX, y, 20, WHITE
    );
    y += step;

    DrawTextCentered(
        TextFormat("AVG TIME PER HIT: %.2fs", averageHitTime),
        centerX, y, 20, WHITE
    );
    y += step;

    DrawTextCentered(
        TextFormat("HITS PER MINUTE: %.0f", hitsPerMinute),
        centerX, y, 20, WHITE
    );

    if (RedButton(
        "PLAY AGAIN",
        {centerX - 150, 530, 300, 50}
    ))
        StartGame();

    if (RedButton(
        "MAIN MENU",
        {centerX - 150, 595, 300, 50}
    ))
        SetGameScreen(MENU);
}

static void EnablePerMonitorDpi()
{
    HMODULE user32 = GetModuleHandleA("user32.dll");

    if (!user32)
        return;

    auto setContext = (BOOL (WINAPI *)(void*))
        GetProcAddress(user32, "SetProcessDpiAwarenessContext");

    // -4 is PER_MONITOR_AWARE_V2. Stops Windows putting the
    // window on the wrong screen when the displays use different scaling.
    if (setContext)
        setContext((void*)(LONG_PTR)-4);
}

static int FindPrimaryMonitor()
{
    int count = GetMonitorCount();

    for (int i = 0; i < count; i++)
    {
        Vector2 position = GetMonitorPosition(i);

        // Main display (Windows monitor 1) is the one at (0, 0).
        // Raylib's monitor 0 is often your second screen.
        if (fabsf(position.x) < 1.0f && fabsf(position.y) < 1.0f)
            return i;
    }

    return 0;
}

static void OpenFullscreenOnMainMonitor()
{
    int monitor = FindPrimaryMonitor();

    if (IsWindowFullscreen())
        ToggleFullscreen();

    if (IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE))
        ToggleBorderlessWindowed();

    // Park a normal window on the main display FIRST.
    // Fullscreen uses whichever monitor that window is on.
    // Doing it in the same step sends it to the screen under the cursor.
    SetWindowMonitor(monitor);

    ClearWindowState(FLAG_WINDOW_HIDDEN);
    SetWindowFocused();

    BeginDrawing();
    ClearBackground(BACKGROUND);
    EndDrawing();

    SetWindowMonitor(monitor);

    if (!IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE))
        ToggleBorderlessWindowed();

    if (GetCurrentMonitor() != monitor)
    {
        if (IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE))
            ToggleBorderlessWindowed();

        if (IsWindowFullscreen())
            ToggleFullscreen();

        Vector2 origin = GetMonitorPosition(monitor);

        SetWindowState(FLAG_WINDOW_UNDECORATED);
        SetWindowSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
        SetWindowPosition((int)origin.x, (int)origin.y);
    }

    fullscreen = true;
    SetWindowFocused();
}

static void ToggleGameFullscreen()
{
    bool wasPlaying = currentScreen == GAME;

    if (wasPlaying)
        EnableCursor();

    if (!fullscreen)
    {
        int monitor = FindPrimaryMonitor();

        SetWindowMonitor(monitor);

        if (!IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE))
            ToggleBorderlessWindowed();

        fullscreen = true;
    }
    else
    {
        if (IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE))
            ToggleBorderlessWindowed();

        if (IsWindowFullscreen())
            ToggleFullscreen();

        ClearWindowState(FLAG_WINDOW_UNDECORATED);

        int monitor = FindPrimaryMonitor();
        Vector2 origin = GetMonitorPosition(monitor);
        int monitorWidth = GetMonitorWidth(monitor);
        int monitorHeight = GetMonitorHeight(monitor);

        int width = resolutions[resolutionIndex].width;
        int height = resolutions[resolutionIndex].height;

        if (width > monitorWidth)
            width = monitorWidth;

        if (height > monitorHeight)
            height = monitorHeight;

        SetWindowSize(width, height);
        SetWindowMonitor(monitor);
        SetWindowPosition(
            (int)origin.x + (monitorWidth - width) / 2,
            (int)origin.y + (monitorHeight - height) / 2
        );

        fullscreen = false;
    }

    if (wasPlaying)
    {
        DisableCursor();
        GetMouseDelta();
    }
}

// ============================================================
// MAIN
// ============================================================

int main()
{
    SetRandomSeed((unsigned int)time(nullptr));

    LoadSettings();

    EnablePerMonitorDpi();

    SetConfigFlags(
        FLAG_MSAA_4X_HINT |
        FLAG_WINDOW_RESIZABLE |
        FLAG_WINDOW_HIDDEN
    );

    InitWindow(
        resolutions[resolutionIndex].width,
        resolutions[resolutionIndex].height,
        "Aim Trainer 3D"
    );

    SetExitKey(KEY_NULL);
    SetWindowMinSize(960, 720);

    OpenFullscreenOnMainMonitor();

    SetTargetFPS(fpsLimit);

    LoadGameFont();
    LoadGameSounds();
    InitializeStars();
    InitializeTargets();

    camera.position = {0.0f, 2.0f, 0.0f};
    camera.target = {0.0f, 2.0f, -20.0f};
    camera.up = {0.0f, 1.0f, 0.0f};
    camera.fovy = 75.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    while (!WindowShouldClose() && !shouldExit)
    {
        if (IsKeyPressed(KEY_F11))
            ToggleGameFullscreen();

        if (benchmarkMode)
            SetTargetFPS(0);
        else
            SetTargetFPS(fpsLimit);

        if (currentScreen == GAME)
        {
            UpdateGame();
        }
        else if (IsKeyPressed(KEY_ESCAPE))
        {
            // ESC works as "back" everywhere.
            if (currentScreen == PAUSED)
                SetGameScreen(GAME);
            else if (currentScreen == SETTINGS || currentScreen == RESULTS)
                SetGameScreen(MENU);
            else if (currentScreen == CROSSHAIR)
                SetGameScreen(SETTINGS);
        }

        BeginDrawing();

        switch (currentScreen)
        {
            case MENU:
                DrawMenu();
                break;

            case SETTINGS:
                DrawSettings();
                break;

            case CROSSHAIR:
                DrawCrosshairScreen();
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

    SaveSettings();
    UnloadGameSounds();

    if (customFontLoaded)
        UnloadFont(gameFont);

    CloseWindow();

    return 0;
}
