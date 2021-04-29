#include "FileTransfer.h"

#define CMD_BUFSIZE 1024

inline char separator() {
    #ifdef _WIN32
        return '\\';
    #else
        return '/';
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
            std::cout << CMD << std::endl;
            return 0;
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
        SCPArgumentsBuilder() {
            port = 22; // default SSH port
            limit = 1000; // 1000 KB/s
        }
        char const * build(int pathSize) {
            int const ARGS_BUFSIZE = 1024 - pathSize;
            char args[ARGS_BUFSIZE];
            // what is a reaaaally not bad way to do this...

            std::string cppstrtmp;
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

void GetOpenSSHDirectory(std::string &path, TCHAR *SYSTEM_DIR, size_t size);
void terminalExitCallbackHandler(int signum);
void programExit(int exitCode);

int main() {
    std::string OPENSSH_DIR;
    TCHAR SYSTEM_DIR[MAX_PATH];
    GetWindowsDirectory(SYSTEM_DIR, MAX_PATH);
    
    GetOpenSSHDirectory(OPENSSH_DIR, SYSTEM_DIR, MAX_PATH);
    OpenSSHHandler sshHandler(OPENSSH_DIR);

    if (FILE *file = fopen(sshHandler.SCP_DIR.c_str(), "r")) {
        std::cout << "OpenSSH is installed!" << std::endl;
        fclose(file);
    } else {
        std::cout << "OpenSSH is not installed on your machine, or is not located in your PATH variable." << std::endl;
    }

    // std::system("C:\\Windows\\sysnative\\OpenSSH\\ssh.exe -p 2222 mmiyamoto@petdandertutorials.com");

    // std::cout << sshHandler.SSH_DIR.length() << std::endl;
    // std::cout << WindowsProcessDelegate::exec(sshHandler.SSH_DIR.c_str(), "-p 2222") << std::endl;
    
    signal(SIGINT, terminalExitCallbackHandler);

    programExit(0);
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
            programExit(1);
        }
    }
}

void terminalExitCallbackHandler(int signum) {
    programExit(signum);
}

void programExit(int exitCode) {
    std::cout << "Program written by Makoto Miyamoto. https://makotomiyamoto.com/filetransfer" << std::endl;
    std::exit(exitCode);
}
