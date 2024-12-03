/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_APPLICATION_HXX
#define ERP_PROCESSING_CONTEXT_APPLICATION_HXX

#include <functional>
#include <string>

class Application {
public:
	static int MainFn(const int argc, const char* argv[], const std::string& name, const std::function<int()>& entryPoint);
};

#endif
