#include "FontExtractorCLI.h"
#include <iostream>
#include <exception>

int main(int argc, char* argv[])
{
    try
    {
        PhyreUnpacker::FontExtractorCLI cli;
        
        if (!cli.parseArguments(argc, argv))
        {
            std::cerr << "Failed to parse arguments" << std::endl;
            return 1;
        }
        
        if (!cli.run())
        {
            std::cerr << "Execution failed" << std::endl;
            return 1;
        }
        
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << "Unknown exception occurred" << std::endl;
        return 1;
    }
}