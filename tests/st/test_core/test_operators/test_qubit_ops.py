#   Copyright (c) 2020 Huawei Technologies Co.,ltd.
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   You may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

# pylint: disable=invalid-name

"""The test function for QubitOperator."""

from mindquantum.core import ParameterResolver
from mindquantum.core.operators import QubitOperator


def _get_terms_as_set(qubit_op):
    return {s.strip() for s in str(qubit_op).split('+')}


def test_qubit_ops_num_coeff():
    """
    Description: Test qubit ops num coeff
    Expectation:
    """
    q1 = QubitOperator('Z1 Z2') + QubitOperator('X1')
    assert _get_terms_as_set(q1) == {'1 [Z1 Z2]', '1 [X1]'}
    q5 = QubitOperator('X1') * QubitOperator('Y1')
    assert _get_terms_as_set(q5) == {'(1j) [Z1]'}
    q6 = QubitOperator('Y1') * QubitOperator('Z1')
    assert _get_terms_as_set(q6) == {'(1j) [X1]'}
    q7 = QubitOperator('Z1') * QubitOperator('X1')
    assert _get_terms_as_set(q7) == {'(1j) [Y1]'}

    q8 = QubitOperator('Z1 z2')
    q8 *= 2
    assert _get_terms_as_set(q8) == {'2 [Z1 Z2]'}

    q9 = QubitOperator('Z1 z2')
    q10 = QubitOperator('X1 X2')
    q9 = q9 * q10
    assert _get_terms_as_set(q9) == {'-1 [Y1 Y2]'}

    q9 = q9 * 2
    assert _get_terms_as_set(q9) == {'-2 [Y1 Y2]'}

    q9 = 2 * q9
    assert _get_terms_as_set(q9) == {'-4 [Y1 Y2]'}

    q9 = q9 / 2.0
    assert _get_terms_as_set(q9) == {'-2 [Y1 Y2]'}

    q10 = QubitOperator('Z1 z2')
    q10 *= q10
    assert _get_terms_as_set(q10) == {'1 []'}

    q11 = QubitOperator('Z1 z2') + 1e-9 * QubitOperator('X1 z2')
    assert _get_terms_as_set(q11.compress()) == {'1 [Z1 Z2]'}

    q12 = QubitOperator('Z1 Z2') + 1e-4 * QubitOperator('X1 Z2')
    q13 = QubitOperator('Z3 X2') + 1e-5 * QubitOperator('X1 Y2')
    q14 = q12 * q13
    assert _get_terms_as_set(q14) == {
        '(1/10000j) [X1 Y2 Z3]',
        '1/100000 [Y1 X2]',
        '(1j) [Z1 Y2 Z3]',
        '(-1/1000000000j) [X2]',
    }
    assert _get_terms_as_set(q14.compress()) == {'(1/10000j) [X1 Y2 Z3]', '1/100000 [Y1 X2]', '(1j) [Z1 Y2 Z3]'}

    iden = QubitOperator('')
    assert _get_terms_as_set(iden) == {'1 []'}

    zero_op = QubitOperator()
    assert _get_terms_as_set(zero_op) == {'0'}

    iden = -QubitOperator('')
    assert _get_terms_as_set(iden) == {'-1 []'}

    ham = QubitOperator('X0 Y3', 0.5) + 0.6 * QubitOperator('X0 Y3')
    assert _get_terms_as_set(ham) == {'1.1 [X0 Y3]'}


def test_qubit_ops_symbol_coeff():
    """
    Description: Test ops symbol coeff
    Expectation:
    """
    q1 = QubitOperator('Z1 Z2', 'a') + QubitOperator('X1', 'b')
    assert _get_terms_as_set(q1) == {'b [X1]', 'a [Z1 Z2]'}

    q2 = QubitOperator('Z1 Z2', 'a') + ParameterResolver('a') * QubitOperator('Z2 Z1')
    assert _get_terms_as_set(q2) == {'2*a [Z1 Z2]'}

    q8 = QubitOperator('Z1 z2')
    q8 *= ParameterResolver('a')
    assert _get_terms_as_set(q8) == {'a [Z1 Z2]'}

    q9 = QubitOperator('Z1 z2')
    q10 = QubitOperator('X1 X2', 'a')
    q9 = q9 * q10
    assert _get_terms_as_set(q9) == {'-a [Y1 Y2]'}

    q9 = q9 / 2.0
    assert _get_terms_as_set(q9) == {'-1/2*a [Y1 Y2]'}

    q12 = QubitOperator('Z1 Z2') + 1e-4 * QubitOperator('X1 Z2')
    q13 = QubitOperator('Z3 X2') + 1e-5 * QubitOperator('X1 Y2', 'b')
    q14 = q12 * q13

    ref_terms = {
        '(1/10000j) [X1 Y2 Z3]',
        '1/100000*b [Y1 X2]',
        '(1j) [Z1 Y2 Z3]',
    }

    # NB: We should really *NOT* have tests like this...
    #     But since the *= operator for ParameterResolver<T> is not symmetric, there can be some discrepancies like this
    #     one if the coefficients are small enough
    terms_a = ref_terms | {'(-1/1000000000j)*b [X2]'}
    terms_b = ref_terms | {'1/1000000000*b [X2]'}
    assert _get_terms_as_set(q14) == terms_a or _get_terms_as_set(q14) == terms_b

    assert _get_terms_as_set(q14.compress()) == _get_terms_as_set(q14)

    ham = QubitOperator('X0 Y3', 'a') + ParameterResolver('a') * QubitOperator('X0 Y3')
    assert str(ham).strip() == '2*a [X0 Y3]'
    assert ham == QubitOperator('X0 Y3', {'a': 2})


def test_qubit_ops_subs():
    """
    Description: Test ops sub
    Expectation:
    """
    q = QubitOperator('X0', 'b') + QubitOperator('X0', 'a')
    q = q.subs({'a': 1, 'b': 2})
    assert str(q) == '3 [X0] '


def test_qubit_ops_sub():
    """
    Description: Test ops sub
    Expectation:
    """
    q1 = QubitOperator('X0')
    q2 = QubitOperator('Y0')
    assert str(q1 - q2) == '1 [X0] +\n-1 [Y0] '


def test_fermion_operator_iter():
    """
    Description: Test fermion operator iter
    Expectation: success.
    """
    a = QubitOperator('X0 Y1') + QubitOperator('Z2 X3', {"a": -3})
    assert a == sum(list(a))
    b = QubitOperator("X0 Y1")
    b_exp = [QubitOperator("X0"), QubitOperator("Y1")]
    for idx, o in enumerate(b.singlet()):
        assert o == b_exp[idx]
    assert b.singlet_coeff() == 1
    assert b.is_singlet


def test_qubit_ops_dumps_and_loads():
    """
    Description: Test qubit operator dumps to json and json loads to qubit operator
    Expectation:
    """
    ops = QubitOperator('X0 Y1', 1.2) + QubitOperator('Z0 X1', {'a': 2.1})
    strings = ops.dumps()
    obj = QubitOperator.loads(strings, dtype=float)
    assert obj == ops


def test_qubit_ops_trans():
    """
    Description: Test transfor fermion operator to openfermion back and force.
    Expectation: success.
    """
    # pylint: disable=import-outside-toplevel
    from openfermion import QubitOperator as OFQubitOperator

    ofo_ops = OFQubitOperator("X0 Y1 Z2", 1)
    mq_ops = QubitOperator("X0 Y1 Z2", 1)

    assert mq_ops.to_openfermion() == ofo_ops
    assert mq_ops == QubitOperator.from_openfermion(ofo_ops, dtype=complex)
