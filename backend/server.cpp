// server.cpp
#define _WIN32_WINNT 0x0A00

// Winsock must be included before windows.h on Windows
#include <winsock2.h>
#include <windows.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <regex>
#include <filesystem>
#include <cerrno>

// cpp-httplib and nlohmann::json (single‑header libraries)
#include "httplib.h"
#include "json.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

// ----- Configuration -------------------------------------------------------
const std::string FRONTEND_DIR = "frontend";
const std::string DATA_DIR = "backend/data";

// ----- Helper functions ----------------------------------------------------
// Validate dataset name (alphanumeric + underscore)
bool is_valid_name(const std::string& name) {
    static const std::regex re("^[A-Za-z0-9_]+$");
    return std::regex_match(name, re);
}

std::string csv_path(const std::string& name) { return DATA_DIR + "/" + name + ".csv"; }
std::string meta_path(const std::string& name) { return DATA_DIR + "/" + name + "_meta.json"; }

// List all CSV datasets in DATA_DIR
std::vector<std::string> list_datasets() {
    std::vector<std::string> datasets;
    for (const auto& entry : fs::directory_iterator(DATA_DIR)) {
        if (entry.is_regular_file() && entry.path().extension() == ".csv") {
            datasets.push_back(entry.path().stem().string());
        }
    }
    return datasets;
}

// Load column metadata – fall back to CSV header if meta file missing
json load_meta(const std::string& name) {
    std::string mpath = meta_path(name);
    if (fs::exists(mpath)) {
        std::ifstream fin(mpath);
        json j; fin >> j; return j;
    }
    // Fallback: infer from CSV header (type = Text)
    std::ifstream fin(csv_path(name));
    std::string header;
    if (std::getline(fin, header)) {
        std::vector<std::string> cols;
        std::stringstream ss(header);
        std::string col;
        while (std::getline(ss, col, ',')) cols.push_back(col);
        json j; j["columns"] = json::array();
        for (auto& c : cols) j["columns"].push_back({{"name", c}, {"type", "Text"}});
        return j;
    }
    return json::object();
}

// Append a record to the CSV file (order follows meta)
bool append_record(const std::string& name, const json& record, std::string& err) {
    std::ofstream fout(csv_path(name), std::ios::app);
    if (!fout) { err = "Cannot open CSV for appending"; return false; }
    json meta = load_meta(name);
    const auto& cols = meta["columns"];
    bool first = true;
    for (const auto& col : cols) {
        if (!first) fout << ','; else first = false;
        std::string key = col["name"];
        std::string value = record.value(key, "");
        bool needQuote = value.find(',') != std::string::npos || value.find('"') != std::string::npos;
        if (needQuote) fout << '"';
        for (char ch : value) {
            if (ch == '"') fout << "\"\""; // escape double quotes for CSV
            else fout << ch;
        }
        if (needQuote) fout << '"';
    }
    fout << "\n";
    return true;
}

// Load all records (excluding header)
json load_records(const std::string& name) {
    json result;
    std::ifstream fin(csv_path(name));
    std::string line;
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> rows;
    bool first = true;
    while (std::getline(fin, line)) {
        std::stringstream ss(line);
        std::vector<std::string> fields;
        std::string cell;
        while (std::getline(ss, cell, ',')) {
            // Strip surrounding quotes and un‑escape double quotes
            if (cell.size() >= 2 && cell.front() == '"' && cell.back() == '"') {
                cell = cell.substr(1, cell.size() - 2);
                std::string fixed;
                for (size_t i = 0; i < cell.size(); ++i) {
                    if (cell[i] == '"' && i + 1 < cell.size() && cell[i+1] == '"') { fixed.push_back('"'); ++i; }
                    else fixed.push_back(cell[i]);
                }
                cell = fixed;
            }
            fields.push_back(cell);
        }
        if (first) { columns = fields; first = false; }
        else rows.push_back(fields);
    }
    result["columns"] = columns;
    result["records"] = rows;
    return result;
}

// Delete a record by zero‑based index (skip header)

// Delete a whole dataset (CSV + optional meta) safely
bool delete_dataset(const std::string& name, std::string& err) {
    if (!is_valid_name(name)) { err = "Invalid dataset name"; return false; }
    std::string csv = csv_path(name);
    if (!fs::exists(csv)) { err = "Dataset file not found"; return false; }
    // attempt to remove CSV
    std::error_code ec;
    bool csv_removed = fs::remove(csv, ec);
    if (!csv_removed) { err = "Failed to delete CSV: " + ec.message(); return false; }
    // optional meta file – ignore errors if it does not exist
    std::string meta = meta_path(name);
    if (fs::exists(meta)) {
        fs::remove(meta, ec); // ignore result, not critical
    }
    return true;
}
bool delete_record(const std::string& name, size_t idx, std::string& err) {
    std::ifstream fin(csv_path(name));
    if (!fin) { err = "Cannot read CSV"; return false; }
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(fin, line)) lines.push_back(line);
    if (idx + 1 >= lines.size()) { err = "Index out of range"; return false; }
    lines.erase(lines.begin() + idx + 1); // +1 skips header
    std::ofstream fout(csv_path(name), std::ios::trunc);
    if (!fout) { err = "Cannot write CSV"; return false; }
    for (const auto& l : lines) fout << l << "\n";
    return true;
}

// ----- Static file serving ------------------------------------------------
void serve_static(httplib::Server& svr) {
    svr.Get("/", [&](const httplib::Request&, httplib::Response& res) {
        std::ifstream fin(FRONTEND_DIR + "/index.html", std::ios::binary);
        if (fin) {
            std::ostringstream ss; ss << fin.rdbuf();
            res.set_content(ss.str(), "text/html");
        } else res.status = 404;
    });

    svr.Get(R"(/(.*\.(?:css|js|html)))", [&](const httplib::Request& req, httplib::Response& res) {
        std::string rel = req.matches[1];
        std::string path = FRONTEND_DIR + "/" + rel;
        if (!fs::exists(path) || fs::is_directory(path)) { res.status = 404; return; }
        std::ifstream fin(path, std::ios::binary);
        std::ostringstream ss; ss << fin.rdbuf();
        // C++17 does not have std::string::ends_with, use manual check
        auto has_suffix = [&](const std::string& s, const std::string& suffix) {
            return s.size() >= suffix.size() &&
                   s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
        };
        if (has_suffix(path, ".css"))      res.set_content(ss.str(), "text/css");
        else if (has_suffix(path, ".js")) res.set_content(ss.str(), "application/javascript");
        else if (has_suffix(path, ".html")) res.set_content(ss.str(), "text/html");
        else                                 res.set_content(ss.str(), "text/plain");
    });
}

// ----- Main ---------------------------------------------------------------
int main() {
    try {

    // Ensure the data directory exists
    std::ofstream log("backend/log.txt", std::ios::app);
    log << "Server starting...\n";
    log.flush();
    fs::create_directories(DATA_DIR);

    httplib::Server svr;

    // ---- API routes ------------------------------------------------------
    svr.Get("/api/datasets", [&](const httplib::Request&, httplib::Response& res) {
        json j; j["datasets"] = list_datasets();
        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/api/dataset/meta", [&](const httplib::Request& req, httplib::Response& res) {
        auto it = req.params.find("name");
        if (it == req.params.end() || !is_valid_name(it->second)) { res.status = 400; return; }
        json meta = load_meta(it->second);
        res.set_content(meta.dump(), "application/json");
    });

    svr.Post("/api/dataset/create", [&](const httplib::Request& req, httplib::Response& res) {
        json body = json::parse(req.body);
        std::string name = body.value("name", "");
        if (!is_valid_name(name)) {
            res.status = 400;
            res.set_content(R"({"success":false,"message":"Invalid name"})", "application/json");
            return;
        }
        std::string cpath = csv_path(name);
        if (fs::exists(cpath)) {
            res.status = 400;
            res.set_content(R"({"success":false,"message":"Dataset exists"})", "application/json");
            return;
        }
        // Create CSV header
        std::ofstream fout(cpath);
        const auto& cols = body["columns"]; // expected array of {name,type}
        for (size_t i = 0; i < cols.size(); ++i) {
            fout << cols[i]["name"].get<std::string>();
            if (i + 1 < cols.size()) fout << ',';
        }
        fout << "\n";
        // Save meta file
        std::ofstream mout(meta_path(name));
        mout << body.dump(4);
        res.set_content(R"({"success":true})", "application/json");
    });

    svr.Post("/api/record/add", [&](const httplib::Request& req, httplib::Response& res) {
        json body = json::parse(req.body);
        std::string dataset = body.value("dataset", "");
        json record = body["record"];
        std::string err;
        if (!is_valid_name(dataset) || !fs::exists(csv_path(dataset))) {
            res.status = 400;
            res.set_content(R"({"success":false,"message":"Dataset not found"})", "application/json");
            return;
        }
        if (append_record(dataset, record, err))
            res.set_content(R"({"success":true})", "application/json");
        else {
            res.status = 500;
            res.set_content("{\"success\":false,\"message\":\"" + err + "\"}", "application/json");
        }
    });

    svr.Get("/api/records", [&](const httplib::Request& req, httplib::Response& res) {
        auto it = req.params.find("dataset");
        if (it == req.params.end() || !is_valid_name(it->second) || !fs::exists(csv_path(it->second))) {
            res.status = 400; return;
        }
        json data = load_records(it->second);
        res.set_content(data.dump(), "application/json");
    });

    // Delete a whole dataset (CSV + optional meta)
    svr.Post("/api/dataset/delete", [&](const httplib::Request& req, httplib::Response& res) {
        json body = json::parse(req.body);
        std::string name = body.value("name", "");
        std::string err;
        if (!is_valid_name(name)) {
            res.status = 400;
            res.set_content(R"({"success":false,"message":"Invalid dataset name"})", "application/json");
            return;
        }
        if (delete_dataset(name, err)) {
            res.set_content(R"({"success":true})", "application/json");
        } else {
            res.status = 500;
            res.set_content("{\"success\":false,\"message\":\"" + err + "\"}", "application/json");
        }
    });

        json body = json::parse(req.body);
        std::string dataset = body.value("dataset", "");
        size_t index = body.value("index", 0);
        std::string err;
        if (!is_valid_name(dataset) || !fs::exists(csv_path(dataset))) {
            res.status = 400;
            res.set_content(R"({"success":false,"message":"Dataset not found"})", "application/json");
            return;
        }
        if (delete_record(dataset, index, err))
            res.set_content(R"({"success":true})", "application/json");
        else {
            res.status = 500;
            res.set_content("{\"success\":false,\"message\":\"" + err + "\"}", "application/json");
        }
    });

    svr.Get("/api/records/export", [&](const httplib::Request& req, httplib::Response& res) {
        auto it = req.params.find("dataset");
        if (it == req.params.end() || !is_valid_name(it->second) || !fs::exists(csv_path(it->second))) {
            res.status = 400; return;
        }
        std::ifstream fin(csv_path(it->second), std::ios::binary);
        std::ostringstream ss; ss << fin.rdbuf();
        res.set_content(ss.str(), "text/csv");
        res.set_header("Content-Disposition", "attachment; filename=\"" + it->second + ".csv\"");
    });

    // Serve static files
    serve_static(svr);

    std::cout << "Server started at http://localhost:8080" << std::endl;
    bool ok = svr.listen("0.0.0.0", 8080);
    if (!ok) {
        std::cerr << "Failed to bind or listen on port 8080" << std::endl;
        return 1;
    }
    return 0;
}
