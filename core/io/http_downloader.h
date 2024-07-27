#pragma once

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <mutex>
#include <condition_variable>

class HttpDownloader {
public:
	HttpDownloader(const std::string& ipv4, unsigned int port, int max_threads = 3);

	static bool is_port_open(const std::string& ip, int port, int timeout_ms = 500);
	void fetch_threaded(std::vector<std::string> urls);
	const std::unordered_map<std::string, std::vector<uint8_t>>& get_results() const;

	static void parse_url(const std::string& url, std::string& ipv4, int& port);
	static std::vector<std::string> url_from_params_for_godot_pack(const std::string &size, const std::vector<std::map<std::string, std::string>> &data);

private:
	std::string m_host;
	unsigned int m_port;
	int m_max_threads;

	std::vector<uint8_t> http_get_binary(const std::string& host, const std::string& path);
	void downloader(const std::string& url);

	std::mutex mtx;
	std::condition_variable cv;
	int activeThreads = 0;

	std::unordered_map<std::string, std::vector<uint8_t>> results;
};

