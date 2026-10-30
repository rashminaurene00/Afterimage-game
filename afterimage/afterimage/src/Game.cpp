#include "Game.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include "AtomicFile.h"
#include "Config.h"
#include "Pathfinding.h"

namespace {
std::string AppFile(const char* name) {
    return std::string(GetApplicationDirectory()) + name;
}

std::string SaveFilePath(const char* name) {
    return AppFile(name);
}

bool InRect(Point p, int x0, int y0, int x1, int y1) {
    return p.x >= x0 && p.x <= x1 && p.y >= y0 && p.y <= y1;
}
float Dist(Vector2 a, Vector2 b) { return std::hypot(a.x - b.x, a.y - b.y); }
bool AnyKeyPressed(std::initializer_list<int> keys) {
    for (int k : keys)
        if (IsKeyPressed(k)) return true;
    return false;
}
}  // namespace

// =====================================================================================
// Lifecycle
// =====================================================================================
void Game::init(bool selfTestMode) {
    init(selfTestMode, false);
}

void Game::init(bool selfTestMode, bool freshAIProfile) {
    selfTest = selfTestMode;
    if (!IsAudioDeviceReady()) {
        InitAudioDevice();
        ownsAudioDevice = IsAudioDeviceReady();
    }
    if (IsAudioDeviceReady()) {
        deathStinger = LoadSound("assets/audio/death_stinger.wav");
        if (IsSoundValid(deathStinger)) SetSoundVolume(deathStinger, 0.82f);
        else TraceLog(LOG_WARNING, "Death stinger could not be loaded from assets/audio/death_stinger.wav");
        footstepSound = LoadSound("assets/audio/footstep.wav");
        sprintClatterSound = LoadSound("assets/audio/sprint_clatter.wav");
        breathingSound = LoadSound("assets/audio/breathing.wav");
        jumpscareBed = LoadSound("assets/audio/jumpscare_bed.wav");
        if (IsSoundValid(footstepSound)) SetSoundVolume(footstepSound, 0.24f);
        if (IsSoundValid(sprintClatterSound)) SetSoundVolume(sprintClatterSound, 0.38f);
        if (IsSoundValid(breathingSound)) SetSoundVolume(breathingSound, 0.3f);
        if (IsSoundValid(jumpscareBed)) SetSoundVolume(jumpscareBed, 0.68f);
        if (!IsSoundValid(footstepSound) || !IsSoundValid(sprintClatterSound) || !IsSoundValid(breathingSound))
            TraceLog(LOG_WARNING, "One or more movement sound assets could not be loaded from assets/audio");
    } else {
        TraceLog(LOG_WARNING, "Audio device unavailable; death jumpscare will be silent");
    }
    loadSettings();
    loadHighScore();
    loadProgress();
    loadHabits(freshAIProfile);
    if (!selfTest && !loadMovementHistory(SaveFilePath("afterimage_movement_history.dat")))
        TraceLog(LOG_WARNING, "Saved movement history is invalid; starting with an empty replay trace");
    menuIndex = progressAvailable ? 0 : 1;
    worldRT = LoadRenderTexture(VIRTUAL_W, VIRTUAL_H);
    SetTextureFilter(worldRT.texture, TEXTURE_FILTER_POINT);
    art.load();
    cam.offset = {VIRTUAL_W / 2.f, VIRTUAL_H / 2.f};
    cam.zoom = 1.f;
    cam.rotation = 0.f;
    screen = Screen::Title;
    if (selfTest) {
        Enemy::logTransitions = true;
        validateLevels();
    }
}

void Game::shutdown() {
    saveHabits();
    if (IsSoundValid(deathStinger)) UnloadSound(deathStinger);
    if (IsSoundValid(footstepSound)) UnloadSound(footstepSound);
    if (IsSoundValid(sprintClatterSound)) UnloadSound(sprintClatterSound);
    if (IsSoundValid(breathingSound)) UnloadSound(breathingSound);
    if (IsSoundValid(jumpscareBed)) UnloadSound(jumpscareBed);
    if (ownsAudioDevice && IsAudioDeviceReady()) CloseAudioDevice();
    art.unload();
    UnloadRenderTexture(worldRT);
}

// =====================================================================================
// Flow
// =====================================================================================
void Game::newGame() {
    inventory.clear();
    visits = 47;
    score = 0;
    memorySampleTimer = 0.f;
    mimicMemoryAge = -1.f;
    currentMimicPlayback = -1.f;
    lastHabitTile = {-1, -1};
    scoredTiles.assign(LEVEL_COUNT, {});
    afterimageLearned = false;
    progressAvailable = false;
    pendingLevel = -1;
    loadLevel(0, true);
    checkpointInv.clear();
    saveProgress();
}

void Game::loadHabits(bool freshProfile) {
    const std::string path = SaveFilePath("afterimage_ai_profile.dat");
    if (freshProfile) {
        if (std::remove(path.c_str()) != 0 && FileExists(path.c_str()))
            TraceLog(LOG_WARNING, "Could not remove AI profile for --fresh-ai");
        habits.reset();
        habits.dirty = false;
        TraceLog(LOG_INFO, "AI profile reset only; game progress/settings were retained");
        return;
    }
    if (!habits.load(path)) {
        habits.reset();
        habits.dirty = false;
        TraceLog(LOG_WARNING, "AI profile invalid or unsupported; using baseline behaviour");
    }
}

void Game::saveHabits() {
    if (selfTest) return;
    if (!habits.save(SaveFilePath("afterimage_ai_profile.dat")))
        TraceLog(LOG_WARNING, "Could not save adaptive AI profile");
    if (!saveMovementHistory(SaveFilePath("afterimage_movement_history.dat")))
        TraceLog(LOG_WARNING, "Could not save persistent movement history");
}

bool Game::saveMovementHistory(const std::string& path) const {
    if (playerMemory.empty()) return true;

    std::ostringstream data;
    size_t validCount = 0;
    for (const MemoryFrame& frame : playerMemory) {
        if (frame.area < 0 || frame.area >= LEVEL_COUNT) continue;
        const float age = areaPlayTime[frame.area] - frame.time;
        if (!std::isfinite(age) || age < 0.f || age > 132.f ||
            !std::isfinite(frame.pos.x) || !std::isfinite(frame.pos.y) ||
            frame.pos.x < 0.f || frame.pos.y < 0.f || frame.pos.x > 4096.f || frame.pos.y > 4096.f ||
            frame.dir < 0 || frame.dir > 3) continue;
        ++validCount;
    }
    if (validCount == 0) return true;

    data << "AFTERIMAGE_TRACE 1 " << validCount << '\n' << std::fixed << std::setprecision(3);
    for (const MemoryFrame& frame : playerMemory) {
        if (frame.area < 0 || frame.area >= LEVEL_COUNT) continue;
        const float age = areaPlayTime[frame.area] - frame.time;
        if (!std::isfinite(age) || age < 0.f || age > 132.f ||
            !std::isfinite(frame.pos.x) || !std::isfinite(frame.pos.y) ||
            frame.pos.x < 0.f || frame.pos.y < 0.f || frame.pos.x > 4096.f || frame.pos.y > 4096.f ||
            frame.dir < 0 || frame.dir > 3) continue;
        data << "F " << frame.area << ' ' << age << ' ' << frame.pos.x << ' ' << frame.pos.y << ' '
             << frame.dir << ' ' << (frame.moving ? 1 : 0) << ' ' << (frame.sprinting ? 1 : 0) << ' '
             << (frame.hiding ? 1 : 0) << '\n';
    }

    const std::string temporary = path + ".tmp";
    std::string contents = data.str();
    std::vector<char> writable(contents.begin(), contents.end());
    writable.push_back('\0');
    if (!SaveFileText(temporary.c_str(), writable.data())) return false;
    if (!AtomicReplaceFile(temporary.c_str(), path.c_str())) {
        std::remove(temporary.c_str());
        return false;
    }
    return true;
}

bool Game::loadMovementHistory(const std::string& path) {
    if (!FileExists(path.c_str())) return true;
    char* raw = LoadFileText(path.c_str());
    if (!raw) return false;

    std::istringstream input(raw);
    UnloadFileText(raw);
    std::string magic;
    int version = 0;
    size_t count = 0;
    if (!(input >> magic >> version >> count) || magic != "AFTERIMAGE_TRACE" || version != 1 || count > 2200)
        return false;

    std::deque<MemoryFrame> loaded;
    for (size_t i = 0; i < count; ++i) {
        char kind = 0;
        int moving = 0, sprinting = 0, hiding = 0;
        float age = 0.f;
        MemoryFrame frame;
        if (!(input >> kind >> frame.area >> age >> frame.pos.x >> frame.pos.y >> frame.dir >> moving >> sprinting >> hiding) ||
            kind != 'F' || frame.area < 0 || frame.area >= LEVEL_COUNT || !std::isfinite(age) ||
            age < 0.f || age > 132.f || !std::isfinite(frame.pos.x) || !std::isfinite(frame.pos.y) ||
            frame.pos.x < 0.f || frame.pos.y < 0.f || frame.pos.x > 4096.f || frame.pos.y > 4096.f ||
            frame.dir < 0 || frame.dir > 3 || moving < 0 || moving > 1 || sprinting < 0 || sprinting > 1 ||
            hiding < 0 || hiding > 1) return false;
        frame.time = -age;  // The previous session ends at time zero in this run's trace.
        frame.moving = moving != 0;
        frame.sprinting = sprinting != 0;
        frame.hiding = hiding != 0;
        loaded.push_back(frame);
    }
    std::string extra;
    if (input >> extra) return false;
    playerMemory = std::move(loaded);
    return true;
}

void Game::continueGame() {
    if (!progressAvailable) return;
    inventory.clear();
    checkpointInv.clear();
    scoredTiles.assign(LEVEL_COUNT, {});
    afterimageLearned = continueLevel > 0;
    score = continueScore;
    visits = continueVisits;
    visits++;
    inventory = continueInventory;
    loadLevel(continueLevel, false);
    checkpointInv = inventory;
    continuePosition = TileCenter(level.playerStart);
    continueHasPosition = true;
    saveProgress();
    screen = Screen::Playing;
    pushUrgent("Checkpoint restored: " + level.name + ". Your objective is shown above.");
}

void Game::loadProgress() {
    const std::string path = SaveFilePath("afterimage_progress.txt");
    if (!FileExists(path.c_str())) return;
    char* text = LoadFileText(path.c_str());
    if (!text) return;
    int savedLevel = 0, savedScore = 0, savedVisits = 47, itemCount = 0, posX = 0, posY = 0;
    char item1[64] = {}, item2[64] = {};
    int parsed = std::sscanf(text, "%d %d %d %d %d %d %63s %63s", &savedLevel, &savedScore, &savedVisits,
                             &itemCount, &posX, &posY, item1, item2);
    if (parsed >= 2 && savedLevel >= 0 && savedLevel < LEVEL_COUNT && savedScore >= 0) {
        continueLevel = savedLevel;
        continueScore = savedScore;
        continueVisits = parsed >= 3 ? std::max(47, savedVisits) : 47;
        continueInventory.clear();
        if (parsed >= 7 && itemCount > 0) continueInventory.emplace_back(item1);
        if (parsed >= 8 && itemCount > 1) continueInventory.emplace_back(item2);
        continueHasPosition = parsed >= 6;
        continuePosition = {(float)posX, (float)posY};
        progressAvailable = true;
    }
    UnloadFileText(text);
}

void Game::saveProgress() {
    if (!level.w || !level.h || (screen != Screen::Playing && screen != Screen::Paused && screen != Screen::CaseFile && screen != Screen::Reading)) return;
    char text[256];
    const std::vector<std::string>& savedInventory = checkpointInv;
    std::string item1 = savedInventory.size() > 0 ? savedInventory[0] : "-";
    std::string item2 = savedInventory.size() > 1 ? savedInventory[1] : "-";
    std::snprintf(text, sizeof(text), "%d %d %d %d %d %d %s %s\n", levelIndex, score, visits,
                  (int)savedInventory.size(), (int)TileCenter(level.playerStart).x, (int)TileCenter(level.playerStart).y,
                  item1.c_str(), item2.c_str());
    const std::string path = SaveFilePath("afterimage_progress.txt");
    if (SaveFileText(path.c_str(), text)) {
        continueLevel = levelIndex;
        continueScore = score;
        continueVisits = visits;
        continueInventory = checkpointInv;
        continuePosition = TileCenter(level.playerStart);
        continueHasPosition = true;
        progressAvailable = true;
    }
}

void Game::loadLevel(int index, bool showBriefing) {
    BuildLevel(index, level);
    levelIndex = index;
    if (screen == Screen::Briefing || screen == Screen::Title || index != continueLevel) {
        checkpointInv = inventory;
        continueLevel = index;
    }
    mimicMemoryAge = -1.f;
    currentMimicPlayback = -1.f;
    lastHabitTile = {-1, -1};
    if (scoredTiles.size() != LEVEL_COUNT) scoredTiles.resize(LEVEL_COUNT);
    if (scoredTiles[index].size() != (size_t)level.w * level.h)
        scoredTiles[index].assign((size_t)level.w * level.h, 0);
    flags = LevelFlags{};
    player = Player{};
    player.pos = TileCenter(level.playerStart);
    player.dir = 0;
    player.paceSetting = movementSetting;
    lastPlayerTile = {-1, -1};
    rings.clear();
    messages.clear();
    msgTimer = 0.f;

    // The building's memory of this area is captured on arrival...
    after.reset(level.w, level.h);
    after.recordPast(level.objects);
    after.unlocked = afterimageLearned || index > 0;
    if (index > 0) afterimageLearned = true;

    // ...and then the present diverges from it.
    if (index == 1) {
        if (Object* j = level.findGlyph('j')) { j->tx += 1; j->ty += 1; }  // the Tenant moved it
        if (Object* d = level.objectAt(23, 7)) d->open = true;           // and left this open
    }
    level.refresh();

    enemy = Enemy{};
    if (level.hasEnemy) {
        enemy.spawn(level);
        const float pressure[] = {0.78f, 1.f, 1.2f};
        enemy.pressure = pressure[std::clamp(threatSetting, 0, 2)];
        enemy.dormant = index == 0 ? 40.f : (index == 3 ? 8.f : 24.f);
    }
    previousEnemyState = enemy.state;
    checkpointInv = inventory;

    switch (index) {
        case 0:
            objective = "Find a way out of Apartment 4C.";
            pushMsg("Rain. Somewhere above you, water is finding its way through the ceiling.");
            pushMsg("You are lying in an apartment you do not remember entering.");
            pushMsg("Something is moving in the living room. The Tenant wakes after 40 seconds.");
            pushMsg("It can replay the last two minutes of your route, including traces from an earlier visit.");
            pushMsg("[WASD] move    [SHIFT] run    [E] interact / read");
            pushMsg("Press [J] any time to review the case file, clues and current lead.");
            break;
        case 1:
            objective = "Find the ARCHIVE key somewhere along the corridor.";
            pushMsg("The door opens onto a corridor far too long for this building.");
            pushMsg("The Tenant is here too. Saved movement traces can make it retrace an earlier route.");
            pushMsg("Break its line of sight, close doors, hide in wardrobes. Running [SHIFT] is loud.");
            break;
        case 2:
            objective = "Read the relay order with Q, then power the terminal.";
            pushMsg("The archive. Shelves of files, each labelled with a date and a name.");
            pushMsg("Every label is your name.");
            pushMsg("It keeps your recent routes between visits. In every area, change paths when it copies you.");
            break;
        case 3:
            objective = "Find Ilse's letter, then reach the outside stair door.";
            pushMsg("The final record room. The Tenant is nearby, but slower to wake.");
            pushMsg("It copies your old posture and hiding places. A red eye-slit gives the imitation away.");
            pushMsg("J opens your case file: room leads, objective and the Tenant's behavior.");
            break;
        default: break;
    }
    screen = showBriefing ? Screen::Briefing : Screen::Playing;
    if (showBriefing) screenTimer = 0.f;
}

void Game::retryArea() {
    visits++;
    inventory = checkpointInv;
    loadLevel(levelIndex);
    fade = 1.f;
    screen = Screen::Playing;
}

void Game::beginTransition(int target) {
    if (pendingLevel < 0) {
        pendingLevel = target;
    }
}

void Game::gameOver() {
    score = std::max(0, score - 250);
    saveHighScore();
    inventory = checkpointInv;
    continueLevel = levelIndex;
    continueScore = score;
    continueVisits = visits;
    continueInventory = checkpointInv;
    BuildLevel(levelIndex, level);
    continuePosition = TileCenter(level.playerStart);
    continueHasPosition = true;
    char save[256];
    const std::string item1 = checkpointInv.size() > 0 ? checkpointInv[0] : "-";
    const std::string item2 = checkpointInv.size() > 1 ? checkpointInv[1] : "-";
    std::snprintf(save, sizeof(save), "%d %d %d %d %d %d %s %s\n", continueLevel, continueScore,
                  continueVisits, (int)checkpointInv.size(), (int)continuePosition.x,
                  (int)continuePosition.y, item1.c_str(), item2.c_str());
    if (!selfTest) SaveFileText(SaveFilePath("afterimage_progress.txt").c_str(), save);
    progressAvailable = true;
    saveHabits();
    if (IsSoundValid(deathStinger)) {
        StopSound(deathStinger);
        SetSoundPitch(deathStinger, 0.96f);
        PlaySound(deathStinger);
    }
    if (IsSoundValid(jumpscareBed)) {
        StopSound(jumpscareBed);
        PlaySound(jumpscareBed);
    }
    screen = Screen::GameOver;
    screenTimer = 0.f;
    gameOverIndex = 0;
    player.hidden = false;
}

void Game::victory() {
    score += 5000;
    saveHighScore();
    if (!selfTest) std::remove(SaveFilePath("afterimage_progress.txt").c_str());
    progressAvailable = false;
    habits.observeEscape();
    saveHabits();
    screen = Screen::Victory;
    screenTimer = 0.f;
    victoryIndex = 0;
}

// =====================================================================================
// Update
// =====================================================================================
void Game::update(float dt) {
    clock += dt;
    screenTimer += dt;
    if (selfTest) runSelfTest();

    lightningTimer -= dt;
    if (lightningTimer <= 0.f) {
        lightning = 0.35f;
        lightningTimer = 12.f + GetRandomValue(0, 160) / 10.f;
    }
    lightning = std::max(0.f, lightning - dt * 1.6f);
    flicker = std::max(0.f, flicker - dt);
    spotFlash = std::max(0.f, spotFlash - dt * 1.2f);
    interactFlash = std::max(0.f, interactFlash - dt * 2.5f);

    switch (screen) {
        case Screen::Title: {
            if (IsKeyPressed(KEY_ESCAPE)) { quit = true; break; }
            if (AnyKeyPressed({KEY_UP, KEY_W})) menuIndex = (menuIndex + 6) % 7;
            if (AnyKeyPressed({KEY_DOWN, KEY_S})) menuIndex = (menuIndex + 1) % 7;
            if (AnyKeyPressed({KEY_ENTER, KEY_E, KEY_SPACE})) {
                if (menuIndex == 0) continueGame();
                else if (menuIndex == 1) newGame();
                else if (menuIndex == 2) { controlsBack = Screen::Title; screen = Screen::Controls; }
                else if (menuIndex == 3) { rulesBack = Screen::Title; screen = Screen::Rules; }
                else if (menuIndex == 4) { settingsBack = Screen::Title; screen = Screen::Settings; }
                else if (menuIndex == 5) screen = Screen::HighScores;
                else quit = true;
            }
        } break;
        case Screen::Controls:
            if (AnyKeyPressed({KEY_ESCAPE, KEY_ENTER, KEY_E, KEY_BACKSPACE, KEY_SPACE})) screen = controlsBack;
            break;
        case Screen::Rules:
        case Screen::HighScores:
            if (AnyKeyPressed({KEY_ESCAPE, KEY_ENTER, KEY_E, KEY_BACKSPACE, KEY_SPACE})) {
                if (screen == Screen::Rules) screen = rulesBack;
                else { saveProgress(); screen = Screen::Title; menuIndex = progressAvailable ? 0 : 1; }
            }
            break;
        case Screen::Settings: {
            if (AnyKeyPressed({KEY_UP, KEY_W})) settingsIndex = (settingsIndex + 3) % 4;
            if (AnyKeyPressed({KEY_DOWN, KEY_S})) settingsIndex = (settingsIndex + 1) % 4;
            bool changed = false;
            const int direction = AnyKeyPressed({KEY_LEFT, KEY_A}) ? -1 : (AnyKeyPressed({KEY_RIGHT, KEY_D}) ? 1 : 0);
            if (direction != 0) {
                if (settingsIndex == 0) movementSetting = std::clamp(movementSetting + direction, 0, 2);
                else if (settingsIndex == 1) threatSetting = std::clamp(threatSetting + direction, 0, 2);
                else if (settingsIndex == 2) messageSetting = std::clamp(messageSetting + direction, 0, 2);
                else screenEffects = !screenEffects;
                player.paceSetting = movementSetting;
                const float pressure[] = {0.78f, 1.f, 1.2f};
                enemy.pressure = pressure[std::clamp(threatSetting, 0, 2)];
                changed = true;
            }
            if (changed) saveSettings();
            if (AnyKeyPressed({KEY_ESCAPE, KEY_ENTER, KEY_E, KEY_BACKSPACE, KEY_SPACE})) screen = settingsBack;
        } break;
        case Screen::Briefing:
            if (AnyKeyPressed({KEY_ENTER, KEY_E, KEY_SPACE})) {
                screen = Screen::Playing;
                screenTimer = 0.f;
                fade = 1.f;
                if (levelIndex == 0) {
                    checkpointInv = inventory;
                    continuePosition = TileCenter(level.playerStart);
                    continueHasPosition = true;
                    continueInventory = inventory;
                }
                saveProgress();
            } else if (IsKeyPressed(KEY_ESCAPE)) {
                screen = Screen::Title;
                menuIndex = progressAvailable ? 0 : 1;
            }
            break;
        case Screen::Playing: updatePlaying(dt); break;
        case Screen::CaseFile:
            if (AnyKeyPressed({KEY_J, KEY_ESCAPE, KEY_ENTER, KEY_SPACE})) screen = Screen::Playing;
            break;
        case Screen::Paused: {
            if (AnyKeyPressed({KEY_UP, KEY_W})) pauseIndex = (pauseIndex + 5) % 6;
            if (AnyKeyPressed({KEY_DOWN, KEY_S})) pauseIndex = (pauseIndex + 1) % 6;
            if (AnyKeyPressed({KEY_ESCAPE, KEY_P})) { saveProgress(); screen = Screen::Playing; break; }
            if (AnyKeyPressed({KEY_ENTER, KEY_E, KEY_SPACE})) {
                if (pauseIndex == 0) { saveProgress(); screen = Screen::Playing; }
                else if (pauseIndex == 1) { settingsBack = Screen::Paused; screen = Screen::Settings; }
                else if (pauseIndex == 2) { rulesBack = Screen::Paused; screen = Screen::Rules; }
                else if (pauseIndex == 3) retryArea();
                else if (pauseIndex == 4) { newGame(); }
                else { saveProgress(); screen = Screen::Title; menuIndex = progressAvailable ? 0 : 1; }
            }
        } break;
        case Screen::Reading:
            if (AnyKeyPressed({KEY_E, KEY_ENTER, KEY_SPACE})) {
                readPage++;
                if (readPage >= (int)readPages.size()) finishReading();
            } else if (IsKeyPressed(KEY_ESCAPE)) {
                finishReading();
            }
            break;
        case Screen::GameOver:
            if (screenTimer >= 5.f) {
                if (AnyKeyPressed({KEY_UP, KEY_W})) gameOverIndex = (gameOverIndex + 2) % 3;
                if (AnyKeyPressed({KEY_DOWN, KEY_S})) gameOverIndex = (gameOverIndex + 1) % 3;
                if (IsKeyPressed(KEY_R)) retryArea();
                else if (IsKeyPressed(KEY_N)) newGame();
                else if (IsKeyPressed(KEY_M)) { screen = Screen::Title; menuIndex = progressAvailable ? 0 : 1; }
                else if (AnyKeyPressed({KEY_ENTER, KEY_E, KEY_SPACE})) {
                    if (gameOverIndex == 0) retryArea();
                    else if (gameOverIndex == 1) newGame();
                    else { screen = Screen::Title; menuIndex = progressAvailable ? 0 : 1; }
                }
            }
            break;
        case Screen::Victory:
            if (screenTimer > 3.f) {
                if (AnyKeyPressed({KEY_UP, KEY_W, KEY_DOWN, KEY_S})) victoryIndex = 1 - victoryIndex;
                if (AnyKeyPressed({KEY_ENTER, KEY_E, KEY_SPACE})) {
                    if (victoryIndex == 0) newGame();
                    else { screen = Screen::Title; menuIndex = progressAvailable ? 0 : 1; }
                } else if (IsKeyPressed(KEY_ESCAPE)) {
                    screen = Screen::Title;
                    menuIndex = progressAvailable ? 0 : 1;
                }
            }
            break;
    }
}

void Game::updatePlaying(float dt) {
    if (AnyKeyPressed({KEY_ESCAPE, KEY_P})) {
        saveProgress();
        saveHabits();
        screen = Screen::Paused;
        pauseIndex = 0;
        return;
    }
    if (IsKeyPressed(KEY_F1)) debug = !debug;
    if (IsKeyPressed(KEY_J)) { screen = Screen::CaseFile; return; }

    // Area transition: fade out, rebuild, fade in.
    if (pendingLevel >= 0) {
        fade += dt * 1.8f;
        if (fade >= 1.f) {
            fade = 1.f;
            int t = pendingLevel;
            pendingLevel = -1;
            loadLevel(t);
            saveProgress();
            saveHabits();
        }
        updateCamera();
        return;
    }
    if (fade > 0.f) fade = std::max(0.f, fade - dt * 0.7f);

    level.refresh();
    player.update(dt, level, true);
    if (levelIndex >= 0 && levelIndex < LEVEL_COUNT) areaPlayTime[levelIndex] += dt;

    // Footfalls follow actual movement; sprint steps are louder and have a metal
    // rattle so running has an audible cost as well as the enemy noise event.
    if (player.moving && !player.hidden) {
        footstepTimer -= dt;
        if (footstepTimer <= 0.f) {
            Sound step = player.sprinting ? sprintClatterSound : footstepSound;
            if (IsSoundValid(step)) {
                SetSoundPitch(step, player.sprinting ? 0.94f + GetRandomValue(0, 12) / 100.f
                                                     : 0.92f + GetRandomValue(0, 16) / 100.f);
                PlaySound(step);
            }
            footstepTimer = player.sprinting ? 0.29f : 0.39f;
        }
    } else {
        footstepTimer = 0.f;
    }

    // Breath becomes audible while stamina is low or the Tenant is actively
    // hunting. A cooldown avoids a constant loop over dialogue and menus.
    const bool strained = player.stamina < 0.32f || enemy.state == EnemyState::Chase;
    if (strained && !player.hidden) {
        breathingTimer -= dt;
        if (breathingTimer <= 0.f) {
            if (IsSoundValid(breathingSound)) {
                SetSoundPitch(breathingSound, 0.9f + GetRandomValue(0, 12) / 100.f);
                PlaySound(breathingSound);
            }
            breathingTimer = 2.5f;
        }
    } else {
        breathingTimer = std::max(0.f, breathingTimer - dt);
    }

    // Store posture and movement so the Tenant can replay the player's habits
    // from precisely two minutes earlier within this area, including a prior run.
    memorySampleTimer -= dt;
    if (memorySampleTimer <= 0.f) {
        memorySampleTimer = 0.25f;
        playerMemory.push_back({areaPlayTime[levelIndex], levelIndex, player.pos, player.dir,
                                player.moving, player.sprinting, player.hidden});
        for (auto it = playerMemory.begin(); it != playerMemory.end();) {
            const float age = areaPlayTime[it->area] - it->time;
            if (age > 132.f) it = playerMemory.erase(it);
            else ++it;
        }
    }
    movementSaveTimer -= dt;
    if (movementSaveTimer <= 0.f) {
        movementSaveTimer = 5.f;
        if (!selfTest && !saveMovementHistory(SaveFilePath("afterimage_movement_history.dat")))
            TraceLog(LOG_WARNING, "Could not periodically save movement history");
    }
    mimicMemoryAge = -1.f;
    currentMimicPlayback = areaPlayTime[levelIndex] - 120.f;
    const float oldestCurrentArea = [&]() {
        for (const MemoryFrame& f : playerMemory) if (f.area == levelIndex) return f.time;
        return areaPlayTime[levelIndex];
    }();
    bool historicalTraceReady = currentMimicPlayback >= oldestCurrentArea;
    if (!historicalTraceReady) currentMimicPlayback = -1.f;
    const MemoryFrame* replay = nullptr;
    for (auto it = playerMemory.rbegin(); it != playerMemory.rend(); ++it) {
        if (historicalTraceReady && it->area == levelIndex && it->time <= currentMimicPlayback + 0.25f &&
            it->time >= currentMimicPlayback - 1.0f) {
            replay = &*it;
            mimicMemoryPos = it->pos;
            mimicMemoryDir = it->dir;
            mimicMemoryAge = areaPlayTime[levelIndex] - it->time;
            break;
        }
    }
    if (!replay) {
        currentMimicPlayback = -1.f;
        historicalTraceReady = false;
    }

    // Freeze a short, ordered route segment from the trace ending at the two-minute
    // echo. The enemy copies this segment once; sliding samples won't rebuild its path.
    std::vector<Point> mimicRoute;
    int mimicRouteIndex = -1;
    if (historicalTraceReady) {
        const float routeStart = currentMimicPlayback - 8.f;
        for (const MemoryFrame& f : playerMemory) {
            if (f.area != levelIndex || f.time < routeStart || f.time > currentMimicPlayback) continue;
            Point sample = ToTile(f.pos);
            if (mimicRoute.empty() || mimicRoute.back() != sample) mimicRoute.push_back(sample);
        }
        if (mimicRoute.size() >= 2) mimicRouteIndex = 0;
    }

    Point pt = ToTile(player.pos);
    if (pt != lastPlayerTile) {
        lastPlayerTile = pt;
        if (levelIndex >= 0 && levelIndex < (int)scoredTiles.size()) {
            const size_t tileIdx = (size_t)pt.y * level.w + pt.x;
            auto& scored = scoredTiles[levelIndex];
            if (tileIdx < scored.size() && !scored[tileIdx]) {
                scored[tileIdx] = 1;
                addScore(10);
            }
        }
        after.notePlayerTile(pt);
    }
    if (pt != lastHabitTile) {
        if (lastHabitTile.x >= 0) habits.observeRoute(levelIndex, lastHabitTile, pt);
        lastHabitTile = pt;
    }

    if (player.sprinting && player.moving) {
        sprintNoiseTimer -= dt;
        if (sprintNoiseTimer <= 0.f) {
            sprintNoiseTimer = 0.55f;
            emitNoise(player.pos, 6.f, true);
            habits.observeSprint();
        }
    } else {
        sprintNoiseTimer = 0.f;
    }

    if (IsKeyPressed(KEY_Q) && after.tryActivate()) flicker = 0.2f;
    after.update(dt);

    if (IsKeyPressed(KEY_E)) {
        if (player.hidden) leaveHide();
        else if (Object* o = nearestInteractable()) interact(*o);
    }

    if (level.hasEnemy && screen == Screen::Playing) {
        enemy.mimicking = mimicMemoryAge >= 0.f && enemy.dormant <= 0.f;
        if (enemy.mimicking) {
            enemy.mimicEcho = mimicMemoryPos;
            enemy.mimicDir = mimicMemoryDir;
            enemy.mimicMoving = replay ? replay->moving : false;
            enemy.mimicSprinting = replay ? replay->sprinting : false;
            if (replay && replay->hiding) enemy.mimicDir = replay->dir;
        } else {
            enemy.mimicking = false;
        }
        EnemyEvents ev = enemy.update(dt, level, player, after, mimicRoute, mimicRouteIndex, habits);
        if (ev.spotted) spotFlash = 1.f;
        if (ev.attackStarted) pushUrgent("The Tenant reaches for you. RUN.");
        if (ev.caught) {
            gameOver();
            return;
        }
    }

    if (previousEnemyState == EnemyState::Chase && enemy.state == EnemyState::Search) habits.observeEscape();
    previousEnemyState = enemy.state;
    updateScripts(dt);

    if (!messages.empty()) {
        msgTimer += dt;
        const float hold[] = {1.4f, 1.f, 0.75f};
        float dur = (4.0f + messages.front().size() * 0.055f) * hold[std::clamp(messageSetting, 0, 2)];
        if (messages.size() > 4) dur *= 0.9f;
        if (msgTimer >= dur) {
            messages.pop_front();
            msgTimer = 0.f;
        }
    }
    for (auto& r : rings) r.t += dt;
    rings.erase(std::remove_if(rings.begin(), rings.end(), [](const NoiseRing& r) { return r.t > 0.8f; }),
                rings.end());
    updateCamera();
}

void Game::updateCamera() {
    float halfW = VIRTUAL_W / 2.f, halfH = VIRTUAL_H / 2.f;
    float mapW = (float)level.w * TILE, mapH = (float)level.h * TILE;
    float tx = player.pos.x, ty = player.pos.y;
    tx = mapW <= VIRTUAL_W ? mapW / 2.f : std::clamp(tx, halfW, mapW - halfW);
    ty = mapH <= VIRTUAL_H ? mapH / 2.f : std::clamp(ty, halfH, mapH - halfH);
    cam.target = {std::floor(tx), std::floor(ty)};
}

// =====================================================================================
// Scripted events per area
// =====================================================================================
void Game::updateScripts(float dt) {
    Point pt = ToTile(player.pos);
    if (levelIndex == 0) {
        // Entering the living room: a cabinet shifts in the dark -> afterimage tutorial.
        if (!flags.livingTriggered && InRect(pt, 9, 1, 28, 7)) {
            flags.livingTriggered = true;
            flicker = 0.9f;
            if (Object* b = level.findGlyph('b')) b->ty += 1;
            level.refresh();
            after.unlocked = true;
            afterimageLearned = true;
            pushUrgent("The bulb overhead stutters. Something scraped across the floor in the dark.");
            pushMsg("Something in this room is not where it was. Press [Q] to see the AFTERIMAGE - "
                    "the room as it was before.");
            objective = "Press [Q] to reveal the ghost cabinet position. Search the red-outlined moved cabinet for the Fuse.";
        }
        // After power is restored, the Tenant is glimpsed in the hall.
        if (flags.powerOn && !flags.revealShown && InRect(pt, 1, 9, 7, 13)) {
            flags.revealShown = true;
            flags.apparition = 2.6f;
            flicker = 0.5f;
            spotFlash = 0.6f;
            pushUrgent("Something tall is standing in the hall. It holds itself exactly the way you do.");
            pushMsg("The light blinks, and it is gone.");
        }
        if (flags.apparition > 0.f) flags.apparition -= dt;
    } else if (levelIndex == 1) {
        // The repeating corridor: walking off the east end puts you back at the west end.
        if (pt.x >= level.w - 1) {
            player.pos = TileCenter({1, pt.y});
            flags.loopCount++;
            flicker = 0.6f;
            lastPlayerTile = {-1, -1};
            pushUrgent("The corridor begins again. The same doors. The same stains.");
            if (flags.loopCount == 1)
        pushMsg("Forward only leads back here. Q reveals which crate the Tenant moved; search the red outline for the key.");
        }
    }
}

// =====================================================================================
// Interaction
// =====================================================================================
Object* Game::nearestInteractable() {
    Object* best = nullptr;
    float bestScore = 1e9f;
    Vector2 f = player.facingVector();
    for (Object& o : level.objects) {
        if (!o.isInteractable()) continue;
        Rectangle r{(float)o.tx * TILE, (float)o.ty * TILE, (float)TILE, (float)TILE};
        float cx = std::clamp(player.pos.x, r.x, r.x + r.width);
        float cy = std::clamp(player.pos.y, r.y, r.y + r.height);
        float d = Dist(player.pos, {cx, cy});
        if (d > 9.f) continue;
        Vector2 to = {r.x + 8.f - player.pos.x, r.y + 8.f - player.pos.y};
        float l = std::max(0.001f, std::hypot(to.x, to.y));
        float facingBonus = (to.x / l * f.x + to.y / l * f.y) * 4.f;
        float score = d - facingBonus;
        if (score < bestScore) {
            bestScore = score;
            best = &o;
        }
    }
    return best;
}

std::string Game::promptFor(const Object& o) const {
    switch (o.type) {
        case ObjType::Door: return o.locked ? "[E] Try the door" : (o.open ? "[E] Close door" : "[E] Open door");
        case ObjType::Exit: return std::string(o.locked ? "[E] Try the " : "[E] Go through the ") + o.name;
        case ObjType::Cabinet: return o.searched ? "Cabinet (searched)" : "[E] Search cabinet";
        case ObjType::Crate: return o.searched ? "Crate (searched)" : "[E] Search crate";
        case ObjType::FuseBox: return o.on ? "Fuse box (humming)" : "[E] Use fuse box";
        case ObjType::Wardrobe: return "[E] Hide in wardrobe";
        case ObjType::Alcove: return "[E] Hide in the dark alcove";
        case ObjType::Note: return "[E] Read: " + o.name;
        case ObjType::Furniture: return "[E] Inspect " + o.name;
        case ObjType::Switch: return o.on ? "Relay lever (thrown)" : "[E] Throw relay lever";
        case ObjType::Terminal: return "[E] Use terminal";
        case ObjType::Shelf: return "";
    }
    return "";
}

void Game::interact(Object& o) {
    switch (o.type) {
        case ObjType::Door: {
            if (o.locked) { pushMsg(o.lockedMsg.empty() ? "Locked." : o.lockedMsg); flash(Pal::Crimson); return; }
            if (o.open) {
                Rectangle tile{(float)o.tx * TILE, (float)o.ty * TILE, (float)TILE, (float)TILE};
                bool blocked = CheckCollisionRecs(tile, player.hitboxAt(player.pos)) ||
                               (level.hasEnemy && ToTile(enemy.pos) == Point{o.tx, o.ty});
                if (blocked) { pushMsg("Something is in the way."); flash(Pal::Crimson); return; }
            }
            o.open = !o.open;
            flash(Pal::Lilac);
        } break;

        case ObjType::Exit: {
            if (o.locked) {
                if (!o.needItem.empty() && hasItem(o.needItem)) {
                    o.locked = false;
                    pushMsg("The " + o.needItem + " turns. The " + o.name + " unlocks.");
                    flash(Pal::PaleGreen);
                } else {
                    pushMsg(o.lockedMsg.empty() ? "It won't open." : o.lockedMsg);
                    flash(Pal::Crimson);
                }
                return;
            }
            if (o.target >= 0) beginTransition(o.target);
            else if (o.target == -1) victory();
        } break;

        case ObjType::Cabinet:
        case ObjType::Crate: {
            if (o.searched) { pushMsg("You already searched it."); return; }
            o.searched = true;
            if (!o.holds.empty()) {
                addItem(o.holds);
                addScore(250);
                flash(Pal::PaleGreen);
                if (o.holds == "Fuse") {
                    pushMsg("Behind the moved cabinet, wedged in a drawer: a ceramic FUSE.");
                    objective = "Restore power. The fuse box is in the kitchen (south-east).";
                } else if (o.holds == "Brass Key") {
                    pushMsg("Under the moved crate: a BRASS KEY on a tag stamped ARCHIVE.");
                    objective = "Unlock the ARCHIVE door on the far landing (north wall, east end).";
                } else if (o.holds == "Ilse's letter") {
                    pushMsg("Inside the drawer: Ilse's last letter, addressed to you.");
                    objective = "Use Ilse's letter at the stair door to release the latch.";
                } else {
                    pushMsg("You found: " + o.holds);
                }
            } else {
                flash(Pal::Crimson);
                if (o.type == ObjType::Crate) {
                    pushMsg("Old newspapers and a broken lamp. The lid clatters - too loud.");
                    emitNoise(TileCenter({o.tx, o.ty}), 8.f);
                } else {
                    pushMsg("Empty. Dust, and a smell of wet plaster.");
                }
            }
        } break;

        case ObjType::FuseBox: {
            if (o.on) { pushMsg("The fuse box hums steadily."); return; }
            if (hasItem(o.needItem)) {
                removeItem(o.needItem);
                o.on = true;
                flags.powerOn = true;
                flicker = 0.4f;
                flash(Pal::PaleGreen);
                if (Object* x = level.findGlyph('X')) x->locked = false;
                pushMsg("You push the fuse home. Somewhere a lock disengages with a heavy click.");
                objective = "Leave through the front door (west side of the hall).";
            } else {
                pushMsg("A fuse box. One slot is empty - the power is out.");
                flash(Pal::Crimson);
            }
        } break;

        case ObjType::Wardrobe:
        case ObjType::Alcove: enterHide(o); break;

        case ObjType::Note:
            if (!o.searched) { o.searched = true; addScore(100); }
            openReading(o.name, {o.text}, ReadAction::None);
            break;

        case ObjType::Furniture: pushMsg(o.text); break;

        case ObjType::Switch: useSwitch(o); break;

        case ObjType::Terminal: {
            if (!o.on) {
                pushMsg("A dead screen. Four relay indicators beneath it, all dark.");
                flash(Pal::Crimson);
                return;
            }
            openReading("TERMINAL",
                        {"> HALVORSEN COURT RESIDUE SYSTEM\n> RELAYS: 4/4 ONLINE\n\n> QUERY: WHO IS THE TENANT?",
                         "> THE TENANT IS NOT A RESIDENT.\n> THE TENANT IS AN ACCUMULATION.\n\n"
                         "> 47 VISITS. 47 RESIDUES. ONE SHAPE.\n> SHAPE MATCHES: YOU.",
                         "You came here looking for Ilse.\n\nIlse left this building years ago. She is "
                         "alive. She wrote to you, again and again.\n\nYou are the one who never left. "
                         "Every visit, you left another afterimage behind - and they learned to walk.\n\n"
                         "It is not hunting you. It is trying to become you.",
                         "> EXIT MAGLOCK: RELEASED.\n> CONTAINMENT: RELEASED.\n\n> RUN."},
                        ReadAction::TerminalReveal);
        } break;

        case ObjType::Shelf: break;
    }
}

void Game::useSwitch(Object& o) {
    if (o.on) { pushMsg("That lever is already thrown."); return; }
    int expected = flags.switchStep + 1;
    if (o.echoOrder == expected) {
        o.on = true;
        flags.switchStep++;
        addScore(150);
        flash(Pal::PaleGreen);
        pushMsg(TextFormat("The relay locks into place. (%d/4)", flags.switchStep));
        if (flags.switchStep == 4) {
            flags.terminalOn = true;
            addScore(1000);
            if (Object* t = level.findGlyph('T')) t->on = true;
            flicker = 0.4f;
            pushMsg("Down the aisles, a screen flickers green.");
            objective = "The terminal is awake (south side of the archive). Use it.";
        }
    } else {
        for (Object& s : level.objects)
            if (s.type == ObjType::Switch) s.on = false;
        flags.switchStep = 0;
        flash(Pal::Crimson);
        spotFlash = 0.35f;
        pushUrgent("The levers snap back with a crack. Something heard that.");
        pushMsg("The building remembers the right order. Press [Q] near the levers.");
        emitNoise(TileCenter({o.tx, o.ty}), 10.f);
    }
}

void Game::enterHide(Object& o) {
    if (level.hasEnemy) enemy.onPlayerHide(level, player, after);
    if (o.type == ObjType::Wardrobe || o.type == ObjType::Alcove) habits.observeHide(levelIndex, {o.tx, o.ty});
    player.preHidePos = player.pos;
    player.hidden = true;
    player.hideObj = o.id;
    player.pos = TileCenter({o.tx, o.ty});
    pushMsg(o.type == ObjType::Wardrobe ? "You pull the wardrobe door shut. Breathe slowly. [E] to leave."
                                        : "You press yourself into the dark. [E] to leave.");
    if (levelIndex == 0) pushMsg("You hope you will never need to do this for real.");
}

void Game::leaveHide() {
    player.hidden = false;
    player.pos = player.preHidePos;
    player.hideObj = -1;
}

void Game::openReading(const std::string& title, const std::vector<std::string>& pages, ReadAction action) {
    readTitle = title;
    readPages = pages;
    readPage = 0;
    readAction = action;
    screen = Screen::Reading;
}

void Game::finishReading() {
    ReadAction a = readAction;
    readAction = ReadAction::None;
    screen = Screen::Playing;
    if (a == ReadAction::TerminalReveal && !flags.finalChase) {
        flags.finalChase = true;
        if (Object* z = level.findGlyph('Z')) z->locked = false;
        // The Tenant unfolds from the far corner and hunts you to the exit.
        enemy.pos = TileCenter(level.enemySpawn);
        enemy.forceChase(6.f);
        enemy.dormant = 1.2f;
        spotFlash = 1.f;
        flicker = 0.8f;
        objective = "The final record room is open. Reach its exit beyond the Tenant.";
        pushUrgent("The shelves shudder. Something tall unfolds between them. RUN.");
    }
}

// =====================================================================================
// Helpers
// =====================================================================================
void Game::emitNoise(Vector2 pos, float radiusTiles, bool sprintNoise) {
    if (sprintNoise) {
        // Repeated sprinting leaves a persistent, bounded acoustic signature.
        // Noise remains tied to the present event; the profile only changes how
        // far that particular sprint can be heard.
        radiusTiles *= habits.sprintNoiseMultiplier();
    }
    rings.push_back({pos, radiusTiles * TILE, 0.f});
    if (level.hasEnemy) enemy.hearNoise(pos, radiusTiles * TILE);
}

void Game::pushMsg(const std::string& m) {
    if (!messages.empty() && messages.back() == m) return;
    messages.push_back(m);
    if (messages.size() == 1) msgTimer = 0.f;
}

void Game::pushUrgent(const std::string& m) {
    messages.clear();
    msgTimer = 0.f;
    pushMsg(m);
}

std::string Game::afterimageHint() const {
    if (levelIndex == 0 && flags.livingTriggered && !hasItem("Fuse"))
        return "Ghost cabinet = old place. Search the red-outlined moved cabinet for the Fuse.";
    if (levelIndex == 1 && !hasItem("Brass Key"))
        return "Ghost crate = old place. Search the red-outlined moved crate for the key.";
    if (levelIndex == 2 && !flags.terminalOn)
        return "Green numbers tell the lever order. Pull each numbered lever, then use the terminal.";
    if (levelIndex == 2 && flags.terminalOn)
        return "The terminal released the exit. Reach the north-west door.";
    return "Ghosts show past positions; red marks changes. Use these clues to progress.";
}

bool Game::hasItem(const std::string& it) const {
    return std::find(inventory.begin(), inventory.end(), it) != inventory.end();
}
void Game::addItem(const std::string& it) {
    if (!hasItem(it)) inventory.push_back(it);
}
void Game::removeItem(const std::string& it) {
    inventory.erase(std::remove(inventory.begin(), inventory.end(), it), inventory.end());
}
void Game::flash(Color c) {
    interactFlash = 1.f;
    interactFlashColor = c;
}

void Game::addScore(int points) { score = std::max(0, score + points); }

void Game::loadSettings() {
    const std::string path = SaveFilePath("afterimage_settings.cfg");
    if (!FileExists(path.c_str())) return;
    char* text = LoadFileText(path.c_str());
    if (!text) return;
    int movement = movementSetting, threat = threatSetting, messages = messageSetting, effects = screenEffects ? 1 : 0;
    if (std::sscanf(text, "%d %d %d %d", &movement, &threat, &messages, &effects) == 4) {
        movementSetting = std::clamp(movement, 0, 2);
        threatSetting = std::clamp(threat, 0, 2);
        messageSetting = std::clamp(messages, 0, 2);
        screenEffects = effects != 0;
    }
    UnloadFileText(text);
}

void Game::saveSettings() const {
    char text[96];
    std::snprintf(text, sizeof(text), "%d %d %d %d\n", movementSetting, threatSetting, messageSetting,
                  screenEffects ? 1 : 0);
    const std::string path = SaveFilePath("afterimage_settings.cfg");
    SaveFileText(path.c_str(), text);
}

void Game::loadHighScore() {
    const std::string path = SaveFilePath("afterimage_highscore.txt");
    if (!FileExists(path.c_str())) return;
    char* text = LoadFileText(path.c_str());
    if (!text) return;
    int saved = 0;
    if (std::sscanf(text, "%d", &saved) == 1) highScore = std::max(0, saved);
    UnloadFileText(text);
}

void Game::saveHighScore() {
    if (selfTest || score <= highScore) return;
    highScore = score;
    char text[48];
    std::snprintf(text, sizeof(text), "%d\n", highScore);
    const std::string path = SaveFilePath("afterimage_highscore.txt");
    SaveFileText(path.c_str(), text);
}

// =====================================================================================
// Automated self test (run with --selftest). Checks map reachability and puzzle logic,
// exercises the enemy AI, and saves screenshots of every screen.
// =====================================================================================
void Game::check(bool cond, const char* what) {
    std::printf("[SELFTEST] %s: %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond) testFailures++;
}

void Game::validateLevels() {
    for (int i = 0; i < LEVEL_COUNT; ++i) {
        Level lv;
        BuildLevel(i, lv);
        // Unlock everything so we test geometry, not progression.
        for (Object& o : lv.objects) o.locked = false;
        lv.refresh();
        int unreachable = 0;
        for (const Object& o : lv.objects) {
            if (!o.isInteractable()) continue;
            if (o.target == -2) continue;  // decorative "way back" doors
            bool ok = false;
            const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
            for (int k = 0; k < 4 && !ok; ++k) {
                Point n{o.tx + dx[k], o.ty + dy[k]};
                if (!lv.passableForEnemy(n.x, n.y)) continue;
                if (n == lv.playerStart || !FindPath(lv, lv.playerStart, n).empty()) ok = true;
            }
            if (!o.isSolid() && (Point{o.tx, o.ty} == lv.playerStart ||
                                 !FindPath(lv, lv.playerStart, {o.tx, o.ty}).empty()))
                ok = true;
            if (!ok) {
                std::printf("[SELFTEST] level %d: '%c' %s at (%d,%d) unreachable\n", i, o.glyph,
                            o.name.c_str(), o.tx, o.ty);
                unreachable++;
            }
        }
        for (const Point& wp : lv.waypoints)
            if (!lv.passableForEnemy(wp.x, wp.y)) unreachable++;
        check(unreachable == 0, TextFormat("level %d: all objects/waypoints reachable", i));
    }
}

void Game::runSelfTest() {
    testFrame++;
    auto shot = [this](const char* n) { pendingShot = n; };
    switch (testFrame) {
        case 3: {
            check(IsSoundValid(deathStinger) || !IsAudioDeviceReady(),
                  "death stinger loads when an audio device is available");
            check((IsSoundValid(footstepSound) && IsSoundValid(sprintClatterSound) && IsSoundValid(breathingSound)) ||
                      !IsAudioDeviceReady(),
                  "footstep, sprint clatter, and breathing cues load when audio is available");
            check(IsSoundValid(jumpscareBed) || !IsAudioDeviceReady(),
                  "five-second jumpscare horror bed loads when audio is available");
            // Verify that a repeated route survives serialization and changes an
            // actual enemy decision, then exercise ordered historical playback.
            HabitProfile learned;
            Point learnedTile{-1, -1};
            Level aiLevel;
            BuildLevel(2, aiLevel);
            const Point origin = aiLevel.enemySpawn;
            Player hiddenPlayer;
            hiddenPlayer.pos = TileCenter({1, 1});
            hiddenPlayer.hidden = true;
            Afterimage emptyHeat;
            for (int y = 1; y < aiLevel.h - 1 && learnedTile.x < 0; ++y)
                for (int x = 1; x < aiLevel.w - 1; ++x) {
                    Point p{x, y};
                    if (p != origin && aiLevel.passableForEnemy(x, y) &&
                        !FindPath(aiLevel, origin, p).empty()) { learnedTile = p; break; }
                }
            if (learnedTile.x >= 0) {
                for (int i = 0; i < 4; ++i) learned.observeRoute(2, origin, learnedTile);
                for (int i = 0; i < 8; ++i) learned.observeSprint();
                const std::string profilePath = AppFile("afterimage_ai_profile_test.dat");
                const bool saved = learned.save(profilePath);
                HabitProfile reloaded;
                const bool loaded = reloaded.load(profilePath);
                check(saved && loaded && reloaded.routeUse(2, learnedTile) == 4,
                      "adaptive route observations persist and reload");
                check(reloaded.noisySprints == 8 && reloaded.sprintNoiseMultiplier() > 1.f,
                      "repeated sprinting persists as a larger audible signature");
                std::remove(profilePath.c_str());
                for (int i = 0; i < 1200; ++i) learned.observeRoute(2, origin, learnedTile);
                check(learned.routes.front().count == 1000,
                      "adaptive route counts remain bounded");

                const std::string corruptPath = AppFile("afterimage_ai_profile_bad_test.dat");
                const std::string corruptData = "AFTERIMAGE_AI 1 99 0 0 0\n";
                std::vector<char> corruptBytes(corruptData.begin(), corruptData.end());
                corruptBytes.push_back('\0');
                SaveFileText(corruptPath.c_str(), corruptBytes.data());
                HabitProfile corrupted;
                check(!corrupted.load(corruptPath), "corrupt AI profile is rejected safely");
                std::remove(corruptPath.c_str());

                Enemy baseline;
                baseline.spawn(aiLevel);
                baseline.dormant = 0.f;
                bool baselineUsedLearnedRoute = false;
                static const std::vector<Point> noRoute;
                for (int i = 0; i < 1100 && !baselineUsedLearnedRoute; ++i) {
                    baseline.update(1.f / 60.f, aiLevel, hiddenPlayer, emptyHeat, noRoute, -1, HabitProfile{});
                    baselineUsedLearnedRoute = baseline.decisionReason &&
                        std::string(baseline.decisionReason) == "repeated route tendency";
                }
                check(!baselineUsedLearnedRoute,
                      "baseline patrol has no repeated-route decision without learning");

                Enemy adapting;
                adapting.spawn(aiLevel);
                adapting.dormant = 0.f;
                bool selectedLearnedRoute = false;
                for (int i = 0; i < 1100 && !selectedLearnedRoute; ++i) {
                    adapting.update(1.f / 60.f, aiLevel, hiddenPlayer, emptyHeat, noRoute, -1, reloaded);
                    selectedLearnedRoute = adapting.decisionReason &&
                        std::string(adapting.decisionReason) == "repeated route tendency" &&
                        adapting.targetTile == learnedTile;
                }
                check(selectedLearnedRoute,
                      "repeated saved route changes a future patrol decision");
            } else {
                check(false, "adaptive route test found a reachable tile");
                check(false, "repeated saved route changes a future patrol decision");
            }

            // A short route is intentionally supplied in chronological order.
            // The enemy must enter the dedicated playback state and advance its
            // route cursor through actual pathfinding.
            std::vector<Point> route{origin};
            for (int y = 1; y < aiLevel.h - 1 && route.size() < 4; ++y)
                for (int x = 1; x < aiLevel.w - 1 && route.size() < 4; ++x) {
                    Point p{x, y};
                    if (p != route.back() && aiLevel.passableForEnemy(x, y) &&
                        !FindPath(aiLevel, route.back(), p).empty()) route.push_back(p);
                }
            Enemy mimicker;
            mimicker.spawn(aiLevel);
            mimicker.dormant = 0.f;
            mimicker.mimicking = true;
            bool enteredPlayback = false, advancedPlayback = false;
            for (int i = 0; i < 600; ++i) {
                mimicker.update(1.f / 60.f, aiLevel, hiddenPlayer, emptyHeat, route, 0, learned);
                enteredPlayback |= mimicker.state == EnemyState::Mimic;
                advancedPlayback |= mimicker.mimicRouteIndex > 0;
            }
            check(route.size() >= 3 && enteredPlayback && advancedPlayback,
                  "mimic follows and advances through an ordered historical route");

            Enemy stationary;
            stationary.spawn(aiLevel);
            stationary.dormant = 0.f;
            stationary.mimicking = true;
            const std::vector<Point> stillRoute{origin, origin, origin};
            stationary.update(1.f / 60.f, aiLevel, hiddenPlayer, emptyHeat, stillRoute, 0, learned);
            stationary.update(1.f / 60.f, aiLevel, hiddenPlayer, emptyHeat, stillRoute, 0, learned);
            check(stationary.state == EnemyState::Return,
                  "stationary historical samples collapse without stalling mimic mode");

            Point blocked{-1, -1};
            for (int y = 0; y < aiLevel.h && blocked.x < 0; ++y)
                for (int x = 0; x < aiLevel.w; ++x)
                    if (!aiLevel.passableForEnemy(x, y)) { blocked = {x, y}; break; }
            Enemy blockedMimic;
            blockedMimic.spawn(aiLevel);
            blockedMimic.dormant = 0.f;
            blockedMimic.mimicking = true;
            const std::vector<Point> blockedRoute{origin, blocked};
            for (int i = 0; i < 3; ++i)
                blockedMimic.update(1.f / 60.f, aiLevel, hiddenPlayer, emptyHeat, blockedRoute, 0, learned);
            check(blocked.x >= 0 && blockedMimic.state == EnemyState::Return,
                  "unreachable historical sample cancels playback safely");

            Enemy attacker;
            attacker.spawn(aiLevel);
            attacker.dormant = 0.f;
            Player exposed;
            exposed.pos = attacker.pos;
            int catches = 0;
            bool enteredAttack = false, attackStayedCommitted = true;
            for (int i = 0; i < 90; ++i) {
                EnemyEvents events = attacker.update(1.f / 60.f, aiLevel, exposed, emptyHeat);
                if (attacker.state == EnemyState::Attack) enteredAttack = true;
                else if (enteredAttack) attackStayedCommitted = false;
                if (events.caught) { ++catches; break; }
            }
            check(catches == 1 && enteredAttack && attackStayedCommitted,
                  "attack wind-up remains committed and emits one catch event");
        } break;
        case 2: {
            const int movement = movementSetting, threat = threatSetting, messages = messageSetting;
            const bool effects = screenEffects;
            saveSettings();
            movementSetting = (movementSetting + 1) % 3;
            threatSetting = (threatSetting + 1) % 3;
            messageSetting = (messageSetting + 1) % 3;
            screenEffects = !screenEffects;
            loadSettings();
            check(movementSetting == movement && threatSetting == threat && messageSetting == messages &&
                      screenEffects == effects,
                  "settings persist beside the executable");
        } break;
        case 20: shot("selftest_01_title.png"); break;
        case 21: screen = Screen::Rules; break;
        case 22: shot("selftest_11_rules.png"); break;
        case 23: screen = Screen::Settings; break;
        case 24: shot("selftest_12_settings.png"); break;
        case 25: screen = Screen::HighScores; break;
        case 26: shot("selftest_13_high_scores.png"); break;
        case 27: screen = Screen::Title; break;
        case 28: newGame(); screen = Screen::Briefing; shot("selftest_10_briefing.png"); break;
        case 29: screen = Screen::Playing; shot("selftest_10_briefing.png"); break;
        case 200: shot("selftest_02_apartment.png"); break;
        case 205: player.pos = TileCenter({15, 4}); break;  // walk into the living room
        case 215:
            check(flags.livingTriggered && after.unlocked, "living room trigger unlocks afterimage");
            check(level.findGlyph('b') && after.moved(*level.findGlyph('b')), "cabinet b registered as moved");
            after.tryActivate();
            break;
        case 250: shot("selftest_03_afterimage.png"); break;
        case 255: {
            player.pos = TileCenter({19, 4});
            Object* b = level.findGlyph('b');
            if (b) interact(*b);
            check(hasItem("Fuse"), "searching moved cabinet yields fuse");
            Object* f = level.findGlyph('F');
            if (f) interact(*f);
            Object* x = level.findGlyph('X');
            check(flags.powerOn && x && !x->locked, "fuse box restores power and unlocks front door");
        } break;
        case 260: loadLevel(1); fade = 0.f; break;
        case 300: shot("selftest_04_corridor.png"); break;
        case 305: {
            Object* j = level.findGlyph('j');
            check(j && after.moved(*j), "corridor key crate differs from snapshot");
            // Put the player in the Tenant's view to test detection -> chase.
            enemy.dormant = 0.f;
            enemy.pos = TileCenter({22, 5});
            enemy.facing = {-1, 0};
            player.pos = TileCenter({19, 5});
        } break;
        case 400:
            check(enemy.state == EnemyState::Chase || screen == Screen::GameOver,
                  TextFormat("enemy detects player (state=%s)", EnemyStateName(enemy.state)));
            shot("selftest_05_chase.png");
            break;
        case 405:
            screen = Screen::Playing;
            loadLevel(1);
            fade = 0.f;
            enemy.dormant = 0.f;
            // Noise test: a wrong crate is searched within earshot.
            enemy.pos = TileCenter({12, 5});
            player.pos = TileCenter({7, 3});
            if (Object* c = level.findGlyph('c')) interact(*c);
            check(enemy.state == EnemyState::Investigate, "noisy crate makes enemy investigate");
            player.pos = TileCenter({2, 3});  // tuck the player away
            break;
        case 406: if (Object* w = level.findGlyph('W')) enterHide(*w); break;
        case 1100:
            check(enemy.state == EnemyState::Return || enemy.state == EnemyState::Patrol ||
                      enemy.state == EnemyState::Search,
                  TextFormat("enemy gives up and returns (state=%s)", EnemyStateName(enemy.state)));
            check(screen == Screen::Playing, "hidden player not caught");
            leaveHide();
            break;
        case 1110: {
            loadLevel(2);
            fade = 0.f;
            enemy.dormant = 100.f;
            const char order[4] = {'c', 'a', 'd', 'b'};
            Object* a = level.findGlyph('a');
            if (a) useSwitch(*a);  // wrong first lever -> reset
            check(flags.switchStep == 0, "wrong lever resets the sequence");
            for (char c : order)
                if (Object* s = level.findGlyph(c)) useSwitch(*s);
            Object* t = level.findGlyph('T');
            check(flags.switchStep == 4 && t && t->on, "correct lever order powers terminal");
            for (Object& s : level.objects)
                if (s.type == ObjType::Switch) s.on = false;
            flags.switchStep = 0;
            if (t) t->on = false;
            after.tryActivate();
        } break;
        case 1160: shot("selftest_06_archive_afterimage.png"); break;
        case 1165: {
            Object* t = level.findGlyph('T');
            if (t) t->on = true;
            openReading("TERMINAL", {"test"}, ReadAction::TerminalReveal);
            finishReading();
            Object* z = level.findGlyph('Z');
            check(z && !z->locked && z->target == 3 && enemy.state == EnemyState::Chase,
                  "terminal opens the final level and starts the chase");
        } break;
        case 1200: shot("selftest_07_final_chase.png"); break;
        case 1205: gameOver(); break;
        case 1206:
            check(screen == Screen::GameOver, "caught player enters the game-over sequence");
            shot("selftest_death_jumpscare.png");
            break;
        case 1207:
            check(screen == Screen::GameOver && screenTimer < 5.f,
                  "full-screen creature jumpscare is held for five seconds");
            break;
        case 1240: shot("selftest_08_gameover.png"); break;
        case 1245: victory(); break;
        case 1246: shot("selftest_09_victory.png"); break;
        case 1420:
            loadLevel(3, true);
            break;
        case 1421: {
            screen = Screen::Playing;
            Object* drawer = level.findGlyph('c');
            Object* stair = level.findGlyph('Z');
            if (drawer) interact(*drawer);
            if (stair) interact(*stair);
            bool unlockedWithLetter = stair && !stair->locked && hasItem("Ilse's letter");
            if (stair) interact(*stair);
            check(unlockedWithLetter && screen == Screen::Victory,
                  "final room letter unlocks the outside stair");
            shot("selftest_14_residue_ward.png");
            break;
        }
        case 1422: shot("selftest_15_residue_ending.png"); break;
        case 1450:
            std::printf("[SELFTEST] finished with %d failure(s)\n", testFailures);
            quit = true;
            break;
        default: break;
    }
}

void Game::afterDraw() {
    if (!pendingShot.empty()) {
        const std::string path = AppFile(pendingShot.c_str());
        Image screenshot = LoadImageFromScreen();
        // raylib 5.5 returns DPI-scaled pixels here, while this pixel-art game is
        // composed in logical window coordinates. Crop to the logical render area.
        const int screenW = GetScreenWidth(), screenH = GetScreenHeight();
        if (IsImageValid(screenshot) && screenshot.width >= screenW && screenshot.height >= screenH) {
            const int offsetY = screenshot.height - screenH;
            ImageCrop(&screenshot, {0.f, (float)offsetY, (float)screenW, (float)screenH});
        }
        if (!ExportImage(screenshot, path.c_str()))
            TraceLog(LOG_WARNING, "Self-test screenshot could not be exported: %s", path.c_str());
        UnloadImage(screenshot);
        pendingShot.clear();
    }
}
