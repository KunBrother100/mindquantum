//   Copyright 2022 <Huawei Technologies Co., Ltd>
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.

#ifndef MQ_CONFIG_TRAITS_HPP
#define MQ_CONFIG_TRAITS_HPP

#include <complex>
#include <tuple>
#include <type_traits>

#include "config/config.hpp"

#include "details/cxx20_compatibility.hpp"

namespace mindquantum::traits {
template <typename T, typename U>
inline constexpr auto is_same_decay_v = std::is_same_v<std::remove_cvref_t<T>, std::remove_cvref_t<U>>;

// =============================================================================

template <typename T>
struct is_tuple : std::false_type {};

template <typename... Ts>
struct is_tuple<std::tuple<Ts...>> : std::true_type {};

template <typename T>
inline constexpr auto is_tuple_v = is_tuple<T>::value;

template <typename T>
inline constexpr auto is_tuple_decay_v = is_tuple_v<std::remove_cvref_t<T>>;

// =============================================================================

template <typename T>
struct is_std_complex : std::false_type {};

template <typename T>
struct is_std_complex<std::complex<T>> : std::true_type {};

template <typename T>
inline constexpr auto is_std_complex_v = is_std_complex<T>::value;

template <typename T>
inline constexpr auto is_std_complex_decay_v = is_std_complex_v<std::remove_cvref_t<T>>;

// -----------------------------------------------------------------------------

template <typename T>
struct is_complex : is_std_complex<T> {};

template <typename T>
inline constexpr auto is_complex_v = is_complex<T>::value;

template <typename T>
inline constexpr auto is_complex_decay_v = is_complex_v<std::remove_cvref_t<T>>;

// =============================================================================

template <typename T>
struct is_scalar
    : std::conditional_t<std::is_arithmetic_v<T> || is_std_complex_v<T>, std::true_type, std::false_type> {};

template <typename T>
inline constexpr auto is_scalar_v = is_scalar<T>::value;

template <typename T>
inline constexpr auto is_scalar_decay_v = is_scalar_v<std::remove_cvref_t<T>>;

// =============================================================================

template <typename float_t, typename = void>
struct to_cmplx_type;

template <typename float_t>
struct to_cmplx_type<float_t, std::enable_if_t<std::is_floating_point_v<float_t>>> {
    using type = std::complex<float_t>;
};

template <typename float_t>
struct to_cmplx_type<std::complex<float_t>> {
    using type = std::complex<float_t>;
};

template <typename float_t>
using to_cmplx_type_t = typename to_cmplx_type<float_t>::type;

// -----------------------------------------------------------------------------

template <typename float_t, typename = void>
struct to_real_type;

template <typename float_t>
struct to_real_type<float_t, std::enable_if_t<std::is_floating_point_v<float_t>>> {
    using type = float_t;
};

template <typename float_t>
struct to_real_type<std::complex<float_t>> {
    using type = float_t;
};

template <typename float_t>
using to_real_type_t = typename to_real_type<float_t>::type;

// -----------------------------------------------------------------------------

static_assert(std::is_same_v<std::complex<double>, to_cmplx_type_t<double>>);
static_assert(std::is_same_v<std::complex<double>, to_cmplx_type_t<std::complex<double>>>);
static_assert(std::is_same_v<double, to_real_type_t<double>>);
static_assert(std::is_same_v<double, to_real_type_t<std::complex<double>>>);

// =============================================================================
}  // namespace mindquantum::traits

#endif /* MQ_CONFIG_TRAITS_HPP */
