#pragma once

#include "waste_analyzer.hpp"
#include <filesystem>
#include <string>

namespace storage_optimizer {

class Reporter {
public:
    static void print_cli_summary(const WasteAnalysisReport& report);
    
    static bool export_json(const WasteAnalysisReport& report, const std::filesystem::path& output_path);
    
    static bool generate_cleanup_script_ps1(
        const WasteAnalysisReport& report,
        const std::filesystem::path& script_path,
        const std::filesystem::path& quarantine_dir = ""
    );

    static bool generate_cleanup_script_bat(
        const WasteAnalysisReport& report,
        const std::filesystem::path& script_path,
        const std::filesystem::path& quarantine_dir = ""
    );

private:
    static std::string escape_json_string(const std::string& input);
};

} // namespace storage_optimizer
