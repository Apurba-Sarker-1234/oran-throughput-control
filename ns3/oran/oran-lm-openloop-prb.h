#ifndef ORAN_LM_OPENLOOP_PRB_H
#define ORAN_LM_OPENLOOP_PRB_H

#include "oran-lm.h"

#include <map>

namespace ns3
{

/**
 * @ingroup oran
 *
 * Open-loop Logic Module: replays a PRB schedule "t0:N0,t1:N1,..." (absolute simulation
 * seconds, nominal PRB levels) by issuing OranCommandSetPrb to every LTE eNB E2 node
 * whenever the scheduled level changes. Used for identification data and step tests so that
 * open-loop runs traverse the same E2 path as the closed-loop xApps.
 */
class OranLmOpenLoopPrb : public OranLm
{
  public:
    static TypeId GetTypeId();
    OranLmOpenLoopPrb();
    ~OranLmOpenLoopPrb() override;
    std::vector<Ptr<OranCommand>> Run() override;
    /// Last requested (nominal) PRB level, 0 if none yet.
    uint16_t GetLastRequested() const;
    /// Times at which commands were issued by Run()
    const std::vector<double>& GetIssueTimes() const { return m_issueTimes; }
    /// (Run time, generation time of the newest throughput report seen) per Run()
    const std::vector<std::pair<double, double>>& GetRunLog() const { return m_runLog; }

  private:
    std::string m_schedule;                       //!< "t:N,..." string
    std::map<double, uint16_t> m_levels;          //!< parsed schedule
    std::vector<double> m_issueTimes;
    std::vector<std::pair<double, double>> m_runLog;
    uint16_t m_lastSent{0};                       //!< last level commanded
};

} // namespace ns3

#endif
