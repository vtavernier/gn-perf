#include <epoxy/gl.h>
#include <GLFW/glfw3.h>

#include <iostream>

#include <shadertoy.hpp>
#include <shadertoy/utils/log.hpp>

#include "gn_perf_config.hpp"
#include "gn_glfw.hpp"

using shadertoy::utils::log;

gn_perf_ctx::gn_perf_ctx(int width, int height)
    : context(),
    chain(),
    render_size(width, height)
{
    // Create the image buffer
    auto imageBuffer(std::make_shared<shadertoy::buffers::toy_buffer>("image"));
    imageBuffer->source_file(GN_PERF_BASE_DIR "/shaders/shader-gradient.glsl");
    image_buffer = imageBuffer;

    // Add the image buffer to the swap chain, at the given size
    // The default_framebuffer policy makes this buffer draw directly to
    // the window instead of using a texture that is then copied to the
    // screen.
    chain.emplace_back(imageBuffer, shadertoy::make_size_ref(render_size),
                       shadertoy::member_swap_policy::default_framebuffer);

    // Initialize context
    context.init(chain);
    log::shadertoy()->info("Initialized swap chain");

}

void gn_set_framebuffer_size(GLFWwindow *window, int width, int height)
{
    // Get the context from the window user pointer
    auto ctx = static_cast<gn_perf_ctx *>(glfwGetWindowUserPointer(window));
    assert(ctx);

    // Reallocate textures
    ctx->render_size = shadertoy::rsize(width, height);
    ctx->context.allocate_textures(ctx->chain);

    log::shadertoy()->info("Resized render context to {}x{}", width, height);
}

int glfw_run(int width, int height, int swap_interval, std::function<void(GLFWwindow*)> window_cb)
{
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize glfw" << std::endl;
        return 2;
    }

    try
    {
        if (swap_interval <= 0)
            glfwWindowHint(GLFW_VISIBLE, 0);

        GLFWwindow *window = glfwCreateWindow(width, height, "gn-perf - Gabor noise performance test", nullptr, nullptr);

        if (!window)
            throw std::runtime_error("Failed to create glfw window");

        glfwSetFramebufferSizeCallback(window, gn_set_framebuffer_size);
        glfwMakeContextCurrent(window);
        glfwSwapInterval(swap_interval);

        try
        {
            try
            {
                window_cb(window);
                glfwDestroyWindow(window);
            }
            catch (shadertoy::gl::shader_compilation_error &sce)
            {
                std::stringstream ss;
                ss << "Failed to compile shader: " << sce.log();
                throw std::runtime_error(ss.str());
            }
        }
        catch (const std::exception &ex)
        {
            glfwDestroyWindow(window);
            throw;
        }

        glfwTerminate();
        return 0;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Fatal error: " << ex.what() << std::endl;
        glfwTerminate();
        return 2;
    }
}

