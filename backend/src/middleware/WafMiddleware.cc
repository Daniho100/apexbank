#include "WafMiddleware.h"
#include <drogon/drogon.h>
#include <algorithm>
#include <cctype>
#include <json/json.h>

namespace banking::middleware {

bool WafMiddleware::isMalicious(const std::string& input) {
    if (input.empty()) return false;

    // Convert to lowercase
    std::string lower = input;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return std::tolower(c);
    });

    // 1. SQL Injection (SQLi) Signatures
    if (lower.find("select ") != std::string::npos && (lower.find(" from ") != std::string::npos || lower.find(" union ") != std::string::npos)) return true;
    if (lower.find("insert into ") != std::string::npos) return true;
    if (lower.find("update ") != std::string::npos && lower.find(" set ") != std::string::npos) return true;
    if (lower.find("delete from ") != std::string::npos) return true;
    if (lower.find("drop table ") != std::string::npos) return true;
    if (lower.find("union select ") != std::string::npos) return true;
    if (lower.find("or 1=1") != std::string::npos || lower.find("or '1'='1'") != std::string::npos || lower.find("or \"1\"=\"1\"") != std::string::npos) return true;
    if (lower.find("admin' --") != std::string::npos || lower.find("' or ") != std::string::npos) return true;

    // 2. Cross-Site Scripting (XSS) Signatures
    if (lower.find("<script") != std::string::npos) return true;
    if (lower.find("javascript:") != std::string::npos) return true;
    if (lower.find("onerror=") != std::string::npos) return true;
    if (lower.find("onload=") != std::string::npos) return true;
    if (lower.find("onclick=") != std::string::npos) return true;
    if (lower.find("alert(") != std::string::npos) return true;
    if (lower.find("<iframe") != std::string::npos) return true;
    if (lower.find("eval(") != std::string::npos) return true;

    // 3. Path Traversal Signatures
    if (lower.find("../") != std::string::npos) return true;
    if (lower.find("..\\") != std::string::npos) return true;
    if (lower.find("/etc/passwd") != std::string::npos) return true;
    if (lower.find("c:\\windows") != std::string::npos) return true;

    // 4. Command Injection Signatures
    if (lower.find("shell_exec") != std::string::npos) return true;
    if (lower.find("system(") != std::string::npos) return true;
    if (lower.find("/bin/sh") != std::string::npos) return true;
    if (lower.find("/bin/bash") != std::string::npos) return true;
    if (lower.find("cmd.exe") != std::string::npos) return true;
    if (lower.find("powershell") != std::string::npos) return true;

    return false;
}

bool WafMiddleware::isRequestMalicious(const drogon::HttpRequestPtr& req, std::string& blockReason) {
    // 1. Scan request path
    if (isMalicious(req->path())) {
        blockReason = "malicious path: " + req->path();
        return true;
    }

    // 2. Scan query parameters
    for (const auto& [key, value] : req->parameters()) {
        if (isMalicious(key) || isMalicious(value)) {
            blockReason = "malicious query param: key=" + key + ", value=" + value;
            return true;
        }
    }

    // 3. Scan specific request headers
    {
        std::vector<std::string> headersToScan = {"user-agent", "x-forwarded-for", "cookie", "referer"};
        for (const auto& h : headersToScan) {
            std::string hVal = req->getHeader(h);
            if (isMalicious(hVal)) {
                blockReason = "malicious header " + h + "=" + hVal;
                return true;
            }
        }
    }

    // 4. Scan body (if present and not empty)
    if (!req->body().empty()) {
        if (isMalicious(std::string(req->body()))) {
            blockReason = "malicious payload in body";
            return true;
        }
    }

    return false;
}

void WafMiddleware::doFilter(
    const drogon::HttpRequestPtr& req,
    drogon::FilterCallback&& fcb,
    drogon::FilterChainCallback&& fccb
) {
    std::string blockReason;
    if (isRequestMalicious(req, blockReason)) {
        LOG_WARN << "WAF filter blocked request: " << blockReason;
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            []() {
                Json::Value json;
                json["error"] = "Forbidden";
                json["message"] = "Request blocked by Web Application Firewall (WAF).";
                return json;
            }()
        );
        resp->setStatusCode(drogon::k403Forbidden);
        fcb(resp);
        return;
    }

    fccb();
}

} // namespace banking::middleware
