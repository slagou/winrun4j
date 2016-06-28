/*******************************************************************************
 * This program and the accompanying materials
 * are made available under the terms of the Common Public License v1.0
 * which accompanies this distribution, and is available at 
 * http://www.eclipse.org/legal/cpl-v10.html
 * 
 * Contributors:
 *     Peter Smith
 *******************************************************************************/

#ifndef SERVICE_H
#define SERVICE_H

#include "../common/Runtime.h"
#include "../common/INI.h"

class Service
{
public:
	static int Register(dictionary* ini);
	static int Unregister(dictionary* ini);
	static int Run(HINSTANCE hInstance, dictionary* ini, int argc, char* argv[]);
	static void Shutdown(int exitCode);

	// Internal methods
	static int Control(DWORD opCode);
	static int Main(int argc, char* argv[]);

private:
	static int Initialise(dictionary* ini);
};

struct ServiceStartThreadParams {
	int argc;
	char** argv;
};

#endif // SERVICE_H