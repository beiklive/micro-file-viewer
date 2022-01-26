#include <ghc/filesystem.hpp>
#include <nlohmann/json.hpp>
#include <cpp-httplib/httplib.h>
#include <spdlog/spdlog.h>
#include <fplus/fplus.hpp>
#include <iostream>

constexpr int ServerPort = 8080;

void HandleFile(const httplib::Request &, httplib::Response &);

int main(int argc, char const *argv[])
{
    httplib::Server server;
    spdlog::set_level(spdlog::level::debug);

    spdlog::info("Hello MFV!");

    spdlog::info("HTTP Server will listen on {}:{}.", "localhost", ServerPort);
    if (server.bind_to_port("localhost", ServerPort))
    {
        spdlog::info("Server bind Done.");
    }
    else
    {
        spdlog::critical("Server bind Failed. Cannot bind to port:{}", ServerPort);
        return -1;
    }

    server.Get("/.*", HandleFile);

    if (server.listen_after_bind())
    {
        spdlog::info("Server close.");
    }
    else
    {
        spdlog::error("Server start Failed.");
    }
    return 0;
}

void HandleFile(const httplib::Request &req, httplib::Response &res)
{
    spdlog::info("{} {} from {:<30}:{}", req.method, req.path, req.remote_addr, req.remote_port);

    auto firstAccept = fplus::split(',', false, req.get_header_value("accept")).front();
    spdlog::debug("First accept: {}", firstAccept);

    if(firstAccept == "text/html")
    {
        res.set_content("<h1>test render page</h1>", firstAccept.c_str());
    }
    else if(firstAccept == "application/json")
    {
        nlohmann::json j;
        j["status"] = "ok";
        j["path"] = "/";
        res.set_content(j.dump(4), firstAccept.c_str());
    }
    else
    {
        res.status = 403;
    }
}