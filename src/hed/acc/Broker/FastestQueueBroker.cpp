// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cstdlib>
#include <algorithm>
#include "FastestQueueBroker.h"

namespace Arc {

  bool CompareExecutionTarget(const ExecutionTarget* T1, const ExecutionTarget* T2) {

    //Scale queue to become cluster size independent
    float T1queue = (float)T1->WaitingJobs / T1->TotalSlots;
    float T2queue = (float)T2->WaitingJobs / T2->TotalSlots;

    return T1queue < T2queue;

  }

  FastestQueueBroker::FastestQueueBroker(Config *cfg)
    : Broker(cfg) {}

  FastestQueueBroker::~FastestQueueBroker() {}

  Plugin* FastestQueueBroker::Instance(PluginArgument *arg) {
    ACCPluginArgument *accarg =
      arg ? dynamic_cast<ACCPluginArgument*>(arg) : NULL;
    if (!accarg)
      return NULL;
    return new FastestQueueBroker((Config*)(*accarg));
  }

  void FastestQueueBroker::SortTargets() {

    logger.msg(DEBUG, "FastestQueueBroker is filtering %d targets", PossibleTargets.size());

    //Remove clusters with incomplete information for target sorting
    std::list<ExecutionTarget*>::iterator iter = PossibleTargets.begin();
    while (iter != PossibleTargets.end()) {
      if ((*iter)->WaitingJobs == -1 || (*iter)->TotalSlots == -1 || (*iter)->FreeSlots == -1) {
        if ((*iter)->WaitingJobs == -1)
          logger.msg(DEBUG, "Target %s removed by FastestQueueBroker, doesn't report number of waiting jobs", (*iter)->DomainName);
        else if ((*iter)->TotalSlots == -1)
          logger.msg(DEBUG, "Target %s removed by FastestQueueBroker, doesn't report number of total slots", (*iter)->DomainName);
        else if ((*iter)->FreeSlots == -1)
          logger.msg(DEBUG, "Target %s removed by FastestQueueBroker, doesn't report number of free slots", (*iter)->DomainName);
        iter = PossibleTargets.erase(iter);
        continue;
      }
      iter++;
    }

    logger.msg(DEBUG, "FastestQueueBroker will rank the following %d targets", PossibleTargets.size());
    iter = PossibleTargets.begin();
    for (int i = 1; iter != PossibleTargets.end(); iter++, i++)
      logger.msg(DEBUG, "%d. Cluster: %s", i, (*iter)->DomainName);

    //Sort the targets according to the number of waiting jobs (in % of the cluster size)
    PossibleTargets.sort(CompareExecutionTarget);

    //Check is several clusters(queues) have 0 waiting jobs
    int ZeroQueueCluster = 0;
    int TotalFreeCPUs = 0;
    for (iter = PossibleTargets.begin(); iter != PossibleTargets.end(); iter++)
      if ((*iter)->WaitingJobs == 0) {
        ZeroQueueCluster++;
        TotalFreeCPUs += (*iter)->FreeSlots / abs(job->Slots);
      }

    //If several clusters(queues) have free slots (CPUs) do basic load balancing
    if (ZeroQueueCluster > 1)
      for (std::list<ExecutionTarget*>::iterator itN = PossibleTargets.begin();
           itN != PossibleTargets.end() && (*itN)->WaitingJobs == 0;
           itN++) {
        double RandomCPU = rand() * TotalFreeCPUs;
        for (std::list<ExecutionTarget*>::iterator itJ = itN;
             itJ != PossibleTargets.end() && (*itJ)->WaitingJobs == 0;
             itJ++) {
          if (((*itJ)->FreeSlots / abs(job->Slots)) > RandomCPU) {
            TotalFreeCPUs -= ((*itJ)->FreeSlots / abs(job->Slots));
            std::iter_swap(itJ, itN);
            break;
          }
          else
            RandomCPU -= ((*itJ)->FreeSlots / abs(job->Slots));
        }
      }

    logger.msg(DEBUG, "Best targets are: %d", PossibleTargets.size());

    iter = PossibleTargets.begin();

    for (int i = 1; iter != PossibleTargets.end(); iter++, i++)
      logger.msg(DEBUG, "%d. Cluster: %s", i, (*iter)->DomainName);

    TargetSortingDone = true;

  }

} // namespace Arc
