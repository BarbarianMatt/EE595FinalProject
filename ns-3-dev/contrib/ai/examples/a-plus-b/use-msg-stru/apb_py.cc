/*
 * Copyright (c) 2023 Huazhong University of Science and Technology
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors:  Muyuan Shen <muyuan_shen@hust.edu.cn>
 */

#include "apb.h"

#include <ns3/ai-module.h>

#include <iostream>
#include <pybind11/pybind11.h>

namespace py = pybind11;

PYBIND11_MODULE(ns3ai_apb_py_stru, m)
{
    py::class_<EnvStruct>(m, "PyEnvStruct")
        .def(py::init<>())
        .def_readwrite("mldThptTotal", &EnvStruct::env_mldThptTotal)
        .def_readwrite("mldSuccPrLink1", &EnvStruct::env_mldSuccPrLink1)
        .def_readwrite("mldSuccPrLink2", &EnvStruct::env_mldSuccPrLink2)
        .def_readwrite("mldSuccPrTotal", &EnvStruct::env_mldSuccPrTotal)
        .def_readwrite("mldThptLink1", &EnvStruct::env_mldThptLink1)
        .def_readwrite("mldThptLink2", &EnvStruct::env_mldThptLink2)
        .def_readwrite("mldMeanQueDelayLink1", &EnvStruct::env_mldMeanQueDelayLink1)
        .def_readwrite("mldMeanQueDelayLink2", &EnvStruct::env_mldMeanQueDelayLink2)
        .def_readwrite("mldMeanQueDelayTotal", &EnvStruct::env_mldMeanQueDelayTotal)
        .def_readwrite("mldMeanAccDelayLink1", &EnvStruct::env_mldMeanAccDelayLink1)
        .def_readwrite("mldMeanAccDelayLink2", &EnvStruct::env_mldMeanAccDelayLink2)
        .def_readwrite("mldMeanAccDelayTotal", &EnvStruct::env_mldMeanAccDelayTotal)
        .def_readwrite("mldMeanE2eDelayLink1", &EnvStruct::env_mldMeanE2eDelayLink1)
        .def_readwrite("mldMeanE2eDelayLink2", &EnvStruct::env_mldMeanE2eDelayLink2)
        .def_readwrite("mldMeanE2eDelayTotal", &EnvStruct::env_mldMeanE2eDelayTotal)
        .def_readwrite("mldSecondRawMomentAccDelayLink1", &EnvStruct::env_mldSecondRawMomentAccDelayLink1)
        .def_readwrite("mldSecondRawMomentAccDelayLink2", &EnvStruct::env_mldSecondRawMomentAccDelayLink2)
        .def_readwrite("mldSecondRawMomentAccDelayTotal", &EnvStruct::env_mldSecondRawMomentAccDelayTotal)
        .def_readwrite("mldSecondCentralMomentAccDelayLink1", &EnvStruct::env_mldSecondCentralMomentAccDelayLink1)
        .def_readwrite("mldSecondCentralMomentAccDelayLink2", &EnvStruct::env_mldSecondCentralMomentAccDelayLink2)
        .def_readwrite("mldSecondCentralMomentAccDelayTotal", &EnvStruct::env_mldSecondCentralMomentAccDelayTotal)
        .def_readwrite("rngRun", &EnvStruct::env_rngRun)
        .def_readwrite("simulationTime", &EnvStruct::env_simulationTime)
        .def_readwrite("payloadSize", &EnvStruct::env_payloadSize)
        .def_readwrite("mcs", &EnvStruct::env_mcs)
        .def_readwrite("mcs2", &EnvStruct::env_mcs2)
        .def_readwrite("channelWidth", &EnvStruct::env_channelWidth)
        .def_readwrite("channelWidth2", &EnvStruct::env_channelWidth2)
        .def_readwrite("nMldSta", &EnvStruct::env_nMldSta)
        .def_readwrite("mldPerNodeLambda", &EnvStruct::env_mldPerNodeLambda)
        .def_readwrite("mldProbLink1", &EnvStruct::env_mldProbLink1)
        .def_readwrite("mldAcLink1Int", &EnvStruct::env_mldAcLink1Int)
        .def_readwrite("mldAcLink2Int", &EnvStruct::env_mldAcLink2Int)
        .def_readwrite("acBECwminLink1", &EnvStruct::env_acBECwminLink1)
        .def_readwrite("acBECwStageLink1", &EnvStruct::env_acBECwStageLink1)
        .def_readwrite("acBKCwminLink1", &EnvStruct::env_acBKCwminLink1)
        .def_readwrite("acBKCwStageLink1", &EnvStruct::env_acBKCwStageLink1)
        .def_readwrite("acVICwminLink1", &EnvStruct::env_acVICwminLink1)
        .def_readwrite("acVICwStageLink1", &EnvStruct::env_acVICwStageLink1)
        .def_readwrite("acVOCwminLink1", &EnvStruct::env_acVOCwminLink1)
        .def_readwrite("acVOCwStageLink1", &EnvStruct::env_acVOCwStageLink1)
        .def_readwrite("acBECwminLink2", &EnvStruct::env_acBECwminLink2)
        .def_readwrite("acBECwStageLink2", &EnvStruct::env_acBECwStageLink2)
        .def_readwrite("acBKCwminLink2", &EnvStruct::env_acBKCwminLink2)
        .def_readwrite("acBKCwStageLink2", &EnvStruct::env_acBKCwStageLink2)
        .def_readwrite("acVICwminLink2", &EnvStruct::env_acVICwminLink2)
        .def_readwrite("acVICwStageLink2", &EnvStruct::env_acVICwStageLink2)
        .def_readwrite("acVOCwminLink2", &EnvStruct::env_acVOCwminLink2)
        .def_readwrite("acVOCwStageLink2", &EnvStruct::env_acVOCwStageLink2)
        .def_readwrite("stepNumber", &EnvStruct::env_stepNumber);

    py::class_<ActStruct>(m, "PyActStruct")
        .def(py::init<>())
        .def_readwrite("done_simulation", &ActStruct::act_done_simulation)
        .def_readwrite("end_experiment", &ActStruct::act_end_experiment)
        .def_readwrite("acBECwminLink1", &ActStruct::act_acBECwminLink1)
        .def_readwrite("acBECwminLink2", &ActStruct::act_acBECwminLink2)
        .def_readwrite("acBECwStageLink1", &ActStruct::act_acBECwStageLink1)
        .def_readwrite("simulationTime", &ActStruct::act_simulationTime)
        .def_readwrite("mldPerNodeLambda", &ActStruct::act_mldPerNodeLambda)
        .def_readwrite("totalSteps", &ActStruct::act_totalSteps)
        .def_readwrite("mldProbLink1", &ActStruct::act_mldProbLink1);
        

    py::class_<ns3::Ns3AiMsgInterfaceImpl<EnvStruct, ActStruct>>(m, "Ns3AiMsgInterfaceImpl")
        .def(py::init<bool,
                      bool,
                      bool,
                      uint32_t,
                      const char*,
                      const char*,
                      const char*,
                      const char*>())
        .def("PyRecvBegin", &ns3::Ns3AiMsgInterfaceImpl<EnvStruct, ActStruct>::PyRecvBegin)
        .def("PyRecvEnd", &ns3::Ns3AiMsgInterfaceImpl<EnvStruct, ActStruct>::PyRecvEnd)
        .def("PySendBegin", &ns3::Ns3AiMsgInterfaceImpl<EnvStruct, ActStruct>::PySendBegin)
        .def("PySendEnd", &ns3::Ns3AiMsgInterfaceImpl<EnvStruct, ActStruct>::PySendEnd)
        .def("PyGetFinished", &ns3::Ns3AiMsgInterfaceImpl<EnvStruct, ActStruct>::PyGetFinished)
        .def("GetCpp2PyStruct",
             &ns3::Ns3AiMsgInterfaceImpl<EnvStruct, ActStruct>::GetCpp2PyStruct,
             py::return_value_policy::reference)
        .def("GetPy2CppStruct",
             &ns3::Ns3AiMsgInterfaceImpl<EnvStruct, ActStruct>::GetPy2CppStruct,
             py::return_value_policy::reference);
}
