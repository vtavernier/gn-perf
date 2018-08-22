#define M_PI 3.141592653589793
#define W0VEC vec2(cos(W0), sin(W0))

#define RESOLUTION ivec2(WIDTH, HEIGHT)
#define TILE_COUNT (RESOLUTION / TILE_SIZE)

#define WEIGHTS_UNIFORM 0
#define WEIGHTS_BERNOULLI 1
#define WEIGHTS_NONE 2

#define POINTS_WHITE 0
#define POINTS_STRATIFIED 1
#define POINTS_JITTERED 2
#define POINTS_HEX_JITTERED 3
#define POINTS_GRID 4
#define POINTS_HEX_GRID 5

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

float prng_rand1(inout prng_state this_) {
    return (this_.x_ *= 3039177861u) / float(4294967295u);
}

vec2 prng_rand2(inout prng_state this_) {
    return vec2(prng_rand1(this_), prng_rand1(this_));
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

float prng_rand1(inout prng_state this_) {
    return tofloat(next(this_.x_.xy));
}

vec2 prng_rand2(inout prng_state this_) {
    return tofloat2(next2(this_.x_));
}

#endif

int prng_poisson(inout prng_state this_, float mean) {
    int em = 0;

    if (mean < 50.)
    {
        // Knuth
        float g = exp(-mean);
        float t = prng_rand1(this_);
        while (t > g) {
            ++em;
            t *= prng_rand1(this_);
        }
    }
    else
    {
        // Gaussian approximation
        vec2 u = prng_rand2(this_);
        float v = sqrt(-2. * log(u.x)) * cos(2. * M_PI * u.y);
        em = int((v * sqrt(mean)) + mean + .5);
    }

    return em;
}

#if POINTS <= POINTS_STRATIFIED
// White noise generator
struct point_gen_state {
    prng_state state;
};

void pg_seed(inout point_gen_state this_, ivec2 nc, out int splats, out int expected_splats)
{
    uint seed = uint(nc.x * TILE_COUNT.x + nc.y + 1 + RANDOM_SEED);
    prng_seed(this_.state, seed);

    // The expected number of points for given splats is splats
    splats = SPLATS;
    expected_splats = splats;

#if POINTS == POINTS_WHITE
    // Poisson
    splats = prng_poisson(this_.state, splats);
#endif
}

void pg_point(inout point_gen_state this_, out vec4 pt)
{
    // Generate random position
    pt.xy = 2. * prng_rand2(this_.state) - 1.;

    // Generate random weight and phase
#if WEIGHTS != WEIGHTS_NONE && defined(RANDOM_PHASE)
    pt.zw = 2. * prng_rand2(this_.state) - 1.;
#elif WEIGHTS != WEIGHTS_NONE && !defined(RANDOM_PHASE)
    pt.z = 2. * prng_rand1(this_.state) - 1.;
#elif WEIGHTS == WEIGHTS_NONE && defined(RANDOM_PHASE)
    pt.w = 2. * prng_rand1(this_.state) - 1.;
#endif

#if WEIGHTS == WEIGHTS_NONE
    pt.z = 1.;
#elif WEIGHTS == WEIGHTS_UNIFORM
    // Compensate loss of variance compared to Bernoulli
    pt.z *= sqrt(3.);
#elif WEIGHTS == WEIGHTS_BERNOULLI
    pt.z = sign(pt.z);
#endif

#ifdef RANDOM_PHASE
    pt.w *= M_PI;
#else
    pt.w = 0.;
#endif
}
#elif POINTS <= POINTS_HEX_GRID
#define HAS_JITTERED_GRID (POINTS < POINTS_GRID)
#define HAS_HEX_GRID (POINTS == POINTS_HEX_JITTERED || POINTS == POINTS_HEX_GRID)

// Grid point generator
struct point_gen_state {
    prng_state prng;

    uint ic;
    uint c;
#if !defined(SPLATS_SQRTI) || !defined(SPLATS_HEX_SQRTI)
    uint splats;
    uint dx;
    uint dy;

#define STATE_SPLATS(this_) this_.splats
#define STATE_DX(this_) this_.dx
#define STATE_DY(this_) this_.dy
#else /* !defined(SPLATS_SQRTI) || !defined(SPLATS_HEX_SQRTI) */
#define SPLATS_SPLATS ((HAS_HEX_GRID)?((SPLATS/2) == 0?1:(SPLATS/2)):(SPLATS))
#if HAS_HEX_GRID
#define SPLATS_DX SPLATS_HEX_SQRTI
#else /* HAS_HEX_GRID */
#define SPLATS_DX SPLATS_SQRTI
#endif /* HAS_HEX_GRID */
#define SPLATS_DY ((SPLATS_SPLATS - (SPLATS_DX * SPLATS_DX)) / SPLATS_DX + SPLATS_DX)

#define STATE_SPLATS(this_) (SPLATS_DX * SPLATS_DY)
#define STATE_DX(this_) SPLATS_DX
#define STATE_DY(this_) SPLATS_DY
#endif /* !defined(SPLATS_SQRTI) || !defined(SPLATS_HEX_SQRTI) */
};

#if !defined(SPLATS_SQRTI) || !defined(SPLATS_HEX_SQRTI)
uint sqrti(uint n)
{
    uint op = n;
    uint res = 0;
    uint one = 1u << 30;

    one >>= (31 - findMSB(op)) & ~0x3;

    while (one != 0)
    {
        if (op >= res + one)
        {
            op = op - (res + one);
            res = res + (one << 1);
        }

        res >>= 1;
        one >>= 2;
    }

    if (op > res)
    {
        //res++;
    }

    return res;
}
#endif /* !defined(SPLATS_SQRTI) || !defined(SPLATS_HEX_SQRTI) */

void pg_seed(inout point_gen_state this_, ivec2 nc, out int splats, out int expected_splats)
{
    uint seed = uint(nc.x * TILE_COUNT.x + nc.y + 1 + RANDOM_SEED);
    prng_seed(this_.prng, seed);

#ifndef SPLATS_SPLATS
    splats = SPLATS;

#if HAS_HEX_GRID
    splats = splats / 2;
    if (splats == 0)
        splats = 1;
#endif /* HAS_HEX_GRID */

    this_.dx = sqrti(splats);
    this_.dy = (splats - (this_.dx * this_.dx)) / this_.dx + this_.dx;

    expected_splats = splats = int(this_.dx * this_.dy);

    this_.splats = splats;
#else
    expected_splats = splats = int(SPLATS_DX * SPLATS_DY);
#endif /* SPLATS_SPLATS */

    this_.ic = (seed >> 2) % splats;
    this_.c = this_.ic;

#if HAS_HEX_GRID
    // An hexagonal grid at that scale is 2 times denser
    splats = expected_splats *= 2;
#endif /* HAS_HEX_GRID */
}

void pg_point(inout point_gen_state this_, out vec4 pt)
{
#if HAS_JITTERED_GRID
    // Generate random position
    pt.xy = 2. * prng_rand2(this_.prng) - 1.;
#else
    pt.xy = vec2(0.);
#endif /* HAS_JITTERED_GRID */

#if HAS_HEX_GRID
#if !HAS_JITTERED_GRID
    pt.xy = vec2(0., 1.);
#endif /* !HAS_JITTERED_GRID */

    // Apply triangle transform
    pt.xy = vec2(.25 * (pt.x - pt.y), .5 * abs(pt.x + pt.y));
    if ((this_.c - this_.ic) >= STATE_SPLATS(this_))
    {
        pt.x += 1.;
        pt.y = -pt.y;
    }
#endif /* HAS_HEX_GRID */

    // pt.xy is in [0, tsx] x [0, tsy] after these lines
    pt.xy = pt.xy / 2.f + vec2(.5f);
    pt.xy = pt.xy / vec2(STATE_DX(this_), STATE_DY(this_));

    // move pt.xy to subcell
    pt.xy += vec2((this_.c % STATE_SPLATS(this_)) / STATE_DY(this_), (this_.c % STATE_SPLATS(this_)) % STATE_DY(this_)) / vec2(STATE_DX(this_), STATE_DY(this_));

    // back to [-1, 1]
    pt.xy = 2. * (pt.xy - vec2(.5f));

    // next cell
    this_.c++;

    // Generate random weight and phase
#if WEIGHTS != WEIGHTS_NONE && defined(RANDOM_PHASE)
    pt.zw = 2. * prng_rand2(this_.prng) - 1.;
#elif WEIGHTS != WEIGHTS_NONE && !defined(RANDOM_PHASE)
    pt.z = 2. * prng_rand1(this_.prng) - 1.;
#elif WEIGHTS == WEIGHTS_NONE && defined(RANDOM_PHASE)
    pt.w = 2. * prng_rand1(this_.prng) - 1.;
#endif

#if WEIGHTS == WEIGHTS_NONE
    pt.z = 1.;
#elif WEIGHTS == WEIGHTS_UNIFORM
    // Compensate loss of variance compared to Bernoulli
    pt.z *= sqrt(3.);
#elif WEIGHTS == WEIGHTS_BERNOULLI
    pt.z = sign(pt.z);
#endif

#ifdef RANDOM_PHASE
    pt.w *= M_PI;
#else
    pt.w = 0.;
#endif
}

#endif /* POINTS */

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
            int splats;
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
