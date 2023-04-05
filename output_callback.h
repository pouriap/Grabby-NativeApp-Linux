#pragma once

#include <string>

class output_callback
{
	private:
	std::string dlHash;

	public:
	output_callback(const std::string &hash);
	~output_callback(void);
	void call(const std::string &output);
};

