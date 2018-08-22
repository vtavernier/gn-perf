#ifndef _GN_PERF_GLFW_HPP_
#define _GN_PERF_GLFW_HPP_

struct gn_perf_ctx
{
    shadertoy::render_context context;
    shadertoy::swap_chain chain;
    shadertoy::rsize render_size;
    std::shared_ptr<shadertoy::buffers::toy_buffer> image_buffer;
    std::string identifier;

    gn_perf_ctx(int width, int height, const std::vector<std::string> &defines, bool visible);
};

void gn_set_framebuffer_size(GLFWwindow *window, int width, int height);

int glfw_run(int width, int height, int swap_interval, std::function<void(GLFWwindow*)> window_cb);

#endif /* _GN_PERF_GLFW_HPP_ */
