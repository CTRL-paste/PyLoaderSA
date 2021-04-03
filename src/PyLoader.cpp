#include "PyLoader.h"
#include "sdk/PyCHud.h"
#include "sdk/PyCommon.h"
#include <frameobject.h>

void PyLoader::PluginThread(void* param)
{
	AllocConsole();
	freopen("CONOUT$", "w", stdout);
	std::cout << "Starting PyLoader" << std::endl;

    HANDLE dir;
    WIN32_FIND_DATA file_data;

    if ((dir = FindFirstFile("./PyLoader/*.py", &file_data)) == INVALID_HANDLE_VALUE)
        return;

    PyImport_AppendInittab("hud", &PyCHud::Init);
    PyImport_AppendInittab("common", &PyCommon::Init);

    Py_Initialize();
    PyEval_InitThreads();
    PyEval_ReleaseLock();

    do {
        std::string *file_name = new std::string("PyLoader." + std::string(file_data.cFileName));
        
        file_name->erase(file_name->end()-3,file_name->end());
        CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)&PyLoader::ExecuteScript, file_name, NULL, NULL);
        Sleep(100);

    } while (FindNextFile(dir, &file_data));

    while (true)
    {   
        Sleep(10000);
    }
    // TODO: Handle sending a terminate message to py script
    Py_Finalize();
}

void PyLoader::PrintError()
{
    PyObject* excType, *excValue, *excTraceback;
    PyErr_Fetch(&excType, &excValue, &excTraceback);
    PyErr_NormalizeException(&excType, &excValue, &excTraceback);

    PyTracebackObject* traceback = (PyTracebackObject*)excTraceback;

    std::cout << "Error occured, traceback" << std::endl;
    while (traceback != NULL && traceback->tb_frame != NULL)
    {
        int line = PyCode_Addr2Line(traceback->tb_frame->f_code, traceback->tb_frame->f_lasti);
        const char *filename = PyUnicode_AsUTF8(traceback->tb_frame->f_code->co_filename);
        const char *funcname = PyUnicode_AsUTF8(traceback->tb_frame->f_code->co_name);
        std::cout << filename << ", Line " << line << ", " << funcname << "()" <<  std::endl;  
        traceback = traceback->tb_next;
    }
    Py_DECREF(excType);
    Py_DECREF(excValue);
    Py_DECREF(excTraceback);
}

int PyLoader::ExecuteScript(std::string *file_name)
{
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();
    std::cout << "Loading script " << *file_name << std::endl;

    PyObject *pName, *pModule, *pFunc;
    PyObject *pValue;

    pName = PyUnicode_FromString(file_name->c_str());

    pModule = PyImport_Import(pName);
    Py_DECREF(pName);

    if (pModule != NULL) {
        pFunc = PyObject_GetAttrString(pModule, MAIN_FUNC_NAME);

        if (pFunc && PyCallable_Check(pFunc)) {
            
            pValue = PyObject_CallObject(pFunc, nullptr);
            if (pValue != NULL) {
                std::cout << "Result of call: " << PyLong_AsLong(pValue) << std::endl;
                Py_DECREF(pValue);
            }
            else {
                PrintError();
                PyErr_Restore(NULL,NULL,NULL);
                Py_DECREF(pFunc);
                Py_DECREF(pModule);
                return 1;
            }
        }
        else 
        {
            std::cout << "Couldn't find " <<  MAIN_FUNC_NAME << "()" << std::endl;
        }
        Py_XDECREF(pFunc);
        Py_DECREF(pModule);
    }
    else {
        std::cout << "Failed to load " << file_name << std::endl;
        return 1;
    }

    PyGILState_Release(gstate);
    delete file_name;
    return 0;
}