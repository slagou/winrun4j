/*******************************************************************************
 * This program and the accompanying materials
 * are made available under the terms of the Common Public License v1.0
 * which accompanies this distribution, and is available at 
 * http://www.eclipse.org/legal/cpl-v10.html
 * 
 * Contributors:
 *     Peter Smith
 *******************************************************************************/

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <jni.h>
#include <string>

#include "INI.h"
#include "JNI.h"
#include "Icon.h"
#include "Log.h"
#include "VM.h"
#include "Classpath.h"

// Internal keys
#define MODULE_NAME "WinRun4J:ModuleName"
#define MODULE_INI "WinRun4J:ModuleIni"
#define MODULE_BASE "WinRun4J:ModuleBaseName"
#define MODULE_DIR "WinRun4J:ModuleDir"

// Ini keys
#define WORKING_DIR ":working.directory"
#define MAIN_CLASS ":main.class"
#define LOG_FILE ":log"
#define LOG_LEVEL ":log.level"
#define CLASS_PATH ":classpath"
#define VM_ARG ":vmarg"
#define PROG_ARG ":arg"

using namespace std;

void SetWorkingDirectory(dictionary* ini)
{
	char* dir = iniparser_getstr(ini, WORKING_DIR);
	if(dir != NULL) {
		// First set the current directory to the module directory
		SetCurrentDirectory(iniparser_getstr(ini, MODULE_DIR));

		// Now set working directory to specified (this allows for a relative working directory)
		SetCurrentDirectory(dir);
	} else {
		Log::Info("Working directory not set\n");
	}
}


void GetNumberedKeysFromIni(dictionary* ini, TCHAR* keyName, TCHAR** entries, int& index)
{
	int i = 0;
	TCHAR entryName[MAX_PATH];
	while(true) {
		sprintf_s(entryName, sizeof(entryName), "%s.%d", keyName, i+1);
		TCHAR* entry = iniparser_getstr(ini, entryName);
		if(entry != NULL) {
			entries[index++] = _strdup(entry);
		}
		i++;
		if(i > 10 && entry == NULL) {
			break;
		}
	}
	entries[index] = NULL;
}

/* The ini filename is in the same directory as the executable and called the same (except with ini at the end). */
dictionary* LoadIniFile()
{
	TCHAR filename[MAX_PATH], inifile[MAX_PATH], filedir[MAX_PATH];
	GetModuleFileName(NULL, filename, sizeof(filename));
	strcpy_s(inifile, sizeof(inifile), filename);
	strcpy_s(filedir, sizeof(filedir), filename);
	int len = strlen(inifile);
	// It is assumed the executable ends with "exe"
	inifile[len - 1] = 'i';
	inifile[len - 2] = 'n';
	inifile[len - 3] = 'i';
	dictionary* ini = iniparser_load(inifile);
	iniparser_setstr(ini, MODULE_NAME, filename);
	iniparser_setstr(ini, MODULE_INI, inifile);
	Log::Info("Module Name: %s\n", filename);
	Log::Info("Module INI: %s\n", inifile);

	// strip off filename to get module directory
	for(int i = len - 1; i >= 0; i--) {
		if(filedir[i] == '\\') {
			filedir[i] = 0;
			break;
		}
	}
	iniparser_setstr(ini, MODULE_DIR, filedir);
	Log::Info("Module Dir: %s\n", filedir);

	// Fix main class - ie. replace x.y.z with x/y/z for use in jni
	char* mainClass = iniparser_getstr(ini, MAIN_CLASS);
	if(mainClass != NULL) {
		len = strlen(mainClass);
		for(int i = 0; i < len; i++) {
			if(mainClass[i] == '.') {
				mainClass[i] = '/';
			}
		}
	}
	if(mainClass == NULL) {
		Log::Error("ERROR: no main class specified\n");
	} else {
		Log::Info("Main Class: %s\n", mainClass);
	}

	return ini;
}

bool StrTrimInChars(LPSTR trimChars, char c)
{
	unsigned int len = strlen(trimChars);
	for(unsigned int i = 0; i < len; i++) {
		if(c == trimChars[i]) {
			return true;
		}
	}
	return false;
}

void StrTrim(LPSTR str, LPSTR trimChars)
{
	unsigned int start = 0;
	unsigned int end = strlen(str) - 1;
	for(int i = 0; i < end; i++) {
		char c = str[i];
		if(!StrTrimInChars(trimChars, c)) {
			start = i;
			break;
		}
	}
	for(int i = end; i >= 0; i--) {
		char c = str[i];
		if(!StrTrimInChars(trimChars, c)) {
			end = i;
			break;
		}
	}
	if(start != 0 || end != strlen(str) - 1) {
		int k = 0;
		for(int i = start; i <= end; i++, k++) {
			str[k] = str[i];
		}
		str[k] = 0;
	}
}

void ParseCommandLine(LPSTR lpCmdLine, TCHAR** args, int& count)
{
	StrTrim(lpCmdLine, " ");
	int len = strlen(lpCmdLine);
	if(len == 0) {
		return;
	}

	int start = 0;
	bool quote = false;
	TCHAR arg[4096];
	for(int i = 0; i < len; i++) {
		char c = lpCmdLine[i];
		if(c == '\"') {
			quote = !quote;
		} else if(!quote && c == ' ') {
			int k = 0;
			for(int j = start; j < i; j++, k++) {
				arg[k] = lpCmdLine[j];
			}
			arg[k] = 0;
			args[count] = strdup(arg);
			StrTrim(args[count], " ");
			StrTrim(args[count], "\"");
			count++;
			start = i;
		}
	}

	// Add the last one
	int k = 0;
	for(int j = start; j < len; j++, k++) {
		arg[k] = lpCmdLine[j];
	}
	arg[k] = 0;
	args[count] = _strdup(arg);
	StrTrim(args[count], " ");
	StrTrim(args[count], "\"");
	count++;
}

int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	// Initialise the logger using std streams
	Log::Init(hInstance, NULL, NULL);

	// Check for seticon util request
	if(strncmp(lpCmdLine, "--seticon", 9) == 0) {
		Icon::SetExeIcon(lpCmdLine);
		Log::Close();
		return 0;
	}

	dictionary* ini = LoadIniFile();
	if(ini == NULL) {
		MessageBox(NULL, "Failed to find or load ini file.", "Startup Error", 0);
		Log::Close();
		return 1;
	}

	// Now initialise the logger using std streams + specified log dir
	Log::Init(hInstance, iniparser_getstr(ini, LOG_FILE), iniparser_getstr(ini, LOG_LEVEL));

	// Attempt to find an appropriate java VM
	char* vmlibrary = VM::FindJavaVMLibrary(ini);
	if(!vmlibrary) {
		MessageBox(NULL, "Failed to find Java VM.", "Startup Error", 0);
		Log::Close();
		return 1;
	}

	// Set the current working directory if specified
	SetWorkingDirectory(ini);

	// Collect the VM args from the INI file
	TCHAR *vmargs[MAX_PATH];
	int vmargsCount = 0;
	GetNumberedKeysFromIni(ini, VM_ARG, vmargs, vmargsCount);

	// Build up the classpath and add to vm args
	Classpath::BuildClassPath(ini, vmargs, vmargsCount);

	// Extract the specific VM args
	VM::ExtractSpecificVMArgs(ini, vmargs, vmargsCount);

	// Log the VM args
	for(int i = 0; i < vmargsCount; i++) {
		Log::Info("vmarg.%d=%s\n", i, vmargs[i]);
	}

	// Collect the program arguments from the INI file
	TCHAR *progargs[MAX_PATH];
	int progargsCount = 0;
	GetNumberedKeysFromIni(ini, PROG_ARG, progargs, progargsCount);

	// Add the args from commandline
	ParseCommandLine(lpCmdLine, progargs, progargsCount);

	// Log the commandline args
	for(int i = 0; i < progargsCount; i++) {
		Log::Info("arg.%d=%s\n", i, progargs[i]);
	}

	// Make sure there is a NULL at the end of the args
	vmargs[vmargsCount] = NULL;
	progargs[progargsCount] = NULL;

	// Fire up the VM
	startJavaVM(vmlibrary, vmargs, iniparser_getstr(ini, MAIN_CLASS), progargs);
	
	// Free vm args
	for(int i = 0; i < vmargsCount; i++) {
		free(vmargs[i]);
	}

	// Free program args
	for(int i = 0; i < progargsCount; i++) {
		free(progargs[i]);
	}

	// Free ini file
	iniparser_freedict(ini);

	// Close VM (This will block until all non-daemon java threads finish).
	int result = cleanupVM();

	// Close the log
	Log::Close();

	return result;
}
