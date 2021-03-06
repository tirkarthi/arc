#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <algorithm>

#include <glibmm/fileutils.h>

#include <arc/message/MCC.h>

namespace Arc {

  Logger MCC::logger(Logger::getRootLogger(), "MCC");

  MCCInterface::MCCInterface(PluginArgument* arg):Plugin(arg) {
  }

  MCCInterface::~MCCInterface() {
  }

  MCC::~MCC() {
  }

  MCC::MCC(Config *, PluginArgument* arg):MCCInterface(arg) {
  }

  void MCC::Next(MCCInterface *next, const std::string& label) {
    Glib::Mutex::Lock lock(next_lock_);
    if (next == NULL) next_.erase(label);
    else next_[label] = next;
  }

  MCCInterface *MCC::Next(const std::string& label) {
    Glib::Mutex::Lock lock(next_lock_);
    std::map<std::string, MCCInterface *>::iterator n = next_.find(label);
    if (n == next_.end()) return NULL;
    return n->second;
  }

  void MCC::Unlink() {
    Glib::Mutex::Lock lock(next_lock_);
    next_.clear();
  }

  void MCC::AddSecHandler(Config *cfg, ArcSec::SecHandler *sechandler,
        const std::string& label) {
    if (sechandler) {
      sechandlers_[label].push_back(sechandler);
      // need polishing to put the SecHandlerFactory->getinstance here
      XMLNode cn = (*cfg)["SecHandler"];
      Config cfg_(cn);
    }
  }

  MCC_Status MCC::ProcessSecHandlers(Message& message,
             const std::string& label) const {
    // Each MCC/Service can define security handler queues in the configuration
    // file, the queues have labels specified in handlers configuration 'event'
    // attribute.
    // Security handlers in one queue are called sequentially.
    // Each one should be configured carefully, because there can be some
    // relationship between them (e.g. authentication should be put in front
    // of authorization).
    // The SecHandler::Handle() only returns true/false with true meaning that
    // handler processed message successfuly. If SecHandler implements
    // authorization functionality, it returns false if message is disallowed
    // and true otherwise.
    // If any SecHandler in the handler chain produces some information which
    // will be used by some following handler, the information should be
    // stored in the attributes of message (e.g. the Identity extracted from
    // authentication will be used by authorization to make access control
    // decision).
    std::map<std::string, std::list<ArcSec::SecHandler *> >::const_iterator q =
      sechandlers_.find(label);
    if (q == sechandlers_.end()) {
      logger.msg(DEBUG, "No security processing/check requested for '%s'", label);
      return MCC_Status(STATUS_OK);
    }
    for (std::list<ArcSec::SecHandler*>::const_iterator h = q->second.begin();
         h != q->second.end(); ++h) {
      const ArcSec::SecHandler *handler = *h;
      if (!handler) continue; // Shouldn't happen. Just a sanity check.
      ArcSec::SecHandlerStatus ret = handler->Handle(&message);
      if (!ret) {
        logger.msg(INFO, "Security processing/check failed: %s", (std::string)ret);
        return MCC_Status(GENERIC_ERROR, ret.getOrigin(),
                           ret.getExplanation().empty()?(std::string("Security error: ")+Arc::tostring(ret.getCode())):ret.getExplanation());
      }
    }
    logger.msg(DEBUG, "Security processing/check passed");
    return MCC_Status(STATUS_OK);
  }

  XMLNode MCCConfig::MakeConfig(XMLNode cfg) const {
    XMLNode mm = BaseConfig::MakeConfig(cfg);
    std::list<std::string> mccs;
    for (std::list<std::string>::const_iterator path = plugin_paths.begin();
         path != plugin_paths.end(); path++) {
      try {
        Glib::Dir dir(*path);
        for (Glib::DirIterator file = dir.begin(); file != dir.end(); file++) {
          if ((*file).substr(0, 6) == "libmcc") {
            std::string name = (*file).substr(6, (*file).find('.') - 6);
            if (std::find(mccs.begin(), mccs.end(), name) == mccs.end()) {
              mccs.push_back(name);
              cfg.NewChild("Plugins").NewChild("Name") = "mcc" + name;
            }
          }
          //Since the security handler could also be used by mcc like
          // tls and soap, putting the libarcshc here. Here we suppose
          // all of the sec handlers are put in libarcshc
          // TODO: Rewrite it to behave in generic way.
          //if ((*file).substr(0, 9) == "libarcshc") {
          //  cfg.NewChild("Plugins").NewChild("Name") = "arcshc";
          //}
        }
      }
      catch (Glib::FileError&) {}
    }
    return mm;
  }

/*
  SecHandlerConfig::SecHandlerConfig(XMLNode cfg) {
    cfg.New(cfg_);
    NS ns("cfg","http://www.nordugrid.org/schemas/ArcConfig/2007");
    cfg_.Namespaces(ns);
    cfg_.Name("cfg:SecHandler");
  }

  XMLNode SecHandlerConfig::MakeConfig(XMLNode cfg) const {
    cfg.NewChild(cfg_);
    return cfg;
  }
*/

} // namespace Arc
