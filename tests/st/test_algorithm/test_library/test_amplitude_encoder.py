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
'''test for amplitude encoder'''

import warnings

import pytest

from mindquantum.simulator import get_supported_simulator

with warnings.catch_warnings():
    warnings.filterwarnings('ignore', category=UserWarning, message='MindSpore not installed.*')
    warnings.filterwarnings(
        'ignore', category=DeprecationWarning, message=r'Please use `OptimizeResult` from the `scipy\.optimize`'
    )

    from mindquantum.algorithm.library import amplitude_encoder
    from mindquantum.simulator import Simulator


@pytest.mark.parametrize('backend', get_supported_simulator())
def test_amplitude_encoder(backend):
    '''
    Feature: amplitude_encoder
    Description: test amplitude encoder.
    Expectation: success.
    '''
    sim = Simulator(backend, 3)
    circuit, params = amplitude_encoder([0.5, 0.5, 0.5, 0.5], 3)
    sim.apply_circuit(circuit, params)
    state = sim.get_qs(False)
    assert abs(state[0].real - 0.5) < 1e-10
    assert abs(state[1].real - 0.5) < 1e-10
    assert abs(state[2].real - 0.5) < 1e-10
    assert abs(state[3].real - 0.5) < 1e-10
    circuit, params = amplitude_encoder([0, 0, 0.5, 0.5, 0.5, 0.5], 3)
    sim.reset()
    sim.apply_circuit(circuit, params)
    state = sim.get_qs(False)
    assert abs(state[2].real - 0.5) < 1e-10
    assert abs(state[3].real - 0.5) < 1e-10
    assert abs(state[4].real - 0.5) < 1e-10
    assert abs(state[5].real - 0.5) < 1e-10
    circuit, params = amplitude_encoder([0.5, -0.5, 0.5, 0.5], 3)
    sim.reset()
    sim.apply_circuit(circuit, params)
    state = sim.get_qs(False)
    assert abs(state[0].real - 0.5) < 1e-10
    assert abs(state[1].real + 0.5) < 1e-10
    assert abs(state[2].real - 0.5) < 1e-10
    assert abs(state[3].real - 0.5) < 1e-10
