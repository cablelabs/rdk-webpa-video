#include "wal.h"
#include "libIBus.h"
#include "libIARM.h"
typedef unsigned int bool;
#include "hostIf_tr69ReqHandler.h"
#include <stdio.h>
#include <string.h>


static int get_ParamValues_tr69hostIf(HOSTIF_MsgData_t param);
static int GetParamInfo(const char *pParameterName, ParamVal ***parametervalArr,int *TotalParams);
static int set_ParamValues_tr69hostIf (HOSTIF_MsgData_t param);
static int SetParamInfo(ParamVal paramVal);
static int getParamAttributes(const char *pParameterName, AttrVal ***attr, int *TotalParams);
static int setParamAttributes(const char *pParameterName, const AttrVal *attArr);


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
		printf("Parameter Name: %s, Parameter Value return: %d\n",paramName[cnt],retStatus[cnt]);
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
	ret = IARM_Bus_Init(name);
	if(ret != IARM_RESULT_SUCCESS)
	{
		/*TODO: Log Error */
		return WAL_FAILURE;
	}

	ret = IARM_Bus_Connect();
	if(ret != IARM_RESULT_SUCCESS)
	{
		printf("Error connecting to IARM Bus for '%s'\r\n", name);
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
static int get_ParamValues_tr69hostIf(HOSTIF_MsgData_t param)
{
	IARM_Result_t ret = IARM_RESULT_IPCCORE_FAIL;
	printf("[%s:%d] Enter\n", __FUNCTION__, __LINE__ );

	param.reqType = HOSTIF_GET;

	ret = IARM_Bus_Call(IARM_BUS_TR69HOSTIFMGR_NAME, IARM_BUS_TR69HOSTIFMGR_API_GetParams, (void *)&param, sizeof(HOSTIF_MsgData_t));


	if(ret != IARM_RESULT_SUCCESS) {
		printf("[%s:%s:%d] Failed in IARM_Bus_Call(), with return value: %d\n", __FILE__, __FUNCTION__, __LINE__, ret);
		return WAL_ERR_INVALID_PARAM;
	}
	else
	{
		printf("[%s:%s:%d] The value of param: %s paramLen : %d\n", __FILE__, __FUNCTION__, __LINE__, param.paramName, param.paramLen);
	}

	return WAL_SUCCESS;
}


static int GetParamInfo(const char *pParameterName, ParamVal ***parametervalArr,int *TotalParams)
{
	//TODO:: Check if pParameterName is in the tree and convert to a list if a wildcard/branch
	int ret = WAL_SUCCESS;
	HOSTIF_MsgData_t Param = {0};
	memset(&Param, '\0', sizeof(HOSTIF_MsgData_t));

	strncpy(Param.paramName,pParameterName,strlen(pParameterName)+1);
	Param.paramtype=hostIf_StringType;
	Param.instanceNum = 0;

	// TODO: What about array of values
	ret = get_ParamValues_tr69hostIf(Param);

	if(ret == WAL_SUCCESS)
	{
		parametervalArr[0] = (ParamVal **) malloc(sizeof(ParamVal*));
		// TODO: What about array of values
		//Allocate value and copy
		parametervalArr[0][0]->value=malloc(sizeof(char)*strlen(Param.paramValue));
		strncpy((parametervalArr[0][0])->value,Param.paramValue, strlen(Param.paramValue));
	}
	else
	{
		printf("get_ParamValues_tr69hostIf failed:ret is %d\n",ret);
	}
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
		printf("[%s:%s:%d] Failed in IARM_Bus_Call(), with return value: %d\n", __FILE__, __FUNCTION__, __LINE__, ret);
		return WAL_ERR_INVALID_PARAMETER_NAME;
	}
	else
	{
		printf("[%s:%s:%d] Set Successful for value : %s\n", __FILE__, __FUNCTION__, __LINE__, (char *)param.paramValue);
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
	switch(Param.paramtype)
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
		printf("Parameter Name: %s, Parameter Attributes return: %d\n",paramName[cnt],retStatus[cnt]);
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

int main ( int arc, char **argv )
{
     // TODO:Implement main
     return 0;
}
