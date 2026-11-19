#include "game.h"

#define API __attribute__((visibility("default")))

__attribute__((import_module("browser"), import_name("record")))
extern void store_record(unsigned score, const unsigned char *bytes, unsigned size, unsigned type);

Game game;
static const Shape MOVEMENT_SHAPE = {10, 15, 7, 2, 0, 0, 1};
static unsigned char transfer[MODEL_BYTES];

/* Clang can emit these for large struct moves in a freestanding build. */
void *memset(void *p, int c, size_t n) {
    unsigned char *out = p;
    for (size_t i = 0; i < n; i++) out[i] = (unsigned char)c;
    return p;
}

void *memcpy(void *dst, const void *src, size_t n) {
    unsigned char *out = dst;
    const unsigned char *in = src;
    for (size_t i = 0; i < n; i++) out[i] = in[i];
    return dst;
}

static uint32_t random_u32(Rng *r) {
    uint32_t x = r->state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return r->state = x;
}

/* GSL's MT19937 stream used by the native obstacle/food integer sampler. */
static void layout_seed(LayoutRng *r,uint32_t seed) {
    r->state[0]=seed?seed:4357;
    for (unsigned i=1;i<624;i++) {
        uint32_t prev=r->state[i-1];
        r->state[i]=1812433253u*(prev^(prev>>30))+i;
    }
    r->index=624;
}

static uint32_t layout_next(LayoutRng *r) {
    if (r->index==624) {
        for (unsigned i=0;i<624;i++) {
            uint32_t y=(r->state[i]&0x80000000u)|(r->state[(i+1)%624]&0x7fffffffu);
            r->state[i]=r->state[(i+397)%624]^(y>>1)^((y&1)?0x9908b0dfu:0);
        }
        r->index=0;
    }
    uint32_t y=r->state[r->index++];
    y^=y>>11;
    y^=(y<<7)&0x9d2c5680u;
    y^=(y<<15)&0xefc60000u;
    return y^(y>>18);
}

static unsigned layout_roll(LayoutRng *r,unsigned count) {
    return (unsigned)(((uint64_t)layout_next(r)*count)>>32);
}

static double noise(LayoutRng *r) {
    /* Sum of uniforms (zero mean, unit variance). */
    double v = -3;
    for (unsigned i = 0; i < 6; i++) v += layout_next(r)/4294967296.;
    return v * 1.4142135623730951;
}

/* esGenetica::sortearNumeroNormal (440150): Box-Muller on two integer
   samples scaled by 1/10000. Precompute its discrete radii and phases;
   zero radius input to log is rejected rather than producing infinity. */
static double radius[10001], phase_sin[10001];
static void prepare_normal(void) {
    if (radius[1] != 0) return;
    for (unsigned i=1; i<=10000; i++) {
        double x=i*.0001;
        int exponent=0;
        while (x < .7071067811865475) { x*=2; exponent--; }
        double t=(x-1)/(x+1), term=t, sum=t;
        for (unsigned k=3;k<=21;k+=2) { term*=t*t; sum+=term/k; }
        radius[i]=__builtin_sqrt(-2*(2*sum + exponent*.6931471805599453));
        phase_sin[i]=dino_sin(i*(2*PI/10000));
    }
}

static double normal(LayoutRng *r) {
    unsigned a,b;
    do {
        a=layout_roll(r,10001);
        b=layout_roll(r,10001);
    } while (!a);
    return radius[a]*phase_sin[b];
}

static unsigned genome_size(void) {
    if (game.birdless) return DODGE_WEIGHTS;
    if (game.move_only) return (unsigned)weight_count(game.dodge_shape);
    return TRAIN_WEIGHTS + (game.joint ? (unsigned)weight_count(game.dodge_shape) : 0);
}

int movement_only(int test) {
    return game.birdless || (test ? game.test_move_only : game.move_only);
}

const double *movement_weights(int test, unsigned i, Shape *shape) {
    if (test && movement_only(1)) { *shape=game.model_shape; return game.model; }
    if (test && game.test_joint) { *shape=game.test_dodge_shape; return game.test_dodge; }
    if (!test && game.birdless) { *shape=MOVEMENT_SHAPE; return game.genes[i]; }
    if (!test && !game.joint && !game.move_only && game.firing_custom) {
        *shape=game.firing_dodge_shape; return game.firing_dodge;
    }
    *shape=game.dodge_shape;
    if (!test && game.move_only) return game.genes[i];
    if (!test && game.joint) return game.genes[i]+TRAIN_WEIGHTS;
    return game.dodge;
}

static void seed_movement(void) {
    if ((!game.joint && !game.move_only) || !game.ready) return;
    unsigned weights = (unsigned)weight_count(game.dodge_shape);
    for (unsigned i = 0; i < game.population; i++) {
        double *moving = game.genes[i] + (game.move_only ? 0 : TRAIN_WEIGHTS);
        for (unsigned j = 0; j < weights; j++) {
            double base = game.dodge[j];
            /* Keep one reference; the rest explore several radii around the pretrained policy. */
            double spread = i ? (.12 + .16*(i%5)) * (12 + absd(base)) : 0;
            moving[j] = i ? base + spread*noise(&game.train.layout) : base;
        }
    }
}

static Obstacle make_obstacle(LayoutRng *r,double x,unsigned serial) {
    static const unsigned widths[] = {32,23,15,49,73,23,91,49,226};
    static const unsigned heights[] = {33,46,33,33,47,65,28,117,28};
    /* Types 11/12 in the PE are physical cave/tree obstacles. The large
       sprites are attached to them, not independent parallax scenery. */
    unsigned type=layout_roll(r,game.birdless?13:14)+game.birdless;
    /* Preserve the original single obstacle-bird slot only in the bird edition. */
    unsigned kind=type==0?OB_BIRD:type<=9?type-1:type;
    const double k=MOVEMENT_SCALE;
    switch (kind) {
        case OB_BIRD:
            return (Obstacle){x,42*k,34*k,(5+layout_roll(r,101))*k,1,OB_BIRD,0,serial};
        case OB_CHROME: return (Obstacle){x+500*k,250*k,250*k,5*k,1,OB_CHROME,-5,serial};
        case OB_CAVE: return (Obstacle){x,665*k,665*k,35*k,1,OB_CAVE,0,serial};
        case OB_TREE: return (Obstacle){x+200*k,220*k,165*k,5*k,1,OB_TREE,0,serial};
        case OB_SPIKES: return (Obstacle){x,1400*k,30*k,5*k,1,OB_SPIKES,0,serial};
        default: {
            return (Obstacle){x,widths[kind]*k,heights[kind]*k,5*k,1,(int)kind,0,serial};
        }
    }
}

static double next_obstacle_x(World *w,const Obstacle *prev,int initial) {
    double width=initial?prev->w/prev->scale:prev->w;
    double x=prev->x+width+MOVEMENT_SCALE*(600+layout_roll(&w->layout,151)+(prev->sprite==OB_TREE?100:0));
    return w->distance+(int)((x-w->distance)/MOVEMENT_SCALE)*MOVEMENT_SCALE;
}

static void reset_food(World *w) {
    w->coin_x=w->distance+MOVEMENT_SCALE*layout_roll(&w->layout,1881);
    w->coin_y=(150-25)*MOVEMENT_SCALE;
    for (unsigned i=0;i<w->count;i++) w->dinos[i].coin_taken=0;
}

static void seed_world(World *w,unsigned seed) {
    layout_seed(&w->layout,seed);
    w->starts.state=seed^0xb5629a49u;
    if (!w->starts.state) w->starts.state=1;
}

static void spawn_bird(World *w,Bird *b);

static void prepare_world(World *w,unsigned n,int testing) {
    w->tick = w->lead = w->winner = 0;
    w->alive = w->count = n;
    w->serial = 0;
    w->distance = 0;
    w->speed=5;
    w->speed_step=.001;
    double x=W;
    for (unsigned i = 0; i < OBSTACLES; i++) {
        w->obstacles[i] = make_obstacle(&w->layout, x, w->serial++);
        if (i+1<OBSTACLES) x=next_obstacle_x(w,&w->obstacles[i],1);
    }
    reset_food(w);
    for (unsigned i = 0; i < n; i++) {
        if (game.birdless) {
            w->dinos[i] = (Dino){.x=MOVEMENT_SCALE*(325 + random_u32(&w->starts)%300), .energy=100, .alive=1};
            continue;
        }
        /* Native prepararDinossauros spreads every individual around the
           spawn X (rand()%300 - 150 native px) instead of stacking all of
           them on one point. */
        double spread=((int)(random_u32(&w->starts)%300)-150)*MOVEMENT_SCALE;
        w->dinos[i] = (Dino){.x=testing && i == 0 ? 86 : 132+spread, .energy=100, .ammo=20,
            .alive=1, .bird={.timer=50}};
        if (!movement_only(testing)) {
            Bird *b=&w->dinos[i].bird;
            spawn_bird(w,b);
            b->active=0;
            b->timer=50;
        }
    }
}

static void reset_world(World *w,unsigned n,unsigned seed,int testing) {
    seed_world(w,seed);
    prepare_world(w,n,testing);
}

static const Obstacle *nearest_obstacle(const Obstacle obstacles[OBSTACLES],double x) {
    for (unsigned i = 0; i < OBSTACLES; i++) {
        if (obstacles[i].x+obstacles[i].w>x) return &obstacles[i];
    }
    return 0;
}

unsigned dodge_actions(const World *w, const Dino *d, int test, unsigned i, Trace *trace) {
    const Obstacle *o=nearest_obstacle(w->obstacles,w->distance+d->x);
    double obstacle_drift=o?o->drift:0;
    /* Native Y starts at 25; world/render use height above the floor. */
    double scale=MOVEMENT_SCALE, floor_y=25;
    double in[10] = {d->x/W, (d->y/scale+floor_y)/540., o?o->w/(250*scale):0, o?o->h/(250*scale):0,
        ((o?o->x-w->distance:W)-d->x)/(W*.5), o?(o->y/scale+floor_y)/130:0, (w->speed*w->speed)/100,
        d->energy/100, obstacle_drift*obstacle_drift/25, (w->coin_x-w->distance-d->x)/W};
    double out[7];
    Shape shape;
    const double *weights=movement_weights(test,i,&shape);
    infer(shape,weights,in,out,trace);
    unsigned actions = 0;
    for (unsigned i = 0; i < 7; i++) if (out[i] > 0) actions |= 1u << i;
    return actions;
}

static int bird_in_view(const Bird *b) {
    return b->active && b->x+BIRD_W>0 && b->x<W && b->y+BIRD_H>-25*MOVEMENT_SCALE && b->y<BIRD_CEILING;
}

static void aim_sensors(const World *w,const Dino *d,double in[4]) {
    for (unsigned j=0;j<4;j++) in[j]=0;
    if (!bird_in_view(&d->bird)) return;
    double dx=d->bird.x-d->x,dy=d->bird.y-d->y;
    in[0]=1-__builtin_sqrt(dx*dx+dy*dy)/W;
    in[1]=d->ammo/20;
    double width,height;
    runner_pose(d,w->tick,&width,&height);
    dx+=(BIRD_W-width)*.5;
    dy+=(BIRD_H-height)*.5;
    double dist=__builtin_sqrt(dx*dx+dy*dy);
    if (dist>0) {
        double c=dino_cos(d->angle),s=dino_sin(d->angle);
        in[2]=(dx*c-dy*s)/dist;
        in[3]=(dy*c+dx*s)/dist;
    }
}

void aim_outputs(const World *w, const Dino *d, int test, unsigned i, double out[3], Trace *trace) {
    if (movement_only(test)) {
        out[0] = out[1] = out[2] = 0;
        if (trace) *trace = (Trace){0};
        return;
    }
    double in[4];
    aim_sensors(w,d,in);
    if (test) infer(game.model_shape, game.model, in, out, trace);
    else infer(TRAIN_SHAPE, game.genes[i], in, out, trace);
}

static void spawn_bird(World *w,Bird *b) {
    unsigned side=layout_roll(&w->layout,5);
    if (side==0 || side==4) {
        *b=(Bird){.x=side==0?-50*MOVEMENT_SCALE:W+50*MOVEMENT_SCALE,
            .y=(300-25+layout_roll(&w->layout,431))*MOVEMENT_SCALE,.active=1};
    } else {
        *b=(Bird){.x=((double)layout_roll(&w->layout,2021)-50)*MOVEMENT_SCALE,.y=BIRD_CEILING,.active=1};
    }
}

static void update_bird(Dino *d) {
    Bird *b = &d->bird;
    if (!b->active) {
        if (b->timer>0 && --b->timer>0) return;
        b->active=1;
        return;
    }
    double dx=d->x-b->x,dy=d->y-b->y;
    double dist=__builtin_sqrt(dx*dx+dy*dy);
    double speed=dist>0?clamp(dist,0,5*MOVEMENT_SCALE)/dist:0;
    b->vx=dx*speed;
    b->vy=dy*speed;
    b->x += b->vx;
    b->y += b->vy;
}

static int overlap(double a, double b, double c, double d) { return a < d && c < b; }

unsigned runner_pose(const Dino *d,unsigned tick,double *width,double *height) {
    static const unsigned widths[]={40,40,56,56,40,40,56,36,43,40,40,51};
    static const unsigned heights[]={43,43,25,26,43,43,25,37,48,47,43,44};
    unsigned pose;
    if (!d->alive) pose=10;
    else if (d->dash) pose=11;
    else if (d->duck) pose=2+(tick/5)%2;
    else if (d->super_used && d->vy>0) pose=9;
    else if (d->y>0 && d->double_used) pose=7;
    else if (d->y>0 && d->vy < -5) pose=8;
    else pose=d->y>0?0:(tick/5)%2;
    *width=widths[pose]*MOVEMENT_SCALE;
    *height=heights[pose]*MOVEMENT_SCALE;
    return pose;
}

static int obstacle_collision(const World *w, const Dino *d) {
    const double k=MOVEMENT_SCALE;
    double width,height;
    runner_pose(d,w->tick,&width,&height);
    double left=d->x+.1*width,right=d->x+.9*width;
    double bottom=d->y+.1*height,top=d->y+height;
    for (unsigned i = 0; i < OBSTACLES; i++) {
        const Obstacle *o = &w->obstacles[i];
        double x = o->x - w->distance;
        if (x>right) break;
        if (!overlap(left,right,x+5*k,x+o->w)) continue;
        if (o->sprite==OB_TREE) {
            if (overlap(bottom,top,10*k,170*k) || top>235*k) return 1;
        } else if (o->sprite==OB_CHROME) {
            double r=o->w*.5, cx=x+r, cy=o->y+r;
            double dx=cx-clamp(cx,left,right), dy=cy-clamp(cy,bottom,top);
            if (dx*dx+dy*dy<r*r) return 1;
        } else if (overlap(bottom,top,o->y+5*k,o->y+o->h)) return 1;
    }
    return 0;
}

/* Both PEs store 9/-4.5 for jump/left. Duck adds a -5 displacement
   before the floor check; it doesn't replace gravity. Dash re-arms on
   the ground, not when its button is released. */
static void move_runner(Dino *d,unsigned actions) {
    int grounded = d->y <= 0;
    if (d->smoke) d->smoke--;
    if (grounded) {
        d->double_used=d->super_used=0;
        if (!d->dash) d->dash_used=0;
    }
    if ((actions & JUMP) && grounded) d->vy = 9;
    d->duck = !!(actions & DUCK);
    if (d->duck && d->vy > 0) d->vy = 0;
    if ((actions & DOUBLE) && (!grounded || (actions & JUMP)) && !d->double_used && d->energy >= 20) {
        d->vy = 9; d->double_used = 1; d->energy -= 20;
    }
    if ((actions & SUPER) && grounded && !(actions & JUMP) && !d->super_used && d->energy >= 50 && !d->double_used) {
        d->vy = 1.8*9; d->energy -= 50; d->super_used = 1; d->smoke = 15;
    }
    if ((actions & DASH) && !d->dash_used && !d->dash && d->energy >= 30) {
        d->dash_v = 20; d->energy -= 30; d->dash = d->dash_used = 1; d->smoke = 14;
    }
    double dx = d->dash ? d->dash_v : 3*!!(actions & RIGHT) - 4.5*!!(actions & LEFT);
    d->x = clamp(d->x + dx*MOVEMENT_SCALE, 5*MOVEMENT_SCALE, W-45*MOVEMENT_SCALE);
    d->y += (d->vy-(d->duck && (!grounded || d->vy>0)?5:0))*MOVEMENT_SCALE;
    if (d->dash) {
        d->dash_v *= .9; d->vy = 0;
        if (d->dash_v < 1) d->dash = 0;
    } else d->vy -= .4;
    if (d->y <= 0) d->y = d->vy = 0;

}

static void collect_food(const World *w,Dino *d) {
    if (d->coin_taken) return;
    double width,height;
    runner_pose(d,w->tick,&width,&height);
    const double k=MOVEMENT_SCALE;
    double x=w->coin_x-w->distance,y=w->coin_y;
    if (overlap(d->x+.1*width,d->x+width,x+2*k,x+36*k) &&
        overlap(d->y+.1*height,d->y+height,y+4*k,y+46*k)) {
        d->coin_taken=1;
        d->coins++;
    }
}

/* Both PEs compare the next obstacle before and after the tick. The serial
   identifies the same obstacle even when the port's queue is shifted. */
static int changed_obstacle(const Obstacle *before,const Obstacle *after) {
    return before && (!after || before->serial!=after->serial);
}

static void regenerate(Dino *d,const Obstacle *before,const Obstacle *after) {
    if (!d->alive) return;
    d->energy=clamp(d->energy+.01,0,100);
    if (!game.birdless) d->ammo=clamp(d->ammo+.01,0,20);
    if (!changed_obstacle(before,after)) return;
    switch (before->sprite) {
        case 7: case 8: d->energy=clamp(d->energy+20,0,100); break; /* Native obstacle types 8/9. */
        case OB_CHROME: case OB_TREE: d->energy=clamp(d->energy+50,0,100); break;
        case OB_SPIKES: d->energy=100; break;
    }
}

static void update_runner(World *w, Dino *d, unsigned actions) {
    move_runner(d,actions);
    if (obstacle_collision(w,d)) { d->alive=0; w->alive--; return; }
    collect_food(w,d);
}

static void turn_gun(Dino *d,double amount,int sign) {
    if (!(amount>0) || d->ammo<1) return;
    amount=clamp(amount,0,1000); /* Also accept legacy models with unbounded outputs safely. */
    d->angle+=sign*amount*(PI/15);
    d->angle-=(int)(d->angle/(2*PI))*(2*PI);
    d->ammo=clamp(d->ammo-.167*amount,0,20);
}

static void update_dino(World *w,Dino *d,unsigned actions,const double aim[3]) {
    if (d->cooldown) d->cooldown--;
    if (aim) {
        /* Output 0 is "Girar Cima" and output 1 "Girar Baixo"; a positive
           model angle aims up, as in the native HUD. */
        turn_gun(d,aim[0],1);
        turn_gun(d,aim[1],-1);
        if (aim[2]>.5 && !d->cooldown && d->ammo>=5) {
            for (unsigned i=0;i<SHOT_SLOTS;i++) if (!d->shots[i].active) {
                double width,height;
                runner_pose(d,w->tick,&width,&height);
                double speed=(20+60*(clamp(aim[2],.5,1000)-.5))*MOVEMENT_SCALE;
                d->shots[i]=(Shot){d->x+width*.5,d->y+height*.5,
                    speed*dino_cos(d->angle),-speed*dino_sin(d->angle),1};
                d->ammo-=5;
                d->cooldown=10;
                break;
            }
        }
    }
    move_runner(d,actions);

    Bird *b = &d->bird;
    double width,height;
    runner_pose(d,w->tick,&width,&height);
    if (obstacle_collision(w,d)) d->alive=0;
    if (b->active && overlap(d->x+.1*width,d->x+width,b->x+2*MOVEMENT_SCALE,b->x+BIRD_W-4*MOVEMENT_SCALE) &&
        overlap(d->y+.1*height,d->y+height,b->y+4*MOVEMENT_SCALE,b->y+BIRD_H-4*MOVEMENT_SCALE)) d->alive=0;

    for (unsigned i=0;i<SHOT_SLOTS;i++) {
        Shot *p = &d->shots[i];
        if (!p->active) continue;
        p->x += p->vx; p->y += p->vy;
        if (p->x<0 || p->x>W || p->y<-25*MOVEMENT_SCALE || p->y>(1080-25)*MOVEMENT_SCALE) {
            p->active=0;
            continue;
        }
        double size=3*MOVEMENT_SCALE;
        if (bird_in_view(b) && overlap(p->x,p->x+size,b->x+.2*BIRD_W,b->x+BIRD_W) &&
            overlap(p->y,p->y+size,b->y+.2*BIRD_H,b->y+BIRD_H)) {
            double dx=b->x-d->x,dy=b->y-d->y;
            d->score+=1+__builtin_sqrt(dx*dx+dy*dy)/W;
            d->ammo=20;
            d->hits++;
            spawn_bird(w,b);
        }
    }

    if (!d->alive) w->alive--;
    else collect_food(w,d);
}

static int better_live_dino(const Dino *a,const Dino *b) {
    return a->score>b->score || (a->score==b->score && !game.birdless && a->x>b->x);
}

static void step_world(World *w, int test, unsigned input) {
    Obstacle previous[OBSTACLES];
    for (unsigned i=0;i<OBSTACLES;i++) previous[i]=w->obstacles[i];
    double previous_distance=w->distance;
    w->tick++;
    for (unsigned i=0;i<OBSTACLES;i++) {
        Obstacle *o=&w->obstacles[i];
        if (o->drift && o->x-w->distance<W) {
            double x=o->x+o->drift*MOVEMENT_SCALE;
            /* Don't let moving obstacles overtake a stationary obstacle. */
            double limit=i?w->obstacles[i-1].x+w->obstacles[i-1].w:x;
            o->x=x<limit?limit:x;
        }
    }
    double scroll=w->speed*MOVEMENT_SCALE;
    w->distance+=scroll;
    if (w->obstacles[0].x < w->distance-1500*MOVEMENT_SCALE) {
        for (unsigned i = 1; i < OBSTACLES; i++) w->obstacles[i-1] = w->obstacles[i];
        const Obstacle *last = &w->obstacles[OBSTACLES-2];
        double x=next_obstacle_x(w,last,0);
        w->obstacles[OBSTACLES-1] = make_obstacle(&w->layout, x, w->serial++);
    }
    w->coin_x+=scroll; /* Screen-relative in both originals. */
    unsigned lead=w->count,winner=w->count;
    for (unsigned i = 0; i < w->count; i++) {
        Dino *d = &w->dinos[i];
        const Obstacle *before=d->alive?nearest_obstacle(previous,previous_distance+d->x):0;
        if (d->alive && game.birdless) {
            unsigned actions = test && !i ? input : dodge_actions(w,d,test,i,0);
            update_runner(w,d,actions);
        } else if (d->alive && movement_only(test)) {
            unsigned actions = test && !i ? input & 127u : dodge_actions(w,d,test,i,0);
            update_dino(w,d,actions,0);
        } else if (d->alive) {
            unsigned actions;
            double aim[3];
            if (test && i == 0) {
                actions = input;
                aim[0]=(actions&AIM_UP)?.5:0;
                aim[1]=(actions&AIM_DOWN)?.5:0;
                aim[2]=(actions&SHOOT)?1:0;
            } else {
                actions = dodge_actions(w, d, test, i, 0);
                aim_outputs(w,d,test,i,aim,0);
            }
            update_bird(d);
            update_dino(w,d,actions,aim);
        }
        if (d->alive) {
            double x=w->distance+d->x;
            const Obstacle *after=nearest_obstacle(w->obstacles,x);
            regenerate(d,before,after);
            if (movement_only(test)) {
                double edge=after?after->x+after->w:w->distance+W;
                double bonus=changed_obstacle(before,after)?(d->energy>50?2:1):0;
                d->score+=bonus+.001*(1-(edge-x+absd(w->coin_x-x))/W);
            }
        }
        if (winner==w->count || d->score>w->dinos[winner].score) winner=i;
        if (d->alive && (lead==w->count || better_live_dino(d,&w->dinos[lead]))) lead=i;
    }
    if (!(w->tick%1001)) reset_food(w);
    if (winner<w->count) w->winner=winner;
    w->lead=lead<w->count?lead:w->winner;
    double next=w->speed+w->speed_step;
    if (next>10 || next<5) w->speed_step=-w->speed_step;
    w->speed=clamp(next,5,10);
}

enum { MODEL_AIM=1, MODEL_PAIR=2, MODEL_MOVE=3 };

static unsigned record_type(void) {
    return movement_only(0) ? MODEL_MOVE : game.joint || game.firing_custom ? MODEL_PAIR : MODEL_AIM;
}

static unsigned write_record(unsigned char *out, const double *genes) {
    if (game.birdless) return (unsigned)write_model(out, MOVEMENT_SHAPE, genes);
    if (game.move_only) return (unsigned)write_model(out,game.dodge_shape,genes);
    if (game.joint) return (unsigned)write_pair(out, TRAIN_SHAPE, genes, game.dodge_shape, genes+TRAIN_WEIGHTS);
    if (game.firing_custom) return (unsigned)write_pair(out,TRAIN_SHAPE,genes,game.firing_dodge_shape,game.firing_dodge);
    return (unsigned)write_model(out, TRAIN_SHAPE, genes);
}

static void update_record(void) {
    double score=game.train.dinos[game.train.winner].score;
    if (!(score>=0 && score<4294967295.) || !game.train.tick) return;
    if (game.record_ready && score<game.record_score) return;
    unsigned best=(unsigned)score;
    double fraction=score-best;
    best+=fraction>.5 || (fraction==.5 && (best&1)); /* Native %.0f record name. */
    game.record = best;
    game.record_ready=1;
    game.record_score=score;
    unsigned weights = genome_size();
    for (unsigned i = 0; i < weights; i++) game.best[i] = game.genes[game.train.winner][i];
    /* Separate buffer: a browser callback must not invalidate an incoming model. */
    unsigned char saved[RECORD_BYTES];
    unsigned size = write_record(saved, game.best);
    store_record(best, saved, size, record_type());
}

static void breed(unsigned i,unsigned parents,unsigned weights) {
    unsigned p=game.rank[i%parents];
    double sigma=game.deviations[p];
    game.next_dev[i]=sigma;
    int mutate=game.birdless || i!=0;
    for (unsigned j=0;j<weights;j++) {
        double v=game.genes[p][j];
        if (mutate && game.mutation>0) {
            /* The extra movement controller in the bird edition is a web extension. */
            int seeded_move=!game.birdless && (game.move_only || j>=TRAIN_WEIGHTS);
            double scale=seeded_move?1+.12*absd(game.dodge[j-(game.move_only?0:TRAIN_WEIGHTS)]):1;
            v+=game.mutation*sigma*scale*normal(&game.train.layout);
        }
        game.next[i][j]=v;
    }
    if (!game.birdless && !game.move_only && mutate && game.mutation>0)
        game.next[i][layout_roll(&game.train.layout,TRAIN_WEIGHTS)]=0;
}

static void next_generation(void) {
    const World *w = &game.train;
    unsigned parents = game.cfg_parents < w->count ? game.cfg_parents : w->count;
    if (!parents) return;
    unsigned weights = genome_size();
    double mean = 0, diversity = 0, center[GENOME_SIZE] = {0};
    for (unsigned i = 0; i < w->count; i++) {
        mean += w->dinos[i].score;
        for (unsigned j = 0; j < weights; j++) center[j] += game.genes[i][j] / w->count;
        unsigned pos = i;
        while (pos>0 && w->dinos[i].score>w->dinos[game.rank[pos-1]].score) {
            game.rank[pos] = game.rank[pos-1]; pos--;
        }
        game.rank[pos] = i;
    }
    /* The native trainer saves after comparing the evaluation average, not
       during either individual round. */
    game.train.winner=game.rank[0];
    update_record();
    for (unsigned i = 0; i < w->count; i++) {
        double v = 0;
        for (unsigned j = 0; j < weights; j++) {
            double delta = (center[j] - game.genes[i][j]) / (!game.birdless && (game.move_only || j >= TRAIN_WEIGHTS) ? 100 : 1);
            v += delta*delta;
        }
        diversity += __builtin_sqrt(v / weights);
    }
    unsigned at = (game.history_start + game.history_count) % HISTORY_CAP;
    game.history[at] = (Sample){w->dinos[game.rank[0]].score, mean/w->count, diversity/w->count};
    if (game.history_count == HISTORY_CAP) game.history_start = (game.history_start+1) % HISTORY_CAP;
    else game.history_count++;

    game.population = game.cfg_population;
    game.parents = game.cfg_parents;
    game.limit = game.cfg_limit;
    game.mutation = game.cfg_mutation;
    for (unsigned i=0;i<game.population;i++) breed(i,parents,weights);
    for (unsigned i = 0; i < game.population; i++) {
        game.deviations[i] = game.next_dev[i];
        for (unsigned j = 0; j < weights; j++) game.genes[i][j] = game.next[i][j];
    }
    game.generation++;
    game.round = 1;
    prepare_world(&game.train,game.population,0);
}

API unsigned char *model_buffer(void) { return transfer; }
API unsigned model_capacity(void) { return MODEL_BYTES; }

/* 0: validate a test policy without selecting it. 1: fixed dodge policy
   (bird edition only). 2: select a policy for the AI in test mode. */
API int model_load(unsigned len, unsigned role) {
    Shape s, move;
    static double weights[MAX_WEIGHTS], moving[MAX_WEIGHTS];
    if (!game.generation || len > MODEL_BYTES || role > 2) return 0;
    if (game.birdless && role == 1) return 0;
    int pair = !game.birdless && role != 1 && read_pair(transfer, len, &s, weights, &move, moving);
    if (!pair && !read_model(transfer, len, &s, weights)) return 0;
    int movement = s.inputs==10 && s.outputs==7;
    int aiming = s.inputs==4 && s.outputs==3;
    if (game.birdless || role==1) { if (!movement || pair) return 0; }
    else if ((!movement && !aiming) || (pair && !aiming)) return 0;
    if (pair && (move.inputs != 10 || move.outputs != 7)) return 0;
    if (role == 1 && weight_count(s) > DODGE_WEIGHTS) return 0;
    if (role) {
        double *dest = role == 1 ? game.dodge : game.model;
        for (unsigned i = 0; i < weight_count(s); i++) dest[i] = weights[i];
        if (role == 1) {
            game.dodge_shape = s; game.ready = 1;
            seed_movement();
        } else {
            game.model_shape = s; game.model_ready = 1; game.test_joint = pair; game.test_move_only = movement;
            if (pair) {
                game.test_dodge_shape = move;
                for (unsigned i = 0; i < weight_count(move); i++) game.test_dodge[i] = moving[i];
            }
        }
    }
    return game.birdless || role==1 ? MODEL_AIM : pair ? MODEL_PAIR : movement ? MODEL_MOVE : MODEL_AIM;
}

API unsigned export_best(void) {
    return game.record_ready ? write_record(transfer, game.best) : 0;
}

API unsigned export_leader(void) {
    return game.ready ? write_record(transfer, game.genes[game.train.lead]) : 0;
}

API void configure(unsigned pop, unsigned parents, double mutation, unsigned seconds) {
    game.cfg_population = (unsigned)clamp(pop, 2, game.birdless ? MAX_POP : 500);
    game.cfg_parents = (unsigned)clamp(parents, 1, game.cfg_population);
    game.cfg_limit = (unsigned)clamp(seconds, 10, 240)*60;
    game.cfg_mutation = __builtin_isfinite(mutation) ? clamp(mutation, 0, 3) : 1;
}

API void reset_training(unsigned seed) {
    seed_world(&game.train,seed);
    game.generation = 1;
    game.round = 1;
    game.elapsed = 0;
    game.tick_credit = 0;
    game.record = game.history_count = game.history_start = 0;
    game.record_ready=0;
    game.population = game.cfg_population;
    game.parents = game.cfg_parents;
    game.limit = game.cfg_limit;
    game.mutation = game.cfg_mutation;
    game.graph_offset = 0;
    for (unsigned i = 0; i < game.population; i++) {
        game.deviations[i] = 1;
        unsigned n = game.birdless ? DODGE_WEIGHTS : TRAIN_WEIGHTS;
        for (unsigned j=0;j<n;j++) game.genes[i][j]=((int)layout_roll(&game.train.layout,20001)-10000)/10000.;
    }
    seed_movement();
    prepare_world(&game.train,game.population,0);
}

API void set_training(unsigned goal, unsigned seed) {
    game.joint = !game.birdless && goal == 1;
    game.move_only = !game.birdless && goal == 2;
    game.firing_custom = 0;
    reset_training(seed);
}

/* Atomically select a movement model and start a new aim-only population.
   No genome contains movement weights in this mode: they cannot mutate. */
API int start_firing(unsigned len, unsigned seed) {
    if (game.birdless || !game.ready || len>MODEL_BYTES) return 0;
    if (!len) { set_training(0,seed); return 1; }
    Shape shape;
    static double weights[MAX_WEIGHTS];
    if (!read_model(transfer,len,&shape,weights) || shape.inputs!=10 || shape.outputs!=7) return 0;
    game.firing_dodge_shape=shape;
    unsigned count=(unsigned)weight_count(shape);
    for (unsigned j=0;j<count;j++) game.firing_dodge[j]=weights[j];
    game.firing_custom=1;
    game.joint=game.move_only=0;
    reset_training(seed);
    return 1;
}

API void init(unsigned seed) {
    memset(&game,0,sizeof game);
    game.speed = game.drawing = game.info = 1;
    game.zoom = 1;
    prepare_normal();
    configure(500,50,1,90);
    set_training(0, seed);
}

API void init_birdless(unsigned seed) {
    memset(&game,0,sizeof game);
    game.birdless = game.ready = game.speed = game.drawing = game.info = 1;
    prepare_normal();
    game.dodge_shape = MOVEMENT_SHAPE;
    game.zoom = 1;
    configure(1000, 100, 1, 90);
    reset_training(seed);
}

API int mode(unsigned testing) {
    if (testing && (!game.ready || !game.model_ready)) return 0;
    game.testing = testing != 0;
    game.tick_credit = 0;
    game.graph_offset = 0;
    if (game.testing) {
        game.test_rounds = game.test_wait = 0;
        game.test_sum = game.test_sum_sq = game.test_min = game.test_max = 0;
        reset_world(&game.test, 2, 0x8452da3fu, 1);
    }
    return 1;
}

static void restart_test(void) {
    game.test_wait = 0;
    reset_world(&game.test, 2, 0x8452da3fu ^ (game.test_rounds*0x2a5ad061u), 1);
}

static void end_test(void) {
    double fitness=game.test.dinos[1].score;
    if (!game.test_rounds || fitness < game.test_min) game.test_min = fitness;
    if (fitness > game.test_max) game.test_max = fitness;
    game.test_sum += fitness;
    game.test_sum_sq += fitness*fitness;
    game.test_history[game.test_rounds%HISTORY_CAP] = fitness;
    game.test_rounds++;
    game.test_wait = 55;
}

/* UI command IDs shared with the browser adapter (see web/main.js). */
API void command(unsigned cmd) {
    switch (cmd) {
        case 0: game.camera_x = game.camera_y = 0; game.zoom = 1; break;
        case 1: game.drawing ^= 1; break;
        case 2: game.overlay = game.overlay == 1 ? 0 : 1; break;
        case 3: game.moving_avg ^= 1; break;
        case 4: game.overlay = game.overlay == 2 ? 0 : 2; break;
        case 5: game.overlay = game.overlay == 3 ? 0 : 3; game.graph_offset = 0; break;
        case 6: if (game.speed < 64) game.speed *= 2; break;
        case 7: if (game.speed > .015625) game.speed /= 2; break;
        case 8: game.info ^= 1; break;
        case 9: game.paused ^= 1; break;
        case 10: if (game.testing) restart_test(); break;
        case 11: game.overlay = game.overlay == 4 ? 0 : 4; break;
    }
}

/* Pointer position in logical canvas units. The native picks the network
   neuron under the cursor and draws only that neuron's connections. */
API void pointer(unsigned inside, double x, double y) {
    game.pointer_inside = inside && __builtin_isfinite(x) && __builtin_isfinite(y);
    game.pointer_x = x;
    game.pointer_y = y;
}

API void graph_scroll(double delta) {
    if (game.overlay != 3 || !__builtin_isfinite(delta)) return;
    unsigned count = game.testing ? game.test_rounds + (game.birdless && !game.test_wait) : game.history_count;
    unsigned width = game.birdless ? MOVEMENT_GRAPH_WIDTH : BIRDS_GRAPH_WIDTH;
    if (count > HISTORY_CAP) count = HISTORY_CAP;
    double max_offset = count > width ? count-width : 0;
    game.graph_offset = clamp(game.graph_offset - delta, 0, max_offset);
}

API void frame(double ms, unsigned actions, unsigned pan) {
    if (!game.ready || !__builtin_isfinite(ms) || ms < 0) return;
    if (ms > 0) game.fps += .06*(1000/ms-game.fps);
    if (!game.testing || game.birdless) {
        game.camera_x = clamp(game.camera_x + .4*ms*(((pan&PAN_RIGHT)!=0)-((pan&PAN_LEFT)!=0)), -1100, 1100);
        game.camera_y = clamp(game.camera_y + .4*ms*(((pan&PAN_DOWN)!=0)-((pan&PAN_UP)!=0)), -500, 500);
        game.zoom = clamp(game.zoom + .0015*ms*(((pan&ZOOM_IN)!=0)-((pan&ZOOM_OUT)!=0)), .45, 3);
    }
    if (game.paused) return;
    if (!game.testing) game.elapsed += clamp(ms,0,50);
    double speed = game.speed;
    if (!game.testing && !game.drawing && speed < 8) speed = 8;
    game.tick_credit += clamp(ms, 0, 50)*.06*speed;
    /* Bound work per paint and do not accumulate an unbounded backlog. */
    unsigned steps = (unsigned)clamp(game.tick_credit, 0, 96);
    game.tick_credit -= (unsigned)game.tick_credit;
    if (ms > 0) game.mps += .06*(steps*1000/ms-game.mps);
    for (unsigned s = 0; s < steps; s++) {
        if (game.testing) {
            if (game.test_wait) {
                if (!--game.test_wait) restart_test();
            } else {
                step_world(&game.test,1,actions);
                if (!game.test.alive) end_test();
            }
        } else {
            step_world(&game.train, 0, 0);
            if (!game.train.alive || game.train.tick >= game.limit) {
                if (game.birdless) {
                    next_generation(); /* Native movement HUD reports Partida (1/1). */
                } else if (game.round == 1) {
                    for (unsigned i=0;i<game.train.count;i++) game.first_round[i]=game.train.dinos[i].score;
                    game.round=2;
                    prepare_world(&game.train,game.population,0);
                } else {
                    for (unsigned i=0;i<game.train.count;i++)
                        game.train.dinos[i].score=(game.train.dinos[i].score+game.first_round[i])*.5;
                    next_generation();
                }
            }
        }
    }
}

API void paint(void) { draw_game(); draw_aux(); }

API double status(unsigned i) {
    const World *w = game.testing ? &game.test : &game.train;
    switch (i) {
        case 0: return game.generation;
        case 1: return w->alive;
        case 2: return w->count;
        case 3: return w->dinos[w->winner].score;
        case 4: return game.record;
        case 36: return game.record_ready;
        case 5: return game.testing;
        case 6: return game.speed;
        case 7: return game.drawing;
        case 8: return game.info;
        case 9: return game.overlay;
        case 10: return game.moving_avg;
        case 11: return game.paused;
        case 12: return w->tick;
        case 13: return game.history_count;
        case 14: return w->dinos[0].score;
        case 15: return w->dinos[1].score;
        case 16: return w->dinos[0].alive;
        case 17: return w->dinos[1].alive;
        case 18: return game.cfg_population != game.population || game.cfg_parents != game.parents ||
            game.cfg_mutation != game.mutation || game.cfg_limit != game.limit;
        case 19: return game.zoom;
        case 20: return game.camera_x;
        case 21: return game.camera_y;
        case 22: return w->dinos[0].y;
        case 23: return w->dinos[0].energy;
        case 24: return w->dinos[0].angle;
        case 25: return w->dinos[0].x;
        case 26: return game.history_count ? game.history[(game.history_start+game.history_count-1)%HISTORY_CAP].diversity : 0;
        case 27: return game.move_only ? 2 : game.joint;
        case 28: return game.test_joint;
        case 29: return game.round;
        case 30: return game.test_rounds;
        case 31: return game.birdless;
        case 32: return record_type();
        case 33: return movement_only(game.testing);
        case 34: return w->speed;
        case 35: return w->distance/MOVEMENT_SCALE;
        default: return 0;
    }
}
