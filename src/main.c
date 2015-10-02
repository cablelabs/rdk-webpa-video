/**
 * @file main.c
 *
 * @description This file defines WebPA's main function
 *
 * Copyright (c) 2015  Comcast
 */
#include "wal.h"
#include "websocket_mgr.h"
#include "stdlib.h"
#include "signal.h"

/*----------------------------------------------------------------------------*/
/*                             Function Prototypes                            */
/*----------------------------------------------------------------------------*/
static void __terminate_listener(int value);
static void sig_handler(int sig);

/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/
int main()
{
	signal(SIGTERM, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGUSR1, sig_handler);
	signal(SIGUSR2, sig_handler);
	signal(SIGSEGV, sig_handler);
	signal(SIGBUS, sig_handler);
	signal(SIGKILL, sig_handler);
	signal(SIGFPE, sig_handler);
	signal(SIGILL, sig_handler);
	signal(SIGQUIT, sig_handler);
	signal(SIGHUP, sig_handler);
	signal(SIGALRM, sig_handler);

	msgBusInit("webpa");
	createSocketConnection();
	while(1)
    {
        sleep(1);
    }
	return 1;
}

/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/
static void __terminate_listener(int value) {
	terminateSocketConnection();
	return;
}
static void sig_handler(int sig)
{
	if ( sig == SIGINT ) {
		signal(SIGINT, sig_handler); /* reset it to this function */
		printf("WEBPA SIGINT received!\n");
		//exit(0);
	}
	else if ( sig == SIGUSR1 ) {
		signal(SIGUSR1, sig_handler); /* reset it to this function */
		printf("WEBPA SIGUSR1 received!\n");
	}
	else if ( sig == SIGUSR2 ) {
		printf("WEBPA SIGUSR2 received!\n");
	}
	else if ( sig == SIGCHLD ) {
		signal(SIGCHLD, sig_handler); /* reset it to this function */
		printf("WEBPA SIGHLD received!\n");
	}
	else if ( sig == SIGPIPE ) {
		signal(SIGPIPE, sig_handler); /* reset it to this function */
		printf("WEBPA SIGPIPE received!\n");
	}
	else if ( sig == SIGALRM ) {
		signal(SIGALRM, sig_handler); /* reset it to this function */
		printf("WEBPA SIGALRM received!\n");
	}
	else if( sig == SIGTERM ) {
		signal(SIGTERM, __terminate_listener);
		printf("WEBPA SIGTERM received!\n");
		exit(0);
	}
	else {
		printf("WEBPA Signal %d received!\n", sig);
		exit(0);
	}
}

