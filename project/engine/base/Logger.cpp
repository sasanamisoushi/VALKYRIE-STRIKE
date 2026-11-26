#include "Logger.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <Windows.h>

Logger& GetLogger() {
	static Logger instance;
	return instance;
}

void Logger::Initialize() {
	//ログの出力用のフォルダ
	std::filesystem::create_directory("logs");
	//現在時刻を取得
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	//コンマを削り秒にする
	std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
		nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
	//日本時間に変更
	std::chrono::zoned_time localTime{ std::chrono::current_zone(),nowSeconds };
	//formatを使って年月日時分秒の文字に変換
	std::string dateSting = std::format("{:%Y%m%d_%H%M%S}", localTime);
	//時刻を使ってファイル名を決定
	std::string logFilePath = std::string("logs/") + dateSting + "log";
	if (logStream_.is_open()) {
		logStream_.close();
	}
	logStream_.open(logFilePath);
}

void Logger::Log(const std::string &message) {

	//コンソールに出力
	std::cout << message << std::endl;

	//Visual Studio の出力ウィンドウにも出す
	OutputDebugStringA((message + "\n").c_str());

	////ファイルに出力
	//static std::ofstream logFile("log.txt", std::ios::app);

	if (logStream_.is_open()) {
		logStream_ << message << std::endl;
		logStream_.flush(); 
	}

	/*logFile << message;
	logFile.flush();*/
}