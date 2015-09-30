#include "waldb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_PARAMETER_LENGTH 512
#define MAX_NUM_PARAMETERS 2048

int main(int argc,char *argv[])
{
    DB_STATUS ret = DB_FAILURE;
    char paramName[MAX_PARAMETER_LENGTH];


    if(argc>1)
    {
        strncpy(paramName,argv[1],MAX_PARAMETER_LENGTH-1);
        paramName[MAX_PARAMETER_LENGTH]='\0';
    }
    else
    {
        strncpy(paramName,"Device.WiFi.RadioNumberOfEntries",MAX_PARAMETER_LENGTH-1);
        paramName[MAX_PARAMETER_LENGTH]='\0';
    }

    //Load document model
    int dbhandle = 0;
    ret = loaddb("./data-model.xml",(void *)&dbhandle);

    const char wcard = '*';
    int i = 0;

    if(ret == DB_SUCCESS && dbhandle)
    {
        if(strchr(paramName,wcard))
        {
            char **getParamList;
            int paramCount = 0;

            /* Translate wildcard to list of parameters */
            getParamList = (char **) malloc(MAX_NUM_PARAMETERS * sizeof(char *));
            ret = getParameterList((void *)dbhandle,paramName,getParamList,&paramCount);

            for(i = 0; i < paramCount; i++)
                printf("%s\n",getParamList[i]);

        }
        else /* No wildcard check for validity of parameter */
        {
            if(isParameterValid((void *)dbhandle,paramName))
            {
                printf("Parameter %s is valid\n",paramName);
            }
            else
            {
                printf("Parameter %s is invalid\n",paramName);
            }
        }
    }
}
