#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <signal.h>
#include <unistd.h>

#include <arc/Logger.h>
#include <arc/Utils.h>

#include "conf.h"
#include "environment.h"

#include "daemon.h"


namespace gridftpd {

  static Arc::Logger logger(Arc::Logger::getRootLogger(),"Daemon");

  static Arc::LogFile* sighup_dest = NULL;

  static void sighup_handler(int) {
    if(!sighup_dest) return;
    sighup_dest->setReopen(true);
    sighup_dest->setReopen(false);
  }

  Daemon::Daemon(void):logfile_(""),logsize_(0),lognum_(5),logreopen_(false),uid_((uid_t)(-1)),gid_((gid_t)(-1)),daemon_(true),pidfile_(""),debug_(-1) {
  }

  Daemon::~Daemon(void) {
  }

  int Daemon::arg(char c) {
    switch(c) {
      case 'F': {
        daemon_=false;
      }; break;
      case 'L': {
        logfile_=optarg;
      }; break;
      case 'U': {
        std::string username(optarg);
        std::string groupname("");
        std::string::size_type n = username.find(':');
        if(n != std::string::npos) { groupname=optarg+n+1; username.resize(n); };
        if(username.length() == 0) { uid_=0; gid_=0; } else {
          struct passwd pw_;
          struct passwd *pw;
          char buf[BUFSIZ];
          getpwnam_r(username.c_str(),&pw_,buf,BUFSIZ,&pw);
          if(pw == NULL) {
            logger.msg(Arc::ERROR, "No such user: %s", username);
            uid_=0; gid_=0; return -1;
          };
          uid_=pw->pw_uid;
          gid_=pw->pw_gid;
        };
        if(groupname.length() != 0) {
          struct group gr_;
          struct group *gr;
          char buf[BUFSIZ];
          getgrnam_r(groupname.c_str(),&gr_,buf,BUFSIZ,&gr);
          if(gr == NULL) {
            logger.msg(Arc::ERROR, "No such group: %s", groupname);
            gid_=0; return -1;
          };
          gid_=gr->gr_gid;
        };
      }; break;
      case 'P': {
        pidfile_=optarg;
      }; break;
      case 'd': {
        char* p;
        debug_ = strtol(optarg,&p,10);
        if(((*p) != 0) || (debug_<0)) {
          logger.msg(Arc::ERROR, "Improper debug level '%s'", optarg);
          return 1;
        };
      }; break;
      default:
        return 1;
    };
    return 0;
  }

  int Daemon::config(const std::string& cmd,std::string& rest) {
    if(cmd == "gridmap") {
      Arc::SetEnv("GRIDMAP",rest.c_str()); return 0;
    } else if(cmd == "hostname") {
      Arc::SetEnv("GLOBUS_HOSTNAME",rest.c_str()); return 0;
    } else if(cmd == "globus_tcp_port_range") {
      Arc::SetEnv("GLOBUS_TCP_PORT_RANGE",rest.c_str()); return 0;
    } else if(cmd == "globus_udp_port_range") {
      Arc::SetEnv("GLOBUS_UDP_PORT_RANGE",rest.c_str()); return 0;
    } else if(cmd == "x509_user_key") {
      Arc::SetEnv("X509_USER_KEY",rest.c_str()); return 0;
    } else if(cmd == "x509_user_cert") {
      Arc::SetEnv("X509_USER_CERT",rest.c_str()); return 0;
    } else if(cmd == "x509_cert_dir") {
      Arc::SetEnv("X509_CERT_DIR",rest.c_str()); return 0;
    } else if(cmd == "http_proxy") {
      Arc::SetEnv("ARC_HTTP_PROXY",rest.c_str()); return 0;
    } else if(cmd == "x509_voms_dir") {
      Arc::SetEnv("X509_VOMS_DIR",rest.c_str()); return 0;
    } else if(cmd == "voms_processing") {
      Arc::SetEnv("VOMS_PROCESSING",rest.c_str()); return 0;
    } else if(cmd == "voms_trust_chain") {
      // There could be multiple "voms_trust_chain" for multiple voms servers
      std::string voms_trust_chains = Arc::GetEnv("VOMS_TRUST_CHAINS");
      if(!voms_trust_chains.empty()) voms_trust_chains.append("\n").append(rest);
      else voms_trust_chains = rest;
      Arc::SetEnv("VOMS_TRUST_CHAINS",voms_trust_chains.c_str()); return 0;
    };
    if(cmd == "daemon") {
      if(daemon_) {
        std::string arg = config_next_arg(rest);
        if(arg=="") {
          logger.msg(Arc::ERROR, "Missing option for command daemon");
          return -1;
        };
        if(strcasecmp("yes",arg.c_str()) == 0) { daemon_=true; }
        else if(strcasecmp("no",arg.c_str()) == 0) { daemon_=false; }
        else { logger.msg(Arc::ERROR, "Wrong option in daemon"); return -1; };
      };
    } else if(cmd == "logfile") {
      if(logfile_.length() == 0) logfile_=config_next_arg(rest);
    } else if(cmd == "logsize") {
      if(logsize_ == 0) {
        char* p;
        logsize_ = strtol(rest.c_str(),&p,10);
        if(logsize_ < 0) {
          logsize_=0;
          logger.msg(Arc::ERROR, "Improper size of log '%s'", rest);
          return -1;
        };
        if((*p) == ' ') {
          for(;*p;++p) if((*p) != ' ') break;
          if(*p) {
            lognum_ = strtol(p,&p,10);
            if(lognum_ < 0) {
              logsize_=0; lognum_=0;
              logger.msg(Arc::ERROR, "Improper number of logs '%s'", rest);
              return -1;
            };
          };
        } else if((*p) != 0) {
          logsize_=0; lognum_=0;
          logger.msg(Arc::ERROR, "Improper argument for logsize '%s'", rest);
          return -1;
        };
      };
    } else if(cmd == "logreopen") {
      std::string arg = config_next_arg(rest);
      if(arg=="") {
        logger.msg(Arc::ERROR, "Missing option for command logreopen");
        return -1;
      };
      if(strcasecmp("yes",arg.c_str()) == 0) { logreopen_=true; }
      else if(strcasecmp("no",arg.c_str()) == 0) { logreopen_=false; }
      else { logger.msg(Arc::ERROR, "Wrong option in logreopen"); return -1; };
    } else if(cmd == "user") {
      if(uid_ == (uid_t)(-1)) {
        std::string username = config_next_arg(rest);
        std::string groupname("");
        std::string::size_type n = username.find(':');
        if(n != std::string::npos) { groupname=username.c_str()+n+1; username.resize(n); };
        if(username.length() == 0) { uid_=0; gid_=0; } else {
          struct passwd pw_;
          struct passwd *pw;
          char buf[BUFSIZ];
          getpwnam_r(username.c_str(),&pw_,buf,BUFSIZ,&pw);

          if(pw == NULL) {
            logger.msg(Arc::ERROR, "No such user: %s", username);
            uid_=0; gid_=0; return -1;
          };
          uid_=pw->pw_uid;
          gid_=pw->pw_gid;
         };
        if(groupname.length() != 0) {
          struct group gr_;
          struct group *gr;
          char buf[BUFSIZ];
          getgrnam_r(groupname.c_str(),&gr_,buf,BUFSIZ,&gr);
          if(gr == NULL) {
            logger.msg(Arc::ERROR, "No such group: %s", groupname);
            gid_=0; return -1;
          };
          gid_=gr->gr_gid;
        };
      };
    } else if(cmd == "pidfile") {
      if(pidfile_.length() == 0) pidfile_=config_next_arg(rest);
    } else if(cmd == "debug") {
      if(debug_ == -1) {
        char* p;
        debug_ = strtol(rest.c_str(),&p,10);
        if(((*p) != 0) || (debug_<0)) {
          logger.msg(Arc::ERROR, "Improper debug level '%s'", rest);
          return -1;
        };
      };
    } else {
      return 1;
    };
    return 0;
  }

  int Daemon::skip_config(const std::string& cmd) {
    if(cmd == "debug") return 0;
    if(cmd == "daemon") return 0;
    if(cmd == "logfile") return 0;
    if(cmd == "logsize") return 0;
    if(cmd == "user") return 0;
    if(cmd == "pidfile") return 0;
    return 1;
  }

  int Daemon::getopt(int argc, char * const argv[],const char *optstring) {
    int n;
    std::string opts(optstring);
    opts+=DAEMON_OPTS;
    while((n=::getopt(argc,argv,opts.c_str())) != -1) {
      switch(n) {
        case 'F':
        case 'L':
        case 'U':
        case 'P':
        case 'd': {
          if(arg(n) != 0) return '.';
        }; break;
        default: return n;
      };
    };
    return -1;
  }

  int Daemon::daemon(bool close_fds) {
    // set up logging
    // this must be a pointer which is not deleted because log destinations
    // are added by reference...
    Arc::LogFile* logger_file = new Arc::LogFile(logfile_);
    if (!logger_file || !(*logger_file)) {
      logger.msg(Arc::ERROR, "Failed to open log file %s", logfile_);
      return 1;
    }
    if (logsize_ > 0) logger_file->setMaxSize(logsize_);
    if (lognum_ > 0) logger_file->setBackups(lognum_);
    logger_file->setReopen(logreopen_);
    if (debug_ > 0) {
      Arc::Logger::getRootLogger().setThreshold(Arc::old_level_to_level((unsigned int)debug_));
    };
    Arc::Logger::getRootLogger().removeDestinations();
    Arc::Logger::getRootLogger().addDestination(*logger_file);
    if(!logreopen_) {
      sighup_dest = logger_file;
      signal(SIGHUP,&sighup_handler);
    };
    if(close_fds) {
      struct rlimit lim;
      int max_files;
      if(getrlimit(RLIMIT_NOFILE,&lim) == 0) { max_files=lim.rlim_cur; }
      else { max_files=4096; };
      if(max_files == RLIM_INFINITY) max_files=4096;
      for(int i=3;i<max_files;i++) { close(i); };
    };
    {
      close(0);
      int h=::open("/dev/null",O_RDONLY);
      if(h != 0) {
        if(h != -1) {
          int hh = dup2(h,0); if(hh != 0) { if(hh != -1) close(hh); };
          close(h);
        };
      };
    };
    const char* log = logfile_.c_str();
    if(daemon_) { /*if(log[0] == 0)*/ log="/dev/null"; };
    if(log[0] != 0) {
      close(1); close(2);
      int h=::open(log,O_WRONLY | O_CREAT | O_APPEND,S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
      if(h != 1) {
        if(h != -1) {
          int hh = dup2(h,1); if(hh != 1) { if(hh != -1) close(hh); };
          hh=dup2(h,2); if(hh != 2) { if(hh != -1) close(hh); };
          close(h);
        };
      } else {
        int hh = dup2(h,2); if(hh != 2) { if(hh != -1) close(hh); };
      };
    } else {  // old stderr
      close(1);
      int hh = dup2(2,1); if(hh != 1) { if(hh != -1) close(hh); };
    };
    int hp = -1;
    if(pidfile_.length() != 0) hp=::open(pidfile_.c_str(),O_WRONLY | O_CREAT | O_TRUNC,S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if((gid_ != (gid_t)(-1)) && (gid_ != 0)) setgid(gid_);
    if((uid_ != (uid_t)(-1)) && (uid_ != 0)) setuid(uid_);
    int r = 0;
    if(daemon_) {
  #ifdef HAVE_DAEMON
      r=::daemon(1,1);
  #else
      r=::fork();
      if(r == 0) {
        // child
        r=setsid();
        if(r != -1) r=0;
      } else if(r != -1) {
        // parent
        _exit(0);
      };
  #endif
      if(r != 0) return r;
      /** is this necessary???
      if(nordugrid_loc.empty()) {
        chdir("/");
      } else {
        chdir(nordugrid_loc.c_str());
      };
      */
    };
    if(hp != -1) {
      char buf[30]; int l = snprintf(buf,29,"%u",getpid()); buf[l]=0;
      if(::write(hp,buf,l)<0) {
        ::close(hp);
        return 0;
      };
      ::close(hp);
    };
    return 0;
  }

  const char* Daemon::short_help(void) {
    return "[-F] [-U user[:group]] [-L logfile] [-P pidfile] [-d level]";
  }

  void Daemon::logfile(const char* path) {
    if(logfile_.length() == 0) logfile_=path;
  }

  void Daemon::pidfile(const char* path) {
    if(pidfile_.length() == 0) pidfile_=path;
  }

} // namespace gridftpd
