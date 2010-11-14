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

#include "SubmitterARC1.h"
#include "AREXClient.h"

namespace Arc {

  Logger SubmitterARC1::logger(Submitter::logger, "ARC1");

  SubmitterARC1::SubmitterARC1(const UserConfig& usercfg)
    : Submitter(usercfg, "ARC1") {}

  SubmitterARC1::~SubmitterARC1() {}

  Plugin* SubmitterARC1::Instance(PluginArgument *arg) {
    SubmitterPluginArgument *subarg =
      dynamic_cast<SubmitterPluginArgument*>(arg);
    if (!subarg)
      return NULL;
    return new SubmitterARC1(*subarg);
  }

  bool SubmitterARC1::Submit(const JobDescription& jobdesc,
                             const ExecutionTarget& et, Job& job) const {
    MCCConfig cfg;
    usercfg.ApplyToConfig(cfg);
    AREXClient ac(et.url, cfg, usercfg.Timeout());

    JobDescription modjobdesc(jobdesc);

    if (!ModifyJobDescription(modjobdesc, et)) {
      logger.msg(INFO, "Failed adapting job description to target resources");
      return false;
    }

    std::string sJobid;
    if (!ac.submit(modjobdesc.UnParse("ARCJSDL"), sJobid, et.url.Protocol() == "https"))
      return false;

    if (sJobid.empty()) {
      logger.msg(INFO, "No job identifier returned by A-REX");
      return false;
    }

    XMLNode jobidx(sJobid);
    URL jobid((std::string)(jobidx["ReferenceParameters"]["JobSessionDir"]));

    if (!PutFiles(modjobdesc, jobid)) {
      logger.msg(INFO, "Failed uploading local input files");
      return false;
    }

    AddJobDetails(modjobdesc, jobid, et.Cluster, jobid, job);

    return true;
  }

  bool SubmitterARC1::Migrate(const URL& jobid, const JobDescription& jobdesc,
                             const ExecutionTarget& et,
                             bool forcemigration, Job& job) const {
    MCCConfig cfg;
    usercfg.ApplyToConfig(cfg);
    AREXClient ac(et.url, cfg, usercfg.Timeout());

    std::string idstr;
    AREXClient::createActivityIdentifier(jobid, idstr);

    JobDescription modjobdesc(jobdesc);

    // Modify the location of local files and files residing in a old session directory.
    for (std::list<FileType>::iterator it = modjobdesc.DataStaging.File.begin();
         it != modjobdesc.DataStaging.File.end(); it++) {
      // Do not modify Output and Error files.
      if (it->Name == modjobdesc.Application.Output ||
          it->Name == modjobdesc.Application.Error ||
          it->Source.empty())
        continue;

      if (!it->Source.front().URI || it->Source.front().URI.Protocol() == "file") {
        it->Source.front().URI = URL(jobid.str() + "/" + it->Name);
        it->DownloadToCache = false;
      }
      else {
        // URL is valid, and not a local file. Check if the source reside at a
        // old job session directory.
        const size_t foundRSlash = it->Source.front().URI.str().rfind('/');
        if (foundRSlash == std::string::npos)
          continue;

        const std::string uriPath = it->Source.front().URI.str().substr(0, foundRSlash);
        // Check if the input file URI is pointing to a old job session directory.
        for (std::list<std::string>::const_iterator itAOID = modjobdesc.Identification.ActivityOldId.begin();
             itAOID != modjobdesc.Identification.ActivityOldId.end(); itAOID++)
          if (uriPath == *itAOID) {
            it->Source.front().URI = URL(jobid.str() + "/" + it->Name);
            it->DownloadToCache = false;
            break;
          }
      }
    }

    if (!ModifyJobDescription(modjobdesc, et)) {
      logger.msg(INFO, "Failed adapting job description to target resources");
      return false;
    }

    // Add ActivityOldId.
    modjobdesc.Identification.ActivityOldId.push_back(jobid.str());

    std::string sNewjobid;
    if (!ac.migrate(idstr, modjobdesc.UnParse("ARCJSDL"), forcemigration, sNewjobid,
                    et.url.Protocol() == "https"))
      return false;

    if (sNewjobid.empty()) {
      logger.msg(INFO, "No job identifier returned by A-REX");
      return false;
    }

    XMLNode xNewjobid(sNewjobid);
    URL newjobid((std::string)(xNewjobid["ReferenceParameters"]["JobSessionDir"]));

    if (!PutFiles(modjobdesc, newjobid)) {
      logger.msg(INFO, "Failed uploading local input files");
      return false;
    }

    AddJobDetails(modjobdesc, newjobid, et.Cluster, newjobid, job);

    return true;
  }

  bool SubmitterARC1::ModifyJobDescription(JobDescription& jobdesc, const ExecutionTarget& et) const {
    // Check for identical file names.
    bool executableIsAdded(false), inputIsAdded(false), outputIsAdded(false), errorIsAdded(false), logDirIsAdded(false);
    for (std::list<FileType>::const_iterator it1 = jobdesc.DataStaging.File.begin();
         it1 != jobdesc.DataStaging.File.end(); it1++) {
      for (std::list<FileType>::const_iterator it2 = it1;
           it2 != jobdesc.DataStaging.File.end(); it2++) {
        if (it1 == it2) continue;

        if (it1->Name == it2->Name && ((!it1->Source.empty() && !it2->Source.empty()) ||
                                       (!it1->Target.empty() && !it2->Target.empty()))) {
          logger.msg(VERBOSE, "Two files have identical file name '%s'.", it1->Name);
          return false;
        }

      }

      executableIsAdded  |= (it1->Name == jobdesc.Application.Executable.Name);
      inputIsAdded       |= (it1->Name == jobdesc.Application.Input);
      outputIsAdded      |= (it1->Name == jobdesc.Application.Output);
      errorIsAdded       |= (it1->Name == jobdesc.Application.Error);
      logDirIsAdded      |= (it1->Name == jobdesc.Application.LogDir);
    }

    if (!executableIsAdded &&
        !Glib::path_is_absolute(jobdesc.Application.Executable.Name)) {
      FileType file;
      file.Name = jobdesc.Application.Executable.Name;
      DataSourceType s;
      s.URI = file.Name;
      file.Source.push_back(s);
      file.KeepData = false;
      file.IsExecutable = true;
      file.DownloadToCache = false;
      jobdesc.DataStaging.File.push_back(file);
    }

    if (!jobdesc.Application.Input.empty() && !inputIsAdded) {
      FileType file;
      file.Name = jobdesc.Application.Input;
      DataSourceType s;
      s.URI = file.Name;
      file.Source.push_back(s);
      file.KeepData = false;
      file.IsExecutable = false;
      file.DownloadToCache = false;
      jobdesc.DataStaging.File.push_back(file);
    }

    if (!jobdesc.Application.Output.empty() && !outputIsAdded) {
      FileType file;
      file.Name = jobdesc.Application.Output;
      file.KeepData = true;
      file.IsExecutable = false;
      file.DownloadToCache = false;
      jobdesc.DataStaging.File.push_back(file);
    }

    if (!jobdesc.Application.Error.empty() && !errorIsAdded) {
      FileType file;
      file.Name = jobdesc.Application.Error;
      file.KeepData = true;
      file.IsExecutable = false;
      file.DownloadToCache = false;
      jobdesc.DataStaging.File.push_back(file);
    }

    if (!jobdesc.Application.LogDir.empty() && !logDirIsAdded) {
      FileType file;
      file.Name = jobdesc.Application.LogDir;
      file.KeepData = true;
      file.IsExecutable = false;
      file.DownloadToCache = false;
      jobdesc.DataStaging.File.push_back(file);
    }

    if (!jobdesc.Resources.RunTimeEnvironment.empty() &&
        !jobdesc.Resources.RunTimeEnvironment.selectSoftware(et.ApplicationEnvironments)) {
      // This error should never happen since RTE is checked in the Broker.
      logger.msg(VERBOSE, "Unable to select run time environment");
      return false;
    }

    if (!jobdesc.Resources.CEType.empty() &&
        !jobdesc.Resources.CEType.selectSoftware(et.Implementation)) {
      // This error should never happen since Middleware is checked in the Broker.
      logger.msg(VERBOSE, "Unable to select middleware");
      return false;
    }

    if (!jobdesc.Resources.OperatingSystem.empty() &&
        !jobdesc.Resources.OperatingSystem.selectSoftware(et.Implementation)) {
      // This error should never happen since OS is checked in the Broker.
      logger.msg(VERBOSE, "Unable to select operating system.");
      return false;
    }

    // Set endpoint and queue name to the selected ExecutionTarget
    jobdesc.Resources.CandidateTarget.clear();
    ResourceTargetType candidateTarget;
    candidateTarget.EndPointURL = et.Cluster;
    candidateTarget.QueueName = et.ComputingShareName;
    candidateTarget.UseQueue = true;
    jobdesc.Resources.CandidateTarget.push_back(candidateTarget);

    return true;
  }

} // namespace Arc
