#include "FileUtils.h"

FileUtils::FileUtils()
{
    //ctor
}

std::string FileUtils::loadFileInString(const std::string &filePath) {
    std::ifstream file;
    file.open (filePath.c_str());

    std::string output;
    std::string line;

    if (file.is_open ()) {
        while (file.good ()) {
            getline (file, line);
            output.append (line + "\n");
        }
    } else {
        std::cout << "Unable to load file: " << filePath << std::endl;
    }
    file.close ();

    return output;
}

FileUtils::~FileUtils()
{
    //dtor
}
