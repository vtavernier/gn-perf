#include <epoxy/gl.h>
#include <GLFW/glfw3.h>

#include <boost/filesystem.hpp>
#include <iostream>

#include <shadertoy.hpp>
#include <shadertoy/utils/log.hpp>

#include "gn_perf_config.hpp"

namespace fs = boost::filesystem;
using shadertoy::utils::log;
using shadertoy::gl::gl_call;

struct example_ctx
{
    shadertoy::render_context context;
    shadertoy::swap_chain chain;
    shadertoy::rsize render_size;
};

template<typename ExampleContext>
void example_set_framebuffer_size(GLFWwindow *window, int width, int height)
{
    // Get the context from the window user pointer
    auto &ctx = *static_cast<ExampleContext *>(glfwGetWindowUserPointer(window));

    // Reallocate textures
    ctx.render_size = shadertoy::rsize(width, height);
    ctx.context.allocate_textures(ctx.chain);
}

int main(int argc, char *argv[])
{
    int code = 0;

    log::shadertoy()->info("gn-perf {}", GN_PERF_VERSION);
    log::shadertoy()->info("Base dir {}", GN_PERF_BASE_DIR);

    if (!glfwInit())
    {
        log::shadertoy()->error("Failed to initialize glfw");
        return 2;
    }

    // Initialize window
    int width = 640, height = 480;
    GLFWwindow *window = glfwCreateWindow(width, height, "gn-perf - Gabor noise performance test", nullptr, nullptr);

    if (!window)
    {
        log::shadertoy()->error("Failed to create glfw window");
        code = 1;
    }
    else
    {
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);

        shadertoy::utils::log::shadertoy()->set_level(spdlog::level::trace);

        try
        {
            example_ctx ctx;
            auto &context(ctx.context);
            auto &chain(ctx.chain);

            // Set the context parameters (render size and some uniforms)
            ctx.render_size = shadertoy::rsize(width, height);
            context.state().get<shadertoy::iTimeDelta>() = 1.0 / 60.0;
            context.state().get<shadertoy::iFrameRate>() = 60.0;

            // Create the image buffer
            auto imageBuffer(std::make_shared<shadertoy::buffers::toy_buffer>("image"));
            imageBuffer->source_file(GN_PERF_BASE_DIR "/shaders/shader-gradient.glsl");

            // Add the image buffer to the swap chain, at the given size
            // The default_framebuffer policy makes this buffer draw directly to
            // the window instead of using a texture that is then copied to the
            // screen.
            chain.emplace_back(imageBuffer, shadertoy::make_size_ref(ctx.render_size),
                               shadertoy::member_swap_policy::default_framebuffer);

            // Initialize context
            context.init(chain);
            log::shadertoy()->info("Initialized swap chain");

            // Now render for 5s
            int frameCount = 0;
            double t = 0.;

            // Set the resize callback
            glfwSetWindowUserPointer(window, &ctx);
            glfwSetFramebufferSizeCallback(window, example_set_framebuffer_size<example_ctx>);

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

                // Update time and framecount
                t = glfwGetTime();
                frameCount++;
            }
        }
        catch (shadertoy::gl::shader_compilation_error &sce)
        {
            std::cerr << "Failed to compile shader: " << sce.log();
            code = 2;
        }
        catch (shadertoy::shadertoy_error &err)
        {
            std::cerr << "Error: " << err.what();
            code = 2;
        }

        glfwDestroyWindow(window);
    }

    glfwTerminate();
    return code;
}

