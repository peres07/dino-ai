/* Verify the actual simulation routines without shipping debug exports. */
#include <assert.h>
#include "../wasm/game.c"

void store_record(unsigned score,const unsigned char *p,unsigned len,unsigned type) {
    (void)score; (void)p; (void)len; (void)type;
}
void draw_game(void) {}
void draw_aux(void) {}

static int close_to(double a,double b) { return absd(a-b)<1e-8; }

static void test_movement(void) {
    const double k=MOVEMENT_SCALE;
    Dino d={.x=100,.energy=100,.alive=1};
    move_runner(&d,JUMP|LEFT);
    assert(close_to(d.x,100-4.5*k));
    assert(close_to(d.y,9*k));
    assert(close_to(d.vy,8.6));
    move_runner(&d,DOUBLE|RIGHT);
    assert(close_to(d.x,100-1.5*k));
    assert(close_to(d.y,18*k));
    assert(d.double_used && close_to(d.energy,80));
    d=(Dino){.x=100,.energy=100,.alive=1};
    move_runner(&d,SUPER|DASH);
    assert(close_to(d.x,100+20*k));
    assert(close_to(d.y,9*1.8*k));
    assert(close_to(d.dash_v,18));
    assert(close_to(d.energy,20));
}

static void test_obstacles(void) {
    const double k=MOVEMENT_SCALE;
    World w={0};
    for(unsigned i=1;i<OBSTACLES;i++) w.obstacles[i].x=3000;
    w.obstacles[0]=(Obstacle){100,665*k,665*k,35*k,1,OB_CAVE,0,0};
    Dino d={.x=105,.alive=1};
    assert(obstacle_collision(&w,&d));
    d.duck=1;
    assert(!obstacle_collision(&w,&d));
    d.duck=0;
    d.x=100+665*k;
    assert(!obstacle_collision(&w,&d));

    w.obstacles[0]=(Obstacle){100,220*k,165*k,5*k,1,OB_TREE,0,0};
    d.x=105;
    assert(obstacle_collision(&w,&d));
    d.y=175*k;
    assert(!obstacle_collision(&w,&d));
    d.y=230*k;
    assert(obstacle_collision(&w,&d));
}

static void test_scroll(void) {
    for(unsigned i=0;i<2;i++) {
        game.birdless=i;
        reset_world(&game.train,0,1,0);
        World *w=&game.train;
        assert(close_to(w->speed,5));
        step_world(w,0,0);
        assert(close_to(w->distance,5*MOVEMENT_SCALE));
        assert(close_to(w->speed,5.001));
        w->speed=9.9995;
        step_world(w,0,0);
        assert(close_to(w->speed,10) && w->speed_step<0);
        step_world(w,0,0);
        assert(close_to(w->speed,9.999));
        w->speed=5.0005;
        step_world(w,0,0);
        assert(close_to(w->speed,5) && w->speed_step>0);
    }
}

int main(void) {
    test_movement();
    test_obstacles();
    test_scroll();
    return 0;
}
