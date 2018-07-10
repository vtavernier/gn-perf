#include <epoxy/gl.h>
#include <GLFW/glfw3.h>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <iomanip>

#include <shadertoy.hpp>
#include <shadertoy/utils/log.hpp>

#include "gn_perf_config.hpp"
#include "stat_acc.hpp"
#include "gn_glfw.hpp"

namespace po = boost::program_options;
namespace fs = boost::filesystem;
using shadertoy::utils::log;
using shadertoy::gl::gl_call;

int main(int argc, char *argv[])
{
    int width, height, size;
    unsigned long long samples;
    bool st_silent, all_silent, raw_output, sync_anyways;
    std::string include_stat;
    std::vector<std::string> defines;

    po::options_description desc("gn-perf " GN_PERF_VERSION " (" GN_PERF_BASE_DIR ")");
    desc.add_options()
        ("width", po::value(&width)->default_value(640), "Width of the rendering")
        ("height", po::value(&height)->default_value(480), "Height of the rendering")
        ("size,s", po::value(&size)->default_value(-1), "Size (overrides width and height) of the rendering")
        ("samples,n", po::value(&samples)->default_value(0), "Number of samples to collect for statistics")
        ("quiet,q", po::bool_switch(&st_silent)->default_value(false), "Silence libshadertoy debug messages")
        ("very-quiet,Q", po::bool_switch(&all_silent)->default_value(false), "Silence everything but the final output")
        ("include-stat,I", po::value(&include_stat)->default_value("t_ms,fps,mpxps"), "Stats to include in the output")
        ("raw,r", po::bool_switch(&raw_output)->default_value(false), "Raw value output (no header nor size). Only applies to final output")
        ("sync,S", po::bool_switch(&sync_anyways)->default_value(false), "Force vsync even if measuring performance")
        ("define,D", po::value(&defines)->multitoken(), "Preprocessor definitions for the shader")
        ("help,h", "Show this help message");

    po::variables_map vm;

    try
    {
        po::store(po::parse_command_line(argc, argv, desc), vm);
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

    if (all_silent)
    {
        freopen("/dev/null", "w", stderr);
    }

    log::shadertoy()->set_level(st_silent ? spdlog::level::warn : spdlog::level::debug);

    log::shadertoy()->info("gn-perf {}", GN_PERF_VERSION);
    log::shadertoy()->info("Base dir {}", GN_PERF_BASE_DIR);

    if (samples == 0)
        log::shadertoy()->info("No sample count specified, running at vsync for debug");
    else
        log::shadertoy()->info("About to collect {} samples", samples);


    return glfw_run(width, height, samples <= 0 || sync_anyways ? 1 : 0, [&](auto *window)
    {
        // Create the context and swap chain
        gn_perf_ctx ctx(width, height, defines);
        auto &context(ctx.context);
        auto &chain(ctx.chain);

        // Now render for 5s
        int frameCount = 0;
        double t = 0.;

        // Set the resize callback
        glfwSetWindowUserPointer(window, &ctx);

        stat_acc time_ms;

        fprintf(stderr, "%8s\t%10s\t%9s\t%13s\t%4s\t%4s\n", "frame", "time_ms", "fps", "mpx_s", "wh_px", "ch_px");

        while (!glfwWindowShouldClose(window))
        {
            // Poll events
            glfwPollEvents();

            // Update uniforms
            context.state().get<shadertoy::iTime>() = t;
            context.state().get<shadertoy::iFrame>() = frameCount;

            // Set viewport
            // This is not necessary when the last pass is rendering to a
            // texture and it is followed by a screen_member, which calls
            // glViewport. In this example, we render directly to the
            // default framebuffer, so we need to set the viewport
            // ourselves.
            gl_call(glViewport, 0, 0, ctx.render_size.width, ctx.render_size.height);

            // Render the swap chain
            context.render(chain);

            // Buffer swapping
            glfwSwapBuffers(window);

            // Get the render time for the frame
            auto elapsed_time = ctx.image_buffer->elapsed_time();
            auto pixel_count = static_cast<double>(ctx.render_size.width * ctx.render_size.height);
            fprintf(stderr, "%8d\t%10lf\t%8.2lf\t%12.2lf\t%4d\t%4d\n",
                    frameCount,
                    elapsed_time / 1e6,
                    1.0e9 / elapsed_time,
                    1.0e3 * pixel_count / elapsed_time,
                    ctx.render_size.width,
                    ctx.render_size.height);

            // 0 should not be measured by the driver
            if (elapsed_time != 0)
                time_ms.sample(elapsed_time / 1e6);

            // 1024 samples is enough
            if (samples > 0 && time_ms.sample_count() == samples)
                glfwSetWindowShouldClose(window, 1);

            // Update time and framecount
            t = glfwGetTime();
            frameCount++;
        }

        fprintf(stderr, "\n");

        bool output_header = false;
        if (include_stat.find("t_ms") != std::string::npos)
            printf("%s\n", time_ms.summary(raw_output, output_header, "t_ms").c_str());
        if (include_stat.find("fps") != std::string::npos)
            printf("%s\n", time_ms.summary(raw_output, output_header, "fps", [](auto x) { return 1.0e3 / x; }).c_str());
        if (include_stat.find("mpxps") != std::string::npos)
            printf("%s\n", time_ms.summary(raw_output, output_header, "mpxps", [&ctx](auto x) { return 1.0e-3 * ctx.render_size.width * ctx.render_size.height / x; }).c_str());
    });
}

// vim: cino=
