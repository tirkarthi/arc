// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string>
#include <sstream>

#include <glibmm.h>

#include <arc/StringConv.h>
#include <arc/UserConfig.h>
#include <arc/client/ExecutionTarget.h>
#include <arc/client/Job.h>
#include <arc/client/JobDescription.h>
#include <arc/message/MCC.h>

#include "SubmitterEMIES.h"
#include "EMIESClient.h"

namespace Arc {

  Logger SubmitterEMIES::logger(Logger::getRootLogger(), "Submitter.EMIES");

  SubmitterEMIES::SubmitterEMIES(const UserConfig& usercfg)
    : Submitter(usercfg, "EMIES") {}

  SubmitterEMIES::~SubmitterEMIES() {
    deleteAllClients();
  }

  Plugin* SubmitterEMIES::Instance(PluginArgument *arg) {
    SubmitterPluginArgument *subarg =
      dynamic_cast<SubmitterPluginArgument*>(arg);
    if (!subarg) return NULL;
    return new SubmitterEMIES(*subarg);
  }

  EMIESClient* SubmitterEMIES::acquireClient(const URL& url) {
    std::map<URL, EMIESClient*>::const_iterator url_it = clients.find(url);
    if ( url_it != clients.end() ) {
      return url_it->second;
    } else {
      MCCConfig cfg;
      usercfg.ApplyToConfig(cfg);
      EMIESClient* ac = new EMIESClient(url, cfg, usercfg.Timeout());
      return clients[url] = ac;
    }
  }

  bool SubmitterEMIES::releaseClient(const URL& url) {
    return true;
  }

  bool SubmitterEMIES::deleteAllClients() {
    std::map<URL, EMIESClient*>::iterator it;
    for (it = clients.begin(); it != clients.end(); it++) {
        if ((*it).second != NULL) delete (*it).second;
    }
    return true;
  }

  bool SubmitterEMIES::Submit(const JobDescription& jobdesc,
                             const ExecutionTarget& et, Job& job) {
    // TODO: this is multi step process. So having retries would be nice.

    EMIESClient* ac = acquireClient(et.url);

    JobDescription preparedjobdesc(jobdesc);

    // TODO: redo after EMI ES job description is implemented
    /* The above comment is from removed ModifyJobDescription method. It has
     * been replaced by the JobDescription::Prepare method.
     */

    if (!preparedjobdesc.Prepare(et)) {
      logger.msg(INFO, "Failed preparing job description to target resources");
      releaseClient(et.url);
      return false;
    }

    std::string product;
    if (!preparedjobdesc.UnParse(product, "emies:adl")) {
      logger.msg(INFO, "Unable to submit job. Job description is not valid in the %s format", "emies:adl");
      releaseClient(et.url);
      return false;
    }

    EMIESJob jobid;
    EMIESJobState jobstate;
    if (!ac->submit(product, jobid, jobstate, et.url.Protocol() == "https")) {
      logger.msg(INFO, "Failed to submit job description");
      releaseClient(et.url);
      return false;
    }

    if (!jobid) {
      logger.msg(INFO, "No valid job identifier returned by EMI ES");
      releaseClient(et.url);
      return false;
    }

    if(!jobid.manager) jobid.manager = et.url;

    for(;;) {
      if(jobstate.HasAttribute("CLIENT-STAGEIN-POSSIBLE")) break;
      if(jobstate.state == "TERMINAL") {
        logger.msg(INFO, "Job failed on service side");
        releaseClient(et.url);
        return false;
      }
      // If service jumped over stageable state client probably does not
      // have to send anything.
      if((jobstate.state != "ACCEPTED") && (jobstate.state != "PREPROCESSING")) break;
      sleep(5);
      if(!ac->stat(jobid, jobstate)) {
        logger.msg(INFO, "Failed to obtain state of job");
        releaseClient(et.url);
        return false;
      };
    }

    if(jobstate.HasAttribute("CLIENT-STAGEIN-POSSIBLE")) {
      URL stageurl(jobid.stagein);
      if (!PutFiles(preparedjobdesc, stageurl)) {
        logger.msg(INFO, "Failed uploading local input files");
        releaseClient(et.url);
        return false;
      }
      // It is not clear how service is implemented. So notifying should not harm.
      if (!ac->notify(jobid)) {
        logger.msg(INFO, "Failed to notify service");
        releaseClient(et.url);
        return false;
      }
    } else {
      // TODO: check if client has files to send
    }

    // URL-izing job id
    URL jobidu(jobid.manager);
    jobidu.AddOption("emiesjobid",jobid.id,true);

    AddJobDetails(preparedjobdesc, jobidu, et.Cluster, jobid.manager, job);

    releaseClient(et.url);
    return true;
  }

  bool SubmitterEMIES::Migrate(const URL& jobid, const JobDescription& jobdesc,
                         const ExecutionTarget& et, bool forcemigration,
                         Job& job) {
    logger.msg(VERBOSE, "Migration for EMI ES is not implemented");
    return false;
  }


} // namespace Arc
