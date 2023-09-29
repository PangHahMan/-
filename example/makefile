.PHONY:mysql

mysql:mysql.c
	gcc mysql.c -o mysql -L/usr/lib64/mysql -lmysqlclient

.PHONY:server
server:server.cpp
	g++ -std=c++11 $^ -o $@ -pthread -lboost_system