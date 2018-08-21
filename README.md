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
./gn_perf -h
```

## Usage

The parameters of the Gabor noise shader are controlled through a set of preprocessor
definitions. These are set up using the `-D` argument. Configuration file options are
overridden by those on the command line.

    gn-perf b11814c (/disc/vtaverni/phd/gn-perf):
      -C [ --config ] arg                   Configuration file for the noise
      -q [ --quiet ]                        Silence libshadertoy debug messages
      -Q [ --very-quiet ]                   Silence everything but the final output
      -I [ --include-stat ] arg (=t_ms,fps,mpxps)
                                            Stats to include in the output
      -r [ --raw ]                          Raw value output (no header nor size). 
                                            Only applies to final output
      -S [ --sync ]                         Force vsync even if measuring 
                                            performance
      -h [ --help ]                         Show this help message
    
    Noise options:
      --width arg (=640)                    Width of the rendering
      --height arg (=480)                   Height of the rendering
      -s [ --size ] arg (=-1)               Size (overrides width and height) of 
                                            the rendering
      -n [ --samples ] arg (=0)             Number of samples to collect for 
                                            statistics
      -D [ --define ] arg                   Preprocessor definitions for the shader
                                            The following values are supported: 
                                             * SPLATS=n: number of splats per cell
                                             * F0=freq: frequency of the kernel
                                             - W0=angle: angle of the anisotropic 
                                            kernel (defaults to pi/4)
                                             - TILE_SIZE=size: kernel diameter, in 
                                            pixels (defaults to width/3)
                                             - RANDOM_SEED=r: random seed (defaults
                                            to 0)
                                             - DISP_SIZE=s: number of cells to look
                                            for contributing splats (defaults to 1)
                                             - KTRUNC: make kernel boundary C0
                                             - KSIN: use sin instead of cos for 
                                            kernel
                                             - RANDOM_PHASE: use random phase 
                                            kernel
                                             - WEIGHTS=weight: type of the random 
                                            weights to use:
                                               - WEIGHTS_UNIFORM: uniform [-1, 1] 
                                            weights (default)
                                               - WEIGHTS_BERNOULLI: Bernoulli {-1, 
                                            1} weights
                                               - WEIGHTS_NONE: no weights
                                             - POINTS=type: type of the point 
                                            distribution:
                                               - POINTS_WHITE: white (Poisson) 
                                            points (default)

## Author

Vincent Tavernier <vincent.tavernier@inria.fr>

## License

The source code in this repository is licensed under the [MIT license](LICENSE).
