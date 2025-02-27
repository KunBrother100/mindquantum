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
"""Test channel."""
import numpy as np
import pytest

import mindquantum.core.gates.channel as C
from mindquantum.core.gates import X
from mindquantum.simulator import Simulator, get_supported_simulator


@pytest.mark.parametrize('backend', get_supported_simulator())
def test_pauli_channel(backend):
    """
    Description: Test pauli channel
    Expectation: success.
    """
    sim = Simulator(backend, 1)
    sim.apply_gate(C.PauliChannel(1, 0, 0).on(0))
    sim.apply_gate(C.PauliChannel(0, 0, 1).on(0))
    sim.apply_gate(C.PauliChannel(0, 1, 0).on(0))
    assert np.allclose(sim.get_qs(), np.array([0.0 + 1.0j, 0.0 + 0.0j]))


@pytest.mark.parametrize('backend', get_supported_simulator())
def test_flip_channel(backend):
    """
    Description: Test flip channel
    Expectation: success.
    """
    sim1 = Simulator(backend, 1)
    sim1.apply_gate(C.BitFlipChannel(1).on(0))
    assert np.allclose(sim1.get_qs(), np.array([0.0 + 0.0j, 1.0 + 0.0j]))
    sim1.apply_gate(C.PhaseFlipChannel(1).on(0))
    assert np.allclose(sim1.get_qs(), np.array([0.0 + 0.0j, -1.0 + 0.0j]))
    sim1.apply_gate(C.BitPhaseFlipChannel(1).on(0))
    assert np.allclose(sim1.get_qs(), np.array([0.0 + 1.0j, 0.0 + 0.0j]))


@pytest.mark.parametrize('backend', get_supported_simulator())
def test_depolarizing_channel(backend):
    """
    Description: Test depolarizing channel
    Expectation: success.
    """
    sim2 = Simulator(backend, 1)
    sim2.apply_gate(C.DepolarizingChannel(0).on(0))
    assert np.allclose(sim2.get_qs(), np.array([1.0 + 0.0j, 0.0 + 0.0j]))


@pytest.mark.parametrize('backend', get_supported_simulator())
def test_damping_channel(backend):
    """
    Description: Test damping channel
    Expectation: success.
    """
    sim = Simulator(backend, 2)
    sim.apply_gate(X.on(0))
    sim.apply_gate(X.on(1))
    sim.apply_gate(C.AmplitudeDampingChannel(1).on(0))
    assert np.allclose(sim.get_qs(), np.array([0, 0, 1, 0]))
    sim2 = Simulator(backend, 2)
    sim2.apply_gate(X.on(0))
    sim2.apply_gate(X.on(1))
    sim2.apply_gate(C.PhaseDampingChannel(0.5).on(0))
    assert np.allclose(sim2.get_qs(), np.array([0, 0, 0, 1]))


@pytest.mark.parametrize('backend', get_supported_simulator())
def test_kraus_channel(backend):
    """
    Description: Test kraus channel
    Expectation: success.
    """
    kmat0 = [[1, 0], [0, 0]]
    kmat1 = [[0, 1], [0, 0]]
    kraus = C.KrausChannel("amplitude_damping", [kmat0, kmat1])
    sim = Simulator(backend, 2)
    sim.apply_gate(X.on(0))
    sim.apply_gate(X.on(1))
    sim.apply_gate(kraus.on(0))
    assert np.allclose(sim.get_qs(), np.array([0, 0, 1, 0]))
