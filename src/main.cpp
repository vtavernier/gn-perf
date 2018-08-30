#include <epoxy/gl.h>
#include <GLFW/glfw3.h>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <iomanip>

#include <shadertoy.hpp>
#include <shadertoy/utils/log.hpp>

#include <signal.h>

#include <png++/png.hpp>

#include "gn_perf_config.hpp"
#include "stat_acc.hpp"
#include "gn_glfw.hpp"
#include "hash.hpp"

static volatile int sigint_signaled = 0;

void sigint_handler(int _) {
    sigint_signaled = 1;
}

namespace po = boost::program_options;
namespace fs = boost::filesystem;
using shadertoy::utils::log;
using shadertoy::gl::gl_call;

template <typename inttype> inttype topx(float value, float k = 1.f)
{
	float scaled_value = (((value * 2.f - 1.f) * k) / 2.f + .5f) * std::numeric_limits<inttype>::max();

	// Clamp
	if (scaled_value > std::numeric_limits<inttype>::max())
		scaled_value = static_cast<float>(std::numeric_limits<inttype>::max());
	else if (scaled_value < 0.0f)
		scaled_value = 0.0f;

	return static_cast<inttype>(scaled_value);
}

void write_output(const std::string &output_basename, const stat_acc &time_ms, const std::shared_ptr<shadertoy::members::basic_member> &last_result, int width, int height, const std::string &include_stat, bool raw_output, const std::string &identifier)
{
    log::shadertoy()->info("Writing output at {}", output_basename);

    // Fetch from OpenGL
    std::vector<float> image_data(width * height * 4);

    // Get the texture
    auto texture = last_result->output();
    assert(texture);
    texture->get_image(0, GL_RGBA, GL_FLOAT, sizeof(float) * image_data.size(), image_data.data());

    // Write image
    png::image<png::rgba_pixel_16> image(width, height);
    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < height; ++y) {
            float *pi = &image_data[4 * (y * width + x)];
            image.set_pixel(x, y, png::rgba_pixel_16(
                        topx<uint16_t>(pi[0]),
                        topx<uint16_t>(pi[1]),
                        topx<uint16_t>(pi[2]),
                        std::numeric_limits<uint16_t>::max()));
        }
    }

    image.write(output_basename + ".png");

    // Write details
    std::ofstream ofs((output_basename + ".txt").c_str());
    assert(!ofs.fail());

    // Print state identifier
    if (!raw_output)
    {
        ofs << "# " << identifier << std::endl;
    }

    bool output_header = false;
    if (include_stat.find("t_ms") != std::string::npos)
        ofs << time_ms.summary("", raw_output, output_header, "t_ms").c_str() << std::endl;
    if (include_stat.find("fps") != std::string::npos)
        ofs << time_ms.summary("", raw_output, output_header, "fps", [](auto x) { return 1.0e3 / x; }).c_str() << std::endl;
    if (include_stat.find("mpxps") != std::string::npos)
        ofs << time_ms.summary("", raw_output, output_header, "mpxps", [width, height](auto x) { return 1.0e-3 * width * height / x; }).c_str() << std::endl;
}

int main(int argc, char *argv[])
{
    int width, height, size;
    long long samples, warmup_samples;
    bool st_silent, all_silent, raw_output, sync_anyways, test_mode;
    std::string include_stat, output;
    std::vector<std::string> defines;

    po::options_description gn_desc("Noise options");
    gn_desc.add_options()
        ("width", po::value(&width)->default_value(640), "Width of the rendering")
        ("height", po::value(&height)->default_value(480), "Height of the rendering")
        ("size,s", po::value(&size)->default_value(-1), "Size (overrides width and height) of the rendering")
        ("warmup,W", po::value(&warmup_samples)->default_value(-1), "Number of samples to warm-up the measurements")
        ("samples,n", po::value(&samples)->default_value(0), "Number of samples to collect for statistics")
        ("output,o", po::value(&output)->default_value(""), "Output path for the control frame")
        ("define,D", po::value(&defines)->multitoken()->composing(), "Preprocessor definitions for the shader\n"
         "The following values are supported: \n"
         "\t * SPLATS=n: number of splats per cell\n"
         "\t * F0=freq: frequency of the kernel\n"
         "\t - W0=angle: angle of the anisotropic kernel (defaults to pi/4)\n"
         "\t - TILE_SIZE=size: kernel diameter, in pixels (defaults to width/3)\n"
         "\t - RANDOM_SEED=r: random seed (defaults to 0)\n"
         "\t - DISP_SIZE=s: number of cells to look for contributing splats (defaults to 1)\n"
         "\t - KTRUNC: make kernel boundary C0\n"
         "\t - KSIN: use sin instead of cos for kernel\n"
         "\t - KSHOW: show kernel footprint as a disk\n"
         "\t - RANDOM_PHASE: use random phase kernel\n"
         "\t - WEIGHTS=weight: type of the random weights to use:\n"
         "\t   - WEIGHTS_UNIFORM: uniform [-1, 1] weights (default)\n"
         "\t   - WEIGHTS_BERNOULLI: Bernoulli {-1, 1} weights\n"
         "\t   - WEIGHTS_NONE: no weights\n"
         "\t - POINTS=type: type of the point distribution:\n"
         "\t   - POINTS_WHITE: white (Poisson) points (default)\n"
         "\t   - POINTS_STRATIFIED: stratified points\n"
         "\t   - POINTS_JITTERED: jittered rectangular grid points\n"
         "\t   - POINTS_HEX_JITTERED: jittered triangular grid points\n"
         "\t   - POINTS_GRID: regular rectangular grid points\n"
         "\t   - POINTS_HEX_GRID: regular triangular grid points\n"
         "\t - PRNG=type: type of the PRNG to use:\n"
         "\t   - PRNG_LCG: linear congruential generator (default)\n"
         "\t   - PRNG_XOROSHIRO: SIMD xoroshiro64** generator\n"
         "\t   - PRNG_HASH: integer hash generator (2^8 period)\n"
         "\t   - PRNG_XORSHIFT: xorshift tuple generator\n"
         "\t   - PRNG_NONE: constant generator for debugging\n");

    po::options_description desc("gn-perf " GN_PERF_VERSION " (" GN_PERF_BASE_DIR ")");
    desc.add_options()
        ("config,C", po::value<std::string>(), "Configuration file for the noise")
        ("quiet,q", po::bool_switch(&st_silent)->default_value(false), "Silence libshadertoy debug messages")
        ("very-quiet,Q", po::bool_switch(&all_silent)->default_value(false), "Silence everything but the final output")
        ("include-stat,I", po::value(&include_stat)->default_value("t_ms,fps,mpxps"), "Stats to include in the output")
        ("raw,r", po::bool_switch(&raw_output)->default_value(false), "Raw value output (no header nor size). Only applies to final output")
        ("sync,S", po::bool_switch(&sync_anyways)->default_value(false), "Force vsync even if measuring performance")
        ("test,T", po::bool_switch(&test_mode)->default_value(false), "TAP self-test mode")
        ("help,h", "Show this help message");

    desc.add(gn_desc);

    po::positional_options_description po_desc;
    po_desc.add("config", 1);

    po::variables_map vm;

    try
    {
        po::store(po::command_line_parser(argc, argv).options(desc).positional(po_desc).run(), vm);

        if (vm.count("config") > 0)
        {
            auto config_name(vm["config"].as<std::string>());
            std::ifstream ifs(config_name.c_str());

            if (ifs.fail())
            {
                throw po::error("Failed to open config file");
            }

            po::store(po::parse_config_file(ifs, gn_desc), vm);
        }

        po::notify(vm);
    }
    catch (const po::error &ex)
    {
        std::cerr << ex.what() << std::endl;
        std::cerr << "See --help option for usage" << std::endl;
        return 1;
    }

    if (vm.count("help"))
    {
        std::cout << desc << std::endl;
        return 0;
    }

    if (size > 0)
    {
        width = height = size;
    }

    if (warmup_samples < 0 && samples != 0)
    {
        warmup_samples = 64;
    }
    else
    {
        warmup_samples = 0;
    }

    if (test_mode)
    {
        all_silent = true;
    }

    if (all_silent)
    {
        freopen("/dev/null", "w", stderr);
    }

    log::shadertoy()->set_level(st_silent ? spdlog::level::info : spdlog::level::debug);

    log::shadertoy()->info("gn-perf {}", GN_PERF_VERSION);
    log::shadertoy()->info("Base dir {}", GN_PERF_BASE_DIR);

    if (samples == 0)
        log::shadertoy()->info("No sample count specified, running at vsync for debug");
    else if (samples < 0)
        log::shadertoy()->info("About to collect enough samples so stddev% < {}e-2%", -samples);
    else
        log::shadertoy()->info("About to collect {} samples", samples);

#if HAS_NVML
    // We are using the first GPU anyways
    nvmlDevice_t device;
    bool nvml_enabled = false;

    if (nvmlInit() == NVML_SUCCESS) {
        nvml_enabled = true;
        log::shadertoy()->debug("Initialized NVML");
    } else {
        log::shadertoy()->warn("Failed to initialize NVML");
    }

    if (nvml_enabled && nvmlDeviceGetHandleByIndex(0, &device) == NVML_SUCCESS) {
        nvmlPstates_t pstate = NVML_PSTATE_UNKNOWN;
        nvmlDeviceGetPerformanceState(device, &pstate);

        char buf[NVML_DEVICE_NAME_BUFFER_SIZE];
        if (nvmlDeviceGetName(device, buf, sizeof(buf)) == NVML_SUCCESS) {
            log::shadertoy()->info("Found NVML device: {}, initial p-state: {}", buf, pstate);
        } else {
            log::shadertoy()->info("Found NVML device, initial p-state: {}", pstate);
        }
    } else {
        nvml_enabled = false;
        log::shadertoy()->warn("Device not found");
    }
#endif /* HAS_NVML */

    bool visible = samples == 0 || sync_anyways ? 1 : 0;
    return glfw_run(width, height, visible, [&](auto *window)
    {
        // Create the context and swap chain
        gn_perf_ctx ctx(width, height, defines, visible);
        auto &context(ctx.context);
        auto &chain(ctx.chain);

        // Now render for 5s
        int frameCount = 0;
        double t = 0.;

        // Set the resize callback
        glfwSetWindowUserPointer(window, &ctx);

        stat_acc time_ms;

        fprintf(stderr, "%8s\t%10s\t%9s\t%13s\t%4s\t%4s\t%9s\n", "frame", "time_ms", "fps", "mpx_s", "wh_px", "ch_px", "stddevp");

        signal(SIGINT, sigint_handler);

        std::shared_ptr<shadertoy::members::basic_member> last_result;
        while (!glfwWindowShouldClose(window) && !sigint_signaled)
        {
            // Poll events
            glfwPollEvents();

            // Update uniforms
            context.state().get<shadertoy::iTime>() = t;
            context.state().get<shadertoy::iFrame>() = uhash(frameCount);

            // Set viewport
            // This is not necessary when the last pass is rendering to a
            // texture and it is followed by a screen_member, which calls
            // glViewport. In this example, we render directly to the
            // default framebuffer, so we need to set the viewport
            // ourselves.
            gl_call(glViewport, 0, 0, ctx.render_size.width, ctx.render_size.height);

            // Render the swap chain
            last_result = context.render(chain);

            // Buffer swapping
            glfwSwapBuffers(window);

            bool pstate_ok = true;
#if HAS_NVML
            // If we have a working NVML, only start measuring at P2 or higher
            nvmlPstates_t pstate = NVML_PSTATE_UNKNOWN;
            if (nvml_enabled && nvmlDeviceGetPerformanceState(device, &pstate) == NVML_SUCCESS) {
                pstate_ok = pstate <= NVML_PSTATE_2;
            }
#endif

            if (warmup_samples <= 0 && pstate_ok)
            {
                // Get the render time for the frame
                auto elapsed_time = ctx.image_buffer->elapsed_time();
                auto pixel_count = static_cast<double>(ctx.render_size.width * ctx.render_size.height);

                // 0 should not be measured by the driver
                if (elapsed_time != 0)
                    time_ms.sample(elapsed_time / 1e6);

                auto stddevp = time_ms.stddevp();
                if ((samples > 0 && time_ms.sample_count() == samples) ||
                    (samples < 0 && time_ms.sample_count() >= 16 && (stddevp * 1e4) < -samples))
                    glfwSetWindowShouldClose(window, 1);

                fprintf(stderr, "%8d\t%10lf\t%8.2lf\t%12.2lf\t%4d\t%4d\t%8.2lf\n",
                        frameCount,
                        elapsed_time / 1e6,
                        1.0e9 / elapsed_time,
                        1.0e3 * pixel_count / elapsed_time,
                        ctx.render_size.width,
                        ctx.render_size.height,
                        stddevp * 1e2);
            }
            else
            {
                warmup_samples--;
            }

            // Update time and framecount
            t = glfwGetTime();
            frameCount++;
        }

        fprintf(stderr, "\n");

        // Write output data
        if (!output.empty())
        {
            write_output(output, time_ms, last_result, ctx.render_size.width, ctx.render_size.height, include_stat, raw_output, ctx.identifier);
        }

        // TODO: actually test something
        const char *test_prefix = test_mode ? "# " : "";
        if (test_mode)
        {
            printf("1..1\nok 1\n");
        }

        // Print state identifier
        if (!raw_output)
        {
            printf("%s# %s\n", test_prefix, ctx.identifier.c_str());
        }

        bool output_header = false;
        if (include_stat.find("t_ms") != std::string::npos)
            printf("%s\n", time_ms.summary(test_prefix, raw_output, output_header, "t_ms").c_str());
        if (include_stat.find("fps") != std::string::npos)
            printf("%s\n", time_ms.summary(test_prefix, raw_output, output_header, "fps", [](auto x) { return 1.0e3 / x; }).c_str());
        if (include_stat.find("mpxps") != std::string::npos)
            printf("%s\n", time_ms.summary(test_prefix, raw_output, output_header, "mpxps", [&ctx](auto x) { return 1.0e-3 * ctx.render_size.width * ctx.render_size.height / x; }).c_str());

#if HAS_NVML
        nvmlShutdown();
#endif
    });
}

// vim: cino=
