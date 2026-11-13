// Single LTE cell + ns3-oran E2 path for DL PRB control. See README.md.
// policy: openloop (OranLmOpenLoopPrb schedule) | fixed (constant cap, no LM) | threshold | model (Phase 5)
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/oran-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/component-carrier-enb.h"

#include <cmath>
#include <fstream>
#include <sstream>

using namespace ns3;

static std::vector<Ptr<PacketSink>> g_sinks;
static std::vector<uint64_t> g_lastRx;
static std::ofstream g_out;
static double g_Ts = 0.1;
static uint32_t g_k = 0;
static std::string g_policy;
static uint32_t g_seed;
static Ptr<LteEnbNetDevice> g_enb;
static Ptr<OranLmOpenLoopPrb> g_olLm;
static std::vector<Ptr<OnOffApplication>> g_onoff;
static double g_offeredPerUe = 0;
static std::vector<double> g_applyTimes;
static void OnPrbApplied(uint16_t) { g_applyTimes.push_back(Simulator::Now().GetSeconds()); }

static uint16_t EffectivePrb(uint16_t nominal)
{
    // mirrors PfFfMacScheduler patch: 4-PRB RBGs at 100 PRB, allowed = max(1, round(N/4))
    return 4 * std::max<long>(1, std::lround(nominal / 4.0));
}
static uint16_t CurrentNominal()
{
    UintegerValue v;
    DynamicCast<ComponentCarrierEnb>(g_enb->GetCcMap().at(0))->GetFfMacScheduler()->GetAttribute("MaxDlPrb", v);
    return v.Get();
}
static void SetCap(uint16_t n)
{
    DynamicCast<ComponentCarrierEnb>(g_enb->GetCcMap().at(0))->GetFfMacScheduler()->SetAttribute("MaxDlPrb", UintegerValue(n));
}
static void SetLoad(double mbpsPerUe)
{
    g_offeredPerUe = mbpsPerUe;
    for (auto& a : g_onoff)
        a->SetAttribute("DataRate", DataRateValue(DataRate(static_cast<uint64_t>(mbpsPerUe * 1e6))));
}
static void Sample()
{
    std::stringstream ue;
    double tot = 0;
    for (size_t i = 0; i < g_sinks.size(); i++)
    {
        uint64_t rx = g_sinks[i]->GetTotalRx();
        double m = (rx - g_lastRx[i]) * 8.0 / g_Ts / 1e6;
        g_lastRx[i] = rx;
        tot += m;
        ue << "," << m;
    }
    uint16_t nom = CurrentNominal();
    uint16_t req = g_olLm ? g_olLm->GetLastRequested() : nom;
    g_out << Simulator::Now().GetSeconds() << "," << g_k++ << "," << EffectivePrb(nom) << ","
          << EffectivePrb(req ? req : nom) << "," << tot << ue.str() << "," << g_offeredPerUe * g_sinks.size() << ","
          << g_policy << "," << g_seed << "," << nom << "\n";
    Simulator::Schedule(Seconds(g_Ts), &Sample);
}
static void ScheduleKv(const std::string& s, void (*fn)(uint16_t))
{
    std::stringstream ss(s); std::string tok;
    while (std::getline(ss, tok, ',')) { auto c = tok.find(':'); Simulator::Schedule(Seconds(std::stod(tok.substr(0, c))), fn, (uint16_t)std::stoi(tok.substr(c + 1))); }
}

int main(int argc, char* argv[])
{
    uint32_t nUe = 4; std::string traffic = "udp", policy = "openloop", schedule = "0:100", loadSched = "0:6", out = "run.csv", dbFile = "";
    double simTime = 60, Ts = 0.1, startT = 1.0, ricPhase = 0.005; uint32_t seed = 1, fixedPrb = 100, e2Latency = 0; std::string distances = "50,100,150,200";
    CommandLine cmd(__FILE__);
    cmd.AddValue("nUe", "number of UEs", nUe);
    cmd.AddValue("traffic", "udp|tcp (tcp = rate-limited OnOff over TCP)", traffic);
    cmd.AddValue("loadSched", "offered load per UE in Mbps: \"t:Mbps,...\"", loadSched);
    cmd.AddValue("simTime", "simulation time (s)", simTime);
    cmd.AddValue("Ts", "control/measurement period (s)", Ts);
    cmd.AddValue("seed", "RNG run number", seed);
    cmd.AddValue("policy", "openloop|fixed|threshold|model", policy);
    cmd.AddValue("schedule", "open-loop PRB schedule \"t:N,...\" (nominal levels)", schedule);
    cmd.AddValue("fixedPrb", "nominal PRB for policy=fixed", fixedPrb);
    cmd.AddValue("out", "output CSV", out);
    cmd.AddValue("distances", "UE distances from the eNB in m, comma separated (cycled over UEs)", distances);
    cmd.AddValue("e2Latency", "extra command (actuation) latency in whole control periods (0,1,2), applied at the eNB terminator; reports unaffected", e2Latency);
    cmd.AddValue("ricPhase", "RIC query phase after the report instant (s); LM polls at 0.2+phase+k*Ts", ricPhase);
    cmd.AddValue("db", "sqlite file for the RIC data repository (E2 log)", dbFile);
    cmd.Parse(argc, argv);
    if (policy == "threshold" || policy == "model") NS_FATAL_ERROR("policy " << policy << " is implemented in Phase 5");
    g_Ts = Ts; g_policy = policy; g_seed = seed;
    RngSeedManager::SetSeed(1); RngSeedManager::SetRun(seed);

    if (traffic == "tcp")
    {
        Config::SetDefault("ns3::LteEnbRrc::EpsBearerToRlcMapping", EnumValue(LteEnbRrc::RLC_AM_ALWAYS));
        Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1400));
        Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(4194304));
        Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(4194304));
    }
    else Config::SetDefault("ns3::LteEnbRrc::EpsBearerToRlcMapping", EnumValue(LteEnbRrc::RLC_UM_ALWAYS));
    Config::SetDefault("ns3::LteHelper::Scheduler", StringValue("ns3::PfFfMacScheduler"));
    Config::SetDefault("ns3::LteEnbNetDevice::DlBandwidth", UintegerValue(100));
    Config::SetDefault("ns3::LteEnbNetDevice::UlBandwidth", UintegerValue(100));
    Config::SetDefault("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue(10 * 1024 * 1024));
    Config::SetDefault("ns3::LteRlcAm::MaxTxBufferSize", UintegerValue(10 * 1024 * 1024));

    auto lte = CreateObject<LteHelper>(); auto epc = CreateObject<PointToPointEpcHelper>(); lte->SetEpcHelper(epc);
    Ptr<Node> pgw = epc->GetPgwNode(); NodeContainer remote; remote.Create(1); InternetStackHelper ip; ip.Install(remote);
    PointToPointHelper p2p; p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s"))); p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));
    auto rd = p2p.Install(pgw, remote.Get(0)); Ipv4AddressHelper a; a.SetBase("1.0.0.0", "255.0.0.0"); a.Assign(rd);
    Ipv4StaticRoutingHelper().GetStaticRouting(remote.Get(0)->GetObject<Ipv4>())->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);
    NodeContainer enbN, ueN; enbN.Create(1); ueN.Create(nUe);
    MobilityHelper mob; auto pe = CreateObject<ListPositionAllocator>(); pe->Add(Vector(0, 0, 30));
    mob.SetPositionAllocator(pe); mob.SetMobilityModel("ns3::ConstantPositionMobilityModel"); mob.Install(enbN);
    auto pu = CreateObject<ListPositionAllocator>(); std::vector<double> d;
    { std::stringstream ss(distances); std::string tok; while (std::getline(ss, tok, ',')) d.push_back(std::stod(tok)); }
    for (uint32_t i = 0; i < nUe; i++) pu->Add(Vector(d[i % d.size()], 0, 1.5));
    mob.SetPositionAllocator(pu); mob.Install(ueN);
    auto enbD = lte->InstallEnbDevice(enbN); auto ueD = lte->InstallUeDevice(ueN); ip.Install(ueN);
    auto ifs = epc->AssignUeIpv4Address(ueD);
    for (uint32_t i = 0; i < nUe; i++)
        Ipv4StaticRoutingHelper().GetStaticRouting(ueN.Get(i)->GetObject<Ipv4>())->SetDefaultRoute(epc->GetUeDefaultGatewayAddress(), 1);
    lte->Attach(ueD, enbD.Get(0));
    g_enb = DynamicCast<LteEnbNetDevice>(enbD.Get(0));

    std::string fac = traffic == "udp" ? "ns3::UdpSocketFactory" : "ns3::TcpSocketFactory";
    for (uint32_t i = 0; i < nUe; i++)
    {
        uint16_t port = 9000 + i;
        PacketSinkHelper sh(fac, InetSocketAddress(Ipv4Address::GetAny(), port));
        auto sa = sh.Install(ueN.Get(i)); sa.Start(Seconds(0));
        g_sinks.push_back(DynamicCast<PacketSink>(sa.Get(0))); g_lastRx.push_back(0);
        OnOffHelper oo(fac, InetSocketAddress(ifs.GetAddress(i), port));
        oo.SetAttribute("PacketSize", UintegerValue(1400));
        oo.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        oo.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        oo.SetAttribute("DataRate", DataRateValue(DataRate("1Mb/s")));
        auto ap = oo.Install(remote.Get(0)); ap.Start(Seconds(startT));
        g_onoff.push_back(DynamicCast<OnOffApplication>(ap.Get(0)));
    }
    { std::stringstream ss(loadSched); std::string tok;
      while (std::getline(ss, tok, ',')) { auto c = tok.find(':'); double t = std::stod(tok.substr(0, c)), L = std::stod(tok.substr(c + 1));
          Simulator::Schedule(Seconds(std::max(0.0, t)), &SetLoad, L); } }

    // ---- O-RAN near-RT RIC with E2 path (reporter: aggregate DL throughput; command: set PRB) ----
    Ptr<OranDataRepository> dataRepo = CreateObject<OranDataRepositorySqlite>();
    Ptr<OranLmOpenLoopPrb> lm = CreateObject<OranLmOpenLoopPrb>();
    Ptr<OranCmm> cmm = CreateObject<OranCmmNoop>();
    Ptr<OranNearRtRic> ric = CreateObject<OranNearRtRic>();
    Ptr<OranNearRtRicE2Terminator> ricTerm = CreateObject<OranNearRtRicE2Terminator>();
    if (policy == "openloop") { lm->SetAttribute("Schedule", StringValue(schedule)); g_olLm = lm; }
    else { lm->SetAttribute("Schedule", StringValue("0:" + std::to_string(fixedPrb))); g_olLm = lm; }  // fixed: commanded once through E2 as well
    if (dbFile.empty()) dbFile = out + ".e2.db"; std::remove(dbFile.c_str());
    dataRepo->SetAttribute("DatabaseFile", StringValue(dbFile));
    lm->SetAttribute("Verbose", BooleanValue(false)); lm->SetAttribute("NearRtRic", PointerValue(ric));
    lm->SetAttribute("ProcessingDelayRv", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    cmm->SetAttribute("NearRtRic", PointerValue(ric));
    ricTerm->SetAttribute("NearRtRic", PointerValue(ric)); ricTerm->SetAttribute("DataRepository", PointerValue(dataRepo));
    ricTerm->SetAttribute("TransmissionDelayRv", StringValue("ns3::ConstantRandomVariable[Constant=0.001]"));
    ric->SetAttribute("DefaultLogicModule", PointerValue(lm)); ric->SetAttribute("E2Terminator", PointerValue(ricTerm));
    ric->SetAttribute("DataRepository", PointerValue(dataRepo)); ric->SetAttribute("LmQueryInterval", TimeValue(Seconds(Ts)));
    ric->SetAttribute("ConflictMitigationModule", PointerValue(cmm));
    ric->SetAttribute("E2NodeInactivityThreshold", TimeValue(Seconds(1e6)));
    ric->SetAttribute("E2NodeInactivityIntervalRv", StringValue("ns3::ConstantRandomVariable[Constant=1e6]"));
    ric->SetAttribute("LmQueryMaxWaitTime", TimeValue(Seconds(0)));
    ric->SetAttribute("LmQueryLateCommandPolicy", EnumValue(OranNearRtRic::DROP));
    Simulator::Schedule(Seconds(0.2 + ricPhase), &OranNearRtRic::Start, ric);

    Ptr<OranE2NodeTerminatorLteEnb> enbTerm = CreateObject<OranE2NodeTerminatorLteEnb>();
    Ptr<OranReporterThroughput> thrRep = CreateObject<OranReporterThroughput>();
    thrRep->SetAttribute("Terminator", PointerValue(enbTerm));
    Ptr<OranReportTriggerPeriodic> trig = CreateObject<OranReportTriggerPeriodic>();
    { std::ostringstream rv; rv << "ns3::ConstantRandomVariable[Constant=" << Ts << "]"; trig->SetAttribute("IntervalRv", StringValue(rv.str())); }
    thrRep->SetAttribute("Trigger", PointerValue(trig));
    enbTerm->SetAttribute("NearRtRic", PointerValue(ric));
    enbTerm->SetAttribute("RegistrationIntervalRv", StringValue("ns3::ConstantRandomVariable[Constant=0.05]"));
    enbTerm->SetAttribute("SendIntervalRv", StringValue("ns3::ConstantRandomVariable[Constant=0.001]"));
    enbTerm->SetAttribute("TransmissionDelayRv", StringValue("ns3::ConstantRandomVariable[Constant=0.001]"));
    enbTerm->SetAttribute("ExtraCommandDelay", TimeValue(Seconds(e2Latency * Ts)));
    enbTerm->AddReporter(thrRep); enbTerm->Attach(enbN.Get(0));
    for (auto& s : g_sinks) s->TraceConnectWithoutContext("Rx", MakeCallback(&OranReporterThroughput::AddRx, thrRep));
    Simulator::Schedule(Seconds(0.0), &OranE2NodeTerminatorLteEnb::Activate, enbTerm); // report grid = k*Ts, aligned with the log windows
    enbTerm->TraceConnectWithoutContext("PrbApplied", MakeCallback(&OnPrbApplied));

    g_out.open(out);
    g_out << "time_s,k,prb_applied,prb_requested,thr_total_mbps";
    for (uint32_t i = 0; i < nUe; i++) g_out << ",thr_ue_" << i;
    g_out << ",offered_load_mbps,policy,seed,prb_nominal\n";
    Simulator::Schedule(Seconds(startT + Ts), &Sample);
    Simulator::Stop(Seconds(simTime)); Simulator::Run();
    { std::ofstream lf(out + ".latency.csv"); lf << "issue_time_s,apply_time_s,latency_s\n";
      const auto& iss = lm->GetIssueTimes(); for (double ap : g_applyTimes) { double is = -1; for (double t : iss) if (t <= ap) is = t; lf << is << "," << ap << "," << ap - is << "\n"; }
      std::ofstream rf(out + ".lmrun.csv"); rf << "run_time_s,newest_report_time_s\n"; for (auto& r : lm->GetRunLog()) rf << r.first << "," << r.second << "\n"; }
    Simulator::Destroy();
    return 0;
}
