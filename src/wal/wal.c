#include "wal.h"
#include "waldb/waldb.h"
#include "libIBus.h"
#include "libIARM.h"
typedef unsigned int bool;
#include "hostIf_tr69ReqHandler.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_NUM_PARAMETERS 2048
#define MAX_DATATYPE_LENGTH 48
static int get_ParamValues_tr69hostIf(HOSTIF_MsgData_t *param);
static int GetParamInfo(const char *pParameterName, ParamVal ***parametervalArr,int *TotalParams);
static int set_ParamValues_tr69hostIf (HOSTIF_MsgData_t param);
static int SetParamInfo(ParamVal paramVal);
static int getParamAttributes(const char *pParameterName, AttrVal ***attr, int *TotalParams);
static int setParamAttributes(const char *pParameterName, const AttrVal *attArr);

static void converttohostIfType(char *ParamDataType,HostIf_ParamType_t* pParamType);
static void converttoWalType(HostIf_ParamType_t paramType,DATA_TYPE* walType);

static int dbhandle = 0;

static void converttohostIfType(char *ParamDataType,HostIf_ParamType_t* pParamType)
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

static void converttoWalType(HostIf_ParamType_t paramType,DATA_TYPE* pwalType)
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


/**
 * @brief getValues interface returns the parameter values.
 *
 * getValues supports an option to pass wildward parameters. This can be achieved by passing an object name followed by '.'
 * instead of parameter name.
 *
 * @param[in] paramName List of Parameters.
 * @param[in] paramCount Number of parameters.
 * @param[out] paramValArr Two dimentional array of parameter name/value pairs.
 * @param[out] retValCount List of "number of parameters" for each input paramName. Usually retValCount will be 1 except
 * for wildcards request where it represents the number of param/value pairs retrieved for the particular wildcard parameter.
 * @param[out] retStatus List of Return status.
 */
void getValues(const char *paramName[], const unsigned int paramCount, ParamVal ***paramValArr, int *retValCount, WAL_STATUS *retStatus)
//void getValues(const char *paramName[], int paramCount, ParamVal ***paramValArr, int *retValCount, WAL_STATUS *retStatus)
{
	int cnt = 0;
	int numParams = 0;
	for (cnt = 0; cnt < paramCount; cnt++) {
		retStatus[cnt] = GetParamInfo(paramName[cnt], &paramValArr[cnt],&numParams);
		retValCount[cnt]=numParams;
		RDK_LOG(RDK_LOG_DEBUG,LOG_MOD_WEBPA,"Parameter Name: %s, Parameter Value: %s return: %d\n",paramName[cnt],(paramValArr[cnt][0])->value,retStatus[cnt]);
	}
}

/**
 * @brief Initializes the Message Bus and registers WebPA component with the stack.
 *
 * @param[in] name WebPA Component Name.
 * @return WAL_STATUS.
 */

WAL_STATUS msgBusInit(const char *name)
{
	IARM_Result_t ret = IARM_RESULT_IPCCORE_FAIL;
	DB_STATUS dbRet = DB_FAILURE;
	ret = IARM_Bus_Init(name);
	if(ret != IARM_RESULT_SUCCESS)
	{
		/*TODO: Log Error */
		return WAL_FAILURE;
	}

	ret = IARM_Bus_Connect();
	if(ret != IARM_RESULT_SUCCESS)
	{
		RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"Error connecting to IARM Bus for '%s'\r\n", name);
		return WAL_FAILURE;
	}

	// Load Document model
	dbRet = loaddb("/etc/data-model.xml",(void *)&dbhandle);

	if(dbRet != DB_SUCCESS)
	{
		RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"Error loading database\n");
		return WAL_FAILURE;
	}
}

/**
 * @brief Registers the notification callback function.
 *
 * @param[in] cb Notification callback function.
 * @return WAL_STATUS.
 */
WAL_STATUS RegisterNotifyCB(notifyCB cb)
{
	//TODO::
}

/**
 * generic Api for get HostIf parameters through IARM_TR69Bus
**/
static int get_ParamValues_tr69hostIf(HOSTIF_MsgData_t *ptrParam)
{
	IARM_Result_t ret = IARM_RESULT_IPCCORE_FAIL;

	ptrParam->reqType = HOSTIF_GET;

	ret = IARM_Bus_Call(IARM_BUS_TR69HOSTIFMGR_NAME, IARM_BUS_TR69HOSTIFMGR_API_GetParams, (void *)ptrParam, sizeof(HOSTIF_MsgData_t));


	if(ret != IARM_RESULT_SUCCESS) {
		RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"[%s:%s:%d] Failed in IARM_Bus_Call(), with return value: %d\n", __FILE__, __FUNCTION__, __LINE__, ret);
		return WAL_ERR_INVALID_PARAM;
	}
	else
	{
		 RDK_LOG(RDK_LOG_DEBUG,LOG_MOD_WEBPA,"[%s:%s:%d] The value for param: %s is %s paramLen : %d\n", __FILE__, __FUNCTION__, __LINE__, ptrParam->paramName,ptrParam->paramValue, ptrParam->paramLen);
	}

	return WAL_SUCCESS;
}


static int GetParamInfo(const char *pParameterName, ParamVal ***parametervalArr,int *TotalParams)
{
	//Check if pParameterName is in the tree and convert to a list if a wildcard/branch
	const char wcard = '*'; // TODO: Currently support only wildcard character *
	int i = 0;
	int ret = WAL_SUCCESS;
	DB_STATUS dbRet = DB_FAILURE;
	HOSTIF_MsgData_t Param = {0};
	memset(&Param, '\0', sizeof(HOSTIF_MsgData_t));

	if(dbhandle)
	{
		if(strchr(pParameterName,wcard))
		{
			char **getParamList;
			char **ParamDataTypeList;
			int paramCount = 0;

			/* Translate wildcard to list of parameters */
			getParamList = (char **) malloc(MAX_NUM_PARAMETERS * sizeof(char *));
			ParamDataTypeList = (char **) malloc(MAX_NUM_PARAMETERS * sizeof(char *));
			dbRet = getParameterList((void *)dbhandle,pParameterName,getParamList,ParamDataTypeList,&paramCount);
			*TotalParams = paramCount;
			parametervalArr[0] = (ParamVal **) malloc(paramCount * sizeof(ParamVal*));

			for(i = 0; i < paramCount; i++)
			{
				RDK_LOG(RDK_LOG_DEBUG,LOG_MOD_WEBPA,"%s:%s\n",getParamList[i],ParamDataTypeList[i]);
				strncpy(Param.paramName,getParamList[i],strlen(getParamList[i])+1);

				// Convert ParamDataType to hostIf datatype
				converttohostIfType(ParamDataTypeList[i],&(Param.paramtype));
				Param.instanceNum = 0;
				parametervalArr[0][i]=malloc(sizeof(ParamVal));
				// Convert Param.paramtype to ParamVal.type
				converttoWalType(Param.paramtype,&(parametervalArr[0][i]->type));


				ret = get_ParamValues_tr69hostIf(&Param);

				if(ret == WAL_SUCCESS)
				{
					parametervalArr[0][i]->name=malloc(sizeof(char)*strlen(Param.paramName)+1);
					parametervalArr[0][i]->value=malloc(sizeof(char)*strlen(Param.paramValue)+1);
					char *ptrtovalue = parametervalArr[0][i]->value;
					char *ptrtoname = parametervalArr[0][i]->name;
					strncpy(ptrtoname,Param.paramName, strlen(Param.paramName));
					ptrtoname[strlen(Param.paramName)] = '\0';
					strncpy(ptrtovalue,Param.paramValue, strlen(Param.paramValue));
					ptrtovalue[strlen(Param.paramValue)] = '\0';
				}
				else
				{
					ret = WAL_FAILURE;
				}
				free(getParamList[i]);
				free(ParamDataTypeList[i]);
			}
			free(getParamList);
			free(ParamDataTypeList);
		}
		else /* No wildcard, check whether given parameter is valid */
		{
			char *dataType = malloc(sizeof(char) * MAX_DATATYPE_LENGTH);
			parametervalArr[0] = (ParamVal **) malloc(sizeof(ParamVal*));
			memset(parametervalArr[0],0,sizeof(ParamVal*));
			parametervalArr[0][0]=malloc(sizeof(ParamVal));
			memset(parametervalArr[0][0],0,sizeof(ParamVal));
			parametervalArr[0][0]->name = NULL;
			parametervalArr[0][0]->value = NULL;
			parametervalArr[0][0]->type = 0;
			
			if(isParameterValid((void *)dbhandle,pParameterName,dataType))
			{
				*TotalParams = 1;
				strncpy(Param.paramName,pParameterName,strlen(pParameterName)+1);
				converttohostIfType(dataType,&(Param.paramtype));
				Param.instanceNum = 0;
				// Convert Param.paramtype to ParamVal.type
				converttoWalType(Param.paramtype,&(parametervalArr[0][0]->type));

				ret = get_ParamValues_tr69hostIf(&Param);
				if(ret == WAL_SUCCESS)
				{
					parametervalArr[0][0]->name=malloc(sizeof(char)*strlen(Param.paramName)+1);
					parametervalArr[0][0]->value=malloc(sizeof(char)*strlen(Param.paramValue)+1);
					char *ptrtovalue = parametervalArr[0][0]->value;
					strncpy(ptrtovalue,Param.paramValue, strlen(Param.paramValue));
					ptrtovalue[strlen(Param.paramValue)] = '\0';
					char *ptrtoname = parametervalArr[0][0]->name;
					strncpy(ptrtoname,Param.paramName, strlen(Param.paramName));
					ptrtoname[strlen(Param.paramName)] = '\0';
					RDK_LOG(RDK_LOG_DEBUG,LOG_MOD_WEBPA,"CMCSA:: GetParamInfo value is %s ptrtovalue %s\n",parametervalArr[0][0]->value,ptrtovalue);
				}
				else
				{
					RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"get_ParamValues_tr69hostIf failed:ret is %d\n",ret);
					ret = WAL_FAILURE;
				}
			}
			else
			{
				return WAL_ERR_INVALID_PARAMETER_NAME;
			}
			free(dataType);
		}
	}
	return ret;
}

/**
 * @brief setValues interface sets the parameter value.
 *
 * @param[in] paramVal List of Parameter name/value pairs.
 * @param[in] paramCount Number of parameters.
 * @param[out] retStatus List of Return status.
 */
void setValues(const ParamVal paramVal[], const unsigned int paramCount, const unsigned int isAtomic, WAL_STATUS *retStatus)
//void setValues(const ParamVal paramVal[], int paramCount, WAL_STATUS *retStatus)
{
	int cnt=0;
	for(cnt = 0; cnt < paramCount; cnt++)
	{
		retStatus[cnt] = SetParamInfo(paramVal[cnt]);
	}
}

/**
 * generic Api for get HostIf parameters through IARM_TR69Bus
**/
static int set_ParamValues_tr69hostIf (HOSTIF_MsgData_t param)
{
	IARM_Result_t ret = IARM_RESULT_IPCCORE_FAIL;
	param.reqType = HOSTIF_SET;
	ret = IARM_Bus_Call(IARM_BUS_TR69HOSTIFMGR_NAME,
	                    IARM_BUS_TR69HOSTIFMGR_API_SetParams,
	                    (void *)&param,
	                    sizeof(param));
	if(ret != IARM_RESULT_SUCCESS) {
		RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"[%s:%s:%d] Failed in IARM_Bus_Call(), with return value: %d\n", __FILE__, __FUNCTION__, __LINE__, ret);
		return WAL_ERR_INVALID_PARAMETER_NAME;
	}
	else
	{
		RDK_LOG(RDK_LOG_DEBUG,LOG_MOD_WEBPA,"[%s:%s:%d] Set Successful for value : %s\n", __FILE__, __FUNCTION__, __LINE__, (char *)param.paramValue);
	}
	return WAL_SUCCESS;
}

static int SetParamInfo(ParamVal paramVal)
{

	int ret = WAL_SUCCESS;
	HOSTIF_MsgData_t Param = {0};
	memset(&Param, '\0', sizeof(HOSTIF_MsgData_t));

	char value[100] = { 0 };
	char paramName[100] = { 0 };


	strncpy(Param.paramName, paramVal.name,strlen(paramVal.name));
	strncpy(Param.paramValue, paramVal.value,strlen(paramVal.value));

	/* Convert DATA_TYPE to ParamType */
	switch(paramVal.type)
	{
	case WAL_STRING:
		Param.paramtype = hostIf_StringType;
		break;
	case WAL_INT:
		Param.paramtype = hostIf_IntegerType;
		break;
	case WAL_UINT:
		Param.paramtype = hostIf_UnsignedIntType;
		break;
	case WAL_BOOLEAN:
		Param.paramtype = hostIf_BooleanType;
		break;
	case WAL_DATETIME:
		Param.paramtype = hostIf_DateTimeType;
		break;
	case WAL_BASE64:
//		Param.paramtype = Base64Type;
		break;
	case WAL_LONG:
		Param.paramtype = hostIf_UnsignedLongType; // TODO: No LongType in Parameter Type?
		break;
	case WAL_ULONG:
		Param.paramtype = hostIf_UnsignedLongType;
		break;
	case WAL_FLOAT:
		//type = ; TODO
		break;
	case WAL_DOUBLE:
		//type = ; TODO
		break;
	case WAL_BYTE:
		//type = ; TODO
		break;
	case WAL_NONE:
		//type = ; TODO
		break;
	default:
		break;
	}

	ret = set_ParamValues_tr69hostIf(Param);
	RDK_LOG(RDK_LOG_DEBUG,LOG_MOD_WEBPA,"set_ParamValues_tr69hostIf %d\n",ret);
	return ret;
}

/**
 *
 *
 * @param[in] paramName List of Parameters.
 * @param[in] paramCount Number of parameters.
 * @param[out] attr Two dimentional array of attribute name/value pairs.
 * @param[out] retAttrCount List of "number of attributes" for each input paramName.
 * @param[out] retStatus List of Return status.
 */
void getAttributes(const char *paramName[], const unsigned int paramCount, AttrVal ***attr, int *retAttrCount, WAL_STATUS *retStatus)
//void getAttributes(const char *paramName[], int paramCount, AttrVal ***attr, int *retAttrCount, WAL_STATUS *retStatus)
{
	int cnt=0;
	for(cnt=0; cnt<paramCount; cnt++)
	{
		retStatus[cnt]=getParamAttributes(paramName[cnt], &attr[cnt], &retAttrCount[cnt]);
		RDK_LOG(RDK_LOG_DEBUG,LOG_MOD_WEBPA,"Parameter Name: %s, Parameter Attributes return: %d\n",paramName[cnt],retStatus[cnt]);
	}
}

static int getParamAttributes(const char *pParameterName, AttrVal ***attr, int *TotalParams)
{
	// TODO:Implement Attributes
}

/**
 * @brief setAttributes interface sets the attribute values.
 *
 * @param[in] paramName List of Parameters.
 * @param[in] paramCount Number of parameters.
 * @param[in] attr List of attribute name/value pairs.
 * @param[out] retStatus List of Return status.
 */
void setAttributes(const char *paramName[], const unsigned int paramCount, const AttrVal *attr[], WAL_STATUS *retStatus)
//void setAttributes(const char *paramName[], int paramCount, const AttrVal *attr[], WAL_STATUS *retStatus)
{
	int cnt=0;
	for(cnt=0; cnt<paramCount; cnt++)
	{
		retStatus[cnt] = setParamAttributes(paramName[cnt],attr[cnt]);
	}
}

static int setParamAttributes(const char *pParameterName, const AttrVal *attArr)
{
	// TODO:Implement Attributes
}

/* int main ( int arc, char **argv )
{
	// TODO:Implement main
	return 0;
}
*/
