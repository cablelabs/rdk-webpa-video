#include "waldb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_PARAMETER_LENGTH 512
#define MAX_DATATYPE_LENGTH 48
#define MAX_NUM_PARAMETERS 2048


typedef enum
{
    WAL_STRING = 0,
    WAL_INT,
    WAL_UINT,
    WAL_BOOLEAN,
    WAL_DATETIME,
    WAL_BASE64,
    WAL_LONG,
    WAL_ULONG,
    WAL_FLOAT,
    WAL_DOUBLE,
    WAL_BYTE,
    WAL_NONE
} DATA_TYPE;


typedef enum _HostIf_ParamType
{
    hostIf_StringType = 0,
    hostIf_IntegerType,
    hostIf_UnsignedIntType,
    hostIf_BooleanType,
    hostIf_DateTimeType,
    hostIf_UnsignedLongType
}
HostIf_ParamType_t;

static void	converttohostIfType(char *ParamDataType,HostIf_ParamType_t* pParamType);
static void	converttoWalType(HostIf_ParamType_t paramType,DATA_TYPE* walType);

static void	converttohostIfType(char *ParamDataType,HostIf_ParamType_t* pParamType)
{
	if(!strcmp(ParamDataType,"string"))
		*pParamType = hostIf_StringType;
	else if(!strcmp(ParamDataType,"unsignedInt"))
		*pParamType = hostIf_UnsignedIntType;
	else if(!strcmp(ParamDataType,"int"))
		*pParamType = hostIf_IntegerType;
	else if(!strcmp(ParamDataType,"unsignedLong"))
		*pParamType = hostIf_UnsignedLongType;
	else if(!strcmp(ParamDataType,"boolean"))
		*pParamType = hostIf_BooleanType;
	else if(!strcmp(ParamDataType,"hexBinary"))
		*pParamType = hostIf_StringType;
	else
		*pParamType = hostIf_StringType;
}

static void	converttoWalType(HostIf_ParamType_t paramType,DATA_TYPE* pwalType)
{
	switch(paramType)
	{
	case hostIf_StringType:
		*pwalType = WAL_STRING;
		break;
	case hostIf_UnsignedIntType:
		*pwalType = WAL_UINT;
		break;
	case hostIf_IntegerType:
		*pwalType = WAL_INT;
		break;
	case hostIf_BooleanType:
		*pwalType = WAL_BOOLEAN;
		break;
	case hostIf_UnsignedLongType:
		*pwalType = WAL_ULONG;
		break;
	case hostIf_DateTimeType:
		*pwalType = WAL_DATETIME;
		break;
	default:
		*pwalType = WAL_STRING;
		break;
	}
}

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
	HostIf_ParamType_t hostIfType;
	DATA_TYPE walType;

	if(ret == DB_SUCCESS && dbhandle)
	{
		if(strchr(paramName,wcard))
		{
			char **getParamList;
			char **ParamDataTypeList;
			int paramCount = 0;

			/* Translate wildcard to list of parameters */
			getParamList = (char **) malloc(MAX_NUM_PARAMETERS * sizeof(char *));
			ParamDataTypeList = (char **) malloc(MAX_NUM_PARAMETERS * sizeof(char *));
			ret = getParameterList((void *)dbhandle,paramName,getParamList,ParamDataTypeList,&paramCount);

			for(i = 0; i < paramCount; i++)
			{
				// Convert ParamDataType to hostIf datatype
				converttohostIfType(ParamDataTypeList[i],&(hostIfType));
				printf("hostIfType is %d\n",hostIfType);
				//Convert hostifDatatype to ParamVal.type
				converttoWalType(hostIfType,&(walType));
				printf("walType is %d\n",walType);
				printf("%s:%s:%d:%d\n",getParamList[i],ParamDataTypeList[i],hostIfType,walType);
				free(getParamList[i]);
				free(ParamDataTypeList[i]);
			}
			free(getParamList);
			free(ParamDataTypeList);
		}
		else /* No wildcard check for validity of parameter */
		{
			char *dataType = malloc(sizeof(char) * MAX_DATATYPE_LENGTH);
			if(isParameterValid((void *)dbhandle,paramName,dataType))
			{
				// Convert ParamDataType to hostIf datatype
				converttohostIfType(dataType,&(hostIfType));
				printf("hostIfType is %d\n",hostIfType);
				//Convert hostifDatatype to ParamVal.type
				converttoWalType(hostIfType,&(walType));
				printf("walType is %d\n",walType);
				printf("Parameter %s is valid DataType is %s hostIfType is %d walType is %d\n",paramName,dataType,hostIfType,walType);
			}
			else
			{
				printf("Parameter %s is invalid\n",paramName);
			}
		}
	}
}
