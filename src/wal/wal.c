#include "wal.h"
#include "waldb/waldb.h"
#include "libIBus.h"
#include "libIARM.h"
typedef unsigned int bool;
#include "hostIf_tr69ReqHandler.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "rdk_debug.h"

#include "wal_internal.h"
#define MAX_NUM_PARAMETERS 2048
#define MAX_DATATYPE_LENGTH 48
#define MAX_PARAMETER_LENGTH 512
#define MAX_PARAMETERVALUE_LEN 128
#define MAX_PARAM_LENGTH TR69HOSTIFMGR_MAX_PARAM_LEN

/* WebPA Configuration for RDKV */
#define RDKV_WEBPA_COMPONENT_NAME            "webpaagent"
#define RDKV_WEBPA_CFG_FILE                  "/etc/webpa_cfg.json"
#define RDKV_WEBPA_CFG_FILE_OVERRIDE         "/opt/webpa_cfg.json"
#define RDKV_WEBPA_CFG_FILE_SRC              "/etc/webpa_cfg.json"
#define RDKV_WEBPA_CFG_FILE_SRC_OVERRIDE     "/opt/webpa_cfg.json"
#define RDKV_WEBPA_CFG_DEVICE_INTERFACE      "not.defined"
#define RDKV_WEBPA_DEVICE_MAC                "Device.DeviceInfo.X_COMCAST-COM_STB_MAC"
#define RDKV_XPC_SYNC_PARAM_CID              "not.defined"
#define RDKV_XPC_SYNC_PARAM_CMC              "not.defined"
#define RDKV_XPC_SYNC_PARAM_SPV              "not.defined"
#define RDKV_FIRMWARE_VERSION                "Device.DeviceInfo.X_COMCAST-COM_FirmwareFilename"
#define RDKV_DEVICE_UP_TIME                  "Device.DeviceInfo.UpTime"
#define STR_NOT_DEFINED                      "not.defined"
#define LOG_MOD_WEBPA                        "LOG.RDK.WEBPAVIDEO"

static int get_ParamValues_tr69hostIf(HOSTIF_MsgData_t *param);
static int GetParamInfo (const char *pParameterName, ParamVal **parametervalPtrPtr, int *paramCountPtr);
static int set_ParamValues_tr69hostIf (HOSTIF_MsgData_t param);
static int SetParamInfo(ParamVal paramVal);
static int get_AttribValues_tr69hostIf(HOSTIF_MsgData_t *ptrParam);
static int getParamAttributes(const char *pParameterName, AttrVal ***attr, int *TotalParams);
static int set_AttribValues_tr69hostIf (HOSTIF_MsgData_t param);
static int setParamAttributes(const char *pParameterName, const AttrVal *attArr);
static void  _tr69Event_handler(const char *owner, IARM_Bus_tr69HostIfMgr_EventId_t eventId, void *data, size_t len);
static int getNotificationValue(const char *pParameterName);
static void converttohostIfType(char *ParamDataType,HostIf_ParamType_t* pParamType);
static void converttoWalType(HostIf_ParamType_t paramType,DATA_TYPE* walType);
//static char *get_NetworkIfName(void);
static int g_dbhandle = 0;
void (*notifyCbFn)(NotifyData*) = NULL;
char **g_notifyParamList = NULL;
unsigned int g_notifyListSize = 0;
const char* webpaNotifyConfigFile = NULL;

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
 * @brief Initializes WebPA configuration file
 *
 * @return void.
 */
void setNotifyConfigurationFile(const char* nofityConfigFile)
{
    if(NULL != nofityConfigFile)
    {
	webpaNotifyConfigFile = nofityConfigFile;
	RDK_LOG(RDK_LOG_INFO,LOG_MOD_WEBPA,"Notify Configuration file set %s \n",webpaNotifyConfigFile);
    }
    else
    {
        RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"Unable to set Notify Configuration file\n");
    }
}

int getnotifyparamList(char ***notifyParamList,int *ptrnotifyListSize)
{
	char *temp_ptr = NULL;
	char *notifycfg_file_content = NULL;
	int i = 0;
	int ch_count = 0;
	FILE *fp = NULL;

	// Read file notification Configuration file
	if(NULL == webpaNotifyConfigFile)
	{
		RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"WebPA notification file path not set");
		return -1;
	}
	RDK_LOG(RDK_LOG_INFO,LOG_MOD_WEBPA,"Inside getnotifyparamList trying to open %s\n", webpaNotifyConfigFile);
	fp = fopen(webpaNotifyConfigFile, "r");
	if (fp == NULL)
	{
		RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"Failed to open cfg file %s\n", webpaNotifyConfigFile);
		return -1;
	}
	fseek(fp, 0, SEEK_END);
	ch_count = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	notifycfg_file_content = (char *) malloc(sizeof(char) * (ch_count + 1));
	fread(notifycfg_file_content, 1, ch_count,fp);
	notifycfg_file_content[ch_count] ='\0';
	fclose(fp);
	if(ch_count < 1)
	{
		RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"WebPA notification file is Empty %s\n", webpaNotifyConfigFile);
		return -1;
	}
	cJSON *notify_cfg = cJSON_Parse(notifycfg_file_content);
	cJSON *notifyArray = cJSON_GetObjectItem(notify_cfg,"Notify");
	if(NULL != notifyArray)
	{
	    *ptrnotifyListSize =(int)cJSON_GetArraySize(notifyArray);
	    //RDK_LOG(RDK_LOG_DEBUG,LOG_MOD_WEBPA,"Number of Notify Params = %d\n", *ptrnotifyListSize);
	    *notifyParamList = (char **)malloc(sizeof(char *) * *ptrnotifyListSize);
	    for (i = 0 ; i < cJSON_GetArraySize(notifyArray) ; i++)
	    {
		temp_ptr = cJSON_GetArrayItem(notifyArray, i)->valuestring;
		if(temp_ptr)
		{
		    (*notifyParamList)[i] = (char *)malloc(sizeof(char ) * (strlen(temp_ptr)+1));
		    strcpy((*notifyParamList)[i],temp_ptr);
		    RDK_LOG(RDK_LOG_DEBUG,LOG_MOD_WEBPA,"Notify Param  = %s\n", temp_ptr);
		}
	    }
	    // Update local Parameter list from generic layer
	    if(NULL != notifyParamList && NULL != ptrnotifyListSize)
	    {
		g_notifyParamList = *notifyParamList;
		g_notifyListSize = *ptrnotifyListSize;
		RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"Notification Param list if NULL");
	    }
	}
	else
	{
	    RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"Unable to parse Configuration file");
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
void getValues (const char *paramName[], const unsigned int paramCount, money_trace_spans *timeSpan, ParamVal ***paramValArr,
        int *retValCount, WAL_STATUS *retStatus)
{
    // Generic code mallocs paramValArr to hold paramCount items but only paramValArr[0] is ever accessed.

    // Generic code uses "paramValArr[0][cnt2]" (iterating over the 2nd dimension instead of the 1st). This means
    // paramValArr[0] (which is of type "ParamVal**") is expected to point to an array of "ParamVal*" objects
    paramValArr[0] = calloc (paramCount, sizeof(ParamVal*));

    int cnt = 0;
    int numParams = 0;
    for (cnt = 0; cnt < paramCount; cnt++)
    {
        // Because GetParamInfo is responsible for producing results (including wildcard explansion) for only 1 input
        // parameter, the address of the correct "ParamVal*" object from the above allocated array has to be given to
        // GetParamInfo for initialization. So GetParamInfo has to take a "ParamVal**" as input.
        retStatus[cnt] = GetParamInfo (paramName[cnt], &paramValArr[0][cnt], &numParams);
        retValCount[cnt] = numParams;
    }
}

/**
 * @brief Check if Parameter Name present in Notification list or not
 *
 * @param[in] pParameterName Name of the Parameter.
 * @param[out] retStatus 0 if present and 1 if not
 */
static int getNotificationValue(const char *pParameterName)
{
    // Check if Parameter Name present in Notification list or not
    int found = 0;
    int count;
    for(count = 0; count < g_notifyListSize; count++)
    {
        if((NULL != pParameterName) && (!strcmp(pParameterName,g_notifyParamList[count])))
        {
            found = 1;
            break;
        }
    }
    return found;
}
static void  _tr69Event_handler(const char *owner, IARM_Bus_tr69HostIfMgr_EventId_t eventId, void *data, size_t len)
{
    IARM_Bus_tr69HostIfMgr_EventData_t *tr69EventData = (IARM_Bus_tr69HostIfMgr_EventData_t *)data;
    ParamNotify *paramNotify = NULL;
    int isNotificationEnabled = 0;

    if (0 == strcmp(owner, IARM_BUS_TR69HOSTIFMGR_NAME))
    {
        switch (eventId)
        {
        /*case IARM_BUS_TR69HOSTIFMGR_EVENT_ADD:
            if(tr69EventData->paramName)  // Ignoring Add events Since it is not relevant for now.
            {
                paramNotify->paramName = tr69EventData->paramName;
            }
            if(tr69EventData->paramValue)
            {
                paramNotify->newValue = tr69EventData->paramValue;
            }
            //paramNotify->oldValue= val->oldValue;
            converttoWalType(tr69EventData->paramtype,&(paramNotify->type));
            break;*/
	/*case IARM_BUS_TR69HOSTIFMGR_EVENT_REMOVE:
            if(tr69EventData->paramName) // Ignoring Add events Since it is not Relevant for now.
            {
                paramNotify->paramName = tr69EventData->paramName;
            }
            if(tr69EventData->paramValue)
            {
                paramNotify->newValue = tr69EventData->paramValue;
            }
            //paramNotify->oldValue= val->oldValue;
            converttoWalType(tr69EventData->paramtype,&(paramNotify->type));
            break;*/
        case IARM_BUS_TR69HOSTIFMGR_EVENT_VALUECHANGED:
            isNotificationEnabled = getNotificationValue(tr69EventData->paramName);
            if(isNotificationEnabled)
            {
                paramNotify = (ParamNotify *) malloc(sizeof(ParamNotify));
        	memset(paramNotify,0,sizeof(ParamNotify));
                if(tr69EventData->paramName)
                {
		    paramNotify->paramName = tr69EventData->paramName;
		}
		if(tr69EventData->paramValue)
		{
		    paramNotify->newValue = tr69EventData->paramValue;
		}
		converttoWalType(tr69EventData->paramtype,&(paramNotify->type));
		//paramNotify->changeSource = mapWriteID(val->writeID);
       	   }
            break;
        default:
            break;
        }
    }

    if((notifyCbFn != NULL) && isNotificationEnabled && (eventId == IARM_BUS_TR69HOSTIFMGR_EVENT_VALUECHANGED))
    {
        NotifyData *notifyDataPtr = (NotifyData *) malloc(sizeof(NotifyData) * 1);
        if(NULL == notifyDataPtr)
        {
            if(paramNotify) WAL_FREE(paramNotify);
        }
        else
        {
            notifyDataPtr->type = NOTIFY_PARAM_VALUE_CHANGE;
            Notify_Data *notify_data = (Notify_Data *) malloc(sizeof(Notify_Data) * 1);
            if(NULL != notify_data)
            {
                notify_data->notify = paramNotify;
                notifyDataPtr->data = notify_data;
                RDK_LOG(RDK_LOG_INFO,LOG_MOD_WEBPA,"Notification forwarded for Parameter Name (%s) with Value (%s) and Data type (%d).\n",
                        paramNotify->paramName,  paramNotify->newValue, paramNotify->type);
                (*notifyCbFn)(notifyDataPtr);
            }
            else
            {
                if(paramNotify) WAL_FREE(paramNotify);
            }
        }
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
	IARM_Bus_RegisterEventHandler(IARM_BUS_TR69HOSTIFMGR_NAME, IARM_BUS_TR69HOSTIFMGR_EVENT_ADD,(IARM_EventHandler_t) _tr69Event_handler);
	IARM_Bus_RegisterEventHandler(IARM_BUS_TR69HOSTIFMGR_NAME, IARM_BUS_TR69HOSTIFMGR_EVENT_REMOVE,(IARM_EventHandler_t)_tr69Event_handler);
	IARM_Bus_RegisterEventHandler(IARM_BUS_TR69HOSTIFMGR_NAME, IARM_BUS_TR69HOSTIFMGR_EVENT_VALUECHANGED, (IARM_EventHandler_t)_tr69Event_handler);

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

static int GetParamInfo (const char *pParameterName, ParamVal **parametervalPtrPtr, int *paramCountPtr)
{
    //Check if pParameterName is in the tree and convert to a list if a wildcard/branch
    const char wcard = '*'; // TODO: Currently support only wildcard character *
    int i = 0;
    int ret = WAL_FAILURE;
    DB_STATUS dbRet = DB_FAILURE;
    HOSTIF_MsgData_t Param = { 0 };
	memset(&Param, '\0', sizeof(HOSTIF_MsgData_t));
#if 0
        /* Fake getvalues for SYNC Parameters*/
	if(strstr(pParameterName,"Device.DeviceInfo.Webpa."))
	{
		parametervalArr[0] = (ParamVal **) malloc(sizeof(ParamVal*));
                memset(parametervalArr[0],0,sizeof(ParamVal*));
                parametervalArr[0][0]=malloc(sizeof(ParamVal));
                memset(parametervalArr[0][0],0,sizeof(ParamVal));
                parametervalArr[0][0]->name = NULL;
                parametervalArr[0][0]->value = NULL;
		parametervalArr[0][0]->name=malloc(sizeof(char)*strlen(Param.paramName)+1);
		parametervalArr[0][0]->value=malloc(sizeof(char)*strlen(Param.paramValue)+1);
                parametervalArr[0][0]->type = WAL_UINT;
		strncpy(parametervalArr[0][i]->value,"2", strlen("2")); // 2 corresponds to CHANGED_BY_ACS
		return WAL_SUCCESS;
	}
#endif
    if (g_dbhandle)
    {
        ParamVal* parametervalPtr = NULL;

        if (strchr (pParameterName, wcard))
        {
            /* Translate wildcard to list of parameters */
            char **getParamList = (char**) calloc (MAX_NUM_PARAMETERS, sizeof(char*));
            char **ParamDataTypeList = (char**) calloc (MAX_NUM_PARAMETERS, sizeof(char*));
            if (getParamList == NULL || ParamDataTypeList == NULL)
            {
                RDK_LOG (RDK_LOG_ERROR, LOG_MOD_WEBPA, "Error allocating memory\n");
                ret = WAL_FAILURE;
                goto exit0;
            }

            *paramCountPtr = 0;
            dbRet = getParameterList ((void *) g_dbhandle, pParameterName, getParamList, ParamDataTypeList, paramCountPtr);

            // allocate parametervalPtr as an array of ParamVal elements (wildcard case)
            parametervalPtr = (ParamVal*) calloc (*paramCountPtr, sizeof(ParamVal));
            if (parametervalPtr == NULL)
            {
                RDK_LOG (RDK_LOG_ERROR, LOG_MOD_WEBPA, "Error allocating memory\n");
                ret = WAL_FAILURE;
                goto exit0;
            }

            for (i = 0; i < *paramCountPtr; i++)
            {
                RDK_LOG (RDK_LOG_DEBUG, LOG_MOD_WEBPA, "%s:%s\n", getParamList[i], ParamDataTypeList[i]);
                strncpy (Param.paramName, getParamList[i], MAX_PARAM_LENGTH - 1);
                Param.paramName[MAX_PARAM_LENGTH - 1] = '\0';

                // Convert ParamDataType to hostIf datatype
                converttohostIfType (ParamDataTypeList[i], &(Param.paramtype));
                Param.instanceNum = 0;

                // Convert Param.paramtype to ParamVal.type
                converttoWalType (Param.paramtype, &(parametervalPtr[i].type));

                ret = get_ParamValues_tr69hostIf (&Param);
                if (ret == WAL_SUCCESS)
                {
                    parametervalPtr[i].name = (char*) calloc (MAX_PARAM_LENGTH, sizeof(char));
                    parametervalPtr[i].value = (char*) calloc (MAX_PARAM_LENGTH, sizeof(char));
                    if (parametervalPtr[i].name == NULL || parametervalPtr[i].value == NULL)
                    {
                        RDK_LOG (RDK_LOG_ERROR, LOG_MOD_WEBPA, "Error allocating memory\n");
                        ret = WAL_FAILURE;
                        goto exit0;
                    }

                    strncat (parametervalPtr[i].name, Param.paramName, MAX_PARAM_LENGTH - 1);
                    strncat (parametervalPtr[i].value, Param.paramValue, MAX_PARAM_LENGTH - 1);
                }
                else
                {
                    ret = WAL_FAILURE;
                }
                free (getParamList[i]);
                free (ParamDataTypeList[i]);
            }
exit0:
            // For success case generic layer would free up parametervalPtr after consuming data
            if (ret != WAL_SUCCESS && parametervalPtr != NULL)
            {
                int j;
                for (j = 0; j < i; j++)
                {
                    if (parametervalPtr[j].name != NULL)
                    {
                        free (parametervalPtr[j].name);
                        parametervalPtr[j].name = NULL;
                    }
                    if (parametervalPtr[j].value != NULL)
                    {
                        free (parametervalPtr[j].value);
                        parametervalPtr[j].value = NULL;
                    }
                }
                free (parametervalPtr);
                parametervalPtr = NULL;
            }
            if (getParamList != NULL)
                free (getParamList);
            if (ParamDataTypeList != NULL)
                free (ParamDataTypeList);
        }
        else /* No wildcard, check whether given parameter is valid */
        {
            char *dataType = (char*) calloc (MAX_DATATYPE_LENGTH, sizeof(char));
            if (dataType == NULL)
            {
                RDK_LOG (RDK_LOG_ERROR, LOG_MOD_WEBPA, "Error allocating memory\n");
                ret = WAL_FAILURE;
                goto exit1;
            }
            // allocate parametervalPtr as a single ParamVal element (the usual case)
            parametervalPtr = (ParamVal*) calloc (1, sizeof(ParamVal));
            if (parametervalPtr == NULL)
            {
                RDK_LOG (RDK_LOG_ERROR, LOG_MOD_WEBPA, "Error allocating memory\n");
                ret = WAL_FAILURE;
                goto exit1;
            }

            if (isParameterValid ((void *) g_dbhandle, pParameterName, dataType))
            {
                *paramCountPtr = 1;
                strncpy (Param.paramName, pParameterName, MAX_PARAM_LENGTH - 1);
                Param.paramName[MAX_PARAM_LENGTH - 1] = '\0';

                converttohostIfType (dataType, &(Param.paramtype));
                Param.instanceNum = 0;

                // Convert Param.paramtype to ParamVal.type
                RDK_LOG (RDK_LOG_DEBUG, LOG_MOD_WEBPA, "CMCSA:: GetParamInfo Param.paramtype is %d\n", Param.paramtype);
                converttoWalType (Param.paramtype, &(parametervalPtr->type));

                ret = get_ParamValues_tr69hostIf (&Param);
                if (ret == WAL_SUCCESS)
                {
                    parametervalPtr->name = (char*) calloc (MAX_PARAM_LENGTH, sizeof(char));
                    parametervalPtr->value = (char*) calloc (MAX_PARAM_LENGTH, sizeof(char));
                    if (parametervalPtr->name == NULL || parametervalPtr->value == NULL)
                    {
                        RDK_LOG (RDK_LOG_ERROR, LOG_MOD_WEBPA, "Error allocating memory\n");
                        ret = WAL_FAILURE;
                        goto exit1;
                    }

                    strncat (parametervalPtr->name, Param.paramName, MAX_PARAM_LENGTH - 1);

                    switch (Param.paramtype)
                    {
                    case hostIf_IntegerType:
                    case hostIf_BooleanType:
                        snprintf (parametervalPtr->value, MAX_PARAM_LENGTH, "%d", *((int *) Param.paramValue));
                        break;
                    case hostIf_UnsignedIntType:
                        snprintf (parametervalPtr->value, MAX_PARAM_LENGTH, "%u", *((unsigned int *) Param.paramValue));
                        break;
                    case hostIf_UnsignedLongType:
                        snprintf (parametervalPtr->value, MAX_PARAM_LENGTH, "%u", *((unsigned long *) Param.paramValue));
                        break;
                    case hostIf_StringType:
                        strncat (parametervalPtr->value, Param.paramValue, MAX_PARAM_LENGTH - 1);
                        break;
                    default: // handle as string
                        strncat (parametervalPtr->value, Param.paramValue, MAX_PARAM_LENGTH - 1);
                        break;
                    }
                    RDK_LOG (RDK_LOG_DEBUG, LOG_MOD_WEBPA, "CMCSA:: GetParamInfo value is %s\n", parametervalPtr->value);
                }
                else
                {
                    RDK_LOG (RDK_LOG_ERROR, LOG_MOD_WEBPA, "get_ParamValues_tr69hostIf failed:ret is %d\n", ret);
                    ret = WAL_FAILURE;
                }
            }
            else
            {
                RDK_LOG (RDK_LOG_ERROR, LOG_MOD_WEBPA, " Invalid Parameter name %s\n", pParameterName);
                ret = WAL_ERR_INVALID_PARAMETER_NAME;
            }
exit1:
            // For success case generic layer would free up parametervalPtr after consuming data
            if (ret != WAL_SUCCESS && parametervalPtr != NULL)
            {
                if (parametervalPtr->name != NULL)
                {
                    free (parametervalPtr->name);
                    parametervalPtr->name = NULL;
                }
                if (parametervalPtr->value != NULL)
                {
                    free (parametervalPtr->value);
                    parametervalPtr->value = NULL;
                }
                free (parametervalPtr);
                parametervalPtr = NULL;
            }
            if (dataType != NULL)
            {
                free (dataType);
                dataType = NULL;
            }
        }
        *parametervalPtrPtr = parametervalPtr;
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
void setValues(const ParamVal paramVal[], const unsigned int paramCount, const unsigned int isAtomic, money_trace_spans *timeSpan, WAL_STATUS *retStatus,char * transaction_id)
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

	char *pdataType = NULL;
	pdataType = malloc(sizeof(char) * MAX_DATATYPE_LENGTH);
	if(pdataType == NULL)
	{
		RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"Error allocating memory\n");
		return WAL_FAILURE;
	}

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

        strncpy(Param.paramName, paramVal.name,MAX_PARAM_LENGTH-1);
        Param.paramName[MAX_PARAM_LENGTH-1]='\0';

        if (Param.paramtype == hostIf_BooleanType)
        {
            bool* boolPtr = (bool*) Param.paramValue;
            if (strcmp (paramVal.value, "1") == 0 || strcasecmp (paramVal.value, "true") == 0)
            {
                *boolPtr = 1;
            }
            else if (strcmp (paramVal.value, "0") == 0 || strcasecmp (paramVal.value, "false") == 0)
            {
                *boolPtr = 0;
            }
            else
            {
                return WAL_ERR_INVALID_PARAMETER_VALUE;
            }
        }
        else if (Param.paramtype == hostIf_IntegerType)
        {
            char *tailPtr;
            long int value = (int) strtol (paramVal.value, &tailPtr, 10);
            if (strlen (tailPtr)) // "whole" string cannot be interpreted as integer
                return WAL_ERR_INVALID_PARAMETER_VALUE;
            *((int*) Param.paramValue) = (int) value;
        }
        else if (Param.paramtype == hostIf_UnsignedIntType)
        {
            char *tailPtr;
            long int value = (int) strtol (paramVal.value, &tailPtr, 10);
            if (strlen (tailPtr) || value < 0) // "whole" string cannot be interpreted as unsigned integer
                return WAL_ERR_INVALID_PARAMETER_VALUE;
            *((int*) Param.paramValue) = (int) value;
        }
        else
        {
            strncpy(Param.paramValue, paramVal.value,MAX_PARAM_LENGTH-1);
            Param.paramValue[MAX_PARAM_LENGTH-1]='\0';
        }

        ret = set_ParamValues_tr69hostIf(Param);
        RDK_LOG(RDK_LOG_DEBUG,LOG_MOD_WEBPA,"set_ParamValues_tr69hostIf %d\n",ret);
    }
    else
    {
        RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA," Invalid Parameter name %s\n",paramVal.name);
        ret = WAL_ERR_INVALID_PARAMETER_NAME;
    }
    free (pdataType);
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
void getAttributes(const char *paramName[], const unsigned int paramCount, money_trace_spans *timeSpan, AttrVal ***attr, int *retAttrCount, WAL_STATUS *retStatus)
//void getAttributes(const char *paramName[], int paramCount, AttrVal ***attr, int *retAttrCount, WAL_STATUS *retStatus)
{
	int cnt=0;
	for(cnt=0; cnt<paramCount; cnt++)
	{
		retStatus[cnt]=getParamAttributes(paramName[cnt], &attr[cnt], &retAttrCount[cnt]);
		RDK_LOG(RDK_LOG_DEBUG,LOG_MOD_WEBPA,"Parameter Name: %s, Parameter Attributes return: %d\n",paramName[cnt],retStatus[cnt]);
	}
}
static int get_AttribValues_tr69hostIf(HOSTIF_MsgData_t *ptrParam)
{
	IARM_Result_t ret = IARM_RESULT_IPCCORE_FAIL;

	ptrParam->reqType = HOSTIF_GETATTRIB;
	ret = IARM_Bus_Call(IARM_BUS_TR69HOSTIFMGR_NAME, IARM_BUS_TR69HOSTIFMGR_API_GetAttributes, (void *)ptrParam, sizeof(HOSTIF_MsgData_t));
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


static int getParamAttributes(const char *pParameterName, AttrVal ***attr, int *TotalParams)
{
    int ret = WAL_SUCCESS;
	int sizeAttrArr = 1; // Currently support only Notification parameter
	int i = 0;
    HOSTIF_MsgData_t Param = {0};

    memset(&Param, '\0', sizeof(HOSTIF_MsgData_t));

	// Check if pParameterName is in the list of notification parameters and check if the parameter is one among them
	int found = 0;
	for(i = 0; i < g_notifyListSize; i++)
	{
		if(!strcmp(pParameterName,g_notifyParamList[i]))
		{
			found = 1;
			break;
		}
	}
	if(!found)
	{
		return WAL_ERR_INVALID_PARAM;
	}

	*TotalParams = sizeAttrArr;
	attr[0] = (AttrVal **) malloc(sizeof(AttrVal *) * sizeAttrArr);
	for(i = 0; i < sizeAttrArr; i++)
	{
      	attr[0][i] = (AttrVal *) malloc(sizeof(AttrVal) * 1);
              attr[0][i]->name = (char *) malloc(sizeof(char) * MAX_PARAMETER_LENGTH);
              attr[0][i]->value = (char *) malloc(sizeof(char) * MAX_PARAMETERVALUE_LEN);

		strcpy(attr[0][i]->name,pParameterName); // Currently only one attribute ie., notification, so use the parameter name to get its value
		/* Get Notification value for the parameter from hostif */
		strncpy(Param.paramName,pParameterName,strlen(pParameterName)+1);
		Param.instanceNum = 0;
		Param.paramtype = hostIf_IntegerType;
		ret = get_AttribValues_tr69hostIf(&Param);
        strncpy(attr[0][i]->value,Param.paramValue, strlen(Param.paramValue));
        attr[0][i]->value[strlen(Param.paramValue)] = '\0';
	 	attr[0][i]->type = WAL_INT; // Currently only notification which is a int
	}
	return WAL_SUCCESS;
}

/**
 * @brief setAttributes interface sets the attribute values.
 *
 * @param[in] paramName List of Parameters.
 * @param[in] paramCount Number of parameters.
 * @param[in] attr List of attribute name/value pairs.
 * @param[out] retStatus List of Return status.
 */
void setAttributes(const char *paramName[], const unsigned int paramCount, money_trace_spans *timeSpan, const AttrVal *attr[], WAL_STATUS *retStatus)
//void setAttributes(const char *paramName[], int paramCount, const AttrVal *attr[], WAL_STATUS *retStatus)
{
	RDK_LOG(RDK_LOG_INFO,LOG_MOD_WEBPA,"[%s:%s:%d] Inside setAttributes, Param Count = %d\n", __FILE__, __FUNCTION__, __LINE__,paramCount);
	int cnt=0;
	for(cnt=0; cnt<paramCount; cnt++)
	{
		retStatus[cnt] = setParamAttributes(paramName[cnt],attr[cnt]);
	}
}
/**
 * generic Api for set attribute HostIf parameters through IARM_TR69Bus
**/
static int set_AttribValues_tr69hostIf (HOSTIF_MsgData_t param)
{
    	RDK_LOG(RDK_LOG_DEBUG,LOG_MOD_WEBPA,"[%s:%s:%d] BEFORE IARM BUS CALL\n", __FILE__, __FUNCTION__, __LINE__);
	IARM_Result_t ret = IARM_RESULT_IPCCORE_FAIL;
	param.reqType = HOSTIF_SETATTRIB;

	// Try to set value
	ret = IARM_Bus_Call(IARM_BUS_TR69HOSTIFMGR_NAME,
						IARM_BUS_TR69HOSTIFMGR_API_SetAttributes,
						(void *)&param,
						sizeof(param));
	if(ret != IARM_RESULT_SUCCESS)
	{
		RDK_LOG(RDK_LOG_ERROR,LOG_MOD_WEBPA,"[%s:%s:%d] Failed in IARM_Bus_Call(), with return value: %d\n", __FILE__, __FUNCTION__, __LINE__, ret);
		return WAL_ERR_INVALID_PARAMETER_NAME;
	}
	else
	{
		RDK_LOG(RDK_LOG_INFO,LOG_MOD_WEBPA,"[%s:%s:%d] Set Successful for value : %s\n", __FILE__, __FUNCTION__, __LINE__, (char *)param.paramValue);
	}
	return WAL_SUCCESS;
}

static int setParamAttributes(const char *pParameterName, const AttrVal *attArr)
{
    int ret = WAL_SUCCESS;
    int i = 0;
    HOSTIF_MsgData_t Param = {0};
    memset(&Param, '\0', sizeof(HOSTIF_MsgData_t));
    // Enable only for notification parameters in the config file
    int found = 0;
    for(i = 0; i < g_notifyListSize; i++)
    {
            if(!strcmp(pParameterName,g_notifyParamList[i]))
            {
                    found = 1;
                    RDK_LOG(RDK_LOG_DEBUG,LOG_MOD_WEBPA,"[%s:%s:%d] Inside setParamAttributes, Param Found in Glist \n", __FILE__, __FUNCTION__, __LINE__,pParameterName);
                    break;
            }
    }
    if(!found)
    {
            return WAL_SUCCESS; //Fake success for all setattributes now
    }

    strcpy(Param.paramName, pParameterName);
    strcpy(Param.paramValue, attArr->value);
    Param.paramtype = hostIf_IntegerType;
    ret = set_AttribValues_tr69hostIf (Param);
    return ret;
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

char *getInterfaceNameFromConfig()
{
    char szBuf[256];
    static char szEthName[32] = "";
    FILE * fp = fopen("/etc/device.properties", "r");
    if(NULL != fp)
    {
        while(NULL != fgets(szBuf, sizeof(szBuf), fp))
        {
            char * pszTag = NULL;
            if(NULL != (pszTag = strstr(szBuf, "MOCA_INTERFACE")))
            {
                char * pszEqual = NULL;
                if(NULL != (pszEqual = strstr(pszTag, "=")))
                {
                    sscanf(pszEqual+1, "%s", szEthName);
                }
            }
        }
        fclose(fp);
    }
    return szEthName;
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
                        if (access(RDKV_WEBPA_CFG_FILE_OVERRIDE, F_OK) != -1)
                        {
                                ret = RDKV_WEBPA_CFG_FILE_OVERRIDE;
                        }
                        else
                        {
                                ret = RDKV_WEBPA_CFG_FILE;
                        }
			break;

		case WCFG_CFG_FILE_SRC:
                        if (access(RDKV_WEBPA_CFG_FILE_SRC_OVERRIDE, F_OK) != -1)
                        {
                                ret = RDKV_WEBPA_CFG_FILE_SRC_OVERRIDE;
                        }
                        else
                        {
                                ret = RDKV_WEBPA_CFG_FILE_SRC;
                        }

			break;

		case WCFG_DEVICE_INTERFACE:
		        RDK_LOG(RDK_LOG_INFO, LOG_MOD_WEBPA,"[%s] The WCFG_DEVICE_INTERFACE: %s\n", __FUNCTION__, STR_NOT_DEFINED);
			ret = STR_NOT_DEFINED;
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
                case WCFG_FIRMWARE_VERSION:
                        ret = RDKV_FIRMWARE_VERSION;
                        break;

                case WCFG_DEVICE_UP_TIME:
                        ret = RDKV_DEVICE_UP_TIME;
                        break;
		default:
			ret = STR_NOT_DEFINED;
	}

	return ret;
}

/**
 * @brief sendIoTMessage interface sends message to IoT.
 *
 * @param[in] msg Message to be sent to IoT.
 * @return WAL_STATUS
 */
WAL_STATUS sendIoTMessage(const void *msg)
{
	//TODO: Implement sendIoTMessage fn
}

void addRowTable(const char *objectName,TableData *list,char **retObject, WAL_STATUS *retStatus)
{
	// TODO: Implement ADD row API for TR181 dynamic table objects
}

void deleteRowTable(const char *object,WAL_STATUS *retStatus)
{
	// TODO: Implement DELETE row API for TR181 dynamic table objects
}

void replaceTable(const char *objectName,TableData * list,int paramcount,WAL_STATUS *retStatus)
{
	// TODO: Implement REPLACE table (DELETE and ADD) API for TR181 dynamic table objects
}

void WALInit()
{
	// TODO: Implement any initialization required for webpa
}

void waitForConnectReadyCondition()
{
	// TODO: Implement wait for any dependent components for webpa to be ready to connect to server
}

void waitForOperationalReadyCondition()
{
	// TODO: Implement wait for any dependent components for webpa to be operational
}

void getNotifyParamList(const char ***paramList,int *size)
{

    RDK_LOG(RDK_LOG_INFO,LOG_MOD_WEBPA,"[%s:%s:%d] Initializing Notification parameters\n", __FILE__, __FUNCTION__, __LINE__);
    getnotifyparamList(paramList,size);
}

/* int main ( int arc, char **argv )
{
	// TODO:Implement main
	return 0;
}
*/

/* Get network interface from device properties */
#if 0
char *get_NetworkIfName( void )
{
    static char *curIfName = NULL;
    RDK_LOG(RDK_LOG_TRACE1, LOG_MOD_WEBPA,"[%s] Enter %s\n", __FUNCTION__);
    if (!curIfName)
    {
	curIfName = ((access( "/tmp/wifi-on", F_OK ) != 0 ) ? getenv("MOCA_INTERFACE") : getenv("WIFI_INTERFACE"));
	RDK_LOG(RDK_LOG_DEBUG ,LOG_MOD_WEBPA," [%s] Interface Name : %s\n", __FUNCTION__, curIfName);
    }

    RDK_LOG(RDK_LOG_TRACE1, LOG_MOD_WEBPA,"[%s]Exit %s\n", __FUNCTION__);
    return curIfName;
}
#endif

