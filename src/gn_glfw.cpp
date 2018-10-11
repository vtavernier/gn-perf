#include <epoxy/gl.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <iostream>

#include <shadertoy.hpp>
#include <shadertoy/utils/log.hpp>

#include <picosha2.h>

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

gn_perf_ctx::gn_perf_ctx(int width, int height, const std::vector<std::string> &defines, bool visible, const std::string &lut_path)
    : context(),
    chain(),
    render_size(width, height)
{
    // Merge the defines with the default template
    auto preprocessor_defines(std::make_shared<shadertoy::compiler::preprocessor_defines>());
    auto &buffer_definitions(preprocessor_defines->definitions());

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

    if (!lut_path.empty())
        buffer_definitions.emplace("ENABLE_LUT", std::string());

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

    context.buffer_template().shader_defines().emplace("gn_perf", preprocessor_defines);

    // Create the image buffer
    std::map<GLenum, std::string> sources;

    image_buffer = std::make_shared<shadertoy::buffers::toy_buffer>("image");
    image_buffer->source_map(&sources);
    image_buffer->source_file(GN_PERF_BASE_DIR "/shaders/shader-gn.glsl");

    if (!lut_path.empty())
    {
        shadertoy::utils::input_loader loader;

        image_buffer->inputs().emplace_back(loader.create(std::string("file:///") + lut_path));

        auto &input(*image_buffer->inputs().front().input());
        input.wrap(GL_CLAMP_TO_EDGE);
        input.mag_filter(GL_LINEAR);
        input.min_filter(GL_LINEAR);
    }

    // Add the image buffer to the swap chain, at the given size
    chain.emplace_back(image_buffer, shadertoy::make_size_ref(render_size),
                       shadertoy::member_swap_policy::double_buffer);

    if (visible)
    {
        // Render the result to the screen
        chain.emplace_back<shadertoy::members::screen_member>(shadertoy::make_size_ref(render_size));
    }

    // Initialize context
    context.init(chain);

    // Compute identifier for the sources
    auto &sources_str(sources[GL_FRAGMENT_SHADER]);
    std::vector<uint8_t> hash(picosha2::k_digest_size);
    picosha2::hash256(sources_str.begin(), sources_str.end(), hash.begin(), hash.end());
    identifier = picosha2::bytes_to_hex_string(hash.begin(), hash.end());
    log::shadertoy()->info("Initialized swap chain {}", identifier);

    // Clear the source map
    image_buffer->source_map(nullptr);
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

