// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <arc/Logger.h>
#include <arc/StringConv.h>
#include <arc/UserConfig.h>
#include <arc/URL.h>
#include <arc/client/ExecutionTarget.h>
#include <arc/client/EndpointQueryingStatus.h>
#include <arc/data/DataBuffer.h>
#include <arc/data/DataHandle.h>

#include "TargetInformationRetrieverPluginLDAPGLUE2.h"

namespace Arc {

  Logger TargetInformationRetrieverPluginLDAPGLUE2::logger(Logger::getRootLogger(), "TargetInformationRetrieverPlugin.LDAPGLUE2");

  bool TargetInformationRetrieverPluginLDAPGLUE2::isEndpointNotSupported(const Endpoint& endpoint) const {
    const std::string::size_type pos = endpoint.URLString.find("://");
    return pos != std::string::npos && lower(endpoint.URLString.substr(0, pos)) != "ldap";
  }

  static URL CreateURL(std::string service) {
    std::string::size_type pos1 = service.find("://");
    if (pos1 == std::string::npos) {
      service = "ldap://" + service;
      pos1 = 4;
    } else {
      if(lower(service.substr(0,pos1)) != "ldap") return URL();
    }
    std::string::size_type pos2 = service.find(":", pos1 + 3);
    std::string::size_type pos3 = service.find("/", pos1 + 3);
    if (pos3 == std::string::npos) {
      if (pos2 == std::string::npos)
        service += ":2135";
      // Is this a good default path?
      service += "/o=glue";
    }
    else if (pos2 == std::string::npos || pos2 > pos3)
      service.insert(pos3, ":2135");

    return service;
  }


  class Extractor {
  public:
    Extractor(XMLNode node, const std::string prefix = "", Logger* logger = NULL) : node(node), prefix(prefix), logger(logger) {}

    std::string get(const std::string name) {
      std::string value = node["GLUE2" + prefix + name];
      if (value.empty()) {
        value = (std::string)node["GLUE2" + name];
      }
      if (logger) logger->msg(DEBUG, "Extractor (%s): %s = %s", prefix, name, value);
      return value;
    }

    std::string operator[](const std::string name) {
      return get(name);
    }

    bool set(const std::string name, std::string& string) {
      std::string value = get(name);
      if (!value.empty()) {
        string = value;
        return true;
      } else {
        return false;
      }
    }

    bool set(const std::string name, Period& period) {
      std::string value = get(name);
      if (!value.empty()) {
        period = Period(value);
        return true;
      } else {
        return false;
      }
    }

    bool set(const std::string name, Time& time) {
      std::string value = get(name);
      if (!value.empty()) {
        time = Time(value);
        return true;
      } else {
        return false;
      }
    }

    bool set(const std::string name, int& integer) {
      std::string value = get(name);
      if (!value.empty()) {
        integer = stringtoi(value);
        return true;
      } else {
        return false;
      }
    }

    bool set(const std::string name, float& number) {
      std::string value = get(name);
      if (!value.empty()) {
        number = stringtof(value);
        return true;
      } else {
        return false;
      }
    }

    bool set(const std::string name, double& number) {
      std::string value = get(name);
      if (!value.empty()) {
        number = stringtod(value);
        return true;
      } else {
        return false;
      }
    }

    bool set(const std::string name, URL& url) {
      std::string value = get(name);
      if (!value.empty()) {
        url = URL(value);
        return true;
      } else {
        return false;
      }
    }

    bool set(const std::string name, bool& boolean) {
      std::string value = get(name);
      if (!value.empty()) {
        boolean = (value == "TRUE");
        return true;
      } else {
        return false;
      }
    }

    bool set(const std::string name, std::list<std::string>& list) {
      XMLNodeList nodelist = node.Path("GLUE2" + prefix + name);
      if (nodelist.empty()) {
        nodelist = node.Path("GLUE2" + name);
      }
      if (nodelist.empty()) {
        return false;
      }
      list.clear();
      for(XMLNodeList::iterator it = nodelist.begin(); it != nodelist.end(); it++) {
        std::string value = *it;
        list.push_back(value);
        if (logger) logger->msg(DEBUG, "Extractor (%s): %s contains %s", prefix, name, value);
      }
      return true;
    }

    static Extractor First(XMLNode& node, const std::string objectClass, Logger* logger = NULL) {
      XMLNode object = node.XPathLookup("//*[objectClass='GLUE2" + objectClass + "']", NS()).front();
      return Extractor(object, objectClass , logger);
    }

    static Extractor First(Extractor& e, const std::string objectClass) {
      return First(e.node, objectClass, e.logger);
    }

    static std::list<Extractor> All(XMLNode& node, const std::string objectClass, Logger* logger = NULL) {
      std::list<XMLNode> objects = node.XPathLookup("//*[objectClass='GLUE2" + objectClass + "']", NS());
      std::list<Extractor> extractors;
      for (std::list<XMLNode>::iterator it = objects.begin(); it != objects.end(); it++) {
        extractors.push_back(Extractor(*it, objectClass, logger));
      }
      return extractors;
    }

    static std::list<Extractor> All(Extractor& e, const std::string objectClass) {
      return All(e.node, objectClass, e.logger);
    }


    XMLNode node;
    std::string prefix;
    Logger *logger;
  };


  EndpointQueryingStatus TargetInformationRetrieverPluginLDAPGLUE2::Query(const UserConfig& uc, const Endpoint& ce, std::list<ComputingServiceType>& csList, const EndpointQueryOptions<ComputingServiceType>&) const {
    EndpointQueryingStatus s(EndpointQueryingStatus::FAILED);


    URL url(CreateURL(ce.URLString));
    url.ChangeLDAPScope(URL::subtree);

    if (!url) {
      return s;
    }

    DataHandle handler(url, uc);
    DataBuffer buffer;

    if (!handler) {
      logger.msg(INFO, "Can't create information handle - "
                 "is the ARC ldap DMC plugin available?");
      return s;
    }

    if (!handler->StartReading(buffer)) {
      return s;
    }

    int handle;
    unsigned int length;
    unsigned long long int offset;
    std::string result;

    while (buffer.for_write() || !buffer.eof_read())
      if (buffer.for_write(handle, length, offset, true)) {
        result.append(buffer[handle], length);
        buffer.is_written(handle);
      }

    if (!handler->StopReading()) {
      return s;
    }

    XMLNode xml_document(result);
    Extractor document(xml_document, "", &logger);

    std::list<Extractor> services = Extractor::All(document, "ComputingService");
    for (std::list<Extractor>::iterator it = services.begin(); it != services.end(); it++) {
      Extractor& service = *it;
      ComputingServiceType cs;
      AdminDomainType& AdminDomain = cs.AdminDomain;
      LocationType& Location = cs.Location;

      cs->Cluster = url; // contains the URL of the infosys that provided the info

      // GFD.147 GLUE2 5.3 Location
      Extractor location = Extractor::First(service, "Location");
      location.set("Address", Location->Address);
      location.set("Place", Location->Place);
      location.set("Country", Location->Country);
      location.set("PostCode", Location->PostCode);
      location.set("Latitude", Location->Latitude);
      location.set("Longitude", Location->Longitude);

      // GFD.147 GLUE2 5.5.1 Admin Domain
      Extractor domain = Extractor::First(document, "AdminDomain");
      domain.set("EntityName", AdminDomain->Name);
      domain.set("Owner", AdminDomain->Owner);

      // GFD.147 GLUE2 6.1 Computing Service
      service.set("EntityName", cs->Name);
      service.set("ServiceType", cs->Type);

      // GFD.147 GLUE2 6.2 ComputingEndpoint
      std::list<Extractor> endpoints = Extractor::All(service, "ComputingEndpoint");
      int endpointID = 0;
      for (std::list<Extractor>::iterator ite = endpoints.begin(); ite != endpoints.end(); ++ite) {
        Extractor& endpoint = *ite;
        endpoint.prefix = "Endpoint";

        ComputingEndpointType ComputingEndpoint;
        endpoint.set("URL", ComputingEndpoint->URLString);
        endpoint.set("Capability", ComputingEndpoint->Capability);
        endpoint.set("Technology", ComputingEndpoint->Technology);
        endpoint.set("InterfaceName", ComputingEndpoint->InterfaceName);
        endpoint.set("InterfaceVersion", ComputingEndpoint->InterfaceVersion);
        endpoint.set("InterfaceExtension", ComputingEndpoint->InterfaceExtension);
        endpoint.set("SupportedProfile", ComputingEndpoint->SupportedProfile);
        endpoint.set("Implementor", ComputingEndpoint->Implementor);
        ComputingEndpoint->Implementation = Software(endpoint["ImplementationName"], endpoint["ImplementationVersion"]);
        endpoint.set("QualityLevel", ComputingEndpoint->QualityLevel);
        endpoint.set("HealthState", ComputingEndpoint->HealthState);
        endpoint.set("HealthStateInfo", ComputingEndpoint->HealthStateInfo);
        endpoint.set("ServingState", ComputingEndpoint->ServingState);
        endpoint.set("IssuerCA", ComputingEndpoint->IssuerCA);
        endpoint.set("TrustedCA", ComputingEndpoint->TrustedCA);
        endpoint.set("DowntimeStarts", ComputingEndpoint->DowntimeStarts);
        endpoint.set("DowntimeEnds", ComputingEndpoint->DowntimeEnds);
        endpoint.set("Staging", ComputingEndpoint->Staging);
        endpoint.set("JobDescription", ComputingEndpoint->JobDescriptions);

        cs.ComputingEndpoint.insert(std::pair<int, ComputingEndpointType>(endpointID++, ComputingEndpoint));
      }

      // GFD.147 GLUE2 6.3 Computing Share
      std::list<Extractor> shares = Extractor::All(service, "ComputingShare");
      int shareID = 0;
      for (std::list<Extractor>::iterator its = shares.begin(); its != shares.end(); ++its) {
        Extractor& share = *its;

        ComputingShareType ComputingShare;
        share.set("EntityName", ComputingShare->Name);
        share.set("MappingQueue", ComputingShare->MappingQueue);
        share.set("MaxWallTime", ComputingShare->MaxWallTime);
        share.set("MaxTotalWallTime", ComputingShare->MaxTotalWallTime);
        share.set("MinWallTime", ComputingShare->MinWallTime);
        share.set("DefaultWallTime", ComputingShare->DefaultWallTime);
        share.set("MaxCPUTime", ComputingShare->MaxCPUTime);
        share.set("MaxTotalCPUTime", ComputingShare->MaxTotalCPUTime);
        share.set("MinCPUTime", ComputingShare->MinCPUTime);
        share.set("DefaultCPUTime", ComputingShare->DefaultCPUTime);
        share.set("MaxTotalJobs", ComputingShare->MaxTotalJobs);
        share.set("MaxRunningJobs", ComputingShare->MaxRunningJobs);
        share.set("MaxWaitingJobs", ComputingShare->MaxWaitingJobs);
        share.set("MaxPreLRMSWaitingJobs", ComputingShare->MaxPreLRMSWaitingJobs);
        share.set("MaxUserRunningJobs", ComputingShare->MaxUserRunningJobs);
        share.set("MaxSlotsPerJob", ComputingShare->MaxSlotsPerJob);
        share.set("MaxStageInStreams", ComputingShare->MaxStageInStreams);
        share.set("MaxStageOutStreams", ComputingShare->MaxStageOutStreams);
        share.set("SchedulingPolicy", ComputingShare->SchedulingPolicy);
        share.set("MaxMainMemory", ComputingShare->MaxMainMemory);
        share.set("MaxVirtualMemory", ComputingShare->MaxVirtualMemory);
        share.set("MaxDiskSpace", ComputingShare->MaxDiskSpace);
        share.set("DefaultStorageService", ComputingShare->DefaultStorageService);
        share.set("Preemption", ComputingShare->Preemption);
        share.set("TotalJobs", ComputingShare->TotalJobs);
        share.set("RunningJobs", ComputingShare->RunningJobs);
        share.set("LocalRunningJobs", ComputingShare->LocalRunningJobs);
        share.set("WaitingJobs", ComputingShare->WaitingJobs);
        share.set("LocalWaitingJobs", ComputingShare->LocalWaitingJobs);
        share.set("SuspendedJobs", ComputingShare->SuspendedJobs);
        share.set("LocalSuspendedJobs", ComputingShare->LocalSuspendedJobs);
        share.set("StagingJobs", ComputingShare->StagingJobs);
        share.set("PreLRMSWaitingJobs", ComputingShare->PreLRMSWaitingJobs);
        share.set("EstimatedAverageWaitingTime", ComputingShare->EstimatedAverageWaitingTime);
        share.set("EstimatedWorstWaitingTime", ComputingShare->EstimatedWorstWaitingTime);
        share.set("FreeSlots", ComputingShare->FreeSlots);
        std::string fswdValue = share["FreeSlotsWithDuration"];
        if (!fswdValue.empty()) {
          // Format: ns[:t] [ns[:t]]..., where ns is number of slots and t is the duration.
          ComputingShare->FreeSlotsWithDuration.clear();
          std::list<std::string> fswdList;
          tokenize(fswdValue, fswdList);
          for (std::list<std::string>::iterator it = fswdList.begin(); it != fswdList.end(); it++) {
            std::list<std::string> fswdPair;
            tokenize(*it, fswdPair, ":");
            long duration = LONG_MAX;
            int freeSlots = 0;
            if (fswdPair.size() > 2 || !stringto(fswdPair.front(), freeSlots) || (fswdPair.size() == 2 && !stringto(fswdPair.back(), duration))) {
              logger.msg(VERBOSE, "The \"FreeSlotsWithDuration\" attribute is wrongly formatted. Ignoring it.");
              logger.msg(DEBUG, "Wrong format of the \"FreeSlotsWithDuration\" = \"%s\" (\"%s\")", fswdValue, *it);
              continue;
            }
            ComputingShare->FreeSlotsWithDuration[Period(duration)] = freeSlots;
          }
        }
        share.set("UsedSlots", ComputingShare->UsedSlots);
        share.set("RequestedSlots", ComputingShare->RequestedSlots);
        share.set("ReservationPolicy", ComputingShare->ReservationPolicy);

        cs.ComputingShare.insert(std::pair<int, ComputingShareType>(shareID++, ComputingShare));
      }


      // GFD.147 GLUE2 6.4 Computing Manager
      std::list<Extractor> managers = Extractor::All(service, "ComputingManager");
      int managerID = 0;
      for (std::list<Extractor>::iterator itm = managers.begin(); itm != managers.end(); ++itm) {
        Extractor& manager = *itm;

        ComputingManagerType ComputingManager;
        manager.set("ManagerProductName", ComputingManager->ProductName);
        manager.set("ManagerProductVersion", ComputingManager->ProductVersion);
        manager.set("Reservation", ComputingManager->Reservation);
        manager.set("BulkSubmission", ComputingManager->BulkSubmission);
        manager.set("TotalPhysicalCPUs", ComputingManager->TotalPhysicalCPUs);
        manager.set("TotalLogicalCPUs", ComputingManager->TotalLogicalCPUs);
        manager.set("TotalSlots", ComputingManager->TotalSlots);
        manager.set("Homogeneous", ComputingManager->Homogeneous);
        manager.set("NetworkInfo", ComputingManager->NetworkInfo);
        manager.set("WorkingAreaShared", ComputingManager->WorkingAreaShared);
        manager.set("WorkingAreaTotal", ComputingManager->WorkingAreaTotal);
        manager.set("WorkingAreaFree", ComputingManager->WorkingAreaFree);
        manager.set("WorkingAreaLifeTime", ComputingManager->WorkingAreaLifeTime);
        manager.set("CacheTotal", ComputingManager->CacheTotal);
        manager.set("CacheFree", ComputingManager->CacheFree);

        // TODO: Only benchmarks belonging to this ComputingManager should be considered.
        // GFD.147 GLUE2 6.5 Benchmark
        std::list<Extractor> benchmarks = Extractor::All(service, "Benchmark");
        for (std::list<Extractor>::iterator itb = benchmarks.begin(); itb != benchmarks.end(); ++itb) {
          Extractor& benchmark = *itb;
          std::string Type; benchmark.set("Type", Type);
          double Value = -1.0; benchmark.set("Value", Value);
          (*ComputingManager.Benchmarks)[Type] = Value;
        }

        // GFD.147 GLUE2 6.6 Execution Environment
        std::list<Extractor> execenvironments = Extractor::All(service, "ExecutionEnvironment");
        int eeID = 0;
        for (std::list<Extractor>::iterator ite = execenvironments.begin(); ite != execenvironments.end(); ite++) {
          Extractor& environment = *ite;

          ExecutionEnvironmentType ExecutionEnvironment;
          environment.set("Platform", ExecutionEnvironment->Platform);
          environment.set("VirtualMachine", ExecutionEnvironment->VirtualMachine);
          environment.set("CPUVendor", ExecutionEnvironment->CPUVendor);
          environment.set("CPUModel", ExecutionEnvironment->CPUModel);
          environment.set("CPUVersion", ExecutionEnvironment->CPUVersion);
          environment.set("CPUClockSpeed", ExecutionEnvironment->CPUClockSpeed);
          environment.set("MainMemorySize", ExecutionEnvironment->MainMemorySize);
          std::string OSName = environment["OSName"];
          std::string OSVersion = environment["OSVersion"];
          std::string OSFamily = environment["OSFamily"];
          if (!OSName.empty()) {
            if (!OSVersion.empty()) {
              if (!OSFamily.empty()) {
                ExecutionEnvironment->OperatingSystem = Software(OSFamily, OSName, OSVersion);
              } else {
                ExecutionEnvironment->OperatingSystem = Software(OSName, OSVersion);
              }
            } else {
              ExecutionEnvironment->OperatingSystem = Software(OSName);
            }
          }
          environment.set("ConnectivityIn", ExecutionEnvironment->ConnectivityIn);
          environment.set("ConnectivityOut", ExecutionEnvironment->ConnectivityOut);

          ComputingManager.ExecutionEnvironment.insert(std::pair<int, ExecutionEnvironmentType>(eeID++, ExecutionEnvironment));
        }

        // GFD.147 GLUE2 6.7 Application Environment
        std::list<Extractor> appenvironments = Extractor::All(service, "ApplicationEnvironment");
        ComputingManager.ApplicationEnvironments->clear();
        for (std::list<Extractor>::iterator ita = appenvironments.begin(); ita != appenvironments.end(); ita++) {
          Extractor& application = *ita;
          ApplicationEnvironment ae(application["AppName"], application["AppVersion"]);
          ae.State = application["State"];
          ComputingManager.ApplicationEnvironments->push_back(ae);
        }

        cs.ComputingManager.insert(std::pair<int, ComputingManagerType>(managerID++, ComputingManager));
      }

      csList.push_back(cs);
    }

    if (!csList.empty()) s = EndpointQueryingStatus::SUCCESSFUL;
    return s;
  }

} // namespace Arc