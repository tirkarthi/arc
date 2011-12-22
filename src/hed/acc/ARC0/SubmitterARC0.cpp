// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <unistd.h>
#include <sys/stat.h>

#include <glibmm.h>

#include <arc/CheckSum.h>
#include <arc/Logger.h>
#include <arc/UserConfig.h>
#include <arc/StringConv.h>
#include <arc/client/ExecutionTarget.h>
#include <arc/client/Job.h>
#include <arc/client/JobDescription.h>

#include "SubmitterARC0.h"
#include "FTPControl.h"

namespace Arc {

  // Characters to be escaped in LDAP filter according to RFC4515
  static const std::string filter_esc("&|=!><~*/()");

  Logger SubmitterARC0::logger(Logger::getRootLogger(), "Submitter.ARC0");

  SubmitterARC0::SubmitterARC0(const UserConfig& usercfg)
    : Submitter(usercfg, "ARC0") {}

  SubmitterARC0::~SubmitterARC0() {}

  Plugin* SubmitterARC0::Instance(PluginArgument *arg) {
    SubmitterPluginArgument *subarg =
      dynamic_cast<SubmitterPluginArgument*>(arg);
    if (!subarg)
      return NULL;
    Glib::Module* module = subarg->get_module();
    PluginsFactory* factory = subarg->get_factory();
    if(!(factory && module)) {
      logger.msg(ERROR, "Missing reference to factory and/or module. It is unsafe to use Globus in non-persistent mode - Submitter for ARC0 is disabled. Report to developers.");
      return NULL;
    }
    factory->makePersistent(module);
    return new SubmitterARC0(*subarg);
  }


  bool SubmitterARC0::Submit(const JobDescription& jobdesc, const ExecutionTarget& et, Job& job) {

    FTPControl ctrl;

    if (!ctrl.Connect(et.url,
                      usercfg.ProxyPath(), usercfg.CertificatePath(),
                      usercfg.KeyPath(), usercfg.Timeout())) {
      logger.msg(INFO, "Submit: Failed to connect");
      return false;
    }

    if (!ctrl.SendCommand("CWD " + et.url.Path(), usercfg.Timeout())) {
      logger.msg(INFO, "Submit: Failed sending CWD command");
      ctrl.Disconnect(usercfg.Timeout());
      return false;
    }

    std::string response;

    if (!ctrl.SendCommand("CWD new", response, usercfg.Timeout())) {
      logger.msg(INFO, "Submit: Failed sending CWD new command");
      ctrl.Disconnect(usercfg.Timeout());
      return false;
    }

    std::string::size_type pos2 = response.rfind('"');
    std::string::size_type pos1 = response.rfind('/', pos2 - 1);
    std::string jobnumber = response.substr(pos1 + 1, pos2 - pos1 - 1);

    JobDescription preparedjobdesc(jobdesc);

    if (preparedjobdesc.OtherAttributes["nordugrid:xrsl;clientxrsl"].empty())
      preparedjobdesc.UnParse(preparedjobdesc.OtherAttributes["nordugrid:xrsl;clientxrsl"], "nordugrid:xrsl");

    preparedjobdesc.OtherAttributes["nordugrid:xrsl;action"] = "request";
    preparedjobdesc.OtherAttributes["nordugrid:xrsl;savestate"] = "yes";
    preparedjobdesc.OtherAttributes["nordugrid:xrsl;clientsoftware"] = "libarcclient-" VERSION;
#ifdef HAVE_GETHOSTNAME
    char hostname[1024];
    gethostname(hostname, 1024);
    preparedjobdesc.OtherAttributes["nordugrid:xrsl;hostname"] = hostname;
#endif

    if (!preparedjobdesc.Prepare(et)) {
      logger.msg(INFO, "Failed to prepare job description to target resources.");
      ctrl.Disconnect(usercfg.Timeout());
      return false;
    }

    std::string jobdescstring;
    if (!preparedjobdesc.UnParse(jobdescstring, "nordugrid:xrsl", "GRIDMANAGER")) {
      logger.msg(INFO, "Unable to submit job. Job description is not valid in the %s format", "nordugrid:xrsl");
      return false;
    }

    if (!ctrl.SendData(jobdescstring, "job", usercfg.Timeout())) {
      logger.msg(INFO, "Submit: Failed sending job description");
      ctrl.Disconnect(usercfg.Timeout());
      return false;
    }

    if (!ctrl.Disconnect(usercfg.Timeout())) {
      logger.msg(INFO, "Submit: Failed to disconnect after submission");
      return false;
    }

    URL jobid(et.url);
    jobid.ChangePath(jobid.Path() + '/' + jobnumber);

    if (!PutFiles(preparedjobdesc, jobid)) {
      logger.msg(INFO, "Submit: Failed uploading local input files");
      return false;
    }

    // Prepare contact url for information about this job
    URL infoendpoint(et.Cluster);
    infoendpoint.ChangeLDAPFilter("(nordugrid-job-globalid=" + escape_chars(jobid.str(),filter_esc,'\\',false,escape_hex) + ")");
    infoendpoint.ChangeLDAPScope(URL::subtree);

    AddJobDetails(preparedjobdesc, jobid, et.Cluster, infoendpoint, job);

    return true;
  }

  bool SubmitterARC0::Migrate(const URL& /* jobid */, const JobDescription& /* jobdesc */,
                             const ExecutionTarget& et, bool /* forcemigration */,
                             Job& /* job */) {
    logger.msg(INFO, "Trying to migrate to %s: Migration to a legacy ARC resource is not supported.", et.url.str());
    return false;
  }

} // namespace Arc
