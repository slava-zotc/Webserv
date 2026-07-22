diff --git a/Makefile b/Makefile
index dec7bc1..7f66739 100644
--- a/Makefile
+++ b/Makefile
@@ -13,7 +13,8 @@ SRCS = \
 	$(SRC_DIR)/Socket.cpp \
 	$(SRC_DIR)/ListeningSocket.cpp \
 	$(SRC_DIR)/ClientSocket.cpp \
-	$(SRC_DIR)/HttpRequest.cpp
+	$(SRC_DIR)/HttpRequest.cpp \
+	$(SRC_DIR)/HttpResponse.cpp
 
 OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
 
diff --git a/include/ClientSocket.hpp b/include/ClientSocket.hpp
index 01653a2..aae9679 100644
--- a/include/ClientSocket.hpp
+++ b/include/ClientSocket.hpp
@@ -5,6 +5,7 @@
 
 #include <string>
 
+#include "HttpRequest.hpp"
 #include "Socket.hpp"
 
 class ClientSocket
@@ -36,9 +37,7 @@ public:
 private:
 	enum State
 	{
-		READ_HEADERS,
-		READ_BODY,
-		PROCESS,
+		READING,
 		READY_SEND,
 		READY_DELETE
 	};
@@ -47,7 +46,7 @@ private:
 	ClientSocket& operator=(const ClientSocket& rhs);
 	Socket socket_fd;
 	std::string response_buffer;
-	std::string request_buffer;
+	HttpRequest request;
 	std::string::size_type partial_write;
 	State state_client;
 };
diff --git a/src/ClientSocket.cpp b/src/ClientSocket.cpp
index 9aed60b..70699ad 100644
--- a/src/ClientSocket.cpp
+++ b/src/ClientSocket.cpp
@@ -1,6 +1,9 @@
 #include "ClientSocket.hpp"
+
 #include <sys/socket.h>
 
+#include "HttpRequest.hpp"
+#include "HttpResponse.hpp"
 /**
  * @brief Конструктор ClientSocket
  * @param fd Файловый дескриптор клиентского сокета
@@ -8,20 +11,21 @@
  * Инициализирует клиентское соединение в начальном состоянии (READ_HEADERS).
  */
 ClientSocket::ClientSocket(int fd)
-    : socket_fd(fd), partial_write(0), state_client(READ_HEADERS) {}
+	: socket_fd(fd), partial_write(0), state_client(READING)
+{
+}
 
 /**
  * @brief Возвращает маску готовых событий
- * @return Комбинация флагов WANT_READ/WANT_WRITE в зависимости от статуса обработки
+ * @return Комбинация флагов WANT_READ/WANT_WRITE в зависимости от статуса
+ * обработки
  */
 short ClientSocket::get_ready_events() const
 {
-    short mask = 0;
-    if (is_ready_send())
-        mask |= WANT_WRITE; // Эта клиент готов до отправки
-    if (is_reading_phase())
-        mask |= WANT_READ; // Равно, я может читать ещё
-    return mask;
+	short mask = 0;
+	if (is_ready_send()) mask |= WANT_WRITE;	// Эта клиент готов до отправки
+	if (is_reading_phase()) mask |= WANT_READ;	// Равно, я может читать ещё
+	return mask;
 }
 /**
  * @brief Проверяет, готов ли ответ к отправке
@@ -29,7 +33,7 @@ short ClientSocket::get_ready_events() const
  */
 bool ClientSocket::is_ready_send() const
 {
-    return partial_write < response_buffer.size();
+	return partial_write < response_buffer.size();
 }
 /**
  * @brief Проверяет, находится ли клиент в фазе чтения
@@ -37,7 +41,7 @@ bool ClientSocket::is_ready_send() const
  */
 bool ClientSocket::is_reading_phase() const
 {
-    return state_client != READY_DELETE && state_client != READY_SEND;
+	return state_client == READING;
 }
 /**
  * @brief Проверяет, готов ли сокет к удалению
@@ -45,7 +49,7 @@ bool ClientSocket::is_reading_phase() const
  */
 bool ClientSocket::is_ready_delete() const
 {
-    return state_client == READY_DELETE;
+	return state_client == READY_DELETE;
 }
 /**
  * @brief Обрабатывает получение данных от клиента
@@ -55,20 +59,34 @@ bool ClientSocket::is_ready_delete() const
  */
 void ClientSocket::handle_read()
 {
-    char tmp_buffer[4096];
-    int byte_recv = recv(socket_fd.get_fd(), tmp_buffer, sizeof(tmp_buffer), 0);
-    // Клиент рассоединился или ошибка
-    if (byte_recv <= 0)
-    {
-        state_client = READY_DELETE;
-        return;
-    }
-    // Скопируем данные в буфер ресурс
-    request_buffer.append(tmp_buffer, byte_recv);
-    // На данный момент генерируем тривиальный HTTP ответ
-    response_buffer = "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, World!";
-    state_client = READY_SEND;   // Переводим в режим отправки
-    std::cout << request_buffer; // Логируем ресурс
+	char tmp_buffer[4096];
+
+	int byte_recv = recv(socket_fd.get_fd(), tmp_buffer, sizeof(tmp_buffer), 0);
+	// Клиент рассоединился или ошибка
+	if (byte_recv <= 0)
+	{
+		state_client = READY_DELETE;
+		return;
+	}
+	std::string recv_string(tmp_buffer, byte_recv);
+	// Скопируем данные в буфер ресурс
+	request.parse(recv_string);
+	// Временно до роутера
+	if (request.get_parsing_state() == HttpRequest::PARSING_DONE)
+	{
+		HttpResponse response(200);
+		response.set_body("Hello, World!");
+		response_buffer = response.serialize();
+	}
+	else if (request.get_parsing_state() == HttpRequest::PARSING_ERROR)
+	{
+		HttpResponse response(400);
+		response_buffer = response.serialize();
+	}
+	else
+		return;
+	// На данный момент генерируем тривиальный HTTP ответ
+	state_client = READY_SEND;	// Переводим в режим отправки
 }
 /**
  * @brief Обрабатывает отправку HTTP-ответа клиенту
@@ -78,24 +96,24 @@ void ClientSocket::handle_read()
  */
 void ClientSocket::handle_write()
 {
-    // Отправляем неотправленную часть ответа
-    int byte_send = send(socket_fd.get_fd(),
-                         response_buffer.c_str() + partial_write,
-                         response_buffer.size() - partial_write, 0);
-    // Ошибка жати
-    if (byte_send == -1)
-    {
-        state_client = READY_DELETE;
-        return;
-    }
-    // Обновляем номер невыдаченных байтов
-    partial_write += byte_send;
-    // Проверяем, не отправлен ли все данные
-    if (partial_write == response_buffer.size())
-    {
-        partial_write = 0;
-        state_client = READY_DELETE; // Да это соединение достаточно
-    }
+	// Отправляем неотправленную часть ответа
+	int byte_send =
+		send(socket_fd.get_fd(), response_buffer.c_str() + partial_write,
+			 response_buffer.size() - partial_write, 0);
+	// Ошибка жати
+	if (byte_send == -1)
+	{
+		state_client = READY_DELETE;
+		return;
+	}
+	// Обновляем номер невыдаченных байтов
+	partial_write += byte_send;
+	// Проверяем, не отправлен ли все данные
+	if (partial_write == response_buffer.size())
+	{
+		partial_write = 0;
+		state_client = READY_DELETE;  // Да это соединение достаточно
+	}
 }
 /**
  * @brief Возвращает файловый дескриптор клиентского сокета
@@ -103,10 +121,12 @@ void ClientSocket::handle_write()
  */
 int ClientSocket::get_client_socket_fd() const
 {
-    return socket_fd.get_fd();
+	return socket_fd.get_fd();
 }
 
 /**
  * @brief Деструктор ClientSocket
  */
-ClientSocket::~ClientSocket() {}
\ No newline at end of file
+ClientSocket::~ClientSocket()
+{
+}
diff --git a/src/Core.cpp b/src/Core.cpp
index 101c05f..8ddce0c 100644
--- a/src/Core.cpp
+++ b/src/Core.cpp
@@ -1,5 +1,7 @@
 #include "Core.hpp"
 
+#include <csignal>
+
 /**
  * @brief Глобальная переменная для отслеживания статуса сигналов
  */
@@ -14,10 +16,10 @@ volatile sig_atomic_t Core::g_signal_status = 0;
  */
 void signal_handler(int signum)
 {
-    if (signum == SIGINT)
-    {
-        Core::g_signal_status = signum;
-    }
+	if (signum == SIGINT)
+	{
+		Core::g_signal_status = signum;
+	}
 }
 
 /**
@@ -28,8 +30,8 @@ void signal_handler(int signum)
  */
 Core::Core(int port)
 {
-    ListeningSocket *server_socket = new ListeningSocket(port);
-    listening_sockets[server_socket->get_listen_socket_fd()] = server_socket;
+	ListeningSocket* server_socket = new ListeningSocket(port);
+	listening_sockets[server_socket->get_listen_socket_fd()] = server_socket;
 }
 
 /**
@@ -41,12 +43,10 @@ Core::Core(int port)
  */
 short Core::translate_client_mask_in_posix(short mask)
 {
-    short result = 0;
-    if (mask & ClientSocket::WANT_READ)
-        result |= POLLIN;
-    if (mask & ClientSocket::WANT_WRITE)
-        result |= POLLOUT;
-    return result;
+	short result = 0;
+	if (mask & ClientSocket::WANT_READ) result |= POLLIN;
+	if (mask & ClientSocket::WANT_WRITE) result |= POLLOUT;
+	return result;
 }
 
 /**
@@ -61,103 +61,108 @@ short Core::translate_client_mask_in_posix(short mask)
  */
 void Core::core_loop()
 {
-    int ret = 0;
-    signal(SIGINT, signal_handler);
-    while (Core::g_signal_status == 0)
-    {
-        // Список файловых дескрипторов клиентов, готовых к удалению
-        std::vector<int> delete_client;
-        fds.clear();
-        struct pollfd tmp_pollfd;
-        // Добавляем в poll() все слушающие сокеты
-        for (std::map<int, ListeningSocket *>::iterator it = listening_sockets.begin(); it != listening_sockets.end(); it++)
-        {
-            tmp_pollfd.fd = it->first;
-            tmp_pollfd.events = POLLIN; // Ждём входящих соединений
-            fds.push_back(tmp_pollfd);
-        }
-        // Добавляем в poll() все активные клиентские соединения
-        // События зависят от состояния обработки (чтение, запись)
-        for (std::map<int, ClientSocket *>::iterator it = client_sockets.begin(); it != client_sockets.end(); it++)
-        {
-            short want_events = translate_client_mask_in_posix(
-                it->second->get_ready_events());
-            tmp_pollfd.fd = it->first;
-            tmp_pollfd.events = want_events;
-            fds.push_back(tmp_pollfd);
-        }
+	int ret = 0;
+	signal(SIGINT, signal_handler);
+	signal(SIGPIPE, SIG_IGN);
+	while (Core::g_signal_status == 0)
+	{
+		// Список файловых дескрипторов клиентов, готовых к удалению
+		std::vector<int> delete_client;
+		fds.clear();
+		struct pollfd tmp_pollfd;
+		// Добавляем в poll() все слушающие сокеты
+		for (std::map<int, ListeningSocket*>::iterator it =
+				 listening_sockets.begin();
+			 it != listening_sockets.end(); it++)
+		{
+			tmp_pollfd.fd = it->first;
+			tmp_pollfd.events = POLLIN;	 // Ждём входящих соединений
+			fds.push_back(tmp_pollfd);
+		}
+		// Добавляем в poll() все активные клиентские соединения
+		// События зависят от состояния обработки (чтение, запись)
+		for (std::map<int, ClientSocket*>::iterator it = client_sockets.begin();
+			 it != client_sockets.end(); it++)
+		{
+			short want_events =
+				translate_client_mask_in_posix(it->second->get_ready_events());
+			tmp_pollfd.fd = it->first;
+			tmp_pollfd.events = want_events;
+			fds.push_back(tmp_pollfd);
+		}
 
-        // poll() ждёт готовых событий (таймаут 1000ms = 1 секунда)
-        ret = poll(fds.data(), fds.size(), 1000);
+		// poll() ждёт готовых событий (таймаут 1000ms = 1 секунда)
+		ret = poll(fds.data(), fds.size(), 1000);
 
-        // Ошибка poll()
-        if (ret < 0)
-        {
-            std::cerr << "ERROR Poll failed" << std::endl;
-            continue;
-        }
+		// Ошибка poll()
+		if (ret < 0)
+		{
+			std::cerr << "ERROR Poll failed" << std::endl;
+			continue;
+		}
 
-        // Таймаут poll() истёк, событий нет
-        if (ret == 0)
-            continue;
+		// Таймаут poll() истёк, событий нет
+		if (ret == 0) continue;
 
-        // Обрабатываем все готовые события
-        for (size_t i = 0; i < fds.size(); i++)
-        {
-            // Событие чтения (новое соединение или данные от клиента)
-            if (fds[i].revents & (POLLIN | POLLERR | POLLHUP))
-            {
-                std::map<int, ListeningSocket *>::iterator it_listening =
-                    listening_sockets.find(fds[i].fd);
+		// Обрабатываем все готовые события
+		for (size_t i = 0; i < fds.size(); i++)
+		{
+			// Событие чтения (новое соединение или данные от клиента)
+			if (fds[i].revents & (POLLIN | POLLERR | POLLHUP))
+			{
+				std::map<int, ListeningSocket*>::iterator it_listening =
+					listening_sockets.find(fds[i].fd);
 
-                // Это слушающий сокет - приём нового клиента
-                if (it_listening != listening_sockets.end())
-                {
-                    int client_fd = it_listening->second->accept_conection();
-                    if (client_fd != -1)
-                    {
-                        client_sockets[client_fd] = new ClientSocket(client_fd);
-                    }
-                }
-                // Это клиентский сокет - чтение данных
-                else
-                {
-                    std::map<int, ClientSocket *>::iterator it_client =
-                        client_sockets.find(fds[i].fd);
+				// Это слушающий сокет - приём нового клиента
+				if (it_listening != listening_sockets.end())
+				{
+					int client_fd = it_listening->second->accept_conection();
+					if (client_fd != -1)
+					{
+						client_sockets[client_fd] = new ClientSocket(client_fd);
+					}
+				}
+				// Это клиентский сокет - чтение данных
+				else
+				{
+					std::map<int, ClientSocket*>::iterator it_client =
+						client_sockets.find(fds[i].fd);
 
-                    if (it_client != client_sockets.end())
-                    {
-                        it_client->second->handle_read();
-                    }
-                }
-            }
-            // Событие записи - отправка данных клиенту
-            if (fds[i].revents & POLLOUT)
-            {
-                std::map<int, ClientSocket *>::iterator it_client =
-                    client_sockets.find(fds[i].fd);
+					if (it_client != client_sockets.end())
+					{
+						it_client->second->handle_read();
+					}
+				}
+			}
+			// Событие записи - отправка данных клиенту
+			if (fds[i].revents & POLLOUT)
+			{
+				std::map<int, ClientSocket*>::iterator it_client =
+					client_sockets.find(fds[i].fd);
 
-                if (it_client != client_sockets.end())
-                {
-                    it_client->second->handle_write();
-                }
-            }
-        }
-        // Находим готовые к удалению клиентские соединения
-        for (std::map<int, ClientSocket *>::iterator it = client_sockets.begin(); it != client_sockets.end(); it++)
-        {
-            if (it->second->is_ready_delete())
-                delete_client.push_back(it->first);
-        }
+				if (it_client != client_sockets.end())
+				{
+					if (!it_client->second->is_ready_delete())
+						it_client->second->handle_write();
+				}
+			}
+		}
+		// Находим готовые к удалению клиентские соединения
+		for (std::map<int, ClientSocket*>::iterator it = client_sockets.begin();
+			 it != client_sockets.end(); it++)
+		{
+			if (it->second->is_ready_delete())
+				delete_client.push_back(it->first);
+		}
 
-        // Удаляем закрытые соединения
-        for (size_t i = 0; i < delete_client.size(); i++)
-        {
-            int fd_client_delete = delete_client[i];
-            delete client_sockets[fd_client_delete];
-            client_sockets.erase(fd_client_delete);
-        }
-    }
+		// Удаляем закрытые соединения
+		for (size_t i = 0; i < delete_client.size(); i++)
+		{
+			int fd_client_delete = delete_client[i];
+			delete client_sockets[fd_client_delete];
+			client_sockets.erase(fd_client_delete);
+		}
+	}
 }
 
 /**
@@ -168,14 +173,17 @@ void Core::core_loop()
  */
 Core::~Core(void)
 {
-    // Удаляем все слушающие сокеты
-    for (std::map<int, ListeningSocket *>::iterator it = listening_sockets.begin(); it != listening_sockets.end(); it++)
-    {
-        delete it->second;
-    }
-    // Удаляем все клиентские соединения
-    for (std::map<int, ClientSocket *>::iterator it = client_sockets.begin(); it != client_sockets.end(); it++)
-    {
-        delete it->second;
-    }
-}
\ No newline at end of file
+	// Удаляем все слушающие сокеты
+	for (std::map<int, ListeningSocket*>::iterator it =
+			 listening_sockets.begin();
+		 it != listening_sockets.end(); it++)
+	{
+		delete it->second;
+	}
+	// Удаляем все клиентские соединения
+	for (std::map<int, ClientSocket*>::iterator it = client_sockets.begin();
+		 it != client_sockets.end(); it++)
+	{
+		delete it->second;
+	}
+}
diff --git a/src/HttpRequest.cpp b/src/HttpRequest.cpp
index f04655f..8dbd7cf 100644
--- a/src/HttpRequest.cpp
+++ b/src/HttpRequest.cpp
@@ -132,6 +132,11 @@ HttpRequest::ParsingState HttpRequest::parse(const std::string& data)
 	return parsing_state_;
 }
 
+HttpRequest::ParsingState HttpRequest::get_parsing_state() const
+{
+	return parsing_state_;
+}
+
 HttpRequest::~HttpRequest(void)
 {
 }
diff --git a/src/HttpResponse.cpp b/src/HttpResponse.cpp
index 1af74f4..1d29857 100644
--- a/src/HttpResponse.cpp
+++ b/src/HttpResponse.cpp
@@ -57,3 +57,7 @@ std::string HttpResponse::serialize() const
 	ss << body_;
 	return ss.str();
 }
+
+HttpResponse::~HttpResponse()
+{
+}
diff --git a/src/ListeningSocket.cpp b/src/ListeningSocket.cpp
index 615b5ca..8782655 100644
--- a/src/ListeningSocket.cpp
+++ b/src/ListeningSocket.cpp
@@ -1,5 +1,9 @@
 #include "ListeningSocket.hpp"
+
+#include <sys/socket.h>
+
 #include <cstring>
+#include <iostream>
 
 /**
  * @brief Статический метод для создания неблокирующего TCP-сокета
@@ -10,18 +14,17 @@
  */
 int ListeningSocket::create_non_blocking_socket_fd()
 {
-    // Создаём IPv4 TCP сокет
-    int server_fd = socket(PF_INET, SOCK_STREAM, 0);
-    if (server_fd == -1)
-        throw std::runtime_error("ERROR func socket");
+	// Создаём IPv4 TCP сокет
+	int server_fd = socket(PF_INET, SOCK_STREAM, 0);
+	if (server_fd == -1) throw std::runtime_error("ERROR func socket");
 
-    // Устанавливаем non-blocking мод
-    if (fcntl(server_fd, F_SETFL, O_NONBLOCK) == -1)
-    {
-        close(server_fd);
-        throw std::runtime_error("ERROR func fcntl");
-    }
-    return server_fd;
+	// Устанавливаем non-blocking мод
+	if (fcntl(server_fd, F_SETFL, O_NONBLOCK) == -1)
+	{
+		close(server_fd);
+		throw std::runtime_error("ERROR func fcntl");
+	}
+	return server_fd;
 }
 
 /**
@@ -30,7 +33,7 @@ int ListeningSocket::create_non_blocking_socket_fd()
  */
 int ListeningSocket::get_listen_socket_fd() const
 {
-    return fd_socket.get_fd();
+	return fd_socket.get_fd();
 }
 
 /**
@@ -39,24 +42,30 @@ int ListeningSocket::get_listen_socket_fd() const
  * @throw std::runtime_error если ошибка при bind() или listen()
  */
 ListeningSocket::ListeningSocket(int port)
-    : fd_socket(create_non_blocking_socket_fd())
+	: fd_socket(create_non_blocking_socket_fd())
 {
-    int server_fd = get_listen_socket_fd();
+	int server_fd = get_listen_socket_fd();
 
-    // Настраиваем адрес для bind()
-    struct sockaddr_in adrr;
-    std::memset(&adrr, 0, sizeof(adrr));
-    adrr.sin_family = AF_INET;         // IPv4
-    adrr.sin_port = htons(port);       // Преобразуем номер порта в нетверковой формат
-    adrr.sin_addr.s_addr = INADDR_ANY; // Послушиваем все интерфейсы
+	// Настраиваем адрес для bind()
+	struct sockaddr_in adrr;
+	std::memset(&adrr, 0, sizeof(adrr));
+	adrr.sin_family = AF_INET;	// IPv4
+	adrr.sin_port =
+		htons(port);  // Преобразуем номер порта в нетверковой формат
+	adrr.sin_addr.s_addr = INADDR_ANY;	// Послушиваем все интерфейсы
 
-    // Привязываем сокет к порту
-    if (bind(server_fd, (struct sockaddr *)&adrr, sizeof(adrr)) == -1)
-        throw std::runtime_error("ERROR func bind");
+	int opt = 1;
+	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))
+		== -1)
+		std::cerr << "Error func setsockopt" << std::endl;
+	// Привязываем сокет к порту
+	if (bind(server_fd, (struct sockaddr*)&adrr, sizeof(adrr)) == -1)
+		throw std::runtime_error("ERROR func bind");
 
-    // Настраиваем ковартилу входящих необработанных соединений (128 соединения)
-    if (listen(server_fd, 128) == -1)
-        throw std::runtime_error("ERROR func listen");
+	// Настраиваем ковартилу входящих необработанных соединений
+	// (128 соединения)
+	if (listen(server_fd, 128) == -1)
+		throw std::runtime_error("ERROR func listen");
 }
 
 /**
@@ -74,17 +83,16 @@ ListeningSocket::~ListeningSocket(void)
  */
 int ListeningSocket::accept_conection()
 {
-    // Принимаем соединение
-    int fd_client = accept(get_listen_socket_fd(), NULL, NULL);
+	// Принимаем соединение
+	int fd_client = accept(get_listen_socket_fd(), NULL, NULL);
 
-    // Ошибка accept() или non-blocking режим (нет данных)
-    if (fd_client < 0)
-        return -1;
-    // Устанавливаем non-blocking режим для клиента
-    if (fcntl(fd_client, F_SETFL, O_NONBLOCK) == -1)
-    {
-        close(fd_client);
-        return -1;
-    }
-    return fd_client;
-}
\ No newline at end of file
+	// Ошибка accept() или non-blocking режим (нет данных)
+	if (fd_client < 0) return -1;
+	// Устанавливаем non-blocking режим для клиента
+	if (fcntl(fd_client, F_SETFL, O_NONBLOCK) == -1)
+	{
+		close(fd_client);
+		return -1;
+	}
+	return fd_client;
+}
diff --git a/src/main.cpp b/src/main.cpp
index 8e0ce2e..fb95fc1 100644
--- a/src/main.cpp
+++ b/src/main.cpp
@@ -1,31 +1,19 @@
-#include "Core.hpp"
+#include <exception>
+#include <iostream>
 
-/**
- * @file main.cpp
- * @brief Точка входа простого HTTP-сервера на C++
- *
- * Создает объект Core, который инициализирует сервер на порте 8080
- * и запускает главный event loop для обработки входящих HTTP-запросов.
- *
- * Использование:
- * - Скомпилировать: make
- * - Запустить: ./webserv
- * - Остановить: Ctrl+C
- */
+#include "Core.hpp"
 
-/**
- * @brief Главная функция программы
- * @return 0 при успешном завершении
- */
 int main()
 {
-    // Создаём объект Core и инициализируем сервер на порте 8080
-    Core core(8080);
-
-    // Запускаем главный event loop
-    // Цикл завершится при получении сигнала SIGINT (Ctrl+C)
-    core.core_loop();
-
-    // Деструктор Core автоматически закроет все сокеты
-    return 0;
-}
\ No newline at end of file
+	try
+	{
+		Core core(8080);
+		core.core_loop();
+	}
+	catch (std::exception& e)
+	{
+		std::cerr << e.what() << std::endl;
+		return 1;
+	}
+	return 0;
+}
