#ifndef FILEUTILS_H
#define FILEUTILS_H

#include <string>
#include <fstream>
#include <iostream>
#include "rapidjson/document.h"

class FileUtils
{
    public:
        static std::string loadFileInString(const std::string &filePath);
        virtual ~FileUtils();

    protected:

    private:
        FileUtils();
};

#endif // FILEUTILS_H
