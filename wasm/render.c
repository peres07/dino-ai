#include "game.h"

#define HOST(name) __attribute__((import_module("browser"),import_name(#name)))
extern HOST(surface) void surface(unsigned);
extern HOST(rect) void rect(double,double,double,double,unsigned,double);
extern HOST(line) void line(double,double,double,double,unsigned,double,double);
extern HOST(sprite) void sprite(unsigned,double,double,double,double,double,double,unsigned);
extern HOST(text) void host_text(const char *,unsigned,double,double,double,unsigned);
extern HOST(camera) void camera(double,double,double);
extern HOST(mini) void mini(double,double,double);

enum { DINO=0, CACTUS=12, BIRD=21, GROUND_TILE=25, HILLS=31, CLOUD=33, SUN=34,
    MOON, TREE, CAVE, SPIKES, GUN, BULLET, COIN, SMOKE=53, TREE_VARIANT=62,
    TREE_BACK=63, TREE_BACK2=64, CAVE_VARIANT=65, NEURON=68, NEURON_LARGE, GLOW, INPUT, CHROME };
static const unsigned sand=0xfddc8f, sky=0xb2e4fd, orange=0xff8700;
static const unsigned colors[]={0xffbd00,0x00e7f4,0xff00fa,0x00eb14,0xff3300,0x1754ff,0xbe52ff,0x808080,0xff8000};

static unsigned dino_color(unsigned i) {
    static const unsigned movement_colors[]={0x00ff00,0xff00ff,0x0000ff,0xffff00,0x00ffff,0xff8000,0x8f00ff,0xff0000,0x808080};
    if(game.birdless) return game.testing?(i?0x00ff00:0xff00ff):movement_colors[i%9];
    return game.testing?(i?0xff8000:0x00e7f4):colors[i%9];
}

static void text(const char *s,double x,double y,double size,unsigned rgb) {
    unsigned len=0; while(s[len]) len++;
    host_text(s,len,x,y,size,rgb);
}

static void decimal(double v,unsigned digits,double x,double y,double size,unsigned color) {
    char s[20]; unsigned n=0;
    if(!__builtin_isfinite(v) || absd(v)>10000000) {text("–",x,y,size,color);return;}
    unsigned scale=1;
    for(unsigned i=0;i<digits;i++) scale*=10;
    unsigned value=(unsigned)(absd(v)*scale+.5);
    if(digits) {
        for(unsigned i=0;i<digits;i++) {s[n++]=(char)('0'+value%10); value/=10;}
        s[n++]='.';
    }
    do {s[n++]=(char)('0'+value%10); value/=10;} while(value);
    if(v<0) s[n++]='-';
    for(unsigned i=0;i<n/2;i++) {char c=s[i];s[i]=s[n-1-i];s[n-1-i]=c;}
    host_text(s,n,x,y,size,color);
}

/* Arial digits are tabular; anchor the value column to the right like SDL. */
static void value_at(double v,unsigned digits,double old_x,double right,double y,double size) {
    double x=old_x;
    if(game.birdless) {
        double scale=digits==2?100:digits==1?10:1;
        double whole=(double)(unsigned)(clamp(absd(v),0,10000000)*scale+.5)/scale;
        unsigned n=1; while(whole>=10) {whole/=10;n++;}
        x=right-size*(.556*(n+digits)+.278*!!digits+.333*(v<0));
    }
    decimal(v,digits,x,y,size,0);
}

static double wrap(double x,double m) { return x-(int)(x/m)*m; }

/* Native HUD uses "%dh:%dm:%ds" with no padding. */
static void time_value(unsigned seconds,double x,double y,double size) {
    char s[24]; unsigned n=0;
    const unsigned part[3]={seconds/3600,seconds/60%60,seconds%60};
    const char suffix[3]={'h','m','s'};
    for(unsigned i=0;i<3;i++) {
        unsigned v=part[i];
        char digits[6]; unsigned k=0;
        do {digits[k++]=(char)('0'+v%10); v/=10;} while(v);
        while(k) s[n++]=digits[--k];
        s[n++]=suffix[i];
        if(i<2) s[n++]=':';
    }
    host_text(s,n,x,y,size,0);
}

/* Native TA field is a single "%.3fx" string. */
static void speed_value(double v,double x,double y,double size) {
    char s[20]; unsigned n=0;
    unsigned value=(unsigned)(clamp(v,0,1000000)*1000+.5);
    for(unsigned i=0;i<3;i++) {s[n++]=(char)('0'+value%10); value/=10;}
    s[n++]='.';
    do {s[n++]=(char)('0'+value%10); value/=10;} while(value);
    for(unsigned i=0;i<n/2;i++) {char c=s[i];s[i]=s[n-1-i];s[n-1-i]=c;}
    s[n++]='x';
    host_text(s,n,x,y,size,0);
}

static int visible(double x,double y,double w,double h) {
    int fixed=game.testing&&!game.birdless;
    double zoom=fixed?1:game.zoom, dx=fixed?0:game.camera_x, dy=fixed?0:game.camera_y;
    double left=W*.5*(1-1/zoom)+dx, top=VIEW_H*.5*(1-1/zoom)+dy;
    return x+w>=left && x<=left+W/zoom && y+h>=top && y<=top+VIEW_H/zoom;
}

static void draw_dino(const Dino *d,unsigned tick,unsigned tint) {
    double h,w;
    unsigned pose=runner_pose(d,tick,&w,&h);
    double y=GROUND-d->y;
    if(!visible(d->x,y-h,w,h)) return;
    if(!game.birdless && d->smoke) sprite(SMOKE+((15-d->smoke)*8)/14,d->x-24,y-20,42,31,0,.65,0);
    sprite(DINO+pose,d->x,y-h,w,h,0,1,tint);
    if(movement_only(game.testing)) return;
    double gy=y-h*.5, gx=d->x+w*.5, c=dino_cos(d->angle), s=dino_sin(d->angle);
    /* Canvas rotation is clockwise; the model's positive angle aims up. */
    sprite(GUN,gx+9*c-15,gy-9*s-6.5,30,13,-d->angle,1,0);
}

/* Approximate atan2 in the math convention (y up). */
static double velocity_angle(double vx,double vy) {
    double x=vx,y=-vy, ax=absd(x),ay=absd(y);
    if(ax+ay<.001) return 0;
    double q=ax>ay?ay/ax:ax/ay, a=q*(PI/4+.273*(1-q));
    if(ay>ax) a=PI/2-a;
    if(x<0) a=PI-a;
    return y<0?-a:a;
}

static double shot_angle(const Shot *s) {
    /* The projectile must not follow the current gun angle. */
    return velocity_angle(s->vx,s->vy);
}

/* passaro::desenhar (0x45038b-0x450456): the angle at +0x18 picks the pair and
   the rotation. Left flight uses passaro3/4 rotated by angle-180; right flight
   uses passaro1/2 rotated by angle. Frame interval is 10 in the native group. */
static void draw_bird(const Bird *b,unsigned tick) {
    if(!b->active) return;
    double y=GROUND-b->y-BIRD_H;
    if(!visible(b->x,y,BIRD_W,BIRD_H)) return;
    int left=b->vx<0;
    double screen=-velocity_angle(b->vx,b->vy);
    double rotation=left?screen-PI:screen;
    sprite(BIRD+(left?2:0)+(tick/10)%2,b->x,y,BIRD_W,BIRD_H,rotation,1,0);
}

static void projectiles(const World *w) {
    for(unsigned i=0;i<w->count;i++) {
        const Dino *d=&w->dinos[i];
        if(!d->alive && i!=w->lead) continue;
        unsigned tint=dino_color(i);
        for(unsigned j=0;j<SHOT_SLOTS;j++) {
            const Shot *p=&d->shots[j];
            double y=GROUND-p->y;
            if(!p->active || !visible(p->x-14,y-14,28,28)) continue;
            if(game.info) line(p->x,y,p->x+68*p->vx,y-68*p->vy,0x44525a,.65,.22);
            sprite(BULLET,p->x-14,y-3.5,28,7,-shot_angle(p),1,tint);
        }
    }
}

/* Large sprites use the obstacle's position, but not its collision rectangle.
   The foreground layers are drawn over the runner after its sprite. */
static void draw_obstacle(const World *w,const Obstacle *o,int front) {
    const double k=MOVEMENT_SCALE;
    double x=o->x-w->distance,y=GROUND-o->y-o->h,width=o->w,height=o->h;
    if(o->sprite==OB_CAVE || o->sprite==OB_TREE) {
        int tree=o->sprite==OB_TREE;
        x-=(tree?795:170)*k;
        width=(tree?1800:1000)*k;
        height=(tree?1604:842)*k;
        /* Image bottom on the ground line: the roots/cave base rest on the sand. */
        y=GROUND-height;
        if(!visible(x,y,width,height)) return;
        /* arvore3/arvore4 are allocated at manager+0x7c/+0x80 and drawn in
           pass 1, behind the runner (0x4520a0). arvore2 lives at +0x78 and
           is the only tree layer drawn in pass 2 (0x451f92). */
        if(!front) {
            sprite(tree?TREE:CAVE,x,y,width,height,0,1,0);
            if(tree && !game.birdless) {
                sprite(TREE_BACK,x,y,width,height,0,1,0);
                sprite(TREE_BACK2,x,y,width,height,0,1,0);
            }
        }
        else if(tree) sprite(TREE_VARIANT,x,y,width,height,0,1,0);
        else for(unsigned i=0;i<3;i++) sprite(CAVE_VARIANT+i,x,y,width,height,0,1,0);
        return;
    }
    if(front || !visible(x,y,width,height)) return;
    unsigned id,tint=0;
    switch(o->sprite) {
        case OB_BIRD: id=BIRD+(w->tick/10)%2; break;
        case OB_CHROME: id=CHROME; break;
        case OB_SPIKES: id=SPIKES; break;
        default: id=CACTUS+o->sprite; tint=game.birdless?0x138600:0x549723; break;
    }
    /* Native adds 3 degrees to the obstacle angle before drawing (0x451dc4). */
    sprite(id,x,y,width,height,(o->angle+3)*(PI/180),1,tint);
}

void draw_game(void) {
    surface(0);
    if(!game.drawing) {rect(0,0,W,VIEW_H,sand,1);return;}
    const World *w=game.testing?&game.test:&game.train;
    const Dino *lead=&w->dinos[w->lead];
    int night=((w->tick/1750)%2)!=0;
    rect(0,0,W,VIEW_H,night?0x192440:sky,1);
    if(night) for(unsigned i=0;i<50;i++) rect((i*1537+19)%W,26+(i*2311)%315,2,2,0xf3f2dd,.75);
    for(unsigned i=0;i<47;i++) {
        double x=wrap((i*727)%1255+1255-wrap(w->distance*.16,1255),1255)-38;
        double size=11+(i*7)%22;
        sprite(CLOUD,x,15+(i*131)%350,size,size*.28,0,1,0);
    }
    if(w->tick>480) sprite(night?MOON:SUN,965,44,63,63,0,.9,0);
    /* Both editions show three ridge bands (fundo0/fundo1 tinted). */
    /* fundo0/fundo1 are 1200x90 native: 750x56.25 logical, tiled at natural
       size with the terrain strip touching the ground line. */
    static const unsigned ridge[3]={0x513a29,0x79573d,0xb5825b};
    for(unsigned layer=0;layer<3;layer++) {
        unsigned tint=night?0x56494a:ridge[layer];
        double height=90*MOVEMENT_SCALE;
        for(int i=0;i<4;i++) {
            double x=750*i-wrap(w->distance*(.15+.08*layer)+layer*237,750);
            if(x>W || x+750<0) continue;
            sprite(HILLS+((i+layer)&1),x,GROUND-height,750,height,0,1,tint);
        }
    }
    int fixed=game.testing&&!game.birdless;
    camera(fixed?0:game.camera_x,fixed?0:game.camera_y,fixed?1:game.zoom);
    rect(-1600,GROUND,W+3200,1200,night?0x504354:sand,1);
    const double tile_w=60*MOVEMENT_SCALE, tile_h=12*MOVEMENT_SCALE;
    double offset=wrap(w->distance,tile_w*6);
    int shift=(int)(w->distance/tile_w);
    for(int i=-44;i<76;i++) {
        double x=i*tile_w-offset;
        if(!visible(x,GROUND-2,tile_w,tile_h)) continue;
        sprite(GROUND_TILE+ground_tile(i+shift),x,GROUND-2,tile_w,tile_h,0,1,0xb5825b);
    }
    for(unsigned i=0;i<OBSTACLES;i++) draw_obstacle(w,&w->obstacles[i],0);
    double cx=w->coin_x-w->distance;
    double coin=40*MOVEMENT_SCALE;
    double bob=5*MOVEMENT_SCALE*dino_sin(w->tick*(2*PI/72));
    double coin_y=GROUND-(w->coin_y+bob);
    if(!lead->coin_taken) sprite(COIN+(w->tick/4)%12,cx,coin_y-coin,coin,coin,0,1,0);
    for(unsigned i=0;i<w->count;i++) {
        const Dino *d=&w->dinos[i];
        if(i!=w->lead && (d->alive||game.testing)) draw_dino(d,w->tick,dino_color(i));
    }
    for(unsigned i=0;!movement_only(game.testing) && i<w->count;i++) {
        const Dino *d=&w->dinos[i];
        if((!game.testing && i!=w->lead) || !d->alive) continue;
        draw_bird(&d->bird,w->tick);
    }
    if(!movement_only(game.testing)) projectiles(w);
    draw_dino(lead,w->tick,dino_color(w->lead));
    for(unsigned i=0;i<OBSTACLES;i++) draw_obstacle(w,&w->obstacles[i],1);
    camera(0,0,1);
    if(game.testing&&!w->alive&&!game.birdless) {
        rect(0,0,W,VIEW_H,0,.45);
        text("Fim da partida",530,300,23,0xffffff);
        text("T para reiniciar · Enter para voltar",485,332,14,0xffffff);
    }
}

static Sample sample(unsigned i) { return game.history[(game.history_start+i)%HISTORY_CAP]; }

static double average(unsigned i) {
    if(!game.moving_avg) return sample(i).best;
    double sum=0;
    unsigned start=i>9?i-9:0;
    for(unsigned j=start;j<=i;j++) sum+=sample(j).best;
    return sum/(i-start+1);
}

/* The movement edition plots one solid histogram. The test plot includes the
 * current round; the empty first round fills the plot even at score zero. */
static double movement_fitness(unsigned i) {
    if(!game.testing) return average(i);
    return i==game.test_rounds?game.test.dinos[1].score:game.test_history[i%HISTORY_CAP];
}

static void movement_chart(double x,double y,double w,double h,int expanded) {
    unsigned ink=expanded?0xffffff:0, fill=game.testing?dino_color(1):ink;
    unsigned end=game.testing?game.test_rounds+!game.test_wait:game.history_count;
    unsigned first=end>HISTORY_CAP?end-HISTORY_CAP:0;
    int free_size=expanded && game.overlay==3;
    double step;
    if(free_size) {
        /* Native free plot starts at display pixel 100 and grows ~3 px/round. */
        x=62.5; step=1.875;
        unsigned cap=MOVEMENT_GRAPH_WIDTH;
        unsigned offset=(unsigned)clamp(game.graph_offset,0,end-first>cap?end-first-cap:0);
        end-=offset;
        if(end-first>cap) first=end-cap;
        w=(end-first)*step;
    } else step=w/(end>first?end-first:1);
    const char *title=game.testing?"Aptidão por Partida":game.moving_avg?
        "Melhor Aptidão por Geração (Média Móvel 10)":"Melhor Aptidão por Geração (Média Móvel 1)";
    text(title,x+w*.5-(game.testing?(expanded?68:40):(expanded?158:90)),y-(expanded?21:6),expanded?15:9,ink);
    double max=0;
    for(unsigned i=first;i<end;i++) {double v=movement_fitness(i);if(v>max) max=v;}
    for(unsigned i=0;i<=10;i++) {
        double a=x+w*i/10,b=y+h*i/10;
        line(a,y,a,y+h,ink,.625,.9);
        line(x,b,x+w,b,ink,.625,.9);
        if(expanded && end>first && (i || !free_size)) {
            decimal(max*(10-i)/10,0,x-18,b+3,9,ink);
            decimal(first+(end-first)*i/10.,0,a-2,y+h+14,9,ink);
        }
    }
    unsigned stride=step>=1?1:(unsigned)(1/step);
    for(unsigned i=first;i<end;i+=stride) {
        double v=movement_fitness(i);
        for(unsigned j=i+1;j<i+stride && j<end;j++) {double t=movement_fitness(j); if(t>v) v=t;}
        double height=max>0?h*clamp(v/max,0,1):game.testing?h:0;
        unsigned n=end-i<stride?end-i:stride;
        rect(x+(i-first)*step,y+h-height,step*n+.12,height,fill,1);
    }
}

static void test_chart(double x,double y,double w,double h,int expanded) {
    unsigned ink=expanded?0xffffff:0x554224;
    text("Aptidão por Partida",x+w*.5-40,y-6,expanded?15:9,ink);
    for(unsigned i=0;i<=10;i++) {
        line(x+w*i/10,y,x+w*i/10,y+h,ink,.6,.65);
        line(x,y+h*i/10,x+w,y+h*i/10,ink,.6,.65);
    }
    unsigned end=game.test_rounds, count=end>HISTORY_CAP?HISTORY_CAP:end;
    if(!count) return;
    unsigned start=end-count;
    int fixed=expanded&&game.overlay==3;
    if(fixed) {
        end-=(unsigned)clamp(game.graph_offset,0,count>BIRDS_GRAPH_WIDTH?count-BIRDS_GRAPH_WIDTH:0);
        start=end-start>BIRDS_GRAPH_WIDTH?end-BIRDS_GRAPH_WIDTH:start;
        count=end-start;
    }
    double step=fixed?4:w/(count>10?count:10), scale=(h-2)/(game.test_max>1?game.test_max:1);
    unsigned stride=count>(unsigned)w?(count+(unsigned)w-1)/(unsigned)w:1;
    for(unsigned i=0;i<count;i+=stride) {
        double height=0;
        for(unsigned j=i;j<count&&j<i+stride;j++) {
            double v=game.test_history[(start+j)%HISTORY_CAP]*scale;
            if(v>height) height=v;
        }
        rect(x+i*step,y+h-height,clamp(step*stride,1,9),height,orange,.9);
    }
}

static void chart(double x,double y,double w,double h,int expanded) {
    if(game.birdless) {movement_chart(x,y,w,h,expanded);return;}
    if(game.testing) {test_chart(x,y,w,h,expanded);return;}
    unsigned ink=expanded?0xffffff:0, count=game.history_count;
    const char *title=game.moving_avg?"Melhor Aptidão por Geração (Média Móvel 10)":"Melhor Aptidão por Geração (Média Móvel 1)";
    text(title,x+w*.5-(expanded?155:94),y-6,expanded?15:9,ink);
    for(unsigned i=0;i<=10;i++) {
        line(x+w*i/10,y,x+w*i/10,y+h,ink,.6,.9);
        line(x,y+h*i/10,x+w,y+h*i/10,ink,.6,.9);
    }
    if(!count) return;
    unsigned end=count, start=0;
    int free_size=expanded&&game.overlay==3;
    if(free_size) {
        unsigned length=BIRDS_GRAPH_WIDTH;
        end=count-(unsigned)game.graph_offset;
        start=end>length?end-length:0;
    }
    double max=1;
    for(unsigned i=start;i<end;i++) {double f=average(i);if(f>max)max=f;}
    unsigned n=end-start, pixels=(unsigned)w;
    unsigned stride=!free_size&&n>pixels?(n+pixels-1)/pixels:1;
    double step=free_size?4:w/(n>10?n:10);
    for(unsigned i=start;i<end;i+=stride) {
        double v=0,a=0;
        for(unsigned j=i;j<i+stride&&j<end;j++) {double f=average(j);if(f>v){v=f;a=sample(j).average;}}
        double bar=clamp(step*stride,1,free_size?4:9), xpos=x+(i-start)*step;
        double top=(h-2)*v/max, bottom=(h-2)*a/max;
        rect(xpos+1,y+h-top,bar,top,orange,.88);
        rect(xpos+1,y+h-bottom,bar,bottom,0x66400f,.95);
    }
    decimal(max,0,x+4,y+12,expanded?13:9,ink);
    decimal(game.generation-count+start-1,0,x,y+h+11,9,ink);
    decimal(game.generation-count+end-2,0,x+w-22,y+h+11,9,ink);
}

static double node_y(double y,double h,unsigned n,unsigned i,unsigned level,unsigned levels,int expanded) {
    if(game.birdless) {
        unsigned col=!level?0:level==levels-1?2:1;
        /* First and last node centres measured on the 1920 x 1080 Wine surface. */
        static const double top[][3]={{828,815,834},{102,76,122}};
        static const double bottom[][3]={{992,991,985},{890,941,910}};
        return .625*(top[expanded][col]+(bottom[expanded][col]-top[expanded][col])*(n>1?(double)i/(n-1):.5));
    }
    return y+h*(i+.5)/n;
}

static unsigned shade(unsigned color,double intensity) {
    double a=clamp(intensity,0,1);
    return ((unsigned)((color>>16)*a)<<16)|((unsigned)(((color>>8)&255)*a)<<8)|(unsigned)((color&255)*a);
}

static double neuron_radius(int expanded,int dodging,unsigned l,unsigned levels) {
    if(game.birdless) return expanded?15.625:7.813;
    return expanded?(dodging?8:16):dodging&&l>0&&l<levels-1?7:7.5;
}

static void neural(double x,double y,double w,double h,int expanded,int dodging) {
    const World *world=game.testing?&game.test:&game.train;
    unsigned leader=game.testing?1:world->lead, text_color=expanded?0xffffff:0;
    Trace t;
    double out[3];
    const double *weights;
    if(dodging) {
        Shape shape;
        weights=movement_weights(game.testing,leader,&shape);
        dodge_actions(world,&world->dinos[leader],game.testing,leader,&t);
    } else {
        weights=game.testing?game.model:game.genes[leader];
        aim_outputs(world,&world->dinos[leader],game.testing,leader,out,&t);
    }
    Shape s=t.shape;
    unsigned accent=game.birdless?dino_color(leader):orange;
    unsigned levels=s.depth+2, start=0;
    double gap=w/(levels-1);
    /* Native hit test: the neuron under the pointer owns the highlight. */
    int hover_l=-1, hover_i=-1;
    if(game.pointer_inside) {
        for(unsigned l=0;l<levels;l++) {
            unsigned n=l==0?s.inputs:l==levels-1?s.outputs:s.hidden;
            double r=neuron_radius(expanded,dodging,l,levels);
            for(unsigned i=0;i<n;i++) {
                double a=x+l*gap, b=node_y(y,h,n,i,l,levels,expanded);
                double dx=game.pointer_x-a, dy=game.pointer_y-b;
                if(dx*dx+dy*dy<=r*r) {hover_l=(int)l; hover_i=(int)i;}
            }
        }
    }
    for(unsigned l=1;l<levels;l++) {
        unsigned prev=l==1?s.inputs:s.hidden,n=l==levels-1?s.outputs:s.hidden;
        for(unsigned j=0;j<n;j++) {
            double b=node_y(y,h,n,j,l,levels,expanded);
            for(unsigned i=0;i<prev;i++) {
                double a=node_y(y,h,prev,i,l-1,levels,expanded), value=weights[start++], lv=t.values[l-1][i];
                if(hover_l>=0 && !(l-1==(unsigned)hover_l && i==(unsigned)hover_i) &&
                   !(l==(unsigned)hover_l && j==(unsigned)hover_i)) continue;
                unsigned c=absd(value*lv)>.015?orange:expanded?0xe5e5e5:0x684925;
                if(game.birdless) {
                    int activated=lv>0 && t.values[l][j]>0 && value>0;
                    line(x+(l-1)*gap,a,x+l*gap,b,activated?accent:0x808080,activated?(expanded?1.25:2):.56,1);
                } else line(x+(l-1)*gap,a,x+l*gap,b,c,clamp(absd(value)/6,.35,2),expanded?.58:.72);
                if(expanded&&!dodging&&i==prev/2)
                    decimal(value,2,x+(l-.5)*gap,(a+b)*.5,8,0xffffff);
            }
            start+=s.bias;
        }
    }
    static const char * aim[]={"Girar Cima","Girar Baixo","Atirar"};
    static const char * move[]={"Pular","Abaixar","Pulo Duplo","Esquerda","Direita","Super Pular","Dash"};
    for(unsigned l=0;l<levels;l++) {
        unsigned n=l==0?s.inputs:l==levels-1?s.outputs:s.hidden;
        for(unsigned i=0;i<n;i++) {
            double a=x+l*gap,b=node_y(y,h,n,i,l,levels,expanded), v=t.values[l][i];
            int active=v>.001;
            double r=expanded?(dodging?8:16):dodging&&l>0&&l<levels-1?7:7.5;
            if(game.birdless) r=expanded?15.625:7.813;
            if(!l) {
                double side=game.birdless?2*r:expanded?(dodging?16:31):15;
                unsigned tint=game.birdless?shade(accent,v):0x9f5b05;
                sprite(INPUT,a-side/2,b-side/2,side,side,0,1,active&&tint?tint:0x000001);
                decimal(v,game.birdless?1:2,a-side/2-(game.birdless?(expanded?26:18):22),b+3,game.birdless?9:8,text_color);
            } else {
                if(active) sprite(GLOW,a-r*2.2,b-r*2.2,r*4.4,r*4.4,0,.64,accent);
                sprite(expanded?NEURON_LARGE:NEURON,a-r,b-r,2*r,2*r,0,1,active?accent:0x000001);
            }
            if(l==levels-1) text(dodging?move[i]:aim[i],a+r+5,b+3,expanded?10:9,text_color);
        }
    }
}

void draw_aux(void) {
    if(!game.ready || !game.info) return;
    if(game.overlay) {
        int network_view=game.overlay==1 || game.overlay==4;
        rect(0,0,W,VIEW_H,0,game.birdless?(network_view?.75:.50):(network_view?.76:.53));
        if(game.birdless && (game.overlay==1 || game.overlay==4)) neural(315.625,0,538.75,0,1,1);
        else if(game.overlay==1) neural(315,65,540,460,1,movement_only(game.testing));
        else if(game.overlay==4) neural(175,52,780,514,1,1);
        else chart(318,114,600,460,1);
        return;
    }
    const World *w=game.testing?&game.test:&game.train;
    unsigned leader_index=game.testing?1:w->lead;
    const Dino *leader=&w->dinos[leader_index];
    unsigned elapsed=(unsigned)(game.elapsed/1000);
    if(game.drawing) {
        mini(leader->x+(game.birdless?12:0),GROUND-leader->y,leader_index);
        if(game.birdless) {
            unsigned c=dino_color(leader_index);
            rect(1060,505,128,1,c,1); rect(1060,632,128,1,c,1);
            rect(1060,505,1,128,c,1); rect(1187,505,1,128,c,1);
        }
    }
    if(!game.testing) {
        text("Estado:",19,496,12,0); text(game.paused?"Pausado":"Treinando",167,496,12,0);
        text("Geração:",19,518,12,0); value_at(game.generation-1,0,208,221,518,12);
        text("Partida:",19,540,12,0); text(game.birdless?"(1/1)":game.round==1?"(1/2)":"(2/2)",194,540,12,0);
        text("Indivíduos Vivos:",19,562,12,0); value_at(w->alive,0,200,221,562,12);
        text("Melhor Aptidão:",19,584,12,0); value_at(game.record_ready?game.record:-1.,0,200,221,584,12);
        text("Tempo treinando:",138,634,12,0);
        time_value((unsigned)elapsed,152,656,10);
    } else {
        double mean=game.test_rounds?game.test_sum/game.test_rounds:0;
        double variance=game.test_rounds?game.test_sum_sq/game.test_rounds-mean*mean:0;
        text("Estado:",19,484,12,0); text(game.paused?"Pausado":"Testando",167,484,12,0);
        text("Partida:",19,506,12,0); value_at(game.test_rounds+!game.test_wait,0,208,221,506,12);
        text("Maior Aptidão:",19,528,12,0); value_at(game.test_max,0,208,221,528,12);
        text(game.birdless?"Media Aptidão:":"Média Aptidão:",19,550,12,0); value_at(mean,0,208,221,550,12);
        text("Menor Aptidão:",19,572,12,0); value_at(game.test_min,0,208,221,572,12);
        text("Desvio Padrão:",19,594,12,0); value_at(variance>0?__builtin_sqrt(variance):0,0,208,221,594,12);
    }
    chart(281,488,375,175,0);
    text("MPS:",19,615,12,0); decimal(game.mps,0,63,615,12,0);
    text("FPS:",19,637,12,0); decimal(game.fps,0,63,637,12,0);
    text("TA:",19,659,12,0);
    speed_value(game.speed,63,659,12);
    text("Melhor Indivíduo",690,530,15,0);
    text("Pontuação:",688,565,12,0); value_at(leader->score,game.birdless?0:2,779,812,565,12);
    text("Velocidade:",688,587,12,0); value_at(w->speed,1,779,812,587,12);
    text("Distância:",688,609,12,0); value_at(w->distance/MOVEMENT_SCALE,0,779,812,609,12);
    text(game.birdless?"Energia":"Energia:",688,631,12,0); value_at(leader->energy,1,779,812,631,12);
    if(game.birdless) neural(868.125,0,112.5,0,0,1);
    else if(movement_only(game.testing)) neural(861,484,124,163,0,1);
    else {
        neural(875,466,112,61,0,0);
        neural(875,548,112,109,0,1);
    }
}
