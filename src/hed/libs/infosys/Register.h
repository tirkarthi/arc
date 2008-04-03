#ifndef __ARC_INFOSYS_REGISTER_H__
#define __ARC_INFOSYS_REGISTER_H__

#include <list>
#include <string>
#include <arc/ArcConfig.h>
#include <arc/XMLNode.h>
#include <arc/misc/ClientInterface.h>
#include <arc/Logger.h>

namespace Arc {

/// Registration to ISIS interface
/** This class provides an interface for service to register
  itself in Information Indexing Service. */
class Register
{
    private:
        std::list<std::string> urls;
        Arc::NS ns;
        Arc::MCCConfig mcc_cfg;
        Arc::ClientSOAP *cli;
        Arc::Logger logger;
        std::string service_id;
        long int reg_period;
    public:
        /** Constructor.
          It takes service identifier @sid, registration frequency @reg_period 
          in seconds and configuration XML subtree @cfg. */
        Register(const std::string &sid, long int reg_period, Arc::Config &cfg);
        ~Register(void);
        /** Adds @url of ISIS service.
          Specified URLs will all be used during registration process. */
        void AddUrl(std::string &url);
        /** Perform registration.
          All specified ISIS services are contacted and service specified in 
          constructor is registred. */
        void registration(void);
        /** Perform registration process in loop.
          This method calls registration() in loop every reg_period seconds.
          Never returns so should be run in a separate thread. */
        void registration_forever(void);

}; // class Register

} // namespace

#endif
