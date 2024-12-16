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

#include "apb.h"

#include <ns3/ai-module.h>
#include <ns3/command-line.h>

#include <chrono>
#include <iostream>
#include <random>

#define NUM_ENV 100

using namespace ns3;

#include "ns3/attribute-container.h"
#include "ns3/bernoulli_packet_socket_client.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/constant-rate-wifi-manager.h"
#include "ns3/eht-configuration.h"
#include "ns3/eht-phy.h"
#include "ns3/frame-exchange-manager.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/log.h"
#include "ns3/mobility-helper.h"
#include "ns3/multi-model-spectrum-channel.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/packet-socket-client.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/packet-socket-server.h"
#include "ns3/qos-utils.h"
#include "ns3/rng-seed-manager.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/uinteger.h"
#include "ns3/wifi-mac-queue.h"
#include "ns3/wifi-net-device.h"
#include "ns3/wifi-phy-common.h"
#include "ns3/wifi-phy-rx-trace-helper.h"
#include "ns3/wifi-phy.h"
#include "ns3/wifi-tx-stats-helper.h"
#include "ns3/wifi-utils.h"
#include "ns3/yans-wifi-helper.h"

#include <array>
#include <cmath>

#define PI 3.1415926535

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("single-bss-mld");

enum TrafficTypeEnum
{
    TRAFFIC_DETERMINISTIC,
    TRAFFIC_BERNOULLI,
    TRAFFIC_INVALID
};

// Per STA traffic config
struct TrafficConfig
{
    WifiDirection m_dir;
    TrafficTypeEnum m_type;
    AcIndex m_link1Ac;
    AcIndex m_link2Ac;
    double m_lambda;
    double m_determIntervalNs;
    bool m_split;
    double m_prob;
};

using TrafficConfigMap = std::map<uint32_t /* Node ID */, TrafficConfig>;

Time slotTime;


// void
// CheckStats()
// {
//     wifiStats.PrintStatistics();

//     std::ofstream outFile("tx-timeline.txt");
//     outFile << "Start Time,End Time,Source Node,DropReason\n";

//     for (const auto& record : wifiStats.GetPpduRecords())
//     {
//         if (record.m_reason)
//         {
//             outFile << record.m_startTime.GetMilliSeconds()
//                 << "," // Convert Time to a numerical format
//                 << record.m_endTime.GetMilliSeconds()
//                 << "," // Convert Time to a numerical format
//                 << record.m_senderId << "," << record.m_reason << "\n";
//         }
//         else
//         {
//             bool allSuccess = true;
//             for (const auto& status : record.m_statusPerMpdu)
//             {
//                 if (!status)
//                 {
//                     allSuccess = false;
//                 }
//             }
//             if (allSuccess)
//             {
//                 outFile << record.m_startTime.GetMilliSeconds()
//                     << "," // Convert Time to a numerical format
//                     << record.m_endTime.GetMilliSeconds()
//                     << "," // Convert Time to a numerical format
//                     << record.m_senderId << ",success\n";
//             }
//             else
//             {
//                 outFile << record.m_startTime.GetMilliSeconds()
//                     << "," // Convert Time to a numerical format
//                     << record.m_endTime.GetMilliSeconds()
//                     << "," // Convert Time to a numerical format
//                     << record.m_senderId << ",PayloadDecodeError\n";
//             }
//         }
//     }
//     outFile.close();
// }

Ptr<PacketSocketClient>
GetDeterministicClient(const PacketSocketAddress& sockAddr,
                       const std::size_t pktSize,
                       const Time& interval,
                       const Time& start,
                       const AcIndex link1Ac,
                       const bool optionalTid = false,
                       const AcIndex link2Ac = AC_UNDEF,
                       const double optionalPr = 0)
{
    NS_ASSERT(link1Ac != AC_UNDEF);
    const auto link1Tids = wifiAcList.at(link1Ac);
    auto lowTid = link1Tids.GetLowTid();

    auto client = CreateObject<PacketSocketClient>();
    client->SetAttribute("PacketSize", UintegerValue(pktSize));
    client->SetAttribute("MaxPackets", UintegerValue(0));
    client->SetAttribute("Interval", TimeValue(interval));
    client->SetAttribute("Priority", UintegerValue(lowTid));
    if (optionalTid && link2Ac != AC_UNDEF)
    {
        const auto link2Tids = wifiAcList.at(link2Ac);
        auto highTid = link2Tids.GetHighTid();
        client->SetAttribute("OptionalTid", UintegerValue(highTid));
        client->SetAttribute("OptionalTidPr", DoubleValue(optionalPr));
    }
    else
    {
        client->SetAttribute("OptionalTid", UintegerValue(lowTid));
    }
    client->SetRemote(sockAddr);
    client->SetStartTime(start);
    return client;
}

Ptr<BernoulliPacketSocketClient>
GetBernoulliClient(const PacketSocketAddress& sockAddr,
                   const std::size_t pktSize,
                   const double prob,
                   const Time& start,
                   const AcIndex link1Ac,
                   const bool optionalTid = false,
                   const AcIndex link2Ac = AC_UNDEF,
                   const double optionalPr = 0)
{
    NS_ASSERT(link1Ac != AC_UNDEF);
    const auto link1Tids = wifiAcList.at(link1Ac);
    auto lowTid = link1Tids.GetLowTid();

    auto client = CreateObject<BernoulliPacketSocketClient>();
    client->SetAttribute("PacketSize", UintegerValue(pktSize));
    client->SetAttribute("MaxPackets", UintegerValue(0));
    client->SetAttribute("TimeSlot", TimeValue(slotTime));
    client->SetAttribute("BernoulliPr", DoubleValue(prob));
    client->SetAttribute("Priority", UintegerValue(lowTid));
    if (optionalTid && link2Ac != AC_UNDEF)
    {
        const auto link2Tids = wifiAcList.at(link2Ac);
        auto highTid = link2Tids.GetHighTid();
        client->SetAttribute("OptionalTid", UintegerValue(highTid));
        client->SetAttribute("OptionalTidPr", DoubleValue(optionalPr));
    }
    else
    {
        client->SetAttribute("OptionalTid", UintegerValue(lowTid));
    }
    client->SetRemote(sockAddr);
    client->SetStartTime(start);
    return client;
}

void contentionWindowSetup(
        uint64_t &acBECwminLink1, uint8_t &acBECwStageLink1, 
        uint64_t &acBKCwminLink1, uint8_t &acBKCwStageLink1, 
        uint64_t &acVICwminLink1, uint8_t &acVICwStageLink1, 
        uint64_t &acVOCwminLink1, uint8_t &acVOCwStageLink1, 
        uint64_t &acBECwminLink2, uint8_t &acBECwStageLink2, 
        uint64_t &acBKCwminLink2, uint8_t &acBKCwStageLink2, 
        uint64_t &acVICwminLink2, uint8_t &acVICwStageLink2, 
        uint64_t &acVOCwminLink2, uint8_t &acVOCwStageLink2) {
    
    uint64_t acBECwmaxLink1 = acBECwminLink1 * pow(2, acBECwStageLink1);

    uint64_t acBKCwmaxLink1 = acBKCwminLink1 * pow(2, acBKCwStageLink1);

    uint64_t acVICwmaxLink1 = acVICwminLink1 * pow(2, acVICwStageLink1);

    uint64_t acVOCwmaxLink1 = acVOCwminLink1 * pow(2, acVOCwStageLink1);

    uint64_t acBECwmaxLink2 = acBECwminLink2 * pow(2, acBECwStageLink2);

    uint64_t acBKCwmaxLink2 = acBKCwminLink2 * pow(2, acBKCwStageLink2);

    uint64_t acVICwmaxLink2 = acVICwminLink2 * pow(2, acVICwStageLink2);

    uint64_t acVOCwmaxLink2 = acVOCwminLink2 * pow(2, acVOCwStageLink2);


    std::string prefixStr = "/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/";
    std::list<uint64_t> acBeCwmins = {acBECwminLink1-1, acBECwminLink2-1};
    Config::Set(prefixStr + "BE_Txop/MinCws", AttributeContainerValue<UintegerValue>(acBeCwmins));
    std::list<uint64_t> acBeCwmaxs = {acBECwmaxLink1-1, acBECwmaxLink2-1};
    Config::Set(prefixStr + "BE_Txop/MaxCws", AttributeContainerValue<UintegerValue>(acBeCwmaxs));
    std::list<uint64_t> acBkCwmins = {acBKCwminLink1-1, acBKCwminLink2-1};
    Config::Set(prefixStr + "BK_Txop/MinCws", AttributeContainerValue<UintegerValue>(acBkCwmins));
    std::list<uint64_t> acBkCwmaxs = {acBKCwmaxLink1-1, acBKCwmaxLink2-1};
    Config::Set(prefixStr + "BK_Txop/MaxCws", AttributeContainerValue<UintegerValue>(acBkCwmaxs));
    std::list<uint64_t> acViCwmins = {acVICwminLink1-1, acVICwminLink2-1};
    Config::Set(prefixStr + "VI_Txop/MinCws", AttributeContainerValue<UintegerValue>(acViCwmins));
    std::list<uint64_t> acViCwmaxs = {acVICwmaxLink1-1, acVICwmaxLink2-1};
    Config::Set(prefixStr + "VI_Txop/MaxCws", AttributeContainerValue<UintegerValue>(acViCwmaxs));
    std::list<uint64_t> acVoCwmins = {acVOCwminLink1-1, acVOCwminLink2-1};
    Config::Set(prefixStr + "VO_Txop/MinCws", AttributeContainerValue<UintegerValue>(acVoCwmins));
    std::list<uint64_t> acVoCwmaxs = {acVOCwmaxLink1-1, acVOCwmaxLink2-1};
    Config::Set(prefixStr + "VO_Txop/MaxCws", AttributeContainerValue<UintegerValue>(acVoCwmaxs));


}

void UpdateBernoulliClientProbability(NodeContainer& nodeContainer, double newProbability) {
    for (auto nodeIt = nodeContainer.Begin(); nodeIt != nodeContainer.End(); ++nodeIt) {
        Ptr<Node> node = *nodeIt;

        for (uint32_t deviceIndex = 0; deviceIndex < node->GetNDevices(); ++deviceIndex) {
            Ptr<NetDevice> netDevice = node->GetDevice(deviceIndex);

            // Check if the device is a WifiNetDevice
            Ptr<WifiNetDevice> wifiDevice = DynamicCast<WifiNetDevice>(netDevice);
            if (!wifiDevice) {
                continue;
            }

            // Check if this node has applications installed
            for (uint32_t appIndex = 0; appIndex < node->GetNApplications(); ++appIndex) {
                Ptr<Application> app = node->GetApplication(appIndex);

                // Check if the application is a BernoulliPacketSocketClient
                Ptr<BernoulliPacketSocketClient> bernoulliClient = DynamicCast<BernoulliPacketSocketClient>(app);
                if (bernoulliClient) {
                    // // Update the BernoulliPr attribute
                    // bernoulliClient->SetAttribute("BernoulliPr", DoubleValue(newProbability));
                    bernoulliClient->SetAttribute("OptionalTidPr", DoubleValue(1-newProbability));

                    // // Logging (optional for debugging purposes)
                    // NS_LOG_INFO("Updated BernoulliPr for client on node " << node->GetId() << 
                    //              " to " << newProbability);
                }
            }
        }
    }
}

std::tuple<NetDeviceContainer, NodeContainer , NodeContainer> Setup(
        bool unlimitedAmpdu, uint8_t maxMpdusInAmpdu, bool useRts, double bssRadius, double frequency, double frequency2, int gi, double apTxPower, double staTxPower, uint8_t nLinks,
        uint32_t &rngRun, double &simulationTime, uint32_t &payloadSize, 
        int &mcs, int &mcs2, int &channelWidth,
        int &channelWidth2, std::size_t &nMldSta, 
        double &mldPerNodeLambda, double &mldProbLink1, 
        uint8_t &mldAcLink1Int, uint8_t &mldAcLink2Int, 
        uint64_t &acBECwminLink1, uint8_t &acBECwStageLink1, 
        uint64_t &acBKCwminLink1, uint8_t &acBKCwStageLink1, 
        uint64_t &acVICwminLink1, uint8_t &acVICwStageLink1, 
        uint64_t &acVOCwminLink1, uint8_t &acVOCwStageLink1, 
        uint64_t &acBECwminLink2, uint8_t &acBECwStageLink2, 
        uint64_t &acBKCwminLink2, uint8_t &acBKCwStageLink2, 
        uint64_t &acVICwminLink2, uint8_t &acVICwStageLink2, 
        uint64_t &acVOCwminLink2, uint8_t &acVOCwStageLink2) {
    RngSeedManager::SetSeed(rngRun);
    RngSeedManager::SetRun(rngRun);

    uint32_t randomStream = rngRun;

    auto mldAcLink1 = static_cast<AcIndex>(mldAcLink1Int);
    auto mldAcLink2 = static_cast<AcIndex>(mldAcLink2Int);


    if (useRts)
    {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("0"));
        Config::SetDefault("ns3::WifiDefaultProtectionManager::EnableMuRts", BooleanValue(true));
    }

    // Disable fragmentation
    Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold",
                       UintegerValue(payloadSize + 100));

    // Make retransmissions persistent
    Config::SetDefault("ns3::WifiRemoteStationManager::MaxSlrc",
                       UintegerValue(std::numeric_limits<uint32_t>::max()));
    Config::SetDefault("ns3::WifiRemoteStationManager::MaxSsrc",
                       UintegerValue(std::numeric_limits<uint32_t>::max()));

    // Set infinitely long queue
    Config::SetDefault(
        "ns3::WifiMacQueue::MaxSize",
        QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, std::numeric_limits<uint32_t>::max())));

    // Don't drop MPDUs due to long stay in queue
    // Config::SetDefault("ns3::WifiMacQueue::MaxDelay", TimeValue(Seconds(2 * simulationTime)));
    Config::SetDefault("ns3::WifiMacQueue::MaxDelay", TimeValue(Seconds(0.5)));

    NodeContainer apNodeCon;
    NodeContainer mldNodeCon;
    apNodeCon.Create(1);
    uint32_t nStaTotal = nMldSta;
    mldNodeCon.Create(nStaTotal);

    NetDeviceContainer apDevCon;
    NetDeviceContainer mldDevCon;

    WifiHelper mldWifiHelp;
    mldWifiHelp.SetStandard(WIFI_STANDARD_80211be);

    // Get channel string for MLD STA
    std::array<std::string, 2> mldChannelStr;
    for (auto freq : {frequency, frequency2})
    {
        std::string widthStr = (nLinks == 0)
                                   ? std::to_string(channelWidth)
                                   : std::to_string(channelWidth2);
        auto linkMcs = (nLinks == 0) ? mcs : mcs2;
        std::string dataModeStr = "EhtMcs" + std::to_string(linkMcs);
        mldChannelStr[nLinks] = "{0, " + widthStr + ", ";
        if (freq == 6)
        {
            mldChannelStr[nLinks] += "BAND_6GHZ, 0}";
            mldWifiHelp.SetRemoteStationManager(nLinks,
                                                "ns3::ConstantRateWifiManager",
                                                "DataMode",
                                                StringValue(dataModeStr),
                                                "ControlMode",
                                                StringValue("OfdmRate24Mbps"));
        }
        else if (freq == 5)
        {
            mldChannelStr[nLinks] += "BAND_5GHZ, 0}";
            mldWifiHelp.SetRemoteStationManager(nLinks,
                                                "ns3::ConstantRateWifiManager",
                                                "DataMode",
                                                StringValue(dataModeStr),
                                                "ControlMode",
                                                StringValue("OfdmRate24Mbps"));
        }
        else
        {
            std::cerr << "Unsupported frequency for reference BSS\n" << std::endl;
            // return std::make_tuple(allNetDevices,  allNodeCon);
        }
        nLinks++;
    }

    SpectrumWifiPhyHelper mldPhyHelp(nLinks);
    // mldPhyHelp.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    Ptr<MultiModelSpectrumChannel> phy5ghzSpectrumChannel = CreateObject<
        MultiModelSpectrumChannel>();
    // Reference Loss for Friss at 1 m with 5.15 GHz
    Ptr<LogDistancePropagationLossModel> phy5ghzLossModel =
        CreateObject<LogDistancePropagationLossModel>();
    phy5ghzLossModel->SetAttribute("Exponent", DoubleValue(3.5));
    phy5ghzLossModel->SetAttribute("ReferenceDistance", DoubleValue(1.0));
    phy5ghzLossModel->SetAttribute("ReferenceLoss", DoubleValue(50));
    phy5ghzSpectrumChannel->AddPropagationLossModel(phy5ghzLossModel);

    Ptr<MultiModelSpectrumChannel> phy6ghzSpectrumChannel = CreateObject<
        MultiModelSpectrumChannel>();
    // Reference Loss for Friss at 1 m with 6.0 GHz
    Ptr<LogDistancePropagationLossModel> phy6ghzLossModel =
        CreateObject<LogDistancePropagationLossModel>();
    phy6ghzLossModel->SetAttribute("Exponent", DoubleValue(2.0));
    phy6ghzLossModel->SetAttribute("ReferenceDistance", DoubleValue(1.0));
    phy6ghzLossModel->SetAttribute("ReferenceLoss", DoubleValue(49.013));
    phy6ghzSpectrumChannel->AddPropagationLossModel(phy6ghzLossModel);

    mldPhyHelp.AddChannel(phy5ghzSpectrumChannel, WIFI_SPECTRUM_5_GHZ);
    mldPhyHelp.AddChannel(phy6ghzSpectrumChannel, WIFI_SPECTRUM_6_GHZ);

    for (uint8_t linkId = 0; linkId < nLinks; linkId++)
    {
        mldPhyHelp.Set(linkId, "ChannelSettings", StringValue(mldChannelStr[linkId]));
    }

    WifiMacHelper mldMacHelp;
    Ssid bssSsid = Ssid("BSS-SLD-MLD-COEX");

    // Set up MLD STAs
    mldMacHelp.SetType("ns3::StaWifiMac",
                       "MaxMissedBeacons",
                       UintegerValue(std::numeric_limits<uint32_t>::max()),
                       "Ssid",
                       SsidValue(bssSsid));
    mldPhyHelp.Set("TxPowerStart", DoubleValue(staTxPower));
    mldPhyHelp.Set("TxPowerEnd", DoubleValue(staTxPower));
    mldDevCon = mldWifiHelp.Install(mldPhyHelp, mldMacHelp, mldNodeCon);

    uint64_t beaconInterval = std::min<uint64_t>(
        (ceil((simulationTime * 1000000) / 1024) * 1024),
        (65535 * 1024)); // beacon interval needs to be a multiple of time units (1024 us)

    // Set up AP
    mldMacHelp.SetType("ns3::ApWifiMac",
                       "BeaconInterval",
                       TimeValue(MicroSeconds(beaconInterval)),
                       "EnableBeaconJitter",
                       BooleanValue(false),
                       "Ssid",
                       SsidValue(bssSsid));
    mldPhyHelp.Set("TxPowerStart", DoubleValue(apTxPower));
    mldPhyHelp.Set("TxPowerEnd", DoubleValue(apTxPower));
    apDevCon = mldWifiHelp.Install(mldPhyHelp, mldMacHelp, apNodeCon);

    NetDeviceContainer allNetDevices;
    allNetDevices.Add(apDevCon);
    allNetDevices.Add(mldDevCon);

    mldWifiHelp.AssignStreams(allNetDevices, randomStream);

    // Enable TID-to-Link Mapping for AP and MLD STAs
    for (auto i = allNetDevices.Begin(); i != allNetDevices.End(); ++i)
    {
        auto wifiDev = DynamicCast<WifiNetDevice>(*i);
        wifiDev->GetMac()->GetEhtConfiguration()->SetAttribute("TidToLinkMappingNegSupport",
                                                               EnumValue(
                                                                   WifiTidToLinkMappingNegSupport::ANY_LINK_SET));
    }

    // Map all low TIDs to link 1 (whose linkId=0), all high TIDs to link 2 (whose linkId=1)
    // E.g. BE traffic with TIDs 0 and 3 are sents to Link 0 and 1, respectively.
    // Adding mapping information at MLD STAs side
    // NOTE: only consider UL data traffic for now
    std::string mldMappingStr = "0,1,4,6 0; 3,2,5,7 1";
    // To use greedy (ns-3 default way for traffic-to-link allocation):
    // (1) use the default string below
    // (2) need to also set mldProbLink1 to 0 or 1 to have only one L-MAC queue
    std::string mldMappingStrDefault = "0,1,2,3,4,5,6,7 0,1";
    for (auto i = mldDevCon.Begin(); i != mldDevCon.End(); ++i)
    {
        auto wifiDev = DynamicCast<WifiNetDevice>(*i);
        wifiDev->GetMac()->SetAttribute("ActiveProbing", BooleanValue(true));
        // wifiDev->GetMac()->GetEhtConfiguration()->SetAttribute("TidToLinkMappingDl", StringValue(mldMappingStr));
        wifiDev->GetMac()->GetEhtConfiguration()->SetAttribute(
            "TidToLinkMappingUl",
            StringValue(mldMappingStr));
    }

    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HeConfiguration/GuardInterval",
                TimeValue(NanoSeconds(gi)));

    if (!unlimitedAmpdu)
    {
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_MaxAmpduSize",
                    UintegerValue(maxMpdusInAmpdu * (payloadSize + 50)));
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BK_MaxAmpduSize",
                    UintegerValue(maxMpdusInAmpdu * (payloadSize + 50)));
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/VO_MaxAmpduSize",
                    UintegerValue(maxMpdusInAmpdu * (payloadSize + 50)));
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/VI_MaxAmpduSize",
                    UintegerValue(maxMpdusInAmpdu * (payloadSize + 50)));
    }

    // set cwmins and cwmaxs for all Access Categories on ALL devices
    // (incl. AP because STAs sync with AP via association, probe, and beacon)
    contentionWindowSetup(
        acBECwminLink1, acBECwStageLink1, 
        acBKCwminLink1, acBKCwStageLink1, 
        acVICwminLink1, acVICwStageLink1, 
        acVOCwminLink1, acVOCwStageLink1, 
        acBECwminLink2, acBECwStageLink2, 
        acBKCwminLink2, acBKCwStageLink2, 
        acVICwminLink2, acVICwStageLink2, 
        acVOCwminLink2, acVOCwStageLink2

    );

    std::string prefixStr = "/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/";

    // set all aifsn to be 2 (so that all aifs equal to legacy difs)
    std::list<uint64_t> aifsnList = {2, 2};
    Config::Set(prefixStr + "BE_Txop/Aifsns", AttributeContainerValue<UintegerValue>(aifsnList));
    Config::Set(prefixStr + "BK_Txop/Aifsns", AttributeContainerValue<UintegerValue>(aifsnList));
    Config::Set(prefixStr + "VI_Txop/Aifsns", AttributeContainerValue<UintegerValue>(aifsnList));
    Config::Set(prefixStr + "VO_Txop/Aifsns", AttributeContainerValue<UintegerValue>(aifsnList));
    std::list<Time> txopLimitList = {MicroSeconds(0), MicroSeconds(0)};
    Config::Set(prefixStr + "BE_Txop/TxopLimits",
                AttributeContainerValue<TimeValue>(txopLimitList));
    Config::Set(prefixStr + "BK_Txop/TxopLimits",
                AttributeContainerValue<TimeValue>(txopLimitList));
    Config::Set(prefixStr + "VI_Txop/TxopLimits",
                AttributeContainerValue<TimeValue>(txopLimitList));
    Config::Set(prefixStr + "VO_Txop/TxopLimits",
                AttributeContainerValue<TimeValue>(txopLimitList));

    auto staWifiManager =
        DynamicCast<ConstantRateWifiManager>(DynamicCast<WifiNetDevice>(mldDevCon.Get(0))
            ->GetRemoteStationManager());
    slotTime = staWifiManager->GetPhy()->GetSlot();
    auto sifsTime = staWifiManager->GetPhy()->GetSifs();
    auto difsTime = sifsTime + 2 * slotTime;

    // mobility.
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    double angle = (static_cast<double>(360) / nStaTotal);
    positionAlloc->Add(Vector(1.0, 1.0, 0.0));
    for (uint32_t i = 0; i < nStaTotal; ++i)
    {
        positionAlloc->Add(Vector(1.0 + (bssRadius * cos((i * angle * PI) / 180)),
                                  1.0 + (bssRadius * sin((i * angle * PI) / 180)),
                                  0.0));
    }
    mobility.SetPositionAllocator(positionAlloc);
    NodeContainer allNodeCon(apNodeCon, mldNodeCon);
    mobility.Install(allNodeCon);

    /* Setting applications */
    // random start time
    Ptr<UniformRandomVariable> startTime = CreateObject<UniformRandomVariable>();
    startTime->SetAttribute("Stream", IntegerValue(randomStream));
    startTime->SetAttribute("Min", DoubleValue(0.0));
    startTime->SetAttribute("Max", DoubleValue(1.0));

    // setup PacketSocketServer for every node
    PacketSocketHelper packetSocket;
    packetSocket.Install(allNodeCon);
    for (auto nodeIt = allNodeCon.Begin(); nodeIt != allNodeCon.End(); ++nodeIt)
    {
        PacketSocketAddress srvAddr;
        auto device = DynamicCast<WifiNetDevice>((*nodeIt)->GetDevice(0));
        srvAddr.SetSingleDevice(device->GetIfIndex());
        srvAddr.SetProtocol(1);
        auto psServer = CreateObject<PacketSocketServer>();
        psServer->SetLocal(srvAddr);
        (*nodeIt)->AddApplication(psServer);
        psServer->SetStartTime(Seconds(0)); // all servers start at 0 s
    }

    // set the configuration pairs for applications (UL, Bernoulli arrival)
    TrafficConfigMap trafficConfigMap;
    double mldDetermIntervalNs = slotTime.GetNanoSeconds() / mldPerNodeLambda;
    for (uint32_t i = 0; i < nStaTotal; ++i)
    {
        trafficConfigMap[i] = {WifiDirection::UPLINK, TRAFFIC_BERNOULLI, mldAcLink1, mldAcLink2,
                               mldPerNodeLambda, mldDetermIntervalNs,
                               true, 1 - mldProbLink1};
    }
    // next, setup clients according to the config
    for (uint32_t i = 0; i < nStaTotal; ++i)
    {
        auto mapIt = trafficConfigMap.find(i);
        Ptr<Node> clientNode = (mapIt->second.m_dir == WifiDirection::UPLINK)
                                   ? mldNodeCon.Get(i)
                                   : apNodeCon.Get(0);
        Ptr<WifiNetDevice> clientDevice = DynamicCast<WifiNetDevice>(clientNode->GetDevice(0));
        Ptr<Node> serverNode = (mapIt->second.m_dir == WifiDirection::UPLINK)
                                   ? apNodeCon.Get(0)
                                   : mldNodeCon.Get(i);
        Ptr<WifiNetDevice> serverDevice = DynamicCast<WifiNetDevice>(serverNode->GetDevice(0));

        switch (mapIt->second.m_type)
        {
        case TRAFFIC_DETERMINISTIC: {
            PacketSocketAddress sockAddr;
            sockAddr.SetSingleDevice(clientDevice->GetIfIndex());
            sockAddr.SetPhysicalAddress(serverDevice->GetAddress());
            sockAddr.SetProtocol(1);
            clientNode->AddApplication(GetDeterministicClient(sockAddr,
                                                              payloadSize,
                                                              NanoSeconds(
                                                                  mapIt->second.m_determIntervalNs),
                                                              Seconds(startTime->GetValue()),
                                                              mapIt->second.m_link1Ac,
                                                              mapIt->second.m_split,
                                                              mapIt->second.m_link2Ac,
                                                              mapIt->second.m_prob));
            break;
        }
        case TRAFFIC_BERNOULLI: {
            PacketSocketAddress sockAddr;
            sockAddr.SetSingleDevice(clientDevice->GetIfIndex());
            sockAddr.SetPhysicalAddress(serverDevice->GetAddress());
            sockAddr.SetProtocol(1);
            clientNode->AddApplication(GetBernoulliClient(sockAddr,
                                                          payloadSize,
                                                          mapIt->second.m_lambda,
                                                          Seconds(startTime->GetValue()),
                                                          mapIt->second.m_link1Ac,
                                                          mapIt->second.m_split,
                                                          mapIt->second.m_link2Ac,
                                                          mapIt->second.m_prob));
            break;
        }
        default: {
            std::cerr << "traffic type " << mapIt->second.m_type << " not supported\n";
            break;
        }
        }
    }
    return std::make_tuple(allNetDevices,  allNodeCon, mldNodeCon);
}

void EventCallback (std::string context, Time time)
{
    std::cout << "Event scheduled at time: " << time.GetSeconds() << " seconds" << std::endl;
}


int RunSimulation(int argc, char *argv[]) {

    std::ofstream g_fileSummary;
    g_fileSummary.open("wifi-mld.dat", std::ofstream::app);
    bool printTxStats{false};
    bool printTxStatsSingleLine{false};
    // bool printRxStats{false};

    // Will not change
    bool unlimitedAmpdu{true};
    uint8_t maxMpdusInAmpdu = 0;
    bool useRts{false};
    double bssRadius{0.001};
    double frequency{5};
    double frequency2{6};
    int gi = 800;
    double apTxPower = 20;
    double staTxPower = 20;
    uint8_t nLinks = 0;

    // Input params
    // uint32_t rngRun{6};
    // double simulationTime{10}; // seconds
    // uint32_t payloadSize = 1500;
    // int mcs{6};
    // int mcs2{6};
    // int channelWidth = 20;
    // int channelWidth2 = 20;
    // MLD STAs
    // std::size_t nMldSta{5};
    // double mldPerNodeLambda{0.00001};
    // double mldProbLink1{0.5}; // prob_link1 + prob_link2 = 1
    // uint8_t mldAcLink1Int{AC_BE};
    // uint8_t mldAcLink2Int{AC_BE};
    // EDCA configuration for CWmins, CWmaxs
    // uint64_t acBECwminLink1{16};
    // uint8_t acBECwStageLink1{6};
    // uint64_t acBECwminLink2{16};
    // uint8_t acBECwStageLink2{6};
    // uint64_t acBKCwminLink1{16};
    // uint8_t acBKCwStageLink1{6};
    // uint64_t acBKCwminLink2{16};
    // uint8_t acBKCwStageLink2{6};
    // uint64_t acVICwminLink1{16};
    // uint8_t acVICwStageLink1{6};
    // uint64_t acVICwminLink2{16};
    // uint8_t acVICwStageLink2{6};
    // uint64_t acVOCwminLink1{16};
    // uint8_t acVOCwStageLink1{6};
    // uint64_t acVOCwminLink2{16};
    // uint8_t acVOCwStageLink2{6};


    ns3::CommandLine cmd;
    int num_env = 100;
    cmd.AddValue("num_env", "Number of environments", num_env);
    std::string m_segmentName = "My_Seg";
    cmd.AddValue("m_segmentName", "Name of Segment0", m_segmentName);
    std::string m_cpp2pyMsgName = "My_Cpp_to_Python_Msg";
    cmd.AddValue("m_cpp2pyMsgName", "Name of Segment1", m_cpp2pyMsgName);
    std::string m_py2cppMsgName = "My_Python_to_Cpp_Msg";
    cmd.AddValue("m_py2cppMsgName", "Name of Segment2", m_py2cppMsgName);
    std::string m_lockableName = "My_Lockable";
    cmd.AddValue("m_lockableName", "Name of Segment3", m_lockableName);
    uint32_t rngRun = 1;
    cmd.AddValue("rngRun", "Seed for simulation", rngRun);
    double stepSize = 1.0;
    cmd.AddValue("simulationTime", "Simulation time in seconds", stepSize);
    uint32_t payloadSize = 1500;
    cmd.AddValue("payloadSize", "Application payload size in Bytes", payloadSize);
    int mcs = 6;
    cmd.AddValue("mcs", "MCS for link 1", mcs);
    int mcs2 = 6;
    cmd.AddValue("mcs2", "MCS for link 2", mcs2);
    int channelWidth = 20;
    cmd.AddValue("channelWidth", "Bandwidth for link 1", channelWidth);
    int channelWidth2 = 20;
    cmd.AddValue("channelWidth2", "Bandwidth for link 2", channelWidth2);
    std::size_t nMldSta = 1;
    cmd.AddValue("nMldSta", "Number of MLD STAs", nMldSta);
    double mldPerNodeLambda = 0.00001;
    cmd.AddValue("mldPerNodeLambda", "Per node arrival rate of MLD STAs", mldPerNodeLambda);
    double mldProbLink1 = 0.5;
    cmd.AddValue("mldProbLink1", "MLD's splitting probability on link 1", mldProbLink1);
    uint8_t mldAcLink1Int = AC_BE;
    cmd.AddValue("mldAcLink1Int", "AC of MLD on link 1", mldAcLink1Int);
    uint8_t mldAcLink2Int = AC_BE;
    cmd.AddValue("mldAcLink2Int", "AC of MLD on link 2", mldAcLink2Int);
    uint64_t acBECwminLink1 = 16;
    cmd.AddValue("acBECwminLink1", "Initial CW for AC_BE on link 1", acBECwminLink1);
    uint8_t acBECwStageLink1 = 6;
    cmd.AddValue("acBECwStageLink1", "Cutoff Stage for AC_BE on link 1", acBECwStageLink1);
    uint64_t acBKCwminLink1 = 16;
    cmd.AddValue("acBKCwminLink1", "Initial CW for AC_BK on link 1", acBKCwminLink1);
    uint8_t acBKCwStageLink1 = 6;
    cmd.AddValue("acBKCwStageLink1", "Cutoff Stage for AC_BK on link 1", acBKCwStageLink1);
    uint64_t acVICwminLink1 = 16;
    cmd.AddValue("acVICwminLink1", "Initial CW for AC_VI on link 1", acVICwminLink1);
    uint8_t acVICwStageLink1 = 6;
    cmd.AddValue("acVICwStageLink1", "Cutoff Stage for AC_VI on link 1", acVICwStageLink1);
    uint64_t acVOCwminLink1 = 16;
    cmd.AddValue("acVOCwminLink1", "Initial CW for AC_VO on link 1", acVOCwminLink1);
    uint8_t acVOCwStageLink1 = 6;
    cmd.AddValue("acVOCwStageLink1", "Cutoff Stage for AC_VO on link 1", acVOCwStageLink1);
    uint64_t acBECwminLink2 = 16;
    cmd.AddValue("acBECwminLink2", "Initial CW for AC_BE on link 2", acBECwminLink2);
    uint8_t acBECwStageLink2 = 6;
    cmd.AddValue("acBECwStageLink2", "Cutoff Stage for AC_BE on link 2", acBECwStageLink2);
    uint64_t acBKCwminLink2 = 16;
    cmd.AddValue("acBKCwminLink2", "Initial CW for AC_BK on link 2", acBKCwminLink2);
    uint8_t acBKCwStageLink2 = 6;
    cmd.AddValue("acBKCwStageLink2", "Cutoff Stage for AC_BK on link 2", acBKCwStageLink2);
    uint64_t acVICwminLink2 = 16;
    cmd.AddValue("acVICwminLink2", "Initial CW for AC_VI on link 2", acVICwminLink2);
    uint8_t acVICwStageLink2 = 6;
    cmd.AddValue("acVICwStageLink2", "Cutoff Stage for AC_VI on link 2", acVICwStageLink2);
    uint64_t acVOCwminLink2 = 16;
    cmd.AddValue("acVOCwminLink2", "Initial CW for AC_VO on link 2", acVOCwminLink2);
    uint8_t acVOCwStageLink2 = 6;
    cmd.AddValue("acVOCwStageLink2", "Cutoff Stage for AC_VO on link 2", acVOCwStageLink2);

    uint64_t totalSteps = 10;
    double simulationTime = totalSteps * stepSize;



    cmd.Parse(argc, argv);

    auto interface = Ns3AiMsgInterface::Get();
    interface->SetIsMemoryCreator(false);
    interface->SetUseVector(false);
    interface->SetHandleFinish(true);
    interface->SetNames(m_segmentName,m_cpp2pyMsgName,m_py2cppMsgName,m_lockableName);

    Ns3AiMsgInterfaceImpl<EnvStruct, ActStruct>* msgInterface =
        interface->GetInterface<EnvStruct, ActStruct>();

    // unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    // std::mt19937 gen(seed);
    // std::uniform_int_distribution<int> distrib(1, 10);


    auto [allNetDevices, allNodeCon, mldNodeCon] = Setup(
        unlimitedAmpdu, maxMpdusInAmpdu, useRts, bssRadius, frequency, frequency2, gi, apTxPower, staTxPower, nLinks,
        rngRun, simulationTime, payloadSize, mcs, mcs2, channelWidth, channelWidth2, nMldSta, mldPerNodeLambda, mldProbLink1, mldAcLink1Int, mldAcLink2Int,
        acBECwminLink1, acBECwStageLink1, 
        acBKCwminLink1, acBKCwStageLink1, 
        acVICwminLink1, acVICwStageLink1, 
        acVOCwminLink1, acVOCwStageLink1, 
        acBECwminLink2, acBECwStageLink2, 
        acBKCwminLink2, acBKCwStageLink2, 
        acVICwminLink2, acVICwStageLink2, 
        acVOCwminLink2, acVOCwStageLink2);
        


    // Simulator::Stop(Seconds(stepSize));
    // Simulator::Run();


    bool loop = true;
    

    uint8_t stepNumber= 0;

    while (loop)
    {

        // std::cout << "simulationTime: " << simulationTime << std::endl << std::flush;

        msgInterface->CppRecvBegin();
        bool end_experiment = msgInterface->GetPy2CppStruct()->act_end_experiment;
        bool done_simulation = msgInterface->GetPy2CppStruct()->act_done_simulation;
        acBECwStageLink1 = msgInterface->GetPy2CppStruct()->act_acBECwStageLink1;
        acBECwminLink1 = msgInterface->GetPy2CppStruct()->act_acBECwminLink1;
        acBECwminLink2 = msgInterface->GetPy2CppStruct()->act_acBECwminLink2;

        mldPerNodeLambda = msgInterface->GetPy2CppStruct()-> act_mldPerNodeLambda;
        stepSize = msgInterface->GetPy2CppStruct()-> act_simulationTime;
        totalSteps =  msgInterface->GetPy2CppStruct()-> act_totalSteps;
        mldProbLink1 = msgInterface->GetPy2CppStruct()-> act_mldProbLink1;

        msgInterface->CppRecvEnd();
        simulationTime = stepSize * totalSteps;

        if (end_experiment){
            loop = false;
            break;
        }
        else if (done_simulation){
            // std::cout << "Done Triggered!" << std::endl << std::flush;
            Simulator::Destroy();

            std::tie(allNetDevices, allNodeCon, mldNodeCon) = Setup(
                unlimitedAmpdu, maxMpdusInAmpdu, useRts, bssRadius, frequency, frequency2, gi, apTxPower, staTxPower, nLinks,
                rngRun, simulationTime, payloadSize, mcs, mcs2, channelWidth, channelWidth2, nMldSta, mldPerNodeLambda, mldProbLink1, mldAcLink1Int, mldAcLink2Int,
                acBECwminLink1, acBECwStageLink1, 
                acBKCwminLink1, acBKCwStageLink1, 
                acVICwminLink1, acVICwStageLink1, 
                acVOCwminLink1, acVOCwStageLink1, 
                acBECwminLink2, acBECwStageLink2, 
                acBKCwminLink2, acBKCwStageLink2, 
                acVICwminLink2, acVICwStageLink2, 
                acVOCwminLink2, acVOCwStageLink2);

            stepNumber= 0;

            // WifiTxStatsHelper wifiTxStats;
            // WifiPhyRxTraceHelper wifiStats;
            // wifiTxStats.Enable(allNetDevices);

            // wifiStats.Enable(allNodeCon);

            // Simulator::Stop(Seconds(5));
            // Simulator::Run();
        }

        contentionWindowSetup(
            acBECwminLink1, acBECwStageLink1, 
            acBKCwminLink1, acBKCwStageLink1, 
            acVICwminLink1, acVICwStageLink1, 
            acVOCwminLink1, acVOCwStageLink1, 
            acBECwminLink2, acBECwStageLink2, 
            acBKCwminLink2, acBKCwStageLink2, 
            acVICwminLink2, acVICwStageLink2, 
            acVOCwminLink2, acVOCwStageLink2

        );

        UpdateBernoulliClientProbability(mldNodeCon, mldProbLink1);

        // std::cout << "Now: " << Simulator::Now() << std::endl << std::flush;
        // std::cout << "stepSize: " << stepSize << std::endl << std::flush;

        WifiTxStatsHelper wifiTxStats;
        WifiPhyRxTraceHelper wifiStats;
        wifiTxStats.Enable(allNetDevices);
        wifiStats.Enable(allNodeCon);

        wifiTxStats.Start(Seconds(0.05));
        wifiTxStats.Stop(Seconds(stepSize));


        // RX stats
        wifiStats.Start(Seconds(0.05));
        wifiStats.Stop(Seconds(stepSize));

        // LogComponentEnable("Simulator", LOG_LEVEL_INFO);
        // Config::Connect ("/NodeList/*/$ns3::ApplicationList/*/Rx", MakeCallback(&EventCallback));



        // if (printRxStats)
        // {
        //     Simulator::Schedule(Seconds(5 + simulationTime), &CheckStats);
        // }

        // mldPhyHelp.EnablePcap("single-bss-coex", allNetDevices);
        // AsciiTraceHelper asciiTrace;
        // mldPhyHelp.EnableAsciiAll(asciiTrace.CreateFileStream("single-bss-coex.tr"));

        Simulator::Stop(Seconds(stepSize));
        Simulator::Run();

        // std::cout << "Now (After): " << Simulator::Now() << std::endl << std::flush;



        auto finalResults = wifiTxStats.GetStatistics();
        auto successInfo = wifiTxStats.GetSuccessInfoMap();

        // total and mean delay calculation per node and link
        std::map<uint32_t /* Node ID */, std::map<uint8_t /* Link ID */, std::vector<double> > >
            enqueueTimeMap;
        std::map<uint32_t /* Node ID */, std::map<uint8_t /* Link ID */, std::vector<double> > >
            dequeueTimeMap;
        std::map<uint32_t /* Node ID */, std::map<uint8_t /* Link ID */, std::vector<double> > >
            holTimeMap;
        for (const auto& nodeMap : successInfo)
        {
            for (const auto& linkMap : nodeMap.second)
            {
                for (const auto& record : linkMap.second)
                {
                    enqueueTimeMap[nodeMap.first][linkMap.first].emplace_back(record.m_enqueueMs);
                    dequeueTimeMap[nodeMap.first][linkMap.first].emplace_back(record.m_dequeueMs);
                }
                for (uint32_t i = 0; i < enqueueTimeMap[nodeMap.first][linkMap.first].size(); ++i)
                {
                    if (i == 0)
                    {
                        // This value is false (some data packet may be already in queue
                        // because our stats did not start at 0 second), and will be removed later
                        holTimeMap[nodeMap.first][linkMap.first].emplace_back(
                            enqueueTimeMap[nodeMap.first][linkMap.first][i]);
                    }
                    else
                    {
                        holTimeMap[nodeMap.first][linkMap.first].emplace_back(
                            std::max(enqueueTimeMap[nodeMap.first][linkMap.first][i],
                                    dequeueTimeMap[nodeMap.first][linkMap.first][i - 1]));
                    }
                }
                // remove the first element
                enqueueTimeMap[nodeMap.first][linkMap.first].erase(
                    enqueueTimeMap[nodeMap.first][linkMap.first].begin());
                dequeueTimeMap[nodeMap.first][linkMap.first].erase(
                    dequeueTimeMap[nodeMap.first][linkMap.first].begin());
                holTimeMap[nodeMap.first][linkMap.first].erase(
                    holTimeMap[nodeMap.first][linkMap.first].begin());
            }
        }
        std::map<uint32_t /* Node ID */, std::map<uint8_t /* Link ID */, double> >
            totalQueuingDelayPerNodeLink;
        std::map<uint32_t /* Node ID */, std::map<uint8_t /* Link ID */, double> >
            meanQueuingDelayPerNodeLink;
        std::map<uint32_t /* Node ID */, std::map<uint8_t /* Link ID */, double> >
            totalAccessDelayPerNodeLink;
        std::map<uint32_t /* Node ID */, std::map<uint8_t /* Link ID */, double> >
            meanAccessDelayPerNodeLink;
        std::map<uint32_t /* Node ID */, std::map<uint8_t /* Link ID */, std::vector<double> > >
            accessDelaysPerNodeLink;
        std::map<uint32_t /* Node ID */, std::map<uint8_t /* Link ID */, std::vector<double> > >
            e2eDelaysPerNodeLink;
        for (const auto& nodeMap : successInfo)
        {
            for (const auto& linkMap : nodeMap.second)
            {
                for (uint32_t i = 0; i < enqueueTimeMap[nodeMap.first][linkMap.first].size(); ++i)
                {
                    totalQueuingDelayPerNodeLink[nodeMap.first][linkMap.first] += holTimeMap[nodeMap.
                        first][linkMap.first][i] - enqueueTimeMap[nodeMap.first][
                        linkMap.first][i];
                    totalAccessDelayPerNodeLink[nodeMap.first][linkMap.first] += dequeueTimeMap[nodeMap.
                        first][linkMap.first][i] - holTimeMap[nodeMap.first][linkMap.
                        first][i];
                    accessDelaysPerNodeLink[nodeMap.first][linkMap.first].emplace_back(
                        dequeueTimeMap[nodeMap.first][linkMap.first][i]
                        - holTimeMap[nodeMap.first][linkMap.first][i]);
                    e2eDelaysPerNodeLink[nodeMap.first][linkMap.first].emplace_back(
                        dequeueTimeMap[nodeMap.first][linkMap.first][i]
                        - enqueueTimeMap[nodeMap.first][linkMap.first][i]);
                }
                meanQueuingDelayPerNodeLink[nodeMap.first][linkMap.first] =
                    totalQueuingDelayPerNodeLink[nodeMap.first][linkMap.first] / (finalResults.
                        m_numSuccessPerNodeLink[nodeMap.first][linkMap.first] - 1);
                meanAccessDelayPerNodeLink[nodeMap.first][linkMap.first] =
                    totalAccessDelayPerNodeLink[nodeMap.first][linkMap.first] / (finalResults.
                        m_numSuccessPerNodeLink[nodeMap.first][linkMap.first] - 1);
            }
        }

        if (printTxStats)
        {
            std::cout << "TX Stats:\n";
            std::cout << "Node_ID\tLink_ID\t#Success\n";
            for (const auto& nodeMap : finalResults.m_numSuccessPerNodeLink)
            {
                for (const auto& linkMap : nodeMap.second)
                {
                    std::cout << nodeMap.first << "\t\t"
                        << +linkMap.first << "\t\t"
                        << linkMap.second << "\n";
                }
            }
            std::cout << "Node_ID\tLink_ID\tMean_Queuing_Delay\n";
            for (const auto& nodeMap : meanQueuingDelayPerNodeLink)
            {
                for (const auto& linkMap : nodeMap.second)
                {
                    std::cout << nodeMap.first << "\t\t"
                        << +linkMap.first << "\t\t"
                        << linkMap.second << "\n";
                }
            }
            std::cout << "Node_ID\tLink_ID\tMean_Access_Delay\n";
            for (const auto& nodeMap : meanAccessDelayPerNodeLink)
            {
                for (const auto& linkMap : nodeMap.second)
                {
                    std::cout << nodeMap.first << "\t\t"
                        << +linkMap.first << "\t\t"
                        << linkMap.second << "\n";
                }
            }
            std::cout << "Summary:"
                << "\n1. Successful pkts: " << finalResults.m_numSuccess
                << "\n2. Successful and retransmitted pkts: " << finalResults.m_numRetransmitted
                << "\n3. Avg retransmissions per successful pkt: " << finalResults.m_avgFailures
                << "\n4. Failed pkts: " << finalResults.m_numFinalFailed
                << "\n";
        }

        // MLD's per link and total successful tx pr
        std::map<uint8_t /* Link ID */, uint64_t> numMldSuccessPerLink;
        std::map<uint8_t /* Link ID */, uint64_t> numMldAttemptsPerLink;
        uint64_t numMldSuccessTotal{0};
        uint64_t numMldAttemptsTotal{0};
        for (uint32_t i = 1; i < 1 + nMldSta; ++i)
        {
            const auto& linkMap = successInfo[i];
            for (const auto& records : linkMap)
            {
                for (const auto& pkt : records.second)
                {
                    numMldSuccessPerLink[records.first] += 1;
                    numMldAttemptsPerLink[records.first] += 1 + pkt.m_failures;
                    numMldSuccessTotal += 1;
                    numMldAttemptsTotal += 1 + pkt.m_failures;
                }
            }
        }


        // std::cout << "numMldAttemptsTotal: " << numMldAttemptsTotal << std::endl << std::flush;
        // std::cout << "numMldSuccessTotal: " << numMldSuccessTotal << std::endl << std::flush;
        // std::cout << "\n" << std::flush;


        double mldSuccPrTotal = static_cast<long double>(numMldSuccessTotal) / numMldAttemptsTotal;
        double mldSuccPrLink1 = static_cast<long double>(numMldSuccessPerLink[0]) /
                                numMldAttemptsPerLink[0];
        double mldSuccPrLink2 = static_cast<long double>(numMldSuccessPerLink[1]) /
                                numMldAttemptsPerLink[1];

        // throughput of MLD
        double mldThptTotal = static_cast<long double>(numMldSuccessTotal) * payloadSize * 8 /
                            simulationTime /
                            1000000;
        double mldThptLink1 = static_cast<long double>(numMldSuccessPerLink[0]) * payloadSize * 8 /
                            simulationTime /
                            1000000;
        double mldThptLink2 = static_cast<long double>(numMldSuccessPerLink[1]) * payloadSize * 8 /
                            simulationTime /
                            1000000;

        // mean delays of MLD
        std::map<uint8_t /* Link ID */, long double> mldQueDelayPerLinkTotal;
        long double mldQueDelayTotal{0};
        std::map<uint8_t /* Link ID */, long double> mldAccDelayPerLinkTotal;
        long double mldAccDelayTotal{0};
        for (uint32_t i = 1; i < 1 + nMldSta; ++i)
        {
            const auto& queLinkMap = totalQueuingDelayPerNodeLink[i];
            for (const auto& item : queLinkMap)
            {
                mldQueDelayPerLinkTotal[item.first] += item.second;
                mldQueDelayTotal += item.second;
            }
            const auto& accLinkMap = totalAccessDelayPerNodeLink[i];
            for (const auto& item : accLinkMap)
            {
                mldAccDelayPerLinkTotal[item.first] += item.second;
                mldAccDelayTotal += item.second;
            }
        }
        double mldMeanQueDelayTotal = mldQueDelayTotal / numMldSuccessTotal;
        double mldMeanQueDelayLink1 = mldQueDelayPerLinkTotal[0] / numMldSuccessPerLink[0];
        double mldMeanQueDelayLink2 = mldQueDelayPerLinkTotal[1] / numMldSuccessPerLink[1];
        double mldMeanAccDelayTotal = mldAccDelayTotal / numMldSuccessTotal;
        double mldMeanAccDelayLink1 = mldAccDelayPerLinkTotal[0] / numMldSuccessPerLink[0];
        double mldMeanAccDelayLink2 = mldAccDelayPerLinkTotal[1] / numMldSuccessPerLink[1];
        // Second raw moment of access delay: mean of (D_a)^2
        // Second central moment (variance) of access delay: mean of (D_a - mean)^2
        std::map<uint8_t /* Link ID */, long double> mldAccDelaySquarePerLinkTotal;
        long double mldAccDelaySquareTotal{0};
        std::map<uint8_t /* Link ID */, long double> mldAccDelayCentralSquarePerLinkTotal;
        long double mldAccDelayCentralSquareTotal{0};
        for (uint32_t i = 1; i < 1 + nMldSta; ++i)
        {
            const auto& accLinkMap = accessDelaysPerNodeLink[i];
            for (const auto& linkAccVec : accLinkMap)
            {
                const auto& accVec = linkAccVec.second;
                auto mldMeanAccDelayLink = (linkAccVec.first == 0)
                                            ? mldMeanAccDelayLink1
                                            : mldMeanAccDelayLink2;
                for (const auto& item : accVec)
                {
                    mldAccDelaySquarePerLinkTotal[linkAccVec.first] += item * item;
                    mldAccDelaySquareTotal += item * item;
                    mldAccDelayCentralSquarePerLinkTotal[linkAccVec.first] +=
                        (item - mldMeanAccDelayLink) * (item - mldMeanAccDelayLink);
                    mldAccDelayCentralSquareTotal +=
                        (item - mldMeanAccDelayLink) * (item - mldMeanAccDelayLink);
                }
            }
        }
        double mldSecondRawMomentAccDelayTotal = mldAccDelaySquareTotal / numMldSuccessTotal;
        double mldSecondRawMomentAccDelayLink1 =
            mldAccDelaySquarePerLinkTotal[0] / numMldSuccessPerLink[0];
        double mldSecondRawMomentAccDelayLink2 =
            mldAccDelaySquarePerLinkTotal[1] / numMldSuccessPerLink[1];
        double mldSecondCentralMomentAccDelayTotal = mldAccDelayCentralSquareTotal / numMldSuccessTotal;
        double mldSecondCentralMomentAccDelayLink1 =
            mldAccDelayCentralSquarePerLinkTotal[0] / numMldSuccessPerLink[0];
        double mldSecondCentralMomentAccDelayLink2 =
            mldAccDelayCentralSquarePerLinkTotal[1] / numMldSuccessPerLink[1];
        double mldMeanE2eDelayTotal = mldMeanQueDelayTotal + mldMeanAccDelayTotal;
        double mldMeanE2eDelayLink1 = mldMeanQueDelayLink1 + mldMeanAccDelayLink1;
        double mldMeanE2eDelayLink2 = mldMeanQueDelayLink2 + mldMeanAccDelayLink2;

        stepNumber++;

        // std::cout << "Simulation Time 2: " << simulationTime << std::endl << std::flush;

        msgInterface->CppSendBegin();
        // std::cout << "sent: " << std::flush;
        // uint32_t temp_a = distrib(gen);
        // uint32_t temp_b = distrib(gen);
        // std::cout << temp_a << "," << temp_b << ";;" << std::flush;
        // msgInterface->GetCpp2PyStruct()->env_a = temp_a;
        // msgInterface->GetCpp2PyStruct()->env_b = temp_b;
        msgInterface->GetCpp2PyStruct()->env_mldThptTotal = mldThptTotal;
        msgInterface->GetCpp2PyStruct()->env_mldSuccPrLink1 = mldSuccPrLink1;
        msgInterface->GetCpp2PyStruct()->env_mldSuccPrLink2 = mldSuccPrLink2;
        msgInterface->GetCpp2PyStruct()->env_mldSuccPrTotal = mldSuccPrTotal;
        msgInterface->GetCpp2PyStruct()->env_mldThptLink1 = mldThptLink1;
        msgInterface->GetCpp2PyStruct()->env_mldThptLink2 = mldThptLink2;
        msgInterface->GetCpp2PyStruct()->env_mldMeanQueDelayLink1 = mldMeanQueDelayLink1;
        msgInterface->GetCpp2PyStruct()->env_mldMeanQueDelayLink2 = mldMeanQueDelayLink2;
        msgInterface->GetCpp2PyStruct()->env_mldMeanQueDelayTotal = mldMeanQueDelayTotal;
        msgInterface->GetCpp2PyStruct()->env_mldMeanAccDelayLink1 = mldMeanAccDelayLink1;
        msgInterface->GetCpp2PyStruct()->env_mldMeanAccDelayLink2 = mldMeanAccDelayLink2;
        msgInterface->GetCpp2PyStruct()->env_mldMeanAccDelayTotal = mldMeanAccDelayTotal;
        msgInterface->GetCpp2PyStruct()->env_mldMeanE2eDelayLink1 = mldMeanE2eDelayLink1;
        msgInterface->GetCpp2PyStruct()->env_mldMeanE2eDelayLink2 = mldMeanE2eDelayLink2;
        msgInterface->GetCpp2PyStruct()->env_mldMeanE2eDelayTotal = mldMeanE2eDelayTotal;
        msgInterface->GetCpp2PyStruct()->env_mldSecondRawMomentAccDelayLink1 = mldSecondRawMomentAccDelayLink1;
        msgInterface->GetCpp2PyStruct()->env_mldSecondRawMomentAccDelayLink2 = mldSecondRawMomentAccDelayLink2;
        msgInterface->GetCpp2PyStruct()->env_mldSecondRawMomentAccDelayTotal = mldSecondRawMomentAccDelayTotal;
        msgInterface->GetCpp2PyStruct()->env_mldSecondCentralMomentAccDelayLink1 = mldSecondCentralMomentAccDelayLink1;
        msgInterface->GetCpp2PyStruct()->env_mldSecondCentralMomentAccDelayLink2 = mldSecondCentralMomentAccDelayLink2;
        msgInterface->GetCpp2PyStruct()->env_mldSecondCentralMomentAccDelayTotal = mldSecondCentralMomentAccDelayTotal;
        msgInterface->GetCpp2PyStruct()->env_rngRun = rngRun;
        msgInterface->GetCpp2PyStruct()->env_simulationTime = simulationTime;
        msgInterface->GetCpp2PyStruct()->env_payloadSize = payloadSize;
        msgInterface->GetCpp2PyStruct()->env_mcs = mcs;
        msgInterface->GetCpp2PyStruct()->env_mcs2 = mcs2;
        msgInterface->GetCpp2PyStruct()->env_channelWidth = channelWidth;
        msgInterface->GetCpp2PyStruct()->env_channelWidth2 = channelWidth2;
        msgInterface->GetCpp2PyStruct()->env_nMldSta = nMldSta;
        msgInterface->GetCpp2PyStruct()->env_mldPerNodeLambda = mldPerNodeLambda;
        msgInterface->GetCpp2PyStruct()->env_mldProbLink1 = mldProbLink1;
        msgInterface->GetCpp2PyStruct()->env_mldAcLink1Int = mldAcLink1Int;
        msgInterface->GetCpp2PyStruct()->env_mldAcLink2Int = mldAcLink2Int;
        msgInterface->GetCpp2PyStruct()->env_acBECwminLink1 = acBECwminLink1;
        msgInterface->GetCpp2PyStruct()->env_acBECwStageLink1 = acBECwStageLink1;
        msgInterface->GetCpp2PyStruct()->env_acBKCwminLink1 = acBKCwminLink1;
        msgInterface->GetCpp2PyStruct()->env_acBKCwStageLink1 = acBKCwStageLink1;
        msgInterface->GetCpp2PyStruct()->env_acVICwminLink1 = acVICwminLink1;
        msgInterface->GetCpp2PyStruct()->env_acVICwStageLink1 = acVICwStageLink1;
        msgInterface->GetCpp2PyStruct()->env_acVOCwminLink1 = acVOCwminLink1;
        msgInterface->GetCpp2PyStruct()->env_acVOCwStageLink1 = acVOCwStageLink1;
        msgInterface->GetCpp2PyStruct()->env_acBECwminLink2 = acBECwminLink2;
        msgInterface->GetCpp2PyStruct()->env_acBECwStageLink2 = acBECwStageLink2;
        msgInterface->GetCpp2PyStruct()->env_acBKCwminLink2 = acBKCwminLink2;
        msgInterface->GetCpp2PyStruct()->env_acBKCwStageLink2 = acBKCwStageLink2;
        msgInterface->GetCpp2PyStruct()->env_acVICwminLink2 = acVICwminLink2;
        msgInterface->GetCpp2PyStruct()->env_acVICwStageLink2 = acVICwStageLink2;
        msgInterface->GetCpp2PyStruct()->env_acVOCwminLink2 = acVOCwminLink2;
        msgInterface->GetCpp2PyStruct()->env_acVOCwStageLink2 = acVOCwStageLink2;
        msgInterface->GetCpp2PyStruct()->env_stepNumber = stepNumber;
        msgInterface->CppSendEnd();


        if (printTxStatsSingleLine)
        {
            g_fileSummary
                << mldSuccPrLink1 << "," << mldSuccPrLink2 << "," << mldSuccPrTotal << ","
                << mldThptLink1 << "," << mldThptLink2 << "," << mldThptTotal << ","
                << mldMeanQueDelayLink1 << "," << mldMeanQueDelayLink2 << "," << mldMeanQueDelayTotal <<
                ","
                << mldMeanAccDelayLink1 << "," << mldMeanAccDelayLink2 << "," << mldMeanAccDelayTotal <<
                ","
                << mldMeanE2eDelayLink1 << "," << mldMeanE2eDelayLink2 << "," << mldMeanE2eDelayTotal <<
                ",";
            // added jitter (second moment, raw/central) results (10 columns)
            g_fileSummary << mldSecondRawMomentAccDelayLink1 << "," << mldSecondRawMomentAccDelayLink2
                << "," << mldSecondRawMomentAccDelayTotal << ","
                << mldSecondCentralMomentAccDelayLink1 << "," << mldSecondCentralMomentAccDelayLink2
                << "," << mldSecondCentralMomentAccDelayTotal << ",";

            // print these input:
            g_fileSummary << rngRun << "," << simulationTime << "," << payloadSize << ","
                << mcs << "," << mcs2 << "," << channelWidth << "," << channelWidth2 << ","
                << nMldSta << "," << mldPerNodeLambda << "," << mldProbLink1 << ","
                << +mldAcLink1Int << "," << +mldAcLink2Int << ","
                << acBECwminLink1 << "," << +acBECwStageLink1 << ","
                << acBKCwminLink1 << "," << +acBKCwStageLink1 << ","
                << acVICwminLink1 << "," << +acVICwStageLink1 << ","
                << acVOCwminLink1 << "," << +acVOCwStageLink1 << ","
                << acBECwminLink2 << "," << +acBECwStageLink2 << ","
                << acBKCwminLink2 << "," << +acBKCwStageLink2 << ","
                << acVICwminLink2 << "," << +acVICwStageLink2 << ","
                << acVOCwminLink2 << "," << +acVOCwStageLink2 << "\n";
        }

        wifiTxStats.Reset();
        wifiStats.Reset();


    }
    g_fileSummary.close();

    Simulator::Destroy();
    return 0;
}

int
main(int argc, char *argv[])
{
    RunSimulation(argc, argv);

    return 0;
}
