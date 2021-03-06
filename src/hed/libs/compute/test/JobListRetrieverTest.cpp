#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cppunit/extensions/HelperMacros.h>

#include <arc/compute/Endpoint.h>
#include <arc/UserConfig.h>
#include <arc/compute/Job.h>
#include <arc/compute/EntityRetriever.h>
#include <arc/compute/TestACCControl.h>
#include <arc/Thread.h>

//static Arc::Logger testLogger(Arc::Logger::getRootLogger(), "JobListRetrieverTest");

class JobListRetrieverTest
  : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(JobListRetrieverTest);
  CPPUNIT_TEST(PluginLoading);
  CPPUNIT_TEST(QueryTest);
  CPPUNIT_TEST_SUITE_END();

public:
  JobListRetrieverTest() {};

  void setUp() {}
  void tearDown() { Arc::ThreadInitializer().waitExit(); }

  void PluginLoading();
  void QueryTest();
};

void JobListRetrieverTest::PluginLoading() {
  Arc::JobListRetrieverPluginLoader l;
  Arc::JobListRetrieverPlugin* p = (Arc::JobListRetrieverPlugin*)l.load("TEST");
  CPPUNIT_ASSERT(p != NULL);
}


void JobListRetrieverTest::QueryTest() {
  Arc::EndpointQueryingStatus sInitial(Arc::EndpointQueryingStatus::SUCCESSFUL);

  Arc::JobListRetrieverPluginTESTControl::delay = 1;
  Arc::JobListRetrieverPluginTESTControl::status = sInitial;
  
  Arc::JobListRetrieverPluginLoader l;
  Arc::JobListRetrieverPlugin* p = (Arc::JobListRetrieverPlugin*)l.load("TEST");
  CPPUNIT_ASSERT(p != NULL);

  Arc::UserConfig uc;
  Arc::Endpoint endpoint;
  std::list<Arc::Job> jobs;
  Arc::EndpointQueryingStatus sReturned = p->Query(uc, endpoint, jobs, Arc::EndpointQueryOptions<Arc::Job>());
  CPPUNIT_ASSERT(sReturned == Arc::EndpointQueryingStatus::SUCCESSFUL);
}

CPPUNIT_TEST_SUITE_REGISTRATION(JobListRetrieverTest);
