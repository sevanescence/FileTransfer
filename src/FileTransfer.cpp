#include "FileTransfer.h"

#define CMD_BUFSIZE 1024

inline char separator() {
    #ifdef _WIN32
        return '\\';
    #else
        return '/';
    #endif
}

inline char const *separatorConst() {
    #ifdef _WIN32
        return "\\";
    #else
        return "/";
    #endif
}

// C:\Windows\System32\OpenSSH

class WindowsProcessDelegate {
    public:
        static int exec(char const *cmdPath, char const *args) {
            char CMD[CMD_BUFSIZE];
            sprintf(CMD, cmdPath);
            int start;
            for (int i = 0; i < CMD_BUFSIZE; i++) {
                if (CMD[i] == '\0') {
                    start = i;
                    break;
                }
            }
            for (int i = start; i < CMD_BUFSIZE; i++) {
                CMD[i] = args[i-start];
            }
            return std::system(CMD);
        }
};

class OpenSSHHandler {
    public:
        std::string OPENSSH_DIR;
        std::string SSH_DIR;
        std::string SCP_DIR;
        OpenSSHHandler(std::string OPENSSH_DIR) {
            this->OPENSSH_DIR = OPENSSH_DIR;
            SSH_DIR.assign(OPENSSH_DIR).push_back(separator());
            SSH_DIR.append("ssh.exe");
            SCP_DIR.assign(OPENSSH_DIR).push_back(separator());
            SCP_DIR.append("scp.exe");
        }
};

struct RemoteFilePipelineMeta {
    std::string remotePath;
    std::string localTarget;
};

class SCPArgumentsBuilder {
    private:
        void appendWithPrefixIfNotEmpty(std::string &target, std::string source, char const *pfx) {
            if (source.length() == 0) {
                return;
            }

            int const pfx_len = 2;
            for (int i = 0; i < pfx_len; i++) {
                target.push_back(pfx[i]);
            }
            target.push_back(' ');
            target.append(source);
            target.push_back(' ');
        }
    public:
        std::string cipher;
        std::string ssh_config;
        std::string identity_file;
        uint32_t limit;
        std::string ssh_option;
        uint16_t port;
        std::string program;
        std::string source;
        SCPArgumentsBuilder() {
            port = 22; // default SSH port
            limit = 1000; // 1000 KB/s
        }
        char const * build(RemoteFilePipelineMeta meta, int pathSize) {
            int const ARGS_BUFSIZE = 1024 - pathSize;
            char args[ARGS_BUFSIZE];
            // what is a reaaaally not bad way to do this...

            std::string cppstrtmp;
            cppstrtmp.append(" ").append(source).append(" ");
            appendWithPrefixIfNotEmpty(cppstrtmp, cipher, "-c");
            appendWithPrefixIfNotEmpty(cppstrtmp, ssh_config, "-F");
            appendWithPrefixIfNotEmpty(cppstrtmp, identity_file, "-i");
            appendWithPrefixIfNotEmpty(cppstrtmp, ssh_option, "-o");
            appendWithPrefixIfNotEmpty(cppstrtmp, program, "-S");
            
            appendWithPrefixIfNotEmpty(cppstrtmp, std::to_string(port), "-P");
            if (limit != 0) {
                appendWithPrefixIfNotEmpty(cppstrtmp, std::to_string(limit), "-l");
            }

            char *tmp = (char *) malloc(sizeof(char) + (cppstrtmp.length() + 1));
            strcpy(tmp, cppstrtmp.c_str());

            return tmp;
        }

};

// strcat specialized for C-style strings with a MAX_PATH length.
// Do not use for anything else.
void pathstrcat(char *target, char const *source);
size_t const cstrlen(char *str);

class DataFolderManager {
    private:
        char *DATA_FOLDER;
        char const SETTINGS_CONFIG_FILENAME[13] = "settings.cfg";
        char const PIPELINE_CONFIGURATIONS[15] = "pipelines.json";
    public:
        DataFolderManager(char *__DATA_FOLDER) {
            DATA_FOLDER = __DATA_FOLDER;
        }
        char *getDataFolder() {
            return DATA_FOLDER;
        }
        char *getSettingsConfigPath() {
            char *tmp = (char *) malloc(MAX_PATH + 1);
            for (int i = 0; DATA_FOLDER[i] != '\0'; i++) {
                tmp[i] = DATA_FOLDER[i];
            }
            pathstrcat(tmp, separatorConst());
            pathstrcat(tmp, SETTINGS_CONFIG_FILENAME);
            return tmp;
        }
        char *getPipelineConfigPath() {
            char *tmp = (char *) malloc(cstrlen(DATA_FOLDER) + 15 + 1);
            for (int i = 0; DATA_FOLDER[i] != '\0'; i++) {
                tmp[i] = DATA_FOLDER[i];
            }
            std::cout << tmp << std::endl;
            std::cout << PIPELINE_CONFIGURATIONS << std::endl;
            pathstrcat(tmp, separatorConst());
            pathstrcat(tmp, PIPELINE_CONFIGURATIONS);
            return tmp;
        }
};

void GetOpenSSHDirectory(std::string &path, TCHAR *SYSTEM_DIR, size_t size);
void GetRoamingFolder(char *APPDATA_PATH);
void InitializeDataFolder(char *DATA_FOLDER);
void TerminalExitCallbackHandler(int signum);
void ProgramExit(int exitCode);

// todo vscode compiler args should fix intellisense

int main() {
    HWND WINDOW = GetConsoleWindow();
    TCHAR SYSTEM_PATH[MAX_PATH];
    GetWindowsDirectory(SYSTEM_PATH, MAX_PATH);

    char DATA_FOLDER[MAX_PATH];
    InitializeDataFolder(DATA_FOLDER);
    DataFolderManager dataFolderManager(DATA_FOLDER);
    std::cout << dataFolderManager.getPipelineConfigPath() << std::endl;
    
    std::string OPENSSH_DIR;
    GetOpenSSHDirectory(OPENSSH_DIR, SYSTEM_PATH, MAX_PATH);
    OpenSSHHandler sshHandler(OPENSSH_DIR);
    SCPArgumentsBuilder builder;



    // std::system("C:\\Windows\\sysnative\\OpenSSH\\ssh.exe -p 2222 mmiyamoto@petdandertutorials.com");

    // std::cout << sshHandler.SSH_DIR.length() << std::endl;
    // std::cout << WindowsProcessDelegate::exec(sshHandler.SSH_DIR.c_str(), "-p 2222") << std::endl;
    
    
    
    signal(SIGINT, TerminalExitCallbackHandler);

    ProgramExit(0);
}

void pathstrcat(char *target, char const *source) {
    int start = 0;
    for (; target[start] != '\0'; start++);
    for (int i = 0; source[i] != '\0'; i++) {
        target[start + i] = source[i];
    }
}

size_t const cstrlen(char *str) {
    size_t tmp = 0;
    for (; str[tmp] != '\0'; tmp++);
    return tmp;
}

void GetOpenSSHDirectory(std::string &path, TCHAR *SYSTEM_DIR, size_t size) {
    for (int i = 0; i < size; i++) {
        if (SYSTEM_DIR[i] == '\0') {
            break;
        }
        path.push_back(SYSTEM_DIR[i]);
    }

    path.push_back(separator());
    path.append("sysnative");
    path.push_back(separator());
    path.append("OpenSSH");

    std::string tmp;
    tmp.assign(path);
    tmp.push_back(separator());
    tmp.append("scp.exe");
    if (FILE *file = fopen(tmp.c_str(), "r")) {
        //std::cout << "File exists!" << '\n';
        fclose(file);
    } else {
        std::cout << "OpenSSH not found in default path. Scanning PATH." << std::endl;
        std::string PATH (std::getenv("path"));

        using namespace std::regex_constants;
        std::regex pathRegex("[^;]+?OpenSSH", ECMAScript | icase);
        std::smatch match;
        if (std::regex_search(PATH, match, pathRegex)) {
            path.assign(*match.begin());
            std::cout << "OpenSSH found at " << path << std::endl;
            path.assign(std::regex_replace(path, std::regex("System32", ECMAScript | icase), "sysnative"));
        } else {
            std::cout << "Error: OpenSSH is not installed. OpenSSH is required to run this program." << std::endl;
            ProgramExit(1);
        }
    }
}

void GetRoamingFolder(char *APPDATA_PATH) {
    TCHAR wAPPDATA_PATH[MAX_PATH];
    SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, wAPPDATA_PATH);
    wcstombs(APPDATA_PATH, wAPPDATA_PATH, wcslen(wAPPDATA_PATH) + 1);
}

void InitializeDataFolder(char *DATA_FOLDER) {
    GetRoamingFolder(DATA_FOLDER);
    size_t const dataFolderSize = 14;
    char const DATA_FOLDER_TITLE[dataFolderSize] = "FileTransfer";
    pathstrcat(DATA_FOLDER, separatorConst());
    pathstrcat(DATA_FOLDER, DATA_FOLDER_TITLE);
}

void TerminalExitCallbackHandler(int signum) {
    ProgramExit(signum);
}

void ProgramExit(int exitCode) {
    std::cout << "Program written by Makoto Miyamoto. https://makotomiyamoto.com/filetransfer" << std::endl;
    std::exit(exitCode);
}
