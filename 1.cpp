#define _CRT_SECURE_NO_WARNINGS

#include <graphics.h>
#include <windows.h>
#include <math.h>
#include <time.h>
#include <tchar.h>

/*
    ===================== 常量配置区 =====================
    这里定义游戏中所有“固定参数”，方便后期统一修改
*/
#define WIDTH 800              // 游戏窗口宽度
#define HEIGHT 600             // 游戏窗口高度
#define MAX_ENEMY 20           // 最大敌人数量
#define MAX_BULLET 100         // 最大子弹数量
#define PLAYER_SPEED 4         // 玩家移动速度
#define ENEMY_BASE_SPEED 1.2f  // 敌人基础速度
#define BULLET_SPEED 8         // 子弹速度
#define SHOOT_CD 12            // 射击冷却（帧数）
#define PLAYER_MAX_HP 100      // 玩家最大血量

/*
    ===================== 工具函数 =====================
    计算两点之间的距离（用于碰撞检测 / 追踪）
*/
float getDist(float x1, float y1, float x2, float y2)
{
    float dx = x1 - x2;
    float dy = y1 - y2;
    return sqrtf(dx * dx + dy * dy);
}

/*
    ===================== 敌人类型 =====================
    不同敌人有不同属性
*/
enum EnemyType { NORMAL, FAST, TANK, BOSS };

/*
    ===================== 玩家类 =====================
    控制玩家的移动、状态、绘制
*/
class Player
{
public:
    float x, y;     // 玩家坐标
    int r;          // 玩家半径（碰撞用）
    int speed;      // 移动速度
    int hp;         // 血量
    bool isDead;    // 是否死亡

    // 构造函数：初始化玩家基本属性
    Player()
    {
        r = 15;
        speed = PLAYER_SPEED;
        reset();
    }

    // 重置玩家状态（用于重新开始游戏）
    void reset()
    {
        x = WIDTH / 2.0f;   // 出生在屏幕中心
        y = HEIGHT / 2.0f;
        hp = PLAYER_MAX_HP;
        isDead = false;
    }

    // 玩家移动逻辑（键盘控制）
    void updateMove()
    {
        if (isDead) return; // 死亡不能移动

        float vx = 0, vy = 0;

        // WASD控制方向
        if (GetAsyncKeyState('W') & 0x8000) vy -= speed;
        if (GetAsyncKeyState('S') & 0x8000) vy += speed;
        if (GetAsyncKeyState('A') & 0x8000) vx -= speed;
        if (GetAsyncKeyState('D') & 0x8000) vx += speed;

        // 更新位置
        x += vx;
        y += vy;

        // 防止出界（边界限制）
        if (x < r) x = r;
        if (x > WIDTH - r) x = WIDTH - r;
        if (y < r) y = r;
        if (y > HEIGHT - r) y = HEIGHT - r;
    }

    // 绘制玩家（红色圆形）
    void draw()
    {
        setfillcolor(RED);
        solidcircle((int)x, (int)y, r);
    }
};

/*
    ===================== 敌人类 =====================
    控制敌人的生成、移动AI、绘制
*/
class Enemy
{
public:
    float x, y;        // 敌人坐标
    bool alive;        // 是否存活
    int r;             // 碰撞半径
    float speed;       // 移动速度
    int hp;            // 当前血量
    int maxHp;         // 最大血量
    int hitFlash;      // 受击闪烁效果计时
    EnemyType type;    // 敌人类型

    // 构造函数：默认不激活
    Enemy()
    {
        alive = false;
        hitFlash = 0;
        type = NORMAL;
    }

    /*
        敌人生成函数
        px, py：玩家位置（避免刷脸）
        rate：难度倍率
        t：敌人类型
    */
    void spawn(float px, float py, float rate, EnemyType t)
    {
        alive = true;
        type = t;
        hitFlash = 0;

        // 根据类型设置属性（不同敌人不同能力）
        switch (t)
        {
        case NORMAL:
            r = 15;
            speed = ENEMY_BASE_SPEED * rate;
            hp = maxHp = 1;
            break;

        case FAST:
            r = 12;
            speed = ENEMY_BASE_SPEED * rate * 1.8f;
            hp = maxHp = 1;
            break;

        case TANK:
            r = 22;
            speed = ENEMY_BASE_SPEED * rate * 0.6f;
            hp = maxHp = 3;
            break;

        case BOSS:
            r = 35;
            speed = ENEMY_BASE_SPEED * rate * 1.2f;
            hp = maxHp = 10;
            break;
        }

        // 随机出生点（避免太靠近玩家）
        int safe = 0;
        do {
            x = rand() % (WIDTH - 40) + 20;
            y = rand() % (HEIGHT - 40) + 20;
            safe++;
        } while (getDist(x, y, px, py) < 120 && safe < 50);
    }

    /*
        敌人AI：自动追踪玩家
    */
    void updateAI(float px, float py)
    {
        if (!alive) return;

        float dx = px - x;
        float dy = py - y;

        float len = sqrtf(dx * dx + dy * dy);
        if (len < 0.001f) return;

        // 单位向量移动（朝玩家方向移动）
        x += dx / len * speed;
        y += dy / len * speed;

        // 受击闪烁递减
        if (hitFlash > 0) hitFlash--;

        // 边界限制
        if (x < r) x = r;
        if (x > WIDTH - r) x = WIDTH - r;
        if (y < r) y = r;
        if (y > HEIGHT - r) y = HEIGHT - r;
    }

    // 绘制敌人
    void draw()
    {
        if (!alive) return;

        // 受击时变白
        if (hitFlash > 0) setfillcolor(WHITE);
        else
        {
            if (type == NORMAL) setfillcolor(BLUE);
            if (type == FAST) setfillcolor(GREEN);
            if (type == TANK) setfillcolor(DARKGRAY);
            if (type == BOSS) setfillcolor(MAGENTA);
        }

        solidcircle((int)x, (int)y, r);

        // Boss血条
        if (type == BOSS)
        {
            float hpRate = (float)hp / maxHp;

            setfillcolor(RED);

            // 血条背景位置（Boss头顶）
            solidrectangle(
                (int)x - 25,
                (int)y - 50,
                (int)x - 25 + (int)(50 * hpRate),
                (int)y - 45
            );
        }
    }
};

/*
    ===================== 子弹类 =====================
    玩家发射的攻击单位
*/
class Bullet
{
public:
    float x, y;   // 位置
    float vx, vy; // 速度方向
    bool alive;   // 是否存活
    int r;        // 半径

    Bullet()
    {
        alive = false;
        r = 5;
    }

    // 更新子弹移动
    void update()
    {
        if (!alive) return;

        x += vx;
        y += vy;

        // 出界则销毁
        if (x < 0 || x > WIDTH || y < 0 || y > HEIGHT)
            alive = false;
    }

    // 绘制子弹
    void draw()
    {
        if (!alive) return;

        setfillcolor(YELLOW);
        solidcircle((int)x, (int)y, r);
    }
};

/*
    ===================== 游戏核心类 =====================
    控制所有游戏逻辑（玩家、敌人、子弹、分数等）
*/
class Game
{
public:
    Player player;                 // 玩家
    Enemy enemies[MAX_ENEMY];     // 敌人数组
    Bullet bullets[MAX_BULLET];   // 子弹数组

    int wave = 1;                 // 当前波数
    int enemyCount = 6;           // 当前敌人数量
    float rate = 1.0f;            // 难度倍率
    int shootTimer = 0;           // 射击计时器
    int score = 0;                // 分数

    bool paused = false;          // 暂停状态
    bool lastP = false;           // P键上一次状态
    bool lastR = false;           // R键上一次状态

    int killNormal = 0;          // 普通敌人击杀数
    int killBoss = 0;            // Boss击杀数
    int hurtCD = 0;              // 受伤无敌时间

    // 初始化敌人
    void initEnemies()
    {
        for (int i = 0; i < MAX_ENEMY; i++)
            enemies[i].alive = false;

        int idx = 0;

        // 每5波出现Boss
        bool boss = (wave % 5 == 0);

        if (boss)
            enemies[idx++].spawn(player.x, player.y, rate, BOSS);

        for (int i = idx; i < enemyCount && i < MAX_ENEMY; i++)
            enemies[i].spawn(player.x, player.y, rate, (EnemyType)(rand() % 3));
    }

    // 计算存活敌人数量
    int aliveCount()
    {
        int c = 0;
        for (int i = 0; i < MAX_ENEMY; i++)
            if (enemies[i].alive) c++;
        return c;
    }

    // 自动射击逻辑
    void shoot()
    {
        if (player.isDead || paused) return;

        shootTimer++;
        if (shootTimer < SHOOT_CD) return;
        shootTimer = 0;

        // 找最近敌人
        int target = -1;
        float md = 1e9f;

        for (int i = 0; i < MAX_ENEMY; i++)
        {
            if (!enemies[i].alive) continue;

            float d = getDist(player.x, player.y, enemies[i].x, enemies[i].y);

            if (d < md)
            {
                md = d;
                target = i;
            }
        }

        if (target == -1) return;

        float dx = enemies[target].x - player.x;
        float dy = enemies[target].y - player.y;

        float len = sqrtf(dx * dx + dy * dy);
        if (len < 0.001f) return;

        // 找空子弹位置发射
        for (int i = 0; i < MAX_BULLET; i++)
        {
            if (!bullets[i].alive)
            {
                bullets[i].alive = true;
                bullets[i].x = player.x;
                bullets[i].y = player.y;

                bullets[i].vx = dx / len * BULLET_SPEED;
                bullets[i].vy = dy / len * BULLET_SPEED;
                break;
            }
        }
    }

    // 游戏更新逻辑
    void update()
    {
        // P键暂停
        bool p = GetAsyncKeyState('P') & 0x8000;
        if (p && !lastP) paused = !paused;
        lastP = p;

        // R键重开
        bool r = GetAsyncKeyState('R') & 0x8000;
        if (r && !lastR)
        {
            wave = 1;
            enemyCount = 6;
            rate = 1;
            score = 0;
            killNormal = killBoss = 0;
            hurtCD = 0;

            player.reset();
            srand((unsigned)time(NULL));
            initEnemies();
        }
        lastR = r;

        if (paused) return;

        // 玩家死亡
        if (player.hp <= 0)
            player.isDead = true;

        if (player.isDead) return;

        if (hurtCD > 0) hurtCD--;

        player.updateMove();
        shoot();

        // 更新敌人和子弹
        for (int i = 0; i < MAX_ENEMY; i++)
            enemies[i].updateAI(player.x, player.y);

        for (int i = 0; i < MAX_BULLET; i++)
            bullets[i].update();

        // 子弹碰撞检测
        for (int i = 0; i < MAX_BULLET; i++)
        {
            if (!bullets[i].alive) continue;

            for (int j = 0; j < MAX_ENEMY; j++)
            {
                if (!enemies[j].alive) continue;

                if (getDist(bullets[i].x, bullets[i].y,
                    enemies[j].x, enemies[j].y) < enemies[j].r)
                {
                    bullets[i].alive = false;
                    enemies[j].hp--;
                    enemies[j].hitFlash = 5;

                    if (enemies[j].hp <= 0)
                    {
                        enemies[j].alive = false;

                        if (enemies[j].type == BOSS)
                        {
                            score += 200;
                            killBoss++;
                        }
                        else
                        {
                            score += 10;
                            killNormal++;
                        }
                    }
                    break;
                }
            }
        }

        // 玩家与敌人碰撞
        for (int i = 0; i < MAX_ENEMY; i++)
        {
            if (!enemies[i].alive) continue;

            if (getDist(player.x, player.y,
                enemies[i].x, enemies[i].y) < player.r + enemies[i].r)
            {
                if (hurtCD == 0)
                {
                    player.hp -= 10;
                    hurtCD = 20;
                }

                score -= 5;
                break;
            }
        }

        // 清空当前波次进入下一波
        if (aliveCount() == 0)
        {
            wave++;
            enemyCount += 2;
            rate += 0.1f;
            score += 50;
            initEnemies();
        }
    }

    // 绘制游戏画面
    void draw()
    {
        for (int i = 0; i < MAX_ENEMY; i++)
            enemies[i].draw();

        for (int i = 0; i < MAX_BULLET; i++)
            bullets[i].draw();

        player.draw();

        settextcolor(WHITE);
        settextstyle(20, 0, _T("宋体"));

        TCHAR buf[200];

        // UI显示
        if (!player.isDead)
        {
            _stprintf(buf,
                _T("Wave:%d HP:%d Score:%d N:%d B:%d %s"),
                wave, player.hp, score, killNormal, killBoss,
                paused ? _T("[PAUSED]") : _T(""));
        }
        else
        {
            _stprintf(buf,
                _T("GAME OVER | Normal:%d Boss:%d Score:%d"),
                killNormal, killBoss, score);
        }

        outtextxy(20, 20, buf);
    }
};

/*
    ===================== 主函数 =====================
    程序入口：初始化窗口 + 游戏循环
*/
int main()
{
    initgraph(WIDTH, HEIGHT);   // 创建窗口
    srand((unsigned)time(NULL)); // 初始化随机数

    Game g;

    // 游戏主循环
    while (true)
    {
        BeginBatchDraw(); // 双缓冲开始
        cleardevice();    // 清屏

        g.update();       // 更新逻辑
        g.draw();         // 绘制画面

        EndBatchDraw();   // 显示帧
        Sleep(10);        // 控制帧率
    }

    return 0;
}