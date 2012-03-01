// -*- indent-tabs-mode: nil -*-

#ifndef __ARC_TARGETINFORMATIONRETRIEVERBES_H__
#define __ARC_TARGETINFORMATIONRETRIEVERBES_H__

#include <list>

#include <arc/client/EndpointRetriever.h>

namespace Arc {

  class ExecutionTarget;
  class Logger;
  class UserConfig;

  class TargetInformationRetrieverPluginBES : public TargetInformationRetrieverPlugin {
  public:
    TargetInformationRetrieverPluginBES() { supportedInterfaces.push_back("org.ogf.bes"); };
    ~TargetInformationRetrieverPluginBES() {};

    static Plugin* Instance(PluginArgument *) { return new TargetInformationRetrieverPluginBES(); };
    virtual EndpointQueryingStatus Query(const UserConfig&, const ComputingInfoEndpoint&, std::list<ExecutionTarget>&, const EndpointQueryOptions<ExecutionTarget>&) const;
    virtual bool isEndpointNotSupported(const ComputingInfoEndpoint&) const;

  private:
    static Logger logger;
  };

} // namespace Arc

#endif // __ARC_TARGETINFORMATIONRETRIEVERBES_H__
