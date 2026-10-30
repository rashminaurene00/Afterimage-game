// AFTERIMAGE - HUD and menus, laid out in a 640x360 virtual space using raylib's
// built-in pixel font at integer multiples of its 10px base size.
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include "Config.h"
#include "Game.h"

namespace {

std::vector<std::string> Wrap(const std::string& text, int size, int maxW) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start <= text.size()) {
        size_t nl = text.find('\n', start);
        if (nl == std::string::npos) nl = text.size();
        std::string para = text.substr(start, nl - start);
        if (para.empty()) {
            lines.push_back("");
        } else {
            std::string line;
            size_t pos = 0;
            while (pos < para.size()) {
                size_t sp = para.find(' ', pos);
                if (sp == std::string::npos) sp = para.size();
                std::string word = para.substr(pos, sp - pos);
                std::string test = line.empty() ? word : line + " " + word;
                if (MeasureText(test.c_str(), size) > maxW && !line.empty()) {
                    lines.push_back(line);
                    line = word;
                } else {
                    line = test;
                }
                pos = sp + 1;
            }
            lines.push_back(line);
        }
        start = nl + 1;
    }
    return lines;
}

void Shadow(const char* t, int x, int y, int s, Color c) {
    DrawText(t, x + 1, y + 1, s, Pal::Black);
    DrawText(t, x, y, s, c);
}

void Panel(Rectangle r, Color border, float alpha = 0.82f) {
    DrawRectangleRec(r, ColorAlpha(Pal::Black, alpha));
    DrawRectangleLinesEx(r, 1, border);
}

int CenterX(const char* t, int s) { return (UI_W - MeasureText(t, s)) / 2; }

void Bar(int x, int y, int w, int h, float v, Color fill, Color back) {
    DrawRectangle(x, y, w, h, back);
    DrawRectangle(x, y, (int)(w * (v < 0 ? 0 : (v > 1 ? 1 : v))), h, fill);
    DrawRectangleLines(x - 1, y - 1, w + 2, h + 2, ColorAlpha(Pal::Muted, 0.8f));
}

// The title, drawn with offset "ghost" copies - a literal afterimage.
void GhostTitle(const char* t, int y, int size, float clock) {
    int x = CenterX(t, size);
    int jitter = ((int)(clock * 3.f) % 7 == 0) ? 2 : 0;
    DrawText(t, x - 3 - jitter, y + 1, size, ColorAlpha(Pal::Crimson, 0.55f));
    DrawText(t, x + 3 + jitter, y - 1, size, ColorAlpha(Pal::Lilac, 0.35f));
    DrawText(t, x, y, size, Pal::Bone);
}

}  // namespace

void Game::drawUI() {
    switch (screen) {
        case Screen::Title: drawTitleMenu(); break;
        case Screen::Controls: drawControls(); break;
        case Screen::Rules: drawRules(); break;
        case Screen::Settings: drawSettings(); break;
        case Screen::HighScores: drawHighScores(); break;
        case Screen::Briefing: drawBriefing(); break;
        case Screen::Playing: drawHud(); break;
        case Screen::CaseFile: drawHud(); drawCaseFile(); break;
        case Screen::Paused: drawHud(); drawPause(); break;
        case Screen::Reading: drawHud(); drawReading(); break;
        case Screen::GameOver: drawGameOver(); break;
        case Screen::Victory: drawVictory(); break;
    }
}

void Game::drawTitleMenu() {
    GhostTitle("AFTERIMAGE", 34, 40, clock);
    const char* sub = "HALVORSEN COURT  -  4TH FLOOR";
    Shadow(sub, CenterX(sub, 10), 85, 10, Pal::Lilac);
    Panel({48, 112, 264, 194}, Pal::Muted, 0.84f);
    Panel({328, 112, 264, 194}, Pal::Muted, 0.84f);
    const char* items[7] = {"CONTINUE", "NEW GAME", "HOW TO PLAY", "GAME RULES", "SETTINGS", "HIGH SCORES", "EXIT"};
    const char* details[7] = {
        progressAvailable ? "Resume from the last area checkpoint." : "No saved visit yet. Start a New Game first.",
        "Start the story from the beginning.",
        "Review movement, interaction and survival controls.",
        "Learn how the Tenant, afterimage and scoring work.",
        "Adjust pace, threat, message time and screen effects.",
        "See the best score recorded on this device.",
        "Close AFTERIMAGE.",
    };
    for (int i = 0; i < 7; ++i) {
        int y = 121 + i * 24;
        bool sel = i == menuIndex;
        Color c = (i == 0 && !progressAvailable) ? Pal::Grey : (sel ? Pal::Bone : Pal::Muted);
        Shadow(items[i], 84, y, 14, c);
        if (sel) {
            if ((int)(clock * 2.f) % 2 == 0) {
                Shadow(">", 64, y, 14, Pal::BrightCrimson);
            }
        }
    }
    Shadow("YOUR NEXT STEP", 348, 130, 10, Pal::Lilac);
    std::vector<std::string> detail = Wrap(details[std::clamp(menuIndex, 0, 6)], 10, 212);
    for (size_t i = 0; i < detail.size(); ++i)
        Shadow(detail[i].c_str(), 348, 152 + (int)i * 14, 10, Pal::PaleGrey);
    DrawRectangle(348, 210, 220, 1, ColorAlpha(Pal::Muted, 0.8f));
    Shadow("HIGH SCORE", 348, 224, 10, Pal::Muted);
    Shadow(TextFormat("%08d", highScore), 348, 242, 20, Pal::Bone);
    Shadow("W/S SELECT   ENTER OPEN   ESC QUIT", 146, 329, 10, Pal::Grey);
}

void Game::drawBriefing() {
    DrawRectangle(0, 0, UI_W, UI_H, ColorAlpha(Pal::Black, 0.76f));
    Panel({54, 28, 532, 304}, Pal::Muted, 0.96f);
    GhostTitle("VISIT 47", 43, 20, clock);
    Shadow(level.name.c_str(), CenterX(level.name.c_str(), 10), 76, 10, Pal::Lilac);

    const char* story[] = {
        "You wake in Apartment 4C during a storm. A note in your handwriting says this is visit 47. "
        "You came back looking for Ilse, who left Halvorsen Court six years ago.",
        "The corridor beyond the apartment repeats. Search the changed crate for the Archive key, "
        "then reach the marked Archive door.",
        "The Archive holds records of your past visits. Use Q to reveal the relay order, restore the "
        "terminal, and follow the exit it releases.",
        "The Tenant is a residue left by every failed visit. Find Ilse's letter in the record room, "
        "then use it to open the stair door and leave the building.",
    };
    const char* goals[] = {
        "FIRST: Read the note beside the bed. Explore with WASD, interact with E, and follow the OBJECTIVE panel.",
        "FIRST: Search for the moved crate, collect the Brass Key, and unlock the Archive door.",
        "FIRST: Press Q near the relays to see their order. Throw the levers, then use the terminal.",
        "FIRST: Search the drawer for Ilse's letter. The stair exit is at the far end of the ward.",
    };
    const int idx = std::clamp(levelIndex, 0, 3);
    std::vector<std::string> lines = Wrap(story[idx], 10, 470);
    int y = 112;
    for (const std::string& line : lines) {
        Shadow(line.c_str(), 86, y, 10, Pal::PaleGrey);
        y += 16;
    }
    DrawRectangle(86, 188, 468, 1, ColorAlpha(Pal::Muted, 0.8f));
    lines = Wrap(goals[idx], 10, 466);
    y = 204;
    for (const std::string& line : lines) {
        Shadow(line.c_str(), 86, y, 10, Pal::PaleGreen);
        y += 16;
    }
    Shadow("SURVIVAL: Shift runs but makes noise. Hide before the Tenant sees you.", 86, 264, 10, Pal::Lilac);
    Shadow("The first 24 seconds in a new area are safe while you get oriented.", 86, 282, 10, Pal::Muted);
    Shadow("[ENTER] BEGIN     [ESC] TITLE MENU", CenterX("[ENTER] BEGIN     [ESC] TITLE MENU", 10), 310, 10,
           Pal::Bone);
}

void Game::drawControls() {
    Panel({110, 40, 420, 280}, Pal::Muted, 0.9f);
    Shadow("CONTROLS", CenterX("CONTROLS", 20), 54, 20, Pal::Bone);
    const char* lines[] = {
        "W A S D / ARROWS ...... move",
        "SHIFT ................. run (noisy, uses stamina)",
        "E ..................... interact / read / hide / leave",
        "Q ..................... reveal ghost positions and puzzle clues",
        "J ..................... review case file and room clues",
        "ESC / P ............... pause",
        "F1 .................... debug view (AI path, sight)",
        "",
        "The Tenant can catch and kill you. Sprint to escape.",
        "Break line of sight, close doors behind you, hide.",
        "Running and loud searches attract it.",
        "Repeated routes make it more alert. Change direction.",
    };
    int y = 92;
    for (const char* l : lines) {
        Shadow(l, 140, y, 10, l[0] == 'T' || l[0] == 'B' || l[0] == 'R' || l[0] == 'W' ? Pal::Lilac : Pal::PaleGrey);
        y += 17;
    }
    Shadow("[ENTER] back", CenterX("[ENTER] back", 10), 318, 10, Pal::Muted);
}

void Game::drawRules() {
    Panel({62, 34, 516, 292}, Pal::Muted, 0.94f);
    GhostTitle("GAME RULES", 44, 20, clock);
    const char* sections[] = {
        "CAMPAIGN      Four areas: apartment, corridor, Archive, residue ward.",
        "OBJECTIVE     Follow the briefing and OBJECTIVE panel in each area.",
        "SURVIVE       The Tenant will chase and kill you if it catches you.",
        "ESCAPE        Break line of sight, close doors, and hide when unseen.",
        "STAY QUIET    Running, empty crates and wrong relay levers make noise.",
        "AFTERIMAGE    Q shows the past; red marks changes, green numbers show the order.",
        "MIMIC         From the Archive onward it retraces your route from 2 minutes earlier.",
        "COUNTERPLAY   Change rooms, break sight and avoid the route it is retracing.",
        "CASE FILE     J opens current clues, room notes and your objective.",
        "SCORE         Explore new rooms, read records and solve puzzles.",
        "RETRY         Death lets you retry this area or begin a new run.",
        "RECORD        Your best score and area checkpoint are saved here.",
    };
    for (int i = 0; i < 12; ++i) Shadow(sections[i], 82, 62 + i * 18, 9, i == 0 ? Pal::Bone : Pal::PaleGrey);
    Shadow("[ENTER / ESC] BACK", CenterX("[ENTER / ESC] BACK", 10), 304, 10, Pal::Lilac);
}

void Game::drawSettings() {
    Panel({88, 36, 464, 286}, Pal::Muted, 0.94f);
    GhostTitle("SETTINGS", 50, 20, clock);
    const char* labels[] = {"MOVEMENT PACE", "TENANT THREAT", "MESSAGE HOLD", "SCREEN EFFECTS"};
    const char* movement[] = {"CALM", "STANDARD", "QUICK"};
    const char* threat[] = {"GUIDED", "STANDARD", "INTENSE"};
    const char* messages[] = {"LONG", "STANDARD", "SHORT"};
    const char* values[] = {movement[std::clamp(movementSetting, 0, 2)],
                            threat[std::clamp(threatSetting, 0, 2)],
                            messages[std::clamp(messageSetting, 0, 2)],
                            screenEffects ? "ON" : "OFF"};
    const char* help[] = {"Slower movement gives more time to read and explore.",
                          "Guided slows the Tenant and lowers its detection range.",
                          "Choose how long on-screen story prompts remain visible.",
                          "Turn off scanlines and vignette for a cleaner image."};
    for (int i = 0; i < 4; ++i) {
        const int y = 102 + i * 42;
        if (i == settingsIndex) {
            DrawRectangle(108, y - 4, 424, 30, ColorAlpha(Pal::Violet, 0.55f));
            DrawRectangle(108, y - 4, 3, 30, Pal::BrightCrimson);
        }
        Shadow(labels[i], 124, y, 10, i == settingsIndex ? Pal::Bone : Pal::Muted);
        Shadow(TextFormat("<  %s  >", values[i]), 378, y, 10, i == settingsIndex ? Pal::PaleGreen : Pal::PaleGrey);
    }
    Shadow(help[settingsIndex], 112, 278, 10, Pal::Lilac);
    Shadow("W/S CHOOSE   A/D CHANGE   ESC BACK", CenterX("W/S CHOOSE   A/D CHANGE   ESC BACK", 10), 302, 10,
           Pal::Muted);
}

void Game::drawHighScores() {
    Panel({104, 44, 432, 272}, Pal::Muted, 0.94f);
    GhostTitle("HIGH SCORES", 66, 20, clock);
    Shadow("BEST VISIT", CenterX("BEST VISIT", 10), 136, 10, Pal::Lilac);
    Shadow(TextFormat("%08d", highScore), CenterX(TextFormat("%08d", highScore), 40), 158, 40, Pal::Bone);
    DrawRectangle(184, 224, 272, 1, ColorAlpha(Pal::Muted, 0.8f));
    Shadow("CURRENT RUN", CenterX("CURRENT RUN", 10), 240, 10, Pal::Muted);
    Shadow(TextFormat("%08d", score), CenterX(TextFormat("%08d", score), 20), 258, 20, Pal::PaleGreen);
    Shadow("[ENTER / ESC] BACK", CenterX("[ENTER / ESC] BACK", 10), 294, 10, Pal::Muted);
}

void Game::drawHud() {
    // Area name banner while fading in.
    if (fade > 0.05f && pendingLevel < 0) {
        Shadow(level.name.c_str(), CenterX(level.name.c_str(), 20), 150, 20, ColorAlpha(Pal::Bone, 1.f));
    }

    // Objective
    std::vector<std::string> obj = Wrap(objective, 10, 230);
    float ph = 22.f + obj.size() * 12.f;
    Panel({8, 8, 244, ph}, ColorAlpha(Pal::Muted, 0.9f), 0.7f);
    Shadow("OBJECTIVE", 14, 12, 10, Pal::Muted);
    for (size_t i = 0; i < obj.size(); ++i) Shadow(obj[i].c_str(), 14, 25 + (int)i * 12, 10, Pal::PaleGrey);

    // Visit counter
    const char* v = TextFormat("VISIT %d   |   SCORE %08d", visits, score);
    Shadow(v, CenterX(v, 10), 10, 10, ColorAlpha(Pal::Muted, 0.8f));

    // Inventory
    float ih = 22.f + (inventory.empty() ? 12.f : inventory.size() * 12.f);
    Panel({482, 8, 150, ih}, ColorAlpha(Pal::Muted, 0.9f), 0.7f);
    Shadow("CARRYING", 488, 12, 10, Pal::Muted);
    if (inventory.empty()) Shadow("nothing", 488, 25, 10, Pal::Grey);
    for (size_t i = 0; i < inventory.size(); ++i) {
        DrawRectangle(488, 26 + (int)i * 12, 6, 6, Pal::Bone);
        Shadow(inventory[i].c_str(), 500, 25 + (int)i * 12, 10, Pal::Bone);
    }

    // Afterimage indicator
    if (after.unlocked) {
        const char* afterLabel = after.isActive() ? "[Q] MEMORY REVEALED" :
                                 after.cooldownLeft() > 0.f ? "AFTERIMAGE RECHARGING" : "[Q] READ THE PAST";
        Panel({8, 320, 160, 32}, after.isActive() ? Pal::Lilac : ColorAlpha(Pal::Muted, 0.9f), 0.7f);
        Shadow(afterLabel, 14, 324, 9, after.ready() ? Pal::Lilac : Pal::Muted);
        if (after.isActive()) {
            Bar(14, 338, 148, 6, after.activeTime() / Tune::AFTERIMAGE_DURATION, Pal::Lilac, Pal::DeepViolet);
        } else if (after.cooldownLeft() > 0.f) {
            Bar(14, 338, 148, 6, 1.f - after.cooldownLeft() / Tune::AFTERIMAGE_COOLDOWN, Pal::Muted, Pal::Void);
        } else {
            float p = 0.6f + 0.4f * std::sin(clock * 5.f);
            Bar(14, 338, 148, 6, 1.f, ColorAlpha(Pal::Lilac, p), Pal::Void);
        }
        if (after.isActive()) {
            std::vector<std::string> tip = Wrap(afterimageHint(), 8, 300);
            Panel({174, 292, 292, (float)(38 + (int)tip.size() * 9)}, ColorAlpha(Pal::DeepViolet, 0.96f), 0.85f);
            Shadow("MEMORY  |  GHOST: PAST  /  RED: CHANGED", 184, 298, 8, Pal::Lilac);
            for (size_t i = 0; i < tip.size(); ++i) Shadow(tip[i].c_str(), 184, 310 + (int)i * 9, 8, Pal::Bone);
        }
        if (after.ready()) Shadow("Q reveals the room's past", 14, 314, 8, Pal::PaleGrey);
    }

    // Stamina, pattern and danger
    if (!level.hasEnemy) {
        Panel({472, 332, 160, 20}, ColorAlpha(Pal::Muted, 0.9f), 0.7f);
        Shadow("STAMINA", 478, 336, 10, Pal::Muted);
        Bar(540, 338, 86, 5, player.stamina, player.exhausted ? Pal::Crimson : Pal::FadedGreen, Pal::Void);
    } else {
        Panel({472, 300, 160, 52}, ColorAlpha(Pal::Muted, 0.9f), 0.7f);
        Shadow("STAMINA", 478, 304, 10, Pal::Muted);
        Bar(540, 306, 86, 5, player.stamina, player.exhausted ? Pal::Crimson : Pal::FadedGreen, Pal::Void);
        Shadow("PATTERN", 478, 318, 10, Pal::Muted);
        float pl = after.patternLevel();
        Bar(540, 320, 86, 5, pl, pl > 0.5f ? Pal::BrightCrimson : Pal::Crimson, Pal::Void);
        const char* st = "IT WANDERS";
        Color sc = Pal::Muted;
        if (enemy.dormant > 0.f) st = "SOMEWHERE";
        else if (enemy.state == EnemyState::Chase) { st = "IT SEES YOU"; sc = Pal::BrightCrimson; }
        else if (enemy.state == EnemyState::Investigate) { st = "IT IS LOOKING"; sc = Pal::Crimson; }
        else if (enemy.state == EnemyState::Search) { st = "IT IS SEARCHING"; sc = Pal::Crimson; }
        Shadow(st, 478, 334, 10, sc);
    }

    // The mimic has a legible two-minute memory loop: its echo is a warning,
    // and its red eye-slit distinguishes it from the real player.
    if (level.hasEnemy && levelIndex >= 2) {
        Panel({242, 326, 174, 26}, ColorAlpha(Pal::Muted, 0.92f), 0.72f);
        if (mimicMemoryAge >= 0.f) {
            Shadow("MEMORY ECHO  02:00", 250, 330, 10, Pal::BrightCrimson);
            Shadow("YOUR OLD ROUTE IS ACTIVE", 250, 340, 8, Pal::PaleGrey);
        } else {
            float remain = std::max(0.f, 120.f - (clock - (playerMemory.empty() ? clock : playerMemory.front().time)));
            Shadow("MEMORY ECHO  SYNCING", 250, 330, 10, Pal::Lilac);
            Shadow(TextFormat("TRACE READY IN %02d:%02d", (int)remain / 60, (int)remain % 60), 250, 340, 8,
                   Pal::PaleGrey);
        }
    }

    // Hidden indicator
    if (player.hidden) {
        const char* h = "HIDDEN  -  [E] leave";
        Panel({(float)CenterX(h, 10) - 8, 40, (float)MeasureText(h, 10) + 16, 18}, Pal::Muted, 0.8f);
        Shadow(h, CenterX(h, 10), 44, 10, Pal::Lilac);
    }

    // Contextual prompt
    if (screen == Screen::Playing && !player.hidden && pendingLevel < 0) {
        Object* o = nearestInteractable();
        if (o) {
            std::string p = promptFor(*o);
            int w = MeasureText(p.c_str(), 10);
            Color border = interactFlash > 0.f ? ColorAlpha(interactFlashColor, 0.4f + 0.6f * interactFlash)
                                               : ColorAlpha(Pal::Muted, 0.9f);
            Panel({(float)(UI_W - w) / 2 - 8, 296, (float)w + 16, 18}, border, 0.8f);
            Shadow(p.c_str(), (UI_W - w) / 2, 300, 10, Pal::Bone);
        }
    }

    // Message line
    if (!messages.empty()) {
        std::vector<std::string> m = Wrap(messages.front(), 10, 420);
        float a = std::min(1.f, msgTimer * 4.f);
        int y = 286 - (int)m.size() * 12;
        for (size_t i = 0; i < m.size(); ++i)
            Shadow(m[i].c_str(), CenterX(m[i].c_str(), 10), y + (int)i * 12, 10, ColorAlpha(Pal::Bone, a));
    }

    if (debug) {
        DrawRectangle(8, 100, 330, 136, ColorAlpha(Pal::Black, 0.8f));
        DrawText(TextFormat("FPS %d", GetFPS()), 12, 104, 10, GREEN);
        if (level.hasEnemy) {
            DrawText(TextFormat("AI: %s", EnemyStateName(enemy.state)), 12, 116, 10, GREEN);
            DrawText(TextFormat("suspicion %.2f  speed x%.2f", enemy.suspicion, enemy.speedMultiplier(after)), 12, 128, 10, GREEN);
            DrawText(TextFormat("target %d,%d  mimic %d/%d", enemy.targetTile.x, enemy.targetTile.y,
                                enemy.mimicRouteIndex, (int)enemy.mimicRoute.size()), 12, 140, 10, GREEN);
            DrawText(TextFormat("profile routes %d hides %d", (int)habits.routes.size(), (int)habits.hides.size()),
                     12, 152, 10, GREEN);
            const Point known = enemy.lastKnownTile();
            DrawText(TextFormat("last known %d,%d", known.x, known.y), 12, 164, 10, GREEN);
            DrawText(TextFormat("target use route %u / hide %u", habits.routeUse(level.index, enemy.targetTile),
                                habits.hideUse(level.index, enemy.targetTile)), 12, 176, 10, GREEN);
            DrawText(TextFormat("sprint noise memory %u", habits.noisySprints), 12, 188, 10, GREEN);
            DrawText(TextFormat("decision: %.55s", enemy.decisionReason ? enemy.decisionReason : "none"), 12, 200, 10, GREEN);
        }
        DrawText(TextFormat("repetition score %d", after.repetitionScore()), 12, 212, 10, GREEN);
        Point t = ToTile(player.pos);
        DrawText(TextFormat("tile %d,%d visits %d", t.x, t.y, after.visits(t.x, t.y)), 12, 224, 10, GREEN);
    }
}

void Game::drawCaseFile() {
    Panel({78, 38, 484, 284}, Pal::Muted, 0.96f);
    GhostTitle("FIELD NOTES", 54, 20, clock);
    const char* title = levelIndex == 0 ? "4C / DOMESTIC RESIDENCE" :
                        levelIndex == 1 ? "NORTH HALL / LOOP 47" :
                        levelIndex == 2 ? "SUB-BASEMENT / RECORD VAULT" : "WARD C / RESIDUE STORAGE";
    Shadow(title, 104, 88, 10, Pal::Lilac);
    std::vector<std::string> lines = Wrap(objective, 10, 420);
    Shadow("CURRENT LEAD", 104, 112, 10, Pal::Muted);
    for (size_t i = 0; i < lines.size(); ++i) Shadow(lines[i].c_str(), 104, 128 + (int)i * 14, 10, Pal::PaleGreen);
    Shadow("BUILDING NOTES", 104, 178, 10, Pal::Muted);
    const char* notes[] = {
        "APARTMENT 4C     Two places are set at the table. One cup is still warm.",
        "NORTH HALL       Resident notices deny that this corridor repeats.",
        "RECORD VAULT     Every failed visit left routes, habits and a residue.",
        "RESIDUE WARD     Intake, mirror bay and records identify the ward's remaining rooms.",
    };
    Shadow(notes[std::clamp(levelIndex, 0, 3)], 104, 196, 10, Pal::PaleGrey);
    if (level.hasEnemy) {
        Shadow("THREAT PROFILE", 104, 226, 10, Pal::Muted);
        Shadow("It follows your old route, repeats your posture and checks familiar hiding places.",
               104, 242, 10, Pal::PaleGrey);
        Shadow("Red eyes = Tenant. If it copies your route, change rooms and break sight.",
               104, 258, 10, Pal::PaleGrey);
    }
    Shadow("[J / ESC] RETURN TO GAME", CenterX("[J / ESC] RETURN TO GAME", 10), 296, 10, Pal::Bone);
}

void Game::drawReading() {
    DrawRectangle(0, 0, UI_W, UI_H, ColorAlpha(Pal::Black, 0.55f));
    Rectangle r{100, 40, 440, 270};
    bool terminal = readTitle == "TERMINAL";
    Color ink = terminal ? Pal::PaleGreen : Pal::Bone;
    Panel(r, terminal ? Pal::FadedGreen : Pal::Muted, 0.94f);
    Shadow(readTitle.c_str(), (int)r.x + 14, (int)r.y + 12, 10, terminal ? Pal::FadedGreen : Pal::Lilac);
    DrawRectangle((int)r.x + 14, (int)r.y + 26, (int)r.width - 28, 1, ColorAlpha(Pal::Muted, 0.8f));
    if (readPage < (int)readPages.size()) {
        std::vector<std::string> lines = Wrap(readPages[readPage], 10, (int)r.width - 32);
        for (size_t i = 0; i < lines.size(); ++i)
            Shadow(lines[i].c_str(), (int)r.x + 16, (int)r.y + 36 + (int)i * 13, 10, ink);
    }
    const char* more = readPage + 1 < (int)readPages.size() ? "[E] next page" : "[E] close";
    Shadow(more, (int)(r.x + r.width) - MeasureText(more, 10) - 14, (int)(r.y + r.height) - 18, 10, Pal::Muted);
    if (readPages.size() > 1)
        Shadow(TextFormat("%d / %d", readPage + 1, (int)readPages.size()), (int)r.x + 14, (int)(r.y + r.height) - 18,
               10, Pal::Muted);
    if (terminal && (int)(clock * 2.f) % 2 == 0) DrawRectangle((int)r.x + 16, (int)(r.y + r.height) - 34, 6, 9, ink);
}

void Game::drawPause() {
    DrawRectangle(0, 0, UI_W, UI_H, ColorAlpha(Pal::Black, 0.6f));
    GhostTitle("PAUSED", 90, 20, clock);
    Panel({174, 122, 292, 206}, Pal::Muted, 0.9f);
    const char* items[6] = {"RESUME", "SETTINGS", "GAME RULES", "RETRY AREA", "RESTART RUN", "TITLE MENU"};
    for (int i = 0; i < 6; ++i) {
        bool sel = i == pauseIndex;
        const char* t = items[i];
        int y = 137 + i * 29;
        Shadow(t, CenterX(t, 14), y, 14, sel ? Pal::Bone : Pal::Muted);
        if (sel) Shadow(">", CenterX(t, 14) - 20, y, 14, Pal::BrightCrimson);
    }
}

void Game::drawGameOver() {
    if (screenTimer < 5.f) {
        // Hold the full-screen close-up through the entire five-second sting.
        DrawRectangle(0, 0, UI_W, UI_H, Color{5, 2, 8, 255});
        const float pulse = 1.0f + 0.025f * std::sin(clock * 92.f);
        const int cx = UI_W / 2 + (int)(std::sin(clock * 113.f) * 12.f);
        const int cy = 187 + (int)(std::cos(clock * 89.f) * 7.f);
        const float faceW = 218.f * pulse, faceH = 292.f * pulse;
        DrawEllipse(cx, cy, faceW, faceH, Color{17, 7, 24, 255});
        DrawEllipse(cx, cy + 3, faceW * 0.76f, faceH * 0.84f, Color{43, 20, 48, 255});
        DrawTriangle({(float)cx - 190, 60}, {(float)cx - 118, 24}, {(float)cx - 122, 108}, Color{9, 4, 16, 255});
        DrawTriangle({(float)cx + 190, 60}, {(float)cx + 118, 24}, {(float)cx + 122, 108}, Color{9, 4, 16, 255});
        // Deep eye sockets and horizontal red slits stay legible at a glance.
        DrawRectangle(cx - 143, cy - 67, 116, 46, Color{12, 3, 15, 255});
        DrawRectangle(cx + 27, cy - 67, 116, 46, Color{12, 3, 15, 255});
        DrawRectangle(cx - 133, cy - 51, 96, 13, Color{190, 12, 35, 255});
        DrawRectangle(cx + 37, cy - 51, 96, 13, Color{190, 12, 35, 255});
        DrawRectangle(cx - 114, cy - 49, 61, 5, Color{255, 95, 112, 255});
        DrawRectangle(cx + 55, cy - 49, 61, 5, Color{255, 95, 112, 255});
        // An open, jagged mouth supplies a readable attack silhouette.
        DrawEllipse(cx, cy + 91, 104.f * pulse, 92.f * pulse, Color{8, 2, 10, 255});
        DrawRectangle(cx - 76, cy + 52, 152, 16, Color{215, 194, 183, 255});
        DrawTriangle({(float)cx - 74, (float)cy + 64}, {(float)cx - 48, (float)cy + 64},
                     {(float)cx - 60, (float)cy + 98}, Color{224, 207, 193, 255});
        DrawTriangle({(float)cx + 74, (float)cy + 64}, {(float)cx + 48, (float)cy + 64},
                     {(float)cx + 60, (float)cy + 98}, Color{224, 207, 193, 255});
        DrawRectangle(cx - 58, cy + 147, 116, 10, Color{190, 12, 35, 255});
        const unsigned char flash = (unsigned char)(32 + 45 * (0.5f + 0.5f * std::sin(clock * 86.f)));
        DrawRectangle(0, 0, UI_W, UI_H, Color{120, 0, 16, flash});
        GhostTitle("LOOK AT ME", 16, 20, clock * 2.f);
        return;
    }
    DrawRectangle(0, 0, UI_W, UI_H, ColorAlpha(Pal::Black, std::min(0.85f, screenTimer * 1.2f)));
    if (screenTimer < 0.25f) DrawRectangle(0, 0, UI_W, UI_H, ColorAlpha(Pal::Crimson, 0.6f));
    GhostTitle("IT TOOK YOUR SHAPE", 110, 20, clock);
    const char* a = "The building files away another afterimage.";
    Shadow(a, CenterX(a, 10), 150, 10, Pal::PaleGrey);
    const char* b = TextFormat("VISIT %d BEGINS.", visits + 1);
    Shadow(b, CenterX(b, 10), 168, 10, Pal::BrightCrimson);
    Shadow(TextFormat("SCORE  %08d", score), CenterX(TextFormat("SCORE  %08d", score), 14), 198, 14, Pal::PaleGreen);
    Shadow(TextFormat("BEST   %08d", highScore), CenterX(TextFormat("BEST   %08d", highScore), 10), 219, 10, Pal::Lilac);
    if (screenTimer >= 5.f) {
        const char* items[3] = {"RETRY AREA", "NEW RUN", "TITLE MENU"};
        for (int i = 0; i < 3; ++i) {
            const int y = 248 + i * 22;
            Shadow(items[i], CenterX(items[i], 10), y, 10, i == gameOverIndex ? Pal::Bone : Pal::Muted);
            if (i == gameOverIndex) Shadow(">", CenterX(items[i], 10) - 14, y, 10, Pal::BrightCrimson);
        }
        Shadow("W/S SELECT   ENTER CONFIRM", CenterX("W/S SELECT   ENTER CONFIRM", 10), 324, 10, Pal::Grey);
    }
}

void Game::drawVictory() {
    DrawRectangle(0, 0, UI_W, UI_H, ColorAlpha(Pal::Black, 0.55f));
    const std::string ending =
        "You step out into the rain.\n\n"
        "The street is real. The cold is real. In your coat pocket is a letter you don't remember "
        "taking from the archive. It is in Ilse's handwriting. It says: come home.\n\n"
        "Behind you, on the fourth floor, a tall shape stands at the window of 4C. It keeps your "
        "posture. It does not follow.\n\n"
        "The building will keep the version of you that stayed.";
    std::vector<std::string> lines = Wrap(ending, 10, 440);
    int shown = (int)(screenTimer * 3.f);
    for (int i = 0; i < (int)lines.size() && i < shown; ++i)
        Shadow(lines[i].c_str(), 100, 40 + i * 13, 10, Pal::Bone);
    if (screenTimer > 3.f) {
        GhostTitle("THE END", 250, 20, clock);
        const char* s = TextFormat("Visits to Halvorsen Court: %d", visits);
        Shadow(s, CenterX(s, 10), 280, 10, Pal::Lilac);
        Shadow(TextFormat("FINAL SCORE  %08d   |   BEST  %08d", score, highScore),
               CenterX(TextFormat("FINAL SCORE  %08d   |   BEST  %08d", score, highScore), 10), 302, 10, Pal::PaleGreen);
        const char* again = "PLAY AGAIN";
        const char* title = "TITLE MENU";
        Shadow(again, CenterX(again, 10), 324, 10, victoryIndex == 0 ? Pal::Bone : Pal::Muted);
        Shadow(title, CenterX(title, 10), 340, 10, victoryIndex == 1 ? Pal::Bone : Pal::Muted);
    }
}
