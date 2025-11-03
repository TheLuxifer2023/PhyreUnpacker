#include "FontExtractorCLI.h"
#include <iostream>
#include <exception>
#include <fstream>

int main(int argc, char* argv[])
{
    // Create debug log file
    std::ofstream debugLog("main_debug.log");
    debugLog << "PhyreUnpacker started" << std::endl;
    debugLog << "argc: " << argc << std::endl;
    for (int i = 0; i < argc; ++i) {
        debugLog << "argv[" << i << "]: " << argv[i] << std::endl;
    }
    
    try
    {
        debugLog << "Creating CLI object..." << std::endl;
        PhyreUnpacker::FontExtractorCLI cli;
        
        debugLog << "Parsing arguments..." << std::endl;
        if (!cli.parseArguments(argc, argv))
        {
            std::cerr << "Failed to parse arguments" << std::endl;
            debugLog << "Failed to parse arguments" << std::endl;
            debugLog.close();
            return 1;
        }
        
        debugLog << "Running CLI..." << std::endl;
        if (!cli.run())
        {
            std::cerr << "Execution failed" << std::endl;
            debugLog << "Execution failed" << std::endl;
            debugLog.close();
            return 1;
        }
        
        debugLog << "Success!" << std::endl;
        debugLog.close();
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        debugLog << "Exception: " << e.what() << std::endl;
        debugLog.close();
        return 1;
    }
    catch (...)
    {
        std::cerr << "Unknown exception occurred" << std::endl;
        debugLog << "Unknown exception occurred" << std::endl;
        debugLog.close();
        return 1;
    }
}
