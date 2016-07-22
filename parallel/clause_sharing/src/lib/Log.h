//
// Created by Matteo on 19/07/16.
//

#ifndef CLAUSE_SHARING_LOG_H
#define CLAUSE_SHARING_LOG_H

#include <ctime>
#include <iostream>
#include <_types/_uint8_t.h>

class Log {
private:
    Log() { }

public:
    static const uint8_t INFO = 1;
    static const uint8_t WARNING = 2;
    static const uint8_t ERROR = 3;

    static void log(uint8_t level, std::string message) {
        std::string record;
        record += std::to_string(std::time(NULL));
        record += "\t";
        switch (level) {
            case INFO:
                record += "INFO\t";
                break;
            case WARNING:
                record += "WARNING\t";
                break;
            case ERROR:
                record += "ERROR\t";
                break;
            default:
                record += "UNKNOWN\t";
        }
        record += message;
        std::cerr << record << "\n";
    }
};


#endif //CLAUSE_SHARING_LOG_H
