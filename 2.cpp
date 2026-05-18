#define _CRT_SECURE_NO_WARNINGS

#include <graphics.h>
#include <windows.h>
#include <math.h>
#include <time.h>
#include <tchar.h>

#define WIDTH 800
#define HEIGHT 600
#define MAX_ENEMY 20
#define MAX_BULLET 100

#define PLAYER_MAX_HP 100
#define ENEMY_BASE_SPEED 1.2f
#define BULLET_SPEED 8
#define SHOOT_CD 10

enum GameState { START, PLAYING, PAUSED, GAMEOVER };
enum EnemyType { NORMAL, FAST, TANK, BOSS };

float dist(float x1, float y1, float x2, float y2)
{
    float dx = x1 - x2;
    float dy = y1 - y2;
    return sqrtf(dx * dx + dy * dy);
}

struct Talent
{
    float atkSpeed = 1.0f;
    float damage = 1.0f;
    float moveSpeed = 1.0f;
} talent;

class Player
{
public:
    float x, y, vx, vy;
    int r = 15;
    int hp = PLAYER_MAX_HP;

    void reset()
    {
        x = WIDTH / 2;
        y = HEIGHT / 2;
        vx = vy = 0;
        hp = PLAYER_MAX_HP;
    }

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
    }

    void draw()
    {
        setfillcolor(hp < 30 ? RGB(255, 80, 80) : RED);
        solidcircle((int)x, (int)y, r);
    }
};

class Enemy
{
public:
    float x, y;
    int r;
    int hp, maxHp;
    float speed;
    bool alive = false;
    EnemyType type;
    bool deadHandled = false;

    void spawn(float px, float py, int wave)
    {
        alive = true;
        deadHandled = false;

        static int bossCount = 0;
        static int lastWave = -1;

        if (wave != lastWave) {
            bossCount = 0;
            lastWave = wave;
        }

        if (wave % 5 == 0 && bossCount < 1) {
            type = BOSS;
            bossCount++;
        }
        else {
            type = (EnemyType)(rand() % 3);
        }

        float waveScale = 1.0f + wave * 0.15f;

        if (type == NORMAL)
        {
            r = 15;
            maxHp = hp =
                (int)(1 * waveScale);
            if (hp < 1) hp = 1;
            speed =
                ENEMY_BASE_SPEED
                + wave * 0.03f;
        }

        if (type == FAST)
        {
            r = 12;
            maxHp = hp =
                (int)(1 * waveScale);
            if (hp < 1) hp = 1;
            speed =
                ENEMY_BASE_SPEED * 1.8f
                + wave * 0.04f;
        }

        if (type == TANK)
        {
            r = 22;
            maxHp = hp =
                (int)(3 * waveScale);
            speed =
                ENEMY_BASE_SPEED * 0.6f
                + wave * 0.02f;
        }

        if (type == BOSS)
        {
            r = 35;
            maxHp = hp =
                (int)(10 * waveScale);
            speed =
                ENEMY_BASE_SPEED * 1.2f
                + wave * 0.03f;
        }

        int safe = 0;
        do {
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

        setfillcolor(type == NORMAL ? BLUE :
            type == FAST ? GREEN :
            type == TANK ? DARKGRAY : MAGENTA);

        solidcircle((int)x, (int)y, r);

        if (type == BOSS)
        {
            float rate = (float)hp / maxHp;
            setfillcolor(RGB(50, 50, 50));
            solidrectangle((int)x - 25, (int)y - 45, (int)x + 25, (int)y - 38);
            setfillcolor(RED);
            solidrectangle((int)x - 25, (int)y - 45, (int)x - 25 + 50 * rate, (int)y - 38);
        }
    }
};

class Bullet
{
public:
    float x, y, vx, vy;
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
                vx = (dx / len) * BULLET_SPEED;
                vy = (dy / len) * BULLET_SPEED;
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
            setfillcolor(YELLOW);
            solidcircle((int)x, (int)y, 4);
        }
    }
};

class Game
{
public:
    Player player;
    Enemy enemies[MAX_ENEMY];
    Bullet bullets[MAX_BULLET];

    int score = 0;
    int wave = 1;
    int enemyCount = 6;

    int shootTimer = 0;
    int hurtCD = 0;

    GameState state = START;
    RECT btn = { 300,300,500,370 };

    int bulletCount = 1;
    bool waveCleared = false;
    bool last1 = false, last2 = false, last3 = false;
    bool lastP = false;
    bool upgradeState = false;

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

        talent = { 1,1,1 };
        for (auto& e : enemies) e.alive = false;
        for (auto& b : bullets) { b.alive = false; b.targetIdx = -1; }
    }

    void spawnEnemies()
    {
        waveCleared = false;

        for (auto& b : bullets)
        {
            b.alive = false;
            b.targetIdx = -1;
        }

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

    void shoot()
    {
        shootTimer++;

        float cd = SHOOT_CD / talent.atkSpeed;

        if (shootTimer < cd) return;

        shootTimer = 0;

        int t = -1;
        float dmin = 99999;

        for (int i = 0; i < MAX_ENEMY; i++)
        {
            if (!enemies[i].alive) continue;

            float d = dist(player.x, player.y,
                enemies[i].x, enemies[i].y);

            if (d < dmin)
            {
                dmin = d;
                t = i;
            }
        }

        if (t == -1) return;

        float baseAngle =
            atan2f(enemies[t].y - player.y,
                enemies[t].x - player.x);

        float spreadRange = 0.3f;

        for (int n = 0; n < bulletCount; n++)
        {
            float offset = 0;

            if (bulletCount > 1)
            {
                offset =
                    -spreadRange / 2.0f
                    + n * (spreadRange / (bulletCount - 1));
            }

            float ang = baseAngle + offset;

            for (int i = 0; i < MAX_BULLET; i++)
            {
                if (!bullets[i].alive)
                {
                    bullets[i].alive = true;

                    float side =
                        (n - (bulletCount - 1) / 2.0f)
                        * 12.0f;

                    float px = -sinf(baseAngle);
                    float py = cosf(baseAngle);

                    bullets[i].x =
                        player.x + px * side;

                    bullets[i].y =
                        player.y + py * side;

                    bullets[i].vx =
                        cosf(ang) * BULLET_SPEED;

                    bullets[i].vy =
                        sinf(ang) * BULLET_SPEED;

                    bullets[i].targetIdx = t;

                    goto nextShot;
                }
            }

        nextShot:;
        }
    }

    void handleUpgrade()
    {
        bool k1 = GetAsyncKeyState('1') & 0x8000;
        bool k2 = GetAsyncKeyState('2') & 0x8000;
        bool k3 = GetAsyncKeyState('3') & 0x8000;
        bool chosen = false;

        if (k1 && !last1) { talent.atkSpeed += 0.2f; chosen = true; }
        if (k2 && !last2) { bulletCount++; chosen = true; }
        if (k3 && !last3) { talent.moveSpeed += 0.15f; chosen = true; }

        last1 = k1; last2 = k2; last3 = k3;

        if (chosen)
        {
            upgradeState = false;
            spawnEnemies();
        }
    }

    void update()
    {
        bool p = GetAsyncKeyState('P') & 0x8000;
        if (p && !lastP)
            state = (state == PLAYING) ? PAUSED : PLAYING;
        lastP = p;

        if (state == START)
        {
            if (GetAsyncKeyState(VK_LBUTTON))
            {
                POINT m;
                GetCursorPos(&m);
                ScreenToClient(GetHWnd(), &m);
                if (m.x > btn.left && m.x < btn.right)
                {
                    reset();
                    state = PLAYING;
                    spawnEnemies();
                }
            }
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

        for (auto& b : bullets) b.update(enemies);

        for (auto& b : bullets)
        {
            if (!b.alive) continue;
            for (auto& e : enemies)
            {
                if (!e.alive) continue;
                if (dist(b.x, b.y, e.x, e.y) < e.r)
                {
                    b.alive = false;
                    e.hp -= (int)talent.damage;
                    if (e.hp <= 0 && !e.deadHandled)
                    {
                        e.deadHandled = true;
                        e.alive = false;
                        score += (e.type == BOSS) ? 200 : 10;
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
                    player.hp -= 10;
                    hurtCD = 30;
                }
                break;
            }
        }

        if (!waveCleared && aliveCount() == 0)
        {
            waveCleared = true;
            wave++;
            enemyCount += 2;

            if (enemyCount > MAX_ENEMY)
                enemyCount = MAX_ENEMY;

            upgradeState = true;
        }
    }

    void drawUI()
    {
        int barWidth = 240;
        int hpWidth = player.hp * barWidth / PLAYER_MAX_HP;

        setfillcolor(RGB(40, 40, 40));
        solidrectangle(10, 10, 260, 60);

        setfillcolor(RED);
        solidrectangle(15, 15, 15 + hpWidth, 55);

        settextcolor(WHITE);
        settextstyle(18, 0, _T("宋体"));
        TCHAR buf[100];
        _stprintf(buf, _T("Wave:%d  Score:%d"), wave, score);
        outtextxy(280, 20, buf);
    }

    void draw()
    {
        cleardevice();

        if (state == START)
        {
            settextstyle(40, 0, _T("宋体"));
            settextcolor(WHITE);
            outtextxy(200, 150, _T("ROGUELIKE SHOOTER"));
            setfillcolor(RGB(80, 80, 80));
            solidrectangle(btn.left, btn.top, btn.right, btn.bottom);
            outtextxy(340, 320, _T("START"));
            return;
        }

        if (state == GAMEOVER)
        {
            settextstyle(50, 0, _T("宋体"));
            settextcolor(RED);
            outtextxy(280, 200, _T("GAME OVER"));
            settextcolor(WHITE);
            settextstyle(25, 0, _T("宋体"));
            TCHAR buf[100];
            _stprintf(buf, _T("Final Score:%d  Wave:%d"), score, wave);
            outtextxy(260, 280, buf);
            outtextxy(260, 330, _T("Press R to Restart"));
            return;
        }

        if (state == PAUSED)
        {
            settextcolor(YELLOW);
            settextstyle(50, 0, _T("宋体"));
            outtextxy(300, 250, _T("PAUSED"));
        }

        for (auto& e : enemies) e.draw();
        for (auto& b : bullets) b.draw();
        player.draw();
        drawUI();

        if (upgradeState)
        {
            settextcolor(YELLOW);
            settextstyle(35, 0, _T("宋体"));
            int titleX = WIDTH / 2 - textwidth(_T("CHOOSE UPGRADE")) / 2;
            outtextxy(titleX, 180, _T("CHOOSE UPGRADE"));

            settextstyle(25, 0, _T("宋体"));
            int optX = 280;
            outtextxy(optX, 230, _T("1 : Attack Speed +"));
            outtextxy(optX, 270, _T("2 : Bullet Count +"));
            outtextxy(optX, 310, _T("3 : Move Speed +"));
        }
    }
};

int main()
{
    initgraph(WIDTH, HEIGHT);
    srand((unsigned)time(NULL));

    Game g;
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