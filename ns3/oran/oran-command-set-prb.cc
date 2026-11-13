/**
 * NIST-developed software is provided by NIST as a public service. You may
 * use, copy and distribute copies of the software in any medium, provided that
 * you keep intact this entire notice. You may improve, modify and create
 * derivative works of the software or any portion of the software, and you may
 * copy and distribute such modifications or works. Modified works should carry
 * a notice stating that you changed the software and should note the date and
 * nature of any such change. Please explicitly acknowledge the National
 * Institute of Standards and Technology as the source of the software.
 *
 * NIST-developed software is expressly provided "AS IS." NIST MAKES NO
 * WARRANTY OF ANY KIND, EXPRESS, IMPLIED, IN FACT OR ARISING BY OPERATION OF
 * LAW, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTY OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, NON-INFRINGEMENT AND DATA ACCURACY. NIST
 * NEITHER REPRESENTS NOR WARRANTS THAT THE OPERATION OF THE SOFTWARE WILL BE
 * UNINTERRUPTED OR ERROR-FREE, OR THAT ANY DEFECTS WILL BE CORRECTED. NIST
 * DOES NOT WARRANT OR MAKE ANY REPRESENTATIONS REGARDING THE USE OF THE
 * SOFTWARE OR THE RESULTS THEREOF, INCLUDING BUT NOT LIMITED TO THE
 * CORRECTNESS, ACCURACY, RELIABILITY, OR USEFULNESS OF THE SOFTWARE.
 *
 * You are solely responsible for determining the appropriateness of using and
 * distributing the software and you assume all risks associated with its use,
 * including but not limited to the risks and costs of program errors,
 * compliance with applicable laws, damage to or loss of data, programs or
 * equipment, and the unavailability or interruption of operation. This
 * software is not intended to be used in any situation where a failure could
 * cause risk of injury or damage to property. The software developed by NIST
 * employees is not subject to copyright protection within the United States.
 */

#include "oran-command-set-prb.h"

#include "ns3/log.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("OranCommandSetPrb");

NS_OBJECT_ENSURE_REGISTERED(OranCommandSetPrb);

TypeId
OranCommandSetPrb::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::OranCommandSetPrb")
            .SetParent<OranCommand>()
            .AddConstructor<OranCommandSetPrb>()
            .AddAttribute("Prb",
                          "Nominal DL PRB cap to apply at the eNB (one of 6,15,25,50,75,100).",
                          UintegerValue(100),
                          MakeUintegerAccessor(&OranCommandSetPrb::m_prb),
                          MakeUintegerChecker<uint16_t>());

    return tid;
}

OranCommandSetPrb::OranCommandSetPrb()
    : OranCommand()
{
    NS_LOG_FUNCTION(this);
}

OranCommandSetPrb::~OranCommandSetPrb()
{
    NS_LOG_FUNCTION(this);
}

std::string
OranCommandSetPrb::ToString() const
{
    NS_LOG_FUNCTION(this);

    std::stringstream ss;

    ss << "OranCommandSetPrb("
       << "TargetE2NodeId = " << GetTargetE2NodeId() << "; Prb = " << m_prb << ")";

    return ss.str();
}

uint16_t
OranCommandSetPrb::GetPrb() const
{
    NS_LOG_FUNCTION(this);

    return m_prb;
}

} // namespace ns3
