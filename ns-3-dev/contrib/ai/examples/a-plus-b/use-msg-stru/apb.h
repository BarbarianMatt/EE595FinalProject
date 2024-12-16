/*
 * Copyright (c) 2020-2023 Huazhong University of Science and Technology
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
 * Authors: Pengyu Liu <eic_lpy@hust.edu.cn>
 *          Hao Yin <haoyin@uw.edu>
 *          Muyuan Shen <muyuan_shen@hust.edu.cn>
 */

#ifndef APB_H
#define APB_H

#include <cstdint>

struct EnvStruct
{
    double env_mldSuccPrLink1;
    double env_mldSuccPrLink2;
    double env_mldSuccPrTotal;
    double env_mldThptLink1;
    double env_mldThptLink2;
    double env_mldThptTotal;
    double env_mldMeanQueDelayLink1;
    double env_mldMeanQueDelayLink2;
    double env_mldMeanQueDelayTotal;
    double env_mldMeanAccDelayLink1;
    double env_mldMeanAccDelayLink2;
    double env_mldMeanAccDelayTotal;
    double env_mldMeanE2eDelayLink1;
    double env_mldMeanE2eDelayLink2;
    double env_mldMeanE2eDelayTotal;
    double env_mldSecondRawMomentAccDelayLink1;
    double env_mldSecondRawMomentAccDelayLink2;
    double env_mldSecondRawMomentAccDelayTotal;
    double env_mldSecondCentralMomentAccDelayLink1;
    double env_mldSecondCentralMomentAccDelayLink2;
    double env_mldSecondCentralMomentAccDelayTotal;
    uint32_t env_rngRun;
    double env_simulationTime;
    uint32_t env_payloadSize;
    int env_mcs;
    int env_mcs2;
    int env_channelWidth;
    int env_channelWidth2;
    std::size_t env_nMldSta;
    double env_mldPerNodeLambda;
    double env_mldProbLink1;
    uint8_t env_mldAcLink1Int;
    uint8_t env_mldAcLink2Int;
    uint64_t env_acBECwminLink1;
    uint8_t env_acBECwStageLink1;
    uint64_t env_acBKCwminLink1;
    uint8_t env_acBKCwStageLink1;
    uint64_t env_acVICwminLink1;
    uint8_t env_acVICwStageLink1;
    uint64_t env_acVOCwminLink1;
    uint8_t env_acVOCwStageLink1;
    uint64_t env_acBECwminLink2;
    uint8_t env_acBECwStageLink2;
    uint64_t env_acBKCwminLink2;
    uint8_t env_acBKCwStageLink2;
    uint64_t env_acVICwminLink2;
    uint8_t env_acVICwStageLink2;
    uint64_t env_acVOCwminLink2;
    uint8_t env_acVOCwStageLink2;

    uint8_t env_stepNumber;
};

struct ActStruct
{
    bool act_done_simulation;
    bool act_end_experiment;
    uint8_t act_acBECwStageLink1;
    uint64_t act_acBECwminLink1;
    uint64_t act_acBECwminLink2;
    double act_simulationTime;
    double act_mldPerNodeLambda;
    uint64_t act_totalSteps;
    double act_mldProbLink1;

};

#endif // APB_H
