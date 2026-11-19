#include "network.h"

const Shape TRAIN_SHAPE = {4, 4, 3, 1, 0, 1, 1};

size_t weight_count(Shape s) {
    if (!s.inputs || s.inputs > MAX_HIDDEN || !s.outputs || s.outputs > MAX_HIDDEN ||
        (s.depth && (!s.hidden || s.hidden > MAX_HIDDEN)) || s.depth > MAX_LAYERS ||
        s.activation > 4 || s.output_activation > 4 || s.bias > 1) return 0;
    size_t n = 0, prev = s.inputs;
    for (unsigned i = 0; i <= s.depth; i++) {
        unsigned next = i == s.depth ? s.outputs : s.hidden;
        n += (prev + s.bias) * next;
        prev = next;
    }
    return n <= MAX_WEIGHTS ? n : 0;
}

static uint64_t get_le(const unsigned char *p, unsigned n) {
    uint64_t x = 0;
    for (unsigned i = 0; i < n; i++) x |= (uint64_t)p[i] << (8 * i);
    return x;
}

static void put_le(unsigned char *p, uint64_t x, unsigned n) {
    for (unsigned i = 0; i < n; i++) p[i] = (unsigned char)(x >> (8 * i));
}

int read_model(const unsigned char *p, size_t len, Shape *shape, double *weights) {
    if (len < 28) return 0;
    Shape s = {(uint32_t)get_le(p, 4), (uint32_t)get_le(p+4, 4), (uint32_t)get_le(p+8, 4),
        (uint32_t)get_le(p+12, 4), (uint32_t)get_le(p+16, 4), (uint32_t)get_le(p+20, 4),
        (uint32_t)get_le(p+24, 4)};
    size_t n = weight_count(s);
    if (!n || len != 28 + 8 * n) return 0;
    for (size_t i = 0; i < n; i++) {
        union {uint64_t bits; double value;} d = {get_le(p + 28 + i * 8, 8)};
        /* The value must stay finite through up to seven dense layers, including
           linear activations. Real legacy weights are many orders smaller. */
        if (!__builtin_isfinite(d.value) || __builtin_fabs(d.value) > 1e30) return 0;
    }
    for (size_t i = 0; i < n; i++) {
        union {uint64_t bits; double value;} d = {get_le(p + 28 + i * 8, 8)};
        weights[i] = d.value;
    }
    *shape = s;
    return 1;
}

size_t write_model(unsigned char *p, Shape s, const double *weights) {
    uint32_t fields[] = {s.inputs, s.hidden, s.outputs, s.depth, s.activation, s.output_activation, s.bias};
    size_t n = weight_count(s);
    if (!n) return 0;
    for (unsigned i = 0; i < 7; i++) put_le(p + i * 4, fields[i], 4);
    for (size_t i = 0; i < n; i++) {
        union {uint64_t bits; double value;} d = {.value = weights[i]};
        put_le(p + 28 + i * 8, d.bits, 8);
    }
    return 28 + n * 8;
}

/* DNN1 wraps two unchanged legacy networks, in aim then movement order. */
int read_pair(const unsigned char *p, size_t len, Shape *aim, double *aw, Shape *move, double *mw) {
    if (len < 12 || get_le(p, 4) != 0x314e4e44u) return 0;
    uint32_t a = get_le(p+4, 4), m = get_le(p+8, 4);
    if (a > len-12 || m != len-12-a) return 0;
    return read_model(p+12, a, aim, aw) && read_model(p+12+a, m, move, mw);
}

size_t write_pair(unsigned char *p, Shape aim, const double *aw, Shape move, const double *mw) {
    size_t a = write_model(p+12, aim, aw);
    size_t m = write_model(p+12+a, move, mw);
    if (!a || !m) return 0;
    put_le(p, 0x314e4e44u, 4);
    put_le(p+4, a, 4);
    put_le(p+8, m, 4);
    return 12+a+m;
}

/* Small, freestanding math routines: the simulation and activations stay in Wasm. */
static double exp_small(double x) {
    const double ln2 = 0.6931471805599453;
    int k = (int)(x / ln2 + (x < 0 ? -.5 : .5));
    double r = x - k * ln2, term = 1, sum = 1;
    for (int i = 1; i <= 12; i++) { term *= r / i; sum += term; }
    union {uint64_t bits; double value;} scale = {(uint64_t)(1023 + k) << 52};
    return sum * scale.value;
}

double dino_sin(double x) {
    const double pi = 3.141592653589793;
    x -= (int)(x / (2 * pi)) * (2 * pi);
    if (x > pi) x -= 2 * pi;
    if (x < -pi) x += 2 * pi;
    if (x > pi/2) x = pi - x;
    if (x < -pi/2) x = -pi - x;
    double z = x*x;
    return x * (1 + z * (-1./6 + z * (1./120 + z * (-1./5040 + z * (1./362880 +
        z * (-1./39916800 + z / 6227020800.))))));
}

double dino_cos(double x) { return dino_sin(x + 1.5707963267948966); }

static double activate(unsigned kind, double x) {
    switch (kind) {
        case 0: return x > 0 ? x : 0;
        case 1: return x > 40 ? 1 : x < -40 ? 0 : 1 / (1 + exp_small(-x));
        case 2: return x > 20 ? 1 : x < -20 ? -1 : 2 / (1 + exp_small(-2 * x)) - 1;
        case 3: return x < 0 ? x * .01 : x;
        default: return x;
    }
}

void infer(Shape s, const double *w, const double *in, double *out, Trace *trace) {
    double a[MAX_HIDDEN], b[MAX_HIDDEN];
    double *next = a;
    if (trace) {
        trace->shape = s;
        for (unsigned i = 0; i < s.inputs; i++) trace->values[0][i] = in[i];
    }
    unsigned prev = s.inputs;
    for (unsigned layer = 0; layer <= s.depth; layer++) {
        unsigned count = layer == s.depth ? s.outputs : s.hidden;
        unsigned act = layer == s.depth ? s.output_activation : s.activation;
        for (unsigned j = 0; j < count; j++) {
            double sum = 0;
            for (unsigned i = 0; i < prev; i++) sum += *w++ * in[i];
            if (s.bias) sum += *w++;
            next[j] = activate(act, sum);
            if (trace) trace->values[layer + 1][j] = next[j];
        }
        in = next;
        next = next == a ? b : a;
        prev = count;
    }
    for (unsigned j = 0; j < s.outputs; j++) out[j] = in[j];
}
