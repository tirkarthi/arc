// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>
#include <list>
#include <string>
#include <algorithm>

#include <arc/ArcLocation.h>
#include <arc/IString.h>
#include <arc/Logger.h>
#include <arc/OptionParser.h>
#include <arc/StringConv.h>
#include <arc/client/JobController.h>
#include <arc/client/JobSupervisor.h>
#include <arc/UserConfig.h>

int main(int argc, char **argv) {

  setlocale(LC_ALL, "");

  Arc::Logger logger(Arc::Logger::getRootLogger(), "arcstat");
  Arc::LogStream logcerr(std::cerr);
  logcerr.setFormat(Arc::ShortFormat);
  Arc::Logger::getRootLogger().addDestination(logcerr);
  Arc::Logger::getRootLogger().setThreshold(Arc::WARNING);

  Arc::ArcLocation::Init(argv[0]);

  Arc::OptionParser options(istring("[job ...]"),
                            istring("The arcstat command is used for "
                                    "obtaining the status of jobs that have\n"
                                    "been submitted to grid enabled resources."),
                            istring("Argument to -c has the format "
                                    "Flavour:URL e.g.\n"
                                    "ARC0:ldap://grid.tsl.uu.se:2135/"
                                    "nordugrid-cluster-name=grid.tsl.uu.se,"
                                    "Mds-Vo-name=local,o=grid"));

  bool all = false;
  options.AddOption('a', "all",
                    istring("all jobs"),
                    all);

  std::string joblist;
  options.AddOption('j', "joblist",
                    istring("file containing a list of jobs"),
                    istring("filename"),
                    joblist);

  std::list<std::string> clusters;
  options.AddOption('c', "cluster",
                    istring("explicitly select or reject a specific cluster"),
                    istring("[-]name"),
                    clusters);

  std::list<std::string> status;
  options.AddOption('s', "status",
                    istring("only select jobs whose status is statusstr"),
                    istring("statusstr"),
                    status);

  bool longlist = false;
  options.AddOption('l', "long",
                    istring("long format (more information)"),
                    longlist);

  typedef bool (*JobSorting)(const Arc::Job*, const Arc::Job*);
  std::string sort = "", rsort = "";
  std::map<std::string, JobSorting> orderings;
  orderings["jobid"] = &Arc::Job::CompareJobID;
  orderings["submissiontime"] = &Arc::Job::CompareSubmissionTime;
  orderings["jobname"] = &Arc::Job::CompareJobName;
  options.AddOption('S', "sort",
                    istring("sort jobs according to jobid, submissiontime or jobname"),
                    istring("order"), sort);
  options.AddOption('R', "rsort",
                    istring("reverse sorting of jobs according to jobid, submissiontime or jobname"),
                    istring("order"), rsort);

  int timeout = -1;
  options.AddOption('t', "timeout", istring("timeout in seconds (default 20)"),
                    istring("seconds"), timeout);

  std::string conffile;
  options.AddOption('z', "conffile",
                    istring("configuration file (default ~/.arc/client.conf)"),
                    istring("filename"), conffile);

  std::string debug;
  options.AddOption('d', "debug",
                    istring("FATAL, ERROR, WARNING, INFO, VERBOSE or DEBUG"),
                    istring("debuglevel"), debug);

  bool version = false;
  options.AddOption('v', "version", istring("print version information"),
                    version);

  std::list<std::string> jobs = options.Parse(argc, argv);

  // If debug is specified as argument, it should be set before loading the configuration.
  if (!debug.empty())
    Arc::Logger::getRootLogger().setThreshold(Arc::string_to_level(debug));

  Arc::UserConfig usercfg(conffile, joblist);
  if (!usercfg) {
    logger.msg(Arc::ERROR, "Failed configuration initialization");
    return 1;
  }

  if (debug.empty() && !usercfg.Verbosity().empty())
    Arc::Logger::getRootLogger().setThreshold(Arc::string_to_level(usercfg.Verbosity()));

  if (timeout > 0)
    usercfg.Timeout(timeout);


  if (version) {
    std::cout << Arc::IString("%s version %s", "arcstat", VERSION)
              << std::endl;
    return 0;
  }

  if (!sort.empty() && !rsort.empty()) {
    logger.msg(Arc::ERROR, "The 'sort' and 'rsort' flags cannot be specified at the same time.");
    return 1;
  }

  if (!rsort.empty()) {
    sort = rsort;
  }

  if (!sort.empty() && orderings.find(sort) == orderings.end()) {
    std::cerr << "Jobs cannot be sorted by \"" << sort << "\", the following orderings are supported:" << std::endl;
    for (std::map<std::string, JobSorting>::const_iterator it = orderings.begin();
         it != orderings.end(); it++)
      std::cerr << it->first << std::endl;
    return 1;
  }

  if ((!joblist.empty() || !status.empty()) && jobs.empty() && clusters.empty())
    all = true;

  if (jobs.empty() && clusters.empty() && !all) {
    logger.msg(Arc::ERROR, "No jobs given");
    return 1;
  }

  if (!jobs.empty() || all)
    usercfg.ClearSelectedServices();

  if (!clusters.empty()) {
    usercfg.ClearSelectedServices();
    usercfg.AddServices(clusters, Arc::COMPUTING);
  }

  Arc::JobSupervisor jobmaster(usercfg, jobs);
  if (!jobmaster.JobsFound()) {
    std::cout << "No jobs" << std::endl;
    return 0;
  }
  std::list<Arc::JobController*> jobcont = jobmaster.GetJobControllers();

  if (jobcont.empty()) {
    logger.msg(Arc::ERROR, "No job controller plugins loaded");
    return 1;
  }

  int retval = 0;
  if (!sort.empty()) {
    std::vector<const Arc::Job*> jobs;
    for (std::list<Arc::JobController*>::iterator it = jobcont.begin();
         it != jobcont.end(); it++)
      (*it)->FetchJobs(status, jobs);

    rsort.empty() ? std::sort(jobs.begin(),  jobs.end(),  orderings[sort]) :
                    std::sort(jobs.rbegin(), jobs.rend(), orderings[sort]);

    for (std::vector<const Arc::Job*>::const_iterator it = jobs.begin();
         it != jobs.end(); it++)
      (*it)->Print(longlist);
  }
  else {
    for (std::list<Arc::JobController*>::iterator it = jobcont.begin();
         it != jobcont.end(); it++) {
      if (!(*it)->PrintJobStatus(status, longlist))
        retval = 1;
    }
  }

  return retval;
}
