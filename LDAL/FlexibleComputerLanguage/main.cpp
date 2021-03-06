//
//  main.cpp
//  FlexibleComputerLanguage
//
//  Created by Dileepa Jayathilaka on 5/8/18.
//  Copyright (c) 2018 Dileepa Jayathilaka. All rights reserved.
//
// VERSION 2

#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "json.hpp"
#include "MemMan.h"
#include "Node.h"
#include "MetaData.h"
#include "ExecutionTemplateList.h"
#include "ExecutionContext.h"
#include "TestCaseExecuter.h"
#include "Int.h"
#include "LogJsonParser.h"
#include "easylogging++.h"
#include <iostream>
#include <memory>
#include <pthread.h>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "QueryExecuter.h"

using namespace rapidjson;
using json = nlohmann::json;

INITIALIZE_EASYLOGGINGPP

int main(int argc, const char * argv[])
{
    std::cout<<"Log analyzer masking started\n";
    std::string line;
    std::string jsonline;

    std::ifstream jsonfile ("../../Files/resultJSON.json");

    if (jsonfile.is_open())
    {
        getline (jsonfile,line);
        jsonline = line;
        jsonfile.close();
    }

    Node* jsonroot = LogJsonParser::LogJSONToNodeTree(jsonline);

    std::string scriptline;
    std::ifstream scriptfile ("../../Files/maskingScript.txt");
    std::string script="";

    while(getline(scriptfile,scriptline))
    {
        script+=scriptline;
        script+="\n";
    }

    std::string res = QueryExecuter::run(jsonroot,script);

    LogJsonParser::LogNodeTreetoJsonRecursivly(jsonroot);

    LogJsonParser::LogNodeTreetoLog(jsonroot);

    std::cout<<"\n\nMasking successfully Completed";
    std::cout<<"\n\nPress Enter To Exit";
    std::getchar();


    return 0;
}
