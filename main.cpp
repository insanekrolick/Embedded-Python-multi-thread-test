#pragma once

#include <windows.h>
#include <Python.h>

#include <iostream>
#include <fstream>
#include <sstream>

#include <thread>
#include <chrono>
#include <string>
#include <map>

using namespace std::chrono_literals;

// ------------------------------------------------------------------------------------------------
class TThreadHolder {
public:
	std::string threadName;
	PyThreadState* pyThreadState;
	std::thread thread;
	std::shared_ptr<bool> stopSwitch;
};

class PythonThreadContext {
public:
	std::string threadName;
	std::string pythonFilePath;
	PyInterpreterState* interp;
	std::shared_ptr<bool> stopSwitch;
};

struct ThreadParams {
	int threadNum;
	std::string threadName;
	std::string fileName;
	PyInterpreterState* interp;
};

//---------------------------------------------------------------------------------------------------------------------------
// FUNCTION USED IN A NEW C++ THREAD TO EXECUTE PYTHON CODE
//---------------------------------------------------------------------------------------------------------------------------
void pyExecutorFunc(std::shared_ptr<PythonThreadContext> spPythonHandlerContext)
{
	bool bContinue = true;
	FILE* file;

	// Create thread state from PyInterpreterState
	PyThreadState* _state = PyThreadState_New(spPythonHandlerContext->interp);

	// Gil Lock
	PyEval_AcquireThread(_state);

	// Swap TState
	PyThreadState* _swap = PyThreadState_Swap(_state);

	std::string filename("pyemb1.py");

	while (!*spPythonHandlerContext->stopSwitch) {
		file = fopen(filename.c_str(), "r");
		// Execute the python
		if (file != NULL) {
			PyRun_SimpleFile(file, filename.c_str());
			PyErr_Print();
		}
		fclose(file);
	}

	// Swap TState Back
	PyThreadState_Swap(_swap);

	// Gil Un-Lock
	PyEval_ReleaseThread(_state);

	// Clear this thread state before close the thread
	PyThreadState_Clear(_state);
	PyThreadState_Delete(_state);
}

//---------------------------------------------------------------------------------------------------------------------------
// Class to handle python executors
//---------------------------------------------------------------------------------------------------------------------------
class PythonRunner {
public:
	int Inintialize()
	{
		int errorCode = 0;
		bool bContinue = true;

		// Initialize python
		Py_InitializeEx(1);
		PyEval_InitThreads();

		this->mainThreadState = PyEval_SaveThread();// Get State and Release GIL
		return errorCode;
	}

	// ---------------------------------------------------------
	int addExecutor(std::string& threadName)
	{
		PyEval_RestoreThread(this->mainThreadState);

		// Create new sub-interpreter
		this->threadCollection[threadName] = TThreadHolder();
		this->threadCollection[threadName].threadName = threadName;
		this->threadCollection[threadName].pyThreadState = Py_NewInterpreter();
		this->threadCollection[threadName].stopSwitch = std::make_shared<bool>(false);

		std::shared_ptr<PythonThreadContext> threadParams = std::make_shared<PythonThreadContext>();
		threadParams->interp = this->threadCollection[threadName].pyThreadState->interp;
		threadParams->pythonFilePath = "pyemb1.py";
		threadParams->threadName = threadName;
		threadParams->stopSwitch = this->threadCollection[threadName].stopSwitch;

		// Start new thread with Py executor:
		this->threadCollection[threadName].thread = std::thread(pyExecutorFunc, threadParams);
		this->mainThreadState = PyEval_SaveThread();

		return 0;
	}

	// ---------------------------------------------------------
	int stopExecutor(std::string& threadName)
	{
		auto itr = this->threadCollection.find(threadName);
		if (itr == this->threadCollection.end()) {
			return 1;
		}
		*itr->second.stopSwitch = true;
		if (itr->second.thread.joinable()) {
			itr->second.thread.join();
		}

		PyEval_RestoreThread(itr->second.pyThreadState);
		Py_EndInterpreter(itr->second.pyThreadState);

		PyThreadState_Swap(this->mainThreadState);
		std::this_thread::sleep_for(4s);
		PyEval_SaveThread();//Release GIL
		this->threadCollection.erase(itr);
		return 0;
	}
private:
	std::map<std::string, TThreadHolder> threadCollection;
	PyThreadState* mainThreadState;
};

// ------------------------------------------------------------------------------------------------

int main()
{
	// Init sytem
	int systemState = 0;
	int counter = 0;
	bool bContinue = true;
	bool created = false;
	
	std::chrono::system_clock::time_point prevIterTimepoint;
	std::chrono::system_clock::time_point lastIterTimepoint;
	auto startPhaseTimepoint = std::chrono::system_clock::now();
	std::chrono::system_clock::time_point lastThreadCreationTimepoint;

	auto mainLoopTick = std::chrono::milliseconds(50);
	auto stopDelayPeriod = std::chrono::seconds(60);
	auto mainThreadTick = std::chrono::seconds(20);

	// Initialize python
	PythonRunner PythonRunnerObj;
	auto errCode = PythonRunnerObj.Inintialize();

	// Main loop
	while (bContinue) {
		prevIterTimepoint = lastIterTimepoint;
		lastIterTimepoint = std::chrono::system_clock::now();
		auto currTime = std::chrono::system_clock::now();

		switch (systemState) {
		case 0: {

			if (/*time to create new thread*/ currTime - lastThreadCreationTimepoint > mainThreadTick && !created) {
				std::shared_ptr<PythonThreadContext> thrContext = std::make_shared<PythonThreadContext>();
				thrContext->threadName = "Tread_" + std::to_string(counter);

				std::cout << "Let`s CREATE " << thrContext->threadName << std::endl;

				PythonRunnerObj.addExecutor(thrContext->threadName);
				lastThreadCreationTimepoint = currTime;
				counter++;
				if (counter > 1) {
					created = true;
				}
			}
			auto currTime = std::chrono::system_clock::now();
			if (currTime >= startPhaseTimepoint + stopDelayPeriod) {
				startPhaseTimepoint = currTime;
				systemState = 1;
				std::cout << "Let`s delete" << std::endl;
				counter = 0;
			}
			break;
		}
		case 1: {

			if (/*time to create new thread*/ currTime - lastThreadCreationTimepoint > mainThreadTick) {
				std::string threadName = "Tread_" + std::to_string(counter);
				std::cout << "Let`s DELETE " << threadName << std::endl;
				int done = PythonRunnerObj.stopExecutor(threadName);
				counter++;
				if (done) {
					systemState = 0;
					counter = 0;
					created = false;
				}
				lastThreadCreationTimepoint = currTime;
			}
			currTime = std::chrono::system_clock::now();
			break;

		}
		}

		std::this_thread::sleep_until(lastIterTimepoint + mainLoopTick);

	} //while (bContinue)

	Py_Finalize();
	return 0;
}
