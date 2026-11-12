// Atrapa lwip/sockets.h - na PC te same funkcje daje system (gniazda BSD).
// Adres centrali (192.168.4.1:80) jest podmieniany na lokalny serwer testowy
// na etapie linkowania (-Wl,--wrap=connect, patrz symulator_panelu.cpp).
#pragma once
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
