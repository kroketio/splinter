#include "http_downloader.h"

#include <iostream>
#include <sstream>
#include <thread>
#include <map>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>
#endif

HttpDownloader::HttpDownloader(const std::string &ipv4, unsigned int port, int max_threads) :
    m_host(ipv4), m_port(port), m_max_threads(max_threads) {}

/*
 *   std::vector<std::map<std::string, std::string>> data = {
 *     {{"name", "ConcretePavementwithGrass57060"}, {"asset_pack", "blenderkit"}},
 *     {{"name", "UnpolishedGrayMarble540"}, {"asset_pack", "fab_1k"}},
 *     {{"name", "PatternedMarbleFloor305"}, {"asset_pack", "fab_1k"}}
 *   };
 */
std::vector<std::string> HttpDownloader::url_from_params_for_godot_pack(const std::string &size,
    const std::vector<std::map<std::string, std::string>> &data) {

  const std::string base = "/api/1/textures/godot_pack";
  std::vector<std::string> urls;

  for (const auto &entry: data) {
    std::ostringstream oss;
    oss << base << "?"
        << "name="
        << entry.at("name")
        << "&asset_pack="
        << entry.at("asset_pack")
        << "&image_size=" << size;
    urls.push_back(oss.str());
  }

  return urls;
}

void HttpDownloader::parse_url(const std::string& url, std::string& ipv4, int& port) {
	port = 80;

	std::string prefix = "http://";
	size_t start = url.find(prefix);
	if (start != std::string::npos) {
		start += prefix.length();
	} else {
		start = 0;
	}

	size_t end = url.find('/', start);
	std::string host_port = url.substr(start, end - start);

	size_t colon_pos = host_port.find(':');
	if (colon_pos != std::string::npos) {
		ipv4 = host_port.substr(0, colon_pos);
		std::string port_str = host_port.substr(colon_pos + 1);
		port = std::stoi(port_str);
	} else {
		ipv4 = host_port;
	}
}

std::vector<uint8_t> HttpDownloader::http_get_binary(const std::string &host, const std::string &path) {
#ifdef _WIN32
  WSADATA wsaData;
  WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

  std::vector<uint8_t> body;

  // 1. Create socket
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    std::cerr << "Socket creation failed.\n";
    return body;
  }

  // 2. Setup sockaddr_in
  sockaddr_in serverAddr = {};
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(m_port);
  inet_pton(AF_INET, m_host.c_str(), &serverAddr.sin_addr);

  // 3. Connect
  if (connect(sockfd, (sockaddr *) &serverAddr, sizeof(serverAddr)) < 0) {
    std::cerr << "Connection failed\n";
#ifdef _WIN32
    closesocket(sockfd);
    WSACleanup();
#else
    close(sockfd);
#endif
    return body;
  }

  // 4. Send HTTP GET request
  std::ostringstream req;
  req << "GET " << path << " HTTP/1.1\r\n"
      << "Host: "<< m_host <<"\r\n"
      << "Connection: close\r\n\r\n";
  std::string requestStr = req.str();
  send(sockfd, requestStr.c_str(), requestStr.size(), 0);

  // 5. Read headers
  std::string header;
  char buffer[1];
  int bytes;
  std::string headers_raw;
  while (headers_raw.find("\r\n\r\n") == std::string::npos && (bytes = recv(sockfd, buffer, 1, 0)) > 0) {
    headers_raw.append(buffer, 1);
  }

  // 6. Parse Content-Length
  size_t contentLength = 0;
  size_t clPos = headers_raw.find("content-length:");
  if (clPos != std::string::npos) {
    size_t start = clPos + 15;
    size_t end = headers_raw.find("\r\n", start);
    std::string lenStr = headers_raw.substr(start, end - start);
    contentLength = std::stoul(lenStr);
  }

  // 7. Copy initial body part
  std::string::size_type bodyStart = headers_raw.find("\r\n\r\n") + 4;
  std::vector<uint8_t> headerPart(headers_raw.begin() + bodyStart, headers_raw.end());
  body.insert(body.end(), headerPart.begin(), headerPart.end());

  // 8. Read remaining
  char readBuf[4096];
  while (body.size() < contentLength && (bytes = recv(sockfd, readBuf, sizeof(readBuf), 0)) > 0) {
    body.insert(body.end(), readBuf, readBuf + bytes);
  }

#ifdef _WIN32
  closesocket(sockfd);
  WSACleanup();
#else
  close(sockfd);
#endif

  return body;
}

void HttpDownloader::downloader(const std::string &url) {
  {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [&]() { return activeThreads < m_max_threads; });
    ++activeThreads;
  }

  std::vector<uint8_t> data = http_get_binary(m_host, url);

  {
    std::lock_guard<std::mutex> lock(mtx);
    results[url] = std::move(data);
    --activeThreads;
    cv.notify_all();
  }
}

bool HttpDownloader::is_port_open(const std::string& ip, int port, int timeout_ms) {
#ifdef _WIN32
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
    std::cerr << "WSAStartup failed.\n";
    return false;
  }
#endif

  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    std::cerr << "Socket creation failed.\n";
#ifdef _WIN32
    WSACleanup();
#endif
    return false;
  }

#ifdef _WIN32
  u_long mode = 1;
  ioctlsocket(sock, FIONBIO, &mode); // non-blocking
#else
  fcntl(sock, F_SETFL, O_NONBLOCK);
#endif

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

  int result = connect(sock, (sockaddr*)&addr, sizeof(addr));
  if (result == 0) {
    // Connected immediately
#ifdef _WIN32
    closesocket(sock);
    WSACleanup();
#else
    close(sock);
#endif
    return true;
  }

#ifdef _WIN32
  fd_set writefds;
  FD_ZERO(&writefds);
  FD_SET(sock, &writefds);

  timeval tv{};
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;

  result = select(0, NULL, &writefds, NULL, &tv);
#else
  fd_set writefds;
  FD_ZERO(&writefds);
  FD_SET(sock, &writefds);

  timeval tv{};
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;

  result = select(sock + 1, NULL, &writefds, NULL, &tv);
#endif

  bool connected = result > 0;

#ifdef _WIN32
  closesocket(sock);
  WSACleanup();
#else
  close(sock);
#endif

  return connected;
}

void HttpDownloader::fetch_threaded(std::vector<std::string> urls) {
  std::mutex qmutex;
  size_t index = 0;

  auto worker = [&]() {
    while (true) {
      std::string url;

      {
        std::lock_guard<std::mutex> lock(qmutex);
        if (index >= urls.size()) break;
        url = urls[index++];
      }

      std::vector<uint8_t> data = http_get_binary(m_host, url);

      {
        std::lock_guard<std::mutex> lock(mtx);
        results[url] = std::move(data);
      }
    }
  };

  std::vector<std::thread> pool;
  for (int i = 0; i < m_max_threads; ++i) {
    pool.emplace_back(worker);
  }

  for (auto& t : pool) t.join();

  std::cout << "All downloads complete.\n";
}

const std::unordered_map<std::string, std::vector<uint8_t>> &HttpDownloader::get_results() const { return results; }
