/**
 * @file main.c
 *
 * @description This file defines WebPA's main function
 *
 * Copyright (c) 2015  Comcast
 */
#include <stdlib.h>
#include <signal.h>
#include "wal.h"
#include "websocket_mgr.h"
#include "rdk_debug.h"
#include "wal_internal.h"

#ifdef WEBPA_RFC_ENABLED
#define RFC_BUFFER_SIZE 	256
#endif
#define LOG_MOD_WEBPA       "LOG.RDK.WEBPAVIDEO"

/*----------------------------------------------------------------------------*/
/*                             Function Prototypes                            */
/*----------------------------------------------------------------------------*/
//static void __terminate_listener(int value);
static void sig_handler(int sig);

#ifdef RDK_LOGGER_ENABLED
int b_rdk_logger_enabled = 0;

void logCallback(const char *buff)
{
    DEBUG_LOG("%s",buff);
}
#endif

#ifdef WEBPA_RFC_ENABLED
/* Utility function to parse the console output. */
int GetFeatureEnabled(char *cmd)
{
    FILE * pipeStream = NULL;
    char buffer[RFC_BUFFER_SIZE];
    int isFeatureEnabled = 0;

    memset(buffer, 0, RFC_BUFFER_SIZE);
    pipeStream = popen(cmd, "r");
    if (pipeStream != NULL)
    {
        if (fgets(buffer, RFC_BUFFER_SIZE, pipeStream) != NULL)
            sscanf(buffer,"%d",&isFeatureEnabled);
        else
            RDK_LOG(RDK_LOG_ERROR, LOG_MOD_WEBPA,"[%s] %s End of stream.\n", __FUNCTION__, cmd);
        pclose(pipeStream);
    }
    return isFeatureEnabled;
}
#endif

/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/
int main(int argc,char *argv[])
{
	const char* debugConfigFile = NULL;
	const char* nofityConfigFile = NULL;
	int itr=0;
#ifdef WEBPA_RFC_ENABLED
	int retVal = 0;
#endif

    while (itr < argc)
    {
        if(strcmp(argv[itr],"--debugconfig")==0)
        {
            itr++;
            if (itr < argc)
            {
                debugConfigFile = argv[itr];

#ifdef RDK_LOGGER_ENABLED
    	   if(rdk_logger_init(debugConfigFile) == 0) b_rdk_logger_enabled = 1;
           IARM_Bus_RegisterForLog(logCallback);
#else
           rdk_logger_init(debugConfigFile);
#endif
            }
            else
            {
                break;
            }
        }
        if(strcmp(argv[itr],"--notifyconfig")==0)
        {
            itr++;
            if (itr < argc)
            {
            	nofityConfigFile = argv[itr];
                RDK_LOG(RDK_LOG_INFO, LOG_MOD_WEBPA,"Setting Notification file %s\n",nofityConfigFile);
            }
            else
            {
                break;
            }
        }
        itr++;
    }

#ifdef WEBPA_RFC_ENABLED
	retVal = GetFeatureEnabled(". /lib/rdk/isFeatureEnabled.sh WEBPAXG");
	RDK_LOG(RDK_LOG_INFO, LOG_MOD_WEBPA,"[%s] WEBPAXG returns %d\n", __FUNCTION__, retVal);
	if( retVal == 0)
	{
	    system("systemctl stop webpavideo.service");
	    return 1;
	}
#endif

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
	setNotifyConfigurationFile(nofityConfigFile);
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
/*static void __terminate_listener(int value) {
	terminateSocketConnection();
	return;
}*/
static void sig_handler(int sig)
{
#if 0
	if ( sig == SIGINT ) {
		signal(SIGINT, sig_handler); /* reset it to this function */
		RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"WEBPA SIGINT received!\n");
		//exit(0);
	}
	else if ( sig == SIGUSR1 ) {
		signal(SIGUSR1, sig_handler); /* reset it to this function */
		RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"WEBPA SIGUSR1 received!\n");
	}
	else if ( sig == SIGUSR2 ) {
		RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"WEBPA SIGUSR2 received!\n");
	}
	else if ( sig == SIGCHLD ) {
		signal(SIGCHLD, sig_handler); /* reset it to this function */
		RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"WEBPA SIGHLD received!\n");
	}
	else if ( sig == SIGPIPE ) {
		signal(SIGPIPE, sig_handler); /* reset it to this function */
		RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"WEBPA SIGPIPE received!\n");
	}
	else if ( sig == SIGALRM ) {
		signal(SIGALRM, sig_handler); /* reset it to this function */
		RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"WEBPA SIGALRM received!\n");
	}
	else if( sig == SIGTERM ) {
		signal(SIGTERM, __terminate_listener);
		RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"WEBPA SIGTERM received!\n");
		exit(0);
	}
	else {
		RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"WEBPA Signal %d received!\n", sig);
		exit(0);
	}
#endif
        if( sig == SIGTERM ) {
                terminateSocketConnection();
        }

        signal(sig, SIG_DFL );
        kill(getpid(), sig );

}

