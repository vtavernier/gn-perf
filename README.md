# gn-perf

This tool implements various optimizations to procedural noise based on sparse Gabor
convolution in order to evaluate their performance.

## Dependencies

* Boost
* GLFW3
* epoxy
* libshadertoy (included in `libshadertoy/`)

```bash
sudo apt install build-essential cmake pkg-config libepoxy-dev libglfw3-dev libboost-filesystem-dev \
                 libboost-date-time-dev libgl1-mesa-dev libglm-dev libunwind-dev
```

## Getting started

```bash
git clone --recursive https://gitlab.inria.fr/vtaverni/gn-perf.git
cd gn-perf
mkdir build && cd build
cmake ..
make -j$(nproc)
./gn_perf
```

## Author

Vincent Tavernier <vincent.tavernier@inria.fr>

## License

The source code in this repository is licensed under the [MIT license](LICENSE).
