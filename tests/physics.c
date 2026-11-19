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

    d=(Dino){.x=100,.y=40*k,.vy=3,.energy=100,.alive=1};
    move_runner(&d,DUCK);
    assert(close_to(d.y,35*k) && close_to(d.vy,-.4));
    move_runner(&d,DUCK);
    assert(close_to(d.y,29.6*k) && close_to(d.vy,-.8));
    d=(Dino){.x=100,.y=40*k,.energy=100,.alive=1,.dash_used=1};
    move_runner(&d,0);
    move_runner(&d,DASH);
    assert(!d.dash && d.energy==100); /* No extra dash by releasing the button in the air. */
    d.y=0;
    move_runner(&d,DASH);
    assert(d.dash && d.energy==70);
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

static void test_regeneration(void) {
    const int types[]={0,7,8,OB_CHROME,OB_CAVE,OB_TREE,OB_SPIKES};
    const double expected[]={10.01,30.01,30.01,60.01,10.01,60.01,100};
    World *w=&game.test;
    for (unsigned edition=0;edition<2;edition++) {
        game.birdless=edition;
        for (unsigned i=0;i<sizeof(types)/sizeof(*types);i++) {
            reset_world(w,1,12,1);
            w->obstacles[0]=(Obstacle){.x=80,.w=22,.sprite=types[i]};
            Dino *d=&w->dinos[0];
            d->x=100; d->y=300; d->energy=10; d->ammo=10;
            step_world(w,1,0);
            assert(close_to(d->energy,expected[i]));
            assert(close_to(d->ammo,edition?10:10.01));
            step_world(w,1,0);
            assert(close_to(d->energy,clamp(expected[i]+.01,0,100)));
        }
        reset_world(w,1,12,1);
        Dino *d=&w->dinos[0];
        w->obstacles[0].x=-1600*MOVEMENT_SCALE;
        w->obstacles[0].w=50;
        w->obstacles[1].sprite=OB_TREE;
        d->energy=40;
        step_world(w,1,0); /* Recycling the previous slot is not a pass. */
        assert(w->obstacles[0].serial==1);
        assert(close_to(d->energy,40.01));

        w->obstacles[0]=(Obstacle){.x=w->distance+d->x,.w=60,.h=40,.sprite=7};
        step_world(w,1,0);
        assert(!d->alive && close_to(d->energy,40.01));
    }
}

static void test_food_and_aim(void) {
    World *w=&game.test;
    game.birdless=0;
    reset_world(w,1,3,1);
    Dino *d=&w->dinos[0];
    d->energy=10; d->ammo=10;
    w->coin_x=d->x;
    d->y=w->coin_y;
    step_world(w,1,0);
    assert(d->coin_taken && d->coins==1);
    assert(d->score==0);
    assert(close_to(d->energy,10.01) && close_to(d->ammo,10.01));
    step_world(w,1,0);
    assert(d->coins==1);
    w->tick=1000;
    step_world(w,1,0);
    assert(!d->coin_taken && d->coins==1);

    reset_world(w,1,3,1);
    const double aim[]={0,0,.75},stop[]={0,0,.5};
    update_dino(w,d,0,stop);
    assert(!d->shots[0].active && close_to(d->ammo,20));
    update_dino(w,d,0,aim);
    assert(d->shots[0].active && absd(d->shots[0].vx-35*MOVEMENT_SCALE)<1e-6);
    assert(close_to(d->ammo,15) && d->cooldown==10);
    d->bird=(Bird){.x=d->shots[0].x+2*d->shots[0].vx-10,.y=0,.active=1};
    update_dino(w,d,0,aim);
    assert(!d->shots[1].active && close_to(d->ammo,15));
    double hit_score=1+(d->bird.x-d->x)/W;
    update_dino(w,d,0,aim);
    assert(d->hits==1 && d->shots[0].active && d->bird.active && close_to(d->ammo,20));
    assert(close_to(d->score,hit_score));

    reset_world(w,1,3,1);
    unsigned draws=w->layout.index;
    Bird initial=d->bird;
    for (unsigned i=0;i<49;i++) update_bird(d);
    assert(!d->bird.active);
    update_bird(d);
    assert(d->bird.active && d->bird.x==initial.x && d->bird.y==initial.y);
    assert(w->layout.index==draws);
    update_bird(d);
    assert(close_to(d->bird.vx*d->bird.vx+d->bird.vy*d->bird.vy,25*MOVEMENT_SCALE*MOVEMENT_SCALE));
    double in[4];
    d->bird.x=W;
    aim_sensors(w,d,in);
    for (unsigned i=0;i<4;i++) assert(in[i]==0);
}

static void test_layout(void) {
    LayoutRng rng;
    layout_seed(&rng,5489);
    assert(layout_next(&rng)==3499211612u);
    assert(layout_next(&rng)==581869302u);
    assert(layout_next(&rng)==3890346734u);
    assert(layout_next(&rng)==3586334585u);

    for (unsigned edition=0;edition<2;edition++) {
        game.birdless=edition;
        layout_seed(&rng,123);
        unsigned seen[OB_SPIKES+1]={0};
        for (unsigned i=0;i<1000;i++) {
            Obstacle o=make_obstacle(&rng,0,i);
            assert(o.scale==1);
            assert(o.sprite>=0 && o.sprite<=OB_SPIKES);
            if (o.sprite==OB_BIRD) assert(o.y>=5*MOVEMENT_SCALE && o.y<=105*MOVEMENT_SCALE);
            seen[o.sprite]++;
        }
        assert((seen[OB_BIRD]==0)==(edition!=0));
        for (unsigned type=0;type<=OB_SPIKES;type++)
            assert(seen[type]>0 || (edition && type==OB_BIRD));
    }

    reset_world(&game.train,0,42,0);
    reset_world(&game.test,0,42,0);
    for (unsigned i=0;i<OBSTACLES;i++) {
        const Obstacle *a=&game.train.obstacles[i],*b=&game.test.obstacles[i];
        assert(a->x==b->x && a->sprite==b->sprite && a->y==b->y);
        if (!i) continue;
        const Obstacle *prev=a-1;
        double gap=(a->x-prev->x-prev->w)/MOVEMENT_SCALE;
        gap-=(prev->sprite==OB_TREE?100:0)+(a->sprite==OB_TREE?200:0)+(a->sprite==OB_CHROME?500:0);
        assert(gap>=600 && gap<=750 && gap==(int)gap);
    }
}

static void test_shared_draw_order(void) {
    game.birdless=game.move_only=0;
    World *w=&game.train;
    reset_world(w,2,5489,0);
    /* Restart order in 448900: obstacles, food, then two draws per attacker. */
    assert(w->obstacles[0].sprite==OB_CAVE);
    assert(w->obstacles[1].sprite==OB_TREE);
    assert(w->obstacles[1].x==W+(665+620+200)*MOVEMENT_SCALE);
    unsigned draws=2*OBSTACLES-1+1+2*w->count;
    for (unsigned i=0;i<OBSTACLES;i++) draws+=w->obstacles[i].sprite==OB_BIRD;
    assert(w->layout.index==draws);

    LayoutRng expected=w->layout;
    reset_world(&game.test,2,44,1); /* Opening the browser test must not change training. */
    assert(w->layout.index==draws);
    assert(layout_next(&w->layout)==layout_next(&expected));

    game.rank[0]=0;
    game.deviations[0]=1;
    game.mutation=1;
    breed(1,1,TRAIN_WEIGHTS);
    assert(w->layout.index>draws+1);
    expected=w->layout;
    Obstacle next=make_obstacle(&expected,W,0);
    prepare_world(w,2,0);
    assert(w->obstacles[0].sprite==next.sprite && w->obstacles[0].y==next.y);
}

static void test_breeding(void) {
    game.birdless=game.move_only=game.joint=0;
    game.rank[0]=2;
    game.rank[1]=1;
    game.deviations[2]=1;
    game.deviations[1]=2;
    for (unsigned j=0;j<TRAIN_WEIGHTS;j++) {
        game.genes[2][j]=100+j;
        game.genes[1][j]=200+j;
    }
    prepare_normal();
    layout_seed(&game.train.layout,123);
    game.mutation=0;
    for (unsigned i=0;i<4;i++) {
        breed(i,2,TRAIN_WEIGHTS);
        assert(game.next[i][0]==(i%2?200:100));
        assert(game.next_dev[i]==(i%2?2:1));
    }
    game.mutation=1;
    breed(0,2,TRAIN_WEIGHTS);
    breed(1,2,TRAIN_WEIGHTS);
    unsigned zeros=0;
    for (unsigned j=0;j<TRAIN_WEIGHTS;j++) {
        assert(game.next[0][j]==game.genes[2][j]);
        if (!game.next[1][j]) zeros++;
        else assert(absd(game.next[1][j]-game.genes[1][j])<10);
    }
    assert(zeros==1);
    game.birdless=1;
    breed(0,2,TRAIN_WEIGHTS);
    assert(game.next[0][0]!=game.genes[2][0]);
    for (unsigned j=0;j<TRAIN_WEIGHTS;j++) assert(game.next[0][j]>0);
}

static void test_ground_tiles(void) {
    assert(ground_tile(0)==5);   /* multiple of 20 */
    assert(ground_tile(1)==1);
    assert(ground_tile(2)==2);
    assert(ground_tile(3)==3);
    assert(ground_tile(4)==0);
    assert(ground_tile(10)==4);  /* multiple of 10 */
    assert(ground_tile(20)==5);
    assert(ground_tile(30)==4);
    assert(ground_tile(-20)==5); /* pattern stays stable while scrolling */
    assert(ground_tile(-1)==3);
}

int main(void) {
    test_movement();
    test_obstacles();
    test_scroll();
    test_regeneration();
    test_food_and_aim();
    test_layout();
    test_breeding();
    test_shared_draw_order();
    test_ground_tiles();
    return 0;
}
