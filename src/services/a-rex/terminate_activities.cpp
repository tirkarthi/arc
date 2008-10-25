#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <arc/message/SOAPEnvelope.h>
#include <arc/ws-addressing/WSA.h>
#include "job.h"

#include "arex.h"

namespace ARex {


Arc::MCC_Status ARexService::TerminateActivities(ARexGMConfig& config,Arc::XMLNode in,Arc::XMLNode out) {
  /*
  TerminateActivities
    ActivityIdentifier (wsa:EndpointReferenceType, unbounded)

  TerminateActivitiesResponse
    Response (unbounded)
      ActivityIdentifier
      Terminated (boolean)
      Fault (soap:Fault)

  */
  {
    std::string s;
    in.GetXML(s);
    logger_.msg(Arc::DEBUG, "TerminateActivities: request = \n%s", s);
  };
  for(int n = 0;;++n) {
    Arc::XMLNode id = in["ActivityIdentifier"][n];
    if(!id) break;
    // Create place for response
    Arc::XMLNode resp = out.NewChild("bes-factory:Response");
    resp.NewChild(id);
    std::string jobid = Arc::WSAEndpointReference(id).ReferenceParameters()["a-rex:JobID"];
    if(jobid.empty()) {
      // EPR is wrongly formated or not an A-REX EPR
      logger_.msg(Arc::ERROR, "TerminateActivities: non-ARex job requested");
      Arc::SOAPFault fault(resp,Arc::SOAPFault::Sender,"Missing a-rex:JobID in ActivityIdentifier");
      UnknownActivityIdentifierFault(fault,"Unrecognized EPR in ActivityIdentifier");
      continue;
    };
    // Look for obtained ID
    ARexJob job(jobid,config,logger_);
    if(!job) {
      // There is no such job
      logger_.msg(Arc::ERROR, "TerminateActivities: job %s - %s", jobid, job.Failure());
      Arc::SOAPFault fault(resp,Arc::SOAPFault::Sender,"No corresponding activity found");
      UnknownActivityIdentifierFault(fault,("No activity "+jobid+" found: "+job.Failure()).c_str());
      continue;
    };
    /*
    // Check permissions on that ID
    */
    // Cancel job (put a mark)
    bool result = job.Cancel();
    if(result) {
      resp.NewChild("bes-factory:Terminated")="true";
    } else {
      resp.NewChild("bes-factory:Terminated")="false";
      // Or should it be a fault?
    };
  };
  {
    std::string s;
    out.GetXML(s);
    logger_.msg(Arc::DEBUG, "TerminateActivities: response = \n%s", s);
  };
  return Arc::MCC_Status(Arc::STATUS_OK);
}

} // namespace ARex

