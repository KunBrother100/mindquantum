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

"""Test QNN operations."""

import numpy as np
import pytest

_HAS_MINDSPORE = True
try:
    import mindspore as ms

    from mindquantum.core import gates as G
    from mindquantum.core.circuit import Circuit
    from mindquantum.core.operators import Hamiltonian, QubitOperator
    from mindquantum.framework import MQAnsatzOnlyOps
    from mindquantum.simulator import Simulator, get_supported_simulator

    ms.context.set_context(mode=ms.context.PYNATIVE_MODE, device_target="CPU")
except ImportError:
    _HAS_MINDSPORE = False

    def get_supported_simulator():
        """Dummy function."""
        return []


@pytest.mark.parametrize('backend', get_supported_simulator())
@pytest.mark.skipif(not _HAS_MINDSPORE, reason='MindSpore is not installed')
def test_mindquantum_ansatz_only_ops(backend):
    """
    Description: Test MQAnsatzOnlyOps
    Expectation:
    """
    circ = Circuit(G.RX('a').on(0))
    data = ms.Tensor(np.array([0.5]).astype(np.float32))
    ham = Hamiltonian(QubitOperator('Z0'))
    sim = Simulator(backend, circ.n_qubits)

    evol = MQAnsatzOnlyOps(sim.get_expectation_with_grad(ham, circ))
    output = evol(data)
    assert np.allclose(output.asnumpy(), [[8.77582550e-01]])
