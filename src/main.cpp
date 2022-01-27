#include <ghc/filesystem.hpp>
#include <nlohmann/json.hpp>
#include <cpp-httplib/httplib.h>
#include <spdlog/spdlog.h>
#include <fplus/fplus.hpp>
#include <iostream>

auto applicationConfig =
    nlohmann::json::parse(R"(
    {
        "root":".",
        "server":
        {
            "host":"localhost",
            "port":8080
        },
        "blob":
        {
            "url_prefix":"http://localhost:8080/blob/"
        }
    }
)");

void loadConfig();
void HandleFile(const httplib::Request &, httplib::Response &);

int main(int argc, char const *argv[])
{
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("Hello MFV!");

    loadConfig();

    if (argc == 2)
    {
        spdlog::info("Use arg = {}.", argv[argc - 1]);
        applicationConfig["server"]["port"] = std::atoi(argv[argc - 1]);
    }
    else if (argc == 3)
    {
        spdlog::info("Use arg = {} {}.", argv[argc - 2], argv[argc - 1]);
        applicationConfig["server"]["host"] = argv[argc - 2];
        applicationConfig["server"]["port"] = std::atoi(argv[argc - 1]);
    }

    httplib::Server server;

    spdlog::info("HTTP Server will listen on {}:{}.", applicationConfig["server"]["host"], applicationConfig["server"]["port"].get<int>());
    if (server.bind_to_port(applicationConfig["server"]["host"].get<std::string>().c_str(), applicationConfig["server"]["port"].get<int>()))
    {
        spdlog::info("Server bind Done.");
    }
    else
    {
        spdlog::critical("Server bind Failed. Cannot bind to {}:{}", applicationConfig["server"]["host"], applicationConfig["server"]["port"].get<int>());
        return -1;
    }

    server.set_mount_point("/", "html");
    server.Get("/file:(.*)", HandleFile);

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
    nlohmann::json j;
    ghc::filesystem::path requirePath(req.matches[1]);

    j["require_path"] = requirePath.string();

    try
    {
        do
        {
            auto canonicalPath = ghc::filesystem::canonical(applicationConfig["root"].get<std::string>() + "/" + requirePath.string());
            auto relativePath = ghc::filesystem::relative(canonicalPath);
            auto absolutePath = ghc::filesystem::absolute(canonicalPath);
    
            j["real_path"] = relativePath.string();
            spdlog::debug("visit canonical path = {}", canonicalPath.string());

            if (!ghc::filesystem::exists(relativePath))
            {
                res.status = 404;
                j["code"] = 404;
                j["success"] = true;
                j["message"] = httplib::detail::status_message(j["code"]);
                break;
            }
            if (ghc::filesystem::is_directory(relativePath))
            {
                j["folder"] = true;

                for (const auto &item : ghc::filesystem::directory_iterator(relativePath))
                {
                    nlohmann::json i;
                    i["name"] = item.path().filename().string();
                    i["path"] = item.path().string();
                    i["canonical_path"] = ghc::filesystem::canonical(item.path()).string();
                    i["url"] = fmt::format("{}{}", applicationConfig["blob"]["url_prefix"], item.path().string());
                    i["attrib"]["directory"] = item.is_directory();
                    i["attrib"]["character_file"] = item.is_character_file();
                    i["attrib"]["block_file"] = item.is_block_file();
                    i["attrib"]["other"] = item.is_other();
                    i["attrib"]["symlink"] = item.is_symlink();
                    i["attrib"]["regular_file"] = item.is_regular_file();
                    i["attrib"]["socket"] = item.is_socket();
                    i["attrib"]["fifo"] = item.is_fifo();

                    j["content"].push_back(i);
                }
            }
            else
            {
                j["folder"] = false;
            }

            res.status = 200;
            j["code"] = 200;
            j["success"] = true;
            j["message"] = httplib::detail::status_message(j["code"]);

        } while (false);
    }
    catch (nlohmann::json::invalid_iterator e)
    {
        spdlog::error("nlohmann::json::invalid_iterator occur @HandleFile {}", e.what());
    }
    catch (nlohmann::json::parse_error e)
    {
        spdlog::error("nlohmann::json::parse_error occur @HandleFile: {}", e.what());
    }
    catch (nlohmann::json::type_error e)
    {
        spdlog::error("nlohmann::json::type_error occur @HandleFile {}", e.what());
    }
    catch (nlohmann::json::out_of_range e)
    {
        spdlog::error("nlohmann::json::out_of_range occur @HandleFile {}", e.what());
    }
    catch (...)
    {
        spdlog::error("Unknow error occur @HandleFile");
    }

    res.set_content(j.dump(4), "application/json");
    spdlog::info("{} {} {} \t\tfrom {}:{}", j["code"].get<int>(), req.method, req.path, req.remote_addr, req.remote_port);
}

void loadConfig()
{
    ghc::filesystem::path applicationConfigPath{"application.json"};

    if (ghc::filesystem::exists(applicationConfigPath))
    {
        spdlog::info("Load config from {}", applicationConfigPath.string());
        try
        {
            ghc::filesystem::ifstream applicationConfigFile(applicationConfigPath);
            applicationConfig.merge_patch(nlohmann::json::parse(std::string{std::istreambuf_iterator<char>(applicationConfigFile),
                                                                            std::istreambuf_iterator<char>()}));
        }
        catch (...)
        {
            spdlog::critical("Unable to load {}, check the content and try again!", applicationConfigPath.string());
            throw;
        }
    }
    else
    {
        spdlog::info("Generate config {}", applicationConfigPath.string());
        ghc::filesystem::ofstream applicationConfigFile(applicationConfigPath);
        applicationConfigFile << applicationConfig.dump(4);
        applicationConfigFile.close();
    }
}
