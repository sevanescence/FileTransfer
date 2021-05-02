#include "FileTransfer.h"

#define CMD_BUFSIZE 1024

inline char separator() {
    #ifdef _WIN32
        return '\\';
    #else
        return '/';
    #endif
}

// I created this because declaring separator
// as const would have made me have to rewrite
// other code that I didn't feel like
// rewriting.
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
            int const ARGS_BUFSIZE = CMD_BUFSIZE - pathSize;
            char args[ARGS_BUFSIZE];
            // what is a reaaaally not bad way to do this...

            std::string cppstrtmp;
            cppstrtmp.append(" ").append(source).append(" ");
            // get source and target from meta
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

std::ostream &operator<<(std::ostream &os, SCPArgumentsBuilder const &builder) {
    os << "cipher=\"" << builder.cipher << "\"" << std::endl;
    os << "ssh_config=\"" << builder.ssh_config << "\"" << std::endl;
    os << "identity_file=\"" << builder.identity_file << "\"" << std::endl;
    os << "limit=" << builder.limit << std::endl;
    os << "ssh_option=\"" << builder.ssh_option << "\"" << std::endl;
    os << "port=" << builder.port << std::endl;
    os << "program=\"" << builder.program << "\"" << std::endl;
    os << "source=\"" << builder.source << "\"" << std::endl;
    return os;
}


std::istream &operator>>(std::istream &is, SCPArgumentsBuilder &builder) {
    std::map<std::string, std::string &> builderStringProps
    { 
        { "cipher", builder.cipher },
        { "ssh_config", builder.ssh_config },
        { "identity_file", builder.identity_file },
        { "ssh_option", builder.ssh_option },
        { "program", builder.program },
        { "source", builder.source }
    };
    // check limit and port manually. they are different data types so cant
    // be put in their own single map ¯\_(ツ)_/¯


    // hardcoded cfg parse implementation because i am lazy

    std::string c;
    std::map<std::string, std::string &>::iterator it;

    using namespace std::regex_constants;

    std::smatch matches;
    std::smatch stmp;

    std::regex cfgRgx("[A-z]+?(?=(\\s+?)?=)");
    std::regex cfgItemStrRgx("\"(.+?)\"");
    std::regex cfgItemRgx("=(\\s+?)?(.+?)\\b");
    std::regex limitOrPortRgx("(limit|port)");

    while (std::getline(is, c)) {
        if (std::regex_search(c, matches, limitOrPortRgx)) {
            std::regex_search(c, stmp, cfgItemRgx);
            int n = std::stoi(stmp[2]);
            if (matches[0] == "limit") {
                builder.limit = n;
            } else {
                if (n < 22 || n > 65535) {
                    std::cout << "ERROR: Port must not be below 22 or above 65535. Defaulting to port 22." << std::endl;
                }
                builder.port = n;
            }
            continue;
        }

        if (std::regex_search(c, matches, cfgRgx)) {
            std::regex_search(c, stmp, cfgItemStrRgx);
            // find matches[0] in map
            it = builderStringProps.find(matches[0]);
            if (it != builderStringProps.end()) {
                it->second = stmp[1];
            }
        }
    }
    return is;
}


// strcat specialized for C-style strings with a MAX_PATH length.
// Do not use for anything else.
void pathstrcat(char *target, char const *source);
size_t const cstrlen(char *str);

class DataFolderManager {
    private:
        char *DATA_FOLDER;
        char const SETTINGS_CONFIG[13] = "settings.cfg";
        char const PIPELINE_CONFIG_FOLDER[10] = "pipelines";
    public:
        DataFolderManager(char *__DATA_FOLDER) {
            DATA_FOLDER = __DATA_FOLDER;
        }
        char *getDataFolder() {
            return DATA_FOLDER;
        }
        char *getSettingsConfigPath() {
            char *tmp = (char *) malloc(MAX_PATH + 1);
            memcpy(tmp, DATA_FOLDER, MAX_PATH);
            strcat(tmp, separatorConst());
            strcat(tmp, SETTINGS_CONFIG);

            return tmp;
        }
        char *getPipelineConfigFolder() {
            char *tmp = (char *) malloc(MAX_PATH + 1);
            memcpy(tmp, DATA_FOLDER, MAX_PATH);
            strcat(tmp, separatorConst());
            strcat(tmp, PIPELINE_CONFIG_FOLDER);

            return tmp;
        }
        // Fetch the settings file with the specified parsing option.
        FILE *getSettingsConfig(const char *__o) {
            char const *path = getSettingsConfigPath();
            return fopen(path, __o);
        }
};

void GetOpenSSHDirectory(std::string &path, TCHAR *SYSTEM_DIR, size_t size);
void GetRoamingFolder(char *APPDATA_PATH);
void InitializeDataFolder(char *DATA_FOLDER);
void TerminalExitCallbackHandler(int signum);
void ProgramExit(int exitCode);
bool SettingsSetupPrompt(SCPArgumentsBuilder &builder);
bool FileExists(char *path);

// todo vscode compiler args should fix intellisense

// TODO:
// 1. prompt settings setup
// 2. ask user if they want to save settings
// 3. display main menu:
//
// []=>=>=>[] FILE TRANSFER []=>=>=>[]
// [] 1. Settings
// [] 2. Pipelines
// [] 3. Run pipelines
// []
// [] CTRL+C to quit
// [] 
// [] > 

// if 1 is pressed, display settings menu:
//
// []=>=>=>[] SETTINGS []=>=>=>[]
// [] cipher=""
// [] ssh_config=""
// [] identity_file=""
// [] limit=1000
// [] ssh_option=""
// [] port=22
// [] program=""
// [] source=""
// []
// [] Type any setting to reassign.
// [] Press BACKSPACE to go back
// [] 
// [] > 

// if 2 is pressed, display pipelines menu:

// []=>=>=>[] PIPELINES []=>=>=>[]
// [] 1. essay
// [] 2. math_answers
// [] 
// [] n. Create new pipeline
// [] d <name>. Delete pipeline
// [] Press BACKSPACE to go back
// []
// [] > 

// []=>=>=>[] ESSAY []=>=>=>[]
// [] remote: mmiyamoto@petdandertutorials.com:/home/mmiyamoto/assignments/essay.txt
// [] local: C:/Users/damia/documents/essay.txt
// []
// [] Type any setting to reassign
// [] Press BACKSPACE to go back
// []
// [] > 

namespace Options {
    namespace MainMenu {
        int const SETTINGS = 1;
        int const PIPELINES = 2;
        int const RUN_PIPELINES = 3;
    }
    namespace System {
        int const BACKSPACE = 8;
    }
    namespace Settings {
        int const CIPHER = 49;
        int const SSH_CONFIG = 50;
        int const IDENTITY_FILE = 51;
        int const LIMIT = 52;
        int const SSH_OPTION = 53;
        int const PORT = 54;
        int const PROGRAM = 55;
        int const SOURCE = 56;
    }
}

class MainProcess {
    private:
        OpenSSHHandler *sshHandler;
        SCPArgumentsBuilder *builder;
        DataFolderManager *dataFolderManager;
    public:
        MainProcess(OpenSSHHandler &_handler, SCPArgumentsBuilder &_builder, DataFolderManager &_manager) {
            this->sshHandler = &_handler;
            this->builder = &_builder;
            this->dataFolderManager = &_manager;
        }
        OpenSSHHandler &getOpenSSHHandler() {
            return *sshHandler;
        }
        SCPArgumentsBuilder &getSCPArgumentsBuilder() {
            return *builder;
        }
        DataFolderManager &getDataFolderManager() {
            return *dataFolderManager;
        }
};

int MainMenu(MainProcess &process);
int Settings(MainProcess &process);
int Pipelines(MainProcess &process);
int RunPipelines(MainProcess &proces);

int main() {
    signal(SIGINT, TerminalExitCallbackHandler);
    HWND WINDOW = GetConsoleWindow();
    TCHAR SYSTEM_PATH[MAX_PATH];
    GetWindowsDirectory(SYSTEM_PATH, MAX_PATH);
    
    std::string OPENSSH_DIR;
    GetOpenSSHDirectory(OPENSSH_DIR, SYSTEM_PATH, MAX_PATH);
    OpenSSHHandler sshHandler(OPENSSH_DIR);
    SCPArgumentsBuilder builder;

    char DATA_FOLDER[MAX_PATH];
    InitializeDataFolder(DATA_FOLDER);
    DataFolderManager dataFolderManager(DATA_FOLDER);

    if (! FileExists(dataFolderManager.getDataFolder())) {
        _mkdir(dataFolderManager.getDataFolder());
    }

    if (FILE *settings_r = fopen(dataFolderManager.getSettingsConfigPath(), "r")) {
        std::ifstream settings_ifstream(dataFolderManager.getSettingsConfigPath());
        settings_ifstream >> builder;
        settings_ifstream.close();
        fclose(settings_r);
    } else { // settings.cfg does not exist
        bool save = SettingsSetupPrompt(builder);
        if (save) {
            std::ofstream settings_ofstream(dataFolderManager.getSettingsConfigPath());
            settings_ofstream << builder;
            settings_ofstream.close();
            std::cout << "Settings saved to " << dataFolderManager.getSettingsConfigPath() << std::endl;
            std::system("pause");
        }

        fclose(settings_r);
    }

    MainProcess process(sshHandler, builder, dataFolderManager);

    int option;
    for (;;) {
        option = MainMenu(process);
        switch (option) {
            case Options::MainMenu::SETTINGS:
                Settings(process);
                break;
            case Options::MainMenu::PIPELINES:
                Pipelines(process);
                break;
            case Options::MainMenu::RUN_PIPELINES:
                RunPipelines(process);
                break;
            case 6683108: // ^C, somehow
            default:
                ProgramExit(0);
        }
    }
}

// functional programming FTW

// []=>=>=>[] FILE TRANSFER []=>=>=>[]
// [] 1. Settings
// [] 2. Pipelines
// [] 3. Run pipelines
// []
// [] CTRL+C to quit
// [] 
// [] > 

int MainMenu(MainProcess &process) {
    std::system("cls");
    std::cout << "[]=>=>=>[] FILE TRANSFER []=>=>=>[]" << std::endl;
    std::cout << "[] 1. Settings\n[] 2. Pipelines\n[] 3. Run Pipelines\n[]\n[] CTRL+C or 0 to quit\n[]\n[] > ";

    int ch;
    std::cin >> ch;
    return ch;
}

// []=>=>=>[] SETTINGS []=>=>=>[]
// [] cipher=""
// [] ssh_config=""
// [] identity_file=""
// [] limit=1000
// [] ssh_option=""
// [] port=22
// [] program=""
// []
// [] Type any setting to reassign.
// [] Press BACKSPACE to go back
// [] 
// [] > 

void SettingsOptionReassign(MainProcess process, int setting) {
    std::system("cls");
    std::cin.clear();
    std::cin.ignore(123, '\n');
    SCPArgumentsBuilder &builder = process.getSCPArgumentsBuilder();
    std::map<int, std::string &> builderStringProps
    { 
        { Options::Settings::CIPHER, builder.cipher },
        { Options::Settings::SSH_CONFIG, builder.ssh_config },
        { Options::Settings::IDENTITY_FILE, builder.identity_file },
        { Options::Settings::SSH_OPTION, builder.ssh_option },
        { Options::Settings::PROGRAM, builder.program },
        { Options::Settings::SOURCE, builder.source }
    }; // im very good at programming, yes.
    char const propertyNames[8][24] = {"cipher", "ssh_config", "identity_file", "limit", "ssh_option", "port", "program", "source"};
    std::map<int, std::string &>::iterator it = builderStringProps.find(setting);
    if (it != builderStringProps.end()) {
        std::cout << propertyNames[setting - Options::Settings::CIPHER] << "=\"" << it->second << "\"\n";
    } else {
        std::string option_name = setting == Options::Settings::LIMIT ? "limit" : "port";
        int option_value = setting == Options::Settings::LIMIT ? builder.limit : builder.port;
        std::cout << option_name << "=" << option_value << std::endl;
    }
    std::cout << "Type new configuration: ";
    std::string c;
    std::getline(std::cin, c);
    if (it != builderStringProps.end()) {
        it->second.assign(c);
    } else {
        if (setting == Options::Settings::LIMIT) {
            builder.limit = std::stoi(c);
        } else {
            builder.port = std::stoi(c);
        }
    }
    std::cout << "Saving settings configuration..." << std::endl;
    std::ofstream settings(process.getDataFolderManager().getSettingsConfigPath());
    settings << builder;
    settings.close();
    std::cout << "Configuration saved!" << std::endl;
    std::system("pause");
}

int Settings(MainProcess &process) {
    std::system("cls");
    SCPArgumentsBuilder &builder = process.getSCPArgumentsBuilder();
    std::cout << "[]=>=>=>[] SETTINGS []=>=>=>[]" << '\n';
    std::cout << "[] 1. cipher=\"" << builder.cipher << "\"\n";
    std::cout << "[] 2. ssh_config=\"" << builder.ssh_config << "\"\n";
    std::cout << "[] 3. identity_file=\"" << builder.identity_file << "\"\n";
    std::cout << "[] 4. limit=" << builder.limit << '\n';
    std::cout << "[] 5. ssh_option=\"" << builder.ssh_option << "\"\n";
    std::cout << "[] 6. port=" << builder.port << '\n';
    std::cout << "[] 7. program=\"" << builder.program << "\"\n";
    std::cout << "[] 8. source=\"" << builder.source << "\"\n";
    std::cout << "[]\n[] Type any setting to reassign.\n[] Press BACKSPACE to go back.\n[]\n[] > ";
    int option = getch();
    switch (option) {
        case Options::Settings::CIPHER:
        case Options::Settings::SSH_CONFIG:
        case Options::Settings::IDENTITY_FILE:
        case Options::Settings::LIMIT:
        case Options::Settings::SSH_OPTION:
        case Options::Settings::PORT:
        case Options::Settings::PROGRAM:
        case Options::Settings::SOURCE:
            SettingsOptionReassign(process, option);
            break;
        case Options::System::BACKSPACE:
            return 0;
        default:
            return Settings(process);
    }
}

int Pipelines(MainProcess &process) {
    std::system("cls");
    std::cout << "Pipelines." << std::endl;
    getch();
}

int RunPipelines(MainProcess &process) {
    std::system("cls");
    std::cout << "Run Pipelines." << std::endl;
    getch();
}


bool FileExists(char *path) {
    DWORD ftyp = GetFileAttributesA(path);
    return ftyp != INVALID_FILE_ATTRIBUTES;
}

std::string input(std::string query) {
    std::cout << query;
    std::string r;
    std::getline(std::cin, r);
    return r;
}

int getInt(std::string query) {
    start:
    std::cout << query;
    std::string tmp;
    std::getline(std::cin, tmp);
    if (tmp.length() == 0) return 0;
    try {
        return std::stoi(tmp);
    } catch (...) {
        std::cout << "Error: Input must be a whole number." << std::endl;
        goto start;
    }
}

bool SettingsSetupPrompt(SCPArgumentsBuilder &builder) {
    std::system("cls");
    std::cout << "Welcome to FileTransfer! To get you set up, let's configure your settings." << std::endl;
    std::cout << "Since FileTransfer uses OpenSSH, the settings will be the same as the SCP command arguments." << std::endl;
    std::cout << "\nIf you want a field left blank, press ENTER without entering any data.\n" << std::endl;
    builder.cipher = input("Enter cypher: ");
    builder.ssh_config = input("Enter SSH config: ");
    builder.identity_file = input("Enter identity file: ");
    int limit = getInt("Enter download limit in KB/s (default is 1 KB/s): ");
    if (limit > 0) {
        builder.limit = limit;
    }
    builder.ssh_option = input("SSH option: ");
    
    getPort:
    int port = getInt("Enter SSH server port (default 22): ");
    if (port < 22 || port > 65535) {
        std::cout << "Port must be between 22 and 65535." << std::endl;
        goto getPort;
    }
    builder.port = port;

    builder.program = input("Enter program for connecting (defaults to OpenSSH ssh): ");
    builder.source = input("Enter your server source (ex. mmiyamoto@mysshserver.com)): ");

    std::cout << "Save configuration to settings.cfg? (Y/n): ";
    std::string r;
    std::getline(std::cin, r);
    
    return r.at(0) == 'Y';
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
    // pathstrcat(DATA_FOLDER, separatorConst());
    // pathstrcat(DATA_FOLDER, DATA_FOLDER_TITLE);
    strcat(DATA_FOLDER, separatorConst());
    strcat(DATA_FOLDER, DATA_FOLDER_TITLE);
}

void TerminalExitCallbackHandler(int signum) {
    ProgramExit(signum);
}

void ProgramExit(int exitCode) {
    std::cout << "Program written by Makoto Miyamoto. https://makotomiyamoto.com/filetransfer" << std::endl;
    std::exit(exitCode);
}
