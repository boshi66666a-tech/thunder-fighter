#define _CRT_SECURE_NO_WARNINGS
#include <graphics.h>
#include <windows.h>
#include <math.h>
#include <time.h>
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

int WIDTH = 800;
int HEIGHT = 600;
#define GAME_TITLE _T("雷霆战机")
#define MAX_ENEMY 20
#define MAX_BULLET 100
#define MAX_ITEM 40
#define MAX_EFFECT 80

#define PLAYER_MAX_HP 100
#define ENEMY_BASE_SPEED 1.2f
#define BULLET_SPEED 8
#define SHOOT_CD 10

// 页面状态
enum GameState { START, GUIDE, PLAYING, PAUSED, GAMEOVER };
enum EnemyType { NORMAL, FAST, TANK, BOSS };
enum ItemType { ITEM_NONE, ITEM_HEAL, ITEM_EXP };
enum UpgradeType { UP_ATK_SPEED, UP_DAMAGE, UP_BULLET, UP_MOVE, UP_HEAL, UP_MAXHP, UP_BULLET_SPEED, UP_LOWHP_DAMAGE };
enum TaskType { TASK_NORMAL, TASK_FAST, TASK_TANK, TASK_BOSS };
enum ShipType { SHIP_THUNDER, SHIP_AURORA, SHIP_METEOR };

// 距离计算
float dist(float x1, float y1, float x2, float y2)
{
    float dx = x1 - x2;
    float dy = y1 - y2;
    return sqrtf(dx * dx + dy * dy);
}

int clampInt(int v, int mn, int mx)
{
    if (v < mn) return mn;
    if (v > mx) return mx;
    return v;
}

void setStartFont(int height)
{
    LOGFONT lf;
    gettextstyle(&lf);
    _tcscpy(lf.lfFaceName, _T("微软雅黑"));
    lf.lfHeight = height;
    lf.lfWidth = 0;
    lf.lfWeight = FW_BOLD;
    lf.lfQuality = ANTIALIASED_QUALITY;
    settextstyle(&lf);
}


// 音频
bool bgmReady = false;

// 打开音频
MCIERROR openSoundFile(const char* fileName, const char* aliasName)
{
    char cmd[260];


    sprintf(cmd, "open \"%s\" type waveaudio alias %s", fileName, aliasName);
    MCIERROR err = mciSendStringA(cmd, NULL, 0, NULL);


    if (err != 0)
    {
        sprintf(cmd, "open \"%s\" alias %s", fileName, aliasName);
        err = mciSendStringA(cmd, NULL, 0, NULL);
    }

    return err;
}

// 初始化音频
void initAudio()
{

    mciSendStringA("close bgm", NULL, 0, NULL);
    mciSendStringA("close levelup", NULL, 0, NULL);
    mciSendStringA("close pickup", NULL, 0, NULL);

    bgmReady = (openSoundFile("bgm.wav", "bgm") == 0);
    openSoundFile("levelup.wav", "levelup");
    openSoundFile("pickup.wav", "pickup");


    if (bgmReady)
        mciSendStringA("setaudio bgm volume to 40", NULL, 0, NULL);  // BGM 音量
}

// 播放 BGM
void playBGM()
{
    if (!bgmReady) return;

    mciSendStringA("stop bgm", NULL, 0, NULL);
    mciSendStringA("seek bgm to start", NULL, 0, NULL);
    mciSendStringA("play bgm from 0", NULL, 0, NULL);
}

// BGM 循环
void keepBGMPlaying()
{
    if (!bgmReady) return;

    char mode[64] = "";
    mciSendStringA("status bgm mode", mode, sizeof(mode), NULL);


    if (strcmp(mode, "stopped") == 0)
    {
        mciSendStringA("seek bgm to start", NULL, 0, NULL);
        mciSendStringA("play bgm from 0", NULL, 0, NULL);
    }
}

// 升级音效
void playLevelUpSound()
{
    mciSendStringA("stop levelup", NULL, 0, NULL);
    mciSendStringA("seek levelup to start", NULL, 0, NULL);
    mciSendStringA("play levelup", NULL, 0, NULL);
}

// 拾取音效
void playPickupSound()
{
    mciSendStringA("stop pickup", NULL, 0, NULL);
    mciSendStringA("seek pickup to start", NULL, 0, NULL);
    mciSendStringA("play pickup", NULL, 0, NULL);
}

void closeAudio()
{
    mciSendStringA("stop bgm", NULL, 0, NULL);
    mciSendStringA("close bgm", NULL, 0, NULL);
    mciSendStringA("close levelup", NULL, 0, NULL);
    mciSendStringA("close pickup", NULL, 0, NULL);
}

// 强化属性
struct Talent
{
    float atkSpeed = 1.0f;
    float damage = 1.0f;
    float moveSpeed = 1.0f;
    float bulletSpeed = 1.0f;
    bool lowHpDamage = false;
} talent;

// 玩家飞船
class Player
{
public:
    float x = WIDTH / 2, y = HEIGHT / 2, vx = 0, vy = 0;
    int r = 15;
    int hp = PLAYER_MAX_HP;
    int maxHp = PLAYER_MAX_HP;
    int hurtFlash = 0;
    int shipType = SHIP_THUNDER;

    // 飞船名字
    const TCHAR* shipName(int type)
    {
        if (type == SHIP_AURORA) return _T("极光号");
        if (type == SHIP_METEOR) return _T("流星号");
        return _T("雷霆号");
    }

    // 重置游戏
    void reset()
    {
        x = WIDTH / 2;
        y = HEIGHT / 2;
        vx = vy = 0;
        hp = PLAYER_MAX_HP;
        maxHp = PLAYER_MAX_HP;
        hurtFlash = 0;
    }

    // 每帧更新
    void update()
    {
        float ax = 0, ay = 0;
        if (GetAsyncKeyState('W') & 0x8000) ay -= 0.35f;
        if (GetAsyncKeyState('S') & 0x8000) ay += 0.35f;
        if (GetAsyncKeyState('A') & 0x8000) ax -= 0.35f;
        if (GetAsyncKeyState('D') & 0x8000) ax += 0.35f;

        vx += ax * talent.moveSpeed;
        vy += ay * talent.moveSpeed;
        vx *= 0.85f;
        vy *= 0.85f;

        x += vx;
        y += vy;

        if (x < r) x = r;
        if (x > WIDTH - r) x = WIDTH - r;
        if (y < r) y = r;
        if (y > HEIGHT - r) y = HEIGHT - r;

        if (hurtFlash > 0) hurtFlash--;
    }

    // 飞船外观
    // 总绘制
    void drawShipModel(int cx, int cy, float s, int type, bool hurt = false)
    {
        if (type == SHIP_AURORA)
        {

            setlinecolor(hurt ? RGB(255, 110, 120) : RGB(75, 245, 205));
            setlinestyle(PS_SOLID, (int)(3 * s < 2 ? 2 : 3 * s));
            circle(cx, cy, (int)(28 * s));

            setlinecolor(RGB(115, 210, 255));
            circle(cx, cy, (int)(39 * s));

            setfillcolor(RGB(40, 150, 140));
            POINT leftWing[4] = {
                { cx - (int)(8 * s), cy - (int)(2 * s) },
                { cx - (int)(50 * s), cy + (int)(18 * s) },
                { cx - (int)(34 * s), cy + (int)(34 * s) },
                { cx - (int)(7 * s), cy + (int)(17 * s) }
            };
            solidpolygon(leftWing, 4);

            POINT rightWing[4] = {
                { cx + (int)(8 * s), cy - (int)(2 * s) },
                { cx + (int)(50 * s), cy + (int)(18 * s) },
                { cx + (int)(34 * s), cy + (int)(34 * s) },
                { cx + (int)(7 * s), cy + (int)(17 * s) }
            };
            solidpolygon(rightWing, 4);

            setfillcolor(RGB(30, 78, 105));
            solidellipse(cx - (int)(15 * s), cy - (int)(32 * s), cx + (int)(15 * s), cy + (int)(27 * s));

            setfillcolor(hurt ? RGB(255, 125, 135) : RGB(185, 255, 230));
            POINT body[4] = {
                { cx, cy - (int)(42 * s) },
                { cx - (int)(17 * s), cy + (int)(10 * s) },
                { cx, cy + (int)(30 * s) },
                { cx + (int)(17 * s), cy + (int)(10 * s) }
            };
            solidpolygon(body, 4);

            setfillcolor(RGB(90, 220, 210));
            solidellipse(cx - (int)(8 * s), cy - (int)(13 * s), cx + (int)(8 * s), cy + (int)(5 * s));

            setlinecolor(RGB(245, 255, 255));
            line(cx, cy - (int)(34 * s), cx, cy + (int)(20 * s));

            setfillcolor(RGB(120, 255, 175));
            solidcircle(cx - (int)(10 * s), cy + (int)(35 * s), (int)(5 * s));
            solidcircle(cx + (int)(10 * s), cy + (int)(35 * s), (int)(5 * s));
        }
        else if (type == SHIP_METEOR)
        {

            setlinecolor(hurt ? RGB(255, 120, 130) : RGB(255, 185, 80));
            setlinestyle(PS_SOLID, (int)(3 * s < 2 ? 2 : 3 * s));
            circle(cx, cy, (int)(30 * s));

            setfillcolor(RGB(110, 55, 75));
            POINT backWing[4] = {
                { cx - (int)(30 * s), cy + (int)(7 * s) },
                { cx - (int)(54 * s), cy + (int)(30 * s) },
                { cx, cy + (int)(20 * s) },
                { cx + (int)(54 * s), cy + (int)(30 * s) }
            };
            solidpolygon(backWing, 4);

            setfillcolor(RGB(185, 75, 65));
            POINT leftWing[3] = {
                { cx - (int)(9 * s), cy + (int)(3 * s) },
                { cx - (int)(38 * s), cy + (int)(16 * s) },
                { cx - (int)(12 * s), cy + (int)(23 * s) }
            };
            solidpolygon(leftWing, 3);

            POINT rightWing[3] = {
                { cx + (int)(9 * s), cy + (int)(3 * s) },
                { cx + (int)(38 * s), cy + (int)(16 * s) },
                { cx + (int)(12 * s), cy + (int)(23 * s) }
            };
            solidpolygon(rightWing, 3);

            setfillcolor(RGB(80, 45, 70));
            POINT shadow[3] = {
                { cx, cy - (int)(43 * s) },
                { cx - (int)(22 * s), cy + (int)(30 * s) },
                { cx + (int)(22 * s), cy + (int)(30 * s) }
            };
            solidpolygon(shadow, 3);

            setfillcolor(hurt ? RGB(255, 120, 130) : RGB(255, 205, 120));
            POINT body[3] = {
                { cx, cy - (int)(39 * s) },
                { cx - (int)(17 * s), cy + (int)(26 * s) },
                { cx + (int)(17 * s), cy + (int)(26 * s) }
            };
            solidpolygon(body, 3);

            setfillcolor(RGB(255, 120, 70));
            POINT lower[3] = {
                { cx, cy + (int)(2 * s) },
                { cx - (int)(15 * s), cy + (int)(26 * s) },
                { cx + (int)(15 * s), cy + (int)(26 * s) }
            };
            solidpolygon(lower, 3);

            setfillcolor(RGB(80, 230, 255));
            solidellipse(cx - (int)(7 * s), cy - (int)(13 * s), cx + (int)(7 * s), cy + (int)(3 * s));

            setfillcolor(RGB(255, 110, 50));
            solidcircle(cx - (int)(9 * s), cy + (int)(38 * s), (int)(6 * s));
            solidcircle(cx + (int)(9 * s), cy + (int)(38 * s), (int)(6 * s));
            setfillcolor(RGB(255, 230, 105));
            solidcircle(cx - (int)(9 * s), cy + (int)(38 * s), (int)(3 * s));
            solidcircle(cx + (int)(9 * s), cy + (int)(38 * s), (int)(3 * s));
        }
        else
        {

            setlinecolor(hurt ? RGB(255, 90, 100) : RGB(80, 255, 220));
            setlinestyle(PS_SOLID, (int)(3 * s < 2 ? 2 : 3 * s));
            circle(cx, cy, (int)(29 * s));

            setlinecolor(hurt ? RGB(255, 130, 140) : RGB(80, 170, 255));
            setlinestyle(PS_SOLID, 2);
            circle(cx, cy, (int)(24 * s));

            setfillcolor(RGB(65, 70, 160));
            POINT leftWing[3] = {
                { cx - (int)(8 * s), cy + (int)(2 * s) },
                { cx - (int)(31 * s), cy + (int)(24 * s) },
                { cx - (int)(10 * s), cy + (int)(18 * s) }
            };
            solidpolygon(leftWing, 3);

            POINT rightWing[3] = {
                { cx + (int)(8 * s), cy + (int)(2 * s) },
                { cx + (int)(31 * s), cy + (int)(24 * s) },
                { cx + (int)(10 * s), cy + (int)(18 * s) }
            };
            solidpolygon(rightWing, 3);

            setfillcolor(RGB(35, 42, 92));
            POINT shadowBody[3] = {
                { cx, cy - (int)(31 * s) },
                { cx - (int)(18 * s), cy + (int)(20 * s) },
                { cx + (int)(18 * s), cy + (int)(20 * s) }
            };
            solidpolygon(shadowBody, 3);

            setfillcolor(hurt ? RGB(255, 105, 115) : RGB(205, 230, 245));
            POINT body[3] = {
                { cx, cy - (int)(29 * s) },
                { cx - (int)(14 * s), cy + (int)(17 * s) },
                { cx + (int)(14 * s), cy + (int)(17 * s) }
            };
            solidpolygon(body, 3);

            setfillcolor(hurt ? RGB(255, 145, 150) : RGB(120, 185, 225));
            POINT lowerBody[3] = {
                { cx, cy + (int)(4 * s) },
                { cx - (int)(12 * s), cy + (int)(17 * s) },
                { cx + (int)(12 * s), cy + (int)(17 * s) }
            };
            solidpolygon(lowerBody, 3);

            setlinecolor(RGB(255, 255, 255));
            setlinestyle(PS_SOLID, 2);
            line(cx, cy - (int)(25 * s), cx, cy + (int)(14 * s));

            setfillcolor(RGB(60, 245, 255));
            solidcircle(cx, cy - (int)(4 * s), (int)(6 * s));

            setfillcolor(RGB(230, 255, 255));
            solidcircle(cx - (int)(2 * s), cy - (int)(6 * s), (int)(2 * s));

            setfillcolor(RGB(255, 145, 55));
            solidcircle(cx - (int)(7 * s), cy + (int)(28 * s), (int)(5 * s));
            solidcircle(cx + (int)(7 * s), cy + (int)(28 * s), (int)(5 * s));

            setfillcolor(RGB(255, 230, 110));
            solidcircle(cx - (int)(7 * s), cy + (int)(28 * s), (int)(3 * s));
            solidcircle(cx + (int)(7 * s), cy + (int)(28 * s), (int)(3 * s));
        }
    }

    void draw()
    {
        drawShipModel((int)x, (int)y, 1.0f, shipType, hurtFlash > 0);
    }
};

// 怪物大小、血量、速度
class Enemy
{
public:
    float x = 0, y = 0;
    int r = 15;
    int hp = 1, maxHp = 1;
    float speed = 1.2f;
    bool alive = false;
    EnemyType type = NORMAL;
    bool deadHandled = false;

    // 怪物生成
    void spawn(float px, float py, int wave)
    {
        alive = true;
        deadHandled = false;

        static int bossCount = 0;
        static int lastWave = -1;

        if (wave != lastWave)
        {
            bossCount = 0;
            lastWave = wave;
        }

        if (wave % 5 == 0 && bossCount < 1)
        {
            type = BOSS;
            bossCount++;
        }
        else
        {
            type = (EnemyType)(rand() % 3);
        }

        float waveScale = 1.0f + wave * 0.22f;

        if (type == NORMAL)
        {
            r = 17;
            maxHp = hp = (int)(1 * waveScale);
            if (hp < 1) hp = 1;
            speed = ENEMY_BASE_SPEED + wave * 0.05f;
        }
        if (type == FAST)
        {
            r = 14;
            maxHp = hp = (int)(1 * waveScale);
            if (hp < 1) hp = 1;
            speed = ENEMY_BASE_SPEED * 1.8f + wave * 0.065f;
        }
        if (type == TANK)
        {
            r = 24;
            maxHp = hp = (int)(3 * waveScale);
            speed = ENEMY_BASE_SPEED * 0.6f + wave * 0.035f;
        }
        if (type == BOSS)
        {
            r = 38;
            maxHp = hp = (int)(10 * waveScale);
            speed = ENEMY_BASE_SPEED * 1.2f + wave * 0.05f;
        }

        int safe = 0;
        do
        {
            x = rand() % (WIDTH - 40) + 20;
            y = rand() % (HEIGHT - 40) + 20;
            safe++;
        } while (dist(x, y, px, py) < 120 && safe < 50);
    }

    void update(float px, float py)
    {
        if (!alive) return;
        float dx = px - x;
        float dy = py - y;
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 0.01f) return;
        x += dx / len * speed;
        y += dy / len * speed;
    }

    void draw()
    {
        if (!alive) return;

        COLORREF bodyColor = RGB(60, 100, 220);
        COLORREF glowColor = RGB(100, 160, 255);

        if (type == FAST)
        {
            bodyColor = RGB(40, 190, 120);
            glowColor = RGB(120, 255, 180);
        }
        else if (type == TANK)
        {
            bodyColor = RGB(80, 80, 110);
            glowColor = RGB(180, 180, 230);
        }
        else if (type == BOSS)
        {
            bodyColor = RGB(180, 70, 190);
            glowColor = RGB(255, 120, 255);
        }

        setlinecolor(glowColor);
        setlinestyle(PS_SOLID, type == BOSS ? 3 : 2);
        circle((int)x, (int)y, r + 6);

        setfillcolor(bodyColor);

        if (type == TANK)
        {
            solidrectangle((int)x - r, (int)y - r, (int)x + r, (int)y + r);
            setlinecolor(RGB(230, 230, 255));
            rectangle((int)x - r, (int)y - r, (int)x + r, (int)y + r);
        }
        else if (type == FAST)
        {
            POINT p[4] = {
                { (int)x, (int)y - r },
                { (int)x + r + 8, (int)y },
                { (int)x, (int)y + r },
                { (int)x - r - 8, (int)y }
            };
            solidpolygon(p, 4);
        }
        else
        {
            solidcircle((int)x, (int)y, r);
        }

        setfillcolor(RGB(255, 255, 255));
        solidcircle((int)x - r / 3, (int)y - r / 4, r / 5);
        solidcircle((int)x + r / 3, (int)y - r / 4, r / 5);

        setfillcolor(RGB(255, 60, 60));
        solidcircle((int)x - r / 3, (int)y - r / 4, 2);
        solidcircle((int)x + r / 3, (int)y - r / 4, 2);

        if (type == FAST)
        {
            setlinecolor(RGB(120, 255, 180));
            line((int)x - r - 8, (int)y, (int)x - r - 22, (int)y + 5);
            line((int)x - r - 8, (int)y - 5, (int)x - r - 22, (int)y - 2);
        }

        if (type == BOSS)
        {
            setlinecolor(RGB(255, 150, 255));
            circle((int)x, (int)y, r + 14);

            float rate = (float)hp / maxHp;
            setfillcolor(RGB(30, 30, 50));
            solidrectangle((int)x - 42, (int)y - 60, (int)x + 42, (int)y - 49);

            setfillcolor(RGB(255, 80, 120));
            solidrectangle((int)x - 42, (int)y - 60, (int)(x - 42 + 84 * rate), (int)y - 49);

            setlinecolor(RGB(255, 255, 255));
            rectangle((int)x - 42, (int)y - 60, (int)x + 42, (int)y - 49);
        }
    }
};

// 子弹
class Bullet
{
public:
    float x = 0, y = 0, vx = 0, vy = 0;
    bool alive = false;
    int targetIdx = -1;

    void update(Enemy enemies[])
    {
        if (!alive) return;
        if (targetIdx >= 0 && enemies[targetIdx].alive)
        {
            float tx = enemies[targetIdx].x;
            float ty = enemies[targetIdx].y;
            float dx = tx - x;
            float dy = ty - y;
            float len = sqrtf(dx * dx + dy * dy);
            if (len > 0.1f)
            {
                vx = (dx / len) * BULLET_SPEED * talent.bulletSpeed;
                vy = (dy / len) * BULLET_SPEED * talent.bulletSpeed;
            }
        }
        x += vx;
        y += vy;
        if (x < 0 || x > WIDTH || y < 0 || y > HEIGHT)
            alive = false;
    }

    void draw()
    {
        if (alive)
        {
            setfillcolor(RGB(255, 120, 40));
            solidcircle((int)(x - vx * 0.4f), (int)(y - vy * 0.4f), 6);

            setfillcolor(RGB(255, 220, 90));
            solidcircle((int)x, (int)y, 4);

            setfillcolor(RGB(255, 255, 230));
            solidcircle((int)x, (int)y, 2);
        }
    }
};

// 道具
class Item
{
public:
    float x = 0, y = 0;
    bool alive = false;
    ItemType type = ITEM_NONE;
    int r = 10;
    float t = 0;

    void spawn(float px, float py, ItemType itemType)
    {
        x = px;
        y = py;
        type = itemType;
        alive = true;
        t = 0;
        r = 10;
    }

    void update()
    {
        if (!alive) return;
        t += 0.08f;
    }

    void draw()
    {
        if (!alive) return;

        int off = (int)(sinf(t) * 3);

        if (type == ITEM_HEAL)
        {
            setfillcolor(RGB(40, 180, 80));
            solidcircle((int)x, (int)y + off, r + 3);
            setfillcolor(RGB(80, 230, 120));
            solidcircle((int)x, (int)y + off, r);

            setlinecolor(RGB(255, 255, 255));
            setlinestyle(PS_SOLID, 3);
            line((int)x - 5, (int)y + off, (int)x + 5, (int)y + off);
            line((int)x, (int)y - 5 + off, (int)x, (int)y + 5 + off);
        }
        else if (type == ITEM_EXP)
        {

            setfillcolor(RGB(120, 80, 230));
            solidcircle((int)x, (int)y + off, r + 5);
            setfillcolor(RGB(180, 130, 255));
            solidcircle((int)x, (int)y + off, r + 1);
            setfillcolor(RGB(255, 230, 120));
            solidcircle((int)x, (int)y + off, 5);
            setfillcolor(RGB(255, 255, 245));
            solidcircle((int)x - 2, (int)y - 3 + off, 2);
        }

        setlinecolor(RGB(240, 240, 255));
        setlinestyle(PS_SOLID, 1);
        circle((int)x, (int)y + off, r + 6);
    }
};

// 特效
class Effect
{
public:
    float x = 0, y = 0;
    int life = 0;
    int maxLife = 20;
    int size = 5;
    COLORREF color = RGB(255, 220, 120);

    void start(float px, float py, int s, COLORREF c)
    {
        x = px;
        y = py;
        size = s;
        color = c;
        life = maxLife;
    }

    void update()
    {
        if (life > 0) life--;
    }

    void draw()
    {
        if (life <= 0) return;
        int rr = size + (maxLife - life);
        setlinecolor(color);
        setlinestyle(PS_SOLID, 2);
        circle((int)x, (int)y, rr);
    }
};

// 游戏主体
class Game
{
public:
    Player player;
    Enemy enemies[MAX_ENEMY];
    Bullet bullets[MAX_BULLET];
    Item items[MAX_ITEM];
    Effect effects[MAX_EFFECT];

    int score = 0;
    int wave = 1;
    int enemyCount = 6;

    int cntNormal = 0;
    int cntFast = 0;
    int cntTank = 0;
    int cntBoss = 0;

    int level = 1;
    int exp = 0;
    int needExp = 20;

    int shootTimer = 0;
    int hurtCD = 0;

    GameState state = START;
    RECT btn = { 0, 0, 0, 0 };
    RECT introBtn = { 0, 0, 0, 0 };
    RECT backBtn = { 0, 0, 0, 0 };
    RECT shipBtns[3] = { {0,0,0,0}, {0,0,0,0}, {0,0,0,0} };

    int bulletCount = 1;
    bool waveCleared = false;
    bool last1 = false, last2 = false, last3 = false;
    bool lastP = false;
    bool lastMouse = false;
    bool lastMenuMouse = false;
    bool upgradeState = false;

    UpgradeType upgradeOptions[3];

    TaskType taskType = TASK_NORMAL;
    int taskTarget = 10;
    int taskBase = 0;
    int taskRewardScore = 100;

    TCHAR message[100] = _T("");
    int messageTimer = 0;

    static const int STAR_NUM = 60;
    int starX[STAR_NUM], starY[STAR_NUM], starSize[STAR_NUM];
    float starSpeed[STAR_NUM];
    float timer = 0;

    // 星星初始化
    void initStar()
    {
        for (int i = 0; i < STAR_NUM; i++)
        {
            starX[i] = rand() % WIDTH;
            starY[i] = rand() % HEIGHT;
            starSize[i] = rand() % 2 + 1;
            starSpeed[i] = 0.2f + rand() % 10 / 20.0f;
        }
    }

    void setMessage(const TCHAR* msg)
    {
        _tcscpy(message, msg);
        messageTimer = 140;
    }

    void addEffect(float x, float y, int size, COLORREF color)
    {
        for (int i = 0; i < MAX_EFFECT; i++)
        {
            if (effects[i].life <= 0)
            {
                effects[i].start(x, y, size, color);
                return;
            }
        }
    }

    int getTaskCurrentTotal()
    {
        if (taskType == TASK_NORMAL) return cntNormal;
        if (taskType == TASK_FAST) return cntFast;
        if (taskType == TASK_TANK) return cntTank;
        return cntBoss;
    }

    int getTaskProgress()
    {
        int p = getTaskCurrentTotal() - taskBase;
        if (p < 0) p = 0;
        if (p > taskTarget) p = taskTarget;
        return p;
    }

    const TCHAR* getTaskName()
    {
        if (taskType == TASK_NORMAL) return _T("击杀普通怪");
        if (taskType == TASK_FAST) return _T("击杀速度怪");
        if (taskType == TASK_TANK) return _T("击杀坦克怪");
        return _T("击杀Boss怪");
    }

    // 任务生成
    void newTask()
    {
        taskType = (TaskType)(rand() % 4);

        if (taskType == TASK_NORMAL)
        {
            taskTarget = 10 + wave;
            taskBase = cntNormal;
        }
        else if (taskType == TASK_FAST)
        {
            taskTarget = 6 + wave / 2;
            taskBase = cntFast;
        }
        else if (taskType == TASK_TANK)
        {
            taskTarget = 4 + wave / 3;
            taskBase = cntTank;
        }
        else
        {
            taskTarget = 1;
            taskBase = cntBoss;
        }

        taskRewardScore = 100 + wave * 20;
    }

    // 任务完成检测
    void checkTask()
    {
        if (getTaskProgress() >= taskTarget)
        {
            score += taskRewardScore;
            player.hp += 10;
            if (player.hp > player.maxHp) player.hp = player.maxHp;

            addExp(12 + wave * 2);
            setMessage(_T("任务完成！获得奖励"));
            newTask();
        }
    }

    // 经验升级
    void addExp(int value)
    {
        exp += value;

        while (exp >= needExp)
        {
            exp -= needExp;
            level++;
            needExp += 12;
            upgradeState = true;
            makeUpgradeOptions();
            setMessage(_T("等级提升！请选择强化能力"));
        }
    }

    const TCHAR* getUpgradeName(UpgradeType t)
    {
        if (t == UP_ATK_SPEED) return _T("提升攻速 +20%");
        if (t == UP_DAMAGE) return _T("攻击力 +1");
        if (t == UP_BULLET) return _T("子弹数量 +1");
        if (t == UP_MOVE) return _T("提升移速 +15%");
        if (t == UP_HEAL) return _T("恢复生命 +30");
        if (t == UP_MAXHP) return _T("最大生命 +20");
        if (t == UP_BULLET_SPEED) return _T("子弹速度 +15%");
        return _T("低血量伤害提升");
    }

    void makeUpgradeOptions()
    {
        for (int i = 0; i < 3; i++)
        {
            bool ok = false;
            while (!ok)
            {
                UpgradeType t = (UpgradeType)(rand() % 8);
                ok = true;
                for (int j = 0; j < i; j++)
                    if (upgradeOptions[j] == t) ok = false;

                if (ok) upgradeOptions[i] = t;
            }
        }
    }

    // 强化效果
    void applyUpgrade(UpgradeType t)
    {
        if (t == UP_ATK_SPEED)
        {
            talent.atkSpeed += 0.2f;
            setMessage(_T("强化成功：攻速提升"));
        }
        else if (t == UP_DAMAGE)
        {
            talent.damage += 1.0f;
            setMessage(_T("强化成功：攻击力提升"));
        }
        else if (t == UP_BULLET)
        {
            bulletCount++;
            setMessage(_T("强化成功：子弹数量增加"));
        }
        else if (t == UP_MOVE)
        {
            talent.moveSpeed += 0.15f;
            setMessage(_T("强化成功：移动速度提升"));
        }
        else if (t == UP_HEAL)
        {
            player.hp += 15;
            if (player.hp > player.maxHp) player.hp = player.maxHp;
            setMessage(_T("强化成功：生命恢复"));
        }
        else if (t == UP_MAXHP)
        {
            player.maxHp += 20;
            player.hp += 10;
            if (player.hp > player.maxHp) player.hp = player.maxHp;
            setMessage(_T("强化成功：最大生命提升"));
        }
        else if (t == UP_BULLET_SPEED)
        {
            talent.bulletSpeed += 0.15f;
            setMessage(_T("强化成功：子弹速度提升"));
        }
        else if (t == UP_LOWHP_DAMAGE)
        {
            talent.lowHpDamage = true;
            setMessage(_T("强化成功：低血量伤害提升"));
        }

        playLevelUpSound();
    }

    // 道具掉落
    void spawnItem(float x, float y)
    {
        int chance = rand() % 100;

        ItemType type = ITEM_NONE;
        if (chance < 10)
            type = ITEM_HEAL;
        else if (chance < 28)
            type = ITEM_EXP;

        if (type == ITEM_NONE) return;

        for (int i = 0; i < MAX_ITEM; i++)
        {
            if (!items[i].alive)
            {
                items[i].spawn(x, y, type);
                return;
            }
        }
    }

    void reset()
    {
        player.reset();
        score = 0;
        wave = 1;
        enemyCount = 6;
        bulletCount = 1;
        hurtCD = 0;
        upgradeState = false;
        waveCleared = false;
        timer = 0;

        level = 1;
        exp = 0;
        needExp = 20;

        cntNormal = 0;
        cntFast = 0;
        cntTank = 0;
        cntBoss = 0;

        talent.atkSpeed = 1.0f;
        talent.damage = 1.0f;
        talent.moveSpeed = 1.0f;
        talent.bulletSpeed = 1.0f;
        talent.lowHpDamage = false;

        for (auto& e : enemies) e.alive = false;
        for (auto& b : bullets) { b.alive = false; b.targetIdx = -1; }
        for (auto& item : items) item.alive = false;
        for (auto& effect : effects) effect.life = 0;

        initStar();
        newTask();
        makeUpgradeOptions();
        setMessage(_T("雷霆战机出击！击杀敌人获得经验"));
    }

    // 每波敌人
    void spawnEnemies()
    {
        waveCleared = false;

        for (auto& b : bullets)
        {
            b.alive = false;
            b.targetIdx = -1;
        }

        for (int i = 0; i < MAX_ENEMY; i++)
            enemies[i].alive = false;

        for (int i = 0; i < enemyCount && i < MAX_ENEMY; i++)
            enemies[i].spawn(player.x, player.y, wave);
    }

    int aliveCount()
    {
        int c = 0;
        for (auto& e : enemies)
            if (e.alive) c++;
        return c;
    }

    // 自动射击
    void shoot()
    {
        shootTimer++;
        float cd = SHOOT_CD / talent.atkSpeed;
        if (cd < 2) cd = 2;
        if (shootTimer < cd) return;
        shootTimer = 0;

        int t = -1;
        float dmin = 99999;
        for (int i = 0; i < MAX_ENEMY; i++)
        {
            if (!enemies[i].alive) continue;
            float d = dist(player.x, player.y, enemies[i].x, enemies[i].y);
            if (d < dmin) { dmin = d; t = i; }
        }
        if (t == -1) return;

        float baseAngle = atan2f(enemies[t].y - player.y, enemies[t].x - player.x);
        float spreadRange = 0.3f;

        for (int n = 0; n < bulletCount; n++)
        {
            float offset = 0;
            if (bulletCount > 1)
                offset = -spreadRange / 2.0f + n * (spreadRange / (bulletCount - 1));
            float ang = baseAngle + offset;

            for (int i = 0; i < MAX_BULLET; i++)
            {
                if (!bullets[i].alive)
                {
                    bullets[i].alive = true;
                    float side = (n - (bulletCount - 1) / 2.0f) * 12.0f;
                    float px = -sinf(baseAngle);
                    float py = cosf(baseAngle);
                    bullets[i].x = player.x + px * side;
                    bullets[i].y = player.y + py * side;
                    bullets[i].vx = cosf(ang) * BULLET_SPEED * talent.bulletSpeed;
                    bullets[i].vy = sinf(ang) * BULLET_SPEED * talent.bulletSpeed;
                    bullets[i].targetIdx = t;
                    goto nextShot;
                }
            }
        nextShot:;
        }
    }

    // 点击升级
    void handleUpgrade()
    {

        bool mouseDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        if (!mouseDown)
        {
            lastMouse = false;
            return;
        }

        if (lastMouse) return;
        lastMouse = true;

        POINT m;
        GetCursorPos(&m);
        ScreenToClient(GetHWnd(), &m);

        int panelW = 660;
        int panelH = 440;

        if (panelW > WIDTH - 80) panelW = WIDTH - 80;
        if (panelH > HEIGHT - 80) panelH = HEIGHT - 80;

        int y1 = HEIGHT / 2 - panelH / 2;
        int cardW = panelW - 120;
        int cardH = 64;
        int cardX = WIDTH / 2 - cardW / 2;
        int startY = y1 + 145;

        for (int i = 0; i < 3; i++)
        {
            RECT r;
            r.left = cardX;
            r.top = startY + i * 82;
            r.right = cardX + cardW;
            r.bottom = r.top + cardH;

            if (m.x >= r.left && m.x <= r.right && m.y >= r.top && m.y <= r.bottom)
            {
                applyUpgrade(upgradeOptions[i]);
                upgradeState = false;
                lastMouse = false;
                return;
            }
        }
    }

    // 拾取检测
    void updateItems()
    {
        for (auto& item : items)
        {
            item.update();
            if (!item.alive) continue;

            if (dist(player.x, player.y, item.x, item.y) < player.r + item.r + 5)
            {
                if (item.type == ITEM_HEAL)
                {
                    player.hp += 5;
                    if (player.hp > player.maxHp) player.hp = player.maxHp;
                    setMessage(_T("拾取回血包：生命 +5"));
                }
                else if (item.type == ITEM_EXP)
                {
                    addExp(8);
                    setMessage(_T("拾取经验球：经验 +8"));
                }

                playPickupSound();
                addEffect(item.x, item.y, 8, RGB(160, 240, 255));
                item.alive = false;
            }
        }
    }

    void update()
    {
        timer += 0.02f;
        keepBGMPlaying();

        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
        {
            closeAudio();  // 关闭音频
            closegraph();
            exit(0);
        }

        if (messageTimer > 0) messageTimer--;

        for (int i = 0; i < STAR_NUM; i++)
        {
            starX[i] -= starSpeed[i];
            if (starX[i] < 0)
            {
                starX[i] = WIDTH;
                starY[i] = rand() % HEIGHT;
            }
        }

        for (auto& effect : effects)
            effect.update();

        bool p = GetAsyncKeyState('P') & 0x8000;
        if (p && !lastP && state != START && state != GAMEOVER)
            state = (state == PLAYING) ? PAUSED : PLAYING;
        lastP = p;


        bool menuMouseDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        bool menuMouseClick = menuMouseDown && !lastMenuMouse;
        lastMenuMouse = menuMouseDown;

        if (state == START)
        {
            if (menuMouseClick)
            {
                POINT m;
                GetCursorPos(&m);
                ScreenToClient(GetHWnd(), &m);

                for (int i = 0; i < 3; i++)
                {
                    if (m.x > shipBtns[i].left && m.x < shipBtns[i].right &&
                        m.y > shipBtns[i].top && m.y < shipBtns[i].bottom)
                    {
                        player.shipType = i;
                        return;
                    }
                }

                if (m.x > btn.left && m.x < btn.right && m.y > btn.top && m.y < btn.bottom)
                {
                    reset();
                    state = PLAYING;
                    spawnEnemies();
                }
                else if (m.x > introBtn.left && m.x < introBtn.right && m.y > introBtn.top && m.y < introBtn.bottom)
                {
                    state = GUIDE;
                }
            }
            return;
        }

        if (state == GUIDE)
        {
            if (menuMouseClick)
            {
                POINT m;
                GetCursorPos(&m);
                ScreenToClient(GetHWnd(), &m);

                if (m.x > backBtn.left && m.x < backBtn.right && m.y > backBtn.top && m.y < backBtn.bottom)
                    state = START;
            }

            if (GetAsyncKeyState('B') & 0x8000)
                state = START;

            return;
        }

        if (state == GAMEOVER)
        {
            if (GetAsyncKeyState('R') & 0x8000)
                state = START;
            return;
        }

        if (state != PLAYING) return;

        if (hurtCD > 0) hurtCD--;
        if (player.hp <= 0)
        {
            state = GAMEOVER;
            return;
        }

        if (upgradeState)
        {
            handleUpgrade();
            return;
        }

        player.update();
        shoot();

        for (auto& e : enemies)
            if (e.alive) e.update(player.x, player.y);

        for (auto& b : bullets)
            b.update(enemies);

        for (auto& b : bullets)
        {
            if (!b.alive) continue;
            for (auto& e : enemies)
            {
                if (!e.alive) continue;
                if (dist(b.x, b.y, e.x, e.y) < e.r)
                {
                    b.alive = false;

                    int realDamage = (int)(talent.damage + 0.5f);
                    if (talent.lowHpDamage && player.hp <= player.maxHp * 0.3f)
                        realDamage += 2;

                    e.hp -= realDamage;
                    addEffect(b.x, b.y, 4, RGB(255, 220, 100));

                    if (e.hp <= 0 && !e.deadHandled)
                    {
                        e.deadHandled = true;
                        e.alive = false;

                        int getExp = 0;

                        if (e.type == NORMAL)
                        {
                            score += 10;
                            cntNormal++;
                            getExp = 3;
                            addEffect(e.x, e.y, 12, RGB(100, 150, 255));
                        }
                        else if (e.type == FAST)
                        {
                            score += 15;
                            cntFast++;
                            getExp = 4;
                            addEffect(e.x, e.y, 12, RGB(100, 255, 180));
                        }
                        else if (e.type == TANK)
                        {
                            score += 25;
                            cntTank++;
                            getExp = 7;
                            addEffect(e.x, e.y, 16, RGB(180, 180, 220));
                        }
                        else if (e.type == BOSS)
                        {
                            score += 200;
                            cntBoss++;
                            getExp = 35;
                            addEffect(e.x, e.y, 28, RGB(255, 100, 255));
                        }

                        addExp(getExp);
                        spawnItem(e.x, e.y);
                        checkTask();
                    }
                }
            }
        }

        for (auto& e : enemies)
        {
            if (!e.alive) continue;
            if (dist(player.x, player.y, e.x, e.y) < player.r + e.r)
            {
                if (hurtCD == 0)
                {
                    int enemyDamage = 4 + wave;
                    if (e.type == FAST) enemyDamage += 1;
                    if (e.type == TANK) enemyDamage += 2;
                    if (e.type == BOSS) enemyDamage += 5;

                    player.hp -= enemyDamage;
                    player.hurtFlash = 12;
                    hurtCD = 6;
                    setMessage(_T("受到伤害！"));
                }
            }
        }

        updateItems();

        if (!waveCleared && aliveCount() == 0)
        {
            waveCleared = true;
            wave++;
            enemyCount += 2;
            if (enemyCount > MAX_ENEMY) enemyCount = MAX_ENEMY;

            setMessage(_T("下一波敌人来袭！"));
            spawnEnemies();
        }
    }

    // 战斗背景
    void drawGameBg()
    {

        for (int i = 0; i < HEIGHT; i++)
        {
            float t = (float)i / (float)HEIGHT;

            int rr = (int)(14 + 24 * t);
            int gg = (int)(24 + 58 * t);
            int bb = (int)(58 + 68 * t);

            setlinecolor(RGB(rr, gg, bb));
            line(0, i, WIDTH, i);
        }


        for (int i = 0; i < 8; i++)
        {
            int y = 80 + i * (HEIGHT / 8);
            int move = (int)(timer * 25 + i * 80) % (WIDTH + 260);
            int x = move - 260;

            setlinecolor(RGB(35, 80, 125));
            line(x, y, x + 180, y - 20);
            line(x + 45, y + 16, x + 260, y - 8);
        }


        for (int i = 0; i < STAR_NUM; i++)
        {
            if (i % 2 == 1) continue;

            int bright = 135 + (int)(sinf(timer * 1.1f + i) * 35);
            bright = clampInt(bright, 100, 190);

            setfillcolor(RGB(bright, bright + 10 > 255 ? 255 : bright + 10, 220));
            solidcircle(starX[i], starY[i], starSize[i]);
        }


        int cx = WIDTH / 2;
        int cy = HEIGHT / 2;
        setlinecolor(RGB(28, 62, 105));
        setlinestyle(PS_SOLID, 1);
        circle(cx, cy, 110);
        circle(cx, cy, 210);
    }

    // 右侧信息栏
    void drawUI()
    {
        setbkmode(TRANSPARENT);

        int margin = 24;
        int sideW = 280;

        if (WIDTH < 1000)
        {
            margin = 14;
            sideW = 240;
        }

        int x1 = WIDTH - sideW - margin;
        int y1 = 24;
        int x2 = WIDTH - margin;
        int y2 = HEIGHT - 24;


        setfillcolor(RGB(8, 12, 32));
        solidrectangle(x1, y1, x2, y2);

        setlinecolor(RGB(80, 170, 240));
        setlinestyle(PS_SOLID, 2);
        rectangle(x1, y1, x2, y2);

        setlinecolor(RGB(35, 70, 120));
        rectangle(x1 + 6, y1 + 6, x2 - 6, y2 - 6);

        TCHAR buf[180];

        int px = x1 + 22;
        int py = y1 + 22;
        int contentW = sideW - 44;
        int barW = contentW;


        settextstyle(24, 0, _T("等线"));
        settextcolor(RGB(235, 250, 255));
        outtextxy(px, py, GAME_TITLE);

        py += 42;


        settextstyle(16, 0, _T("等线"));
        settextcolor(RGB(230, 240, 255));
        outtextxy(px, py, _T("生命值"));

        _stprintf(buf, _T("%d / %d"), player.hp, player.maxHp);
        outtextxy(x2 - 22 - textwidth(buf), py, buf);

        py += 24;

        int hpNow = clampInt(player.hp, 0, player.maxHp);
        int hpWidth = hpNow * barW / player.maxHp;

        setfillcolor(RGB(24, 28, 55));
        solidrectangle(px, py, px + barW, py + 16);

        if (player.hp > player.maxHp * 0.6f)
            setfillcolor(RGB(55, 220, 110));
        else if (player.hp > player.maxHp * 0.3f)
            setfillcolor(RGB(245, 190, 55));
        else
            setfillcolor(RGB(240, 70, 70));

        solidrectangle(px, py, px + hpWidth, py + 16);

        setlinecolor(RGB(120, 180, 230));
        rectangle(px, py, px + barW, py + 16);

        py += 34;



        int expCardY = py;
        int expCardH = 82;

        setfillcolor(RGB(14, 18, 44));
        solidrectangle(px, expCardY, px + contentW, expCardY + expCardH);

        setlinecolor(RGB(125, 105, 235));
        setlinestyle(PS_SOLID, 2);
        rectangle(px, expCardY, px + contentW, expCardY + expCardH);

        settextstyle(15, 0, _T("等线"));
        settextcolor(RGB(205, 210, 255));
        outtextxy(px + 14, expCardY + 10, _T("成长进度"));

        setfillcolor(RGB(105, 85, 225));
        solidrectangle(px + contentW - 72, expCardY + 10, px + contentW - 14, expCardY + 34);

        settextstyle(15, 0, _T("等线"));
        settextcolor(RGB(255, 255, 255));
        _stprintf(buf, _T("Lv.%d"), level);
        outtextxy(px + contentW - 43 - textwidth(buf) / 2, expCardY + 22 - textheight(buf) / 2, buf);

        int expWidth = exp * (contentW - 28) / needExp;
        int expBarX = px + 14;
        int expBarY = expCardY + 45;
        int expBarW = contentW - 28;

        setfillcolor(RGB(28, 30, 65));
        solidrectangle(expBarX, expBarY, expBarX + expBarW, expBarY + 16);

        setfillcolor(RGB(175, 120, 255));
        solidrectangle(expBarX, expBarY, expBarX + expWidth, expBarY + 16);

        setfillcolor(RGB(255, 225, 115));
        if (expWidth > 8)
            solidrectangle(expBarX + expWidth - 8, expBarY, expBarX + expWidth, expBarY + 16);

        setlinecolor(RGB(215, 200, 255));
        setlinestyle(PS_SOLID, 1);
        rectangle(expBarX, expBarY, expBarX + expBarW, expBarY + 16);

        settextstyle(14, 0, _T("等线"));
        settextcolor(RGB(235, 235, 255));
        _stprintf(buf, _T("经验 %d / %d"), exp, needExp);
        outtextxy(expBarX + expBarW - textwidth(buf), expCardY + 64, buf);

        py += expCardH + 24;


        setlinecolor(RGB(45, 80, 135));
        line(px, py, x2 - 22, py);
        py += 20;


        settextstyle(18, 0, _T("等线"));
        settextcolor(RGB(255, 230, 130));
        outtextxy(px, py, _T("基础信息"));

        py += 32;

        settextstyle(16, 0, _T("等线"));
        settextcolor(RGB(225, 235, 255));

        _stprintf(buf, _T("波次：%d"), wave);
        outtextxy(px, py, buf);

        _stprintf(buf, _T("分数：%d"), score);
        outtextxy(px, py + 24, buf);

        _stprintf(buf, _T("子弹数量：%d"), bulletCount);
        outtextxy(px, py + 48, buf);

        _stprintf(buf, _T("攻击力：%.1f"), talent.damage);
        outtextxy(px, py + 72, buf);

        _stprintf(buf, _T("攻速：%.1f"), talent.atkSpeed);
        outtextxy(px, py + 96, buf);

        _stprintf(buf, _T("移速：%.2f"), talent.moveSpeed);
        outtextxy(px, py + 120, buf);

        py += 158;

        setlinecolor(RGB(45, 80, 135));
        line(px, py, x2 - 22, py);
        py += 20;


        settextstyle(18, 0, _T("等线"));
        settextcolor(RGB(255, 230, 130));
        outtextxy(px, py, _T("当前任务"));

        py += 32;

        settextstyle(16, 0, _T("等线"));
        settextcolor(RGB(230, 235, 255));

        _stprintf(buf, _T("%s"), getTaskName());
        outtextxy(px, py, buf);

        py += 25;

        _stprintf(buf, _T("进度：%d / %d"), getTaskProgress(), taskTarget);
        outtextxy(px, py, buf);

        py += 25;

        _stprintf(buf, _T("奖励：+%d 分"), taskRewardScore);
        outtextxy(px, py, buf);

        py += 28;

        int taskNow = getTaskProgress() * barW / taskTarget;

        setfillcolor(RGB(24, 28, 55));
        solidrectangle(px, py, px + barW, py + 14);

        setfillcolor(RGB(255, 210, 90));
        solidrectangle(px, py, px + taskNow, py + 14);

        setlinecolor(RGB(140, 160, 210));
        rectangle(px, py, px + barW, py + 14);

        py += 36;

        setlinecolor(RGB(45, 80, 135));
        line(px, py, x2 - 22, py);
        py += 20;


        settextstyle(18, 0, _T("等线"));
        settextcolor(RGB(255, 230, 130));
        outtextxy(px, py, _T("击杀统计"));

        py += 32;

        settextstyle(16, 0, _T("等线"));
        settextcolor(RGB(225, 235, 255));

        _stprintf(buf, _T("普通怪：%d"), cntNormal);
        outtextxy(px, py, buf);

        _stprintf(buf, _T("速度怪：%d"), cntFast);
        outtextxy(px, py + 24, buf);

        _stprintf(buf, _T("坦克怪：%d"), cntTank);
        outtextxy(px, py + 48, buf);

        _stprintf(buf, _T("Boss怪：%d"), cntBoss);
        outtextxy(px, py + 72, buf);


        settextstyle(14, 0, _T("等线"));
        settextcolor(RGB(145, 170, 205));

        outtextxy(px, y2 - 72, _T("WASD 移动"));
        outtextxy(px, y2 - 50, _T("P 暂停 / 继续"));
        outtextxy(px, y2 - 28, _T("ESC 退出游戏"));


        if (messageTimer > 0)
        {
            int playRight = x1 - 20;
            int msgW = 420;
            int msgH = 38;
            int msgX = playRight / 2 - msgW / 2;
            int msgY = HEIGHT - 85;

            if (msgX < 30) msgX = 30;
            if (msgX + msgW > playRight - 30)
                msgW = playRight - 60;

            setfillcolor(RGB(10, 14, 36));
            solidrectangle(msgX, msgY, msgX + msgW, msgY + msgH);

            setlinecolor(RGB(120, 190, 255));
            rectangle(msgX, msgY, msgX + msgW, msgY + msgH);

            settextstyle(17, 0, _T("等线"));
            settextcolor(RGB(255, 240, 160));
            outtextxy(msgX + msgW / 2 - textwidth(message) / 2,
                msgY + msgH / 2 - textheight(message) / 2,
                message);
        }
    }

    void drawMask()
    {
        setbkmode(TRANSPARENT);

        int panelW = 660;
        int panelH = 440;

        if (panelW > WIDTH - 80) panelW = WIDTH - 80;
        if (panelH > HEIGHT - 80) panelH = HEIGHT - 80;

        int x1 = WIDTH / 2 - panelW / 2;
        int y1 = HEIGHT / 2 - panelH / 2;
        int x2 = x1 + panelW;
        int y2 = y1 + panelH;

        setfillcolor(RGB(6, 9, 25));
        solidrectangle(x1, y1, x2, y2);

        setlinecolor(RGB(90, 170, 255));
        setlinestyle(PS_SOLID, 2);
        rectangle(x1, y1, x2, y2);

        setlinecolor(RGB(35, 70, 130));
        rectangle(x1 + 8, y1 + 8, x2 - 8, y2 - 8);

        setlinecolor(RGB(25, 45, 85));
        for (int y = y1 + 42; y < y2 - 20; y += 34)
            line(x1 + 24, y, x2 - 24, y);
    }

    // 升级弹窗
    void drawUpgrade()
    {
        drawMask();

        int panelW = 660;
        int panelH = 440;

        if (panelW > WIDTH - 80) panelW = WIDTH - 80;
        if (panelH > HEIGHT - 80) panelH = HEIGHT - 80;

        int y1 = HEIGHT / 2 - panelH / 2;

        POINT m;
        GetCursorPos(&m);
        ScreenToClient(GetHWnd(), &m);

        settextstyle(36, 0, _T("等线"));
        settextcolor(RGB(255, 225, 120));
        outtextxy(WIDTH / 2 - textwidth(_T("等级提升")) / 2, y1 + 38, _T("等级提升"));

        settextstyle(18, 0, _T("等线"));
        settextcolor(RGB(185, 205, 235));
        outtextxy(WIDTH / 2 - textwidth(_T("点击一张卡片选择强化能力")) / 2, y1 + 88, _T("点击一张卡片选择强化能力"));

        TCHAR buf[150];

        int cardW = panelW - 120;
        int cardH = 64;
        int cardX = WIDTH / 2 - cardW / 2;
        int startY = y1 + 145;

        for (int i = 0; i < 3; i++)
        {
            int cy = startY + i * 82;
            bool hover = (m.x >= cardX && m.x <= cardX + cardW && m.y >= cy && m.y <= cy + cardH);

            setfillcolor(hover ? RGB(42, 62, 120) : RGB(24, 34, 78));
            solidrectangle(cardX, cy, cardX + cardW, cy + cardH);

            setlinecolor(hover ? RGB(255, 225, 120) : RGB(115, 185, 255));
            setlinestyle(PS_SOLID, 2);
            rectangle(cardX, cy, cardX + cardW, cy + cardH);


            int iconX = cardX + 34;
            int iconY = cy + cardH / 2;
            setfillcolor(hover ? RGB(255, 220, 105) : RGB(95, 185, 245));
            solidcircle(iconX, iconY, 16);
            setfillcolor(RGB(255, 255, 255));
            solidcircle(iconX, iconY, 6);

            settextstyle(22, 0, _T("等线"));
            settextcolor(RGB(245, 250, 255));
            _stprintf(buf, _T("%s"), getUpgradeName(upgradeOptions[i]));
            outtextxy(cardX + 68, cy + 13, buf);

            settextstyle(14, 0, _T("等线"));
            settextcolor(hover ? RGB(255, 235, 150) : RGB(170, 195, 230));
            outtextxy(cardX + 68, cy + 42, hover ? _T("点击即可选择") : _T("点击选择这个强化"));
        }

        settextstyle(17, 0, _T("等线"));
        settextcolor(RGB(175, 195, 225));
        outtextxy(WIDTH / 2 - textwidth(_T("升级时游戏会暂停，选择后继续战斗")) / 2,
            y1 + panelH - 42,
            _T("升级时游戏会暂停，选择后继续战斗"));
    }

    void drawButton(RECT r, const TCHAR* text, bool mainButton)
    {
        setfillcolor(mainButton ? RGB(235, 248, 255) : RGB(20, 34, 55));
        solidrectangle(r.left, r.top, r.right, r.bottom);

        setlinecolor(mainButton ? RGB(90, 210, 230) : RGB(100, 160, 210));
        setlinestyle(PS_SOLID, 2);
        rectangle(r.left, r.top, r.right, r.bottom);

        setStartFont(mainButton ? 48 : 44);
        settextcolor(mainButton ? RGB(20, 70, 90) : RGB(220, 240, 255));
        outtextxy((r.left + r.right) / 2 - textwidth(text) / 2,
            (r.top + r.bottom) / 2 - textheight(text) / 2,
            text);
    }

    // 开始页面
    void drawStart()
    {

        for (int i = 0; i < HEIGHT; i++)
        {
            float t = (float)i / (float)HEIGHT;
            int rr = (int)(202 - 68 * t);
            int gg = (int)(226 - 78 * t);
            int bb = (int)(255 - 16 * t);
            setlinecolor(RGB(clampInt(rr, 72, 255), clampInt(gg, 105, 255), clampInt(bb, 225, 255)));
            line(0, i, WIDTH, i);
        }

        for (int i = 0; i < STAR_NUM; i++)
        {
            if (i % 4 == 0) continue;
            int bright = 168 + (int)(sinf(timer * 1.35f + i) * 34);
            bright = clampInt(bright, 118, 230);
            setfillcolor(RGB(clampInt(bright - 20, 0, 255), clampInt(bright + 4, 0, 255), 255));
            solidcircle(starX[i], starY[i], starSize[i]);

            if (i % 15 == 0)
            {
                setlinecolor(RGB(95, 168, 230));
                line(starX[i] - 7, starY[i], starX[i] + 7, starY[i]);
                line(starX[i], starY[i] - 7, starX[i], starY[i] + 7);
            }
        }

        int centerX = WIDTH / 2;
        int panelW = 980;
        int panelH = 620;  // 开始页面板

        if (panelW > WIDTH - 90) panelW = WIDTH - 90;
        if (panelH > HEIGHT - 75) panelH = HEIGHT - 75;

        int panelX = centerX - panelW / 2;
        int panelY = HEIGHT / 2 - panelH / 2;
        if (panelY < 26) panelY = 26;


        int leftBlank = panelX;
        int rightBlank = WIDTH - (panelX + panelW);


        if (leftBlank > 120)
        {
            int dx = panelX / 2;
            int dy = panelY + 158;


            setfillcolor(RGB(210, 230, 250));
            solidcircle(dx - 18, dy + 6, 88);
            setfillcolor(RGB(222, 238, 253));
            solidcircle(dx + 14, dy - 10, 68);


            setlinecolor(RGB(74, 163, 222));
            setlinestyle(PS_SOLID, 3);
            ellipse(dx - 84, dy - 24, dx + 84, dy + 24);
            setlinecolor(RGB(145, 208, 238));
            setlinestyle(PS_SOLID, 2);
            ellipse(dx - 98, dy - 30, dx + 98, dy + 30);


            setfillcolor(RGB(58, 145, 225));
            solidcircle(dx, dy, 38);
            setfillcolor(RGB(102, 195, 242));
            solidcircle(dx - 12, dy - 10, 20);
            setfillcolor(RGB(227, 249, 255));
            solidcircle(dx - 15, dy - 16, 6);
            setfillcolor(RGB(44, 98, 188));
            solidcircle(dx + 18, dy + 14, 16);


            setfillcolor(RGB(255, 224, 120));
            solidcircle(dx + 78, dy - 56, 10);
            setlinecolor(RGB(255, 237, 170));
            circle(dx + 78, dy - 56, 17);


            setfillcolor(RGB(157, 213, 244));
            solidcircle(dx - 84, dy + 88, 9);
            setlinecolor(RGB(198, 234, 248));
            circle(dx - 84, dy + 88, 15);


            setlinecolor(RGB(42, 120, 205));
            line(dx - 108, dy - 80, dx - 86, dy - 80);
            line(dx - 97, dy - 91, dx - 97, dy - 69);
            line(dx + 86, dy + 78, dx + 110, dy + 78);
            line(dx + 98, dy + 66, dx + 98, dy + 90);

            setfillcolor(RGB(70, 165, 232));
            solidcircle(dx - 121, dy + 24, 2);
            solidcircle(dx + 56, dy + 108, 2);
            solidcircle(dx - 56, dy + 116, 3);
            solidcircle(dx + 112, dy - 6, 2);


            setlinecolor(RGB(38, 118, 210));
            setlinestyle(PS_SOLID, 5);
            line(dx - 112, dy + 150, dx - 30, dy + 112);
            setlinecolor(RGB(105, 190, 255));
            setlinestyle(PS_SOLID, 3);
            line(dx - 96, dy + 144, dx - 23, dy + 109);
            setfillcolor(RGB(255, 255, 255));
            solidcircle(dx - 16, dy + 106, 7);
            setfillcolor(RGB(80, 170, 245));
            solidcircle(dx - 18, dy + 108, 3);
        }


        if (rightBlank > 120)
        {
            int dx = panelX + panelW + rightBlank / 2;
            int dy = panelY + panelH - 165;


            setfillcolor(RGB(218, 234, 252));
            solidcircle(dx + 8, dy - 6, 94);
            setfillcolor(RGB(226, 240, 253));
            solidcircle(dx - 20, dy + 10, 68);


            setlinecolor(RGB(135, 199, 236));
            setlinestyle(PS_SOLID, 1);
            circle(dx, dy, 84);
            circle(dx, dy, 60);


            setfillcolor(RGB(255, 214, 88));
            solidcircle(dx, dy, 22);
            setfillcolor(RGB(255, 244, 176));
            solidcircle(dx - 6, dy - 7, 9);
            setlinecolor(RGB(255, 228, 116));
            setlinestyle(PS_SOLID, 2);
            circle(dx, dy, 33);


            setlinecolor(RGB(78, 174, 232));
            setlinestyle(PS_SOLID, 3);
            ellipse(dx - 82, dy - 24, dx + 82, dy + 24);
            setlinecolor(RGB(138, 204, 238));
            setlinestyle(PS_SOLID, 2);
            ellipse(dx - 102, dy - 34, dx + 102, dy + 34);


            setfillcolor(RGB(92, 184, 235));
            solidcircle(dx - 74, dy - 12, 7);
            setfillcolor(RGB(150, 211, 245));
            solidcircle(dx + 62, dy + 24, 6);
            setfillcolor(RGB(104, 161, 226));
            solidcircle(dx + 5, dy - 60, 5);
            setfillcolor(RGB(188, 227, 248));
            solidcircle(dx - 42, dy + 70, 5);


            setlinecolor(RGB(38, 115, 205));
            setlinestyle(PS_SOLID, 5);
            line(dx - 116, dy - 122, dx - 36, dy - 90);
            line(dx - 92, dy - 98, dx - 18, dy - 68);
            line(dx + 8, dy + 120, dx + 88, dy + 88);
            line(dx + 46, dy - 128, dx + 108, dy - 100);

            setlinecolor(RGB(110, 195, 255));
            setlinestyle(PS_SOLID, 3);
            line(dx - 96, dy - 112, dx - 32, dy - 88);
            line(dx - 74, dy - 88, dx - 14, dy - 66);
            line(dx + 24, dy + 110, dx + 92, dy + 86);
            line(dx + 62, dy - 118, dx + 112, dy - 96);

            setfillcolor(RGB(255, 255, 255));
            solidcircle(dx - 30, dy - 87, 6);
            solidcircle(dx - 12, dy - 65, 5);
            solidcircle(dx + 96, dy + 84, 6);
            solidcircle(dx + 116, dy - 94, 5);

            setfillcolor(RGB(60, 155, 240));
            solidcircle(dx - 32, dy - 88, 3);
            solidcircle(dx + 94, dy + 86, 3);


            setlinecolor(RGB(42, 120, 205));
            line(dx + 92, dy - 82, dx + 116, dy - 82);
            line(dx + 104, dy - 94, dx + 104, dy - 70);
            line(dx - 118, dy + 58, dx - 96, dy + 58);
            line(dx - 107, dy + 47, dx - 107, dy + 69);
            line(dx + 78, dy + 122, dx + 98, dy + 122);
            line(dx + 88, dy + 112, dx + 88, dy + 132);
        }

        setfillcolor(RGB(241, 248, 255));
        solidrectangle(panelX, panelY, panelX + panelW, panelY + panelH);
        setlinecolor(RGB(82, 174, 222));
        setlinestyle(PS_SOLID, 2);
        rectangle(panelX, panelY, panelX + panelW, panelY + panelH);
        setlinecolor(RGB(174, 222, 242));
        rectangle(panelX + 10, panelY + 10, panelX + panelW - 10, panelY + panelH - 10);


        int headerY = panelY + 24;

        setfillcolor(RGB(204, 231, 248));
        solidrectangle(centerX - 130, headerY, centerX + 130, headerY + 32);
        setlinecolor(RGB(82, 178, 222));
        rectangle(centerX - 130, headerY, centerX + 130, headerY + 32);

        setStartFont(28);
        settextcolor(RGB(34, 84, 118));
        outtextxy(centerX - textwidth(_T("选择你的雷霆战机")) / 2, headerY + 6, _T("选择你的雷霆战机"));

        setStartFont(100);
        settextcolor(RGB(12, 48, 86));
        outtextxy(centerX - textwidth(GAME_TITLE) / 2, headerY + 40, GAME_TITLE);
        settextcolor(RGB(34, 108, 158));
        outtextxy(centerX - textwidth(GAME_TITLE) / 2 + 3, headerY + 43, GAME_TITLE);

        setStartFont(38);
        settextcolor(RGB(40, 96, 142));
        outtextxy(centerX - textwidth(_T("挑一架飞船，冲进太空战场")) / 2,
            headerY + 140,
            _T("挑一架飞船，冲进太空战场"));


        int buttonY = panelY + panelH - 82;
        int infoY = buttonY - 84;
        int shipAreaY = panelY + 205;
        if (panelH < 570) shipAreaY = panelY + 185;
        int shipAreaH = infoY - shipAreaY - 16;
        if (shipAreaH > 250) shipAreaH = 250;
        if (shipAreaH < 145) shipAreaH = 145;

        int sidePad = 46;
        int cardGap = 22;  // 飞船卡片间距
        int cardW = (panelW - sidePad * 2 - cardGap * 2) / 3;
        int cardH = shipAreaH;

        const TCHAR* shipDesc[3] = {
            _T("均衡稳定，适合新手"),
            _T("清爽轻盈，手感灵活"),
            _T("火力感强，视觉醒目")
        };

        for (int i = 0; i < 3; i++)
        {
            int cx = panelX + sidePad + i * (cardW + cardGap);
            int cy = shipAreaY;
            shipBtns[i].left = cx;
            shipBtns[i].top = cy;
            shipBtns[i].right = cx + cardW;
            shipBtns[i].bottom = cy + cardH;

            bool selected = (player.shipType == i);

            setfillcolor(selected ? RGB(222, 239, 255) : RGB(230, 244, 252));
            solidrectangle(cx, cy, cx + cardW, cy + cardH);
            setlinecolor(selected ? RGB(34, 138, 224) : RGB(138, 207, 232));
            setlinestyle(PS_SOLID, selected ? 3 : 2);
            rectangle(cx, cy, cx + cardW, cy + cardH);

            if (selected)
            {
                setfillcolor(RGB(62, 166, 232));
                solidrectangle(cx + 14, cy + 14, cx + 78, cy + 40);
                setStartFont(20);
                settextcolor(RGB(255, 255, 255));
                outtextxy(cx + 21, cy + 19, _T("已选择"));
            }

            int shipCenterY = cy + cardH / 2 - 18;
            if (cardH < 210) shipCenterY = cy + 54;
            float shipScale = cardH < 210 ? 0.85f : 1.45f;
            int ring1 = cardH < 210 ? 42 : 72;
            int ring2 = cardH < 210 ? 58 : 96;

            setlinecolor(RGB(198, 236, 246));
            circle(cx + cardW / 2, shipCenterY, ring1);
            setlinecolor(RGB(220, 244, 250));
            circle(cx + cardW / 2, shipCenterY, ring2);

            player.drawShipModel(cx + cardW / 2, shipCenterY + 3, shipScale, i, false);

            int nameY = cy + cardH - 78;
            int descY = cy + cardH - 47;
            int clickY = cy + cardH - 24;
            if (cardH < 210)
            {
                nameY = cy + cardH - 66;
                descY = cy + cardH - 40;
                clickY = cy + cardH - 20;
            }

            setStartFont(42);
            settextcolor(RGB(28, 70, 102));
            outtextxy(cx + cardW / 2 - textwidth(player.shipName(i)) / 2, nameY - 4, player.shipName(i));

            setStartFont(28);
            settextcolor(RGB(70, 105, 130));
            outtextxy(cx + cardW / 2 - textwidth(shipDesc[i]) / 2, descY + 1, shipDesc[i]);

            setStartFont(20);
            settextcolor(selected ? RGB(30, 135, 210) : RGB(95, 130, 155));
            outtextxy(cx + cardW / 2 - textwidth(_T("点击选择")) / 2, clickY, _T("点击选择"));
        }


        int infoH = 66;
        int infoW = (panelW - 92 - 24 * 2) / 3;
        const TCHAR* infoTitle[3] = { _T("自动射击"), _T("任务奖励"), _T("点击升级") };
        const TCHAR* infoDesc[3] = { _T("靠近敌人自动锁定"), _T("完成右侧任务变强"), _T("经验满后点卡片") };
        COLORREF infoColor[3] = { RGB(88, 178, 236), RGB(255, 196, 92), RGB(146, 126, 245) };

        for (int i = 0; i < 3; i++)
        {
            int ix = panelX + 46 + i * (infoW + 24);
            setfillcolor(RGB(226, 242, 252));
            solidrectangle(ix, infoY, ix + infoW, infoY + infoH);
            setlinecolor(RGB(140, 207, 232));
            rectangle(ix, infoY, ix + infoW, infoY + infoH);

            setfillcolor(infoColor[i]);
            solidcircle(ix + 31, infoY + 33, 13);
            setfillcolor(RGB(255, 255, 255));
            solidcircle(ix + 31, infoY + 33, 5);

            setStartFont(28);
            settextcolor(RGB(35, 72, 100));
            outtextxy(ix + 55, infoY + 8, infoTitle[i]);

            setStartFont(20);
            settextcolor(RGB(78, 112, 138));
            outtextxy(ix + 55, infoY + 38, infoDesc[i]);
        }


        int buttonW = 220;
        int buttonH = 54;
        int buttonGap = 30;

        btn.left = centerX - buttonW - buttonGap / 2;
        btn.top = buttonY;
        btn.right = btn.left + buttonW;
        btn.bottom = buttonY + buttonH;

        introBtn.left = centerX + buttonGap / 2;
        introBtn.top = buttonY;
        introBtn.right = introBtn.left + buttonW;
        introBtn.bottom = buttonY + buttonH;

        drawButton(btn, _T("开始游戏"), true);
        drawButton(introBtn, _T("游戏说明"), false);

        setStartFont(25);
        settextcolor(RGB(60, 98, 128));
        outtextxy(centerX - textwidth(_T("WASD 移动   P 暂停   R 返回主菜单   ESC 退出")) / 2,
            HEIGHT - 38,
            _T("WASD 移动   P 暂停   R 返回主菜单   ESC 退出"));
    }

    // 游戏说明
    void drawGuide()
    {
        for (int i = 0; i < HEIGHT; i++)
        {
            float t = (float)i / (float)HEIGHT;
            int rr = (int)(225 - 50 * t);
            int gg = (int)(246 - 60 * t);
            int bb = 255;
            setlinecolor(RGB(clampInt(rr, 70, 255), clampInt(gg, 95, 255), bb));
            line(0, i, WIDTH, i);
        }

        int centerX = WIDTH / 2;
        int panelW = 780;
        int panelH = 560;
        if (panelW > WIDTH - 80) panelW = WIDTH - 80;
        if (panelH > HEIGHT - 70) panelH = HEIGHT - 70;

        int panelX = centerX - panelW / 2;
        int panelY = HEIGHT / 2 - panelH / 2;
        if (panelY < 30) panelY = 30;

        setfillcolor(RGB(247, 253, 255));
        solidrectangle(panelX, panelY, panelX + panelW, panelY + panelH);
        setlinecolor(RGB(145, 215, 230));
        setlinestyle(PS_SOLID, 2);
        rectangle(panelX, panelY, panelX + panelW, panelY + panelH);
        setlinecolor(RGB(205, 238, 246));
        rectangle(panelX + 8, panelY + 8, panelX + panelW - 8, panelY + panelH - 8);

        settextstyle(38, 0, _T("等线"));
        settextcolor(RGB(35, 75, 100));
        TCHAR titleBuf[100];
        _stprintf(titleBuf, _T("%s 说明"), GAME_TITLE);
        outtextxy(centerX - textwidth(titleBuf) / 2, panelY + 32, titleBuf);

        int cardX = panelX + 55;
        int cardW = panelW - 110;
        int cardH = 66;
        int y = panelY + 98;

        const TCHAR* titles[4] = {
            _T("玩法目标"),
            _T("成长方式"),
            _T("任务系统"),
            _T("道具说明")
        };

        const TCHAR* descs[4] = {
            _T("操控飞船躲避敌人，自动射击，尽可能坚持更多波次。"),
            _T("击杀敌人获得经验，经验满后直接点击升级卡片选择强化。"),
            _T("完成右侧任务可以获得分数、回血和经验奖励。"),
            _T("绿色为回血包，紫金色为经验球，靠近后自动拾取。")
        };

        COLORREF iconColors[4] = {
            RGB(85, 180, 240),
            RGB(155, 120, 255),
            RGB(255, 205, 90),
            RGB(70, 210, 120)
        };

        for (int i = 0; i < 4; i++)
        {
            int cy = y + i * (cardH + 12);

            setfillcolor(RGB(236, 249, 253));
            solidrectangle(cardX, cy, cardX + cardW, cy + cardH);
            setlinecolor(RGB(170, 225, 238));
            rectangle(cardX, cy, cardX + cardW, cy + cardH);

            setfillcolor(iconColors[i]);
            solidcircle(cardX + 32, cy + cardH / 2, 14);
            setfillcolor(RGB(255, 255, 255));
            solidcircle(cardX + 32, cy + cardH / 2, 5);

            settextstyle(19, 0, _T("等线"));
            settextcolor(RGB(45, 80, 105));
            outtextxy(cardX + 62, cy + 10, titles[i]);

            settextstyle(16, 0, _T("等线"));
            settextcolor(RGB(80, 110, 135));
            outtextxy(cardX + 62, cy + 38, descs[i]);
        }


        backBtn.left = centerX - 130;
        backBtn.right = centerX + 130;
        backBtn.bottom = panelY + panelH - 28;
        backBtn.top = backBtn.bottom - 50;

        drawButton(backBtn, _T("返回主菜单"), true);

        settextstyle(15, 0, _T("等线"));
        settextcolor(RGB(95, 125, 150));
        outtextxy(centerX - textwidth(_T("也可以按 B 返回主菜单")) / 2,
            backBtn.top - 26,
            _T("也可以按 B 返回主菜单"));
    }

    // 暂停页面
    void drawPause()
    {
        drawMask();

        int panelW = 640;
        int panelH = 430;

        if (panelW > WIDTH - 80) panelW = WIDTH - 80;
        if (panelH > HEIGHT - 80) panelH = HEIGHT - 80;

        int x1 = WIDTH / 2 - panelW / 2;
        int y1 = HEIGHT / 2 - panelH / 2;

        int centerX = WIDTH / 2;

        settextstyle(38, 0, _T("等线"));
        settextcolor(RGB(255, 225, 120));
        outtextxy(centerX - textwidth(_T("游戏暂停")) / 2, y1 + 36, _T("游戏暂停"));

        TCHAR buf[180];

        int leftX = x1 + 70;
        int rightX = x1 + 350;
        int startY = y1 + 105;
        int gap = 32;

        settextstyle(19, 0, _T("等线"));
        settextcolor(RGB(225, 238, 255));

        _stprintf(buf, _T("生命值：%d / %d"), player.hp, player.maxHp);
        outtextxy(leftX, startY, buf);

        _stprintf(buf, _T("等级：%d"), level);
        outtextxy(rightX, startY, buf);

        _stprintf(buf, _T("经验：%d / %d"), exp, needExp);
        outtextxy(leftX, startY + gap, buf);

        _stprintf(buf, _T("分数：%d"), score);
        outtextxy(rightX, startY + gap, buf);

        _stprintf(buf, _T("攻击力：%.1f"), talent.damage);
        outtextxy(leftX, startY + gap * 2, buf);

        _stprintf(buf, _T("攻速：%.1f"), talent.atkSpeed);
        outtextxy(rightX, startY + gap * 2, buf);

        _stprintf(buf, _T("移速：%.2f"), talent.moveSpeed);
        outtextxy(leftX, startY + gap * 3, buf);

        _stprintf(buf, _T("子弹速度：%.2f"), talent.bulletSpeed);
        outtextxy(rightX, startY + gap * 3, buf);

        _stprintf(buf, _T("子弹数量：%d"), bulletCount);
        outtextxy(leftX, startY + gap * 4, buf);

        _stprintf(buf, _T("当前波次：%d"), wave);
        outtextxy(rightX, startY + gap * 4, buf);

        _stprintf(buf, _T("任务：%s"), getTaskName());
        outtextxy(leftX, startY + gap * 5, buf);

        _stprintf(buf, _T("任务进度：%d / %d"), getTaskProgress(), taskTarget);
        outtextxy(leftX, startY + gap * 6, buf);

        _stprintf(buf, _T("击杀：普通%d  速度%d  坦克%d  Boss%d"), cntNormal, cntFast, cntTank, cntBoss);
        outtextxy(leftX, startY + gap * 7, buf);

        settextstyle(18, 0, _T("等线"));
        settextcolor(RGB(180, 205, 235));
        outtextxy(centerX - textwidth(_T("按 P 继续游戏    ESC 退出")) / 2,
            y1 + panelH - 45,
            _T("按 P 继续游戏    ESC 退出"));
    }

    // 结束页面
    void drawGameOver()
    {
        drawMask();

        int panelW = 640;
        int panelH = 430;

        if (panelW > WIDTH - 80) panelW = WIDTH - 80;
        if (panelH > HEIGHT - 80) panelH = HEIGHT - 80;

        int y1 = HEIGHT / 2 - panelH / 2;
        int centerX = WIDTH / 2;

        settextstyle(46, 0, _T("等线"));
        settextcolor(RGB(255, 105, 115));
        outtextxy(centerX - textwidth(_T("游戏结束")) / 2, y1 + 58, _T("游戏结束"));

        TCHAR buf[180];

        settextstyle(24, 0, _T("等线"));
        settextcolor(RGB(225, 238, 255));

        _stprintf(buf, _T("最终分数：%d"), score);
        outtextxy(centerX - textwidth(buf) / 2, y1 + 150, buf);

        _stprintf(buf, _T("到达波次：%d    等级：%d"), wave, level);
        outtextxy(centerX - textwidth(buf) / 2, y1 + 195, buf);

        _stprintf(buf, _T("击杀：普通%d  速度%d  坦克%d  Boss%d"), cntNormal, cntFast, cntTank, cntBoss);
        outtextxy(centerX - textwidth(buf) / 2, y1 + 240, buf);

        settextstyle(20, 0, _T("等线"));
        settextcolor(RGB(180, 205, 235));
        outtextxy(centerX - textwidth(_T("按 R 返回主菜单")) / 2,
            y1 + 335,
            _T("按 R 返回主菜单"));
    }

    void draw()
    {
        cleardevice();
        setbkmode(TRANSPARENT);

        if (state == START)
        {
            drawStart();
            return;
        }

        if (state == GUIDE)
        {
            drawGuide();
            return;
        }

        drawGameBg();

        for (auto& item : items) item.draw();
        for (auto& e : enemies) e.draw();
        for (auto& b : bullets) b.draw();
        for (auto& effect : effects) effect.draw();

        player.draw();
        drawUI();

        if (state == PAUSED)
            drawPause();

        if (upgradeState)
            drawUpgrade();

        if (state == GAMEOVER)
            drawGameOver();
    }
};

// 程序入口
int main()
{
    WIDTH = GetSystemMetrics(SM_CXSCREEN);
    HEIGHT = GetSystemMetrics(SM_CYSCREEN);

    initgraph(WIDTH, HEIGHT);
    SetWindowText(GetHWnd(), GAME_TITLE);

    SetWindowLong(GetHWnd(), GWL_STYLE, WS_POPUP);
    SetWindowPos(GetHWnd(), HWND_TOP, 0, 0, WIDTH, HEIGHT, SWP_SHOWWINDOW);

    srand((unsigned)time(NULL));

    initAudio();
    playBGM();

    Game g;
    g.initStar();

    while (true)
    {
        BeginBatchDraw();
        g.update();
        g.draw();
        EndBatchDraw();
        Sleep(10);
    }

    return 0;
}