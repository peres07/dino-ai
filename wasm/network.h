#ifndef DINO_NETWORK_H
#define DINO_NETWORK_H

#include <stdint.h>
#include <stddef.h>

#define MAX_WEIGHTS 2048
#define MAX_HIDDEN 32
#define MAX_LAYERS 6
#define NETWORK_BYTES (28 + 8 * MAX_WEIGHTS)
#define MODEL_BYTES (12 + 2 * NETWORK_BYTES)

typedef struct {
    uint32_t inputs, hidden, outputs, depth, activation, output_activation, bias;
} Shape;

typedef struct {
    Shape shape;
    double values[MAX_LAYERS + 2][MAX_HIDDEN];
} Trace;

extern const Shape TRAIN_SHAPE;
size_t weight_count(Shape s);
int read_model(const unsigned char *bytes, size_t len, Shape *shape, double *weights);
size_t write_model(unsigned char *bytes, Shape shape, const double *weights);
int read_pair(const unsigned char *bytes, size_t len, Shape *aim, double *aim_weights, Shape *move, double *move_weights);
size_t write_pair(unsigned char *bytes, Shape aim, const double *aim_weights, Shape move, const double *move_weights);
void infer(Shape shape, const double *weights, const double *in, double *out, Trace *trace);
double dino_sin(double x);
double dino_cos(double x);

#endif
