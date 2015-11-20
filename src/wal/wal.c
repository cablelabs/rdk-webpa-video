#include "wal.h"
#include "waldb/waldb.h"
#include "libIBus.h"
#include "libIARM.h"
typedef unsigned int bool;
#include "hostIf_tr69ReqHandler.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "rdk_debug.h"

#define MAX_NUM_PARAMETERS 2048
#define MAX_DATATYPE_LENGTH 48

/* WebPA Configuration for RDKV */
#define RDKV_WEBPA_COMPONENT_NAME            "webpaagent"
#define RDKV_WEBPA_CFG_FILE                  "/etc/webpa_cfg.json"
#define RDKV_WEBPA_CFG_FILE_SRC              "/etc/webpa_cfg.json"
#define RDKV_WEBPA_DEVICE_MAC                "Device.DeviceInfo.X_COMCAST-COM_STB_MAC"
#define RDKV_XPC_SYNC_PARAM_CID              "not.defined"
#define RDKV_XPC_SYNC_PARAM_CMC              "not.defined"
#define RDKV_XPC_SYNC_PARAM_SPV              "not.defined"
#define STR_NOT_DEFINED                      "Not Defined"
#define LOG_MOD_WEBPA                        "LOG.RDK.WEBPAVIDEO"

static int get_ParamValues_tr69hostIf(HOSTIF_MsgData_t *param);
static int GetParamInfo(const char *pParameterName, ParamVal ***parametervalArr,int *TotalParams);
static int set_ParamValues_tr69hostIf (HOSTIF_MsgData_t param);
static int SetParamInfo(ParamVal paramVal);
static int getParamAttributes(const char *pParameterName, AttrVal ***attr, int *TotalParams);
static int setParamAttributes(const char *pParameterName, const AttrVal *attArr);
static void  _tr69Event_handler(const char *owner, IARM_Bus_tr69HostIfMgr_EventId_t eventId, void *data, size_t len);

static void converttohostIfType(char *ParamDataType,HostIf_ParamType_t* pParamType);
static void converttoWalType(HostIf_ParamType_t paramType,DATA_TYPE* walType);

static int g_dbhandle = 0;
void (*notifyCbFn)(ParamNotify*) = NULL;

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

	/* Register for IARM Events */
	IARM_Bus_RegisterEventHandler(IARM_BUS_TR69HOSTIFMGR_NAME, IARM_BUS_TR69HOSTIFMGR_EVENT_ADD, _tr69Event_handler);
	IARM_Bus_RegisterEventHandler(IARM_BUS_TR69HOSTIFMGR_NAME, IARM_BUS_TR69HOSTIFMGR_EVENT_REMOVE, _tr69Event_handler);
	IARM_Bus_RegisterEventHandler(IARM_BUS_TR69HOSTIFMGR_NAME, IARM_BUS_TR69HOSTIFMGR_EVENT_VALUECHANGED, _tr69Event_handler);
	/* Register call for notify events */
	IARM_Bus_RegisterEvent(1);


	// Load Document model
	dbRet = loaddb("/etc/data-model.xml",(void *)&g_dbhandle);

	if(dbRet != DB_SUCCESS)
	{
		RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"Error loading database\n");
		return WAL_FAILURE;
	}
}

static void  _tr69Event_handler(const char *owner, IARM_Bus_tr69HostIfMgr_EventId_t eventId, void *data, size_t len)
{
	IARM_Bus_tr69HostIfMgr_EventData_t *tr69EventData = (IARM_Bus_tr69HostIfMgr_EventData_t *)data;
	ParamNotify *paramNotify = (ParamNotify *) malloc(sizeof(ParamNotify));
	memset(paramNotify,0,sizeof(ParamNotify));

	if (0 == strcmp(owner, IARM_BUS_TR69HOSTIFMGR_NAME))
	{
		switch (eventId)
		{
		case IARM_BUS_TR69HOSTIFMGR_EVENT_ADD:
			if(tr69EventData->paramName)
			{
				paramNotify->paramName = tr69EventData->paramName;
			}
			if(tr69EventData->paramValue)
			{
				paramNotify->newValue = tr69EventData->paramValue;
			}
			//paramNotify->oldValue= val->oldValue;
			converttoWalType(tr69EventData->paramtype,&(paramNotify->type));
			break;
		case IARM_BUS_TR69HOSTIFMGR_EVENT_REMOVE:
			if(tr69EventData->paramName)
			{
				paramNotify->paramName = tr69EventData->paramName;
			}
			if(tr69EventData->paramValue)
			{
				paramNotify->newValue = tr69EventData->paramValue;
			}
			//paramNotify->oldValue= val->oldValue;
			converttoWalType(tr69EventData->paramtype,&(paramNotify->type));
			break;
		case IARM_BUS_TR69HOSTIFMGR_EVENT_VALUECHANGED:
			if(tr69EventData->paramName)
			{
				paramNotify->paramName = tr69EventData->paramName;
			}
			if(tr69EventData->paramValue)
			{
				paramNotify->newValue = tr69EventData->paramValue;
			}
			converttoWalType(tr69EventData->paramtype,&(paramNotify->type));
//    	paramNotify->changeSource = mapWriteID(val->writeID);
			break;
		default:
			break;
		}
	}


	RDK_LOG(RDK_LOG_INFO,LOG_MOD_WEBPA,"Notification Event from stack: Parameter Name: %s, Old Value: %s, New Value: %s, Data Type: %d, Write ID: %d\n", paramNotify->paramName, paramNotify->oldValue, paramNotify->newValue, paramNotify->type, paramNotify->changeSource);

	if(notifyCbFn != NULL)
	{
		(*notifyCbFn)(paramNotify);
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
	notifyCbFn = cb;
	return WAL_SUCCESS;
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
	int ret = WAL_FAILURE;
	DB_STATUS dbRet = DB_FAILURE;
	HOSTIF_MsgData_t Param = {0};
	memset(&Param, '\0', sizeof(HOSTIF_MsgData_t));

	if(g_dbhandle)
	{
		if(strchr(pParameterName,wcard))
		{
			char **getParamList;
			char **ParamDataTypeList;
			int paramCount = 0;

			/* Translate wildcard to list of parameters */
			getParamList = (char **) malloc(MAX_NUM_PARAMETERS * sizeof(char *));
			ParamDataTypeList = (char **) malloc(MAX_NUM_PARAMETERS * sizeof(char *));
			dbRet = getParameterList((void *)g_dbhandle,pParameterName,getParamList,ParamDataTypeList,&paramCount);
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

			if(isParameterValid((void *)g_dbhandle,pParameterName,dataType))
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
				RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA," Invalid Parameter name %s\n",pParameterName);
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


	char *pdataType = malloc(sizeof(char) * MAX_DATATYPE_LENGTH);
	if(isParameterValid((void *)g_dbhandle,paramVal.name,pdataType))
	{
		DATA_TYPE walType;
		converttohostIfType(pdataType,&(Param.paramtype));
		// Convert Param.paramtype to ParamVal.type
		converttoWalType(Param.paramtype,&walType);

		if(walType != paramVal.type)
		{
			RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA," Invalid Parameter type for %s\n",paramVal.name);
			return WAL_ERR_INVALID_PARAMETER_TYPE;
		}

		strncpy(Param.paramName, paramVal.name,strlen(paramVal.name));
		strncpy(Param.paramValue, paramVal.value,strlen(paramVal.value));

		ret = set_ParamValues_tr69hostIf(Param);
		RDK_LOG(RDK_LOG_DEBUG,LOG_MOD_WEBPA,"set_ParamValues_tr69hostIf %d\n",ret);
	}
	else
	{
		RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA," Invalid Parameter name %s\n",paramVal.name);
		return WAL_ERR_INVALID_PARAMETER_NAME;
	}
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
	return 0;
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
	return 0;
}

/**
 * @brief _WEBPA_LOG WEBPA RDK Logger API
 *
 * @param[in] level LOG Level
 * @param[in] msg Message to be logged 
 */
void _WEBPA_LOG(unsigned int level, const char *msg, ...)
{
	va_list arg;
	int ret = 0;
	unsigned int rdkLogLevel = RDK_LOG_DEBUG;
	char *pTempChar = NULL;

	switch(level)
	{
		case WEBPA_LOG_ERROR:
			rdkLogLevel = RDK_LOG_ERROR;
			break;

		case WEBPA_LOG_INFO:
			rdkLogLevel = RDK_LOG_INFO;
			break;

		case WEBPA_LOG_PRINT:
			rdkLogLevel = RDK_LOG_DEBUG;
			break;
	}

	if( rdkLogLevel <= RDK_LOG_DEBUG )
	{
		pTempChar = (char*)malloc(4096);
		if(pTempChar)
		{
			va_start(arg, msg);
			ret = vsnprintf(pTempChar, 4096, msg,arg);
			va_end(arg);
			if(ret < 0)
			{
				perror(pTempChar);
			}
			RDK_LOG(rdkLogLevel,LOG_MOD_WEBPA, pTempChar);
			free(pTempChar);
		}
	}
}

/**
 * @brief getWebPAConfig interface returns the WebPA config data.
 *
 * @param[in] param WebPA config param name.
 * @return const char* WebPA config param value.
 */
const char* getWebPAConfig(WCFG_PARAM_NAME param)
{
	const char *ret = NULL;
	
	switch(param)
	{
		case WCFG_COMPONENT_NAME:
			ret = RDKV_WEBPA_COMPONENT_NAME;
			break;

		case WCFG_CFG_FILE:
			ret = RDKV_WEBPA_CFG_FILE;
			break;

		case WCFG_CFG_FILE_SRC:
			ret = RDKV_WEBPA_CFG_FILE_SRC;
			break;

		case WCFG_DEVICE_MAC:
			ret = RDKV_WEBPA_DEVICE_MAC;
			break;

		case WCFG_XPC_SYNC_PARAM_CID:
			ret = RDKV_XPC_SYNC_PARAM_CID;
			break;

		case WCFG_XPC_SYNC_PARAM_CMC:
			ret = RDKV_XPC_SYNC_PARAM_CMC;
			break;

		case WCFG_XPC_SYNC_PARAM_SPV:
			ret = RDKV_XPC_SYNC_PARAM_SPV;
			break;

		default:
			ret = STR_NOT_DEFINED;
	}
	
	return ret;
}

/* int main ( int arc, char **argv )
{
	// TODO:Implement main
	return 0;
}
*/
