#include <iostream>
#include <filesystem>

#include "TestFramework.h"
#include "Tests/TestObject.h"
#include "Tests/TestWriterReader.h"
#include "Tests/TestStream.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <output_directory>\n";
        std::cerr << "  e.g.: " << argv[0] << " /tmp/otn_tests\n";
        return 1;
    }

    std::filesystem::path basePath = argv[1];

    std::error_code ec;
    std::filesystem::create_directories(basePath, ec);
    if (ec) {
        std::cerr << "Failed to create directory '"
                  << basePath << "': " << ec.message() << "\n";
        return 1;
    }

    std::string title = "OTN Unit Tests - output dir: ";
    std::string pathStr = basePath.u8string();
    std::cout << title << "\"" << pathStr  << "\"" << "\n";
    std::cout << std::string(title.size() + basePath.u8string().size() + 2, '=') << "\n";

    RunObjectTests(basePath);
    RunWriterReaderTests(basePath);
    RunStreamTests(basePath);

    PrintSummary();
    std::cin.get();
    return (g_failed == 0) ? 0 : 1;
}
