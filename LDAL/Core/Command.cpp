#include "Command.h"
#include "Entity.h"
#include "ExecutionContext.h"
#include "ExecutionTemplate.h"
#include "Node.h"
#include "Value.h"
#include "Int.h"
#include "DateTime.h"
#include "EntityList.h"
#include "MemMan.h"
#include "Null.h"
#include "Utils.h"
#include "AdditionalFunctions.h"
#include "Bool.h"
#include "MetaData.h"
#include "DateTimeOperations.h"
#include <set>
#include <algorithm>
#include <functional>
#include <windows.h>
#include <mysql.h>
#include <MysqlConnector.h>
#include <math.h>

//Global variable
MYSQL *connection = nullptr;

int counttest=0;
int countfname = 0;
int countlname = 0;
int countfullname = 0;
int countday=0;
int countmonth=0;
int countyear=0;
int counthour=0;
int countminute=0;
int countseconds=0;
int counttelnum=0;
int countnic=0;
int countprice=0;
int countinteger=0;
int countaddress = 0;
int countpostalcode = 0;
int countemail = 0;

Command::Command()
        :ul_CommandType(COMMAND_TYPE_INVALID), p_Arg(0), p_EntityArg(0), s_AdditionalFuncName(EMPTY_STRING)
{

}

Command::~Command()
{

}

void Command::Destroy()
{
    if(0 != p_Arg)
    {
        p_Arg->Destroy();
    }
    if(0 != p_EntityArg)
    {
        p_EntityArg->Destroy();
    }
}

Command* Command::GetCopy()
{
    Command* pCopy = 0;
    MemoryManager::Inst.CreateObject(&pCopy);
    pCopy->SetType(ul_CommandType);
    if(0 != p_Arg)
    {
        pCopy->SetArg(p_Arg->GetCopy());
    }
    if(0 != p_EntityArg)
    {
        if(ENTITY_TYPE_NODE != p_EntityArg->ul_Type)
        {
            pCopy->SetEntityArg(p_EntityArg->GetCopy());
        }
        else
        {
            pCopy->SetEntityArg(p_EntityArg);
        }
    }
    pCopy->SetAdditionalFuncName(s_AdditionalFuncName);
    return pCopy;
}

void Command::SetType(MULONG ulType)
{
    ul_CommandType = ulType;
}

void Command::SetArg(ExecutionTemplate* pArg)
{
    p_Arg = pArg;
}

void Command::SetEntityArg(PENTITY pArg)
{
    p_EntityArg = pArg;
}

void Command::SetAdditionalFuncName(MSTRING sFun)
{
    s_AdditionalFuncName = sFun;
}

MSTRING Command::GetAdditionalFuncName() {
    return s_AdditionalFuncName;
}

PENTITY Command::Execute(PENTITY pEntity, ExecutionContext* pContext)
{

    if(COMMAND_TYPE_ADDITIONAL_FUNCTION == ul_CommandType)
    {
        // Additional function can be defined either in the control code or inside the script
        // First check whether it is a function defined in the script
        MAP_STR_EXECTMPLIST::const_iterator iteFind2 = pContext->p_mapFunctions->find(s_AdditionalFuncName);
        if(pContext->p_mapFunctions->end() != iteFind2)
        {
            ExecutionContext ec;
            ec.p_mapFunctions = pContext->p_mapFunctions;
            ec.p_MD = pContext->p_MD;
            ec.map_Var[pContext->p_MD->s_FuncArg] = pEntity;
            ((*iteFind2).second)->Execute(&ec);
            MAP_STR_ENTITYPTR::iterator iteFind3 = ec.map_Var.find(pContext->p_MD->s_FuncRet);
            if(ec.map_Var.end() == iteFind3)
            {
                return new String;  // a hack to return a dummy value when the return value is not expected
            }
            return (*iteFind3).second;
        }
        else
        {
            // Now try functions defined in control code
            AdditionalFunc fun = 0;
            MAP_STR_ADDITIONAL_FUNC::const_iterator iteFind = map_AdditionalFunctions.find(s_AdditionalFuncName);
            if(map_AdditionalFunctions.end() == iteFind)
            {
                return 0;
            }
            fun = (*iteFind).second;
            if(0 != p_Arg)
            {
                p_EntityArg = p_Arg->Execute(pContext);
            }
            return fun(p_EntityArg);
        }
    }
    else if(COMMAND_TYPE_STORE_AS_VARIABLE == ul_CommandType)
    {
        // This will change the execution context
        // Get a copy of the entity and add it as a new variable to the context
        // Variable name will be in the p_EntityArg
        PString pVarName = (PString)p_EntityArg;
        if(0 == pVarName)
        {
            return 0;
        }
        PENTITY pVar = pEntity;
        if(ENTITY_TYPE_NODE != pEntity->ul_Type)
        {
            pVar = pEntity->GetCopy();
        }
        MAP_STR_ENTITYPTR::iterator iteFind = pContext->map_Var.find(pVarName->GetValue());
        if(pContext->map_Var.end() != iteFind)
        {
            ((*iteFind).second)->Destroy();
        }
        pContext->map_Var[pVarName->GetValue()] = pVar;
        // Create a Null entity and return
        PNull pRet = 0;
        MemoryManager::Inst.CreateObject(&pRet);
        return pRet;
    }
    else
    {
        if (ENTITY_TYPE_LIST == pEntity->ul_Type) {
            if(0 != p_Arg)
            {
                p_EntityArg = p_Arg->Execute(pContext);
            }
            return ExecuteListCommand(ul_CommandType, pEntity, pContext, p_EntityArg);
        } else if (ENTITY_TYPE_NODE == pEntity->ul_Type) {
            return ExecuteNodeCommand(ul_CommandType, pEntity, pContext);
        } else {
            if(0 != p_Arg)
            {
                p_EntityArg = p_Arg->Execute(pContext);
            }
            return ExecuteEntityCommand(ul_CommandType, pEntity, p_EntityArg);
        }
    }
    return 0;
}

PENTITY Command::ExecuteEntityCommand(MULONG ulCommand, PENTITY pEntity, PENTITY pArg)
{
    // General functions in Entity level
    if(COMMAND_TYPE_IS_NULL == ulCommand)
    {
        PBool pBool = 0;
        MemoryManager::Inst.CreateObject(&pBool);
        pBool->SetValue(pEntity->IsNull());
        return pBool;
    }

    if(COMMAND_TYPE_IS_NOT_NULL == ulCommand)
    {
        PBool pBool = 0;
        MemoryManager::Inst.CreateObject(&pBool);
        pBool->SetValue(!pEntity->IsNull());
        return pBool;
    }

    switch (pEntity->ul_Type)
    {
        case ENTITY_TYPE_INT:
        {
            return ExecuteIntCommand(ulCommand, pEntity, pArg);
        }
        case ENTITY_TYPE_STRING:
        {
            return ExecuteStringCommand(ulCommand, pEntity, pArg);
        }
        case ENTITY_TYPE_BOOL:
        {
            return ExecuteBoolCommand(ulCommand, pEntity, pArg);
        }
        case ENTITY_TYPE_DATETIME:
        {
            return ExecuteDateTimeCommand(ulCommand, pEntity, pArg);
        }
    }
    return 0;
}

PENTITY Command::ExecuteIntCommand(MULONG ulCommand, PENTITY pEntity, PENTITY pArg)
{
    PInt pInt = (PInt)pEntity;
    if(0 == pInt)
    {
        return 0;
    }

    PBool pBoolRes = 0;
    PNull pNullRes = 0;
    PString pStrRes = 0;

    switch(ulCommand)
    {
        case COMMAND_TYPE_IS_INT_EQUAL_TO:
        {
            if(ENTITY_TYPE_INT == pArg->ul_Type)
            {
                MemoryManager::Inst.CreateObject(&pBoolRes);
                PInt pIntArg = (PInt)pArg;
                pBoolRes->SetValue(pInt->GetValue() == pIntArg->GetValue());
                break;
            }
            break;
        }
        case COMMAND_TYPE_IS_INT_MEMBER_OF:
        {
            if(ENTITY_TYPE_LIST == pArg->ul_Type)
            {
                MemoryManager::Inst.CreateObject(&pBoolRes);
                pBoolRes->SetValue(false);
                PENTITYLIST pIntListArg = (PENTITYLIST)pArg;
                EntityList::const_iterator ite1 = pIntListArg->begin();
                EntityList::const_iterator iteEnd1 = pIntListArg->end();
                for( ; ite1 != iteEnd1; ++ite1)
                {
                    if(((PInt)(*ite1))->GetValue() == pInt->GetValue())
                    {
                        pBoolRes->SetValue(true);
                        break;
                    }
                }
            }
            break;
        }
        case COMMAND_TYPE_IS_LESS_THAN:
        {
            if(ENTITY_TYPE_INT == pArg->ul_Type)
            {
                MemoryManager::Inst.CreateObject(&pBoolRes);
                PInt pIntArg = (PInt)pArg;
                pBoolRes->SetValue(pInt->GetValue() < pIntArg->GetValue());
            }
            break;
        }
        case COMMAND_TYPE_IS_LESS_THAN_OR_EQUAL_TO:
        {
            if(ENTITY_TYPE_INT == pArg->ul_Type)
            {
                MemoryManager::Inst.CreateObject(&pBoolRes);
                PInt pIntArg = (PInt)pArg;
                pBoolRes->SetValue(pInt->GetValue() <= pIntArg->GetValue());
            }
            break;
        }
        case COMMAND_TYPE_IS_GREATER_THAN:
        {
            if(ENTITY_TYPE_INT == pArg->ul_Type)
            {
                MemoryManager::Inst.CreateObject(&pBoolRes);
                PInt pIntArg = (PInt)pArg;
                pBoolRes->SetValue(pInt->GetValue() > pIntArg->GetValue());
            }
            break;
        }
        case COMMAND_TYPE_IS_GREATER_THAN_OR_EQUAL_TO:
        {
            if(ENTITY_TYPE_INT == pArg->ul_Type)
            {
                MemoryManager::Inst.CreateObject(&pBoolRes);
                PInt pIntArg = (PInt)pArg;
                pBoolRes->SetValue(pInt->GetValue() >= pIntArg->GetValue());
            }
            break;
        }
        case COMMAND_TYPE_ADD:
        {
            if(ENTITY_TYPE_INT == pArg->ul_Type)
            {
                MemoryManager::Inst.CreateObject(&pNullRes);
                PInt pIntArg = (PInt)pArg;
                MULONG ulVal = pInt->GetValue();
                ulVal += (pIntArg->GetValue());
                pInt->SetValue(ulVal);
            }
            break;
        }
        case COMMAND_TYPE_SUBTRACT:
        {
            if(ENTITY_TYPE_INT == pArg->ul_Type)
            {
                MemoryManager::Inst.CreateObject(&pNullRes);
                PInt pIntArg = (PInt)pArg;
                MULONG ulVal = pInt->GetValue();
                ulVal -= (pIntArg->GetValue());
                pInt->SetValue(ulVal);
            }
            break;
        }
        case COMMAND_TYPE_SET_INTEGER:
        {
            if(ENTITY_TYPE_INT == pArg->ul_Type)
            {
                MemoryManager::Inst.CreateObject(&pNullRes);
                PInt pIntArg = (PInt)pArg;
                MULONG ulVal = pIntArg->GetValue();
                pInt->SetValue(ulVal);
            }
            break;
        }
        case COMMAND_TYPE_TOSTRING:
        {
            MSTRINGSTREAM ss;
            ss<<pInt->GetValue();
            MemoryManager::Inst.CreateObject(&pStrRes);
            pStrRes->SetValue(ss.str());
            break;
        }
    }

    if(0 != pBoolRes)
    {
        return pBoolRes;
    }
    if(0 != pNullRes)
    {
        return pNullRes;
    }
    if(0 != pStrRes)
    {
        return pStrRes;
    }
    return 0;
}

PENTITY Command::ExecuteBoolCommand(MULONG ulCommand, PENTITY pEntity, PENTITY pArg)
{
    PBool pBool = (PBool)pEntity;
    if(0 == pBool)
    {
        return 0;
    }

    PBool pBoolRes = 0;
    PString pStrRes = 0;

    switch(ulCommand)
    {
        case COMMAND_TYPE_BOOL_AND:
        {
            PBool pBoolArg = (PBool)pArg;
            if(0 != pBoolArg)
            {
                MemoryManager::Inst.CreateObject(&pBoolRes);
                pBoolRes->SetValue(pBool->And(pBoolArg)->GetValue());
            }
            break;
        }
        case COMMAND_TYPE_BOOL_OR:
        {
            PBool pBoolArg = (PBool)pArg;
            if(0 != pBoolArg)
            {
                MemoryManager::Inst.CreateObject(&pBoolRes);
                pBoolRes->SetValue(pBool->Or(pBoolArg)->GetValue());
            }
            break;
        }
        case COMMAND_TYPE_BOOLTOSTRING:
        {
            MemoryManager::Inst.CreateObject(&pStrRes);
            if (pBool->GetValue()) {
                pStrRes->SetValue("TRUE");
            }
            else {
                pStrRes->SetValue("FALSE");
            }
            break;
        }
        case COMMAND_TYPE_SET_BOOL:
        {
            if(ENTITY_TYPE_BOOL == pArg->ul_Type)
            {
                PBool pBoolArg = (PBool)pArg;
            }
//			MemoryManager::Inst.CreateObject(&pBoolRes);
//			pBoolRes->SetValue(pBoolArg);
//			pBool->SetValue(pBoolArg);
            break;
        }
        case COMMAND_TYPE_TO_FALSE:
        {
            pBool->SetValue(false);
            break;
        }
        case COMMAND_TYPE_TO_TRUE:
        {
            pBool->SetValue(true);
            break;
        }
    }


    if(0 != pBoolRes)
    {
        return pBoolRes;
    }
    if(0 != pStrRes)
    {
        return pStrRes;
    }

    return 0;
}

PENTITY Command::ExecuteStringCommand(MULONG ulCommand, PENTITY pEntity, PENTITY pArg)
{
    PNODE pNode = (PNODE)pEntity;
    if(0 == pNode)
    {
        return 0;
    }
    PString pString = (PString)pEntity;
    if(0 == pString)
    {
        return 0;
    }

    PNODE pNodeRes = 0;
    PInt pIntRes = 0;
    PNull pNullRes = 0;
    PBool pBoolRes = 0;
    PString pStrRes = 0;

    switch(ulCommand)
    {
        case COMMAND_TYPE_IS_STRING_EQUAL_TO:
        {
            if(ENTITY_TYPE_STRING == pArg->ul_Type)
            {
                MemoryManager::Inst.CreateObject(&pBoolRes);
                PString pStrArg = (PString)pArg;
                pBoolRes->SetValue(pString->GetValue() == pStrArg->GetValue());
            }
            break;
        }
        case COMMAND_TYPE_IS_STRING_MEMBER_OF:
        {
            if(ENTITY_TYPE_LIST == pArg->ul_Type)
            {
                MemoryManager::Inst.CreateObject(&pBoolRes);
                PENTITYLIST pStrListArg = (PENTITYLIST)pArg;
                MSTRING sVal = pString->GetValue();
                pBoolRes->SetValue(false);
                EntityList::const_iterator ite1 = pStrListArg->begin();
                EntityList::const_iterator iteEnd1 = pStrListArg->end();
                for( ; ite1 != iteEnd1; ++ite1)
                {
                    if(((PString)(*ite1))->GetValue() == sVal)
                    {
                        pBoolRes->SetValue(true);
                        break;
                    }
                }
            }
            break;
        }
        case COMMAND_TYPE_IS_HAVING_SUBSTRING:
        {
            if(ENTITY_TYPE_STRING == pArg->ul_Type)
            {
                MemoryManager::Inst.CreateObject(&pBoolRes);
                PString pStrArg = (PString)pArg;
                pBoolRes->SetValue(pString->GetValue().find(pStrArg->GetValue()) != MSTRING::npos);
            }
            break;
        }
        case COMMAND_TYPE_IS_HAVING_LEFT_SUBSTRING:
        {
            if(ENTITY_TYPE_STRING == pArg->ul_Type)
            {
                MemoryManager::Inst.CreateObject(&pBoolRes);
                PString pStrArg = (PString)pArg;
                MSTRING sArg = pStrArg->GetValue();
                pBoolRes->SetValue(pString->GetValue().substr(0, sArg.length()) == sArg);
            }
            break;
        }
        case COMMAND_TYPE_IS_HAVING_RIGHT_SUBSTRING:
        {
            if(ENTITY_TYPE_STRING == pArg->ul_Type)
            {
                MemoryManager::Inst.CreateObject(&pBoolRes);
                PString pStrArg = (PString)pArg;
                MSTRING sArg = pStrArg->GetValue();
                pBoolRes->SetValue(pString->GetValue().substr(pString->GetValue().length() - sArg.length(), sArg.length()) == sArg);
            }
            break;
        }
        case COMMAND_TYPE_ADD_PREFIX:
        {
            MemoryManager::Inst.CreateObject(&pNullRes);
            if(ENTITY_TYPE_STRING == pArg->ul_Type)
            {
                PString pStrArg = (PString)pArg;
                MSTRING sVal = pString->GetValue();
                sVal = pStrArg->GetValue() + sVal;
                pString->SetValue(sVal);
            }
            break;
        }
        case COMMAND_TYPE_ADD_POSTFIX:
        {
            MemoryManager::Inst.CreateObject(&pNullRes);
            if(ENTITY_TYPE_STRING == pArg->ul_Type)
            {
                PString pStrArg = (PString)pArg;
                MSTRING sVal = pString->GetValue();
                sVal += pStrArg->GetValue();
                pString->SetValue(sVal);
            }
            break;
        }
        case COMMAND_TYPE_TRIM_LEFT:
        {
            MemoryManager::Inst.CreateObject(&pNullRes);
            MSTRING sVal = pString->GetValue();
            Utils::TrimLeft(sVal, _MSTR( \t\n));
            pString->SetValue(sVal);
            break;
        }
        case COMMAND_TYPE_TRIM_RIGHT:
        {
            MemoryManager::Inst.CreateObject(&pNullRes);
            MSTRING sVal = pString->GetValue();
            Utils::TrimRight(sVal, _MSTR( \t\n));
            pString->SetValue(sVal);
            break;
        }
        case COMMAND_TYPE_WRITE_TO_FILE:
        {
            MemoryManager::Inst.CreateObject(&pNullRes);
            PString pStrArg = (PString)pArg;
            if(0 != pStrArg)
            {
                MOFSTREAM file;
                file.open(pStrArg->GetValue().c_str(), std::ios::out | std::ios::trunc);
                file<<(pString->GetValue().c_str());
                file.close();
            }
            break;
        }
        case COMMAND_TYPE_GET_LENGTH:
        {
            MemoryManager::Inst.CreateObject(&pIntRes);
            pIntRes->SetValue(pString->GetValue().length());
            break;
        }
        case COMMAND_TYPE_SECONDS_TO_MONTHS:
        {
            MemoryManager::Inst.CreateObject(&pStrRes);
            if (pString->GetValue() != "")
            {
                pStrRes->SetValue(std::to_string(DateTimeOperations::SecondsToMonths(stol(pString->ToString()))));
            }
            break;
        }
        case COMMAND_TYPE_SECONDS_TO_DAYS:
        {
            MemoryManager::Inst.CreateObject(&pStrRes);
            if (pString->GetValue() != "")
            {
                pStrRes->SetValue(std::to_string(DateTimeOperations::SecondsToDays(stol(pString->ToString()))));
            }
            break;
        }
        case COMMAND_TYPE_SECONDS_TO_YEARS:
        {
            MemoryManager::Inst.CreateObject(&pStrRes);
            if (pString->GetValue() != "")
            {
                pStrRes->SetValue(std::to_string(DateTimeOperations::SecondsToYears(stol(pString->ToString()))));
            }
            break;
        }
        case COMMAND_TYPE_GET_DIFFERENCE_BY_STRING:
        {
            MemoryManager::Inst.CreateObject(&pStrRes);
            if (pString->GetValue() != "")
            {
                String* pStrArg = (String*)pArg;
                pStrRes->SetValue(std::to_string(DateTimeOperations::GetDifferenceByString(pString->ToString(), pStrArg->ToString())));
            }
            break;
        }
        case COMMAND_TYPE_STRING_TO_READABLE_DATETIME:
        {
            MemoryManager::Inst.CreateObject(&pStrRes);
            if (pString->GetValue() != "")
            {
                pStrRes->SetValue(DateTimeOperations::StringToReadable(pString->GetValue()));
            }
            break;
        }
        case COMMAND_TYPE_GET_DAY_OF_THE_WEEK_SHORT_STRING:
        {
            MemoryManager::Inst.CreateObject(&pStrRes);
            if (pString->GetValue() != "")
            {
                pStrRes->SetValue(DateTimeOperations::GetDayOfTheWeekShortString(pString->GetValue()));
            }
            break;
        }
        case COMMAND_TYPE_GET_DAY_STRING:
        {
            MemoryManager::Inst.CreateObject(&pStrRes);
            if (pString->GetValue() != "")
            {
                pStrRes->SetValue(DateTimeOperations::GetDayString(pString->GetValue()));
            }
            break;
        }
        case COMMAND_TYPE_GET_MONTH_SHORT_STRING:
        {
            MemoryManager::Inst.CreateObject(&pStrRes);
            if (pString->GetValue() != "")
            {
                pStrRes->SetValue(DateTimeOperations::GetMonthShortString(pString->GetValue()));
            }
            break;
        }
        case COMMAND_TYPE_GET_TIME_24_HOUR_FORMAT:
        {
            MemoryManager::Inst.CreateObject(&pStrRes);
            if (pString->GetValue() != "")
            {
                pStrRes->SetValue(DateTimeOperations::GetTime24HourFormat(pString->GetValue()));
            }
            break;
        }
        case COMMAND_TYPE_GET_YEAR:
        {
            MemoryManager::Inst.CreateObject(&pStrRes);
            if (pString->GetValue() != "")
            {
                pStrRes->SetValue(DateTimeOperations::GetYear(pString->GetValue()));
            }
            break;
        }
        case COMMAND_TYPE_DATE_NOW:
        {
            MemoryManager::Inst.CreateObject(&pStrRes);
            if (pString->GetValue() != "")
            {
                pStrRes->SetValue(std::to_string(DateTimeOperations::GetDateNow()));
            }
            break;
        }
        case COMMAND_TYPE_STRING_TO_UNIX_TIME:
        {
            MemoryManager::Inst.CreateObject(&pStrRes);
            if (pString->GetValue() != "")
            {
                pStrRes->SetValue(std::to_string(DateTimeOperations::StringToUnix(pString->ToString())));
            }
            break;
        }
        case COMMAND_TYPE_STRINGTOINTEGER:
        {
            MemoryManager::Inst.CreateObject(&pIntRes);
            if (pString->GetValue() != "")
            {
                if (pString->GetValue().find_first_not_of("0123456789") == std::string::npos)
                {
                    try
                    {
                        pIntRes->SetValue(std::atoi(pString->GetValue().c_str()));
                    }
                    catch (...)
                    {
                        pIntRes->SetValue(0);
                    }
                }
                else
                {
                    pIntRes->SetValue(0);
                }
            }
            else
            {
                pIntRes->SetValue(0);
            }
            break;
        }
        case COMMAND_TYPE_STRINGTOBOOLEAN:
        {
            MemoryManager::Inst.CreateObject(&pBoolRes);
            if (pString->GetValue() != "")
            {
                std::string val = pString->GetValue();
                if (val.compare("true") == 0)
                {
                    pBoolRes->SetValue(1);
                }
                else
                {
                    pBoolRes->SetValue(0);
                }
            }
            break;
        }
        case COMMAND_TYPE_STRINGTOBOOL:
        {
            MemoryManager::Inst.CreateObject(&pBoolRes);
            PString pStrArg = (PString)pEntity;
            MSTRING sArg = pStrArg->GetValue();
            Utils::MakeLower(sArg);
            pBoolRes->SetValue(sArg == "true");
            break;
        }
        case COMMAND_TYPE_CONVERT_TO_SENTENCE_CASE:
        {
            MemoryManager::Inst.CreateObject(&pStrRes);
            PString pStrArg = (PString)pEntity;
            std::string str = pStrArg->GetValue();
            str[0] = std::toupper(str[0]);
            pStrRes->SetValue(str);
            break;
        }
        case COMMAND_TYPE_ADD_PERIOD:
        {
            MemoryManager::Inst.CreateObject(&pNullRes);
            PString pStrArg = (PString)pArg;
            MSTRING sVal = pString->GetValue();
            sVal += ".";
            pString->SetValue(sVal);
        }
            break;
    }
    if(0 != pNodeRes)
    {
        return pNodeRes;
    }

    if(0 != pIntRes)
    {
        return pIntRes;
    }
    if(0 != pNullRes)
    {
        return pNullRes;
    }
    if(0 != pBoolRes)
    {
        return pBoolRes;
    }
    if(0 != pStrRes)
    {
        return pStrRes;
    }

    return 0;
}

PENTITY Command::ExecuteDateTimeCommand(MULONG ulCommand, PENTITY pEntity, PENTITY pArg)
{
    PDateTime pDateTime = (PDateTime)pEntity;
    if(0 == pDateTime)
    {
        return 0;
    }

    PInt pIntRes = 0;
    PNull pNullRes = 0;
    PBool pBoolRes = 0;
    PString pStrRes = 0;
    PDateTime pDateTimeRes = 0;

//    switch(ulCommand)
//    {
//        case COMMAND_TYPE_DATETOSTRING:
//        {
//            MemoryManager::Inst.CreateObject(&pStrRes);
//            if (pDateTime->GetValue() != "")
//            {
//                //TODO
//            }
//        }
//        case COMMAND_TYPE_STRINGTODATE:
//        {
//            MemoryManager::Inst.CreateObject(&pStrRes);
//            if (pDateTime->GetValue() != "")
//            {
//                //TODO
//            }
//        }
//        case COMMAND_TYPE_DATEDIFFERENCE:
//        {
//            MemoryManager::Inst.CreateObject(&pStrRes);
//            if (pDateTime->GetValue() != "")
//            {
//                //TODO
//            }
//        }
//        case COMMAND_TYPE_DATENOW:
//        {
//            MemoryManager::Inst.CreateObject(&pStrRes);
//            if (pDateTime->GetValue() != "")
//            {
//                //TODO
//            }
//        }
//    }

    if(0 != pIntRes)
    {
        return pIntRes;
    }
    if(0 != pNullRes)
    {
        return pNullRes;
    }
    if(0 != pBoolRes)
    {
        return pBoolRes;
    }
    if(0 != pStrRes)
    {
        return pStrRes;
    }
    if(0 != pDateTimeRes)
    {
        return pDateTimeRes;
    }
    return 0;
}

PENTITY Command::ExecuteNodeCommand(MULONG ulCommand, PENTITY pEntity, ExecutionContext* pContext)
{
    PNODE pNode = (PNODE)pEntity;
    if(0 == pNode)
    {
        return 0;
    }

    PNODE pNodeRes = 0;
    PInt pIntRes = 0;
    PString pStrRes = 0;
    PENTITYLIST pNodeListRes = 0;
    PNull pNullRes = 0;
    PBool pBoolRes = 0;
    PENTITY pEntityRes = 0;

    // first handle the commands that would need to access the execution context
    if (COMMAND_TYPE_FILTER_SUBTREE == ulCommand) {
        MemoryManager::Inst.CreateObject(&pNodeListRes);
        FilterSubTree(pNode, p_Arg, pContext, pNodeListRes);
    } else {
        // now handle commands that would not explicitly need the execution context
        // for these command, for the sake of simplicity, we first evaluate the command argument and use it subsequently
        PENTITY pArg = 0;
        if(0 != p_Arg)
        {
            p_EntityArg = p_Arg->Execute(pContext);
            pArg = p_EntityArg;
        }
        MSTRING argument;
        MSTRING nodeString;
        MSTRING replacement;

        switch(ulCommand)
        {
            case COMMAND_TYPE_LEFT_SIBLING:
            {
                pNodeRes = pNode->GetLeftSibling();
                break;
            }
            case COMMAND_TYPE_RIGHT_SIBLING:
            {
                pNodeRes = pNode->GetRightSibling();
                break;
            }
            case COMMAND_TYPE_PARENT:
            {
                pNodeRes = pNode->GetParent();
                break;
            }
            case COMMAND_TYPE_FIRST_CHILD:
            {
                pNodeRes = pNode->GetFirstChild();
                break;
            }
            case COMMAND_TYPE_CHILDREN:
            {
                MemoryManager::Inst.CreateObject(&pNodeListRes);
                PNODE pChild = pNode->GetFirstChild();
                while(0 != pChild)
                {
                    pNodeListRes->push_back((PENTITY)pChild);
                    pChild = pChild->GetRightSibling();
                }
                break;
            }
            case COMMAND_TYPE_CHILD_COUNT:
            {
                MemoryManager::Inst.CreateObject(&pIntRes);
                pIntRes->SetValue(pNode->GetChildCount());
                break;
            }
            case COMMAND_TYPE_GET_VALUE:
            {
                MemoryManager::Inst.CreateObject(&pStrRes);
                pStrRes->SetValue(MSTRING(pNode->GetValue()));
                break;
            }
            case COMMAND_TYPE_GET_LVALUE:
            {
                MemoryManager::Inst.CreateObject(&pStrRes);
                pStrRes->SetValue(MSTRING(pNode->GetLVal()));
                break;
            }
            case COMMAND_TYPE_GET_RVALUE:
            {
                MemoryManager::Inst.CreateObject(&pStrRes);
                pStrRes->SetValue(MSTRING(pNode->GetRVal()));
                break;
            }
            case COMMAND_TYPE_GET_CUSTOM_STRING:
            {
                MemoryManager::Inst.CreateObject(&pStrRes);
                pStrRes->SetValue(MSTRING(pNode->GetCustomString()));
                break;
            }
            case COMMAND_TYPE_GET_ID:
            {
                MemoryManager::Inst.CreateObject(&pIntRes);
                pIntRes->SetValue(pNode->GetID());
                break;
            }
            case COMMAND_TYPE_GET_TYPE:
            {
                MemoryManager::Inst.CreateObject(&pIntRes);
                pIntRes->SetValue(pNode->GetType());
                break;
            }
            case COMMAND_TYPE_GET_NATURE:
            {
                MemoryManager::Inst.CreateObject(&pIntRes);
                pIntRes->SetValue(pNode->GetNature());
                break;
            }
            case COMMAND_TYPE_GET_WEIGHT:
            {
                MemoryManager::Inst.CreateObject(&pIntRes);
                pIntRes->SetValue(pNode->GetWeight());
                break;
            }
            case COMMAND_TYPE_GET_MIN_CHILD_WEIGHT:
            {
                MemoryManager::Inst.CreateObject(&pIntRes);
                pIntRes->SetValue(pNode->GetMinimumChildWeight());
                break;
            }
            case COMMAND_TYPE_GET_MAX_CHILD_WEIGHT:
            {
                MemoryManager::Inst.CreateObject(&pIntRes);
                pIntRes->SetValue(pNode->GetMaximumChildWeight());
                break;
            }
            case COMMAND_TYPE_SET_DB_STRING:
            {
                MemoryManager::Inst.CreateObject(&pNullRes);
                if(ENTITY_TYPE_STRING == pArg->ul_Type)
                {
                    String* pStrArg = (String*)pArg;
                    if(0 != pStrArg)
                    {
                        MSTRING dbString = pStrArg->GetValue();
                        std::string delimiter = "_";
                        std::size_t pos = 0;
                        pos = dbString.find(delimiter);
                        MSTRING hostname = dbString.substr(0, pos);
                        std::cout <<hostname<<"\n";
                        dbString.erase(0, pos + delimiter.length());
                        pos = dbString.find(delimiter);
                        MSTRING username = dbString.substr(0, pos);
                        std::cout <<username<<"\n";
                        dbString.erase(0, pos + delimiter.length());
                        pos = dbString.find(delimiter);
                        MSTRING password = dbString.substr(0, pos);
                        std::cout <<password<<"\n";
                        dbString.erase(0, pos + delimiter.length());
                        pos = dbString.find(delimiter);
                        MSTRING dbname = dbString.substr(0, pos);
                        std::cout <<dbname<<"\n";
                        dbString.erase(0, pos + delimiter.length());
                        int port = std::stoi(dbString);
                        std::cout << port << std::endl;

                        MysqlConnector mysqlobj;
                        connection = mysqlobj.getConnection(hostname, username, password, dbname, port);
                        pNode->SetValue((PMCHAR)pStrArg->GetValue().c_str());
                    }
                }
                break;
            }
            case COMMAND_TYPE_SET_VALUE:
            {
                MemoryManager::Inst.CreateObject(&pNullRes);
                if(ENTITY_TYPE_STRING == pArg->ul_Type)
                {
                    String* pStrArg = (String*)pArg;
                    if(0 != pStrArg)
                    {
                        pNode->SetValue((PMCHAR)pStrArg->GetValue().c_str());
                    }
                }
                break;
            }
            case COMMAND_TYPE_SET_LVALUE:
            {
                MemoryManager::Inst.CreateObject(&pNullRes);
                if(ENTITY_TYPE_STRING == pArg->ul_Type)
                {
                    String* pStrArg = (String*)pArg;
                    if(0 != pStrArg)
                    {
                        pNode->SetLValue((PMCHAR)pStrArg->GetValue().c_str());
                    }
                }
                break;
            }
            case COMMAND_TYPE_SET_RVALUE:
            {
                MemoryManager::Inst.CreateObject(&pNullRes);
                if(ENTITY_TYPE_STRING == pArg->ul_Type)
                {
                    String* pStrArg = (String*)pArg;
                    if(0 != pStrArg)
                    {
                        pNode->SetRValue((PMCHAR)pStrArg->GetValue().c_str());
                    }
                }
                break;
            }
            case COMMAND_TYPE_SET_TYPE:
            {
                MemoryManager::Inst.CreateObject(&pNullRes);
                if(ENTITY_TYPE_INT == pArg->ul_Type)
                {
                    Int* pIntArg = (Int*)pArg;
                    if(0 != pIntArg)
                    {
                        pNode->SetType((MUSHORT)pIntArg->GetValue());
                    }
                }
                break;
            }
            case COMMAND_TYPE_SET_NATURE:
            {
                MemoryManager::Inst.CreateObject(&pNullRes);
                if(ENTITY_TYPE_INT == pArg->ul_Type)
                {
                    Int* pIntArg = (Int*)pArg;
                    if(0 != pIntArg)
                    {
                        pNode->SetNature((MBYTE)pIntArg->GetValue());
                    }
                }
                break;
            }
            case COMMAND_TYPE_SET_CUSTOM_STRING:
            {
                MemoryManager::Inst.CreateObject(&pNullRes);
                if(ENTITY_TYPE_STRING == pArg->ul_Type)
                {
                    String* pStrArg = (String*)pArg;
                    if(0 != pStrArg)
                    {
                        pNode->SetCustomString((PMCHAR)pStrArg->GetValue().c_str());
                    }
                }
                break;
            }
            case COMMAND_TYPE_SET_MIN_CHILD_WEIGHT:
            {
                MemoryManager::Inst.CreateObject(&pNullRes);
                if(ENTITY_TYPE_INT == pArg->ul_Type)
                {
                    Int* pIntArg = (Int*)pArg;
                    if(0 != pIntArg)
                    {
                        pNode->SetMinimumChildWeight(pIntArg->GetValue());
                    }
                }
                break;
            }
            case COMMAND_TYPE_SET_MAX_CHILD_WEIGHT:
            {
                MemoryManager::Inst.CreateObject(&pNullRes);
                if(ENTITY_TYPE_INT == pArg->ul_Type)
                {
                    Int* pIntArg = (Int*)pArg;
                    if(0 != pIntArg)
                    {
                        pNode->SetMaximumChildWeight(pIntArg->GetValue());
                    }
                }
                break;
            }
            case COMMAND_TYPE_SET_WEIGHT:
            {
                MemoryManager::Inst.CreateObject(&pNullRes);
                if(ENTITY_TYPE_INT == pArg->ul_Type)
                {
                    Int* pIntArg = (Int*)pArg;
                    if(0 != pIntArg)
                    {
                        pNode->SetWeight(pIntArg->GetValue());
                    }
                }
                break;
            }
            case COMMAND_TYPE_EXPAND:
            {
                MemoryManager::Inst.CreateObject(&pNullRes);
                if(ENTITY_TYPE_LIST == pArg->ul_Type)
                {
                    PENTITYLIST pStrListArg = (PENTITYLIST)pArg;
                    if(0 != pStrListArg)
                    {
                        LST_STR lstTokens;
                        EntityList::const_iterator ite1 = pStrListArg->begin();
                        EntityList::const_iterator iteEnd1 = pStrListArg->end();
                        for( ; ite1 != iteEnd1; ++ite1)
                        {
                            lstTokens.push_back(((PString)(*ite1))->GetValue());
                        }
                        pNode->Expand(lstTokens);
                    }
                }
                break;
            }
            case COMMAND_TYPE_ADD_NODE:
            {
                MemoryManager::Inst.CreateObject(&pNullRes);
                if(0 == pArg)
                {
                    pNodeRes = pNode->AddNode();
                }
                else if(ENTITY_TYPE_NODE == pArg->ul_Type)
                {
                    pNodeRes = pNode->AddNode((PNODE)pArg, true);
                }
                break;
            }
            case COMMAND_TYPE_ADD_NODE_WITH_WEIGHT:
            {
                MemoryManager::Inst.CreateObject(&pNullRes);
                if(ENTITY_TYPE_INT == pArg->ul_Type)
                {
                    Int* pIntArg = (Int*)pArg;
                    if(0 != pIntArg)
                    {
                        pNodeRes = pNode->AddNodeWithWeight(pIntArg->GetValue());
                    }
                }
                break;
            }
            case COMMAND_TYPE_READ_FROM_FILE:
            {
                MemoryManager::Inst.CreateObject(&pNullRes);
                if(ENTITY_TYPE_STRING == pArg->ul_Type)
                {
                    PString pStrArg = (PString)pArg;
                    if(0 != pStrArg)
                    {
                        pNode->ReadValueFromFile(pStrArg->GetValue().c_str());
                    }
                }
                break;
            }
            case COMMAND_TYPE_GET_AGGREGATED_VALUE:
            {
                MemoryManager::Inst.CreateObject(&pStrRes);
                pStrRes->SetValue(pNode->GetAggregatedValue());
                break;
            }
            case COMMAND_TYPE_GET_ENTITY_OBJECT:
            {
                pEntityRes = (pNode->GetEntityObj());
                break;
            }
            case COMMAND_TYPE_SET_ENTITY_OBJECT:
            {
                if(ENTITY_TYPE == pArg->ul_Type)
                {
                    Entity* pEntityArg = (Entity*)pArg;
                    if(0 != pEntityArg)
                    {
                        pNode->SetEntityObj(pEntityArg);
                    }
                }
                break;
            }
            case COMMAND_TYPE_GET_SUBTREE:
            {
                MemoryManager::Inst.CreateObject(&pNodeListRes);
                AddSubtreeToNodeList(pNodeListRes, pNode);
                break;
            }
            case COMMAND_TYPE_IS_TYPE:
            {
                MemoryManager::Inst.CreateObject(&pBoolRes);
                if(ENTITY_TYPE_INT == pArg->ul_Type)
                {
                    PInt pIntArg = (PInt)pArg;
                    if(0 != pIntArg)
                    {
                        pBoolRes->SetValue(pNode->GetType() == pIntArg->GetValue());
                    }
                }
                break;
            }
            case COMMAND_TYPE_IS_VALUE:
            {
                MemoryManager::Inst.CreateObject(&pBoolRes);
                if(ENTITY_TYPE_STRING == pArg->ul_Type)
                {
                    PString pStrArg = (PString)pArg;
                    if(0 != pStrArg)
                    {
                        pBoolRes->SetValue(MSTRING(pNode->GetValue()) == pStrArg->GetValue());
                    }
                }
                break;
            }
            case COMMAND_TYPE_GET_CHILD_OF_TYPE:
            {
                if(ENTITY_TYPE_INT == pArg->ul_Type)
                {
                    PInt pIntArg = (PInt)pArg;
                    PNODE pChild = pNode->GetFirstChild();
                    while(0 != pChild)
                    {
                        if(pChild->GetType() == pIntArg->GetValue())
                        {
                            pNodeRes = pChild;
                            break;
                        }
                        pChild = pChild->GetRightSibling();
                    }
                }
                break;
            }
//                pNodeRes = pNode->GetRightSibling();
//                break;
            case COMMAND_TYPE_LAST_CHILD:
            {
                pNodeRes = pNode->GetLastChild();
                break;
            }
            case COMMAND_TYPE_IS_HAVING_CUSTOM_STRING:
            {
                if(ENTITY_TYPE_STRING == pArg->ul_Type)
                {
                    PString pStringArg = (PString)pArg;
                    pNodeRes = pNode->IsHavingCustomString(pStringArg->GetValue());
                }
                break;
            }
            case COMMAND_TYPE_GET_CHILD_NODE_BY_CUSTOM_STRING:
            {
                if(ENTITY_TYPE_STRING == pArg->ul_Type)
                {
                    PString pStringArg = (PString)pArg;
                    pNodeRes = pNode->GetChildNodeByCustomString(pStringArg->GetValue());
                }
                break;
            }
            case COMMAND_TYPE_SET_ID:
            {
                MemoryManager::Inst.CreateObject(&pNullRes);
                if(ENTITY_TYPE_INT == pArg->ul_Type)
                {
                    PInt pIntArg = (PInt)pArg;
                    if(0 != pIntArg)
                    {
                        pNode->SetID(pIntArg->GetValue());
                    }
                }
                break;
            }
            case COMMAND_TYPE_CHECK_NOT_NULL:
            {
                MemoryManager::Inst.CreateObject(&pBoolRes);
                pBoolRes->SetValue(!pNode->IsNull());
                break;
            }
            case COMMAND_TYPE_GET_STRING:
            {
                MemoryManager::Inst.CreateObject(&pStrRes);
                pStrRes->SetValue("");
                break;
            }
            case COMMAND_TYPE_GET_INTEGER:
            {
                MemoryManager::Inst.CreateObject(&pIntRes);
                pIntRes->SetValue(0);
                break;
            }
            case COMMAND_TYPE_GET_BOOLEAN:
            {
                MemoryManager::Inst.CreateObject(&pBoolRes);
                pBoolRes->SetValue(true);
                break;
            }
            case COMMAND_TYPE_GET_COMMA:
            {
                MemoryManager::Inst.CreateObject(&pStrRes);
                PString pStr = (PString) ",";
                pStrRes->SetValue(",");
                break;
            }
            case COMMAND_TYPE_NEXT_SIBLING:
            {
                pNodeRes = pNode->GetRightSibling();
                if (pNodeRes == 0)
                {
                    MemoryManager::Inst.CreateObject(&pNullRes);
                    pNodeRes == 0;
                }
                break;
            }
            case COMMAND_TYPE_GET_CUSTOM_OBJ:
            {
                pNodeRes = (PNODE)pNode->GetCustomObj();
                break;
            }

        }
    }
    if(0 != pNodeRes)
    {
        return pNodeRes;
    }
    if(0 != pEntityRes)
    {
        return pEntityRes;
    }
    if(0 != pNodeListRes)
    {
        return pNodeListRes;
    }
    if(0 != pStrRes)
    {
        return pStrRes;
    }
    if(0 != pIntRes)
    {
        return pIntRes;
    }
    if(0 != pNullRes)
    {
        return pNullRes;
    }
    if(0 != pBoolRes)
    {
        return pBoolRes;
    }
    return 0;
}

PENTITY Command::ExecuteListCommand(MULONG ulCommand, PENTITY pEntity, ExecutionContext* pContext, PENTITY pArg)
{
    PENTITYLIST pEntityList = (PENTITYLIST)pEntity;
    if(0 == pEntityList)
    {
        return 0;
    }

    PInt pIntRes = 0;
    PENTITYLIST pListRes = 0;
    PNull pNullRes = 0;
    PENTITY pEntityRes = 0;
    PNODE pNodeRes = 0;
    PString pStrRes = 0;

    if(COMMAND_TYPE_GET_ITEM_COUNT == ulCommand)
    {
        MemoryManager::Inst.CreateObject(&pIntRes);
        pIntRes->SetValue(pEntityList->size());
    }
    else if(COMMAND_TYPE_GET_INNER_ITEM_COUNT == ulCommand)
    {
        pListRes = pEntityList->GetInnerCount();
        //        MemoryManager::Inst.CreateObject(&pListRes);
        //        pEntityList->SeekToBegin();
        //        PENTITY ent = pEntityList->GetCurrElem();
        //        while (ent) {
        //            PInt count = 0;
        //            MemoryManager::Inst.CreateObject(&count);
        //            count->SetValue(((PENTITYLIST)ent)->size());
        //            pListRes->push_back(count);
        //
        //            pEntityList->Seek(1, false);
        //            ent = pEntityList->GetCurrElem();
        //        }
    }
    else if(COMMAND_TYPE_LIST_FILTER == ulCommand)
    {
        MemoryManager::Inst.CreateObject(&pListRes);
        EntityList::const_iterator ite1 = pEntityList->begin();
        EntityList::const_iterator iteEnd1 = pEntityList->end();
        for( ; ite1 != iteEnd1; ++ite1)
        {
            pContext->map_Var[pContext->p_MD->s_ListItemVar] = *ite1;
            if(0 != p_Arg)
            {
                PBool pRes = (PBool)p_Arg->Execute(pContext);
                if (pRes->GetValue()) {
                    pListRes->push_back(*ite1);
                }
            }
        }
    }
    else if(COMMAND_TYPE_LIST_GROUPBY == ulCommand)
    {
        MemoryManager::Inst.CreateObject(&pListRes);
        std::map<MSTRING, PENTITYLIST> groupedLists;
        EntityList::const_iterator ite1 = pEntityList->begin();
        EntityList::const_iterator iteEnd1 = pEntityList->end();
        for( ; ite1 != iteEnd1; ++ite1)
        {
            pContext->map_Var[pContext->p_MD->s_ListItemVar] = *ite1;
            if(0 != p_Arg)
            {
                PENTITY pRes = p_Arg->Execute(pContext);
                MSTRING key = pRes->ToString();
                std::map<MSTRING, PENTITYLIST>::iterator ite = groupedLists.find(key);
                if (ite == groupedLists.end()) {
                    PENTITYLIST newlist = 0;
                    MemoryManager::Inst.CreateObject(&newlist);
                    newlist->push_back(*ite1);
                    groupedLists[key] = newlist;
                } else {
                    (*ite).second->push_back(*ite1);
                }
            }
        }

        std::map<MSTRING, PENTITYLIST>::iterator ite2 = groupedLists.begin();
        std::map<MSTRING, PENTITYLIST>::iterator end2 = groupedLists.end();
        for ( ; ite2 != end2; ++ite2) {
            pListRes->push_back(ite2->second);
        }
    }
    else if(COMMAND_TYPE_GET_UNIQUE_NODE_LIST_WITH_COUNT == ulCommand)
    {
        // ONLY FOR NODE LIST
        MemoryManager::Inst.CreateObject(&pListRes);
        pEntityList->SeekToBegin();
        PNODE currNode = (PNODE)pEntityList->GetCurrElem();
        std::map<std::string, int> uniqueMap;
        String* pStrArg = (String*)pArg;
        while(currNode != 0) {
            std::string str;
            if (pStrArg != 0 && std::strcmp((char *)pStrArg, "LValue") && currNode->GetLVal() != 0) {
                str.assign(currNode->GetLVal());
            } else if (pStrArg != 0 && std::strcmp((char *)pStrArg, "RValue") && currNode->GetRVal() != 0) {
                str.assign(currNode->GetRVal());
            } else {
                str.assign(currNode->GetValue());
            }
            if (uniqueMap[str] == 0)
            {
                uniqueMap[str] = 1;
            }
            else
            {
                uniqueMap[str] = uniqueMap[str] + 1;
            }
            pEntityList->Seek(1, false);
            currNode = (PNODE)pEntityList->GetCurrElem();
        }

        for (auto const& x : uniqueMap)
        {
            PNODE item = MemoryManager::Inst.CreateNode(999);
            item->SetValue((char *)x.first.c_str());
            item->SetLValue((char *)std::to_string(x.second).c_str());
            pListRes->push_back(item);
        }
    }
    else if(COMMAND_TYPE_GET_UNIQUE_NODE_LIST_WITH_NODE_REF == ulCommand)
    {
        // ONLY FOR NODE LIST
        MemoryManager::Inst.CreateObject(&pListRes);
        pEntityList->SeekToBegin();
        PNODE currNode = (PNODE)pEntityList->GetCurrElem();
        std::map<std::string, PNODE> uniqueMap;
        String* pStrArg = (String*)pArg;
        while(currNode != 0) {
            std::string str;
            if (pStrArg != 0 && std::strcmp((char *)pStrArg, "LValue") && currNode->GetLVal() != 0) {
                str.assign(currNode->GetLVal());
            } else if (pStrArg != 0 && std::strcmp((char *)pStrArg, "RValue") && currNode->GetRVal() != 0) {
                str.assign(currNode->GetRVal());
            } else {
                str.assign(currNode->GetValue());
            }
            if (uniqueMap[str] == 0)
            {
                uniqueMap[str] = currNode;
            }
//            else
//            {
//                uniqueMap[str] = uniqueMap[str] + 1;
//            }
            pEntityList->Seek(1, false);
            currNode = (PNODE)pEntityList->GetCurrElem();
        }

        for (auto const& x : uniqueMap)
        {
            PNODE item = MemoryManager::Inst.CreateNode(999);
            item->SetValue((char *)x.first.c_str());
            item->SetCustomObj((x.second));
            pListRes->push_back(item);
        }
    }
    else if(COMMAND_TYPE_SORT_NODE_LIST == ulCommand)
    {
        // ONLY FOR NODE LIST
        MemoryManager::Inst.CreateObject(&pListRes);
        String* pStrArg = (String*)pArg;
        pEntityList->SeekToBegin();
        PNODE currNode = (PNODE)pEntityList->GetCurrElem();
        std::map<std::string, int> uniqueMap;
        while(currNode != 0)
        {
            std::string str;
            str.assign(currNode->GetValue());
            uniqueMap[str] =  std::stoi(currNode->GetLVal());
            pEntityList->Seek(1, false);
            currNode = (PNODE)pEntityList->GetCurrElem();
        }

//        for (auto const& x : uniqueMap)
//        {
//            std::cout << x.first  // string (key)
//                      << ':'
//                      << x.second // string's value
//                      << std::endl ;
//        }

        // Declaring the type of Predicate that accepts 2 pairs and return a bool
        typedef std::function<bool(std::pair<std::string, int>, std::pair<std::string, int>)> Comparator;

        // Defining a lambda function to compare two pairs. It will compare two pairs using second field
        Comparator compFunctor;
        if (pStrArg != 0)
        {
            int pInt = atoi(pStrArg->GetValue().c_str());
            if (pInt >= 0)
            {
                compFunctor = [](std::pair<std::string, int> elem1 ,std::pair<std::string, int> elem2)
                {
                    return elem1.second >= elem2.second;
                };
            }
            else if (pInt <= 0)
            {
                compFunctor = [](std::pair<std::string, int> elem1 ,std::pair<std::string, int> elem2)
                {
                    return elem1.second <= elem2.second;
                };
            }
        } else {
            compFunctor = [](std::pair<std::string, int> elem1 ,std::pair<std::string, int> elem2)
            {
                return elem1.second >= elem2.second;
            };
        }
        // Declaring a set that will store the pairs using above comparision logic
        std::set<std::pair<std::string, int>, Comparator> setOfSorted(uniqueMap.begin(), uniqueMap.end(), compFunctor);
        for (auto const& x : setOfSorted)
        {
            PNODE item = MemoryManager::Inst.CreateNode(999);
            item->SetValue((char *)x.first.c_str());
            item->SetLValue((char *)std::to_string(x.second).c_str());
            pListRes->push_back(item);
        }
    }
    else if(COMMAND_TYPE_EXTRACT_NODE_LIST_TOP == ulCommand)
    {
        // ONLY FOR NODE LIST
        MemoryManager::Inst.CreateObject(&pListRes);
        String* pStrArg = (String*)pArg;
        int pInt = atoi(pStrArg->GetValue().c_str());
        pEntityList->SeekToBegin();
        PNODE currNode = (PNODE)pEntityList->GetCurrElem();
        int entitySize = pEntityList->size();
        for(int i = 0; i < (entitySize < pInt ? entitySize : pInt); i++)
        {
            pListRes->push_back(currNode->GetCopy());
            pEntityList->Seek(1, false);
            currNode = (PNODE)pEntityList->GetCurrElem();
        }
    }
    else if(COMMAND_TYPE_LIST_GROUP_SEQUENCE_BY == ulCommand)
    {
        bool firstKeyDetected = false;
        MSTRING currentkey;
        PENTITYLIST currentlist = 0;
        EntityList::const_iterator ite1 = pEntityList->begin();
        EntityList::const_iterator iteEnd1 = pEntityList->end();
        for( ; ite1 != iteEnd1; ++ite1)
        {
            pContext->map_Var[pContext->p_MD->s_ListItemVar] = *ite1;
            if(0 != p_Arg)
            {
                PENTITY pRes = p_Arg->Execute(pContext);
                MSTRING key = pRes->ToString();
                if (firstKeyDetected && (key == currentkey)) {
                    currentlist->push_back(*ite1);
                }
                else {
                    PENTITYLIST newlist = 0;
                    MemoryManager::Inst.CreateObject(&newlist);
                    newlist->push_back(*ite1);
                    currentlist = newlist;
                    pListRes->push_back(newlist);
                }
                currentkey = key;
                firstKeyDetected = true;
            }
        }
    }
    else if(COMMAND_TYPE_ADD_NODE_TO_LIST == ulCommand)
    {
        if(ENTITY_TYPE_STRING == pArg->ul_Type)
        {
            String* pStrArg = (String*)pArg;
            Node *newListNode = MemoryManager::Inst.CreateNode(1);
            if(0 != pStrArg)
            {
                newListNode->SetValue((PMCHAR)pStrArg->GetValue().c_str());
            }
            pEntityList->push_back(newListNode);
        }
        MemoryManager::Inst.CreateObject(&pNullRes);
    }
        /*else if(COMMAND_TYPE_ADD_NODE_TO_LI   ST == ulCommand)
        {
            if(ENTITY_TYPE_NODE == pArg->ul_Type)
            {
                pEntityList->push_back((PNODE)pArg);
            }
            MemoryManager::Inst.CreateObject(&pNullRes);
        }*/
    else if(COMMAND_TYPE_SEEK == ulCommand)
    {
        PInt pInt = (PInt)p_Arg->GetEntity();
        if(0 != pInt)
        {
            pEntityList->Seek(pInt->GetValue(), pInt->b_IsNegative);
        }
        MemoryManager::Inst.CreateObject(&pNullRes);
    }
    else if(COMMAND_TYPE_SEEK_TO_BEGIN == ulCommand)
    {
        pEntityList->SeekToBegin();
        MemoryManager::Inst.CreateObject(&pNullRes);
    }
    else if(COMMAND_TYPE_SEEK_TO_END == ulCommand)
    {
        pEntityList->SeekToEnd();
        MemoryManager::Inst.CreateObject(&pNullRes);
    }
    else if(COMMAND_TYPE_GET_CURR_ELEM == ulCommand)
    {
        PENTITY pEntity = pEntityList->GetCurrElem();
        if(0 != pEntity)
        {
            // If the entity is not a node then it will be deleted immediately after use.
            // Therefore we need to get a copy.
            if(ENTITY_TYPE_NODE != pEntity->ul_Type)
            {
                pEntityRes = pEntity->GetCopy();
            }
            else
            {
                pEntityRes = pEntity;
            }
        }
        else
        {
            MemoryManager::Inst.CreateObject(&pNullRes);
        }
    }
    else if (COMMAND_TYPE_GET_NEXT_ELEM == ulCommand)
    {
        pEntityList->Seek(1, false);
        PENTITY pEntity = pEntityList->GetCurrElem();
        if(0 != pEntity)
        {
            // If the entity is not a node then it will be deleted immediately after use.
            // Therefore we need to get a copy.
            if(ENTITY_TYPE_NODE != pEntity->ul_Type)
            {
                pEntityRes = pEntity->GetCopy();
            }
            else
            {
                pEntityRes = pEntity;
            }
        }
        else
        {
            MemoryManager::Inst.CreateObject(&pNullRes);
        }
    }
    else if (COMMAND_TYPE_GET_OLDEST_DATE == ulCommand)
    {
        MemoryManager::Inst.CreateObject(&pStrRes);
        pEntityList->SeekToBegin();
        PNODE currNode = (PNODE)pEntityList->GetCurrElem();
        String* pStrArg = (String*)pArg;
        std::vector<std::string> dateList;
        while(currNode != 0) {
            if (currNode->GetLVal() != 0)
            {
                dateList.push_back(std::string(currNode->GetLVal()));
            }
            pEntityList->Seek(1, false);
            currNode = (PNODE)pEntityList->GetCurrElem();
        }
        std::string oldestDate = (DateTimeOperations::GetOldestDate(dateList));
        pStrRes->SetValue(DateTimeOperations::GetOldestDate(dateList));
    }
    else if (COMMAND_TYPE_GET_LATEST_DATE == ulCommand)
    {
        MemoryManager::Inst.CreateObject(&pStrRes);
        pEntityList->SeekToBegin();
        PNODE currNode = (PNODE)pEntityList->GetCurrElem();
        String* pStrArg = (String*)pArg;
        std::vector<std::string> dateList;
        while(currNode != 0) {
            if (currNode->GetLVal() != 0)
            {
                dateList.push_back(currNode->GetLVal());
            }
            pEntityList->Seek(1, false);
            currNode = (PNODE)pEntityList->GetCurrElem();
        }
        std::string latestDate = (DateTimeOperations::GetLatestDate(dateList));
        pStrRes->SetValue(DateTimeOperations::GetLatestDate(dateList));
    }
        // first handle the commands that would need to access the execution context
    else if (COMMAND_TYPE_FILTER_SUBTREE == ulCommand)
    {
        MemoryManager::Inst.CreateObject(&pListRes);
        pEntityList->SeekToBegin();
        PNODE currNode = (PNODE)pEntityList->GetCurrElem();
        while(currNode != 0)
        {
            PENTITYLIST pNodeList = 0;
            MemoryManager::Inst.CreateObject(&pNodeList);
            FilterSubTree(currNode, p_Arg, pContext, pNodeList);
            pListRes->SeekToBegin();
            PNODE internalNode = (PNODE)pNodeList->GetCurrElem();
            while(internalNode != 0)
            {
                pListRes->push_back(internalNode->GetCopy());
                pNodeList->Seek(1, false);
                internalNode = (PNODE)pNodeList->GetCurrElem();
            }
            pEntityList->Seek(1, false);
            currNode = (PNODE)pEntityList->GetCurrElem();
        }
    }
    else if(COMMAND_TYPE_MASK_VALUE == ulCommand)
    {
        std::cout<<"------------------------------------MASK TEST SUITE-----------------------------------------\n";
        pEntityList->SeekToBegin();
        PNODE currNode = (PNODE)pEntityList->GetCurrElem();
        String* pStrArg = (String*)pArg;
        MSTRING argument=pStrArg->GetValue();
        MSTRING replacement;
        while(currNode != 0)
        {
            MSTRING nodeString;
            std::cout<<currNode->GetValue()<<"\n";
            std::cout<<argument<<"\n";

            nodeString=currNode->GetValue();

            replacement ="dummyTestName";

            static std::set<std::string> setbeforemask;
            std::set<std::string>::iterator it;
            int temp =counttest;
            setbeforemask.insert (argument);
            counttest=setbeforemask.size();
            MSTRING tempStr=std::to_string(counttest);
            MSTRING tempReplace;
            std::cout << "setbeforemask contains : ";
            for (it=setbeforemask.begin(); it!=setbeforemask.end(); ++it)
            {
                std::cout << *it<<" , ";
            }
            std::cout << '\n';
            std::size_t pos=nodeString.find(argument);
            nodeString.replace(pos,argument.length(),replacement);
            std::cout<<argument<<"\n";
            std::cout<<nodeString<<"\n";


            currNode->SetValue((PMCHAR) nodeString.c_str());

            pEntityList->Seek(1, false);
            PENTITY pEntity = pEntityList->GetCurrElem();
            if(0 != pEntity)
            {
                // If the entity is not a node then it will be deleted immediately after use.
                // Therefore we need to get a copy.
                if(ENTITY_TYPE_NODE != pEntity->ul_Type)
                {
                    pEntityRes = pEntity->GetCopy();
                }
                else
                {
                    pEntityRes = pEntity;
                }
            }
            else
            {
                MemoryManager::Inst.CreateObject(&pNullRes);
            }
            currNode = (PNODE)pEntityList->GetCurrElem();
        }
    }
    else if(COMMAND_TYPE_MASK_FIRST_NAME == ulCommand)
    {
        std::cout<<"------------------------------------MASK FIRST NAME-----------------------------------------\n";
        pEntityList->SeekToBegin();
        PNODE currNode = (PNODE)pEntityList->GetCurrElem();
        String* pStrArg = (String*)pArg;
        MSTRING argument=pStrArg->GetValue();
        MSTRING replacement;
        while(currNode != 0)
        {
            MSTRING nodeString;
            std::cout<<currNode->GetValue()<<"\n";
            std::cout<<argument<<"\n";

            nodeString=currNode->GetValue();

            static std::set<std::string> setbeforemaskfname;
            std::set<std::string>::iterator it;
            int temp =countfname;
            setbeforemaskfname.insert (argument);
            countfname=setbeforemaskfname.size();
            MSTRING tempStr=std::to_string(countfname);
            MSTRING tempReplace;
            std::cout << "setbeforemaskfname contains : ";
            for (it=setbeforemaskfname.begin(); it!=setbeforemaskfname.end(); ++it)
            {
                std::cout << *it<<" , ";
            }
            std::cout << '\n';
            srand(time(NULL)*countfname); //generates random seed val
            if(connection)
            {
                MysqlConnector mysqlobj;
                int resultmale = mysqlobj.existsFirstNameMale(connection,argument);
                int resultfemale = mysqlobj.existsFirstNameFemale(connection,argument);

                if(resultmale==1)
                {
                    int randid = rand()%((7732 - 1) + 1) + 1;
                    replacement = mysqlobj.selectFirstNameMale(connection,randid);
                }
                else if(resultfemale == 1)
                {
                    int randid = rand()%((7144 - 1) + 1) + 1;
                    replacement = mysqlobj.selectFirstNameFemale(connection,randid);
                }
                else
                {
                    int randid = rand()%((744 - 1) + 1) + 1;
                    replacement = mysqlobj.selectFirstNameUnisex(connection,randid);
                }
            }
            else
            {
                perror ("The Database could not be connected, Please check the db connection!");
                throw _exception();
            }
            std::size_t pos=nodeString.find(argument);
            nodeString.replace(pos,argument.length(),replacement);
            std::cout<<argument<<"\n";
            std::cout<<nodeString<<"\n";
            currNode->SetValue((PMCHAR) nodeString.c_str());

            pEntityList->Seek(1, false);
            PENTITY pEntity = pEntityList->GetCurrElem();
            if(0 != pEntity)
            {
                // If the entity is not a node then it will be deleted immediately after use.
                // Therefore we need to get a copy.
                if(ENTITY_TYPE_NODE != pEntity->ul_Type)
                {
                    pEntityRes = pEntity->GetCopy();
                }
                else
                {
                    pEntityRes = pEntity;
                }
            }
            else
            {
                MemoryManager::Inst.CreateObject(&pNullRes);
            }
            currNode = (PNODE)pEntityList->GetCurrElem();
        }
    }
    else if(COMMAND_TYPE_MASK_LAST_NAME == ulCommand)
    {
        std::cout<<"------------------------------------MASK LAST NAME-----------------------------------------\n";
        pEntityList->SeekToBegin();
        PNODE currNode = (PNODE)pEntityList->GetCurrElem();
        String* pStrArg = (String*)pArg;
        MSTRING argument=pStrArg->GetValue();
        MSTRING replacement;
        while(currNode != 0)
        {
            MSTRING nodeString;
            std::cout<<currNode->GetValue()<<"\n";
            std::cout<<argument<<"\n";

            nodeString=currNode->GetValue();

            static std::set<std::string> setbeforemasklname;
            std::set<std::string>::iterator it;
            int temp =countlname;
            setbeforemasklname.insert (argument);
            countlname=setbeforemasklname.size();
            MSTRING tempStr=std::to_string(countlname);
            MSTRING tempReplace;

            std::cout << "setbeforemasklname contains : ";
            for (it=setbeforemasklname.begin(); it!=setbeforemasklname.end(); ++it)
            {
                std::cout << *it<<" , ";
            }
            std::cout << '\n';
            srand(time(NULL)*countlname); //generates random seed val
            if(connection)
            {
                MysqlConnector mysqlobj;
                int randid = rand()%((789 - 1) + 1) + 1;
                replacement = mysqlobj.selectLastName(connection,randid);
            }
            else
            {
                perror ("The Database could not be connected, Please check the db connection!");
                throw _exception();
            }
            std::size_t pos=nodeString.find(argument);
            nodeString.replace(pos,argument.length(),replacement);
            std::cout<<argument<<"\n";
            std::cout<<nodeString<<"\n";


            currNode->SetValue((PMCHAR) nodeString.c_str());

            pEntityList->Seek(1, false);
            PENTITY pEntity = pEntityList->GetCurrElem();
            if(0 != pEntity)
            {
                // If the entity is not a node then it will be deleted immediately after use.
                // Therefore we need to get a copy.
                if(ENTITY_TYPE_NODE != pEntity->ul_Type)
                {
                    pEntityRes = pEntity->GetCopy();
                }
                else
                {
                    pEntityRes = pEntity;
                }
            }
            else
            {
                MemoryManager::Inst.CreateObject(&pNullRes);
            }
            currNode = (PNODE)pEntityList->GetCurrElem();
        }
    }
    else if(COMMAND_TYPE_MASK_FULL_NAME == ulCommand)
    {
        std::cout<<"------------------------------------MASK FULL NAME-----------------------------------------\n";
        pEntityList->SeekToBegin();
        PNODE currNode = (PNODE)pEntityList->GetCurrElem();
        String* pStrArg = (String*)pArg;
        MSTRING argument=pStrArg->GetValue();
        MSTRING replacement;
        while(currNode != 0)
        {
            MSTRING nodeString;
            std::cout<<currNode->GetValue()<<"\n";
            std::cout<<argument<<"\n";
            nodeString=currNode->GetValue();

            static std::set<std::string> setbeforemaskfullname;
            std::set<std::string>::iterator it;
            int temp =countfullname;
            setbeforemaskfullname.insert (argument);
            countfullname=setbeforemaskfullname.size();
            MSTRING tempStr=std::to_string(countfullname);
            MSTRING tempReplace;

            std::cout << "setbeforemaskfullname contains : ";
            for (it=setbeforemaskfullname.begin(); it!=setbeforemaskfullname.end(); ++it)
            {
                std::cout << *it<<" , ";
            }
            std::cout << '\n';
            srand(time(NULL)*countfullname); //generates random seed val
            if(connection)
            {
                MysqlConnector mysqlobj;
                MSTRING fname;
                int randlnameid = rand()%((789 - 1) + 1) + 1;
                int resultmale = mysqlobj.existsFirstNameMale(connection,argument);
                int resultfemale = mysqlobj.existsFirstNameFemale(connection,argument);

                if(resultmale==1)
                {
                    int randid = rand()%((7732 - 1) + 1) + 1;
                    fname = mysqlobj.selectFirstNameMale(connection,randid);
                }
                else if(resultfemale == 1)
                {
                    int randid = rand()%((7144 - 1) + 1) + 1;
                    fname = mysqlobj.selectFirstNameFemale(connection,randid);
                }
                else
                {
                    int randid = rand()%((744 - 1) + 1) + 1;
                    fname = mysqlobj.selectFirstNameUnisex(connection,randid);
                }
                MSTRING lname = mysqlobj.selectLastName(connection,randlnameid);
                replacement = fname + " " + lname;
            }
            else
            {
                perror ("The Database could not be connected, Please check the db connection!");
                throw _exception();
            }
            std::size_t pos=nodeString.find(argument);
            nodeString.replace(pos,argument.length(),replacement);
            std::cout<<argument<<"\n";
            std::cout<<nodeString<<"\n";


            currNode->SetValue((PMCHAR) nodeString.c_str());

            pEntityList->Seek(1, false);
            PENTITY pEntity = pEntityList->GetCurrElem();
            if(0 != pEntity)
            {
                // If the entity is not a node then it will be deleted immediately after use.
                // Therefore we need to get a copy.
                if(ENTITY_TYPE_NODE != pEntity->ul_Type)
                {
                    pEntityRes = pEntity->GetCopy();
                }
                else
                {
                    pEntityRes = pEntity;
                }
            }
            else
            {
                MemoryManager::Inst.CreateObject(&pNullRes);
            }
            currNode = (PNODE)pEntityList->GetCurrElem();
        }
    }
    else if(COMMAND_TYPE_MASK_DATE == ulCommand)
    {
        std::cout<<"------------------------------------MASK DATE-----------------------------------------\n";
        pEntityList->SeekToBegin();
        PNODE currNode = (PNODE)pEntityList->GetCurrElem();
        String* pStrArg = (String*)pArg;
        MSTRING argument=pStrArg->GetValue();
        MSTRING replacement;
        while(currNode != 0)
        {
            MSTRING nodeString;
            std::cout<<currNode->GetValue()<<"\n";
            std::cout<<argument<<"\n";
            nodeString=currNode->GetValue();

            static std::set<std::string> setbeforemaskdate;
            std::set<std::string>::iterator it;
            int temp =countday;
            setbeforemaskdate.insert (argument);
            countday=setbeforemaskdate.size();

            std::cout << "setbeforemaskdate contains : ";
            for (it=setbeforemaskdate.begin(); it!=setbeforemaskdate.end(); ++it)
            {
                std::cout << *it<<" , ";
            }
            std::cout << '\n';
            countday++;
            srand(time(NULL)*countday); //generates random seed val

            int randomDate = rand()%((30 - 1) + 1) + 1;
            MSTRING tempStr=std::to_string(randomDate);
            if(tempStr.length()==1)
            {
                tempStr="0"+tempStr;
            }
            if(argument.at(0)=='-')
            {
                replacement ="-"+tempStr;
            }
            else
            {
                replacement =tempStr;
            }
            std::size_t pos=nodeString.find(argument);
            nodeString.replace(pos,argument.length(),replacement);
            std::cout<<argument<<"\n";
            std::cout<<nodeString<<"\n";


            currNode->SetValue((PMCHAR) nodeString.c_str());

            pEntityList->Seek(1, false);
            PENTITY pEntity = pEntityList->GetCurrElem();
            if(0 != pEntity)
            {
                // If the entity is not a node then it will be deleted immediately after use.
                // Therefore we need to get a copy.
                if(ENTITY_TYPE_NODE != pEntity->ul_Type)
                {
                    pEntityRes = pEntity->GetCopy();
                }
                else
                {
                    pEntityRes = pEntity;
                }
            }
            else
            {
                MemoryManager::Inst.CreateObject(&pNullRes);
            }
            currNode = (PNODE)pEntityList->GetCurrElem();
        }
    }
    else if(COMMAND_TYPE_MASK_MONTH == ulCommand)
    {
        std::cout<<"------------------------------------MASK MONTH-----------------------------------------\n";
        pEntityList->SeekToBegin();
        PNODE currNode = (PNODE)pEntityList->GetCurrElem();
        String* pStrArg = (String*)pArg;
        MSTRING argument=pStrArg->GetValue();
        MSTRING replacement;
        while(currNode != 0)
        {
            MSTRING nodeString;
            std::cout<<currNode->GetValue()<<"\n";
            std::cout<<argument<<"\n";
            nodeString=currNode->GetValue();


            static std::set<std::string> setbeforemaskmonth;
            std::set<std::string>::iterator it;
            int temp =countmonth;
            setbeforemaskmonth.insert (argument);
            countmonth=setbeforemaskmonth.size();

            std::cout << "setbeforemaskmonth contains : ";
            for (it=setbeforemaskmonth.begin(); it!=setbeforemaskmonth.end(); ++it)
            {
                std::cout << *it<<" , ";
            }
            std::cout << '\n';
            countmonth++;
            srand(time(NULL)*countmonth); //generates random seed val

            int randomMonth = rand()%((12 - 1) + 1) + 1;
            MSTRING tempStr=std::to_string(randomMonth);
            if(tempStr.length()==1)
            {
                tempStr="0"+tempStr;
            }
            if(argument.at(0)=='-' && argument.at(argument.length()-1)=='-')
            {
                replacement ="-"+tempStr+"-";
            }
            else
            {
                replacement =tempStr;
            }
            std::size_t pos=nodeString.find(argument);
            nodeString.replace(pos,argument.length(),replacement);
            std::cout<<argument<<"\n";
            std::cout<<nodeString<<"\n";

            currNode->SetValue((PMCHAR) nodeString.c_str());

            pEntityList->Seek(1, false);
            PENTITY pEntity = pEntityList->GetCurrElem();
            if(0 != pEntity)
            {
                // If the entity is not a node then it will be deleted immediately after use.
                // Therefore we need to get a copy.
                if(ENTITY_TYPE_NODE != pEntity->ul_Type)
                {
                    pEntityRes = pEntity->GetCopy();
                }
                else
                {
                    pEntityRes = pEntity;
                }
            }
            else
            {
                MemoryManager::Inst.CreateObject(&pNullRes);
            }
            currNode = (PNODE)pEntityList->GetCurrElem();
        }
    }
    else if(COMMAND_TYPE_MASK_YEAR == ulCommand)
    {
        std::cout<<"------------------------------------MASK YEAR-----------------------------------------\n";
        pEntityList->SeekToBegin();
        PNODE currNode = (PNODE)pEntityList->GetCurrElem();
        String* pStrArg = (String*)pArg;
        MSTRING argument=pStrArg->GetValue();
        MSTRING replacement;
        while(currNode != 0)
        {
            MSTRING nodeString;
            std::cout<<currNode->GetValue()<<"\n";
            std::cout<<argument<<"\n";
            nodeString=currNode->GetValue();

            static std::set<std::string> setbeforemaskyear;
            std::set<std::string>::iterator it;
            int temp =countyear;
            setbeforemaskyear.insert (argument);
            countyear=setbeforemaskyear.size();

            std::cout << "setbeforemaskyear contains : ";
            for (it=setbeforemaskyear.begin(); it!=setbeforemaskyear.end(); ++it)
            {
                std::cout << *it<<" , ";
            }
            std::cout << '\n';
            countyear++;
            srand(time(NULL)*countyear); //generates random seed val

            int arg = std::stoi(argument);
            int diff=5;
            int userBeg =arg-diff;
            int userEnd =arg+diff;
            int randomYear = rand()%((userEnd - userBeg) + 1) + userBeg;
            if(argument.at(argument.length()-1)=='-')
            {
                replacement =std::to_string(randomYear)+"-";
            }
            else
            {
                replacement =std::to_string(randomYear);
            }
            std::size_t pos=nodeString.find(argument);
            nodeString.replace(pos,argument.length(),replacement);
            std::cout<<argument<<"\n";
            std::cout<<nodeString<<"\n";


            currNode->SetValue((PMCHAR) nodeString.c_str());

            pEntityList->Seek(1, false);
            PENTITY pEntity = pEntityList->GetCurrElem();
            if(0 != pEntity)
            {
                // If the entity is not a node then it will be deleted immediately after use.
                // Therefore we need to get a copy.
                if(ENTITY_TYPE_NODE != pEntity->ul_Type)
                {
                    pEntityRes = pEntity->GetCopy();
                }
                else
                {
                    pEntityRes = pEntity;
                }
            }
            else
            {
                MemoryManager::Inst.CreateObject(&pNullRes);
            }
            currNode = (PNODE)pEntityList->GetCurrElem();
        }
    }
    else if(COMMAND_TYPE_MASK_HOUR == ulCommand)
    {
        std::cout<<"------------------------------------MASK HOUR-----------------------------------------\n";
        pEntityList->SeekToBegin();
        PNODE currNode = (PNODE)pEntityList->GetCurrElem();
        String* pStrArg = (String*)pArg;
        MSTRING argument=pStrArg->GetValue();
        MSTRING replacement;
        while(currNode != 0)
        {
            MSTRING nodeString;
            std::cout<<currNode->GetValue()<<"\n";
            std::cout<<argument<<"\n";
            nodeString=currNode->GetValue();


            static std::set<std::string> setbeforemaskhour;
            std::set<std::string>::iterator it;
            int temp =counthour;
            setbeforemaskhour.insert (argument);
            counthour=setbeforemaskhour.size();

            std::cout << "setbeforemaskhour contains : ";
            for (it=setbeforemaskhour.begin(); it!=setbeforemaskhour.end(); ++it)
            {
                std::cout << *it<<" , ";
            }
            std::cout << '\n';
            counthour++;
            srand(time(NULL)*counthour); //generates random seed val

            //Assuming hours in 24-hour format
            int randomHour = rand()%((23) + 1);
            MSTRING tempStr=std::to_string(randomHour);
            if(tempStr.length()==1)
            {
                tempStr="0"+tempStr;
            }
            if(argument.at(argument.length()-1)==':')
            {
                replacement =tempStr+":";
            }
            else
            {
                replacement =tempStr;
            }
            std::size_t pos=nodeString.find(argument);
            nodeString.replace(pos,argument.length(),replacement);
            std::cout<<argument<<"\n";
            std::cout<<nodeString<<"\n";

            currNode->SetValue((PMCHAR) nodeString.c_str());

            pEntityList->Seek(1, false);
            PENTITY pEntity = pEntityList->GetCurrElem();
            if(0 != pEntity)
            {
                // If the entity is not a node then it will be deleted immediately after use.
                // Therefore we need to get a copy.
                if(ENTITY_TYPE_NODE != pEntity->ul_Type)
                {
                    pEntityRes = pEntity->GetCopy();
                }
                else
                {
                    pEntityRes = pEntity;
                }
            }
            else
            {
                MemoryManager::Inst.CreateObject(&pNullRes);
            }
            currNode = (PNODE)pEntityList->GetCurrElem();
        }
    }
    else if(COMMAND_TYPE_MASK_MINUTE == ulCommand)
    {
        std::cout<<"------------------------------------MASK MINUTE-----------------------------------------\n";
        pEntityList->SeekToBegin();
        PNODE currNode = (PNODE)pEntityList->GetCurrElem();
        String* pStrArg = (String*)pArg;
        MSTRING argument=pStrArg->GetValue();
        MSTRING replacement;
        while(currNode != 0)
        {
            MSTRING nodeString;
            std::cout<<currNode->GetValue()<<"\n";
            std::cout<<argument<<"\n";
            nodeString=currNode->GetValue();

            static std::set<std::string> setbeforemaskminute;
            std::set<std::string>::iterator it;
            int temp =countminute;
            setbeforemaskminute.insert (argument);
            countminute=setbeforemaskminute.size();

            std::cout << "setbeforemaskminute contains : ";
            for (it=setbeforemaskminute.begin(); it!=setbeforemaskminute.end(); ++it)
            {
                std::cout << *it<<" , ";
            }
            std::cout << '\n';
            countminute++;
            srand(time(NULL)*countminute); //generates random seed val

            int randomMinute = rand()%((59) + 1);
            MSTRING tempStr=std::to_string(randomMinute);
            if(tempStr.length()==1)
            {
                tempStr="0"+tempStr;
            }
            if(argument.at(0)==':' && argument.at(argument.length()-1)==':')
            {
                replacement =":"+tempStr+":";
            }
            else
            {
                replacement =tempStr;
            }
            std::size_t pos=nodeString.find(argument);
            nodeString.replace(pos,argument.length(),replacement);
            std::cout<<argument<<"\n";
            std::cout<<nodeString<<"\n";


            currNode->SetValue((PMCHAR) nodeString.c_str());

            pEntityList->Seek(1, false);
            PENTITY pEntity = pEntityList->GetCurrElem();
            if(0 != pEntity)
            {
                // If the entity is not a node then it will be deleted immediately after use.
                // Therefore we need to get a copy.
                if(ENTITY_TYPE_NODE != pEntity->ul_Type)
                {
                    pEntityRes = pEntity->GetCopy();
                }
                else
                {
                    pEntityRes = pEntity;
                }
            }
            else
            {
                MemoryManager::Inst.CreateObject(&pNullRes);
            }
            currNode = (PNODE)pEntityList->GetCurrElem();
        }
    }
    else if(COMMAND_TYPE_MASK_SECONDS == ulCommand)
    {
        std::cout<<"------------------------------------MASK SECONDS-----------------------------------------\n";
        pEntityList->SeekToBegin();
        PNODE currNode = (PNODE)pEntityList->GetCurrElem();
        String* pStrArg = (String*)pArg;
        MSTRING argument=pStrArg->GetValue();
        MSTRING replacement;
        while(currNode != 0)
        {
            MSTRING nodeString;
            std::cout<<currNode->GetValue()<<"\n";
            std::cout<<argument<<"\n";
            nodeString=currNode->GetValue();

            static std::set<std::string> setbeforemaskseconds;
            std::set<std::string>::iterator it;
            int temp =countseconds;
            setbeforemaskseconds.insert (argument);
            countseconds=setbeforemaskseconds.size();

            std::cout << "setbeforemaskseconds contains : ";
            for (it=setbeforemaskseconds.begin(); it!=setbeforemaskseconds.end(); ++it)
            {
                std::cout << *it<<" , ";
            }
            std::cout << '\n';
            countseconds++;
            srand(time(NULL)*countseconds); //generates random seed val

            int randomSecond = rand()%((59) + 1);
            MSTRING tempStr=std::to_string(randomSecond);
            if(tempStr.length()==1)
            {
                tempStr="0"+tempStr;
            }
            if(argument.at(0)==':')
            {
                replacement =":"+tempStr;
            }
            else
            {
                replacement =tempStr;
            }
            std::size_t pos=nodeString.find(argument);
            nodeString.replace(pos,argument.length(),replacement);
            std::cout<<argument<<"\n";
            std::cout<<nodeString<<"\n";


            currNode->SetValue((PMCHAR) nodeString.c_str());

            pEntityList->Seek(1, false);
            PENTITY pEntity = pEntityList->GetCurrElem();
            if(0 != pEntity)
            {
                // If the entity is not a node then it will be deleted immediately after use.
                // Therefore we need to get a copy.
                if(ENTITY_TYPE_NODE != pEntity->ul_Type)
                {
                    pEntityRes = pEntity->GetCopy();
                }
                else
                {
                    pEntityRes = pEntity;
                }
            }
            else
            {
                MemoryManager::Inst.CreateObject(&pNullRes);
            }
            currNode = (PNODE)pEntityList->GetCurrElem();
        }
    }
    else if(COMMAND_TYPE_MASK_TELEPHONE_NUMBER == ulCommand)
    {
        std::cout<<"------------------------------------MASK TEL NUM-----------------------------------------\n";
        pEntityList->SeekToBegin();
        PNODE currNode = (PNODE)pEntityList->GetCurrElem();
        String* pStrArg = (String*)pArg;
        MSTRING argument=pStrArg->GetValue();
        MSTRING replacement;
        while(currNode != 0)
        {
            MSTRING nodeString;
            std::cout<<currNode->GetValue()<<"\n";
            std::cout<<argument<<"\n";
            nodeString=currNode->GetValue();

            static std::set<std::string> setbeforemasktelnum;
            std::set<std::string>::iterator it;
            int temp =counttelnum;
            setbeforemasktelnum.insert (argument);
            counttelnum=setbeforemasktelnum.size();

            std::cout << "setbeforemasktelnum contains : ";
            for (it=setbeforemasktelnum.begin(); it!=setbeforemasktelnum.end(); ++it)
            {
                std::cout << *it<<" , ";
            }
            std::cout << '\n';
            counttelnum++;
            srand(time(NULL)*counttelnum); //generates random seed val

            int telLength=0;
            MSTRING telNum;
            int randid = rand()%((13 - 1) + 1) + 1;

            if(connection)
            {
                MysqlConnector mysqlobj;
                replacement = mysqlobj.selectCityCode(connection,randid);
            }
            else
            {
                perror ("The Database could not be connected, Please check the db connection!");
                throw _exception();
            }

            if(replacement.length()==1)
            {
                telLength=7;
            }
            else if(replacement.length()==2)
            {
                telLength=6;
            }
            for(int i=0;i<telLength;i++)
            {
                telNum+=(std::to_string(rand()%((9) + 1)));
            }
            replacement+=telNum;
            std::size_t pos=nodeString.find(argument);
            nodeString.replace(pos,argument.length(),replacement);
            std::cout<<argument<<"\n";
            std::cout<<nodeString<<"\n";


            currNode->SetValue((PMCHAR) nodeString.c_str());

            pEntityList->Seek(1, false);
            PENTITY pEntity = pEntityList->GetCurrElem();
            if(0 != pEntity)
            {
                // If the entity is not a node then it will be deleted immediately after use.
                // Therefore we need to get a copy.
                if(ENTITY_TYPE_NODE != pEntity->ul_Type)
                {
                    pEntityRes = pEntity->GetCopy();
                }
                else
                {
                    pEntityRes = pEntity;
                }
            }
            else
            {
                MemoryManager::Inst.CreateObject(&pNullRes);
            }
            currNode = (PNODE)pEntityList->GetCurrElem();
        }
    }
    else if(COMMAND_TYPE_MASK_ADDRESS == ulCommand)
    {
        std::cout<<"------------------------------------MASK ADDRESS-----------------------------------------\n";
        pEntityList->SeekToBegin();
        PNODE currNode = (PNODE)pEntityList->GetCurrElem();
        String* pStrArg = (String*)pArg;
        MSTRING argument=pStrArg->GetValue();
        MSTRING replacement;
        while(currNode != 0)
        {
            MSTRING nodeString;
            std::cout<<currNode->GetValue()<<"\n";
            std::cout<<argument<<"\n";
            nodeString=currNode->GetValue();

            static std::set<std::string> setbeforemaskaddress;
            std::set<std::string>::iterator it;
            int temp =countaddress;
            setbeforemaskaddress.insert (argument);
            countaddress=setbeforemaskaddress.size();
            MSTRING tempStr=std::to_string(countaddress);
            MSTRING tempReplace;

            std::cout << "setbeforemaskaddress contains : ";
            for (it=setbeforemaskaddress.begin(); it!=setbeforemaskaddress.end(); ++it)
            {
                std::cout << *it<<" , ";
            }
            std::cout << '\n';
            srand(time(NULL)*countaddress); //generates random seed val
            if(connection)
            {
                MysqlConnector mysqlobj;
                int randid = rand()%((499 - 1) + 1) + 1;
                replacement = mysqlobj.selectAddress(connection,randid);
            }
            else
            {
                perror ("The Database could not be connected, Please check the db connection!");
                throw _exception();
            }
            std::size_t pos=nodeString.find(argument);
            nodeString.replace(pos,argument.length(),replacement);
            std::cout<<argument<<"\n";
            std::cout<<nodeString<<"\n";


            currNode->SetValue((PMCHAR) nodeString.c_str());

            pEntityList->Seek(1, false);
            PENTITY pEntity = pEntityList->GetCurrElem();
            if(0 != pEntity)
            {
                // If the entity is not a node then it will be deleted immediately after use.
                // Therefore we need to get a copy.
                if(ENTITY_TYPE_NODE != pEntity->ul_Type)
                {
                    pEntityRes = pEntity->GetCopy();
                }
                else
                {
                    pEntityRes = pEntity;
                }
            }
            else
            {
                MemoryManager::Inst.CreateObject(&pNullRes);
            }
            currNode = (PNODE)pEntityList->GetCurrElem();
        }
    }
    else if(COMMAND_TYPE_MASK_POSTAL_CODE == ulCommand)
    {
        std::cout<<"------------------------------------MASK POSTAL CODE-----------------------------------------\n";
        pEntityList->SeekToBegin();
        PNODE currNode = (PNODE)pEntityList->GetCurrElem();
        String* pStrArg = (String*)pArg;
        MSTRING argument=pStrArg->GetValue();
        MSTRING replacement;
        while(currNode != 0)
        {
            MSTRING nodeString;
            std::cout<<currNode->GetValue()<<"\n";
            std::cout<<argument<<"\n";
            nodeString=currNode->GetValue();

            static std::set<std::string> setbeforemaskpostalcode;
            std::set<std::string>::iterator it;
            int temp =countpostalcode;
            setbeforemaskpostalcode.insert (argument);
            countpostalcode=setbeforemaskpostalcode.size();
            MSTRING tempStr=std::to_string(countpostalcode);
            MSTRING tempReplace;

            std::cout << "setbeforemaskpostalcode contains : ";
            for (it=setbeforemaskpostalcode.begin(); it!=setbeforemaskpostalcode.end(); ++it)
            {
                std::cout << *it<<" , ";
            }
            std::cout << '\n';
            srand(time(NULL)*countpostalcode); //generates random seed val
            if(connection)
            {
                MysqlConnector mysqlobj;
                int randid = rand()%((200 - 1) + 1) + 1;
                replacement = mysqlobj.selectPostalCode(connection,randid);
            }
            else
            {
                perror ("The Database could not be connected, Please check the db connection!");
                throw _exception();
            }
            std::size_t pos=nodeString.find(argument);
            nodeString.replace(pos,argument.length(),replacement);
            std::cout<<argument<<"\n";
            std::cout<<nodeString<<"\n";


            currNode->SetValue((PMCHAR) nodeString.c_str());

            pEntityList->Seek(1, false);
            PENTITY pEntity = pEntityList->GetCurrElem();
            if(0 != pEntity)
            {
                // If the entity is not a node then it will be deleted immediately after use.
                // Therefore we need to get a copy.
                if(ENTITY_TYPE_NODE != pEntity->ul_Type)
                {
                    pEntityRes = pEntity->GetCopy();
                }
                else
                {
                    pEntityRes = pEntity;
                }
            }
            else
            {
                MemoryManager::Inst.CreateObject(&pNullRes);
            }
            currNode = (PNODE)pEntityList->GetCurrElem();
        }
    }
    else if(COMMAND_TYPE_MASK_INTEGER == ulCommand)
    {
        std::cout<<"------------------------------------MASK INTEGER-----------------------------------------\n";
        pEntityList->SeekToBegin();
        PNODE currNode = (PNODE)pEntityList->GetCurrElem();
        String* pStrArg = (String*)pArg;
        MSTRING argument=pStrArg->GetValue();
        MSTRING replacement;

        int userBeg = std::stoi(currNode->GetLVal());
        int userEnd = std::stoi(currNode->GetRVal());

        while(currNode != 0)
        {
            MSTRING nodeString;
            std::cout<<currNode->GetValue()<<"\n";
            std::cout<<argument<<"\n";
            nodeString=currNode->GetValue();

            static std::set<std::string> setbeforemaskinteger;
            std::set<std::string>::iterator it;
            int temp =countinteger;
            setbeforemaskinteger.insert (argument);
            countinteger=setbeforemaskinteger.size();

            std::cout << "setbeforemaskinteger contains : ";
            for (it=setbeforemaskinteger.begin(); it!=setbeforemaskinteger.end(); ++it)
            {
                std::cout << *it<<" , ";
            }
            std::cout << '\n';
            countinteger++;
            srand(time(NULL)*countinteger); //generates random seed val

           /* int userBeg =10000;
            int userEnd =20000;*/
            int randid = rand()%((userEnd - userBeg) + 1) + userBeg;
            replacement = std::to_string(randid);
            std::size_t pos=nodeString.find(argument);
            nodeString.replace(pos,argument.length(),replacement);
            std::cout<<argument<<"\n";
            std::cout<<nodeString<<"\n";


            currNode->SetValue((PMCHAR) nodeString.c_str());

            pEntityList->Seek(1, false);
            PENTITY pEntity = pEntityList->GetCurrElem();
            if(0 != pEntity)
            {
                // If the entity is not a node then it will be deleted immediately after use.
                // Therefore we need to get a copy.
                if(ENTITY_TYPE_NODE != pEntity->ul_Type)
                {
                    pEntityRes = pEntity->GetCopy();
                }
                else
                {
                    pEntityRes = pEntity;
                }
            }
            else
            {
                MemoryManager::Inst.CreateObject(&pNullRes);
            }
            currNode = (PNODE)pEntityList->GetCurrElem();
        }
    }
    else if(COMMAND_TYPE_MASK_PRICE == ulCommand)
    {
        std::cout<<"------------------------------------MASK PRICE-----------------------------------------\n";
        pEntityList->SeekToBegin();
        PNODE currNode = (PNODE)pEntityList->GetCurrElem();
        String* pStrArg = (String*)pArg;
        MSTRING argument=pStrArg->GetValue();
        MSTRING replacement;
        
        int lowererBound = std::stoi(currNode->GetLVal());
        int upperBound = std::stoi(currNode->GetRVal());
        while(currNode != 0)
        {
            MSTRING nodeString;
            std::cout<<currNode->GetValue()<<"\n";
            std::cout<<argument<<"\n";
            nodeString=currNode->GetValue();


            static std::set<std::string> setbeforemaskprice;
            std::set<std::string>::iterator it;
            int temp =countprice;
            setbeforemaskprice.insert (argument);
            countprice=setbeforemaskprice.size();

            std::cout << "setbeforemaskprice contains : ";
            for (it=setbeforemaskprice.begin(); it!=setbeforemaskprice.end(); ++it)
            {
                std::cout << *it<<" , ";
            }
            std::cout << '\n';
            countprice++;
            srand(time(NULL)*countprice); //generates random seed val

           /* int upperBound=50000;
            int lowererBound=10000;*/
            std::size_t decpos=(argument.find(".")+1);
            int precision=argument.length() - decpos;

            int randid = rand()%((upperBound - lowererBound) + 1) + lowererBound;
            MSTRING intPart = std::to_string(randid);
            MSTRING decPart;
            for(int i=0;i<precision;i++)
            {
                decPart+=(std::to_string(rand()%((9) + 1)));
            }
            reverse(intPart.begin(), intPart.end());
            int lengthrev = intPart.length();
            MSTRING final;
            MSTRING finalprice;
            int noofrounds=lengthrev/3;
            int count=1;
            int count2=1;

            if(lengthrev>3)
            {
                while(count2<=noofrounds)
                {
                    MSTRING sub=intPart.substr(count-1,3);
                    MSTRING subwithsep = sub+",";
                    finalprice+=subwithsep;
                    count+=3;
                    count2++;
                }
                MSTRING strremain = intPart.substr(count-1,lengthrev - count + 1);
                if(strremain.length()<1)
                {
                    finalprice= finalprice.substr(0,finalprice.length()-1);
                } else
                {
                    finalprice+=strremain;
                }
                reverse(finalprice.begin(), finalprice.end());
                final = finalprice+"."+decPart;
            }
            else
            {
                final = intPart+"."+decPart;
            }
            replacement =final;
            std::size_t pos=nodeString.find(argument);
            nodeString.replace(pos,argument.length(),replacement);
            std::cout<<argument<<"\n";
            std::cout<<nodeString<<"\n";


            currNode->SetValue((PMCHAR) nodeString.c_str());

            pEntityList->Seek(1, false);
            PENTITY pEntity = pEntityList->GetCurrElem();
            if(0 != pEntity)
            {
                // If the entity is not a node then it will be deleted immediately after use.
                // Therefore we need to get a copy.
                if(ENTITY_TYPE_NODE != pEntity->ul_Type)
                {
                    pEntityRes = pEntity->GetCopy();
                }
                else
                {
                    pEntityRes = pEntity;
                }
            }
            else
            {
                MemoryManager::Inst.CreateObject(&pNullRes);
            }
            currNode = (PNODE)pEntityList->GetCurrElem();
        }
    }
    else if(COMMAND_TYPE_MASK_NIC == ulCommand)
    {
        std::cout<<"------------------------------------MASK NIC-----------------------------------------\n";
        pEntityList->SeekToBegin();
        PNODE currNode = (PNODE)pEntityList->GetCurrElem();
        String* pStrArg = (String*)pArg;
        MSTRING argument=pStrArg->GetValue();
        MSTRING replacement;
        while(currNode != 0)
        {
            MSTRING nodeString;
            std::cout<<currNode->GetValue()<<"\n";
            std::cout<<argument<<"\n";
            nodeString=currNode->GetValue();

            static std::set<std::string> setbeforemasknic;
            std::set<std::string>::iterator it;
            int temp =countnic;
            setbeforemasknic.insert (argument);
            countnic=setbeforemasknic.size();

            std::cout << "setbeforemasknic contains : ";
            for (it=setbeforemasknic.begin(); it!=setbeforemasknic.end(); ++it)
            {
                std::cout << *it<<" , ";
            }
            std::cout << '\n';
            countnic++;
            srand(time(NULL)*countnic); //generates random seed val

            int diff =5;
            MSTRING rand1 = std::to_string(rand()%((30 - 1) + 1) + 1);
            MSTRING rand2 = std::to_string(rand()%((12 - 1) + 1) + 1);
            MSTRING rand5 = std::to_string(rand()%((99) + 1));
            MSTRING rand4;

            int year = std::stoi(argument.substr(4, 2));
            int highyear = (year + 2000 + diff) ;
            int lowyear = (year + 2000 - diff);
            int rand3int=rand()%((highyear - lowyear) + 1) + lowyear;
            MSTRING rand3 =std::to_string(rand3int).substr(2, 2);
            int individual = std::stoi(rand3);
            if(individual<40)
            {
                rand4 = std::to_string(rand()%((999) + 1));
            }
            else if(individual<55)
            {
                MSTRING rand41 = std::to_string(rand()%((499) + 1));
                MSTRING rand42 = std::to_string(rand()%((999 - 900) + 1) + 900);
                int randchoice=rand()%(2)+1;
                if(randchoice==1)
                {
                    rand4=rand41;
                }
                else if(randchoice==2)
                {
                    rand4=rand42;
                }
            }
            else if(individual<100)
            {
                MSTRING rand41 = std::to_string(rand()%((750) + 1));
                MSTRING rand42 = std::to_string(rand()%((999 - 900) + 1) + 900);
                int randchoice=rand()%(2)+1;
                if(randchoice==1)
                {
                    rand4=rand41;
                }
                else if(randchoice==2)
                {
                    rand4=rand42;
                }
            }
            if(rand1.length()==1)
            {
                rand1="0"+rand1;
            }
            if(rand2.length()==1)
            {
                rand2="0"+rand2;
            }
            if(rand5.length()==1)
            {
                rand5="0"+rand5;
            }
            if(rand4.length()==1)
            {
                rand4="00"+rand4;
            }
            else if(rand4.length()==2)
            {
                rand4="0"+rand4;
            }
            replacement=rand1+rand2+rand3+rand4+rand5;
            std::size_t pos=nodeString.find(argument);
            nodeString.replace(pos,argument.length(),replacement);
            std::cout<<argument<<"\n";
            std::cout<<nodeString<<"\n";


            currNode->SetValue((PMCHAR) nodeString.c_str());

            pEntityList->Seek(1, false);
            PENTITY pEntity = pEntityList->GetCurrElem();
            if(0 != pEntity)
            {
                // If the entity is not a node then it will be deleted immediately after use.
                // Therefore we need to get a copy.
                if(ENTITY_TYPE_NODE != pEntity->ul_Type)
                {
                    pEntityRes = pEntity->GetCopy();
                }
                else
                {
                    pEntityRes = pEntity;
                }
            }
            else
            {
                MemoryManager::Inst.CreateObject(&pNullRes);
            }
            currNode = (PNODE)pEntityList->GetCurrElem();
        }
    }
    else if(COMMAND_TYPE_MASK_EMAIL == ulCommand)
    {
        std::cout<<"------------------------------------MASK EMAIL-----------------------------------------\n";
        pEntityList->SeekToBegin();
        PNODE currNode = (PNODE)pEntityList->GetCurrElem();
        String* pStrArg = (String*)pArg;
        MSTRING argument=pStrArg->GetValue();
        MSTRING replacement;
        while(currNode != 0)
        {
            MSTRING nodeString;
            std::cout<<currNode->GetValue()<<"\n";
            std::cout<<argument<<"\n";
            nodeString=currNode->GetValue();

            static std::set<std::string> setbeforemaskemail;
            std::set<std::string>::iterator it;
            int temp =countemail;
            setbeforemaskemail.insert (argument);
            countemail=setbeforemaskemail.size();
            MSTRING tempStr=std::to_string(countemail);
            MSTRING tempReplace;
            std::cout << "setbeforemaskemail contains : ";
            for (it=setbeforemaskemail.begin(); it!=setbeforemaskemail.end(); ++it)
            {
                std::cout << *it<<" , ";
            }
            std::cout << '\n';
            srand(time(NULL)*countemail); //generates random seed val

            std::size_t posAt=argument.find("@");
            MSTRING sub = argument.substr(0,posAt);
            MSTRING remainder = argument.substr(posAt,argument.length()-sub.length());
            if(connection)
            {
                MysqlConnector mysqlobj;
                int resultmale = mysqlobj.existsFirstNameMale(connection,sub);
                int resultfemale = mysqlobj.existsFirstNameFemale(connection,sub);
                MSTRING username = "";

                if(resultmale==1)
                {
                    int randid = rand()%((7732 - 1) + 1) + 1;
                    username = mysqlobj.selectFirstNameMale(connection,randid);
                }
                else if(resultfemale == 1)
                {
                    int randid = rand()%((7144 - 1) + 1) + 1;
                    username = mysqlobj.selectFirstNameFemale(connection,randid);
                }
                else
                {
                    int randid = rand()%((744 - 1) + 1) + 1;
                    username = mysqlobj.selectFirstNameUnisex(connection,randid);
                }
                replacement =username+remainder;
            }
            else
            {
                perror ("The Database could not be connected, Please check the db connection!");
                throw _exception();
            }
            std::size_t pos=nodeString.find(argument);
            nodeString.replace(pos,argument.length(),replacement);
            std::cout<<argument<<"\n";
            std::cout<<nodeString<<"\n";


            currNode->SetValue((PMCHAR) nodeString.c_str());

            pEntityList->Seek(1, false);
            PENTITY pEntity = pEntityList->GetCurrElem();
            if(0 != pEntity)
            {
                // If the entity is not a node then it will be deleted immediately after use.
                // Therefore we need to get a copy.
                if(ENTITY_TYPE_NODE != pEntity->ul_Type)
                {
                    pEntityRes = pEntity->GetCopy();
                }
                else
                {
                    pEntityRes = pEntity;
                }
            }
            else
            {
                MemoryManager::Inst.CreateObject(&pNullRes);
            }
            currNode = (PNODE)pEntityList->GetCurrElem();
        }
    }
    else
    {
        if(0 != p_Arg)
        {
            p_EntityArg = p_Arg->Execute(pContext);
        }
        MemoryManager::Inst.CreateObject(&pListRes);
        EntityList::const_iterator ite1 = pEntityList->begin();
        EntityList::const_iterator iteEnd1 = pEntityList->end();
        for( ; ite1 != iteEnd1; ++ite1)
        {
            PENTITY pRes = 0;
            if ((*ite1)->ul_Type == ENTITY_TYPE_NODE) {
                pRes = ExecuteNodeCommand(ulCommand, *ite1, pContext);
            } else {
                pRes = ExecuteEntityCommand(ulCommand, *ite1, p_EntityArg);
            }

            switch(pRes->ul_Type)
            {
                case ENTITY_TYPE_NULL:
                {
                    MemoryManager::Inst.DeleteObject(pRes);
                }
                case ENTITY_TYPE_INT:
                case ENTITY_TYPE_NODE:
                case ENTITY_TYPE_STRING:
                case ENTITY_TYPE_LIST:
                {
                    pListRes->push_back(pRes);
                    break;
                }
                case ENTITY_TYPE_BOOL:
                {
                    if(((PBool)pRes)->GetValue())
                    {
                        if(ENTITY_TYPE_NODE != (*ite1)->ul_Type)
                        {
                            pListRes->push_back((*ite1)->GetCopy());
                        }
                        else
                        {
                            pListRes->push_back(*ite1);
                        }
                    }
                    MemoryManager::Inst.DeleteObject(pRes);
                    break;
                }
            }
        }
    }

    if(0 != pIntRes)
    {
        return pIntRes;
    }
    if(0 != pListRes)
    {
        return pListRes;
    }
    if(0 != pNullRes)
    {
        return pNullRes;
    }
    if(0 != pEntityRes)
    {
        return pEntityRes;
    }
    if(0 != pNodeRes)
    {
        return pNodeRes;
    }
    if(0 != pStrRes)
    {
        return pStrRes;
    }
    return 0;
}

void Command::AddSubtreeToNodeList(PENTITYLIST pList, PNODE pRoot)
{
    pList->push_back(pRoot);
    PNODE pChild = pRoot->GetFirstChild();
    while(0 != pChild)
    {
        AddSubtreeToNodeList(pList, pChild);
        pChild = pChild->GetRightSibling();
    }
}

void Command::FilterSubTree(PNODE root, ExecutionTemplate* arg, ExecutionContext* context, PENTITYLIST resultList)
{
    context->map_Var[context->p_MD->s_ListItemVar] = root;
    PBool res = (PBool)arg->Execute(context);
    if (res->GetValue()) {
        resultList->push_back(root);
        //std::cout<<root ->GetValue()<<"\n";
    }
    PNODE pChild = root->GetFirstChild();
    while(0 != pChild)
    {
        FilterSubTree(pChild, arg, context, resultList);
        pChild = pChild->GetRightSibling();
    }
}
