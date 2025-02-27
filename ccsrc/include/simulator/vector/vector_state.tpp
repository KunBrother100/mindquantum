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

#ifndef INCLUDE_VECTOR_VECTORSTATE_TPP
#define INCLUDE_VECTOR_VECTORSTATE_TPP

#include <cmath>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include "core/mq_base_types.hpp"
#include "core/parameter_resolver.hpp"
#include "ops/basic_gate.hpp"
#include "ops/gates.hpp"
#include "ops/hamiltonian.hpp"
#include "simulator/types.hpp"
#include "simulator/vector/vector_state.hpp"

namespace mindquantum::sim::vector::detail {

template <typename qs_policy_t_>
VectorState<qs_policy_t_>::VectorState(qbit_t n_qubits, unsigned seed)
    : n_qubits(n_qubits), dim(1UL << n_qubits), seed(seed), rnd_eng_(seed) {
    qs = qs_policy_t::InitState(dim);
    std::uniform_real_distribution<double> dist(0., 1.);
    rng_ = std::bind(dist, std::ref(rnd_eng_));
}

template <typename qs_policy_t_>
VectorState<qs_policy_t_>::VectorState(qbit_t n_qubits, unsigned seed, qs_data_p_t vec)
    : n_qubits(n_qubits), dim(1UL << n_qubits), seed(seed), rnd_eng_(seed) {
    qs = qs_policy_t::Copy(vec, dim);
    std::uniform_real_distribution<double> dist(0., 1.);
    rng_ = std::bind(dist, std::ref(rnd_eng_));
}

template <typename qs_policy_t_>
VectorState<qs_policy_t_>::VectorState(qs_data_p_t qs, qbit_t n_qubits, unsigned seed)
    : qs(qs), n_qubits(n_qubits), dim(1UL << n_qubits), seed(seed), rnd_eng_(seed) {
    std::uniform_real_distribution<double> dist(0., 1.);
    rng_ = std::bind(dist, std::ref(rnd_eng_));
}

template <typename qs_policy_t_>
VectorState<qs_policy_t_>::VectorState(const VectorState<qs_policy_t>& sim) {
    this->qs = qs_policy_t::Copy(sim.qs, sim.dim);
    this->dim = sim.dim;
    this->n_qubits = sim.n_qubits;
    this->seed = sim.seed;
    this->rnd_eng_ = RndEngine(seed);
    std::uniform_real_distribution<double> dist(0., 1.);
    this->rng_ = std::bind(dist, std::ref(this->rnd_eng_));
}

template <typename qs_policy_t_>
auto VectorState<qs_policy_t_>::operator=(const VectorState<qs_policy_t>& sim) -> derived_t& {
    qs_policy_t::FreeState(this->qs);
    this->qs = qs_policy_t::Copy(sim.qs, sim.dim);
    this->dim = sim.dim;
    this->n_qubits = sim.n_qubits;
    this->seed = sim.seed;
    this->rnd_eng_ = RndEngine(seed);
    std::uniform_real_distribution<double> dist(0., 1.);
    this->rng_ = std::bind(dist, std::ref(this->rnd_eng_));
    return *this;
};

template <typename qs_policy_t_>
VectorState<qs_policy_t_>::VectorState(VectorState<qs_policy_t>&& sim) {
    this->qs = sim.qs;
    this->dim = sim.dim;
    this->n_qubits = sim.n_qubits;
    this->seed = sim.seed;
    sim.qs = nullptr;
    this->rnd_eng_ = RndEngine(seed);
    std::uniform_real_distribution<double> dist(0., 1.);
    this->rng_ = std::bind(dist, std::ref(this->rnd_eng_));
}

template <typename qs_policy_t_>
auto VectorState<qs_policy_t_>::operator=(VectorState<qs_policy_t>&& sim) -> derived_t& {
    qs_policy_t::FreeState(this->qs);
    this->qs = sim.qs;
    this->dim = sim.dim;
    this->n_qubits = sim.n_qubits;
    this->seed = sim.seed;
    sim.qs = nullptr;
    this->rnd_eng_ = RndEngine(seed);
    std::uniform_real_distribution<double> dist(0., 1.);
    this->rng_ = std::bind(dist, std::ref(this->rnd_eng_));
    return *this;
}

template <typename qs_policy_t_>
void VectorState<qs_policy_t_>::Reset() {
    qs_policy_t::Reset(qs, dim);
}

template <typename qs_policy_t_>
void VectorState<qs_policy_t_>::Display(qbit_t qubits_limit) const {
    qs_policy_t::Display(qs, n_qubits, qubits_limit);
}

template <typename qs_policy_t_>
auto VectorState<qs_policy_t_>::GetQS() const -> py_qs_datas_t {
    return qs_policy_t::GetQS(qs, dim);
}

template <typename qs_policy_t_>
void VectorState<qs_policy_t_>::SetQS(const py_qs_datas_t& qs_out) {
    qs_policy_t::SetQS(qs, qs_out, dim);
}

template <typename qs_policy_t_>
index_t VectorState<qs_policy_t_>::ApplyGate(const std::shared_ptr<BasicGate<calc_type>>& gate,
                                             const ParameterResolver<calc_type>& pr, bool diff) {
    auto name = gate->name_;
    if (gate->is_custom_) {
        std::remove_reference_t<decltype(*gate)>::matrix_t mat;
        if (!gate->parameterized_) {
            mat = gate->base_matrix_;
        } else {
            calc_type val = gate->params_.Combination(pr).const_value;
            if (!diff) {
                mat = gate->numba_param_matrix_(val);
            } else {
                mat = gate->numba_param_diff_matrix_(val);
            }
        }
        qs_policy_t::ApplyMatrixGate(qs, qs, gate->obj_qubits_, gate->ctrl_qubits_, mat.matrix_, dim);
    } else if (name == gI) {
    } else if (name == gX) {
        qs_policy_t::ApplyX(qs, gate->obj_qubits_, gate->ctrl_qubits_, dim);
    } else if (name == gCNOT) {
        qbits_t obj_qubits = {gate->obj_qubits_[0]};
        qbits_t ctrl_qubits = gate->ctrl_qubits_;
        std::copy(std::begin(gate->obj_qubits_) + 1, std::end(gate->obj_qubits_), std::back_inserter(ctrl_qubits));
        qs_policy_t::ApplyX(qs, obj_qubits, ctrl_qubits, dim);
    } else if (name == gY) {
        qs_policy_t::ApplyY(qs, gate->obj_qubits_, gate->ctrl_qubits_, dim);
    } else if (name == gZ) {
        qs_policy_t::ApplyZ(qs, gate->obj_qubits_, gate->ctrl_qubits_, dim);
    } else if (name == gH) {
        qs_policy_t::ApplyH(qs, gate->obj_qubits_, gate->ctrl_qubits_, dim);
    } else if (name == gS) {
        if (gate->daggered_) {
            qs_policy_t::ApplySdag(qs, gate->obj_qubits_, gate->ctrl_qubits_, dim);
        } else {
            qs_policy_t::ApplySGate(qs, gate->obj_qubits_, gate->ctrl_qubits_, dim);
        }
    } else if (name == gT) {
        if (gate->daggered_) {
            qs_policy_t::ApplyTdag(qs, gate->obj_qubits_, gate->ctrl_qubits_, dim);
        } else {
            qs_policy_t::ApplyT(qs, gate->obj_qubits_, gate->ctrl_qubits_, dim);
        }
    } else if (name == gSWAP) {
        qs_policy_t::ApplySWAP(qs, gate->obj_qubits_, gate->ctrl_qubits_, dim);
    } else if (name == gISWAP) {
        qs_policy_t::ApplyISWAP(qs, gate->obj_qubits_, gate->ctrl_qubits_, gate->daggered_, dim);
    } else if (name == gRX) {
        auto val = gate->applied_value_;
        if (!gate->parameterized_) {
            diff = false;
        } else {
            val = gate->params_.Combination(pr).const_value;
        }
        qs_policy_t::ApplyRX(qs, gate->obj_qubits_, gate->ctrl_qubits_, val, dim, diff);
    } else if (name == gRY) {
        auto val = gate->applied_value_;
        if (!gate->parameterized_) {
            diff = false;
        } else {
            val = gate->params_.Combination(pr).const_value;
        }
        qs_policy_t::ApplyRY(qs, gate->obj_qubits_, gate->ctrl_qubits_, val, dim, diff);
    } else if (name == gRZ) {
        auto val = gate->applied_value_;
        if (!gate->parameterized_) {
            diff = false;
        } else {
            val = gate->params_.Combination(pr).const_value;
        }
        qs_policy_t::ApplyRZ(qs, gate->obj_qubits_, gate->ctrl_qubits_, val, dim, diff);
    } else if (name == gXX) {
        auto val = gate->applied_value_;
        if (!gate->parameterized_) {
            diff = false;
        } else {
            val = gate->params_.Combination(pr).const_value;
        }
        qs_policy_t::ApplyXX(qs, gate->obj_qubits_, gate->ctrl_qubits_, val, dim, diff);
    } else if (name == gZZ) {
        auto val = gate->applied_value_;
        if (!gate->parameterized_) {
            diff = false;
        } else {
            val = gate->params_.Combination(pr).const_value;
        }
        qs_policy_t::ApplyZZ(qs, gate->obj_qubits_, gate->ctrl_qubits_, val, dim, diff);
    } else if (name == gYY) {
        auto val = gate->applied_value_;
        if (!gate->parameterized_) {
            diff = false;
        } else {
            val = gate->params_.Combination(pr).const_value;
        }
        qs_policy_t::ApplyYY(qs, gate->obj_qubits_, gate->ctrl_qubits_, val, dim, diff);
    } else if (name == gPS) {
        auto val = gate->applied_value_;
        if (!gate->parameterized_) {
            diff = false;
        } else {
            val = gate->params_.Combination(pr).const_value;
        }
        qs_policy_t::ApplyPS(qs, gate->obj_qubits_, gate->ctrl_qubits_, val, dim, diff);
    } else if (name == gGP) {
        auto val = gate->applied_value_;
        if (!gate->parameterized_) {
            diff = false;
        } else {
            val = gate->params_.Combination(pr).const_value;
        }
        qs_policy_t::ApplyGP(qs, gate->obj_qubits_[0], gate->ctrl_qubits_, val, dim, diff);
    } else if (name == gU3) {
        if (diff) {
            std::runtime_error("Can not apply differential format of U3 gate on quatum states currently.");
        }
        auto u3 = static_cast<U3<calc_type>*>(gate.get());
        Dim2Matrix<calc_type> m;
        if (!gate->parameterized_) {
            m = gate->base_matrix_;
        } else {
            auto theta = u3->theta.Combination(pr).const_value;
            auto phi = u3->phi.Combination(pr).const_value;
            auto lambda = u3->lambda.Combination(pr).const_value;
            m = U3Matrix<calc_type>(theta, phi, lambda);
        }
        qs_policy_t::ApplySingleQubitMatrix(qs, qs, gate->obj_qubits_[0], gate->ctrl_qubits_, m.matrix_, dim);
    } else if (name == gFSim) {
        if (diff) {
            std::runtime_error("Can not apply differential format of FSim gate on quatum states currently.");
        }
        auto fsim = static_cast<FSim<calc_type>*>(gate.get());
        Dim2Matrix<calc_type> m;
        if (!gate->parameterized_) {
            m = gate->base_matrix_;
        } else {
            auto theta = fsim->theta.Combination(pr).const_value;
            auto phi = fsim->phi.Combination(pr).const_value;
            m = FSimMatrix<calc_type>(theta, phi);
        }
        qs_policy_t::ApplyTwoQubitsMatrix(qs, qs, gate->obj_qubits_, gate->ctrl_qubits_, m.matrix_, dim);
    } else if (gate->is_measure_) {
        return ApplyMeasure(gate);
    } else if (gate->is_channel_) {
        ApplyChannel(gate);
    } else {
        throw std::invalid_argument("Apply of gate " + name + " not implement.");
    }
    return 2;  // qubit should be 1 or 0, 2 means nothing.
}

template <typename qs_policy_t_>
auto VectorState<qs_policy_t_>::ApplyMeasure(const std::shared_ptr<BasicGate<calc_type>>& gate) {
    assert(gate->is_measure_);
    index_t one_mask = (1UL << gate->obj_qubits_[0]);
    auto one_amp = qs_policy_t::ConditionalCollect(qs, one_mask, one_mask, true, dim).real();
    index_t collapse_mask = (static_cast<index_t>(rng_() < one_amp) << gate->obj_qubits_[0]);
    qs_data_t norm_fact = (collapse_mask == 0) ? 1 / std::sqrt(1 - one_amp) : 1 / std::sqrt(one_amp);
    qs_policy_t::ConditionalMul(qs, qs, one_mask, collapse_mask, norm_fact, 0.0, dim);
    return static_cast<index_t>(collapse_mask != 0);
}

template <typename qs_policy_t_>
auto VectorState<qs_policy_t_>::ApplyChannel(const std::shared_ptr<BasicGate<calc_type>>& gate) {
    assert(gate->is_channel_);
    if (gate->name_ == "PL") {
        ApplyPauliChannel(gate);
    } else if (gate->kraus_operator_set_.size() != 0) {
        ApplyKrausChannel(gate);
    } else if (gate->name_ == "ADC" || gate->name_ == "PDC") {
        ApplyDampingChannel(gate);
    } else {
        throw std::runtime_error("This noise channel not implemented.");
    }
}

template <typename qs_policy_t_>
auto VectorState<qs_policy_t_>::ApplyPauliChannel(const std::shared_ptr<BasicGate<calc_type>>& gate) {
    assert(gate->name_ == "PL");
    double r = static_cast<double>(rng_());
    auto it = std::lower_bound(gate->cumulative_probs_.begin(), gate->cumulative_probs_.end(), r);
    size_t gate_index;
    if (it != gate->cumulative_probs_.begin()) {
        gate_index = std::distance(gate->cumulative_probs_.begin(), it) - 1;
    } else {
        gate_index = 0;
    }
    if (gate_index == 0) {
        qs_policy_t::ApplyX(qs, gate->obj_qubits_, gate->ctrl_qubits_, dim);
    } else if (gate_index == 1) {
        qs_policy_t::ApplyY(qs, gate->obj_qubits_, gate->ctrl_qubits_, dim);
    } else if (gate_index == 2) {
        qs_policy_t::ApplyZ(qs, gate->obj_qubits_, gate->ctrl_qubits_, dim);
    }
}

template <typename qs_policy_t_>
auto VectorState<qs_policy_t_>::ApplyKrausChannel(const std::shared_ptr<BasicGate<calc_type>>& gate) {
    assert(gate->kraus_operator_set_.size() != 0);
    auto tmp_qs = qs_policy_t::InitState(dim);
    calc_type prob = 0;
    for (size_t n_kraus = 0; n_kraus < gate->kraus_operator_set_.size(); n_kraus++) {
        qs_policy_t::ApplySingleQubitMatrix(qs, tmp_qs, gate->obj_qubits_[0], gate->ctrl_qubits_,
                                            gate->kraus_operator_set_[n_kraus], dim);
        calc_type renormal_factor_square = qs_policy_t::Vdot(tmp_qs, tmp_qs, dim).real();
        prob = renormal_factor_square / (1 - prob);
        calc_type renormal_factor = 1 / std::sqrt(renormal_factor_square);
        if (static_cast<calc_type>(rng_()) <= prob) {
            qs_policy_t::QSMulValue(tmp_qs, tmp_qs, renormal_factor, dim);
            qs_policy_t::FreeState(qs);
            qs = tmp_qs;
            tmp_qs = nullptr;
        }
    }
    qs_policy_t::FreeState(tmp_qs);
}

template <typename qs_policy_t_>
auto VectorState<qs_policy_t_>::ApplyDampingChannel(const std::shared_ptr<BasicGate<calc_type>>& gate) {
    calc_type reduced_factor_b_square = qs_policy_t::OneStateVdot(qs, qs, gate->obj_qubits_[0], dim).real();
    calc_type reduced_factor_b = std::sqrt(reduced_factor_b_square);
    if (reduced_factor_b < 1e-8) {
        return;
    }
    calc_type prob = gate->damping_coeff_ * reduced_factor_b_square;
    if (static_cast<calc_type>(rng_()) <= prob) {
        auto tmp_qs = qs_policy_t::InitState(dim);
        if (gate->name_ == "ADC") {
            std::vector<std::vector<py_qs_data_t>> m({{0, 1 / reduced_factor_b}, {0, 0}});
            qs_policy_t::ApplySingleQubitMatrix(qs, tmp_qs, gate->obj_qubits_[0], gate->ctrl_qubits_, m, dim);
        } else {
            qs_policy_t::ConditionalMul(qs, tmp_qs, (1UL << gate->obj_qubits_[0]), 1, 1 / reduced_factor_b, 0, dim);
        }
        qs = tmp_qs;
        tmp_qs = nullptr;
        qs_policy_t::FreeState(tmp_qs);
    } else {
        calc_type coeff_a = 1 / sqrt(1 - prob);
        calc_type coeff_b = std::sqrt(1 - gate->damping_coeff_) / std::sqrt(1 - prob);
        qs_policy_t::ConditionalMul(qs, qs, (1UL << gate->obj_qubits_[0]), 0, coeff_a, coeff_b, dim);
    }
}

template <typename qs_policy_t_>
auto VectorState<qs_policy_t_>::ExpectDiffGate(qs_data_p_t bra, qs_data_p_t ket,
                                               const std::shared_ptr<BasicGate<calc_type>>& gate,
                                               const ParameterResolver<calc_type>& pr, index_t dim) -> py_qs_data_t {
    auto name = gate->name_;
    auto val = gate->params_.Combination(pr).const_value;
    if (gate->is_custom_) {
        std::remove_reference_t<decltype(*gate)>::matrix_t mat = gate->numba_param_diff_matrix_(val);
        return qs_policy_t::ExpectDiffMatrixGate(bra, ket, gate->obj_qubits_, gate->ctrl_qubits_, mat.matrix_, dim);
    }
    if (name == gRX) {
        return qs_policy_t::ExpectDiffRX(bra, ket, gate->obj_qubits_, gate->ctrl_qubits_, val, dim);
    }
    if (name == gRY) {
        return qs_policy_t::ExpectDiffRY(bra, ket, gate->obj_qubits_, gate->ctrl_qubits_, val, dim);
    }
    if (name == gRZ) {
        return qs_policy_t::ExpectDiffRZ(bra, ket, gate->obj_qubits_, gate->ctrl_qubits_, val, dim);
    }
    if (name == gXX) {
        return qs_policy_t::ExpectDiffXX(bra, ket, gate->obj_qubits_, gate->ctrl_qubits_, val, dim);
    }
    if (name == gZZ) {
        return qs_policy_t::ExpectDiffZZ(bra, ket, gate->obj_qubits_, gate->ctrl_qubits_, val, dim);
    }
    if (name == gYY) {
        return qs_policy_t::ExpectDiffYY(bra, ket, gate->obj_qubits_, gate->ctrl_qubits_, val, dim);
    }
    if (name == gPS) {
        return qs_policy_t::ExpectDiffPS(bra, ket, gate->obj_qubits_, gate->ctrl_qubits_, val, dim);
    }
    if (name == gGP) {
        return qs_policy_t::ExpectDiffGP(bra, ket, gate->obj_qubits_, gate->ctrl_qubits_, val, dim);
    }
    throw std::invalid_argument("Expectation of gate " + name + " not implement.");
}

template <typename qs_policy_t_>
auto VectorState<qs_policy_t_>::ExpectDiffU3(qs_data_p_t bra, qs_data_p_t ket,
                                             const std::shared_ptr<BasicGate<calc_type>>& gate,
                                             const ParameterResolver<calc_type>& pr, index_t dim)
    -> Dim2Matrix<calc_type> {
    py_qs_datas_t grad = {0, 0, 0};
    auto u3 = static_cast<U3<calc_type>*>(gate.get());
    if (u3->parameterized_) {
        Dim2Matrix<calc_type> m;
        auto theta = u3->theta.Combination(pr).const_value;
        auto phi = u3->phi.Combination(pr).const_value;
        auto lambda = u3->lambda.Combination(pr).const_value;
        if (u3->theta.data_.size() != u3->theta.no_grad_parameters_.size()) {
            m = U3DiffThetaMatrix(theta, phi, lambda);
            grad[0] = qs_policy_t::ExpectDiffSingleQubitMatrix(bra, ket, u3->obj_qubits_, u3->ctrl_qubits_, m.matrix_,
                                                               dim);
        }
        if (u3->phi.data_.size() != u3->phi.no_grad_parameters_.size()) {
            m = U3DiffPhiMatrix(theta, phi, lambda);
            grad[1] = qs_policy_t::ExpectDiffSingleQubitMatrix(bra, ket, u3->obj_qubits_, u3->ctrl_qubits_, m.matrix_,
                                                               dim);
        }
        if (u3->lambda.data_.size() != u3->lambda.no_grad_parameters_.size()) {
            m = U3DiffLambdaMatrix(theta, phi, lambda);
            grad[2] = qs_policy_t::ExpectDiffSingleQubitMatrix(bra, ket, u3->obj_qubits_, u3->ctrl_qubits_, m.matrix_,
                                                               dim);
        }
    }
    return Dim2Matrix<calc_type>({grad});
}

template <typename qs_policy_t_>
auto VectorState<qs_policy_t_>::ExpectDiffFSim(qs_data_p_t bra, qs_data_p_t ket,
                                               const std::shared_ptr<BasicGate<calc_type>>& gate,
                                               const ParameterResolver<calc_type>& pr, index_t dim)
    -> Dim2Matrix<calc_type> {
    py_qs_datas_t grad = {0, 0};
    auto fsim = static_cast<FSim<calc_type>*>(gate.get());
    if (fsim->parameterized_) {
        Dim2Matrix<calc_type> m;
        auto theta = fsim->theta.Combination(pr).const_value;
        auto phi = fsim->phi.Combination(pr).const_value;
        if (fsim->theta.data_.size() != fsim->theta.no_grad_parameters_.size()) {
            m = FSimDiffThetaMatrix(theta);  // can be optimized.
            grad[0] = qs_policy_t::ExpectDiffTwoQubitsMatrix(bra, ket, fsim->obj_qubits_, fsim->ctrl_qubits_, m.matrix_,
                                                             dim);
        }
        if (fsim->phi.data_.size() != fsim->phi.no_grad_parameters_.size()) {
            m = FSimDiffPhiMatrix(phi);
            grad[1] = qs_policy_t::ExpectDiffTwoQubitsMatrix(bra, ket, fsim->obj_qubits_, fsim->ctrl_qubits_, m.matrix_,
                                                             dim);
        }
    }
    return Dim2Matrix<calc_type>({grad});
}

template <typename qs_policy_t_>
auto VectorState<qs_policy_t_>::ApplyCircuit(const circuit_t& circ, const ParameterResolver<calc_type>& pr) {
    std::map<std::string, int> result;
    for (auto& g : circ) {
        if (g->is_measure_) {
            result[g->name_] = ApplyMeasure(g);
        } else {
            ApplyGate(g, pr, false);
        }
    }
    return result;
}

template <typename qs_policy_t_>
void VectorState<qs_policy_t_>::ApplyHamiltonian(const Hamiltonian<calc_type>& ham) {
    qs_data_p_t new_qs;
    if (ham.how_to_ == ORIGIN) {
        new_qs = qs_policy_t::ApplyTerms(qs, ham.ham_, dim);
    } else if (ham.how_to_ == BACKEND) {
        new_qs = qs_policy_t::CsrDotVec(ham.ham_sparse_main_, ham.ham_sparse_second_, qs, dim);
    } else {
        new_qs = qs_policy_t::CsrDotVec(ham.ham_sparse_main_, qs, dim);
    }
    qs_policy_t::FreeState(qs);
    qs = new_qs;
}

template <typename qs_policy_t_>
auto VectorState<qs_policy_t_>::GetCircuitMatrix(const circuit_t& circ, const ParameterResolver<calc_type>& pr)
    -> VT<py_qs_datas_t> {
    VVT<CT<calc_type>> out((1 << n_qubits));
    for (size_t i = 0; i < (1UL << n_qubits); i++) {
        auto sim = VectorState<qs_policy_t>(n_qubits, seed);
        sim.ApplyCircuit(circ, pr);
        out[i] = sim.GetQS();
    }
    return out;
}

template <typename qs_policy_t_>
auto VectorState<qs_policy_t_>::GetExpectationWithGradOneOne(const Hamiltonian<calc_type>& ham, const circuit_t& circ,
                                                             const circuit_t& herm_circ,
                                                             const ParameterResolver<calc_type>& pr,
                                                             const MST<size_t>& p_map) -> py_qs_datas_t {
    // auto timer = Timer();
    // timer.Start("First part");
    py_qs_datas_t f_and_g(1 + p_map.size(), 0);
    VectorState<qs_policy_t> sim_l = *this;
    sim_l.ApplyCircuit(circ, pr);
    VectorState<qs_policy_t> sim_r = sim_l;
    sim_r.ApplyHamiltonian(ham);
    f_and_g[0] = qs_policy_t::Vdot(sim_l.qs, sim_r.qs, dim);
    // timer.EndAndStartOther("First part", "Second part");
    for (const auto& g : herm_circ) {
        sim_l.ApplyGate(g, pr);
        if (g->name_ == gU3) {
            auto u3 = static_cast<U3<calc_type>*>(g.get());
            if (const auto& [title, jac] = u3->jacobi; title.size() != 0) {
                auto intrin_grad = ExpectDiffU3(sim_l.qs, sim_r.qs, g, pr, dim);
                auto u3_grad = Dim2MatrixMatMul<calc_type>(intrin_grad, jac);
                for (const auto& [name, idx] : title) {
                    f_and_g[1 + p_map.at(name)] += 2 * std::real(u3_grad.matrix_[0][idx]);
                }
            }
        } else if (g->name_ == gFSim) {
            auto fsim = static_cast<FSim<calc_type>*>(g.get());
            if (const auto& [title, jac] = fsim->jacobi; title.size() != 0) {
                auto intrin_grad = ExpectDiffFSim(sim_l.qs, sim_r.qs, g, pr, dim);
                auto fsim_grad = Dim2MatrixMatMul<calc_type>(intrin_grad, jac);
                for (const auto& [name, idx] : title) {
                    f_and_g[1 + p_map.at(name)] += 2 * std::real(fsim_grad.matrix_[0][idx]);
                }
            }
        } else if (g->params_.data_.size() != g->params_.no_grad_parameters_.size()) {
            // timer.Start("ExpectDiffGate");
            auto gi = ExpectDiffGate(sim_l.qs, sim_r.qs, g, pr, dim);
            // timer.End("ExpectDiffGate");
            for (auto& it : g->params_.GetRequiresGradParameters()) {
                f_and_g[1 + p_map.at(it)] += 2 * std::real(gi) * g->params_.data_.at(it);
            }
        }
        sim_r.ApplyGate(g, pr);
    }
    // timer.End("Second part");
    // timer.Analyze();
    return f_and_g;
}

template <typename qs_policy_t_>
auto VectorState<qs_policy_t_>::GetExpectationNonHermitianWithGradOneMulti(
    const std::vector<std::shared_ptr<Hamiltonian<calc_type>>>& hams,
    const std::vector<std::shared_ptr<Hamiltonian<calc_type>>>& herm_hams, const circuit_t& left_circ,
    const circuit_t& herm_left_circ, const circuit_t& right_circ, const circuit_t& herm_right_circ,
    const ParameterResolver<calc_type>& pr, const MST<size_t>& p_map, int n_thread, const derived_t& simulator_left)
    -> VT<py_qs_datas_t> {
    auto n_hams = hams.size();

    VT<py_qs_datas_t> f_and_g(n_hams, py_qs_datas_t((1 + p_map.size()), 0));
    derived_t sim_left = simulator_left;
    derived_t sim_right = *this;
    sim_left.ApplyCircuit(left_circ, pr);
    sim_right.ApplyCircuit(right_circ, pr);
    auto f_g1 = LeftSizeGradOneMulti(hams, herm_left_circ, pr, p_map, n_thread, sim_left, sim_right);
    auto f_g2 = LeftSizeGradOneMulti(herm_hams, herm_right_circ, pr, p_map, n_thread, sim_right, sim_left);
    for (size_t i = 0; i < f_g2.size(); i++) {
        for (size_t j = 1; j < f_g2[i].size(); j++) {
            f_g1[i][j] += std::conj(f_g2[i][j]);
        }
    }
    return f_g1;
}

template <typename qs_policy_t_>
auto VectorState<qs_policy_t_>::LeftSizeGradOneMulti(const std::vector<std::shared_ptr<Hamiltonian<calc_type>>>& hams,
                                                     const circuit_t& herm_left_circ,
                                                     const ParameterResolver<calc_type>& pr, const MST<size_t>& p_map,
                                                     int n_thread, const derived_t& simulator_left,
                                                     const derived_t& simulator_right) -> VT<py_qs_datas_t> {
    auto n_hams = hams.size();
    int max_thread = 15;
    if (n_thread > max_thread) {
        n_thread = max_thread;
    }

    if (n_thread > static_cast<int>(n_hams)) {
        n_thread = n_hams;
    }

    VT<py_qs_datas_t> f_and_g(n_hams, py_qs_datas_t((1 + p_map.size()), 0));

    int n_group = n_hams / n_thread;
    if (n_hams % n_thread) {
        n_group += 1;
    }
    for (int i = 0; i < n_group; i++) {
        int start = i * n_thread;
        int end = (i + 1) * n_thread;
        if (end > static_cast<int>(n_hams)) {
            end = n_hams;
        }
        std::vector<VectorState<qs_policy_t>> sim_rs(end - start);
        auto sim_l = simulator_left;
        for (int j = start; j < end; j++) {
            sim_rs[j - start] = simulator_right;
            sim_rs[j - start].ApplyHamiltonian(*hams[j]);
            f_and_g[j][0] = qs_policy_t::Vdot(sim_l.qs, sim_rs[j - start].qs, dim);
        }
        for (const auto& g : herm_left_circ) {
            sim_l.ApplyGate(g, pr);
            if (g->name_ == gU3) {
                auto u3 = static_cast<U3<calc_type>*>(g.get());
                for (int j = start; j < end; j++) {
                    if (const auto& [title, jac] = u3->jacobi; title.size() != 0) {
                        auto intrin_grad = ExpectDiffU3(sim_l.qs, sim_rs[j - start].qs, g, pr, dim);
                        auto u3_grad = Dim2MatrixMatMul<calc_type>(intrin_grad, jac);
                        for (const auto& [name, idx] : title) {
                            f_and_g[j][1 + p_map.at(name)] += u3_grad.matrix_[0][idx];
                        }
                    }
                }
            } else if (g->name_ == gFSim) {
                auto fsim = static_cast<FSim<calc_type>*>(g.get());
                for (int j = start; j < end; j++) {
                    if (const auto& [title, jac] = fsim->jacobi; title.size() != 0) {
                        auto intrin_grad = ExpectDiffFSim(sim_l.qs, sim_rs[j - start].qs, g, pr, dim);
                        auto fsim_grad = Dim2MatrixMatMul<calc_type>(intrin_grad, jac);
                        for (const auto& [name, idx] : title) {
                            f_and_g[j][1 + p_map.at(name)] += fsim_grad.matrix_[0][idx];
                        }
                    }
                }
            } else if (g->params_.data_.size() != g->params_.no_grad_parameters_.size()) {
                for (int j = start; j < end; j++) {
                    auto gi = ExpectDiffGate(sim_l.qs, sim_rs[j - start].qs, g, pr, dim);
                    for (auto& it : g->params_.GetRequiresGradParameters()) {
                        f_and_g[j][1 + p_map.at(it)] += gi * g->params_.data_.at(it);
                    }
                }
            }
            for (int j = start; j < end; j++) {
                sim_rs[j - start].ApplyGate(g, pr);
            }
        }
    }
    return f_and_g;
}

template <typename qs_policy_t_>
auto VectorState<qs_policy_t_>::GetExpectationWithGradOneMulti(
    const std::vector<std::shared_ptr<Hamiltonian<calc_type>>>& hams, const circuit_t& circ, const circuit_t& herm_circ,
    const ParameterResolver<calc_type>& pr, const MST<size_t>& p_map, int n_thread) -> VT<py_qs_datas_t> {
    auto n_hams = hams.size();
    int max_thread = 15;
    if (n_thread > max_thread) {
        n_thread = max_thread;
    }
    if (n_thread > static_cast<int>(n_hams)) {
        n_thread = n_hams;
    }
    VT<py_qs_datas_t> f_and_g(n_hams, py_qs_datas_t((1 + p_map.size()), 0));
    VectorState<qs_policy_t> sim = *this;
    sim.ApplyCircuit(circ, pr);
    int n_group = n_hams / n_thread;
    if (n_hams % n_thread) {
        n_group += 1;
    }
    for (int i = 0; i < n_group; i++) {
        int start = i * n_thread;
        int end = (i + 1) * n_thread;
        if (end > static_cast<int>(n_hams)) {
            end = n_hams;
        }
        std::vector<VectorState<qs_policy_t>> sim_rs(end - start);
        auto sim_l = sim;
        for (int j = start; j < end; j++) {
            sim_rs[j - start] = sim_l;
            sim_rs[j - start].ApplyHamiltonian(*hams[j]);
            f_and_g[j][0] = qs_policy_t::Vdot(sim_l.qs, sim_rs[j - start].qs, dim);
        }
        for (const auto& g : herm_circ) {
            sim_l.ApplyGate(g, pr);
            if (g->name_ == gU3) {
                auto u3 = static_cast<U3<calc_type>*>(g.get());
                for (int j = start; j < end; j++) {
                    if (const auto& [title, jac] = u3->jacobi; title.size() != 0) {
                        auto intrin_grad = ExpectDiffU3(sim_l.qs, sim_rs[j - start].qs, g, pr, dim);
                        auto u3_grad = Dim2MatrixMatMul<calc_type>(intrin_grad, jac);
                        for (const auto& [name, idx] : title) {
                            f_and_g[j][1 + p_map.at(name)] += 2 * std::real(u3_grad.matrix_[0][idx]);
                        }
                    }
                }
            } else if (g->name_ == gFSim) {
                auto fsim = static_cast<FSim<calc_type>*>(g.get());
                for (int j = start; j < end; j++) {
                    if (const auto& [title, jac] = fsim->jacobi; title.size() != 0) {
                        auto intrin_grad = ExpectDiffFSim(sim_l.qs, sim_rs[j - start].qs, g, pr, dim);
                        auto fsim_grad = Dim2MatrixMatMul<calc_type>(intrin_grad, jac);
                        for (const auto& [name, idx] : title) {
                            f_and_g[j][1 + p_map.at(name)] += 2 * std::real(fsim_grad.matrix_[0][idx]);
                        }
                    }
                }
            } else if (g->params_.data_.size() != g->params_.no_grad_parameters_.size()) {
                for (int j = start; j < end; j++) {
                    auto gi = ExpectDiffGate(sim_l.qs, sim_rs[j - start].qs, g, pr, dim);
                    for (auto& it : g->params_.GetRequiresGradParameters()) {
                        f_and_g[j][1 + p_map.at(it)] += 2 * std::real(gi) * g->params_.data_.at(it);
                    }
                }
            }
            for (int j = start; j < end; j++) {
                sim_rs[j - start].ApplyGate(g, pr);
            }
        }
    }
    return f_and_g;
}

template <typename qs_policy_t_>
auto VectorState<qs_policy_t_>::GetExpectationNonHermitianWithGradMultiMulti(
    const std::vector<std::shared_ptr<Hamiltonian<calc_type>>>& hams,
    const std::vector<std::shared_ptr<Hamiltonian<calc_type>>>& herm_hams, const circuit_t& left_circ,
    const circuit_t& herm_left_circ, const circuit_t& right_circ, const circuit_t& herm_right_circ,
    const VVT<calc_type>& enc_data, const VT<calc_type>& ans_data, const VS& enc_name, const VS& ans_name,
    const derived_t& simulator_left, size_t batch_threads, size_t mea_threads) -> VT<VT<py_qs_datas_t>> {
    auto n_hams = hams.size();
    auto n_prs = enc_data.size();
    auto n_params = enc_name.size() + ans_name.size();
    VT<VT<py_qs_datas_t>> output;
    for (size_t i = 0; i < n_prs; i++) {
        output.push_back({});
        for (size_t j = 0; j < n_hams; j++) {
            output[i].push_back({});
            for (size_t k = 0; k < n_params + 1; k++) {
                output[i][j].push_back({0, 0});
            }
        }
    }
    MST<size_t> p_map;
    for (size_t i = 0; i < enc_name.size(); i++) {
        p_map[enc_name[i]] = i;
    }
    for (size_t i = 0; i < ans_name.size(); i++) {
        p_map[ans_name[i]] = i + enc_name.size();
    }
    if (n_prs == 1) {
        ParameterResolver<calc_type> pr = ParameterResolver<calc_type>();
        pr.SetItems(enc_name, enc_data[0]);
        pr.SetItems(ans_name, ans_data);
        output[0] = GetExpectationNonHermitianWithGradOneMulti(hams, herm_hams, left_circ, herm_left_circ, right_circ,
                                                               herm_right_circ, pr, p_map, mea_threads, simulator_left);
    } else {
        std::vector<std::thread> tasks;
        tasks.reserve(batch_threads);
        size_t end = 0;
        size_t offset = n_prs / batch_threads;
        size_t left = n_prs % batch_threads;
        for (size_t i = 0; i < batch_threads; ++i) {
            size_t start = end;
            end = start + offset;
            if (i < left) {
                end += 1;
            }
            auto task = [&, start, end]() {
                for (size_t n = start; n < end; n++) {
                    ParameterResolver<calc_type> pr = ParameterResolver<calc_type>();
                    pr.SetItems(enc_name, enc_data[n]);
                    pr.SetItems(ans_name, ans_data);
                    auto f_g = GetExpectationNonHermitianWithGradOneMulti(hams, herm_hams, left_circ, herm_left_circ,
                                                                          right_circ, herm_right_circ, pr, p_map,
                                                                          mea_threads, simulator_left);
                    output[n] = f_g;
                }
            };
            tasks.emplace_back(task);
        }
        for (auto& t : tasks) {
            t.join();
        }
    }
    return output;
}

template <typename qs_policy_t_>
auto VectorState<qs_policy_t_>::GetExpectationWithGradMultiMulti(
    const std::vector<std::shared_ptr<Hamiltonian<calc_type>>>& hams, const circuit_t& circ, const circuit_t& herm_circ,
    const VVT<calc_type>& enc_data, const VT<calc_type>& ans_data, const VS& enc_name, const VS& ans_name,
    size_t batch_threads, size_t mea_threads) -> VT<VT<py_qs_datas_t>> {
    auto n_hams = hams.size();
    auto n_prs = enc_data.size();
    auto n_params = enc_name.size() + ans_name.size();
    VT<VT<py_qs_datas_t>> output;
    for (size_t i = 0; i < n_prs; i++) {
        output.push_back({});
        for (size_t j = 0; j < n_hams; j++) {
            output[i].push_back({});
            for (size_t k = 0; k < n_params + 1; k++) {
                output[i][j].push_back({0, 0});
            }
        }
    }
    MST<size_t> p_map;
    for (size_t i = 0; i < enc_name.size(); i++) {
        p_map[enc_name[i]] = i;
    }
    for (size_t i = 0; i < ans_name.size(); i++) {
        p_map[ans_name[i]] = i + enc_name.size();
    }
    if (n_prs == 1) {
        ParameterResolver<calc_type> pr = ParameterResolver<calc_type>();
        pr.SetItems(enc_name, enc_data[0]);
        pr.SetItems(ans_name, ans_data);
        output[0] = GetExpectationWithGradOneMulti(hams, circ, herm_circ, pr, p_map, mea_threads);
    } else {
        std::vector<std::thread> tasks;
        tasks.reserve(batch_threads);
        size_t end = 0;
        size_t offset = n_prs / batch_threads;
        size_t left = n_prs % batch_threads;
        for (size_t i = 0; i < batch_threads; ++i) {
            size_t start = end;
            end = start + offset;
            if (i < left) {
                end += 1;
            }
            auto task = [&, start, end]() {
                for (size_t n = start; n < end; n++) {
                    ParameterResolver<calc_type> pr = ParameterResolver<calc_type>();
                    pr.SetItems(enc_name, enc_data[n]);
                    pr.SetItems(ans_name, ans_data);
                    auto f_g = GetExpectationWithGradOneMulti(hams, circ, herm_circ, pr, p_map, mea_threads);
                    output[n] = f_g;
                }
            };
            tasks.emplace_back(task);
        }
        for (auto& t : tasks) {
            t.join();
        }
    }
    return output;
}

template <typename qs_policy_t_>
VT<unsigned> VectorState<qs_policy_t_>::Sampling(const circuit_t& circ, const ParameterResolver<calc_type>& pr,
                                                 size_t shots, const MST<size_t>& key_map, unsigned int seed) {
    auto key_size = key_map.size();
    VT<unsigned> res(shots * key_size);
    RndEngine rnd_eng = RndEngine(seed);
    std::uniform_real_distribution<double> dist(1.0, (1 << 20) * 1.0);
    std::function<double()> rng = std::bind(dist, std::ref(rnd_eng));
    for (size_t i = 0; i < shots; i++) {
        auto sim = derived_t(n_qubits, static_cast<unsigned>(rng()), qs);
        auto res0 = sim.ApplyCircuit(circ, pr);
        VT<unsigned> res1(key_map.size());
        for (const auto& [name, val] : key_map) {
            res1[val] = res0[name];
        }
        for (size_t j = 0; j < key_size; j++) {
            res[i * key_size + j] = res1[j];
        }
    }
    return res;
}
}  // namespace mindquantum::sim::vector::detail

#endif
