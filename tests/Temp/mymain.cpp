#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
using namespace std;

int _parseCommandLine(const char* cmd_line) {
  int i = 0;
  std::istringstream iss(string(cmd_line).c_str());
  for(std::string s; iss >> s; ) {
    char* args = (char*)malloc(s.length()+1);
    memset(args, 0, s.length()+1);
    strcpy(args, s.c_str());
    cout << args <<endl;
  }
  return i;
}

int main()
{
    cout << _parseCommandLine("asd af ssd") << endl;
    return 0;
}