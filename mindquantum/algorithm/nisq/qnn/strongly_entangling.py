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
# ============================================================================
"""Strongly entangling ansatz."""

from mindquantum.core.circuit import add_prefix
from mindquantum.core.gates import BasicGate
from mindquantum.core.circuit import Circuit
from mindquantum.core.gates.basicgate import U3
from mindquantum.utils.type_value_check import (
    _check_int_type,
    _check_value_should_not_less,
)
from .._ansatz import Ansatz


class StronglyEntangling(Ansatz):
    """
    Strongly entangling ansatz, please refers `Circuit-centric quantum
    classifiers <https://arxiv.org/pdf/1804.00633.pdf>`_.

    Args:
        n_qubits (int): number of qubit that this ansatz act on.
        depth (int): the depth of ansatz.
        entangle_gate (BasicGate): a quantum gate to generate entanglement. If a single
            qubit gate is given, a control qubit will add, if a two qubits gate is given,
            the two qubits gate will act on different qubits.

    Examples:
        >>> from mindquantum.core.gates import X
        >>> from mindquantum.algorithm.nisq import StronglyEntangling
        >>> ansatz = StronglyEntangling(3, 2, X)
        >>> ansatz.circuit
    """
    def __init__(self, n_qubits: int, depth: int, entangle_gate: BasicGate):
        """Initialize a strongly entangling ansatz."""
        _check_int_type('n_qubits', n_qubits)
        _check_int_type('depth', depth)
        _check_value_should_not_less('n_qubits', 2, n_qubits)
        _check_value_should_not_less('depth', 1, depth)
        if not isinstance(entangle_gate, BasicGate) or entangle_gate.parameterized:
            raise ValueError(f"entangle gate requires a non parameterized gate, but get {entangle_gate}")
        m_dim = entangle_gate.matrix().shape[0]
        if m_dim == 2:
            self.gate_qubits = 1
        elif m_dim == 4:
            self.gate_qubits = 2
        else:
            raise ValueError("error gate, entangle_gate can only be one qubit or two "
                             f"qubits, but get dimension with {m_dim}")
        super().__init__('Strongly Entangling', n_qubits, depth, entangle_gate)

    def _implement(self, depth, entangle_gate):
        """Implement of strongly entangling ansatz."""
        rot_part_ansatz = Circuit([U3(f'a{i}', f'b{i}', f'c{i}').on(i) for i in range(self.n_qubits)])
        circ = Circuit()
        for l in range(depth):
            circ += add_prefix(rot_part_ansatz, f'l{l}')
            for idx in range(self.n_qubits):
                if self.gate_qubits == 1:
                    circ += entangle_gate.on((idx + l + 1) % self.n_qubits, idx)
                else:
                    circ += entangle_gate.on([(idx + l + 1) % self.n_qubits, idx])
        self._circuit = circ
