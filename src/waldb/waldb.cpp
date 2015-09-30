#include <iostream>
#include <string>
#include "waldb.h"
#include "tinyxml.h"
#include "stdlib.h"

#define MAX_PARAMETER_LENGTH 512
#define MAX_NUM_PARAMETERS 2048

static void getList(TiXmlNode *pParent,char *paramName,char **ptrParamList,int *paramCount);

/* @brief Loads the data-model xml data
 *
 * @filename[in] data-model xml filename (with absolute path)
 * @dbhandle[out] database handle
 * @return DB_STATUS
 */
DB_STATUS loaddb(const char *filename,void *dbhandle)
{
	TiXmlDocument *doc = new TiXmlDocument(filename);
	bool loadOK = doc->LoadFile();
    int *dbhdl = (int *)dbhandle;
	if( loadOK )
	{
		*dbhdl = (int) doc;
		return DB_SUCCESS;
	}
	else
	{
		*dbhdl = 0;
		return DB_FAILURE;
	}
}

static void getList(TiXmlNode *pParent,char *paramName,char **ptrParamList,int *paramCount)
{
	static int matched = 0;
	if(!pParent)
	{
		matched = 0;
		return;
	}

	TiXmlNode* pChild;

	static int isObject = 0;
	static char ObjectName[MAX_PARAMETER_LENGTH];
	char ParameterName[MAX_PARAMETER_LENGTH];
	if(pParent->Type() == TiXmlNode::TINYXML_ELEMENT)
	{
		TiXmlElement* pElement =  pParent->ToElement();
		TiXmlAttribute* pAttrib = pElement->FirstAttribute();
		if(!strcmp(pParent->Value(),"object"))
		{
			isObject = 1;
		}
		if(pAttrib)
		{
			if(strstr(pAttrib->Value(),paramName))
			{
				strncpy(ObjectName,pAttrib->Value(),MAX_PARAMETER_LENGTH-1);
				ObjectName[MAX_PARAMETER_LENGTH]='\0';
				matched = 1;
			}
			if(matched || !isObject)
			{
				if(!strcmp(pParent->Value(),"parameter"))
				{
					isObject = 0;
					if(*paramCount <= MAX_NUM_PARAMETERS)
					{
						ptrParamList[*paramCount] = (char *) malloc(MAX_PARAMETER_LENGTH * sizeof(char));
						strncpy(ptrParamList[*paramCount],ObjectName,MAX_PARAMETER_LENGTH-1);
						strncat(ptrParamList[*paramCount],pAttrib->Value(),MAX_PARAMETER_LENGTH-1);
						ptrParamList[*paramCount][MAX_PARAMETER_LENGTH]='\0';
					}
					*paramCount = *paramCount + 1;
				}
			}
		}
	}

	for ( pChild = pParent->FirstChild(); pChild != 0; pChild = pChild->NextSibling())
	{
		getList(pChild,paramName,ptrParamList,paramCount);
	}
	matched = 0;
}
/* @brief Returns a parameter list and count given an input paramName with wildcard characters
 *
 * @dbhandle[in] database handle to query in to
 * @paramName[in] parameter name with wildcard(*)
 * @ParamList[out] parameter list extended by the input parameter
 * @paramCount[out] parameter count
 * @return DB_STATUS
 */
DB_STATUS getParameterList(void *dbhandle,char *paramName,char **ParamList,int *paramCount)
{
	const char wcard = '*';  //This API currently supports only paramName with * wildcard
	char parameterName[MAX_PARAMETER_LENGTH];
	strncpy(parameterName,paramName,MAX_PARAMETER_LENGTH-1);
	parameterName[MAX_PARAMETER_LENGTH]='\0';

	TiXmlDocument *doc = (TiXmlDocument *) dbhandle;
	if(strchr(parameterName,wcard))
	{
		// Remove the wildcard(*) from parameterName to populate list
		char *ptr = strchr(parameterName,wcard);
		*ptr = '\0';

		getList(doc,parameterName,ParamList,paramCount);

		if(*paramCount == 0)
	    {
	    	return DB_ERR_INVALID_PARAMETER;
	    }
    }
    else
    {
    	return DB_ERR_WILDCARD_NOT_SUPPORTED;
    }

    return DB_SUCCESS;
}

void checkforParameterMatch(TiXmlNode *pParent,char *paramName,int *pMatch)
{
    static int matched = 0;
    if(!pParent)
        return;

    TiXmlNode *pChild;
    static int isObject = 0;
    static char ObjectName[MAX_PARAMETER_LENGTH];
    char ParameterName[MAX_PARAMETER_LENGTH];

    if(pParent->Type() == TiXmlNode::TINYXML_ELEMENT)
    {
        TiXmlElement* pElement = pParent->ToElement();
        TiXmlAttribute* pAttrib = pElement->FirstAttribute();
        if(!strcmp(pParent->Value(),"object"))
            isObject = 1;

        if(pAttrib)
        {
            // Construct Object without parameter from input ParamName
            std::string str1(paramName);
            std::size_t found = str1.find_last_of(".");
            char paramObject[MAX_PARAMETER_LENGTH];
            strncpy(paramObject,paramName,found);
            paramObject[found]='.';
            paramObject[found+1]='\0';

            if(!strcmp(pAttrib->Value(),paramObject))
            {
                strncpy(ObjectName,pAttrib->Value(),MAX_PARAMETER_LENGTH-1);
                ObjectName[MAX_PARAMETER_LENGTH] = '\0';
                matched = 1;
            }

            if(matched || !isObject)
            {
                if(!strcmp(pParent->Value(),"parameter"))
                {
                    isObject = 0;
                    strncpy(ParameterName,ObjectName,MAX_PARAMETER_LENGTH-1);
                    strncat(ParameterName,pAttrib->Value(),MAX_PARAMETER_LENGTH-1);
                    ParameterName[MAX_PARAMETER_LENGTH] = '\0';
                    if(!strcmp(ParameterName,paramName))
                    {
                        *pMatch = 1;
                        return;
                    }
                }
            }
        }
    }

    for ( pChild = pParent->FirstChild(); pChild != 0; pChild = pChild->NextSibling())
    {
        checkforParameterMatch(pChild,paramName,pMatch);
    }
    matched = 0;
}

/* @brief Returns a parameter list and count given an input paramName with wildcard characters
 *
 * @filename[in] data-model xml filename (with absolute path)
 * @dbhandle[out] database handle
 * @return DB_STATUS
 */
int isParameterValid(void *dbhandle,char *paramName)
{
    int Match = 0;
    TiXmlDocument *doc = (TiXmlDocument *) dbhandle;
    checkforParameterMatch(doc,paramName,&Match);
    if(Match)
        return 1;
    else
        return 0;
}
