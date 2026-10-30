// AFTERIMAGE - top-level game: screens, level flow, interaction, puzzles, scripting.
// Implementation is split across Game.cpp (logic), Render.cpp (world) and UI.cpp (HUD/menus).
#pragma once
#include <deque>
#include <string>
#include <vector>
#include "Afterimage.h"
#include "Art.h"
#include "Enemy.h"
#include "Habits.h"
#include "Player.h"
#include "World.h"
#include "raylib.h"

enum class Screen { Title, Controls, Rules, Settings, HighScores, Briefing, Playing, Paused, Reading, CaseFile, GameOver, Victory };
enum class ReadAction { None, TerminalReveal };

struct NoiseRing {
    Vector2 pos;
    float radius;
    float t;
};

struct MemoryFrame {
    float time = 0.f;
    int area = -1;
    Vector2 pos{0, 0};
    int dir = 0;
    bool moving = false;
    bool sprinting = false;
    bool hiding = false;
};

struct LevelFlags {
    bool livingTriggered = false;  // Area 1: the cabinet moves, afterimage tutorial
    bool powerOn = false;          // Area 1: fuse restored
    bool revealShown = false;      // Area 1: the Tenant is glimpsed
    float apparition = 0.f;
    int loopCount = 0;             // Area 2: times the corridor repeated
    int switchStep = 0;            // Area 3: correct levers thrown so far
    bool terminalOn = false;
    bool finalChase = false;
};

class Game {
public:
    void init(bool selfTestMode);
    void init(bool selfTestMode, bool freshAIProfile);
    void shutdown();
    void update(float dt);
    void draw();
    void afterDraw();  // screenshots for self-test
    bool quit = false;

private:
    // ---- state ----
    Screen screen = Screen::Title;
    Screen controlsBack = Screen::Title;
    int menuIndex = 1;  // New Game is the first enabled option when no checkpoint exists.
    int pauseIndex = 0;
    int gameOverIndex = 0;
    int victoryIndex = 0;
    int settingsIndex = 0;
    Screen settingsBack = Screen::Title;
    Screen rulesBack = Screen::Title;
    float screenTimer = 0.f;
    float areaPlayTime[LEVEL_COUNT]{};

    Level level;
    int levelIndex = 0;
    Player player;
    Enemy enemy;
    HabitProfile habits;
    Afterimage after;
    Art art;
    Sound deathStinger{};
    Sound footstepSound{};
    Sound sprintClatterSound{};
    Sound breathingSound{};
    Sound jumpscareBed{};
    float footstepTimer = 0.f;
    float breathingTimer = 0.f;
    bool ownsAudioDevice = false;
    std::vector<std::string> inventory;
    std::vector<std::string> checkpointInv;
    LevelFlags flags;
    bool afterimageLearned = false;
    int visits = 47;
    int score = 0;
    int highScore = 0;
    bool progressAvailable = false;
    int continueLevel = 0;
    int continueScore = 0;
    int continueVisits = 47;
    std::vector<std::string> continueInventory;
    Vector2 continuePosition{0, 0};
    bool continueHasPosition = false;
    int movementSetting = 0;
    int threatSetting = 0;
    int messageSetting = 0;
    bool screenEffects = true;
    std::vector<std::vector<unsigned char>> scoredTiles;
    std::string objective;
    std::deque<std::string> messages;
    float msgTimer = 0.f;

    std::string readTitle;
    std::vector<std::string> readPages;
    int readPage = 0;
    ReadAction readAction = ReadAction::None;

    // ---- presentation ----
    RenderTexture2D worldRT{};
    Camera2D cam{};
    Rectangle viewport{0, 0, (float)WINDOW_W, (float)WINDOW_H};
    float fade = 1.f;
    int pendingLevel = -1;
    float flicker = 0.f, lightning = 0.f, lightningTimer = 14.f, spotFlash = 0.f, clock = 0.f;
    float interactFlash = 0.f;
    Color interactFlashColor = WHITE;
    std::vector<NoiseRing> rings;
    float sprintNoiseTimer = 0.f;
    std::deque<MemoryFrame> playerMemory;
    float memorySampleTimer = 0.f;
    float movementSaveTimer = 5.f;
    Vector2 mimicMemoryPos{0, 0};
    int mimicMemoryDir = 0;
    float mimicMemoryAge = -1.f;
    float currentMimicPlayback = 0.f;
    Point lastPlayerTile{-1, -1};
    Point lastHabitTile{-1, -1};
    EnemyState previousEnemyState = EnemyState::Patrol;
    bool debug = false;

    // ---- self test ----
    bool selfTest = false;
    int testFrame = 0;
    std::string pendingShot;
    int testFailures = 0;

    // ---- flow (Game.cpp) ----
    void newGame();
    void continueGame();
    void loadLevel(int index, bool showBriefing = false);
    void saveProgress();
    void loadProgress();
    void loadHabits(bool freshProfile);
    void saveHabits();
    bool loadMovementHistory(const std::string& path);
    bool saveMovementHistory(const std::string& path) const;
    void retryArea();
    void beginTransition(int target);
    void gameOver();
    void victory();
    void updatePlaying(float dt);
    void updateScripts(float dt);
    void updateCamera();
    Object* nearestInteractable();
    std::string promptFor(const Object& o) const;
    void interact(Object& o);
    void enterHide(Object& o);
    void leaveHide();
    void useSwitch(Object& o);
    void openReading(const std::string& title, const std::vector<std::string>& pages, ReadAction action);
    void finishReading();
    void emitNoise(Vector2 pos, float radiusTiles, bool sprintNoise = false);
    void pushMsg(const std::string& m);
    void pushUrgent(const std::string& m);  // clears older queued messages first
    std::string afterimageHint() const;
    bool hasItem(const std::string& it) const;
    void addItem(const std::string& it);
    void removeItem(const std::string& it);
    void flash(Color c);
    void addScore(int points);
    void loadSettings();
    void saveSettings() const;
    void loadHighScore();
    void saveHighScore();
    void runSelfTest();
    void check(bool cond, const char* what);
    void validateLevels();

    // ---- world rendering (Render.cpp) ----
    void drawWorld();
    void drawTiles(int x0, int y0, int x1, int y1);
    void drawDarkness();
    void drawAfterimageOverlay();
    void drawEnemyOverlay();
    void drawDebug();
    void drawScreenFx();
    void drawTitleBackdrop(bool ending);
    const char* spriteFor(const Object& o) const;
    void drawSprite(const char* key, float cx, float yBottom, Color tint, int frame = 0, int frames = 1);
    void drawCharacter(const char* key, float cx, float yBottom, Color tint, int direction, int animationFrame);

    // ---- UI (UI.cpp) ----
    void drawUI();
    void drawHud();
    void drawTitleMenu();
    void drawBriefing();
    void drawControls();
    void drawRules();
    void drawSettings();
    void drawHighScores();
    void drawReading();
    void drawPause();
    void drawGameOver();
    void drawVictory();
    void drawCaseFile();
};
