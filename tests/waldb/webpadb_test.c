/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2017 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "wal.h"
#include "rdk_debug.h"

#define MAX_PARAMETER_LENGTH 512
#define MAX_NUM_PARAMETERS 512
#define MAX_VALUE_LENGTH 48

#define LOG_MOD_WEBPA "LOG.RDK.WEBPAVIDEO" //TODO: Replace all RDK_LOG with WalError/WalInfo 

int main(int argc,char *argv[])
{
rdk_logger_init("/etc/debug.ini");
msgBusInit("Webpa");
char *getParamList[MAX_NUM_PARAMETERS];
int i = 0;
int j = 0;
int paramCount = 1;

if(argc<=1)
{
	RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"Usage: ./webpatest <set/get> <optional list of parameters>");
	exit(0);
}

if(argc>2)
{
	paramCount = argc-2;
	for(i=0;i<paramCount;i++)
	{
		getParamList[i] = (char *) malloc(MAX_PARAMETER_LENGTH);		
		strncpy(getParamList[i],argv[i+2],MAX_PARAMETER_LENGTH-1);
		getParamList[i][MAX_PARAMETER_LENGTH] = '\0';
		RDK_LOG(RDK_LOG_DEBUG,LOG_MOD_WEBPA,"getParamList[%d] is %s\n",i,getParamList[i]);
	}
}
else
{
	getParamList[0]="Device.WiFi.Radio.1.Status";
}

WAL_STATUS *ret = (WAL_STATUS *) malloc(sizeof(WAL_STATUS) * paramCount);


//Test getValues
if(!strcmp(argv[1],"get"))
{
int *count = (int *) malloc(sizeof(int) * paramCount);
ParamVal ***parametervalArr = (ParamVal ***) malloc( sizeof(ParamVal **) * paramCount);
getValues((const char **)getParamList, paramCount, NULL, parametervalArr, count, ret);
for(i=0;i<paramCount;i++)
{
	if(ret[i] == WAL_SUCCESS)
	{
	for(j=0;j<count[i];j++)
	{
		//  RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"getValues returned parmetername isvalue is %s datatype is %d\n",(parametervalArr[i][j])->value,(parametervalArr[i][j])->type);
		RDK_LOG(RDK_LOG_DEBUG,LOG_MOD_WEBPA,"getValues returned parmetername is: %s",(parametervalArr[i][j])->name);
		switch((parametervalArr[i][j])->type)
		{
			case WAL_STRING:
				RDK_LOG(RDK_LOG_DEBUG,LOG_MOD_WEBPA," value is:%s\n",(parametervalArr[i][j])->value);
				break;
			case WAL_INT:
			case WAL_UINT:
			case WAL_BOOLEAN:
			case WAL_LONG:
			case WAL_ULONG:
				RDK_LOG(RDK_LOG_DEBUG,LOG_MOD_WEBPA,"value is:%d\n",*((int *)(parametervalArr[i][j])->value));
				break;
			default:
				RDK_LOG(RDK_LOG_DEBUG,LOG_MOD_WEBPA,"\n");
				break;
		}
	}
	}
}
}
else if(!strcmp(argv[1],"set")) /*Test setvalues*/
{
    ParamVal *paramvalArr = (ParamVal *) malloc(sizeof(ParamVal) * paramCount);
    for (i = 0; i < paramCount; i++)
    {
	paramvalArr[i].name = getParamList[i];
	paramvalArr[i].value = (char *) malloc(sizeof(char) * MAX_VALUE_LENGTH);
	strcpy(paramvalArr[i].value,"up"); 
	paramvalArr[i].type = WAL_STRING;
    }
    setValues(paramvalArr,paramCount,0, NULL,ret,NULL);
}
else
{
	  RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"Unsupported action\n");
	  RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"Usage: ./webpatest <set/get> <optional list of parameters>");
}

   /*TODO: Free up allocated resources*/
}
