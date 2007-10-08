#ifndef __ARC_SEC_ARCEVALUATIONCTX_H__
#define __ARC_SEC_ARCEVALUATIONCTX_H__

#include <list>
#include <fstream>
#include <arc/XMLNode.h>
#include <arc/Logger.h>
#include "attr/AttributeValue.h"

#include "Request.h"

/** EvaluationCtx, storing some context information for evaluation, including Request, current time, etc. */

namespace ArcSec {

/*
typedef struct{
  Arc::Subject sub;
  Arc::Resource res;
  Arc::Action act;
  Arc::Context ctx;
} RequestTuple;
*/

class RequestTuple {
public:
  Subject sub;
  Resource res;
  Action act;
  Context ctx;
public:
  RequestTuple& duplicate(const RequestTuple&);
public:
  ~RequestTuple();
  void erase();
};


class EvaluationCtx {

public:
  //**Construct a new EvaluationCtx based on the given request.*/
  EvaluationCtx (Request* request);
  virtual ~EvaluationCtx();
  
  virtual Request* getRequest() const;
 
  virtual void setRequestItem(RequestItem* reqit){reqitem = reqit;};

  virtual RequestItem* getRequestItem() const {return reqitem;};
/*
  virtual AttributeValue * getSubjectAttribute();
  virtual AttributeValue * getResourceAttribute();
  virtual AttributeValue * getActionAttribute();
  virtual AttributeValue * getContextAttribute();
*/
  //Convert each RequestItem ( one tuple <SubList, ResList, ActList, CtxList>)  into some <Subject, Resource, Action, Context> tuples.
  //The purpose is for evaluation. The evaluator will evaluate each RequestTuple one by one, not the RequestItem because it includes some       //independent <Subject, Resource, Action, Context>s and the evaluator should deal with them independently. 
  virtual void split();

  virtual std::list<RequestTuple*> getRequestTuples() const { return reqtuples; };
  virtual void setEvalTuple(RequestTuple* tuple){ evaltuple = tuple; };
  virtual RequestTuple* getEvalTuple()const { return evaltuple; };
  
private:
  static Arc::Logger logger;
  Request* req;
  RequestItem* reqitem;
  std::list<RequestTuple*> reqtuples;
  //The RequestTuple for evaluation at present
  RequestTuple* evaltuple;
 
};

} // namespace ArcSec

#endif /* __ARC_SEC_EVALUATIONCTX_H__ */
