#pragma once
#include <string>
#include <fstream>


class Logger {
public:

	void Initialize();

	//ログ出力
	void Log(const std::string &message);
private:
	//ファイルへの書き込みストリームをメンバ変数として保持する
	std::ofstream logStream_;
	
};

Logger& GetLogger();

