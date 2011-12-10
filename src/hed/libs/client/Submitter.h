// -*- indent-tabs-mode: nil -*-

#ifndef __ARC_SUBMITTER_H__
#define __ARC_SUBMITTER_H__

#include <list>
#include <map>
#include <string>

#include <arc/URL.h>
#include <arc/loader/Loader.h>
#include <arc/loader/Plugin.h>
#include <arc/client/JobDescription.h>

namespace Arc {

  class Config;
  class ExecutionTarget;
  class Job;
  class JobDescription;
  class Logger;
  class UserConfig;

  //! Base class for the Submitters
  /**
   * Submitter is the base class for Grid middleware specialized
   * Submitter objects. The class submits job(s) to the computing
   * resource it represents and uploads (needed by the job) local
   * input files.
   */
  class Submitter
    : public Plugin {
  protected:
    Submitter(const UserConfig& usercfg,
              const std::string& flavour);
  public:
    virtual ~Submitter();

    /**
    * This virtual method can be ovverriden by plugins which should
    * be capable of getting test job descriptions for the specified flavour.
    * This method should return with the JobDescription or NULL if ther is no
    * test description defined with the requested id.
    */
    bool GetTestJob(const int& testid, JobDescription& jobdescription);

    /// Submit job
    /**
     * This virtual method should be overridden by plugins which should
     * be capable of submitting jobs, defined in the JobDescription
     * jobdesc, to the ExecutionTarget et. The protected convenience
     * method AddJob can be used to save job information.
     * This method should return the URL of the submitted job. In case
     * submission fails an empty URL should be returned.
     */
    virtual bool Submit(const JobDescription& jobdesc,
                        const ExecutionTarget& et, Job& job) = 0;

    bool Submit(const JobDescription& jobdesc, Job& job) {
      return target != NULL && Submit(jobdesc, *target, job);
    }

    /// Migrate job
    /**
     * This virtual method should be overridden by plugins which should
     * be capable of migrating jobs. The active job which should be
     * migrated is pointed to by the URL jobid, and is represented by
     * the JobDescription jobdesc. The forcemigration boolean specifies
     * if the migration should succeed if the active job cannot be
     * terminated. The protected method AddJob can be used to save job
     * information.
     * This method should return the URL of the migrated job. In case
     * migration fails an empty URL should be returned.
     */
    virtual bool Migrate(const URL& jobid, const JobDescription& jobdesc,
                         const ExecutionTarget& et,
                         bool forcemigration, Job& job) = 0;

    bool Migrate(const URL& jobid, const JobDescription& jobdesc,
                 bool forcemigration, Job& job) {
      return target != NULL && Migrate(jobid, jobdesc, *target, forcemigration, job);
    }

    virtual bool ModifyJobDescription(JobDescription& jobdesc,
                                      const ExecutionTarget& et) const = 0;
    void SetSubmissionTarget(const ExecutionTarget& submissiontarget) { target = &submissiontarget; }
  protected:
    bool PutFiles(const JobDescription& jobdesc, const URL& url) const;
    void AddJobDetails(const JobDescription& jobdesc, const URL& jobid,
                       const URL& cluster, const URL& infoendpoint,
                       Job& job) const;

    const std::string flavour;
    const UserConfig& usercfg;

    /// Target to submit to.
    const ExecutionTarget* target;

    static Logger logger;
  };

  //! Class responsible for loading Submitter plugins
  /// The Submitter objects returned by a SubmitterLoader
  /// must not be used after the SubmitterLoader goes out of scope.
  class SubmitterLoader
    : public Loader {

  public:
    //! Constructor
    /// Creates a new SubmitterLoader.
    SubmitterLoader();

    //! Destructor
    /// Calling the destructor destroys all Submitters loaded
    /// by the SubmitterLoader instance.
    ~SubmitterLoader();

    //! Load a new Submitter
    /// \param name    The name of the Submitter to load.
    /// \param usercfg The UserConfig object for the new Submitter.
    /// \returns       A pointer to the new Submitter (NULL on error).
    Submitter* load(const std::string& name, const UserConfig& usercfg);

    //! Retrieve the list of loaded Submitters.
    /// \returns A reference to the list of Submitters.
    const std::list<Submitter*>& GetSubmitters() const {
      return submitters;
    }

  private:
    std::list<Submitter*> submitters;
  };

  class SubmitterPluginArgument
    : public PluginArgument {
  public:
    SubmitterPluginArgument(const UserConfig& usercfg)
      : usercfg(usercfg) {}
    ~SubmitterPluginArgument() {}
    operator const UserConfig&() {
      return usercfg;
    }
  private:
    const UserConfig& usercfg;
  };

} // namespace Arc

#endif // __ARC_SUBMITTER_H__
