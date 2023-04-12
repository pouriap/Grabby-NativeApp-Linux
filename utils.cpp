#include <mutex>
#include <sstream>
#include "utils.h"
#include "exceptions.h"
#include "defines.h"
#include "tinyfiledialogs.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <plog/Log.h>
#include <plog/Initializers/RollingFileInitializer.h>

using namespace std;
using namespace ggicci;


std::mutex guiMutex;


utils::utils(void)
{
}

utils::~utils(void)
{
}

Json utils::parseJSON(const string &JSONstr)
{
	try
	{
		Json json = Json::Parse(JSONstr.c_str());
		return json;
	}
	catch (exception& e)
	{
		string msg = "Error parsing JSON: ";
		msg.append(e.what());
		throw grb_exception(msg.c_str());
	}
}

// a pipe has two ends, a read handle and a write handle
// the read handle reads from it, the write handle writes to it
// we make two pipes and end up with 2 read handles and 2 write handles
// we give one read handle and one write handle to the child
// after we give it the handles we close them becuase the parent doesn't need them anymore
// they will stay open in the child
//our handles:
//h_child_stdout_r
//h_child_stdout_w
//h_child_stdin_r
//h_child_stdin_w
process_result utils::launchExe(const string &exeName, const vector<string> &args, const string &input, 
					   const bool &kill, output_callback *callback)
{
	//first part of cmd line has to also be the application name
	string cmd = "";
	cmd.append("'").append(exeName).append("'");

	//create cmd line of arguments from the args vector
	for(int i=0; i<args.size(); i++)
	{
		//if it's an empty string don't add anything to command line
		if(args[i].length() == 0){
			continue;
		}
		cmd.append(" ").append("'").append(args[i]).append("'");
	}

	//some checks
	if(exeName.length() > MAX_PATH)
	{
		throw grb_exception("Executable file name is too big");
	}

	if(cmd.length() > CMD_MAX_LEN)
	{
		throw grb_exception("Command line too big");
	}

	PLOG_INFO << "exe name: " << exeName << " - cmd: " << cmd;

	FILE *proc;

	// If an error occurs, exit the application.
	if ((proc = popen(cmd.c_str(), "r")) == NULL)
	{
		string msg = "popen() failed - errno: " + errno;
		throw grb_exception(msg.c_str());
	}

	//write the input to the STDIN of the launched process
	if(input.length() > 0)
	{
		DWORD dwWritten;
		DWORD dataLen = input.length();

		dwWritten = fwrite(input.c_str(), sizeof(char), dataLen, proc);
		fflush(stdout);

		if(dwWritten!=dataLen)
		{
			PLOG_ERROR << "process stdin write failed - errno: " + errno;
			//TODO: error
		}
	}

	//we read the output of the process
	const int BUFSIZE = 1024;
	char buf[BUFSIZE];
	unsigned long bytesRead = 0;
	unsigned long totalRead = 0;
	vector<char> out;

	//keep reading process output until it exits or we receive a kill command
	while(true)
	{
		//fgets returns when it reaches a carriage return or eof
		if(fgets(buf, BUFSIZE, proc) == NULL)
		{
			if(feof(proc)){
				//cout << "EOF EOF" << endl;
			} // TODO: end of file
			if(ferror(proc)){
				//cout<< "ERR ERR" << endl;
			} // TODO: error
			break;
		}

		bytesRead = strlen(buf);
		out.reserve(BUFSIZE);
		out.insert(out.end(), buf, buf+bytesRead);
		totalRead += bytesRead;

		string ass(buf, buf+bytesRead);
		PLOG_INFO << "OUTPUT_LINE: " << ass;

		//pass output to callback function
		if(callback != NULL)
		{
			string outStr(buf, buf+bytesRead);
			callback->call(outStr);
		}

		if(kill)
		{
			//TODO: not implemented
			// GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, piProcInfo.dwProcessId);
			break;
		}
	}

	// wait for process to exit and check its exit code
	DWORD exitCode = pclose(proc);
	PLOG_INFO << "process exit code is " << exitCode;

	process_result res;
	res.exitCode = exitCode;

	if(totalRead<=0)
	{
		res.output = "";
	}
	else
	{
		res.output = string(out.begin(), out.end());
	}

	PLOG_INFO << "exe output be: " << res.output;

	return res;

}

void utils::execCmd(string &exeName, vector<string> &args, bool showConsole)
{
	//so we need to show console and we do it with gnome-terminal --
	//but we don't want to input unsatized data to command line
	//so we take the process name and the cmd string separately
	//then we open a new gnome-terminal and start our 'launcher' which takes the process name as its first parameter
	//now launcher can use the execv() command to safely launch the requested program
	if(showConsole)
	{
		pair<string, string> terminalCmd = utils::getTerminalCmd();

		args.insert(args.begin(), exeName);
		args.insert(args.begin(), terminalCmd.second);
		exeName = terminalCmd.first;
	}

	vector<const char*> _args;

	//first element of the array should also be the executable name
	_args.push_back(exeName.c_str());

	//push the rest of the args
	for(int i=0; i<args.size(); i++)
	{
		_args.push_back(args[i].c_str());
	}

	//last element of the array should be NULL
	_args.push_back(NULL);

	string logCmd = "";
	for(int i=0; i<args.size(); i++)
	{
		logCmd.append(_args[i]).append(" ");
	}

	PLOG_INFO << "custom-exe-name: " << exeName << " - custom-cmd: " << logCmd;

	int pid = fork();

	if(pid == 0)
	{
		execvp(exeName.c_str(), const_cast<char* const*>(_args.data()));
		exit(0);
	}
}

void utils::strReplaceAll(string &data, const string &toSearch, const string &replaceStr)
{
	size_t pos = data.find(toSearch);
	while(pos != string::npos)
	{
		data.replace(pos, toSearch.size(), replaceStr);
		pos =data.find(toSearch, pos + replaceStr.size());
	}
}

vector<string> utils::strSplit(const string &str, const char delim)
{
	vector<string> parts;
	stringstream stream(str);
	string temp;

	if(str.find(delim) == string::npos){
		return parts;
	}

	while(getline(stream, temp, delim))
	{
		if(temp.size() > 0) parts.push_back(temp);
	}

	return parts;
}

string utils::fileSaveDialog(const string &filename)
{
	const char* path = tinyfd_saveFileDialog("Save file as", filename.c_str(), 0, NULL, NULL );
	return (path == NULL)? "" : path;
}

string utils::folderOpenDialog()
{
	const char* path = tinyfd_selectFolderDialog("Select folder to save files", NULL);
	return (path == NULL)? "" : path;
}

string utils::sanitizeFilename(const char* filename)
{
	string newName("");

	const char illegalChars[9] = { '<', '>', ':', '"', '/', '\\', '|', '?', '*' };

	for(int i=0; i < strlen(filename); i++)
	{
		char c = filename[i];
		bool replace = false;

		//check for non-printable characters
		if(c < 32 || c > 126){
			replace = true;
		}
		//check in illegal characters
		else
		{
			for(int j=0; j<sizeof(illegalChars); j++)
			{
				if(c == illegalChars[j]){
					replace = true;
					break;
				}
			}
		}

		if(!replace){
			newName += c;
		}
		else{
			newName += '_';
		}
	}

	return newName;
}

string utils::strToLower(const string &str)
{
	string strl("");

	for(int i=0; i<str.length(); i++)
	{
		strl += std::tolower(str[i]);
	}

	return strl;
}

string utils::trim(string str)
{
	const char* ws = " \t\n\r\f\v";
	str.erase(str.find_last_not_of(ws) + 1);
	str.erase(0, str.find_first_not_of(ws));

	return str;
}

pair<string, string> utils::getTerminalCmd()
{
	pair<string, string> cmd;

	if(cmd.first.length() && cmd.second.length())
	{
		return cmd;
	}

	if(!system("which gnome-terminal > /dev/null 2>&1"))
	{
		cmd.first = "gnome-terminal";
		cmd.second = "--";
	}
	else if(!system("which konsole > /dev/null 2>&1"))
	{
		cmd.first = "konsole";
		cmd.second = "e";
	}
	else if(!system("which xterm > /dev/null 2>&1"))
	{
		cmd.first = "xterm";
		cmd.second = "e";
	}
	else
	{
		throw grb_exception("could not fina terminal emulator on system");
	}

	return cmd;
}
