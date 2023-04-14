#pragma once

#include <string>
#include "wintypes.h"
#include "jsonla.h"
#include "output_callback.h"
#include "types.h"

class utils
{

public:
	utils(void);
	~utils(void);
	static ggicci::Json parseJSON(const std::string &JSONstr);
	static process_result launchExe(const std::string &exeName, const std::vector<std::string> &args,
		const std::string &input = "", const std::string &killSwitch = "", output_callback *callback = NULL );
	static void execCmd(std::string &exeName, std::vector<std::string> args, bool showConsole);
	static pid_t popen2(std::vector<const char*> args, int *fd_input, int *fd_output);
	static std::vector<const char*> getExecArgs(const std::string &exeName, const std::vector<std::string> &args);
	static void strReplaceAll(std::string &data, const std::string &toSearch, const std::string &replaceStr);
	static std::vector<std::string> strSplit(const std::string &str, const char delim);
	static std::string fileSaveDialog(const std::string &filename);
	static std::string folderOpenDialog();
	static std::string sanitizeFilename(const char* filename);
	static std::vector<std::string> getEnvarNames();
	static std::string strToLower(const std::string &str);
	static std::string trim(std::string str);
	static std::pair<std::string, std::string> getTerminalCmd();
};

