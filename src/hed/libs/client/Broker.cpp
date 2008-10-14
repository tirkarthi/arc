#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <list>

#include <arc/client/Broker.h>

namespace Arc {
    
    Arc::Logger logger(Arc::Logger::getRootLogger(), "broker");
    Arc::LogStream logcerr(std::cerr);
    //Arc::Logger::getRootLogger().addDestination(logcerr);
    //Arc::Logger::getRootLogger().setThreshold(Arc::WARNING);
 	    
    Broker::Broker(Arc::TargetGenerator& targen,  Arc::JobDescription jd) {
	//for get_Targets test
              ExecutionTarget eTarget;
              eTarget.MaxCPUTime = 1; 
              eTarget.RunningJobs = 1; 
              eTarget.MaxRunningJobs = 1; 
              eTarget.WaitingJobs = 3;
              eTarget.MaxDiskSpace = 10;
              found_Targets.push_back(eTarget);

              eTarget.MinCPUTime = 1; 
              eTarget.MaxRunningJobs = 22;
              eTarget.WaitingJobs = 1; 
              found_Targets.push_back(eTarget);

              ExecutionTarget eTarget2;
              eTarget2.DefaultCPUTime = 1; 
              eTarget2.RunningJobs = 3; 
              eTarget2.MaxRunningJobs = 31;
              eTarget2.WaitingJobs = 20; 
              found_Targets.push_back(eTarget2);
	//end of "for get_Targets test"
 		      
              targen.GetTargets(0, 1);

              Arc::XMLNode jobd;
              bool empty_Jd = false;
              try {
              		jobd = jd.getXML();
  	        	logger.msg(Arc::DEBUG, "Jobdescription: " + jobd );
              } 
              catch (Arc::JobDescriptionError e) {
		        logger.msg(Arc::DEBUG, "The added JobDescription is empty!");
		        empty_Jd = true;
		        std::cout << "The added JobDescription is empty!" << std::endl;
              }

              if ( targen.FoundTargets().empty() ) {
			logger.msg(Arc::ERROR, "No Targets found!");
 	    		throw "No Targets found!";
              }

              for (std::list<Arc::ExecutionTarget>::const_iterator target = \
				   targen.FoundTargets().begin(); target != targen.FoundTargets().end(); \
				   target++) {  

            	 	// Candidate Set Generation
			if ( empty_Jd ) {
				found_Targets.push_back(*target);
				continue;
			}

			//This argoments are dependence from the Resource item.
			if ( jobd["JobDescription"]["Resources"] ) {

				if ( (*target).MaxDiskSpace != -1 ) {
					if ( (*target).MaxDiskSpace < jobd["JobDescription"]["Resources"]["DiskSpace"] || 
					     (*target).MaxDiskSpace < jobd["JobDescription"]["Resources"]["IndividualDiskSpace"] || 
					     (*target).MaxDiskSpace < jobd["JobDescription"]["Resources"]["TotalDiskSpace"])
						continue;
				}

				if ( (*target).TotalPhysicalCPUs != -1 ) {
					if ( (*target).TotalPhysicalCPUs < jobd["JobDescription"]["Resources"]["IndividualCPUCount"] ||
					     (*target).TotalPhysicalCPUs < jobd["JobDescription"]["Resources"]["TotalCPUCount"])
						continue;
				}

				if ( (*target).TotalLogicalCPUs != -1 ) {
					if ( (*target).TotalLogicalCPUs < jobd["JobDescription"]["Resources"]["IndividualCPUCount"] ||
					     (*target).TotalLogicalCPUs < jobd["JobDescription"]["Resources"]["TotalCPUCount"])
						continue;
				}

				if ( (*target).CPUClockSpeed != -1 ) {
					if ( (*target).CPUClockSpeed < jobd["JobDescription"]["Resources"]["IndividualCPUSpeed"])
						continue;
				}
			}

			//This argoments are dependence from the Application item.
			if ( jobd["JobDescription"]["Application"] ) {

				if ( (int)(*target).MinWallTime.GetPeriod() != 0 ) {
					if ( (int)(*target).MinWallTime.GetPeriod() > jobd["JobDescription"]["Application"]["POSIXApplication"]["WallTimeLimit"])
						continue;
				}

				if ( (int)(*target).MaxWallTime.GetPeriod() != 0 ) {
					if ( (int)(*target).MaxWallTime.GetPeriod() < jobd["JobDescription"]["Application"]["POSIXApplication"]["WallTimeLimit"])
						continue;
				}

				if ( (int)(*target).MaxTotalWallTime.GetPeriod() != 0 ) {
					if ( (int)(*target).MaxTotalWallTime.GetPeriod() < jobd["JobDescription"]["Application"]["POSIXApplication"]["WallTimeLimit"])
						continue;
				}

				if ( (int)(*target).DefaultWallTime.GetPeriod() != 0 ) {
					if ( (int)(*target).DefaultWallTime.GetPeriod() < jobd["JobDescription"]["Application"]["POSIXApplication"]["WallTimeLimit"])
						continue;
				}

			}

			//This argoments are dependence from the Application and the Resource items.
			if ( (int)(*target).MinCPUTime.GetPeriod() != 0 ) {
				if ( (int)(*target).MinCPUTime.GetPeriod() > jobd["JobDescription"]["Resources"]["IndividualCPUTime"] ||
				     (int)(*target).MinCPUTime.GetPeriod() > jobd["JobDescription"]["Resources"]["TotalCPUTime"] || 
				     (int)(*target).MinCPUTime.GetPeriod() > jobd["JobDescription"]["Application"]["POSIXApplication"]["CPUTimeLimit"])
					continue;
			}

			if ( (int)(*target).MaxCPUTime.GetPeriod() != 0 ) {
				if ( (int)(*target).MaxCPUTime.GetPeriod() < jobd["JobDescription"]["Resources"]["IndividualCPUTime"] ||
				     (int)(*target).MaxCPUTime.GetPeriod() < jobd["JobDescription"]["Resources"]["TotalCPUTime"] ||
				     (int)(*target).MaxCPUTime.GetPeriod() < jobd["JobDescription"]["Application"]["POSIXApplication"]["CPUTimeLimit"])
					continue;
			}

			if ( (int)(*target).MaxTotalCPUTime.GetPeriod() != 0 ) {
				if ( (int)(*target).MaxTotalCPUTime.GetPeriod() < jobd["JobDescription"]["Resources"]["IndividualCPUTime"] ||
				     (int)(*target).MaxTotalCPUTime.GetPeriod() < jobd["JobDescription"]["Resources"]["TotalCPUTime"] ||
				     (int)(*target).MaxTotalCPUTime.GetPeriod() < jobd["JobDescription"]["Application"]["POSIXApplication"]["CPUTimeLimit"])
					continue;
			}

			if ( (int)(*target).DefaultCPUTime.GetPeriod() != 0 ) {
				if ( (int)(*target).DefaultCPUTime.GetPeriod() < jobd["JobDescription"]["Resources"]["IndividualCPUTime"] ||
				     (int)(*target).DefaultCPUTime.GetPeriod() < jobd["JobDescription"]["Resources"]["TotalCPUTime"] ||
				     (int)(*target).DefaultCPUTime.GetPeriod() < jobd["JobDescription"]["Application"]["POSIXApplication"]["CPUTimeLimit"])
					continue;
			}

			if ( (*target).MaxMemory != -1 ) {
				if ((*target).MaxMemory < jobd["JobDescription"]["Resources"]["IndividualPhysicalMemory"] ||
				    (*target).MaxMemory < jobd["JobDescription"]["Resources"]["TotalPhysicalMemory"] ||
				    (*target).MaxMemory < jobd["JobDescription"]["Application"]["POSIXApplication"]["MemoryLimit"])
					continue;
			}

			if ( (*target).NodeMemory != -1 ) {
				if ((*target).NodeMemory < jobd["JobDescription"]["Resources"]["IndividualPhysicalMemory"] ||
				    (*target).NodeMemory < jobd["JobDescription"]["Resources"]["TotalPhysicalMemory"] ||
				    (*target).NodeMemory < jobd["JobDescription"]["Application"]["POSIXApplication"]["MemoryLimit"])
					continue;
			}

			if ( (*target).MainMemorySize != -1 ) {
				if ((*target).MainMemorySize < jobd["JobDescription"]["Resources"]["IndividualPhysicalMemory"] ||
				    (*target).MainMemorySize < jobd["JobDescription"]["Resources"]["TotalPhysicalMemory"] ||
				    (*target).MainMemorySize < jobd["JobDescription"]["Application"]["POSIXApplication"]["MemoryLimit"])
					continue;
			}
		// End of the Candidate Set Generation	     

	                found_Targets.push_back(*target);	     
              }	
		 

             current = found_Targets.begin();    
   }

   Broker::~Broker() {}
    
   ExecutionTarget& Broker::get_Target() {
        // If first time call this method, then must be sort the Targets list.

        if (current == found_Targets.begin()) {
        	sort_Targets();
        }       
       
        std::vector<Arc::ExecutionTarget>::iterator ret_pointer;
        ret_pointer = current;
        current++;  

        if (ret_pointer == found_Targets.end()) {
        	throw "No more ExecutionTarget!";  
        }

        return *ret_pointer;
  }

} // namespace Arc



