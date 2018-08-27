# Find package for NVML (NVIDIA Management Library)
include(LibFindMacros)

# Include directory
find_path(NVML_INCLUDE_DIR NAMES nvml.h)

# Library
find_library(NVML_LIBRARY
    NAMES nvidia-ml
    PATH_SUFFIXES stubs)

# Set variables
set(NVML_PROCESS_INCLUDES NVML_INCLUDE_DIR)
set(NVML_PROCESS_LIBS NVML_LIBRARY)
libfind_process(NVML)
