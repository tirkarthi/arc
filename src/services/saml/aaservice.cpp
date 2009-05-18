#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>

#include <sys/types.h>
#include <pwd.h>

#include <arc/message/PayloadSOAP.h>
#include <arc/message/SOAPEnvelope.h>
#include <arc/DateTime.h>
#include <arc/GUID.h>

#include <arc/credential/Credential.h>
#include <arc/xmlsec/XmlSecUtils.h>
#include <arc/xmlsec/XMLSecNode.h>
#include <arc/MysqlWrapper.h>

#include "aaservice.h"

namespace ArcSec {

#define SAML_NAMESPACE "urn:oasis:names:tc:SAML:2.0:assertion"
#define SAMLP_NAMESPACE "urn:oasis:names:tc:SAML:2.0:protocol"

#define XENC_NAMESPACE   "http://www.w3.org/2001/04/xmlenc#"
#define DSIG_NAMESPACE   "http://www.w3.org/2000/09/xmldsig#"

static Arc::Plugin* get_service(Arc::PluginArgument* arg) {
    Arc::ServicePluginArgument* servarg = arg?dynamic_cast<Arc::ServicePluginArgument*>(arg):NULL;
    return new Service_AA((Arc::Config*)(*servarg));
}

Arc::MCC_Status Service_AA::make_soap_fault(Arc::Message& outmsg) {
  Arc::PayloadSOAP* outpayload = new Arc::PayloadSOAP(ns_,true);
  Arc::SOAPFault* fault = outpayload?outpayload->Fault():NULL;
  if(fault) {
    fault->Code(Arc::SOAPFault::Sender);
    fault->Reason("Failed processing request");
  };
  outmsg.Payload(outpayload);
  return Arc::MCC_Status(Arc::STATUS_OK);
}

static std::string convert_dn(const std::string& dn) {
  std::string ret;
  size_t pos1 = std::string::npos;
  size_t pos2;
  do {
    std::string str;
    pos2 = dn.find_last_of("/", pos1);
    if(pos2 != std::string::npos && pos1 == std::string::npos) {
      str = dn.substr(pos2+1);
      ret.append(str);
      pos1 = pos2-1;
    }
    else if (pos2 != std::string::npos && pos1 != std::string::npos) {
      str = dn.substr(pos2+1, pos1-pos2);
      ret.append(str);
      pos1 = pos2-1;
    }
    if(pos2 != (std::string::npos+1)) ret.append(",");
  }while(pos2 != std::string::npos && pos2 != (std::string::npos+1));
  return ret;
}

static std::string get_cert_str(const std::string& cert) {
  std::size_t pos = cert.find("BEGIN CERTIFICATE");
  if(pos != std::string::npos) {
    std::size_t pos1 = cert.find_first_of("---", pos);
    std::size_t pos2 = cert.find_first_not_of("-", pos1);
    std::size_t pos3 = cert.find_first_of("---", pos2);
    std::string str = cert.substr(pos2+1, pos3-pos2-2);
    return str;
  }
  return ("");
}

Arc::MCC_Status Service_AA::process(Arc::Message& inmsg,Arc::Message& outmsg) {

  //The DN of peer certificate, which is the authentication result during tls;
  //It could also be set by using the authentication result in message level
  //authentication, such as saml token profile in WS-Security.
  //Note: since the peer DN is obtained from transport level authentication, the request 
  //(with the peer certificate) is exactly the principal inside the <saml:NameID>, which means
  //the request is unlike ServiceProvider in WebSSO scenario which gets the <AuthnQuery>
  //result from IdP, and uses the principal (which is not the principal of ServiceProvider) 
  //in <AuthnQuery> result to initiate a <AttributeQuery> to AA.
  //Hence the scenario is "SAML Attribute Self-Query Deployment Profile for X.509 Subjects" inside 
  //document "SAML V2.0 Deployment Profiles for X.509 Subjects"

  std::string peer_rdn = inmsg.Attributes()->get("TLS:PEERDN");
  std::string peer_dn = convert_dn(peer_rdn);

  // Extracting payload
  Arc::PayloadSOAP* inpayload = NULL;
  try {
    inpayload = dynamic_cast<Arc::PayloadSOAP*>(inmsg.Payload());
  } catch(std::exception& e) { };
  if(!inpayload) {
    logger_.msg(Arc::ERROR, "input is not SOAP");
    return make_soap_fault(outmsg);
  }

  Arc::XMLNode attrqry;
  attrqry = (*inpayload).Body().Child(0);

  std::string query_idname = "ID";

#if 0
  Arc::XMLSecNode attrqry_secnode(attrqry);
  if(attrqry_secnode.VerifyNode(query_idname, cafile_, cadir_)) {
    logger_.msg(Arc::INFO, "Succeeded to verify the signature under <AttributeQuery/>");
  }
  else {     
    logger_.msg(Arc::ERROR, "Failed to verify the signature under <AttributeQuery/>");
    return Arc::MCC_Status();
  }
#endif

  Arc::NS ns;
  ns["saml"] = SAML_NAMESPACE;
  ns["samlp"] = SAMLP_NAMESPACE;
  ns["xsi"] = "http://www.w3.org/2001/XMLSchema-instance";
  ns["xsd"] = "http://www.w3.org/2001/XMLSchema";

  //Reset the namespaces of <AttributeQuery/> in case the prefix is not the same as "saml" and "samlp"
  attrqry.Namespaces(ns);

  Arc::XMLNode issuer = attrqry["saml:Issuer"];

  //Compare the <saml:NameID> inside the <AttributeQuery> message with the peer DN
  //which has been got from the former authentication
  //More complicated processing should be considered, according to 3.3.4 in SAML core specification
  Arc::XMLNode subject = attrqry["saml:Subject"];
  std::string name_id = (std::string)(subject["saml:NameID"]);
  if(name_id == peer_dn) {
    logger_.msg(Arc::INFO, "The NameID inside request is the same as the NameID from the tls authentication: %s", peer_dn.c_str());
  }
  else {
    logger_.msg(Arc::INFO, "The NameID inside request is: %s; not the same as the NameID from the tls authentication: %s", name_id.c_str(), peer_dn.c_str());
    return Arc::MCC_Status();
  }

  Arc::XMLNode subject_confirmation = subject.NewChild("saml:SubjectConfirmation");
  subject_confirmation.NewAttribute("Method")=std::string("urn:oasis:names:tc:SAML:2.0:cm:holder-of-key");
  Arc::XMLNode subject_confirmation_data = subject_confirmation.NewChild("saml:SubjectConfirmationData");
  Arc::NS ds_ns("ds",DSIG_NAMESPACE);
  Arc::XMLNode key_info = subject_confirmation_data.NewChild("ds:KeyInfo",ds_ns);
  Arc::XMLNode x509_data = key_info.NewChild("ds:X509Data");
  Arc::XMLNode x509_cert = x509_data.NewChild("ds:X509Certificate");
  std::string x509_str = get_cert_str(inmsg.Attributes()->get("TLS:PEERCERT"));
  x509_cert = x509_str;


  //Get the <Attribute>s from <AttributeQuery> message, which is required by request; 
  //AA will only return those <Attribute> which is required by request
  std::vector<Arc::XMLNode> attributes;
  for(int i=0;; i++) {
    Arc::XMLNode cn =  attrqry["saml:Attribute"][i];
    if(!cn) break;
    attributes.push_back(cn);
  }

  //AA: Generate a response 
  //AA will try to query local attribute database, intersect the attribute result from
  //database and the attribute requirement from the request
  //Then, insert those <Attribute> into response

  //TODO: access the local attribute database, use the <NameID> as searching key
  
  std::vector<std::string> attribute_name_list;
  for(int i=0; i<attributes.size(); i++) {
    Arc::XMLNode nd = attributes[i];
    std::string str = (std::string)(nd.Attribute("Name"));
    if(!str.empty()) { 
      attribute_name_list.push_back(str); 
    }
    else {
      logger_.msg(Arc::ERROR, "There should be Name attribute in request's <Attribute> node");
      return Arc::MCC_Status();
    }
  }
 
  std::vector<std::string> fqans1;
  //udn = peer_rdn;
  std::string udn("/O=Grid/O=NorduGrid/OU=fys.uio.no/CN=Weizhong Qiang");
  std::vector<std::string> sqlargs1;
  sqlargs1.push_back(udn);
  std::string query_type1("UID");
  get_results(fqans1, sqlargs1, query_type1, dbconf_);
  
  std::vector<std::string> fqans;
  std::string role = "physicist";
  std::string uid = "3"; // std::string uid = fqans1[0];
  std::vector<std::string> sqlargs;
  //sequence of arguments mattes; and it should be corresponding to
  //the arguments squence in sql sentence
  sqlargs.push_back(role);
  sqlargs.push_back(uid);
  std::string query_type("Role");
  get_results(fqans, sqlargs, query_type, dbconf_);
  if(fqans.size()!=0) {
    std::cout<<"Got db query result"<<std::endl;
    for(int i=0; i<fqans.size(); i++)
      std::cout<<fqans[i]<<std::endl;
  }
  else
    std::cout<<"Did not get db query result"<<std::endl;

  //TODO: Compare the attribute name from database result and the attribute_name_list,
  //Only use the intersect as the response
  
  //Compose <saml:Response/>
  Arc::XMLNode attr_response(ns, "samlp:Response");

  Arc::Credential cred(certfile_, keyfile_, cadir_, cafile_);
  std::string local_dn = cred.GetDN();
  std::string aa_name = convert_dn(local_dn);
  attr_response.NewChild("saml:Issuer") = aa_name;

  std::string response_id = Arc::UUID();
  attr_response.NewAttribute("ID") = response_id;
  std::string responseto_id = (std::string)(attrqry.Attribute("ID"));
  attr_response.NewAttribute("InResponseTo") = responseto_id;
  Arc::Time t;
  std::string current_time = t.str(Arc::UTCTime);
  attr_response.NewAttribute("IssueInstant") = current_time;
  attr_response.NewAttribute("Version") = std::string("2.0");
 
  //<samlp:Status/> 
  Arc::XMLNode status = attr_response.NewChild("samlp:Status");
  Arc::XMLNode statuscode = status.NewChild("samlp:StatusCode");
  std::string statuscode_value = "urn:oasis:names:tc:SAML:2.0:status:Success";
  statuscode.NewAttribute("Value") = statuscode_value;

  //<saml:Assertion/>
  Arc::XMLNode assertion = attr_response.NewChild("saml:Assertion");
  assertion.NewAttribute("Version") = std::string("2.0");
  std::string assertion_id = Arc::UUID();
  assertion.NewAttribute("ID") = assertion_id;
  Arc::Time t1;
  std::string current_time1 = t1.str(Arc::UTCTime);
  assertion.NewAttribute("IssueInstant") = current_time1;

  //<saml:Issuer/>
  assertion.NewChild("saml:Issuer") = aa_name;

  //<saml:Subject/>
  //<saml:Subject/> is the same as the one in request
  assertion.NewChild(subject);
 
  //<saml:Conditions>
  Arc::XMLNode conditions = assertion.NewChild("saml:Conditions"); 
  //Arc::XMLNode audience_restriction = conditions.NewChild("saml:AudienceRestriction");
  //std::string client_name("https://sp.com/SAML"); //TODO
  //audience_restriction.NewChild("saml:Audience") = client_name; 
  Arc::Time t_start;
  std::string time_start = t_start.str(Arc::UTCTime);
  Arc::Time t_end = t_start + Arc::Period(43200);
  std::string time_end = t_end.str(Arc::UTCTime);
  conditions.NewAttribute("NotBefore") = time_start;  
  conditions.NewAttribute("NotOnOrAfter") = time_end;              

  //<saml:AttributeStatement/> 
  Arc::XMLNode attr_statement = assertion.NewChild("saml:AttributeStatement");

  //<saml:Attribute/>
  //The following is just one <Attribute> result for test. The real <Attribute> 
  //should be compose according to the database searching result
  Arc::XMLNode attribute = attr_statement.NewChild("saml:Attribute");
  Arc::XMLNode attr_value = attribute.NewChild("saml:AttributeValue");

  attr_value.NewAttribute("xsi:type") = std::string("xs:string");

  //TODO
  attribute.NewAttribute("Name") = std::string("urn:oid:1.3.6.1.4.1.5923.1.1.1.6");
  attribute.NewAttribute("NameFormat")= std::string("urn:oasis:names:tc:SAML:2.0:attrname-format:uri");
  attribute.NewAttribute("FriendlyName") = std::string("eduPersonPrincipalName");
  attr_value = std::string("RoleA");
  //Add one or more <AttributeValue> into <Attribute>
  //Add one or more <Attribute> into <Assertion>

  Arc::XMLSecNode assertion_secnd(assertion);
  std::string assertion_idname("ID");
  std::string inclusive_namespaces = "saml ds xs";
  assertion_secnd.AddSignatureTemplate(assertion_idname, Arc::XMLSecNode::RSA_SHA1, inclusive_namespaces);
  if(assertion_secnd.SignNode(keyfile_, certfile_)) {
    std::cout<<"Succeed to sign the signature under <saml:Assertion/>"<<std::endl;
    //std::string str;
    //assertion_secnd.GetXML(str);
    //std::cout<<"Signed node: "<<std::endl<<str<<std::endl;
  }

  Arc::XMLSecNode attr_response_secnd(attr_response);
  std::string attr_response_idname("ID");
  attr_response_secnd.AddSignatureTemplate(attr_response_idname, Arc::XMLSecNode::RSA_SHA1);
  if(attr_response_secnd.SignNode(keyfile_, certfile_)) {
    std::cout<<"Succeed to sign the signature under <samlp:Response/>"<<std::endl;
    //std::string str;
    //attr_response_secnd.GetXML(str);
    //std::cout<<"Signed node: "<<std::endl<<str<<std::endl;
  }

  //Put the <samlp:Response/> into soap body.
  Arc::NS soap_ns;
  Arc::SOAPEnvelope envelope(soap_ns);
  envelope.NewChild(attr_response);
  Arc::PayloadSOAP *outpayload = new Arc::PayloadSOAP(envelope);

  std::string tmp;
  outpayload->GetXML(tmp);
  std::cout<<"Output payload: "<<tmp<<std::endl;

  outmsg.Payload(outpayload);
  return Arc::MCC_Status(Arc::STATUS_OK);
}

Service_AA::Service_AA(Arc::Config *cfg):Service(cfg), logger_(Arc::Logger::rootLogger, "AA_Service") {
  keyfile_ = (std::string)((*cfg)["KeyPath"]);
  certfile_ = (std::string)((*cfg)["CertificatePath"]);
  cafile_ = (std::string)((*cfg)["CACertificatePath"]);
  cadir_ = (std::string)((*cfg)["CACertificatesDir"]);
  dbconf_ = (*cfg)["Database"];
  Arc::init_xmlsec();
}

Service_AA::~Service_AA(void) {
  Arc::final_xmlsec();
}

bool Service_AA::get_results(std::vector<std::string>& fqans, const std::vector<std::string>& sqlargs, 
      const std::string& idofsqlset, Arc::XMLNode& config) {
  Arc::QueryArrayResult attributes;
  std::vector<std::string> args;
  for(int j = 0; j< sqlargs.size(); j++) {
    std::string item;
    item.append("\"").append(sqlargs[j]).append("\"");
    args.push_back(item);
  }
  bool res;
  res = query_db(attributes, idofsqlset, args, config);
  if(!res) return res;
  for(int i = 0; i< attributes.size(); i++) {
    std::vector<std::string> item = attributes[i];
    int num = item.size();
    std::string fqan;
    if(num == 1) { // example:  UID
      fqan = item[0];
    }
    else if(num == 2) { // example:  /Group=knowarc/Role=physicist
      fqan = ("/Group=" + item[0].substr(1)) + (item[1].empty() ? "":("/Role=" + item[1]));
    }
    else if(num == 4) { // example:  /Group=knowarc/Role=physicist:Degree=PhD
      fqan = ("/Group=" + item[2].substr(1)) + (item[3].empty() ? "":("/Role=" + item[3])) + ":" + item[0] + "=" + item[1];
    }
    fqans.push_back(fqan);
  }
  return true;
}

bool Service_AA::query_db(Arc::QueryArrayResult& attributes, const std::string& idofsqlset, std::vector<std::string>& sqlargs, Arc::XMLNode& config) {
  Arc::XMLNode nd;
  //nd = config["aa:Database"];
  nd = config;
  std::string server, dbname, user, password, portstr;
  int port;
  server = (std::string)(nd.Attribute("ip"));
  dbname = (std::string)(nd.Attribute("dbname"));
  user = (std::string)(nd.Attribute("user"));
  password = (std::string)(nd.Attribute("password"));
  portstr = (std::string)(nd.Attribute("port"));
  port = atoi((portstr.c_str()));

  logger_.msg(Arc::DEBUG, "Access database %s from server %s port %s, with user %s and password %s",
              dbname.c_str(), server.c_str(), portstr.c_str(), user.c_str(), password.c_str()); 

  //TODO: make the database and sql object dynamic loaded 
  //according to the "name" (mysql, oracle, etc.)
  Arc::MySQLDatabase mydb(server, port);
  bool res = false;
  res = mydb.connect(dbname,user,password);
  if(res == false) {
    logger_.msg(Arc::ERROR,"Can't establish connection to mysql database"); return false;
  }

  Arc::MySQLQuery myquery(&mydb);
    logger_.msg(Arc::DEBUG, "Is connected to database? %s", mydb.isconnected()? "yes":"no");

  std::string querystr;
  for(int i = 0;; i++) {
    Arc::XMLNode cn = nd["aa:SQLSet"][i];
    if(!cn) break;
    if(((std::string)(cn.Attribute("name"))) == idofsqlset) {
      for(int k = 0;; k++) {
        Arc::XMLNode scn = cn["aa:SQL"][k];
        if(!scn) break;
        querystr = (std::string)scn;
        logger_.msg(Arc::DEBUG, "Query: %s", querystr.c_str());
        myquery.get_array(querystr, attributes, sqlargs);
      }
    }
  }
  logger_.msg(Arc::DEBUG, "Get result array with %d rows",attributes.size());
  return true;
}

} // namespace ArcSec

Arc::PluginDescriptor PLUGINS_TABLE_NAME[] = {
    { "aa.service", "HED:SERVICE", 0, &ArcSec::get_service },
    { NULL, NULL, 0, NULL }
};
