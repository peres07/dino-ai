#ifndef DINO_GAME_H
#define DINO_GAME_H

#include "network.h"

#define W 1200
#define MOVEMENT_SCALE (W/1920.) /* movement PE window: 1920 x 1080 */
#define H 540
#define VIEW_H 675
#define GROUND 434
#define MAX_POP 1000
#define TRAIN_WEIGHTS 35
#define DODGE_WEIGHTS 517
#define GENOME_SIZE (TRAIN_WEIGHTS + DODGE_WEIGHTS)
#define RECORD_BYTES (12 + 2 * 28 + 8 * (TRAIN_WEIGHTS + MAX_WEIGHTS))
#define HISTORY_CAP 8192
#define MOVEMENT_GRAPH_WIDTH 606
#define BIRDS_GRAPH_WIDTH 150
#define OBSTACLES 7
#define PI 3.141592653589793

enum { JUMP=1, DUCK=2, DOUBLE=4, LEFT=8, RIGHT=16, SUPER=32, DASH=64,
    AIM_UP=128, AIM_DOWN=256, SHOOT=512 };
enum { PAN_UP=1, PAN_DOWN=2, PAN_LEFT=4, PAN_RIGHT=8, ZOOM_IN=16, ZOOM_OUT=32 };
enum { OB_BIRD_LOW=9, OB_BIRD_HIGH, OB_CHROME, OB_CAVE, OB_TREE, OB_SPIKES };

typedef struct { uint32_t state; } Rng;
typedef struct { double x, w, h, y, scale; int sprite; double drift; unsigned serial; } Obstacle;
typedef struct { double x, y, vx, vy; int active; } Shot;
typedef struct { double x, y, vx, vy; int timer, active; } Bird;
typedef struct {
    double x, y, vy, energy, ammo, angle, score, dash_v;
    unsigned super_used, dash_used, next_obstacle;
    unsigned alive, duck, double_used, dash, cooldown, smoke, hits, coins, coin_taken;
    Bird bird;
    Shot shots[6];
} Dino;
typedef struct {
    Dino dinos[MAX_POP];
    Obstacle obstacles[OBSTACLES];
    Rng rng;
    unsigned count, alive, tick, serial, lead, winner;
    double distance, speed, speed_step, coin_x, coin_y;
} World;
typedef struct { double best, average, diversity; } Sample;
typedef struct {
    World train, test;
    double genes[MAX_POP][GENOME_SIZE], next[MAX_POP][GENOME_SIZE];
    double deviations[MAX_POP], next_dev[MAX_POP], best[GENOME_SIZE];
    double first_round[MAX_POP];
    unsigned rank[MAX_POP];
    double dodge[MAX_WEIGHTS], model[MAX_WEIGHTS], test_dodge[MAX_WEIGHTS], firing_dodge[MAX_WEIGHTS];
    Shape dodge_shape, model_shape, test_dodge_shape, firing_dodge_shape;
    Rng rng;
    Sample history[HISTORY_CAP];
    double test_history[HISTORY_CAP], test_sum, test_sum_sq, test_min, test_max;
    unsigned generation, record, history_count, history_start, testing, model_ready, ready;
    unsigned birdless, joint, test_joint, round, test_rounds, test_wait;
    unsigned move_only, test_move_only, firing_custom;
    unsigned population, parents, limit, cfg_population, cfg_parents, cfg_limit;
    double mutation, cfg_mutation, camera_x, camera_y, zoom, graph_offset, tick_credit, elapsed, fps, mps;
    double speed;
    unsigned drawing, info, overlay, moving_avg, paused;
} Game;

extern Game game;
void draw_game(void);
void draw_aux(void);
unsigned dodge_actions(const World *w, const Dino *d, int test, unsigned i, Trace *trace);
void aim_outputs(const World *w, const Dino *d, int test, unsigned i, double out[3], Trace *trace);
int movement_only(int test);
const double *movement_weights(int test, unsigned i, Shape *shape);
unsigned runner_pose(const Dino *d,unsigned tick,double *width,double *height);

static inline double clamp(double x, double a, double b) { return x < a ? a : x > b ? b : x; }
static inline double absd(double x) { return __builtin_fabs(x); }

#endif
