#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <arc/message/SOAPEnvelope.h>
#include <arc/ws-addressing/WSA.h>
#include "job.h"

#include "arex.h"

namespace ARex {


Arc::MCC_Status ARexService::GetActivityDocuments(ARexGMConfig& config,Arc::XMLNode in,Arc::XMLNode out) {
  /*
  GetActivityDocuments
    ActivityIdentifier (wsa:EndpointReferenceType, unbounded)

  GetActivityDocumentsResponse
    Response (unbounded)
      ActivityIdentifier
      JobDefinition (jsdl:JobDefinition)
      Fault (soap:Fault)
  UnknownActivityIdentifierFault
  */
  {
    std::string s;
    in.GetXML(s);
    logger.msg(Arc::DEBUG, "GetActivityDocuments: request = \n%s", s);
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
      logger_.msg(Arc::ERROR, "GetActivityDocuments: non-ARex job requested");
      Arc::SOAPFault fault(resp,Arc::SOAPFault::Sender,"Missing a-rex:JobID in ActivityIdentifier");
      UnknownActivityIdentifierFault(fault,"Unrecognized EPR in ActivityIdentifier");
      continue;
    };
    // Look for obtained ID
    ARexJob job(jobid,config,logger_);
    if(!job) {
      // There is no such job
      logger_.msg(Arc::ERROR, "GetActivityDocuments: job %s - %s", jobid, job.Failure());
      Arc::SOAPFault fault(resp,Arc::SOAPFault::Sender,"No corresponding activity found");
      UnknownActivityIdentifierFault(fault,("No activity "+jobid+" found: "+job.Failure()).c_str());
      continue;
    };
    /*
    // TODO: Check permissions on that ID
    */
    // Read JSDL of job
    Arc::XMLNode jsdl = resp.NewChild("bes-factory:JobDefinition");
    if(!job.GetDescription(jsdl)) {
      logger_.msg(Arc::ERROR, "GetActivityDocuments: job %s - %s", jobid, job.Failure());
      // Processing failure
      jsdl.Destroy();
      Arc::SOAPFault fault(resp,Arc::SOAPFault::Sender,"Failed processing activity");
      UnknownActivityIdentifierFault(fault,("Failed processing activity "+jobid+": "+job.Failure()).c_str());
      continue;
    };
    jsdl.Name("bes-factory:JobDefinition"); // Recovering namespace of element
  };
  {
    std::string s;
    out.GetXML(s);
    logger_.msg(Arc::DEBUG, "GetActivityDocuments: response = \n%s", s);
  };
  return Arc::MCC_Status(Arc::STATUS_OK);
}

} // namespace ARex

