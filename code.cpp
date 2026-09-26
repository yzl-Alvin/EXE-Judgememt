#include <iostream>
#include <string>
#include <sstream>
using namespace std;
int main() {
    ios::sync_with_stdio(false);
    cin.tie(0);
    string content, line;
    while (getline(cin, line)) {
        content += line + "\n";
    }
    if (content.find("#include<bitss/std") != string::npos) {
        cout << ".h: No such file or directory" << endl;
        return 0;
    }
    if (content.find("//using namespace std") != string::npos) {
        cout << "'cout' was not declared in this scope" << endl;
        return 0;
    }
    if (content.find("int mian") != string::npos) {
        if (content.find("\"hello,world\";") != string::npos) {
            cout << "collect2.exe	 ld returned 1 exit status" << endl;
        } else {
            cout << "expected';'before..." << endl;
        }
        return 0;
    }
    cout << "Hello,World!" << endl;
    return 0;
}

