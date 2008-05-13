// based on: 
// http://www.codeproject.com/cpp/embedpython_1.asp
// http://coding.derkeiler.com/Archive/Python/comp.lang.python/2006-11/msg01211.html
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>
#include <arc/loader/Loader.h>
#include <arc/loader/ServiceLoader.h>
#include <arc/message/SOAPMessage.h>
#include <arc/message/PayloadSOAP.h>
#include "pythonwrapper.h"

#ifdef __cplusplus
extern "C" {
#endif

/* SWIG Specific object SHOULD BE SYNC WITH generated SWIG CODE */
typedef void *(*swig_converter_func)(void *);
typedef struct swig_type_info *(*swig_dycast_func)(void **);

typedef struct swig_type_info {
  const char             *name;			/* mangled name of this type */
  const char             *str;			/* human readable name of this type */
  swig_dycast_func        dcast;		/* dynamic cast function down a hierarchy */
  struct swig_cast_info  *cast;			/* linked list of types that can cast into this type */
  void                   *clientdata;		/* language specific type data */
  int                    owndata;		/* flag if the structure owns the clientdata */
} swig_type_info;

/* Structure to store a type and conversion function used for casting */
typedef struct swig_cast_info {
  swig_type_info         *type;			/* pointer to type that is equivalent to this type */
  swig_converter_func     converter;		/* function to cast the void pointers */
  struct swig_cast_info  *next;			/* pointer to next cast in linked list */
  struct swig_cast_info  *prev;			/* pointer to the previous cast */
} swig_cast_info;

typedef struct {
  PyObject_HEAD
  void *ptr;
  swig_type_info *ty;
  int own;
  PyObject *next;
} PySwigObject;

#ifdef __cplusplus
}
#endif

void *extract_swig_wrappered_pointer(PyObject *obj)
{
    char this_str[] = "this";
    if (!PyObject_HasAttrString(obj, this_str)) {
        return NULL;
    }
    PyObject *thisAttr = PyObject_GetAttrString(obj, this_str);
    if (thisAttr == NULL) {
        return NULL;
    }
    return (((PySwigObject *)thisAttr)->ptr);
}

// Thread state of main python interpreter thread
static PyThreadState *tstate = NULL;
static int python_service_counter = 0;

static Arc::Service* get_service(Arc::Config *cfg,Arc::ChainContext*) {
    
    // Initialize the Python Interpreter
    if (!Py_IsInitialized()) {
        Py_InitializeEx(0); // python do not handle signals
        PyEval_InitThreads();
    }
    python_service_counter++;
    return new Arc::Service_PythonWrapper(cfg);
}

service_descriptors ARC_SERVICE_LOADER = {
    { "pythonservice", 0, &get_service },
    { NULL, 0, NULL }
};

namespace Arc {

Arc::Logger Service_PythonWrapper::logger(Service::logger, "PythonWrapper");

Service_PythonWrapper::Service_PythonWrapper(Arc::Config *cfg):Service(cfg) 
{
    PyObject *py_module_name = NULL;
    PyObject *py_arc_module_name = NULL;
    PyObject *dict = NULL;
    PyObject *arc_dict = NULL;
    PyObject *arc_cfg_klass = NULL;
    PyObject *arg = NULL;
    PyObject *py_cfg = NULL;
 
    if (tstate != NULL) {
        PyEval_AcquireThread(tstate);
    }
    std::string path = (std::string)(*cfg)["ClassName"];    
    std::size_t p = path.rfind(".");
    if (p == std::string::npos) {
        logger.msg(Arc::ERROR, "Invalid class name");
        return;
    }
    std::string module_name = path.substr(0, p);
    std::string class_name = path.substr(p+1, path.length());
    logger.msg(Arc::DEBUG, "class name: %s", class_name);
    logger.msg(Arc::DEBUG, "module name: %s", module_name);
    
    // Convert module name to Python string
    py_module_name = PyString_FromString(module_name.c_str());
    if (py_module_name == NULL) {
        logger.msg(Arc::ERROR, "Cannot convert module name to Python string");
        if (PyErr_Occurred() != NULL) {
            PyErr_Print();
        }
        return;
    }
    // Load module
    module = PyImport_Import(py_module_name);
    if (module == NULL) {
        logger.msg(Arc::ERROR, "Cannot import module");
        if (PyErr_Occurred() != NULL) {
            PyErr_Print();
        }
        Py_DECREF(py_module_name);
        return;
    }
    Py_DECREF(py_module_name);
    
    // Import ARC python wrapper
    py_arc_module_name = PyString_FromString("arc");
    if (py_arc_module_name == NULL) {
        logger.msg(Arc::ERROR, "Cannot convert arc module name to Python string");
        if (PyErr_Occurred() != NULL) {
            PyErr_Print();
        }
        return;
    }
    
    // Load arc module
    arc_module = PyImport_Import(py_arc_module_name);
    if (arc_module == NULL) {
        logger.msg(Arc::ERROR, "Cannot import arc module");
        if (PyErr_Occurred() != NULL) {
            PyErr_Print();
        }
        Py_DECREF(py_arc_module_name);
        return;
    }
    Py_DECREF(py_arc_module_name);
    
    arc_dict = PyModule_GetDict(arc_module);
    if (arc_dict == NULL) {
        logger.msg(Arc::ERROR, "Cannot get dictionary of arc module");
        if (PyErr_Occurred() != NULL) {
            PyErr_Print();
        }
        return;
    }
    
    // Get the arc config class 
    arc_cfg_klass = PyDict_GetItemString(arc_dict, "Config");
    if (arc_cfg_klass == NULL) {
        logger.msg(Arc::ERROR, "Cannot find arc Config class");
        if (PyErr_Occurred() != NULL) {
            PyErr_Print();
        }
        return;
    }
    
    // check is it really a class
    if (!PyCallable_Check(arc_cfg_klass)) {
        logger.msg(Arc::ERROR, "Config klass is not an object");
        return;
    }
    
    // Get dictionary of module content
    // dict is a borrowed reference 
    dict = PyModule_GetDict(module);
    if (dict == NULL) {
        logger.msg(Arc::ERROR, "Cannot get dictionary of module");
        if (PyErr_Occurred() != NULL) {
            PyErr_Print();
        }
        return;
    }
    
    // Get the class 
    klass = PyDict_GetItemString(dict, (char*)class_name.c_str());
    if (klass == NULL) {
        logger.msg(Arc::ERROR, "Cannot find service class");
        if (PyErr_Occurred() != NULL) {
            PyErr_Print();
        }
        return;
    }
    
    // check is it really a class
    if (PyCallable_Check(klass)) {
        arg = Py_BuildValue("(l)", (long int)cfg);
        if (arg == NULL) {
            logger.msg(Arc::ERROR, "Cannot create config argument");
            if (PyErr_Occurred() != NULL) {
                PyErr_Print();
            }
            return;
        }

        py_cfg = PyObject_CallObject(arc_cfg_klass, arg);
        if (py_cfg == NULL) {
            logger.msg(Arc::ERROR, "Cannot convert config to python object");
            if (PyErr_Occurred() != NULL) {
                PyErr_Print();
            }
            Py_DECREF(arg);
            return;
        }
        Py_DECREF(arg); 
        arg = Py_BuildValue("(O)", py_cfg);
        if (arg == NULL) {
            logger.msg(Arc::ERROR, "Cannot create argument of the constructor");
            if (PyErr_Occurred() != NULL) {
                PyErr_Print();
            }
            return;
        }
        
        // create instance of class
        object = PyObject_CallObject(klass, arg);
        if (object == NULL) {
            logger.msg(Arc::ERROR, "Cannot create instance of python class");
            if (PyErr_Occurred() != NULL) {
                PyErr_Print();
            }
            return;
        }
        Py_DECREF(arg);

    } else {
        logger.msg(Arc::ERROR, "%s is not an object", class_name);
        return;
    }

    // Get the class 
    arc_msg_klass = PyDict_GetItemString(arc_dict, "SOAPMessage");
    if (arc_msg_klass == NULL) {
        logger.msg(Arc::ERROR, "Cannot find arc Message class");
        if (PyErr_Occurred() != NULL) {
            PyErr_Print();
        }
        return;
    }
    
    // check is it really a class
    if (!PyCallable_Check(klass)) {
        logger.msg(Arc::ERROR, "Message klass is not an object");
        return;
    }
    tstate = PyGILState_GetThisThreadState();
    PyEval_ReleaseThread(tstate);

    logger.msg(Arc::DEBUG, "Python Wrapper constructor called");
}

Service_PythonWrapper::~Service_PythonWrapper(void) 
{
    // Finish the Python Interpreter
    python_service_counter--;
    if (python_service_counter == 0) {
        PyEval_AcquireThread(tstate);
        Py_Finalize();
    }
    logger.msg(Arc::DEBUG, "Python Wrapper destructor called (%d)", python_service_counter);
}

Arc::MCC_Status Service_PythonWrapper::make_fault(Arc::Message& outmsg) 
{
    Arc::PayloadSOAP* outpayload = new Arc::PayloadSOAP(Arc::NS(),true);
    Arc::SOAPFault* fault = outpayload->Fault();
    if(fault) {
        fault->Code(Arc::SOAPFault::Sender);
        fault->Reason("Failed processing request");
    };
    outmsg.Payload(outpayload);
    return Arc::MCC_Status();
}

/*
Arc::MCC_Status Service_PythonWrapper::python_error(const char *str) {
    return Arc::MCC_Status(Arc::GENERIC_ERROR);
}*/

Arc::MCC_Status Service_PythonWrapper::process(Arc::Message& inmsg, Arc::Message& outmsg) 
{
    PyObject *py_status = NULL;
    PyObject *py_inmsg = NULL;
    PyObject *py_outmsg = NULL;
    PyObject *arg = NULL;
   
    logger.msg(Arc::DEBUG, "Python wrapper process called");
    
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();
    logger.msg(Arc::DEBUG, "Python interpreter locked");
    
    // Convert in and out messages to SOAP messages
    Arc::SOAPMessage *inmsg_ptr = NULL;
    Arc::SOAPMessage *outmsg_ptr = NULL;
    try {
        inmsg_ptr = new Arc::SOAPMessage(inmsg);
        outmsg_ptr = new Arc::SOAPMessage(outmsg);
    } catch(std::exception& e) { };
    if(!inmsg_ptr) {
        logger.msg(Arc::ERROR, "input is not SOAP");
        
        PyGILState_Release(gstate);
        return make_fault(outmsg);
    };
    if(!outmsg_ptr) {
        logger.msg(Arc::ERROR, "output is not SOAP");
        
        PyGILState_Release(gstate);
        return make_fault(outmsg);
    };

    // Convert incomming and outcoming messages to python objects
    arg = Py_BuildValue("(l)", (long int)inmsg_ptr);
    if (arg == NULL) {
        logger.msg(Arc::ERROR, "Cannot create inmsg argument");
        if (PyErr_Occurred() != NULL) {
            PyErr_Print();
        }
        
        PyGILState_Release(gstate);
        return Arc::MCC_Status(Arc::GENERIC_ERROR);
    }

    py_inmsg = PyObject_CallObject(arc_msg_klass, arg);
    if (py_inmsg == NULL) {
        logger.msg(Arc::ERROR, "Cannot convert inmsg to python object");
        if (PyErr_Occurred() != NULL) {
            PyErr_Print();
        }
        Py_DECREF(arg);
        
        PyGILState_Release(gstate);
        return Arc::MCC_Status(Arc::GENERIC_ERROR);
    }
    Py_DECREF(arg); delete inmsg_ptr;

    arg = Py_BuildValue("(l)", (long int)outmsg_ptr);
    if (arg == NULL) {
        logger.msg(Arc::ERROR, "Cannot create outmsg argument");
        if (PyErr_Occurred() != NULL) {
            PyErr_Print();
        }
        
        PyGILState_Release(gstate);
        return Arc::MCC_Status(Arc::GENERIC_ERROR);
    }

    py_outmsg = PyObject_CallObject(arc_msg_klass, arg);
    if (py_outmsg == NULL) {
        logger.msg(Arc::ERROR, "Cannot convert outmsg to python object");
        if (PyErr_Occurred() != NULL) {
            PyErr_Print();
        }
        Py_DECREF(arg);
        
        PyGILState_Release(gstate);
        return Arc::MCC_Status(Arc::GENERIC_ERROR);
    }
    Py_DECREF(arg); delete outmsg_ptr;
    
    // Call the process method
    py_status = PyObject_CallMethod(object, "process", "(OO)", 
                                    py_inmsg, py_outmsg);
    if (py_status == NULL) {
        if (PyErr_Occurred() != NULL) {
                PyErr_Print();
        }
        Py_DECREF(py_inmsg);
        Py_DECREF(py_outmsg);
        
        PyGILState_Release(gstate);
        return Arc::MCC_Status(Arc::GENERIC_ERROR);
    }
    
    MCC_Status *status_ptr2 = (MCC_Status *)extract_swig_wrappered_pointer(py_status);
    Arc::MCC_Status status(*status_ptr2);
    std::string str = (std::string)status;
    // std::cout << "status: " << str << std::endl;   
    SOAPMessage *outmsg_ptr2 = (SOAPMessage *)extract_swig_wrappered_pointer(py_outmsg);
    std::string xml;
    SOAPEnvelope *p = outmsg_ptr2->Payload();
    p->GetXML(xml);
    // std::cout << "XML: " << xml << std::endl; 

    Arc::PayloadSOAP *pl = new Arc::PayloadSOAP(*(outmsg_ptr2->Payload()));
    // pl->GetXML(xml);   
    Py_DECREF(py_outmsg);
    Py_DECREF(py_inmsg);
    
    outmsg.Payload(pl);
    
    PyGILState_Release(gstate);
    return status;
}

} // namespace Arc
