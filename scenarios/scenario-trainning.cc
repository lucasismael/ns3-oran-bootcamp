/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include <ns3/lte-ue-net-device.h>
#include "ns3/mmwave-helper.h"
#include "ns3/epc-helper.h"
#include "ns3/mmwave-point-to-point-epc-helper.h"
#include "ns3/lte-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/mmwave-ue-net-device.h"
#include "ns3/config-store.h"
#include "ns3/cc-helper.h"
#include "ns3/ue-throughput-storage.h"
#include <time.h>
#include <iostream>
#include "ns3/mc-ue-pdcp.h"
#include "ns3/mc-enb-pdcp.h"
#include <ns3/cosine-antenna-model.h>
#include "ns3/epc-tft.h"

using namespace ns3;
using namespace mmwave;

NS_LOG_COMPONENT_DEFINE ("ScenarioTrainning");



void PrintThroughputInfo(NetDeviceContainer uesNetDevs) {
    auto nUes = uesNetDevs.GetN();
    for (uint16_t i = 0; i < nUes; i++) {
        Ptr<McUeNetDevice> mcUeNetDevice = DynamicCast<McUeNetDevice>(uesNetDevs.Get(i));
        uint64_t imsi = mcUeNetDevice->GetImsi();
        uint16_t rnti = mcUeNetDevice->GetLteRrc()->GetRnti();
        if (rnti) {
            ObjectMapValue mapValueLte;
            mcUeNetDevice->GetLteTargetEnb()->GetRrc()->GetUeManager(rnti)->GetAttribute("DataRadioBearerMap", mapValueLte);
            for (auto iterLte = mapValueLte.Begin(); iterLte != mapValueLte.End(); ++iterLte) {
                Ptr<LteDataRadioBearerInfo> drb = DynamicCast<LteDataRadioBearerInfo>(iterLte->second);
                int drbId = static_cast<int>(drb->m_drbIdentity);
                
                double lteThp = UeThroughputStorage::GetLteThroughput(imsi, drbId);
                double lteDelay = UeThroughputStorage::GetLteDelay(imsi, drbId);
                uint64_t qciLte = UeThroughputStorage::GetDrbQci(imsi, drbId);
                std::cout << "\033[1;32m" << "Time: " << Simulator::Now().GetSeconds() << "s | "
                          << "IMSI: " << imsi << " DRB: " << drbId << " QCI: " << qciLte 
                          << " LTE THP = " << lteThp << " LTE DELAY = " << lteDelay << "\033[0m" << std::endl;
                
                double nrThp = UeThroughputStorage::GetNrThroughput(imsi, drbId);
                double nrDelay = UeThroughputStorage::GetNrDelay(imsi, drbId);
                std::cout << "\033[1;32m" << "Time: " << Simulator::Now().GetSeconds() << "s | "
                          << "IMSI: " << imsi << " DRB: " << drbId << " QCI: " << qciLte 
                          << " NR THP = " << nrThp << " NR DELAY = " << nrDelay << "\033[0m" << std::endl;
                
                double aggThp = UeThroughputStorage::GetAggregatedThroughput(imsi, drbId);
                std::cout << "\033[1;32m" << "Time: " << Simulator::Now().GetSeconds() << "s | "
                          << "IMSI: " << imsi << " DRB: " << drbId << " Agg Thp = " << aggThp << "\033[0m" << std::endl;
            }
        }
    }
    Simulator::Schedule(Seconds(0.1), &PrintThroughputInfo, uesNetDevs);
}

void PrintUePosition(NodeContainer ues) {
    for (uint32_t i = 0; i < ues.GetN(); ++i) {
        Ptr<MobilityModel> mob = ues.Get(i)->GetObject<MobilityModel>();
        if (mob) {
            Vector pos = mob->GetPosition();
            std::cout << "\033[1;36mTime: " << Simulator::Now().GetSeconds() 
                      << "s | UE " << ues.Get(i)->GetId() 
                      << " Position: (" << pos.x << ", " << pos.y << ", " << pos.z << ")\033[0m" << std::endl;
        }
    }
    Simulator::Schedule(Seconds(1.0), &PrintUePosition, ues);
}

void RegisterSimulationTime(double t) {
    std::string time = std::to_string(t);
    std::cout << "\033[1;31m" << "Simulation time: " << time << "\033[0m" << std::endl;
}

double RunSimulationWithTimeCounter() {
    time_t beginTimer = time(&beginTimer);
    struct tm *timeInfo = localtime(&beginTimer);
    std::cout << "Simulation start time: " << asctime(timeInfo) << std::endl;

    Simulator::Run();
    Simulator::Destroy();

    time_t finishTimer = time(&finishTimer);
    timeInfo = localtime(&finishTimer);
    std::cout << "Simulation end time: " << asctime(timeInfo) << std::endl;

    double elapsedTime = difftime(finishTimer, beginTimer);
    std::cout << "Simulation duration: " << elapsedTime << " seconds" << std::endl;
    return elapsedTime;
}

void SetDrbsSplitRatiosBkg(NetDeviceContainer uesNetDevs) {
    auto nUes = uesNetDevs.GetN();
    for (uint16_t i = 0; i < nUes; i++) {
        Ptr<McUeNetDevice> mcUeNetDevice = DynamicCast<McUeNetDevice>(uesNetDevs.Get(i));
        uint16_t rnti = mcUeNetDevice->GetLteRrc()->GetRnti();
        if (rnti) {
            ObjectMapValue mapValueLte;
            mcUeNetDevice->GetLteTargetEnb()->GetRrc()->GetUeManager(rnti)->GetAttribute("DataRadioBearerMap", mapValueLte);
            for (auto iterLte = mapValueLte.Begin(); iterLte != mapValueLte.End(); ++iterLte) {
                Ptr<LteDataRadioBearerInfo> drb = DynamicCast<LteDataRadioBearerInfo>(iterLte->second);
                Ptr<McEnbPdcp> pdcp = DynamicCast<McEnbPdcp>(drb->m_pdcp);
                if (pdcp) pdcp->SetSplitRatio(1.0);
            }
        }
    }
}

void SetDrbsSplitRatiosMix(NetDeviceContainer uesNetDevs, double ratioMbb, double ratioXr) {
    auto nUes = uesNetDevs.GetN();
    for (uint16_t i = 0; i < nUes; i++) {
        Ptr<McUeNetDevice> mcUeNetDevice = DynamicCast<McUeNetDevice>(uesNetDevs.Get(i));
        uint16_t rnti = mcUeNetDevice->GetLteRrc()->GetRnti();
        if (rnti) {
            ObjectMapValue mapValueLte;
            mcUeNetDevice->GetLteTargetEnb()->GetRrc()->GetUeManager(rnti)->GetAttribute("DataRadioBearerMap", mapValueLte);
            for (auto iterLte = mapValueLte.Begin(); iterLte != mapValueLte.End(); ++iterLte) {
                Ptr<LteDataRadioBearerInfo> drb = DynamicCast<LteDataRadioBearerInfo>(iterLte->second);
                int drbId = static_cast<int>(drb->m_drbIdentity);
                Ptr<McEnbPdcp> pdcp = DynamicCast<McEnbPdcp>(drb->m_pdcp);
                if (pdcp) {
                    if (drbId == 1) pdcp->SetSplitRatio(ratioMbb);
                    else if (drbId == 2) pdcp->SetSplitRatio(ratioXr);
                    else pdcp->SetSplitRatio(1.0);
                }
            }
        }
    }
}

ApplicationContainer createOnOffApplicationWithPacketFilter(
    Ptr<EpcHelper> epcHelper, enum EpsBearer::Qci qci, Ptr<Node> remoteHost, 
    NetDeviceContainer uesNetDevs, Ipv4InterfaceContainer internetIpv4Interfaces, 
    Ipv4InterfaceContainer uesIpv4Interfaces, bool dl = true, bool ul = false, 
    double dataRateInMbps = 1, uint16_t packetSize = 1024, 
    uint16_t startDlPort = 10000, uint16_t startUlPort = 20000, 
    double startTime=0.0, double endTime=120.0, uint8_t remoteHostIndexInContainer = 1) 
{
    ApplicationContainer allOnOffs;
    if (!(dl || ul)) return allOnOffs;

    uint16_t dlPort = startDlPort;
    uint16_t ulPort = startUlPort;
    DataRate dataRateInMbpsDR = DataRate(std::to_string(dataRateInMbps) + "Mib/s");
    std::string socketFactory = "ns3::UdpSocketFactory";

    for (uint8_t u = 0; u < uesNetDevs.GetN(); ++u) {
        Ptr<Node> ue = uesNetDevs.Get(u)->GetNode();
        ApplicationContainer applications1;
        ApplicationContainer applications2;

        if (dl) {
            OnOffHelper dlHelper(socketFactory, InetSocketAddress(uesIpv4Interfaces.GetAddress(u), dlPort));
            dlHelper.SetAttribute("MaxBytes", UintegerValue(0));
            dlHelper.SetConstantRate(dataRateInMbpsDR, packetSize);
            applications1.Add(dlHelper.Install(remoteHost));

            PacketSinkHelper dlPacketSinkHelper(socketFactory, InetSocketAddress(Ipv4Address::GetAny(), dlPort));
            applications2.Add(dlPacketSinkHelper.Install(ue));

            Ptr<McUeNetDevice> mcUeNetDev = DynamicCast<McUeNetDevice>(uesNetDevs.Get(u));
            uint64_t imsi = mcUeNetDev->GetImsi();
            Ptr<EpcUeNas> lteUeNas = mcUeNetDev->GetNas();
            EpcTft::PacketFilter packetFilter;
            packetFilter.remoteAddress = internetIpv4Interfaces.GetAddress(remoteHostIndexInContainer);
            packetFilter.localAddress = uesIpv4Interfaces.GetAddress(u);
            packetFilter.remotePortStart = 0;
            packetFilter.remotePortEnd = 65535;
            packetFilter.localPortStart = dlPort;
            packetFilter.localPortEnd = dlPort;
            packetFilter.direction = EpcTft::DOWNLINK;
            Ptr<EpcTft> epcTft = Create<EpcTft>();
            epcTft->Add(packetFilter);
            epcHelper->ActivateEpsBearer(mcUeNetDev, lteUeNas, imsi, epcTft, qci);
        }

        if (ul) {
            OnOffHelper ulHelper(socketFactory, InetSocketAddress(internetIpv4Interfaces.GetAddress(remoteHostIndexInContainer), ulPort));
            ulHelper.SetAttribute("MaxBytes", UintegerValue(0));
            ulHelper.SetConstantRate(dataRateInMbpsDR, packetSize);
            applications1.Add(ulHelper.Install(ue));

            PacketSinkHelper ulPacketSinkHelper(socketFactory, InetSocketAddress(Ipv4Address::GetAny(), ulPort));
            applications2.Add(ulPacketSinkHelper.Install(remoteHost));

            Ptr<McUeNetDevice> mcUeNetDev = DynamicCast<McUeNetDevice>(uesNetDevs.Get(u));
            uint64_t imsi = mcUeNetDev->GetImsi();
            Ptr<EpcUeNas> lteUeNas = mcUeNetDev->GetNas();
            EpcTft::PacketFilter packetFilter;
            packetFilter.remoteAddress = internetIpv4Interfaces.GetAddress(remoteHostIndexInContainer);
            packetFilter.localAddress = uesIpv4Interfaces.GetAddress(u);
            packetFilter.remotePortStart = 0;
            packetFilter.remotePortEnd = 65535;
            packetFilter.localPortStart = ulPort;
            packetFilter.localPortEnd = ulPort;
            packetFilter.direction = EpcTft::UPLINK;
            Ptr<EpcTft> epcTft = Create<EpcTft>();
            epcTft->Add(packetFilter);
            epcHelper->ActivateEpsBearer(mcUeNetDev, lteUeNas, imsi, epcTft, qci);
        }

        applications1.Start(Seconds(startTime));
        applications1.Stop(Seconds(endTime));
        applications2.Start(Seconds(startTime));
        applications2.Stop(Seconds(endTime));

        ++dlPort;
        ++ulPort;
        allOnOffs.Add(applications1);
    }
    return allOnOffs;
}

ApplicationContainer createOnOffApplication(
    Ptr<Node> remoteHost, NetDeviceContainer uesNetDevs, 
    Ipv4InterfaceContainer internetIpv4Interfaces, Ipv4InterfaceContainer uesIpv4Interfaces, 
    bool dl = true, bool ul = false, double dataRateInMbps = 1, uint16_t packetSize = 1024, 
    uint16_t startDlPort = 10000, uint16_t startUlPort = 20000, 
    double startTime=0.0, double endTime=120.0, uint8_t remoteHostIndexInContainer = 1) 
{
    ApplicationContainer allOnOffs;
    if (!(dl || ul)) return allOnOffs;

    uint16_t dlPort = startDlPort;
    uint16_t ulPort = startUlPort;
    DataRate dataRateInMbpsDR = DataRate(std::to_string(dataRateInMbps) + "Mib/s");
    std::string socketFactory = "ns3::UdpSocketFactory";

    for (uint8_t u = 0; u < uesNetDevs.GetN(); ++u) {
        Ptr<Node> ue = uesNetDevs.Get(u)->GetNode();
        ApplicationContainer applications1;
        ApplicationContainer applications2;

        if (dl) {
            OnOffHelper dlHelper(socketFactory, InetSocketAddress(uesIpv4Interfaces.GetAddress(u), dlPort));
            dlHelper.SetAttribute("MaxBytes", UintegerValue(0));
            dlHelper.SetConstantRate(dataRateInMbpsDR, packetSize);
            applications1.Add(dlHelper.Install(remoteHost));

            PacketSinkHelper dlPacketSinkHelper(socketFactory, InetSocketAddress(Ipv4Address::GetAny(), dlPort));
            applications2.Add(dlPacketSinkHelper.Install(ue));
        }

        if (ul) {
            OnOffHelper ulHelper(socketFactory, InetSocketAddress(internetIpv4Interfaces.GetAddress(remoteHostIndexInContainer), ulPort));
            ulHelper.SetAttribute("MaxBytes", UintegerValue(0));
            ulHelper.SetConstantRate(dataRateInMbpsDR, packetSize);
            applications1.Add(ulHelper.Install(ue));

            PacketSinkHelper ulPacketSinkHelper(socketFactory, InetSocketAddress(Ipv4Address::GetAny(), ulPort));
            applications2.Add(ulPacketSinkHelper.Install(remoteHost));
        }

        applications1.Start(Seconds(startTime));
        applications1.Stop(Seconds(endTime));
        applications2.Start(Seconds(startTime));
        applications2.Stop(Seconds(endTime));

        ++dlPort;
        ++ulPort;
        allOnOffs.Add(applications1);
    }
    return allOnOffs;
}

int main(int argc, char *argv[]) {
    LogComponentEnableAll (LOG_PREFIX_ALL);
    // LogComponentEnable ("MmWaveEnbNetDevice", LOG_LEVEL_INFO);
    // LogComponentEnable ("LteEnbNetDevice", LOG_LEVEL_INFO);
    // LogComponentEnable ("ScenarioTrainning", LOG_LEVEL_DEBUG);
    GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::RealtimeSimulatorImpl"));

    CommandLine cmd;
    double simTime = 300.0;
    double simSplitRatio = 0.0;
    double dataRateMbb = 100.0;
    double dataRateXr = 15.0;
    uint32_t nUesMixed = 1;
    uint32_t nUesBackground = 0;
    
    std::string e2TermIp = "10.244.0.50";
    bool enableE2FileLogging = false;

    cmd.AddValue ("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue ("simSplitRatio", "Ratio in which the data will split (LTE = splitRatio - NR = (1 - LTE))", simSplitRatio);
    cmd.AddValue ("dataRateMbb", "Data Rate for eMBB (Mbps)", dataRateMbb);
    cmd.AddValue ("dataRateXr", "Data Rate for XR (Mbps)", dataRateXr);
    cmd.AddValue ("nUesMixed", "Number of UEs with multiple services", nUesMixed);
    cmd.AddValue ("nUesBackground", "Number of background UEs", nUesBackground);
    bool enableMobility = false;
    cmd.AddValue ("enableMobility", "Enable UE mobility", enableMobility);
    cmd.AddValue ("e2TermIp", "The IP address of the RIC E2 termination", e2TermIp);
    cmd.AddValue ("enableE2FileLogging", "If true, generate offline file logging instead of connecting to RIC", enableE2FileLogging);
    cmd.Parse (argc, argv);

    // RIC / E2 Parameters Configuration
    Config::SetDefault ("ns3::LteEnbNetDevice::E2Periodicity", DoubleValue (0.1));
    Config::SetDefault ("ns3::MmWaveEnbNetDevice::E2Periodicity", DoubleValue (0.1));
    Config::SetDefault ("ns3::MmWaveHelper::E2ModeLte", BooleanValue(true));
    Config::SetDefault ("ns3::MmWaveHelper::E2ModeNr", BooleanValue(true));
    Config::SetDefault ("ns3::MmWaveEnbNetDevice::EnableDuReport", BooleanValue(true));
    Config::SetDefault ("ns3::MmWaveEnbNetDevice::EnableCuUpReport", BooleanValue(true));
    Config::SetDefault ("ns3::LteEnbNetDevice::EnableCuUpReport", BooleanValue(true));
    Config::SetDefault ("ns3::MmWaveEnbNetDevice::EnableCuCpReport", BooleanValue(true));
    Config::SetDefault ("ns3::LteEnbNetDevice::EnableCuCpReport", BooleanValue(true));
    Config::SetDefault ("ns3::MmWaveEnbNetDevice::ReducedPmValues", BooleanValue (true));
    Config::SetDefault ("ns3::LteEnbNetDevice::ReducedPmValues", BooleanValue (true));
    Config::SetDefault ("ns3::LteEnbNetDevice::EnableE2FileLogging", BooleanValue (enableE2FileLogging));
    Config::SetDefault ("ns3::MmWaveEnbNetDevice::EnableE2FileLogging", BooleanValue (enableE2FileLogging));

    Config::SetDefault ("ns3::MmWaveHelper::E2TermIp", StringValue (e2TermIp));
    Config::SetDefault ("ns3::MmWaveHelper::HarqEnabled", BooleanValue (true));
    Config::SetDefault ("ns3::MmWaveHelper::UseIdealRrc", BooleanValue (true));
    
    // System model parameters
    Config::SetDefault("ns3::LteEnbRrc::SrsPeriodicity", UintegerValue(320));
    Config::SetDefault("ns3::LteEnbRrc::FirstSibTime", UintegerValue(2)); 
    Config::SetDefault("ns3::LteEnbRrc::SystemInformationPeriodicity", TimeValue(MilliSeconds(5.0)));
    Config::SetDefault("ns3::LteEnbRrc::AdmitHandoverRequest", BooleanValue(false));
    Config::SetDefault("ns3::LteEnbRrc::SecondaryCellHandoverMode", StringValue ("NoAuto"));
    Config::SetDefault("ns3::MmWaveHelper::LteHandoverAlgorithm", StringValue("ns3::NoOpHandoverAlgorithm"));
    
    // PHY MAC parameters
    Config::SetDefault ("ns3::MmWavePhyMacCommon::Bandwidth", DoubleValue (100e6));
    Config::SetDefault ("ns3::MmWavePhyMacCommon::CenterFreq", DoubleValue (30e9));
    Config::SetDefault("ns3::MmWavePhyMacCommon::Numerology", EnumValue(3));

    Config::SetDefault("ns3::MmWaveEnbPhy::TxPower", DoubleValue(24));
    Config::SetDefault("ns3::MmWaveUePhy::TxPower", DoubleValue(24));
    Config::SetDefault("ns3::LteEnbPhy::TxPower", DoubleValue(46));
    Config::SetDefault("ns3::LteUePhy::TxPower", DoubleValue(24));
    
    // Setup Channel model (No buildings)
    Config::SetDefault("ns3::ThreeGppChannelModel::Scenario", StringValue("UMa"));
    Config::SetDefault("ns3::ThreeGppPropagationLossModel::ShadowingEnabled", BooleanValue(true));
    // Config::SetDefault("ns3::ThreeGppChannelConditionModel::UpdatePeriod", TimeValue (MilliSeconds (100)));
    
    // Missing configs from scenario-master
    Config::SetDefault("ns3::MmWaveEnbPhy::NoiseFigure", DoubleValue(7));
    Config::SetDefault("ns3::MmWaveUePhy::NoiseFigure", DoubleValue(13));
    Config::SetDefault("ns3::LteEnbPhy::NoiseFigure", DoubleValue(5));
    Config::SetDefault("ns3::LteUePhy::NoiseFigure", DoubleValue(9));
    Config::SetDefault("ns3::McUePdcp::LteUplink", BooleanValue(false));
    Config::SetDefault("ns3::McUeNetDevice::AntennaNum", UintegerValue(1));
    Config::SetDefault("ns3::MmWaveNetDevice::AntennaNum", UintegerValue(64));
    Config::SetDefault("ns3::ThreeGppAntennaArrayModel::DowntiltAngle", DoubleValue(0));
    Config::SetDefault("ns3::ThreeGppAntennaArrayModel::IsotropicElements", BooleanValue(true));
    Config::SetDefault("ns3::ThreeGppAntennaArrayModel::ElementGain", DoubleValue(5));
    Config::SetDefault("ns3::ThreeGppAntennaArrayModel::AntennaHorizontalSpacing", DoubleValue(0.5));
    Config::SetDefault("ns3::ThreeGppAntennaArrayModel::AntennaVerticalSpacing", DoubleValue(0.5));
    Config::SetDefault("ns3::ThreeGppAntennaArrayModel::BearingAngle", DoubleValue(0.785398));
    Config::SetDefault("ns3::CosineAntennaModel::Beamwidth", DoubleValue(60));
    Config::SetDefault("ns3::CosineAntennaModel::Orientation", DoubleValue(225));
    Config::SetDefault("ns3::MmWaveFlexTtiMacScheduler::HarqEnabled", BooleanValue(true));
    Config::SetDefault("ns3::MmWavePhyMacCommon::NumHarqProcess", UintegerValue(100));
    Config::SetDefault("ns3::LteRlcAm::ReportBufferStatusTimer", TimeValue(MilliSeconds(10.0)));
    Config::SetDefault("ns3::LteRlcUmLowLat::ReportBufferStatusTimer", TimeValue(MilliSeconds(10.0)));
    Config::SetDefault("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue(10 * 1024 * 1024));
    Config::SetDefault("ns3::LteRlcUmLowLat::MaxTxBufferSize", UintegerValue(10 * 1024 * 1024));
    Config::SetDefault("ns3::LteRlcAm::MaxTxBufferSize", UintegerValue(10 * 1024 * 1024));
    Config::SetDefault("ns3::MmWaveHelper::RlcAmEnabled", BooleanValue(false));
    Config::SetDefault("ns3::MmWavePointToPointEpcHelper::X2LinkDelay", TimeValue(MicroSeconds(500)));
    Config::SetDefault("ns3::MmWavePointToPointEpcHelper::X2LinkDataRate", DataRateValue(DataRate("1000Gb/s")));
    Config::SetDefault("ns3::MmWavePointToPointEpcHelper::X2LinkMtu", UintegerValue(10000));
    Config::SetDefault("ns3::MmWavePointToPointEpcHelper::S1uLinkDelay", TimeValue(MicroSeconds(1000)));
    Config::SetDefault("ns3::MmWavePointToPointEpcHelper::S1apLinkDelay", TimeValue(MicroSeconds(10000)));
    
    ConfigStore inputConfig;
    inputConfig.ConfigureDefaults();

    Ptr<MmWaveHelper> mmwaveHelper = CreateObject<MmWaveHelper>();
    mmwaveHelper->SetAttribute("UseCa", BooleanValue(false));
    mmwaveHelper->SetAttribute("UseIdealRrc", BooleanValue(true));
    mmwaveHelper->SetBeamformingModelType("ns3::MmWaveDftBeamforming");
    mmwaveHelper->SetMcUeNetDeviceAttribute("LteDlEarfcn", UintegerValue(100));

    mmwaveHelper->SetPathlossModelType("ns3::ThreeGppUmiStreetCanyonPropagationLossModel");
    mmwaveHelper->SetLtePathlossModelType("ns3::ThreeGppUmaPropagationLossModel");
    // mmwaveHelper->SetChannelConditionModelType("ns3::AlwaysLosChannelConditionModel");
    mmwaveHelper->SetChannelConditionModelType("ns3::ThreeGppUmaChannelConditionModel");

    mmwaveHelper->SetLteEnbAntennaModelType("ns3::CosineAntennaModel");
    mmwaveHelper->SetLteEnbAntennaModelAttribute("Orientation", DoubleValue(225));
    mmwaveHelper->SetLteEnbAntennaModelAttribute("Beamwidth", DoubleValue(60));

    Ptr<CcHelper> lteCcHelper = CreateObject<CcHelper>();
    lteCcHelper->SetNumberOfComponentCarriers(1);
    lteCcHelper->SetUlEarfcn(18100);
    lteCcHelper->SetDlEarfcn(100);
    lteCcHelper->SetDlBandwidth(100);
    lteCcHelper->SetUlBandwidth(100);
    std::map<uint8_t, ComponentCarrier> lteCcMap = lteCcHelper->EquallySpacedCcs();
    lteCcMap.at(0).SetAsPrimary(true);
    mmwaveHelper->SetLteCcPhyParams(lteCcMap);

    Ptr<MmWavePointToPointEpcHelper> epcHelper = CreateObject<MmWavePointToPointEpcHelper>();
    mmwaveHelper->SetEpcHelper(epcHelper);
    mmwaveHelper->SetHarqEnabled(true);
    mmwaveHelper->Initialize(); 

    // Topology
    NodeContainer ueNodesMixed;
    NodeContainer ueNodesBackground;
    NodeContainer allUeNodes;
    NodeContainer mmWaveEnbNodes;
    NodeContainer lteEnbNodes;
    NodeContainer allEnbNodes;

    lteEnbNodes.Create(1);
    mmWaveEnbNodes.Create(1);
    ueNodesMixed.Create(nUesMixed);
    ueNodesBackground.Create(nUesBackground);
    allUeNodes.Add(ueNodesMixed);
    allUeNodes.Add(ueNodesBackground);
    allEnbNodes.Add(lteEnbNodes);
    allEnbNodes.Add(mmWaveEnbNodes);

    // Mobility
    Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator> ();
    enbPositionAlloc->Add(Vector (250.0, 250.0, 25.0)); // LTE eNB
    enbPositionAlloc->Add(Vector (0.0, 0.0, 3.0));     // mmWave gNB

    MobilityHelper enbMobility;
    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.SetPositionAllocator(enbPositionAlloc);
    enbMobility.Install(allEnbNodes);

    if (enableMobility) {
        MobilityHelper ueMobility;
        ueMobility.SetMobilityModel("ns3::WaypointMobilityModel");
        ueMobility.Install(allUeNodes);

        for (uint32_t i = 0; i < allUeNodes.GetN(); ++i) {
            Ptr<WaypointMobilityModel> mob = allUeNodes.Get(i)->GetObject<WaypointMobilityModel>();
            double offset = i * 2.0; 
            mob->AddWaypoint(Waypoint(Seconds(0.0), Vector(10.0 + offset, 10.0 + offset, 1.5)));
            mob->AddWaypoint(Waypoint(Seconds(simTime), Vector(240.0 + offset, 240.0 + offset, 1.5)));
        }
    } else {
        MobilityHelper mixedUesMobility;
        mixedUesMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        Ptr<RandomRectanglePositionAllocator> mixedUesPositionAlloc = CreateObject<RandomRectanglePositionAllocator>();
        mixedUesPositionAlloc->SetAttribute("X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=120.0]"));
        mixedUesPositionAlloc->SetAttribute("Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
        mixedUesPositionAlloc->SetAttribute("Z", DoubleValue(1.5));
        mixedUesMobility.SetPositionAllocator(mixedUesPositionAlloc);
        mixedUesMobility.Install(ueNodesMixed);

        MobilityHelper backgroundUesMobility;
        backgroundUesMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        Ptr<RandomRectanglePositionAllocator> backgroundUesPositionAlloc = CreateObject<RandomRectanglePositionAllocator>();
        backgroundUesPositionAlloc->SetAttribute("X", StringValue("ns3::UniformRandomVariable[Min=121.0|Max=250.0]"));
        backgroundUesPositionAlloc->SetAttribute("Y", StringValue("ns3::UniformRandomVariable[Min=51.0|Max=250.0]"));
        backgroundUesPositionAlloc->SetAttribute("Z", DoubleValue(1.5));
        backgroundUesMobility.SetPositionAllocator(backgroundUesPositionAlloc);
        backgroundUesMobility.Install(ueNodesBackground);
    }

    // Devices
    NetDeviceContainer lteEnbDevs;
    lteEnbDevs.Add(mmwaveHelper->InstallLteEnbDevice (lteEnbNodes.Get(0)));
    NetDeviceContainer mmWaveEnbDevs = mmwaveHelper->InstallEnbDevice (mmWaveEnbNodes.Get(0));
    NetDeviceContainer mixedUesNetDevs = mmwaveHelper->InstallMcUeDevice (ueNodesMixed);
    NetDeviceContainer backgroundUesNetDevs = mmwaveHelper->InstallMcUeDevice (ueNodesBackground);
    NetDeviceContainer mcUeDevs;
    mcUeDevs.Add(mixedUesNetDevs);
    mcUeDevs.Add(backgroundUesNetDevs);

    // Internet and Routing
    Ptr<Node> pgw = epcHelper->GetPgwNode ();
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create (1);
    Ptr<Node> remoteHost = remoteHostContainer.Get (0);
    InternetStackHelper internet;
    internet.Install (remoteHostContainer);

    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
    p2ph.SetDeviceAttribute ("Mtu", UintegerValue (2500));
    p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));
    NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);
    
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
    
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
    remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

    internet.Install (allUeNodes);
    Ipv4InterfaceContainer ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (mcUeDevs));
    
    Ipv4InterfaceContainer ueIpIfaceMixed;
    for (uint32_t i = 0; i < ueNodesMixed.GetN (); ++i) {
        ueIpIfaceMixed.Add (ueIpIface.Get (i));
    }
    Ipv4InterfaceContainer ueIpIfaceBackground;
    for (uint32_t i = 0; i < ueNodesBackground.GetN (); ++i) {
        ueIpIfaceBackground.Add (ueIpIface.Get (ueNodesMixed.GetN () + i));
    }

    for (uint32_t u = 0; u < allUeNodes.GetN (); ++u) {
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (allUeNodes.Get(u)->GetObject<Ipv4> ());
        ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

    // Attach to base stations
    mmwaveHelper->AddX2Interface (lteEnbNodes, mmWaveEnbNodes);
    mmwaveHelper->AttachToClosestEnb (mcUeDevs, mmWaveEnbDevs, lteEnbDevs);

    // Applications setup
    uint16_t dlPort = 10000;
    uint16_t ulPort = 20000;
    // enum EpsBearer::Qci qciEmbb = EpsBearer::GBR_CONV_VIDEO;
    enum EpsBearer::Qci qciEmbb = EpsBearer::GBR_CONV_VIDEO;
    enum EpsBearer::Qci qciXr = EpsBearer::NGBR_LOW_LAT_EMBB_AR;
    double dataRateEmbbBackgroundMbps = 1.0;
    double packetSizeInBytes = 1024;
    double startTime = 0.1;
    double endTime = simTime;

    ApplicationContainer allApps;

    ApplicationContainer embbMixedApps = createOnOffApplicationWithPacketFilter (
        epcHelper, qciEmbb, remoteHost, mixedUesNetDevs, internetIpIfaces, ueIpIfaceMixed,
        true, false, dataRateMbb, packetSizeInBytes, dlPort, ulPort, startTime, endTime);
    allApps.Add(embbMixedApps);

    dlPort += ueNodesMixed.GetN ();
    ulPort += ueNodesMixed.GetN ();

    // ApplicationContainer xrMixedApps = createOnOffApplicationWithPacketFilter (
    //     epcHelper, qciXr, remoteHost, mixedUesNetDevs, internetIpIfaces, ueIpIfaceMixed,
    //     true, false, dataRateXr, packetSizeInBytes, dlPort, ulPort, startTime, endTime);
    // allApps.Add(xrMixedApps);

    // dlPort += ueNodesMixed.GetN ();
    // ulPort += ueNodesMixed.GetN ();

    ApplicationContainer embbBkgApps = createOnOffApplication (
        remoteHost, backgroundUesNetDevs, internetIpIfaces, ueIpIfaceBackground,
        true, false, dataRateEmbbBackgroundMbps, packetSizeInBytes, dlPort, ulPort, startTime, endTime);
    allApps.Add (embbBkgApps);

    // Scheduling split ratio logic manually, without MAB
    Simulator::Schedule(Seconds(0.5), &SetDrbsSplitRatiosBkg, backgroundUesNetDevs);
    Simulator::Schedule(Seconds(0.5), &SetDrbsSplitRatiosMix, mixedUesNetDevs, simSplitRatio, 0.0);

    for (double t = 0; t <= simTime; t = t + 1) {
        Simulator::Schedule(Seconds(t), &RegisterSimulationTime, t);
    }

    // Simulator::Schedule(Seconds(1.0), &PrintThroughputInfo, mcUeDevs);
    Simulator::Schedule(Seconds(1.0), &PrintUePosition, allUeNodes);
    Simulator::Stop(Seconds(simTime + 0.001));
    RunSimulationWithTimeCounter();

    return 0;
}
