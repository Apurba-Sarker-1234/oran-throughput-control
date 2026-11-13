// Dynamics check (derived from phase 0): prove a runtime DL PRB cap changes throughput. mode=cap (scheduler MaxDlPrb) or bw (SetDlBandwidth)
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include <fstream>
#include <sstream>
using namespace ns3;
static std::vector<Ptr<PacketSink>> g_sinks;
static std::ofstream g_out;
static uint64_t g_last = 0;
static double g_Ts = 0.01;
static void Sample() {
    uint64_t tot = 0; for (auto& s : g_sinks) tot += s->GetTotalRx();
    g_out << Simulator::Now().GetSeconds() << "," << (tot - g_last) * 8.0 / g_Ts / 1e6 << "\n";
    g_last = tot; Simulator::Schedule(Seconds(g_Ts), &Sample);
}
static void SetN(std::string mode, Ptr<LteEnbNetDevice> enb, uint16_t n) {
    std::cout << Simulator::Now().GetSeconds() << " set N=" << n << " mode=" << mode << std::endl;
    if (mode == "cap")
        DynamicCast<ComponentCarrierEnb>(enb->GetCcMap().at(0))->GetFfMacScheduler()->SetAttribute("MaxDlPrb", UintegerValue(n));
    else { enb->SetAttribute("DlBandwidth", UintegerValue(n)); }
}
int main(int argc, char* argv[]) {
    std::string mode = "cap", out = "dyn.csv", traffic = "udp", sched = "0.1:100,10:25,20:100,30:6,40:50,50:100"; double simT = 60; double load = 30; uint32_t nUe = 4, seed = 1;
    CommandLine cmd; cmd.AddValue("mode","cap|bw",mode); cmd.AddValue("traffic","udp|tcp|tcprate",traffic); cmd.AddValue("sched","t:N,...",sched); cmd.AddValue("simT","s",simT); cmd.AddValue("window","s",g_Ts); cmd.AddValue("out","csv",out);
    cmd.AddValue("load","Mbps per UE",load); cmd.AddValue("nUe","UEs",nUe); cmd.AddValue("seed","seed",seed); cmd.Parse(argc, argv);
    RngSeedManager::SetSeed(seed);
    if (traffic != "udp") { Config::SetDefault("ns3::LteEnbRrc::EpsBearerToRlcMapping", EnumValue(LteEnbRrc::RLC_AM_ALWAYS));
        Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1400)); Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(4194304)); Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(4194304)); }
    else Config::SetDefault("ns3::LteEnbRrc::EpsBearerToRlcMapping", EnumValue(LteEnbRrc::RLC_UM_ALWAYS));
    Config::SetDefault("ns3::LteHelper::Scheduler", StringValue("ns3::PfFfMacScheduler"));
    Config::SetDefault("ns3::LteEnbNetDevice::DlBandwidth", UintegerValue(100));
    Config::SetDefault("ns3::LteEnbNetDevice::UlBandwidth", UintegerValue(100));
    Config::SetDefault("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue(10 * 1024 * 1024)); Config::SetDefault("ns3::LteRlcAm::MaxTxBufferSize", UintegerValue(10 * 1024 * 1024));
    auto lte = CreateObject<LteHelper>(); auto epc = CreateObject<PointToPointEpcHelper>(); lte->SetEpcHelper(epc);
    Ptr<Node> pgw = epc->GetPgwNode(); NodeContainer remote; remote.Create(1); InternetStackHelper ip; ip.Install(remote);
    PointToPointHelper p2p; p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s"))); p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));
    auto rd = p2p.Install(pgw, remote.Get(0)); Ipv4AddressHelper a; a.SetBase("1.0.0.0","255.0.0.0"); a.Assign(rd);
    Ipv4StaticRoutingHelper rh; rh.GetStaticRouting(remote.Get(0)->GetObject<Ipv4>())->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);
    NodeContainer enbN, ueN; enbN.Create(1); ueN.Create(nUe);
    MobilityHelper mob; auto pos = CreateObject<ListPositionAllocator>(); pos->Add(Vector(0,0,30));
    mob.SetPositionAllocator(pos); mob.SetMobilityModel("ns3::ConstantPositionMobilityModel"); mob.Install(enbN);
    auto up = CreateObject<ListPositionAllocator>(); double d[4] = {50,100,150,200};
    for (uint32_t i = 0; i < nUe; i++) up->Add(Vector(d[i%4],0,1.5));
    mob.SetPositionAllocator(up); mob.Install(ueN);
    auto enbD = lte->InstallEnbDevice(enbN); auto ueD = lte->InstallUeDevice(ueN); ip.Install(ueN);
    auto ifs = epc->AssignUeIpv4Address(ueD);
    for (uint32_t i = 0; i < nUe; i++) {
        Ptr<Ipv4StaticRouting> sr = Ipv4StaticRoutingHelper().GetStaticRouting(ueN.Get(i)->GetObject<Ipv4>());
        sr->SetDefaultRoute(epc->GetUeDefaultGatewayAddress(), 1);
    }
    lte->Attach(ueD, enbD.Get(0));
    for (uint32_t i = 0; i < nUe; i++) {
        uint16_t port = 9000 + i;
        std::string fac = traffic == "udp" ? "ns3::UdpSocketFactory" : "ns3::TcpSocketFactory";
        PacketSinkHelper sh(fac, InetSocketAddress(Ipv4Address::GetAny(), port));
        auto sa = sh.Install(ueN.Get(i)); sa.Start(Seconds(0)); g_sinks.push_back(DynamicCast<PacketSink>(sa.Get(0)));
        if (traffic == "tcp") { BulkSendHelper bs(fac, InetSocketAddress(ifs.GetAddress(i), port)); bs.SetAttribute("MaxBytes", UintegerValue(0)); bs.SetAttribute("SendSize", UintegerValue(1400)); auto b = bs.Install(remote.Get(0)); b.Start(Seconds(1.0)); continue; }
        OnOffHelper oo(fac, InetSocketAddress(ifs.GetAddress(i), port));
        oo.SetAttribute("DataRate", DataRateValue(DataRate(std::to_string(int(load*1000)) + "kb/s")));
        oo.SetAttribute("PacketSize", UintegerValue(1400));
        oo.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        oo.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        auto ap = oo.Install(remote.Get(0)); ap.Start(Seconds(1.0));
    }
    g_out.open(out); g_out << "time_s,thr_total_mbps\n"; Simulator::Schedule(Seconds(1.0 + g_Ts), &Sample);
    Ptr<LteEnbNetDevice> enb = DynamicCast<LteEnbNetDevice>(enbD.Get(0));
    { std::stringstream ss(sched); std::string tok; while (std::getline(ss, tok, ',')) { auto c = tok.find(':'); Simulator::Schedule(Seconds(std::stod(tok.substr(0,c))), &SetN, mode, enb, (uint16_t)std::stoi(tok.substr(c+1))); } }
    Simulator::Stop(Seconds(simT)); Simulator::Run(); Simulator::Destroy(); return 0;
}
