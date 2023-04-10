#pragma once

#include "output_callback.h"
#include "ytdl_args.h"
#include "types.h"
#include "jsonla.h"
#include <plog/Log.h>
#include <plog/Initializers/RollingFileInitializer.h>
#include <string>

using namespace ggicci;

void processMessage(const Json &msg);
void handle_getversion(const Json &msg);
void handle_getavail(const Json &msg);
void handle_download(const Json &msg);
void handle_userCMD(const Json &msg);
void handle_ytdlinfo(const Json &msg);
void handle_ytdlget(const Json &msg);
void handle_ytdlkill(const Json &msg);
void flashgot_job(const std::string &jobJSON);
void custom_command_fork(std::string exeName, std::vector<std::string> args, const std::string filename, bool showConsole, bool showSaveas);
void ytdl_info_th(const std::string url, const std::string dlHash, ytdl_args *arger);
void ytdl_get_th(const std::string url, const std::string dlHash, ytdl_args *arger, const std::string filename);
process_result ytdl(const std::string &url, const std::string &dlHash, std::vector<std::string> &args, output_callback *callback = NULL);
