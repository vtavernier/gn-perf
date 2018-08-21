#include <epoxy/gl.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <iostream>

#include <shadertoy.hpp>
#include <shadertoy/utils/log.hpp>

#include "gn_perf_config.hpp"
#include "gn_glfw.hpp"

using shadertoy::utils::log;

unsigned int sqrti(unsigned int n)
{
    unsigned int op = n;
    unsigned int res = 0;
    unsigned int one = 1u << 30;

    one >>= __builtin_clz(op) & ~0x3;

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
        // res++;
    }

    return res;
}

int splats_sqrti(int splats)
{
    return sqrti(splats);
}

int splats_hex_sqrti(int splats)
{
    splats = splats / 2;
    if (splats == 0)
        splats = 1;
    return sqrti(splats);
}

gn_perf_ctx::gn_perf_ctx(int width, int height, const std::vector<std::string> &defines)
    : context(),
    chain(),
    render_size(width, height)
{
    // Merge the defines with the default template
    auto &buffer_definitions(static_cast<shadertoy::compiler::define_part*>(context.buffer_template()[GL_FRAGMENT_SHADER].find("glsl:defines").get())->definitions()->definitions());

    std::transform(defines.begin(), defines.end(), std::inserter(buffer_definitions, buffer_definitions.end()), [](const auto &definition)
            {
                auto eq_sign(definition.find("="));
                std::pair<std::string, std::string> rp;
                if (eq_sign == std::string::npos)
                {
                    rp = std::make_pair(std::string(definition), std::string());
                }
                else
                {
                    rp = std::make_pair(
                            std::string(definition.begin(), definition.begin() + eq_sign),
                            std::string(definition.begin() + eq_sign + 1, definition.end()));
                }
                std::transform(rp.first.begin(), rp.first.end(), rp.first.begin(), ::toupper);
                return rp;
            });

    buffer_definitions.emplace("WIDTH", std::to_string(width));
    buffer_definitions.emplace("HEIGHT", std::to_string(height));

    // Define SPLATS_SQRTI if possible
    auto it = buffer_definitions.find("SPLATS");
    if (it != buffer_definitions.end())
    {
        auto splats = std::atoi(it->second.c_str());
        if (splats != 0)
        {
            buffer_definitions.emplace("SPLATS_SQRTI", std::to_string(splats_sqrti(splats)));
            buffer_definitions.emplace("SPLATS_HEX_SQRTI", std::to_string(splats_hex_sqrti(splats)));
        }
    }

    // Create the image buffer
    auto imageBuffer(std::make_shared<shadertoy::buffers::toy_buffer>("image"));
    imageBuffer->source_file(GN_PERF_BASE_DIR "/shaders/shader-gn.glsl");
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

