#!/bin/bash
# Copyright 2021 Huawei Technologies Co., Ltd
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# shellcheck disable=SC2154,SC2034

BASEPATH=$( cd -- "$( dirname -- "${BASH_SOURCE[0]:-$0}" )" &> /dev/null && pwd )
ROOTDIR="$BASEPATH"
PROGRAM=$(basename "${BASH_SOURCE[0]:-$0}")

echo "Called with: $*"

# Test for MindSpore CI
_IS_MINDSPORE_CI=0
if [[ "${JENKINS_URL:-0}" =~ https?://build.mindspore.cn && ! "${CI:-0}" =~ ^(false|0)$ ]]; then
    echo "Detected MindSpore/MindQuantum CI"
    _IS_MINDSPORE_CI=1
fi

# ==============================================================================

# Load common bash helper functions
. "$ROOTDIR/scripts/build/common_functions.sh"

check_for_verbose "$@"

# ------------------------------------------------------------------------------
# Default values for this particular script

python_extra_pkgs=('wheel-filename>1.2' 'build')

if [ "$_IS_MINDSPORE_CI" -eq 1 ]; then
    for var in CUDA_HOME CUDA_PATH; do
        if [ -n "${!var}" ]; then
            echo "$var = ${!var}"
            if [ ! -d "${!var}" ]; then
                print_warning "$var is set, but location does not exist!"
            else
                echo "Adding $var/lib64 and $var/lib to LD_LIBRARY_PATH"
                if [ -z "$LD_LIBRARY_PATH" ]; then
                    export LD_LIBRARY_PATH="${!var}/lib64:${!var}/lib"
                else
                    export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:${!var}/lib64:${!var}/lib"
                fi
            fi
        fi
    done

    echo '----------------------------------------'
    echo 'Environment info'
    echo "PATH = $PATH"
    echo "LD_LIBRARY_PATH = $LD_LIBRARY_PATH"
    echo '----------------------------------------'

    if [[ "$(uname)" == "Linux" ]]; then
        PY_EXEC="$(which python)"
        echo '----------------------------------------'
        echo "PY_EXEC = $PY_EXEC"
        echo '----------------------------------------'
        echo 'System Python environment'
        "$PY_EXEC" -m pip freeze
        echo '----------------------------------------'
        echo 'Force update the system pybind11'
        sudo LD_LIBRARY_PATH="$LD_LIBRARY_PATH" "$PY_EXEC" -m pip install -U pybind11
        "$PY_EXEC" -m pip show pybind11
        echo '----------------------------------------'
    fi

    verbose=1
    set_var cmake_debug_mode true
    set_var do_clean_3rdparty true
    set_var enable_gitee true
    set_var enable_gpu true
    set_var enable_projectq true
fi

# ==============================================================================

function help_header() {
    echo 'Build binary Python wheel for MindQunantum'
    echo ''
    echo 'This is mainly relevant for developers that want to deploy MindQuantum '
    echo 'on machines other than their own.'
    echo ''
    echo 'This script will create a Python virtualenv in the MindQuantum root'
    echo 'directory and then build a binary Python wheel of MindQuantum.'
}

function extra_help() {
    echo 'Extra options:'
    echo '  --(no-)build-isolation Pass --no-isolation to python3 -m build'
    echo '  --(no-)delocate        Delocate the binary wheels after build is finished'
    echo '                         (enabled by default; pass --no-delocate to disable)'
    echo '  --(no-)fast-build      If possible use an existing CMake directory to build the C++ Python extensions'
    echo '                         instead of using the normal Python bdist_wheel process. Assumes that '
    echo '                         IN_PLACE_BUILD=ON'
    echo '                         Use this with caution. CI build should not be using this.'
    echo '  --fast-build-dir       Specify build directory when performing a fast-build'
    echo '  -o,--output=[dir]      Output directory for built wheels'
    echo '  -p,--plat-name=[dir]   Platform name to use for wheel delocation'
    echo '                         (only effective if --delocate is used)'
    echo -e '\nExample calls:'
    echo "$PROGRAM"
    echo "$PROGRAM --gpu"
    echo "$PROGRAM --cxx --with-boost --without-gmp --venv=/tmp/venv"
}

# ==============================================================================

getopts_args_extra='o:p:'

function parse_extra_args() {
    # input args: OPT OPTARG flag_value
    case "$1" in
        fast-build )         no_arg;
                             set_var fast_build "$3"
                             ;;
        fast-build-dir )     needs_arg;
                             set_var fast_build_dir "$2"
                             ;;
        delocate )           no_arg;
                             set_var delocate_wheel "$3"
                             ;;
        build-isolation )    no_arg;
                             set_var build_isolation "$3"
                             ;;
        o | output )         needs_arg;
                             set_var output_path "$2"
                             ;;
        p | plat-name )      needs_arg;
                             set_var platform_name "$2"
                             ;;
        ??* )                return 2  # Delegate error handling to main argument parsing function
                             ;;
    esac
}


# NB: using the default values from parse_common_args.sh
. "$ROOTDIR/scripts/build/parse_common_args.sh"

# ------------------------------------------------------------------------------

# Locate python or python3
. "$ROOTDIR/scripts/build/locate_python3.sh"

# ==============================================================================

set -e

cd "${ROOTDIR}"

# ------------------------------------------------------------------------------

# NB: `created_venv` variable can be used to detect if a virtualenv was created or not
. "$ROOTDIR/scripts/build/python_virtualenv_activate.sh"

# ------------------------------------------------------------------------------
# Locate cmake or cmake3

# NB: `cmake_from_venv` variable is set by this script (and is used by python_virtualenv_update.sh)
. "$ROOTDIR/scripts/build/locate_cmake.sh"

# ------------------------------------------------------------------------------
# Locate ninja if needed

if [ "$cmake_generator" == "Ninja" ]; then
    # NB: `ninja_from_venv` variable is set by this script (and is used by python_virtualenv_update.sh)
    . "$ROOTDIR/scripts/build/locate_ninja.sh"
fi
# -----------------------------------------------------------------------------------------
# Locate patchelf

if [[ "$delocate_wheel" -eq 1 && "$(uname -s)" == "Linux" ]]; then
    if ! command -v patchelf > /dev/null 2>&1; then
        echo "Installing patchelf inside the Python virtual environment"
        call_cmd "$PYTHON" -m pip install patchelf
    fi
fi

# ------------------------------------------------------------------------------
# Update Python virtualenv (if requested/necessary)

. "$ROOTDIR/scripts/build/python_virtualenv_update.sh"

# ------------------------------------------------------------------------------
# Setup arguments for build

args=()

declare_AA cmake_option_names
set_AA cmake_option_names cmake_debug_mode ENABLE_CMAKE_DEBUG
set_AA cmake_option_names do_clean_3rdparty CLEAN_3RDPARTY_INSTALL_DIR
set_AA cmake_option_names enable_cxx ENABLE_CXX_EXPERIMENTAL
set_AA cmake_option_names enable_gitee ENABLE_GITEE
set_AA cmake_option_names enable_gpu ENABLE_CUDA
set_AA cmake_option_names enable_logging ENABLE_LOGGING
set_AA cmake_option_names enable_projectq ENABLE_PROJECTQ
set_AA cmake_option_names enable_tests BUILD_TESTING
set_AA cmake_option_names logging_enable_debug ENABLE_LOGGING_DEBUG_LEVEL
set_AA cmake_option_names logging_enable_trace ENABLE_LOGGING_TRACE_LEVEL

for var in $(get_AA_keys cmake_option_names); do
    if [ "${!var}" -eq 1 ]; then
        args+=(--set "$(get_AA_value cmake_option_names "$var")")
    else
        args+=(--unset "$(get_AA_value cmake_option_names "$var")")
    fi
done

if [ "$_IS_MINDSPORE_CI" -eq 1 ]; then
    args+=(--set MINDSPORE_CI)
fi

if [ "$cmake_make_silent" -eq 0 ]; then
    args+=(--set USE_VERBOSE_MAKEFILE)
else
    args+=(--unset USE_VERBOSE_MAKEFILE)
fi

if [ "$cmake_no_registry" -eq 1 ]; then
    args+=(--unset CMAKE_FIND_USE_PACKAGE_REGISTRY)
    args+=(--unset CMAKE_FIND_PACKAGE_NO_SYSTEM_PACKAGE_REGISTRY)
else
    args+=(--set CMAKE_FIND_USE_PACKAGE_REGISTRY)
    args+=(--set CMAKE_FIND_PACKAGE_NO_SYSTEM_PACKAGE_REGISTRY)
fi

if [ -n "$cmake_generator" ]; then
    args+=(-G "${cmake_generator}")
fi

if [ "$fast_build" -eq 1 ]; then
    args+=(bdist_wheel --fast-build)

    if [ -n "$fast_build_dir" ]; then
        args+=(bdist_wheel --fast-build-dir="$fast_build_dir")
    fi
fi

if [ "$n_jobs" -ne -1 ]; then
    args+=(--var JOBS "$n_jobs")
    args+=(build_ext --jobs="$n_jobs")
fi

if [[ "$build_type" == 'Debug' ]]; then
    args+=(build --debug)
fi

if [ "${_build_dir_was_set:-0}" -eq 1 ]; then
    args+=(build_ext --build-dir "$build_dir")
fi

local_pkgs_str=$(join_by , "${local_pkgs[@]}")
if [[ "$force_local_pkgs" -eq 1 ]]; then
    args+=(--var MQ_FORCE_LOCAL_PKGS all)
elif [ -n "$local_pkgs_str" ]; then
    args+=(--var MQ_FORCE_LOCAL_PKGS "$local_pkgs_str")
fi

# --------------------------------------

if [[ "$enable_gpu" -eq 1 && -n "$cuda_arch" ]]; then
    print_warning "--cuda-arch is unsupported (thus ignored) with $PROGRAM!"
fi

if [ "$enable_ccache" -eq 1 ]; then
    ccache_exec=
    if command -v ccache > /dev/null 2>&1; then
        ccache_exec=ccache
    elif command -v sccache > /dev/null 2>&1; then
        ccache_exec=sccache
    fi
    if [ -n "$ccache_exec" ]; then
        ccache_exec=$(which "$ccache_exec")
        args+=(--var CMAKE_C_COMPILER_LAUNCHER "$ccache_exec")
        args+=(--var CMAKE_CXX_COMPILER_LAUNCHER "$ccache_exec")
        if [ "$enable_gpu" -eq 1 ]; then
            args+=(--var CMAKE_CUDA_COMPILER_LAUNCHER "$ccache_exec")
        fi
    fi
fi

# NB: CMake < 3.24 typically set CC, CXX during the first run, which basically overwrites the values in CC, CXX. In
#     order to work around that, we explicitly set the compilers using the related CMake variables.
if [ -n "$CC" ]; then
    args+=(--var CMAKE_C_COMPILER "$CC")
fi
if [ -n "$CXX" ]; then
    args+=(--var CMAKE_CXX_COMPILER "$CXX")
fi
if [ -n "$CUDACXX" ]; then
    args+=(--var CMAKE_CUDA_COMPILER "$CUDACXX")
fi

debug_print "Will be passing these arguments to setup.py:"
debug_print "    ${args[*]}"

# ==============================================================================

if [ "$enable_gpu" -eq 1 ]; then
    # Older CMake using find_package(CUDA) would rely on CUDA_HOME, but newer CMake only look at CUDACXX and CUDA_PATH
    if [[ -n "$CUDA_HOME" && -z "$CUDA_PATH" ]]; then
        echo 'CUDA_HOME is defined, but CUDA_PATH is not. Setting CUDA_PATH=CUDA_HOME'
        export CUDA_PATH="$CUDA_HOME"
    fi

    debug_print "CUDA_PATH = $CUDA_PATH"
fi

# ==============================================================================

# Convert the CMake arguments for passing them using -C to python3 -m build
fixed_args=()
for arg in "${args[@]}"; do
    fixed_args+=("-C--global-option=$arg")
done

args=("-w")
if [ "$build_isolation" -eq 0 ]; then
    args+=("--no-isolation")
fi

# ------------------------------------------------------------------------------
# Build the wheels

if [ "${_build_dir_was_set:-0}" -eq 1 ]; then
    if [ "$do_clean_build_dir" -eq 1 ]; then
        echo "Deleting build folder: $build_dir"
        call_cmd rm -rf "$build_dir"
    elif [ "$do_clean_cache" -eq 1 ]; then
        echo "Removing CMake cache at: $build_dir/CMakeCache.txt"
        call_cmd rm -f "$build_dir/CMakeCache.txt"
        echo "Removing CMake files at: $build_dir/CMakeFiles"
        call_cmd  rm -rf "$build_dir/CMakeFiles"
        echo "Removing CMake files at: $build_dir/cmake-ldtest*"
        call_cmd  rm -rf "$build_dir/cmake-ldtest*"
    fi
fi

if [ "$delocate_wheel" -eq 1 ]; then
    env_vars=(MQ_DELOCATE_WHEEL=1 LD_LIBRARY_PATH="$LD_LIBRARY_PATH")

    if [ -n "$platform_name" ]; then
        env_vars+=(MQ_DELOCATE_WHEEL_PLAT="$platform_name")
    fi
    call_cmd env "${env_vars[@]}" "${PYTHON}" -m build "${args[@]}" "${fixed_args[@]}" "$@"
else
    call_cmd "${PYTHON}" -m build "${args[@]}" "${fixed_args[@]}" "$@"
fi

# ------------------------------------------------------------------------------
# Move the wheels to the output directory

if [[ -d "${output_path}" ]];then
    call_cmd rm -rf "${output_path}"
fi

call_cmd mkdir -pv "${output_path}"

call_cmd mv -v "$ROOTDIR/dist/"*whl* "${output_path}"

if [[ "$_IS_MINDSPORE_CI" -eq 1 && "$(uname)" == 'Linux' ]]; then
    suffix="-manylinux_2_27_$(uname -m)"
    for ext in ".whl" ".whl.sha256"; do
        for wheel in "${output_path}"/*"${suffix}${ext}"; do
            call_cmd cp -v "$wheel" "${wheel%"${suffix}${ext}"}-linux_$(uname -m)${ext}"
        done
    done
fi

echo "------Successfully created mindquantum package------"
