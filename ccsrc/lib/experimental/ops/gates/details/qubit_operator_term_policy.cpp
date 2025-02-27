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

#include "experimental/ops/gates/details/qubit_operator_term_policy.hpp"

#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/at_c.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include <boost/spirit/home/x3.hpp>

#include "config/logging.hpp"

#include "boost_x3_get_info_impl.hpp"
#include "boost_x3_parse_object.hpp"

#include "experimental/core/parser/boost_x3_error_handler.hpp"
#include "experimental/ops/gates/qubit_operator.hpp"

// -----------------------------------------------------------------------------

using namespace std::literals::string_literals;  // NOLINT(build/namespaces_literals)

// =============================================================================

namespace x3 = boost::spirit::x3;

namespace ast::qb_op {
using mindquantum::ops::TermValue;
using term_t = mindquantum::ops::term_t;
using terms_t = mindquantum::ops::terms_t;

struct TermOp : x3::symbols<TermValue> {
    TermOp() {
        add("X", TermValue::X)("x", TermValue::X)("Y", TermValue::Y)("y", TermValue::Y)("Z", TermValue::Z)(
            "z", TermValue::Z);
    }
} const term_op;
}  // namespace ast::qb_op

// -----------------------------------------------------------------------------

namespace parser::qb_op {
namespace ast = ::ast::qb_op;

struct term_class : mindquantum::parser::x3::rule::error_handler {};
const x3::rule<term_class, ast::term_t> term = "QubitOperator term (ie. [XYZ][0-9]+)";
static const auto term_def = x3::lexeme[((ast::term_op > x3::uint_)[([](auto& ctx) {  // NOLINT(runtime/references)
    x3::_val(ctx) = std::make_pair(boost::fusion::at_c<1>(x3::_attr(ctx)), boost::fusion::at_c<0>(x3::_attr(ctx)));
})])];

struct terms_class : mindquantum::parser::x3::rule::error_handler {};
const x3::rule<terms_class, ast::terms_t> terms = "QubitOperator terms list";
/* NB: Simply using '+term' will not work here since we have to reject cases like: 'X1 YY'
 *     So we make sure that everything will match using a look-ahead on each term before actually matching the term.
 */
static const auto terms_def = +(&term >> term);

// -------------------------------------

BOOST_SPIRIT_DEFINE(term, terms);
}  // namespace parser::qb_op

// -------------------------------------

namespace boost::spirit::x3 {
template <>
struct get_info<ast::qb_op::TermOp> {
    using result_type = std::string;
    result_type operator()(const ast::qb_op::TermOp& /* type */) const noexcept {
        using std::literals::string_literals::operator""s;
        return "local operator (X, Y, Z)"s;
    }
};
}  // namespace boost::spirit::x3

// =============================================================================

namespace mindquantum::ops::details {
auto QubitOperatorTermPolicyBase::hermitian(term_t term) -> term_t {
    return term;
}

auto QubitOperatorTermPolicyBase::hermitian(const terms_t& terms) -> terms_t {
    return terms;
}

auto QubitOperatorTermPolicyBase::to_string(const term_t& term) -> std::string {
    return fmt::format("{}{}", to_string(std::get<1>(term)), std::get<0>(term));
}

auto QubitOperatorTermPolicyBase::parse_terms_string(std::string_view terms_string) -> terms_t {
    MQ_INFO("Attempting to parse: '{}'", terms_string);
    if (terms_t terms; parser::parse_object_skipper(begin(terms_string), end(terms_string), terms,
                                                    ::parser::qb_op::terms, x3::space)) {
        return terms;
    }
    MQ_ERROR("QubitOperator terms string parsing failed for '{}'", terms_string);
    return {};
}

// =============================================================================
}  // namespace mindquantum::ops::details
