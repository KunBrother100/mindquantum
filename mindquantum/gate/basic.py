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
"""Basic module for quantum gate."""

import warnings
from copy import deepcopy
from abc import abstractmethod
from collections.abc import Iterable
import numpy as np
from mindquantum.parameterresolver import ParameterResolver as PR


class BasicGate():
    """BasicGate is the base class of all gaets."""
    def __init__(self, name, isparameter=False):
        if not isinstance(name, str):
            raise TypeError("Excepted string for gate name, get {}".format(
                type(name)))
        self.name = name
        self.isparameter = isparameter
        self.str = self.name
        self.projectq_gate = None

    @abstractmethod
    def matrix(self, *args):
        """The matrix of the gate."""

    @abstractmethod
    def hermitian(self):
        """Return the hermitian gate of this gate."""

    @abstractmethod
    def define_projectq_gate(self):
        """Define the corresponded projectq gate."""

    def generate_description(self):
        """Description generator."""
        if hasattr(self, 'ctrl_qubits') and self.ctrl_qubits:
            obj_str = ' '.join([str(i) for i in self.obj_qubits])
            ctrl_str = ' '.join([str(i) for i in self.ctrl_qubits])
            self.str = "{}({} <-: {})".format(self.name, obj_str, ctrl_str)
        elif hasattr(self, 'obj_qubits'):
            self.str = "{}({})".format(
                self.name, ' '.join([str(i) for i in self.obj_qubits]))

    def on(self, obj_qubits, ctrl_qubits=None):
        """
        Define which qubit the gate act on and the control qubit.

        Note:
            In this framework, the qubit that the gate act on is specified
            first, even for control gate, e.g. CNOT, the second arg is control
            qubits.

        Args:
            obj_qubits (int, list[int]): Specific which qubits the gate act on.
            ctrl_qubits (int, list[int]): Specific the control qbits. Defaults to None.

        Returns:
            Gate, Return a new gate.

        Examples:
            >>> x = X.on(1)
            >>> x.obj_qubits
            [1]
            >>> x.ctrl_qubits
            []

            >>> x = X.on(2, [0, 1])
            >>> x.ctrl_qubits
            [0, 1]
        """
        new = deepcopy(self)

        if isinstance(obj_qubits, int):
            new.obj_qubits = [obj_qubits]
            _check_qubit_id(obj_qubits)
        elif isinstance(obj_qubits, Iterable):
            for i in obj_qubits:
                _check_qubit_id(i)
            new.obj_qubits = list(obj_qubits)
        else:
            raise TypeError("Excepted int, list or tuple for \
                obj_qubits, but get {}".format(type(obj_qubits)))
        if ctrl_qubits is None:
            new.ctrl_qubits = []
        else:
            if isinstance(ctrl_qubits, int):
                new.ctrl_qubits = [ctrl_qubits]
                _check_qubit_id(ctrl_qubits)
            elif isinstance(ctrl_qubits, Iterable):
                for i in ctrl_qubits:
                    _check_qubit_id(i)
                new.ctrl_qubits = list(ctrl_qubits)
            else:
                raise TypeError("Excepted int, list or tuple for \
                    ctrl_qubits, but get {}".format(type(obj_qubits)))
        new.generate_description()
        return new

    def requires_grad(self):
        return self

    def no_grad(self):
        return self

    def __str__(self):
        return self.str

    def __repr__(self):
        return self.__str__()

    def __eq__(self, other):
        _check_gate_type(other)
        if self.name != other.name or \
                self.isparameter != other.isparameter or \
                self.obj_qubits != other.obj_qubits or \
                self.ctrl_qubits != other.ctrl_qubits:
            return False
        return True

    def __or__(self, qubits):
        if not isinstance(qubits, tuple):
            qubits = (qubits,)

        qubits = list(qubits)

        for i, _ in enumerate(qubits):
            if hasattr(qubits[i], "qubit_id"):
                qubits[i] = [qubits[i]]
        ctrls = []
        objs = []
        if len(qubits) == 1:
            objs = [qubit.qubit_id for qubit in qubits[0]]
        else:
            ctrls = [qubit.qubit_id for qubit in qubits[0]]
            objs = [qubit.qubit_id for qubit in qubits[1]]
        qubits[0][0].circuit_.append(self.on(objs, ctrls))


class NoneParameterGate(BasicGate):
    """The basic class of gate that is not parametrized."""
    def __init__(self, name):
        BasicGate.__init__(self, name, False)
        self.coeff = None
        self.matrix_value = None

    def matrix(self, *args):
        """
        Get the matrix of this none parameterized gate.
        """
        return self.matrix_value

    def hermitian(self):
        """
        Get hermitian gate of this none parameterized gate.
        """
        hermitian_gate = deepcopy(self)
        hermitian_gate.matrix_value = np.conj(self.matrix_value.T)
        return hermitian_gate

    def define_projectq_gate(self):
        raise NotImplementedError

    def __call__(self, obj_qubits, ctrl_qubits=None):
        return self.on(obj_qubits, ctrl_qubits)


class ParameterGate(NoneParameterGate, BasicGate):
    """The basic class of gate that is parameterized."""
    def __init__(self, name, coeff=None):
        if isinstance(coeff, (int, float, complex)):
            NoneParameterGate.__init__(self, name)
            self.coeff = coeff
            self.str = self.str + "({})".format(round(self.coeff, 3))
        else:
            BasicGate.__init__(self, name, True)
            if coeff is None:
                warnings.warn("Parameter gate without parameters specified, \
automatically set it to c1.")
                self.coeff = PR({'c1': 1})
            elif not isinstance(coeff, (list, tuple, str, dict, PR)):
                raise TypeError("Excepted str, list or tuple for coeff, \
but get {}".format(type(coeff)))
            else:
                if isinstance(coeff, str):
                    self.coeff = PR({coeff: 1})
                elif isinstance(coeff, PR):
                    self.coeff = coeff
                elif isinstance(coeff, dict):
                    self.coeff = PR(deepcopy(coeff))
                else:
                    self.coeff = PR(dict(zip(coeff, [1 for i in coeff])))
            self.str = self.str + "({})".format(self.coeff.expression())

    def generate_description(self):
        BasicGate.generate_description(self)
        if not hasattr(self, 'obj_qubits'):
            if self.isparameter:
                self.str = f'{self.name}({self.coeff.expression()})'
            else:
                self.str = f'{self.name}({round(self.coeff, 3)})'
        else:
            if self.isparameter:
                self.str = self.str[:len(
                    self.name) + 1] + str(self.coeff.expression())\
                    + '|' + self.str[len(self.name) + 1:]
            else:
                self.str = self.str[:len(
                    self.name) + 1] + str(round(self.coeff, 3))\
                    + '|' + self.str[len(self.name) + 1:]

    @abstractmethod
    def matrix(self, *paras_out):
        pass

    @abstractmethod
    def diff_matrix(self, *paras_out, about_what=None):
        pass

    @staticmethod
    def linearcombination(coeff_in, paras_out):
        """
        Combine the parameters and coefficient.

        Args:
            coeff_in (Union[dict, ParameterResolver]): the coefficient of the parameterized gate.
            paras_out (Union[dict, ParameterResolver]): the parameter you send in.

        Returns:
            float, Multiply the values of the common keys of these two dicts.
        """
        if not isinstance(coeff_in,
                          (dict, PR)) or not isinstance(paras_out, (dict, PR)):
            raise TypeError(
                "Require a dict or ParameterResolver for parameters, but get {} and {}!"
                .format(type(coeff_in), type(paras_out)))
        params = 0
        for key, value in coeff_in.items():
            if key not in paras_out:
                raise KeyError(
                    "parameter {} not in parameters you send in!".format(key))
            params += value * paras_out[key]
        return params

    def __eq__(self, other):
        if BasicGate.__eq__(self, other):
            if self.coeff == other.coeff:
                return True
        return False

    def requires_grad(self):
        """
        All parameters requires grad.
        """
        if self.isparameter:
            self.coeff.requires_grad()
        return self

    def no_grad(self):
        """
        All parameters do not need grad.
        """
        if self.isparameter:
            self.coeff.no_grad()
        return self

    def requires_grad_part(self, names):
        """
        Set certain parameters that need grad.

        Args:
            names (tuple[str]): Parameters that requires grad.
        """
        self.coeff.requires_grad_part(names)
        return self

    def no_grad_part(self, names):
        """
        Set certain parameters that not need grad.

        Args:
            names (tuple[str]): Parameters that not requires grad.
        """
        self.coeff.no_grad_part(names)
        return self


class IntrinsicOneParaGate(ParameterGate):
    """
    The parameterized gate that can be intrinsicly described by only one
    parameter.

    Note:
        A parameterized gate can also be a non parameterized gate, if the
        parameter you send in is only a number.

    Examples:
        >>> rx1 = RX(1.2)
        >>> rx1
        RX(1.2)
        >>> rx2 = RX({'a' : 0.5})
        >>> rx2.coeff
        {'a': 0.5}
        >>> rx2.linearcombination(rx2.coeff,{'a' : 3})
        1.5
    """
    def __init__(self, name, coeff=None):
        ParameterGate.__init__(self, name, coeff)

    def hermitian(self):
        """
        Get the hermitian gate of this parameterized gate.

        Note:
            We only set the coeff to -coeff.
        """
        hermitian_gate = deepcopy(self)
        hermitian_gate.coeff = 1 * self.coeff
        if isinstance(self.coeff, PR):
            hermitian_gate.coeff *= -1
        else:
            hermitian_gate.coeff = -float(self.coeff)
        hermitian_gate.generate_description()
        return hermitian_gate

    @abstractmethod
    def _matrix(self, theta):
        pass

    @abstractmethod
    def _diff_matrix(self, theta):
        pass

    def matrix(self, *paras_out):
        """
        The matrix of parameterized gate.

        Note:
            If the parameterized gate convert to non parameterized gate, then
            you don't need any parameters to get this matrix.

        Returns:
            np.array, Return the numpy array of the matrix.

        Examples:
            >>> rx1 = RX(0)
            >>> rx1.matrix()
            array([[1.+0.j, 0.-0.j],
                   [0.-0.j, 1.+0.j]])
            >>> rx2 = RX({'a' : 1.2})
            >>> np.round(rx2.matrix({'a': 2}), 2)
            array([[0.36+0.j  , 0.  -0.93j],
                   [0.  -0.93j, 0.36+0.j  ]])
        """
        if self.isparameter:
            theta = 0
            if isinstance(paras_out[0], dict):
                theta = self.linearcombination(self.coeff, paras_out[0])
            else:
                if len(self.coeff) != 1:
                    raise Exception("This gate has more than one parameters, \
                        need a parameters map!")
                theta = paras_out[0] * list(self.coeff.values())[0]
            return self._matrix(theta)
        return self._matrix(self.coeff)

    def diff_matrix(self, *paras_out, about_what=None):
        """
        The differential form of this parameterized gate.

        Args:
            about_what (str, optional): Specific the differential is about
                which parameter. Defaults to None.

        Returns:
            numpy.ndarray, Return the numpy array of the differential matrix.

        Examples:
            >>> rx = RX('a')
            >>> np.round(rx.diff_matrix({'a' : 2}), 2)
            array([[-0.42+0.j  ,  0.  -0.27j],
                   [ 0.  -0.27j, -0.42+0.j  ]])
        """
        if self.isparameter:
            theta = 0
            if isinstance(paras_out[0], dict):
                theta = self.linearcombination(self.coeff, paras_out[0])
            else:
                if len(self.coeff) != 1:
                    raise Exception("This gate has more than one parameters, \
                        need a parameters map!")
                theta = paras_out[0] * list(self.coeff.values())[0]
            if about_what is None:
                if len(self.coeff) != 1:
                    raise Exception(
                        "Please specific the diff is about which parameter.")
                about_what = list(self.coeff.keys())[0]
            return self.coeff[about_what] * self._diff_matrix(theta)
        raise Exception("Not a parameterized gate!")


def _check_gate_type(gate):
    if not isinstance(gate, BasicGate):
        raise TypeError("Require a quantum gate, but get {}".format(
            type(gate)))


def _check_qubit_id(qubit_id):
    if not isinstance(qubit_id, int):
        raise TypeError(
            "Qubit should be a non negative int, but get {}!".format(
                type(qubit_id)))
    if qubit_id < 0:
        raise ValueError(
            "Qubit should be non negative int, but get {}!".format(qubit_id))
