#define _GNU_SOURCE

#include <mutex>
#include <sstream>
#include "utils.h"
#include "exceptions.h"
#include "defines.h"
#include "tinyfiledialogs.h"
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
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

process_result utils::launchExe(const string &exeName, const vector<string> &args, const string &input, 
					   const bool &killSwitch, output_callback *callback)
{
	if(exeName.length() > MAX_PATH)
	{
		throw grb_exception("Executable file name is too big");
	}

	vector<const char*> _args = utils::getExecArgs(exeName, args);

	int ch_fd_input, ch_fd_output;

	// If an error occurs, exit the application.
	int pid = utils::popen2(_args, &ch_fd_input, &ch_fd_output);
	if(pid == -1)
	{
		string msg = "popen2() failed";
		throw grb_exception(msg.c_str());
	}

	FILE* ch_inStream = fdopen(ch_fd_input, "w");
	FILE* ch_outStream = fdopen(ch_fd_output, "r");

	if(ch_inStream==NULL || ch_outStream==NULL)
	{
		string msg = "fdopen failed - errno: " + errno;
		PLOG_ERROR << msg;
		throw grb_exception(msg.c_str());
	}

	//write the input to the STDIN of the launched process
	if(input.length() > 0)
	{
		DWORD dwWritten;
		DWORD dataLen = input.length();

		dwWritten = fwrite(input.c_str(), sizeof(char), dataLen, ch_inStream);
		fflush(stdout);

		if(dwWritten!=dataLen)
		{
			PLOG_ERROR << "process stdin write failed - errno: " + errno;
			//TODO: error
		}
	}

	fclose(ch_inStream);

	//we read the output of the process
	const int BUFSIZE = 1024;
	char buf[BUFSIZE];
	unsigned long bytesRead = 0;
	unsigned long totalRead = 0;
	vector<char> out;

	//keep reading process output until it exits or we receive a kill command
	while(true)
	{
		if(feof(ch_outStream)){
			break;
		}

		//fgets returns when it reaches a carriage return or eof
		if(fgets(buf, BUFSIZE, ch_outStream) == NULL)
		{
			if(ferror(ch_outStream))
			{
				PLOG_INFO << "error reading output from process - ferror: " << ferror(ch_outStream);
			}

			break;
		}

		bytesRead = strlen(buf);
		out.reserve(BUFSIZE);
		out.insert(out.end(), buf, buf+bytesRead);
		totalRead += bytesRead;

		string o(buf, buf+bytesRead);

		//pass output to callback function
		if(callback != NULL)
		{
			string outStr(buf, buf+bytesRead);
			callback->call(outStr);
		}

		if(killSwitch)
		{
			kill(pid, SIGINT);
			break;
		}
	}

	// wait for process to exit and check its exit code
	fclose(ch_outStream);
	close(ch_fd_output);

	int status;
	int r;
	do{
		int r = waitpid(pid, &status, 0);
	}while (r == -1 && errno == EINTR);

	DWORD exitCode = 1;
	if(WIFEXITED(status)){
		exitCode = WEXITSTATUS(status);
	}

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

void utils::execCmd(string &exeName, vector<string> args, bool showConsole)
{
	if(exeName.length() > MAX_PATH)
	{
		throw grb_exception("Executable file name is too big");
	}

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

	vector<const char*> _args = utils::getExecArgs(exeName, args);

	int pid = fork();

	if(pid == 0)
	{
		execvp(_args[0], const_cast<char* const*>(_args.data()));
		exit(0);
	}
}

pid_t utils::popen2(vector<const char*> args, int *fd_input, int *fd_output)
{
	const int READ_END = 0;
	const int WRITE_END = 1;

    int p_child_stdin[2] = {0};
    int p_child_stdout[2] = {0};
    pid_t pid;

    //create two set of pipes
    if(pipe2(p_child_stdin, O_CLOEXEC) != 0 || pipe2(p_child_stdout, O_CLOEXEC) != 0)
    {
    	PLOG_ERROR << "failed to create pipes";
    	return -1;
    }

    pid = fork();

    // if fork failed
    if(pid == -1)
    {
    	close(p_child_stdin[WRITE_END]);
    	close(p_child_stdin[READ_END]);
    	close(p_child_stdout[WRITE_END]);
    	close(p_child_stdout[READ_END]);
    	PLOG_ERROR << "failed to fork";
    	return -1;
    }

    // in child process
    else if(pid == 0)
    {
    	//closed pipes that the child doesn't need
    	close(p_child_stdin[WRITE_END]);
    	close(p_child_stdout[READ_END]);

    	// gives "bad file descriptor" error when trying to read
    	// also this is after the fork() so we don't need the CLOEXEC flags
    	//dup3(p_child_stdin[READ_END], STDIN_FILENO, O_CLOEXEC);
    	//dup3(p_child_stdout[WRITE_END], STDOUT_FILENO, O_CLOEXEC);

    	dup2(p_child_stdin[READ_END], STDIN_FILENO);
    	dup2(p_child_stdout[WRITE_END], STDOUT_FILENO);

    	close(p_child_stdin[READ_END]);
    	close(p_child_stdout[WRITE_END]);

    	execvp(args[0], const_cast<char* const*>(args.data()));

    	//we should not reach here
    	printf("execvp failed");
    	_exit(1);
    }

    // in parent process
    else
    {
    	//closed pipes that the parent doesn't need
    	close(p_child_stdin[READ_END]);
    	close(p_child_stdout[WRITE_END]);

    	// assign the read and write handles to the pointers passed to the function and return pid
        if(fd_input == NULL){
        	close(p_child_stdin[WRITE_END]);
        }
        else{
        	*fd_input = p_child_stdin[WRITE_END];
        }

        if(fd_output == NULL)
        {
        	close(p_child_stdout[READ_END]);
        }
        else{
        	*fd_output = p_child_stdout[READ_END];
        }

        return pid;
    }
}

vector<const char*> utils::getExecArgs(const string &exeName, const vector<string> &args)
{
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
	for(int i=0; _args[i] != NULL; i++)
	{
		logCmd.append(_args[i]).append(" ");
	}

	PLOG_INFO << "exe-name: " << _args[0] << " - cmd: " << logCmd;

	return _args;
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

	const char illegalChars[10] = { '<', '>', ':', '"', '\'', '/', '\\', '|', '?', '*' };

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
