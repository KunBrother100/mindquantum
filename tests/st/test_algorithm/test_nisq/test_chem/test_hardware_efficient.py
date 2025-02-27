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
"""Test hardware efficient ansatz"""

import os

import numpy as np
import pytest

_HAS_MINDSPORE = True
try:
    import mindspore as ms

    from mindquantum.algorithm.nisq import HardwareEfficientAnsatz
    from mindquantum.core.gates import RX, RY, X
    from mindquantum.core.operators import Hamiltonian, QubitOperator
    from mindquantum.framework import MQAnsatzOnlyLayer
    from mindquantum.simulator import Simulator, get_supported_simulator

    ms.context.set_context(mode=ms.context.PYNATIVE_MODE, device_target="CPU")
except ImportError:
    _HAS_MINDSPORE = False

    def get_supported_simulator():
        """Dummy function."""
        return []


os.environ.setdefault('OMP_NUM_THREADS', '8')


@pytest.mark.parametrize('backend', get_supported_simulator())
@pytest.mark.skipif(not _HAS_MINDSPORE, reason='MindSpore is not installed')
def test_hardware_efficient(backend):
    """
    Description: Test hardware efficient ansatz
    Expectation:
    """
    depth = 3
    n_qubits = 3
    hea = HardwareEfficientAnsatz(n_qubits, [RX, RY, RX], X, 'all', depth)
    ham = QubitOperator('Z0 Z1 Z2')
    sim = Simulator(backend, hea.circuit.n_qubits)
    f_g_ops = sim.get_expectation_with_grad(Hamiltonian(ham), hea.circuit)
    ms.set_seed(42)
    net = MQAnsatzOnlyLayer(f_g_ops)
    opti = ms.nn.Adagrad(net.trainable_params(), learning_rate=4e-1)
    train_net = ms.nn.TrainOneStepCell(net, opti)
    for _ in range(3):
        res = train_net().asnumpy()[0]
    assert np.allclose(round(res, 4), -0.7588)
