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

#include "experimental/ops/gates/details/std_complex_coeff_policy.hpp"

#include <complex>
#include <optional>

#include "config/logging.hpp"

#include "boost_x3_complex_number.hpp"
#include "boost_x3_parse_object.hpp"

// ==============================================================================

namespace x3 = boost::spirit::x3;

namespace mindquantum::ops::details {
auto std_complex_from_string(const boost::iterator_range<std::string_view::const_iterator> &range)
    -> std::optional<std::complex<double>> {
    MQ_INFO("Attempting to parse: '{}'", std::string(range.begin(), range.end()));
    if (std::complex<double> coeff;
        mindquantum::parser::parse_object_skipper(range.begin(), range.end(), coeff, parser::complex, x3::space)) {
        return coeff;
    }
    MQ_ERROR("CmplxDoubleCoeffPolicy: parsing failed for '{}'", std::string(range.begin(), range.end()));
    return {};
}
}  // namespace mindquantum::ops::details

// ==============================================================================
