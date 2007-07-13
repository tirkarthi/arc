#ifndef __ARC_PDP_H__
#define __ARC_PDP_H__

#include <string>

namespace Arc {

  class Config;
  class Logger;

  class PDP {
   public:
    PDP(Config* cfg) {};
    virtual ~PDP() {};
    virtual bool isPermitted(std::string subject) = 0;
   protected:
    static Logger logger;
  };

} // namespace Arc

#endif /* __ARC_PDP_H__ */
