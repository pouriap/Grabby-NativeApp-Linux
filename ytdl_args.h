#pragma once

#include "jsonla.h"
#include <string>

using namespace ggicci;

class ytdl_args
{
	protected:
		std::vector<std::string> args;
	public:
		ytdl_args(const Json &msg);
		~ytdl_args(void);
		void addArg(const std::string &arg);
		virtual std::vector<std::string> getArgs() = 0;
};

class ytdl_info: public ytdl_args
{
	public:
	ytdl_info(const Json &msg);
	std::vector<std::string> getArgs();
};

class ytdl_video: public ytdl_args
{
	private:
	std::string formatId;

	public:
	ytdl_video(const Json &msg);
	std::vector<std::string> getArgs();
};

class ytdl_audio: public ytdl_args
{
	public:
	ytdl_audio(const Json &msg);
	std::vector<std::string> getArgs();
};

class ytdl_playlist_video: public ytdl_args
{
	private:
	std::string indexesStr;
	std::string res;

	public:
	ytdl_playlist_video(const Json &msg);
	std::vector<std::string> getArgs();
};

class ytdl_playlist_audio: public ytdl_args
{
	private:
	std::string indexesStr;

	public:
	ytdl_playlist_audio(const Json &msg);
	std::vector<std::string> getArgs();
};
