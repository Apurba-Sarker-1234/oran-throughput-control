#include "oran-lm-openloop-prb.h"

#include "oran-command-set-prb.h"
#include "oran-data-repository.h"
#include "oran-near-rt-ric.h"

#include "ns3/abort.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"

#include <sstream>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("OranLmOpenLoopPrb");
NS_OBJECT_ENSURE_REGISTERED(OranLmOpenLoopPrb);

TypeId
OranLmOpenLoopPrb::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::OranLmOpenLoopPrb")
            .SetParent<OranLm>()
            .AddConstructor<OranLmOpenLoopPrb>()
            .AddAttribute("Schedule",
                          "PRB schedule \"t0:N0,t1:N1,...\" (absolute seconds, nominal PRBs)",
                          StringValue("0:100"),
                          MakeStringAccessor(&OranLmOpenLoopPrb::m_schedule),
                          MakeStringChecker());
    return tid;
}

OranLmOpenLoopPrb::OranLmOpenLoopPrb()
{
}

OranLmOpenLoopPrb::~OranLmOpenLoopPrb()
{
}

uint16_t
OranLmOpenLoopPrb::GetLastRequested() const
{
    return m_lastSent;
}

std::vector<Ptr<OranCommand>>
OranLmOpenLoopPrb::Run()
{
    std::vector<Ptr<OranCommand>> commands;
    if (!m_active)
    {
        return commands;
    }
    if (m_levels.empty())
    {
        std::stringstream ss(m_schedule);
        std::string tok;
        while (std::getline(ss, tok, ','))
        {
            auto c = tok.find(':');
            NS_ABORT_MSG_IF(c == std::string::npos, "bad schedule token " << tok);
            m_levels[std::stod(tok.substr(0, c))] = static_cast<uint16_t>(std::stoi(tok.substr(c + 1)));
        }
    }
    double now = Simulator::Now().GetSeconds();
    {
        double newest = -1;
        for (uint64_t id : m_nearRtRic->Data()->GetLteEnbE2NodeIds())
        {
            auto r = m_nearRtRic->Data()->GetThroughput(id, 1);
            if (!r.empty())
            {
                newest = r[0].first.GetSeconds();
            }
        }
        m_runLog.emplace_back(now, newest);
    }
    uint16_t level = 0;
    for (const auto& kv : m_levels)
    {
        if (kv.first <= now + 1e-9)
        {
            level = kv.second;
        }
    }
    if (level != 0 && level != m_lastSent)
    {
        for (uint64_t id : m_nearRtRic->Data()->GetLteEnbE2NodeIds())
        {
            Ptr<OranCommandSetPrb> cmd = CreateObject<OranCommandSetPrb>();
            cmd->SetAttribute("TargetE2NodeId", UintegerValue(id));
            cmd->SetAttribute("Prb", UintegerValue(level));
            commands.push_back(cmd);
        }
        if (!commands.empty())
        {
            m_lastSent = level;
            m_issueTimes.push_back(now);
        }
    }
    return commands;
}

} // namespace ns3
