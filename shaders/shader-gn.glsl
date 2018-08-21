#define M_PI 3.141592653589793
#define W0VEC vec2(cos(W0), sin(W0))

#define RESOLUTION ivec2(WIDTH, HEIGHT)
#define TILE_COUNT (RESOLUTION / TILE_SIZE)

#define WEIGHTS_UNIFORM 0
#define WEIGHTS_BERNOULLI 1
#define WEIGHTS_NONE 2

#define POINTS_WHITE 0

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

#ifndef POINTS
#define POINTS POINTS_WHITE
#endif

#ifndef WEIGHTS
#define WEIGHTS WEIGHTS_UNIFORM
#endif

#ifndef DISP_SIZE
#define DISP_SIZE 1
#endif

#ifndef RANDOM_SEED
#define RANDOM_SEED 0
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

// LCG random
struct lcg_state { uint x_; };

void lcg_seed(inout lcg_state this_, uint seed) {
    this_.x_ = hash(seed);
}

uint lcg_rand(inout lcg_state this_) {
    return this_.x_ *= 3039177861u;
}

float lcg_rand01(inout lcg_state this_) {
    return lcg_rand(this_) / float(4294967295u);
}

vec2 lcg_rand2(inout lcg_state this_) {
    return 2. * vec2(lcg_rand01(this_), lcg_rand01(this_)) - 1.;
}

int lcg_poisson(inout lcg_state this_, float mean) {
	float g = exp(-mean);
	int em = 0;
	float t = lcg_rand01(this_);
	while (t > g) {
		++em;
		t *= lcg_rand01(this_);
	}
	return em;
}


#if POINTS == 0
// White noise generator
struct point_gen_state {
    lcg_state state;
};

void pg_seed(inout point_gen_state this_, ivec2 nc, inout int splats, out int expected_splats)
{
    uint seed = uint(nc.x * TILE_COUNT.x + nc.y + 1 + RANDOM_SEED);
    lcg_seed(this_.state, seed);

    // The expected number of points for given splats is splats
    expected_splats = splats;

    // Poisson
    splats = lcg_poisson(this_.state, splats);
}

void pg_point(inout point_gen_state this_, out vec4 pt)
{
    pt.xy = lcg_rand2(this_.state);

#if WEIGHTS == WEIGHTS_NONE
    pt.z = 1.;
#elif WEIGHTS == WEIGHTS_UNIFORM
    // Compensate loss of variance compared to Bernoulli
    pt.z = sqrt(3.) * (2. * lcg_rand01(this_.state) - 1.);
#elif WEIGHTS == WEIGHTS_BERNOULLI
    pt.z = lcg_rand01(this_.state) < .5 ? -1. : 1.;
#endif

#if RANDOM_PHASE
    pt.w = M_PI * (2. * lcg_rand01(this_.state) - 1.);
#else
    pt.w = 0.;
#endif
}

float pg_rand(inout point_gen_state this_, float max_value)
{
    return max_value * (2. * lcg_rand01(this_.state) - 1.);
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
