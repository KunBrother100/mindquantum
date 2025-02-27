#   Portions Copyright (c) 2020 Huawei Technologies Co.,ltd.
#   Portions Copyright 2017 The OpenFermion Developers.

#   Licensed under the Apache License, Version 2.0 (the "License");
#   You may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#
#   This module we develop is default being licensed under Apache 2.0 license,
#   and also uses or refactor Fermilib and OpenFermion licensed under
#   Apache 2.0 license.

"""This is the module for the Qubit Operator."""

from openfermion import QubitOperator as OFQubitOperator

from ...mqbackend import complex_pr, real_pr
from .. import _mindquantum_cxx as mqcxx
from ..utils import TermValue
from ._terms_operators import TermsOperator

# pylint: disable=no-member

# NB: C++ actually supports FermionOperatorD and FermionOperatorCD that are purely numerical FermionOperator classes

# ==============================================================================


class QubitOperator(TermsOperator):
    """
    A sum of terms acting on qubits, e.g., 0.5 * 'X1 X5' + 0.3 * 'Z1 Z2'.

    A term is an operator acting on n qubits and can be represented as:
    coefficient * local_operator[0] x ... x local_operator[n-1]
    where x is the tensor product. A local operator is a Pauli operator
    ('I', 'X', 'Y', or 'Z') which acts on one qubit. In mathematical notation
    a QubitOperator term is, for example, 0.5 * 'X1 X5', which means that a Pauli X operator acts
    on qubit 1 and 5, while the identity operator acts on all the rest qubits.

    Note that a Hamiltonian composed of QubitOperators should be a hermitian
    operator, thus requires the coefficients of all terms must be real.

    QubitOperator has the following attributes set as follows:
    operators = ('X', 'Y', 'Z'), different_indices_commute = True.

    Args:
        term (str): The input term of qubit operator. Default: None.
        coefficient (Union[numbers.Number, str, ParameterResolver]): The
            coefficient of this qubit operator, could be a number or a variable
            represent by a string or a symbol or a parameter resolver. Default: 1.0.

    Examples:
        >>> from mindquantum.core.operators import QubitOperator
        >>> ham = ((QubitOperator('X0 Y3', 0.5)
        ...         + 0.6 * QubitOperator('X0 Y3')))
        >>> ham2 = QubitOperator('X0 Y3', 0.5)
        >>> ham2 += 0.6 * QubitOperator('X0 Y3')
        >>> ham2
        1.1 [X0 Y3]
        >>> ham3 = QubitOperator('')
        >>> ham3
        1 []
        >>> ham_para = QubitOperator('X0 Y3', 'x')
        >>> ham_para
        x [X0 Y3]
        >>> ham_para.subs({'x':1.2})
        6/5 [X0 Y3]
    """

    # NB: In principle, we support real-valued qubit operators. Unfortunately, in this case, not all coefficient
    #     simplifications are possible.
    #     For now, we simply force any Python code to create complex qubit operators...

    cxx_base_klass = mqcxx.ops.QubitOperatorBase
    real_pr_klass = mqcxx.ops.QubitOperatorPRD
    complex_pr_klass = mqcxx.ops.QubitOperatorPRCD
    openfermion_klass = OFQubitOperator

    ensure_complex_coeff = True

    _type_conversion_table = {
        complex_pr: complex_pr_klass,
        complex: complex_pr_klass,
        real_pr: complex_pr_klass,
        float: complex_pr_klass,
    }

    def __str__(self) -> str:
        """Return string expression of a TermsOperator."""
        out = []
        for terms, coeff in self.terms.items():
            terms_str = ' '.join([f"{TermValue[pauli]}{idx}" for idx, pauli in terms])
            out.append(f"{coeff.expression()} [{terms_str}] ")
        if not out:
            return "0"
        return "+\n".join(out)

    def count_gates(self):
        """
        Return the gate number when treated in single Hamiltonian.

        Returns:
            int, number of the single qubit quantum gates.

        Examples:
            >>> from mindquantum.core.operators import QubitOperator
            >>> a = QubitOperator("X0 Y1") + QubitOperator("X2 Z3")
            >>> a.count_gates()
            4
        """
        return self._cpp_obj.count_gates()


__all__ = ['QubitOperator']
