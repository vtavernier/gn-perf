#define M_PI 3.141592653589793
#define W0VEC vec2(cos(W0), sin(W0))

#define RESOLUTION ivec2(WIDTH, HEIGHT)
#define TILE_COUNT (RESOLUTION / TILE_SIZE)

#define WEIGHTS_UNIFORM 0
#define WEIGHTS_BERNOULLI 1
#define WEIGHTS_NONE 2

#define POINTS_WHITE 0

#define PRNG_LCG 0
#define PRNG_XOROSHIRO 1

#ifndef WIDTH
#define WIDTH int(iResolution.x)
#endif

#ifndef HEIGHT
#define HEIGHT int(iResolution.y)
#endif

#ifndef W0
#define W0 (M_PI/4)
#endif

#ifndef TILE_SIZE
#define TILE_SIZE (RESOLUTION / 3)
#endif

#ifndef DISP_SIZE
#define DISP_SIZE 1
#endif

#ifndef RANDOM_SEED
#define RANDOM_SEED 0
#endif

#ifndef POINTS
#define POINTS POINTS_WHITE
#endif

#ifndef WEIGHTS
#define WEIGHTS WEIGHTS_UNIFORM
#endif

#ifndef PRNG
#define PRNG PRNG_LCG
#endif

float h(vec2 x, float phase) {
    float r = length(x);
    float eb = exp(-M_PI * r * r);

    // Truncate kernel so it fits in a cell
#ifdef KTRUNC
    eb = eb - exp(-M_PI);
    eb = eb < 0. ? 0. : eb;
#else
    eb = eb < exp(-M_PI) ? 0. : eb;
#endif

    float s;
    // Compute the wave part of the kernel
#ifdef KSIN
    s = sin(2. * M_PI * F0 * dot(x / TILE_SIZE, W0VEC) + phase);
#else
    s = cos(2. * M_PI * F0 * dot(x / TILE_SIZE, W0VEC) + phase);
#endif

    return eb * s;
}

// Hashing function
uint hash(uint x) {
    x = ((x >> 16) ^ x) * 0x45d9f3bu;
    x = ((x >> 16) ^ x) * 0x45d9f3bu;
    x = (x >> 16) ^ x;
    return x;
}

uvec4 hash4(uvec4 x) {
    x = ((x >> 16) ^ x) * 0x45d9f3bu;
    x = ((x >> 16) ^ x) * 0x45d9f3bu;
    x = (x >> 16) ^ x;
    return x;
}

// LCG random
#if PRNG == PRNG_LCG
struct prng_state { uint x_; };

void prng_seed(inout prng_state this_, uint seed) {
    this_.x_ = hash(seed);
}

uint prng_rand(inout prng_state this_) {
    return this_.x_ *= 3039177861u;
}

float prng_rand01(inout prng_state this_) {
    return prng_rand(this_) / float(4294967295u);
}

vec2 prng_rand2(inout prng_state this_) {
    return 2. * vec2(prng_rand01(this_), prng_rand01(this_)) - 1.;
}

#elif PRNG == PRNG_XOROSHIRO
struct prng_state { uvec4 x_; };

uint rotl(uint x, int k) {
    return (x << k) | (x >> (32 - k));
}

// Generates 1 uniform integers, updates the state given as input
uint next(inout uvec2 s) {
    uint s0 = s.x;
    uint s1 = s.y;
    uint rs = rotl(s0 * 0x9E3779BBu, 5) * 5u;

    s1 ^= s0;
    s.x = rotl(s0, 26) ^ s1 ^ (s1 << 9);
    s.y = rotl(s1, 13);

    return rs;
}

// Converts an unsigned int to a float in [0,1]
float tofloat(uint u) {
    //Slower, but generates all dyadic rationals of the form k / 2^-24 equally
    //return float(u >> 8) * (1. / float(1u << 24));

    //Faster, but only generates all dyadic rationals of the form k / 2^-23 equally
    return uintBitsToFloat(0x7Fu << 23 | u >> 9) - 1.;
}

uvec2 rotl2(uvec2 x, int k) {
    return (x << k) | (x >> (32 - k));
}

// Generates 2 uniform integers, updates the state given as input
uvec2 next2(inout uvec4 s) {
    uvec2 s0 = s.xz;
    uvec2 s1 = s.yw;
    uvec2 rs = rotl2(s0 * 0x9E3779BBu, 5) * 5u;

    s1 ^= s0;
    s.xz = rotl2(s0, 26) ^ s1 ^ (s1 << 9);
    s.yw = rotl2(s1, 13);

    return rs;
}

// Converts a vector of unsigned ints to floats in [0,1]
vec2 tofloat2(uvec2 u) {
    //Slower, but generates all dyadic rationals of the form k / 2^-24 equally
    //return vec2(u >> 8) * (1. / float(1u << 24));

    //Faster, but only generates all dyadic rationals of the form k / 2^-23 equally
    return uintBitsToFloat(0x7Fu << 23 | u >> 9) - 1.;
}

void prng_seed(inout prng_state this_, uint seed) {
    this_.x_ = hash4(seed << 4 | uvec4(0, 1, 2, 3));
}

uint prng_rand(inout prng_state this_) {
    return next(this_.x_.xy);
}

float prng_rand01(inout prng_state this_) {
    return tofloat(prng_rand(this_));
}

vec2 prng_rand2(inout prng_state this_) {
    return 2. * tofloat2(next2(this_.x_)) - 1.;
}

#endif

int prng_poisson(inout prng_state this_, float mean) {
    float g = exp(-mean);
    int em = 0;
    float t = prng_rand01(this_);
    while (t > g) {
        ++em;
        t *= prng_rand01(this_);
    }
    return em;
}

#if POINTS == POINTS_WHITE
// White noise generator
struct point_gen_state {
    prng_state state;
};

void pg_seed(inout point_gen_state this_, ivec2 nc, inout int splats, out int expected_splats)
{
    uint seed = uint(nc.x * TILE_COUNT.x + nc.y + 1 + RANDOM_SEED);
    prng_seed(this_.state, seed);

    // The expected number of points for given splats is splats
    expected_splats = splats;

    // Poisson
    splats = prng_poisson(this_.state, splats);
}

void pg_point(inout point_gen_state this_, out vec4 pt)
{
    pt.xy = prng_rand2(this_.state);
    pt.zw = prng_rand2(this_.state);

#if WEIGHTS == WEIGHTS_NONE
    pt.z = 1.;
#elif WEIGHTS == WEIGHTS_UNIFORM
    // Compensate loss of variance compared to Bernoulli
    pt.z *= sqrt(3.);
#elif WEIGHTS == WEIGHTS_BERNOULLI
    pt.z = sign(pt.z);
#endif

#if RANDOM_PHASE
    pt.w *= M_PI;
#else
    pt.w = 0.;
#endif
}
#endif

void mainImage(out vec4 O, in vec2 U)
{
    ivec2 ccell = ivec2(U / TILE_SIZE);
    ivec2 disp;

    // Initial return value
    O = vec4(0.);

    // TODO: This can be halved depending on the location U w.r.t. cell center
    for (disp.x = -DISP_SIZE; disp.x <= DISP_SIZE; ++disp.x)
    {
        for (disp.y = -DISP_SIZE; disp.y <= DISP_SIZE; ++disp.y)
        {
            // Current cell coordinates
            ivec2 cell = ccell + disp;
            // Current cell coordinates (periodic)
            ivec2 nc = ivec2(mod(vec2(cell), TILE_COUNT.x));
            // Cell center (pixel coordinates)
            vec2 center = TILE_SIZE * (vec2(cell) + .5);

            // Seed the point generator
            int splats = SPLATS;
            int expected;
            point_gen_state pg_state;
            pg_seed(pg_state, nc, splats, expected);

            for (int i = 0; i < splats; ++i)
            {
                // Get a point properties
                vec4 props;
                pg_point(pg_state, props);

                // Adjust point for tile properties
                props.xy = center + TILE_SIZE / 2 * props.xy;

                // Compute relative location
                props.xy = (U - props.xy) / TILE_SIZE;

                // Compute contribution
                O += props.z * h(props.xy, props.w) / sqrt(float(expected));
            }
        }
    }

    // [0, 1] range
    O = .5 + .5 * O;
}
